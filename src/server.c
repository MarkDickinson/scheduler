#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <sys/utsname.h>
#include <limits.h>
#include <signal.h>
#include <netdb.h>

#include "debug_levels.h"
#include "server_global_data.h"
#include "alerts.h"
#include "calendar.h"
#include "config.h"
#include "jobslib.h"
#include "schedlib.h"
#include "utils.h"
#include "scheduler.h"
#include "api.h"
#include "bulletproof.h"
#include "users.h"
#include "system_type.h"
#include "memorylib.h"

/* For testing new functions */
/* #define BETA 1 */
/* #define USE_BACKDOOR 1 - allows backdoor shutdown via telnet */
#define USE_BACKDOOR 1

/* Exit codes from 'die' */
#define DIE_INTERNAL    1
#define DIE_TCPIP_SETUP 2

#ifndef SHUT_RDWR
#define SHUT_RDWR 3
#endif

#define MAX_STACK   32
#define MAX_CLIENTS 64

/*
typedef struct {
   char buffer[4096];
} mpz_t;
*/
typedef unsigned char MPZ_CHAR;
typedef MPZ_CHAR MPZ_BUF[4096];
typedef MPZ_BUF mpz_t;
/* typedef char[4096] mpz_t; */

/* static mpz_t *stack[MAX_STACK]; */
mpz_t * mpz_t_stack[MAX_STACK];
char * stack;
static int sp = 0;

typedef struct {
   FILE  *rx;
   FILE  *tx;
   mpz_t **stack;     /* stack array   */
   int   sp;          /* stack pointer */
   char  remoteaddr[50];
   char  access_level;   /* 'A'=admin1, 'O'=operator, '0'=no access requested */
   char  user_name[USER_MAX_NAMELEN+1];  /* name of user logged on */
   char accept_unsol;    /* write unsolicted messages to this connection Y/N */
   pid_t user_pid;       /* pid of user process on remote addr if logged on */
} ClientInfo;

ClientInfo client[MAX_CLIENTS+1];

/* make listen_sock global so it is accessable to the shutdown_immed
 * handler rguardless of when we need to do a shutdown */
int listen_sock;         /* socket handle for listener socket */

/* Added so we know if we need to cancel a newday waiting alert */
int GLOBAL_newday_alert_outstanding;

/* a new one to be triggered when the newday runs, to indicate
 * that when we get back to mainline processing we should reset
 * the malloc/free memory counters. Will only be checked when
 * a sched_init check is being processed to keep down overhead. */
int SCHED_init_memcounters_required;

/* =========================================================
 * die:
 * 
 * provide a fast-stop that does a bit of a cleanup. This is called if we
 * encounter a non-recoverable error and need to bail out of the program.
   ========================================================= */
static void die(const char *msgtext, int exitcode) {
   perror( msgtext );
   myprintf(  "*ERR: SE010-***FATAL*** A fatal server error has occurred. Cannot continue.\n" );
   myprintf(  "*ERR: SE010-%s\n", msgtext );
   ALERTS_terminate();
   CALENDAR_Terminate();
   JOBS_Terminate();
   SCHED_Terminate();
   USER_Terminate();
   myprintf(  "INFO: SI001-Server shutdown has completed.\n" );
   exit( exitcode );
}

/* =========================================================
 * shutdowm_immed:
 * 
 * Perform a server shutdown. Called on a shutdown request.
   ========================================================= */
static void shutdown_immed( void ) {
   int z;

   if (pSCHEDULER_CONFIG_FLAGS.log_level > 0) {
      myprintf(  "WARN: SW001-Shutdown in progress\n" );
   }
   
   /* close all the files we have open */
   ALERTS_terminate();
   CALENDAR_Terminate();
   JOBS_Terminate();
   SCHED_Terminate();
   USER_Terminate();
   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
      myprintf(  "INFO: SI002-All databases closed, releasing memory and tcp-ip sockets\n" );
   }
   
   /* Free all memory stacks, Close all open session sockets */
   for ( z = 0; z < MAX_CLIENTS; ++z ) {
      stack = (char *) client[z].stack;
      if (stack != NULL) {
         sp = client[z].sp;
         while (sp > 0) {
            free( &stack[--sp] );
         }
         free( stack );
      }
      if (client[z].tx != NULL) {
         if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
		    myprintf(  "INFO: SI003-Closing session to %s\n", client[z].remoteaddr );
		 }
         fclose( client[z].tx );
         shutdown(fileno(client[z].rx),SHUT_RDWR);
         fclose( client[z].rx );
      }
      client[z].stack = NULL;
      client[z].sp = 0;
      client[z].rx = NULL;
      client[z].tx = NULL;
   }

   /* close the listening socket */
   shutdown(listen_sock,SHUT_RDWR);
   close( listen_sock );

   myprintf(  "----: SI004-ALL OK, Server shutdown has completed.\n" );
   exit( 0 );
}

/* =========================================================
   Called for a forced shutdown. By the time we are called
   the server flags are already set to stop new jobs
   running; we kill off any jobs that are currently
   running to allow the server to shutdown immediately.

   NOTE: I havn't found a signal that kills the child
         processes as well, so only use in a system shutdown.
   ========================================================= */
void shutdown_on_force( void ) {
   int i, kill_status;
   for (i = 0; i < running_job_table.number_of_jobs_running; i++ ) {
      myprintf(  "WARN: SW002-Killing PID %d, Job will go into alert status\n", (int)running_job_table.running_jobs[i].job_pid );
	  kill_status = kill( running_job_table.running_jobs[i].job_pid, SIGKILL );
	  if (kill_status != 0) {
			  perror( "Kill failure" );
	  }
   }
   /* THE BELOW IS BAD: DO NOT USE ON A LINUX SERVER
	* It killed off almost everything (including all my telnet sessions)
	* I had to use the console to 'init 2', then 'init 3' just to get telnet
	* back, nfs and portmap alas didn't recover 
   kill_status = kill( -1L, SIGKILL );
   SO DON'T use the above.
   */
} /* shutdown_on_force */

/* =========================================================
   clear out running_job_entries from (including) the offset passed 
   ========================================================= */
void clear_job_entry_table_from( int start_offset ) {
   int i;
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: entered clear_job_entry_table_from\n" ); 
   }
   for (i = start_offset; i < MAX_RUNNING_JOBS; i++) {
		   running_job_table.running_jobs[i].job_pid = 0;
		   running_job_table.running_jobs[i].job_header.jobnumber[0] = '\0';
		   UTILS_space_fill_string( (char *)&running_job_table.running_jobs[i].job_header.jobnumber, JOB_NUMBER_LEN );
		   running_job_table.running_jobs[i].job_header.jobname[0] = '\0';
		   UTILS_space_fill_string( (char *)&running_job_table.running_jobs[i].job_header.jobname, JOB_NAME_LEN );
		   running_job_table.running_jobs[i].job_header.info_flag = 'D';
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving clear_job_entry_table_from\n" ); 
   }
} /* clear_job_entry_table_from */

/* =========================================================
  remove the entry matching complete_job_pid and move all the
  other entries up, so the end of the table is always empty.

  At the end we call the job routines to update the runtimes.
  As a result of this we may be asked to raise and alert or
  reactivate a job (if its a run every nn minutes type job
  we reactivate the existing record rather than filling up
  the file with new ones).
  2002/09/15: Addedd checks for CCs we now define as
  standard return codes for users to use. This allows the
  alert messages (if needed) to be a bit more usefull.
   ========================================================= */
void clean_up_job_entry_table( pid_t * completed_job_pid, int status ) {
		int addr_found, operation_status;
		int i, j, k, junk;
		pid_t local_pid;
		int map_table[MAX_RUNNING_JOBS];
		active_queue_def local_rec;
		long job_recordnum, junk_long;
        joblog_def local_logrec;
        alertsfile_def local_alert;
		
		int exit_code;
		int signal_killed;
		char msgbuffer[UTILS_MAX_ERRORTEXT];
		char test_buf[10];
        char *ptr;
		
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: entered clean_up_job_entry_table\n" ); 
   }

   exit_code = 0;
   signal_killed = 0;
   if ((status == 99) || (status == 127)) {
      /* 99 and 127 are special cases passed in the status */
      exit_code = status;
   }
   else {
      /* interrogate the status passed to see how the child process stopped */
      if (WIFEXITED(status)) {           /* child exited normally, get exit code */
         exit_code = WEXITSTATUS(status);
      }
      else if (WIFSIGNALED(status)) {   /* killed by signal, get the signal */
      	  signal_killed = WTERMSIG(status);
      }
      else {
         exit_code = 98;                 /* should not have stopped, alert until we know why */
      }
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: clean_up_job_entry_table setup, exit code is %d, signal code is %d.\n", exit_code, signal_killed  ); 
   }
   
   local_pid = *completed_job_pid;
   i = 0;
   addr_found = -1;
   while( (i < MAX_RUNNING_JOBS) && (addr_found < 0) ) {
		   if (running_job_table.running_jobs[i].job_pid == local_pid) {
			  addr_found = i;
		   }
		   else {
		      i++;
		   }
   }

   if (addr_found < 0) {
      /* Always advise job completion, we need an audit trail */
      if (signal_killed != 0) {
         myprintf(  "*ERR: SE011-Unable to find a match for killed job pid=%d, killing signal was %d\n", local_pid, signal_killed );
      }
      else {
         myprintf(  "*ERR: SE012-Unable to find a match for completed job pid=%d, exit code was %d\n", local_pid, exit_code );
      }
      running_job_table.exclusive_job_running = 0;  /* it may have been an exclusive job ? */
      return;
   } /* if addr found */

   /* Always show job completion, we need an audit trail */
   if (signal_killed != 0) {
      myprintf(  "JOB-: SI005-Job %s (pid %d) was killed, killed by signal %d\n",
              running_job_table.running_jobs[addr_found].job_header.jobname,
	          local_pid, signal_killed );
   }
   else {
      myprintf(  "JOB-: SI006-Job %s (pid %d) completed, exit code was %d\n",
              running_job_table.running_jobs[addr_found].job_header.jobname,
	          local_pid, exit_code );
   }

   /* SAVE THE JOB HEADER INFO, we need to refer to it after
    * we have cleared the entry from the pid table.
    */
   memcpy( local_rec.job_header.jobnumber,
		   running_job_table.running_jobs[addr_found].job_header.jobnumber,
		   sizeof( job_header_def ) );
   /* MID: patch, job header was not null terminated and was producing garbage
	* in the alert text (or filled the field so no null was added in copies as
	* strncpy only copied the filed len and no null if its a full field so Im
	* terminating this after each copy below also */
   local_rec.job_header.jobname[JOB_NAME_LEN] = '\0';
   if (strncmp(local_rec.job_header.jobname,"NULL",4) == 0) {
      running_job_table.exclusive_job_running = 0;  /* turn off exclusion flag now */
	  myprintf(  "JOB-: SI007-JOB %s, releasing exclusive control, other jobs may now run\n", 
              local_rec.job_header.jobname );
   }

   /* first find active entries and put them in the map table */
   running_job_table.running_jobs[addr_found].job_pid = 0;
   for (j = 0; j < MAX_RUNNING_JOBS; j++ ) { map_table[j] = 0; }
   j = 0;
   for (i = 0; i < MAX_RUNNING_JOBS; i++ ) {
		   if (running_job_table.running_jobs[i].job_pid != 0) {
				   map_table[j] = i;
				   j++;
		   }
   }

   if (j > 0) { /* only do work if some were still left */
		   k = 0;
		   for (i = 0; i < j; i++ ) {
				   memcpy( running_job_table.running_jobs[i].job_header.jobnumber,
						   running_job_table.running_jobs[map_table[k]].job_header.jobnumber,
						   sizeof( job_header_def )
					     );
				   running_job_table.running_jobs[i].job_pid = running_job_table.running_jobs[map_table[k]].job_pid;
				   k++;
		   }
           clear_job_entry_table_from( k );
		   running_job_table.number_of_jobs_running = running_job_table.number_of_jobs_running - 1;
		   if (running_job_table.number_of_jobs_running < 0) {
		      running_job_table.number_of_jobs_running = 0;
			  myprintf(  "WARN: SW003-ERROR, number of running jobs < 0; adjusted to 0\n" );
		   }
   }
   else {
      running_job_table.number_of_jobs_running = 0; /* we cleared the only running one, 0 left */
   }
   
   /* We will now do all the post processing required for a completed
    */
   /* TODO, update history and log files */
   
   /* common, start builing logrec as alert needs it anyway */
   UTILS_datestamp( (char *)&local_logrec.datestamp, UTILS_timenow_timestamp() );
   strncpy( local_logrec.job_header.jobnumber, local_rec.job_header.jobnumber, JOB_NUMBER_LEN );
   strncpy( local_logrec.job_header.jobname, local_rec.job_header.jobname, JOB_NAME_LEN );
   local_logrec.job_header.jobname[JOB_NAME_LEN] = '\0';
   strncpy( local_logrec.msgid, "0000000000", JOB_LOGMSGID_LEN );
   strncpy( local_logrec.status_code, "000", JOB_STATUSCODE_LEN );

   /* If job failed, we need to set it to a failed state and generate an
    * alert */
   if ( (exit_code != 0) || (signal_killed != 0) ) {
       /* set a default message, some conditions don't seem to generate
        * messages properly anymore, the custom one works ok still */
      snprintf( local_logrec.text, JOB_LOGMSG_LEN, "JOB %s: FAILED, INVESTIGATE", local_rec.job_header.jobname );
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 0) {
         myprintf(  "WARN: SW004-Job %s has status code %d, kill signal %d, setting failure notifications\n",
				 local_rec.job_header.jobname, exit_code, signal_killed );
	  }
      job_recordnum = SCHED_ACTIVE_read_record( (active_queue_def *)&local_rec );
      if (job_recordnum < 0) {
         myprintf(  "*ERR: SE013-Unable to read record for %s to set failure flag\n", local_rec.job_header.jobname );
      }
	  else {
	     /* mark the active queue record as a job in a failed state */
	     local_rec.current_status = '3'; /* failure state */
		 if (signal_killed != 0) {
            strncpy( local_alert.failure_code, ALERTS_SYSTEM_GENERATED, JOB_STATUSCODE_LEN );
		    strncpy(local_logrec.status_code, "SIG", JOB_STATUSCODE_LEN+1 ); /* allow +1 for null to go in */
		 }
		 else {
            strncpy( local_alert.failure_code, ALERTS_USER_GENERATED, JOB_STATUSCODE_LEN );
            /* BUG: Doesn't correctly set three byte values, so use 9 byte
             * field and copy out what we need from that. */
		    UTILS_number_to_string( exit_code, (char *)&test_buf, 9 );
			ptr = (char *)&test_buf;
			ptr = ptr + 6;
		    strncpy( local_logrec.status_code, ptr, JOB_STATUSCODE_LEN+1 ); /* +1, copy null too */
		 }
	     job_recordnum = SCHED_ACTIVE_write_record( (active_queue_def *)&local_rec, job_recordnum );
	     if (job_recordnum < 0) {
            myprintf(  "*ERR: SE014-Unable to write active queue file failure info for %s\n", local_rec.job_header.jobname );
	     }
		 /* Create an alert record for the failure */
		 strncpy( local_logrec.msgid, "0000003000", JOB_LOGMSGID_LEN );  /* use 3000 in failure range */
		 local_alert.severity = '3'; /* page support */
		 local_alert.acknowledged = 'N';
         /* was it terminated by a kill signal */
         if (signal_killed != 0) {
            snprintf( local_logrec.text, JOB_LOGMSG_LEN, "JOB %s: KILLED BY SIGNAL %d",
                      local_rec.job_header.jobname, signal_killed );
         }
         /* If an exit code then check what it was */
         else {
               if (exit_code == 98) {
                  snprintf( local_logrec.text, JOB_LOGMSG_LEN, "JOB %s: FAILED, UNDOCUMENTED SYSTEM TERMINATION", local_rec.job_header.jobname );
                  strncpy( local_alert.failure_code, ALERTS_OS_ERROR, JOB_STATUSCODE_LEN );
               }
			   else if (exit_code == 99) {
                  snprintf( local_logrec.text, JOB_LOGMSG_LEN, "JOB %s: FAILED, UNABLE TO CHANGE UID TO JOB OWNER", local_rec.job_header.jobname );
                  strncpy( local_alert.failure_code, ALERTS_USERNOTFOUND, JOB_STATUSCODE_LEN );
               }
			   else if (exit_code == 126) {
                  snprintf( local_logrec.text, JOB_LOGMSG_LEN, "JOB %s: FAILED, PROGRAM NOT EXECUTABLE", local_rec.job_header.jobname );
                  strncpy( local_alert.failure_code, ALERTS_PGMNOTFOUND, JOB_STATUSCODE_LEN );
               }
			   else if (exit_code == 127) {
                  snprintf( local_logrec.text, JOB_LOGMSG_LEN, "JOB %s: FAILED, PROGRAM NOT FOUND OR NOT RUNABLE", local_rec.job_header.jobname );
                  strncpy( local_alert.failure_code, ALERTS_PGMNOTFOUND, JOB_STATUSCODE_LEN );
               }
               /* some user defined ones */
			   else if ( (exit_code >= 150) || (exit_code <= 199) ) {
                  ALERTS_CUSTOM_set_alert_text( exit_code, (char *)&local_logrec.text, JOB_LOGMSG_LEN,
                                                (char *)&local_alert.severity, (char *)&local_rec.job_header.jobname );
                  strncpy( local_alert.failure_code, ALERTS_USER_CUSTOM, JOB_STATUSCODE_LEN );
               }
			   else {
                  snprintf( local_logrec.text, JOB_LOGMSG_LEN, "JOB %s: EXIT CODE=%d", local_rec.job_header.jobname, exit_code );
                  strncpy( local_alert.failure_code, ALERTS_USER_GENERATED, JOB_STATUSCODE_LEN );
               }
         }
         /* End of response code checks */
	     memcpy( (char *)&local_alert.job_details, (char *)&local_logrec, sizeof(joblog_def) );
		 junk = ALERTS_write_alert_record( &local_alert, -1 );
	  } /* job record read */
   }

   /* ELSE job completed with status 0 which is OK */
   else {
      snprintf( local_logrec.text, JOB_LOGMSG_LEN, "JOB %s COMPLETED OK\n", local_rec.job_header.jobname );
	  /* MID: add completion code 4 for later checks, don't worry if it fails
	   * as the code only needs it for display purposes at present, IT CANNOT
	   * be allowed to stop the rest of the logic in this proc. */
          job_recordnum = SCHED_ACTIVE_read_record( (active_queue_def *)&local_rec );
          if (job_recordnum >= 0) {
                  local_rec.current_status = '4';
                  job_recordnum = SCHED_ACTIVE_write_record( (active_queue_def *)&local_rec, job_recordnum );
                  if (job_recordnum < 0) {
                      myprintf(  "WARN: SW005-JOB %s WILL BE DELETED AS EXEC STATE, CAN NOT SET COMPLETE FLAG\n",
                                local_rec.job_header.jobname );
                  }
          }
          else {
             myprintf(  "WARN: SW006-JOB %s WILL BE DELETED AS EXEC STATE, CAN NOT SET COMPLETE FLAG\n",
                       local_rec.job_header.jobname );
          }
	  /* MID: end of above completion code changes */
          junk_long = SCHED_ACTIVE_delete_record( (active_queue_def *)&local_rec, 1 );
          if (junk_long != 0) {
             myprintf(  "*ERR: SE015-Delete off active queue for completed job failed, JOB %s\n", local_rec.job_header.jobname );
          }
	  /* And, advise we need to clear any dependencies waiting on this job */
	  /* SCHED_DEPEND_delete_all_relying_on_job( (job_header_def *)&local_rec.job_header.jobnumber ); */
	  /* MID: The above is done by SCHED_ACTIVE_delete_record now */

      operation_status = JOBS_completed_job_time_adjustments( &local_rec );
      if (operation_status == 2) {    /* raise an alert */
		   UTILS_get_message( msgbuffer, UTILS_MAX_ERRORTEXT, 1 );
		   ALERTS_generic_system_alert( (job_header_def *)&local_rec, msgbuffer );
      }
      else if (operation_status == 1) { /* job needs requeueing */
         if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARS) {
            myprintf(  "DEBUG: Setting SCHED_init_required = 1 (clean_up_job_entry_table)\n" ); 
         }
         SCHED_init_required = 1;
         local_rec.job_header.info_flag = 'A';   /* reactivate it */
         local_rec.current_status = '0';         /* back to time wait */
         junk_long = SCHED_ACTIVE_write_record( (active_queue_def *)&local_rec, job_recordnum );
         if (junk_long != job_recordnum) {
             myprintf(  "*ERR: SE016-Unable to requeue a repeating job request, job %s\n",
                       local_rec.job_header.jobname );
             ALERTS_generic_system_alert( (job_header_def *)&local_rec, "UNABLE TO REQUEUE JOB, CHECK SERVER LOG" );
         }
         else {
            if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
               myprintf(  "JOB-: SI008-Job %s, requeued to %s\n", local_rec.job_header.jobname, ctime( (time_t *)&local_rec.run_timestamp ) );
            }
         }
      }
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving clean_up_job_entry_table, number of jobs being tracked = %d.\n",
		      running_job_table.number_of_jobs_running ); 
   }
} /* clean_up_job_entry_table */

/* =========================================================
 * With adding the ability to run external programs to
 * generate alerts they were triggering the completed jobs
 * processing and raising error messages as no matching jobs
 * were found for the pid.
 * Now this routine is called before a chanck against the jobs
 * pid's are done. If the pid is an alert task pid we just reset
 * it to an empty slot and return 1. If we don't match we
 * return 0 and the original logic to process a completed
 * job program is carried out.
 * Returns: 1, called does nothing, 0 caller treats pis as a job pid.
   ========================================================= */
int clean_up_alert_table( pid_t * completed_alert_pid ) {
   int i;
   pid_t local_pid;

   local_pid = *completed_alert_pid;
   for (i = 0; i < MAX_RUNNING_ALERTS; i++) {
       if (running_alert_table.running_alerts[i].alert_pid == local_pid) {
          running_alert_table.running_alerts[i].alert_pid = 0;
          return( 1 );  /* it's an alert, ignore it */
       }
   }
   return( 0 ); /* was not an alert task, process as a job completion  */
} /* clean_up_alert_table */

/* =========================================================
 * One issue identified in testing is that if the server
 * is stopped and restarted while jobs are running, it
 * loses track of jobs that completed while the server was down
 * so they hang around in 'executing' status.
 * At this point, during a server restart any jobs on the
 * queue marked as executing will be treated as if they
 * terminated in error, they must be manually checked to
 * determine if they completed correctly.
 * If OK the user can use the alert forceok command, or if
 * the job failed the alert restart option to restart the
 * job.
 *
 * Even if they are still running, and can be detected by
 * a PID check, a retsrated/new server will not get the
 * termination messages, so this is the safest way.
 *
 * The scheduler will delay shutting down if jobs are still
 * running, but there is no defense against a user
 * killing the server.
   ========================================================= */
void clean_up_job_entry_table_on_restart( void ) {
   int lasterror, done;
   long recordpos, updated_recordnum;
   active_queue_def * local_rec;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: entered clean_up_job_entry_table_on_restart\n" );
   }

   myprintf(  "INFO: SI009----- Checking for stale active queue entries.\n" );

   lasterror = fseek( active_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      myprintf(  "*ERR: SE017-Active job queue file %s is corrupt (seek failure), cannot start server !\n", active_file );
      exit(1);
   }
   recordpos = 0;

   /* Allocate memory for a local copy of the record */
   if ((local_rec = (active_queue_def *)MEMORY_malloc( sizeof( active_queue_def ),"clean_up_job_entry_table_on_restart" )) == NULL) {
      myprintf(  "*ERR: SE018-Unable to allocate memory for server startup, need %d bytes !\n", sizeof(active_queue_def) );
      exit(1);
   }

   done = 0;
   /* Loop reading through the file */
   while ( (!(feof(active_handle))) && (!(ferror(active_handle))) && (done == 0) ) {
      lasterror = fread( local_rec, sizeof(active_queue_def), 1, active_handle  );
      if (ferror(active_handle)) {
         MEMORY_free( local_rec );
         myprintf(  "*ERR: SE019-Active job queue file %s is corrupt (read failure), cannot start server !\n", active_file );
         exit(1);
      }
	  recordpos++;
      if ( (!(feof(active_handle))) && (!(ferror(active_handle))) ) {
         /* was the job executing ? */
         if ( (local_rec->current_status == '2') && (local_rec->job_header.info_flag != 'D') ) {
			/* if a match was found, set it to failed */
			   myprintf(  "WARN: SW007-----    %s FLAGGED AS FAILED, CHECK IT\n", local_rec->job_header.jobname );
			   local_rec->current_status = '3';  /* set to failed state */
               updated_recordnum = SCHED_ACTIVE_write_record( local_rec, (recordpos - 1) );
               if (updated_recordnum != (recordpos - 1) ) {
                   myprintf(  "*ERR: SE020-Unable to mark job %s as failed, continuing\n", local_rec->job_header.jobname );
               }
			   ALERTS_generic_system_alert( (job_header_def *)&local_rec->job_header.jobnumber,
							                "STILL RUNNING WHEN SCHEDULER STOPPED");
               lasterror = fseek( active_handle, (recordpos * sizeof(active_queue_def)), SEEK_SET );
			   if (lasterror != 0) {
					   myprintf(  "*ERR: SE021-File seek error on %s, aborting checks\n", active_file );
					   done = 2; /* put abort handling in for this later */
			   }
		 } /* it was executing */
      } /* if not eof */
	  else {
         done = 1;   /* we hit the eof */
	  }
   } /* while */

   /* done, free memory and return */
   MEMORY_free( local_rec );

   myprintf(  "INFO: SI010----- Completed check for stale active queue entries.\n" );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving clean_up_job_entry_table_on_restart\n" );
   }

   return;
} /* clean_up_job_entry_table_on_restart */

/* =========================================================
 * Another issue identified in testing is that if the server
 * is stopped after a job has failed but before the alert
 * record is written it's not possible to restart the job
 * (although delete/resubmit still works) as the alert restart
 * function needs the alrt record.
 *
 * This procedure will consistency check the active queue and
 * alert queue against each other as follows...
 * - if a job is in alert state but there is no alert record
 *   we will create one. Possible as the job is set to alert
 *   state before the alert record is written.
 * - if an alert record exists but there is no matching job on
 *   the active queue file, the alert record is deleted. If there
 *   is a matching job ensure it is set flagged as in an alert
 *   state.
   ========================================================= */
void consistency_check_on_restart( void ) {
   int lasterror;
   long recordnum;
   active_queue_def * local_active_rec;
   alertsfile_def * local_alert_rec;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: entered consistency_check_on_restart\n" );
   }

   myprintf(  "INFO: SI011----- Beginning database consistency checks\n" );

   lasterror = fseek( active_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      myprintf(  "*ERR: SE022-Active job queue file %s is corrupt (seek failure), cannot start server !\n", active_file );
      exit(1);
   }

   /* Allocate memory for a local copies of our records */
   if ((local_active_rec = (active_queue_def *)MEMORY_malloc( sizeof( active_queue_def ),"consistency_check_on_restart" )) == NULL) {
      myprintf(  "*ERR: SE023-Unable to allocate memory for consistency checks, need %d bytes !\n", sizeof(active_queue_def) );
      exit(1);
   }
   if ((local_alert_rec = (alertsfile_def *)MEMORY_malloc( sizeof( alertsfile_def ),"consistency_check_on_restart" )) == NULL) {
      MEMORY_free( local_active_rec );
      myprintf(  "*ERR: SE024-Unable to allocate memory for consistency checks, need %d bytes !\n", sizeof(alertsfile_def) );
      exit(1);
   }

   /* for each job on the active queue check to see if it is in alert state, if
	* it is ensure it has an alert record */
   while ( (!(feof(active_handle))) && (!(ferror(active_handle))) ) {
      lasterror = fread( local_active_rec, sizeof(active_queue_def), 1, active_handle  );
      if (ferror(active_handle)) {
         MEMORY_free( local_active_rec );
         MEMORY_free( local_alert_rec );
         myprintf(  "*ERR: SE025-Active job queue file %s is corrupt (read failure), cannot start server !\n", active_file );
         exit(1);
      }
      if ( (!(feof(active_handle))) && (!(ferror(active_handle))) ) {
         /* if in alert state (3) and not a deleted record */
         if ( (local_active_rec->current_status == '3') && (local_active_rec->job_header.info_flag != 'D') ) {
            memcpy( local_alert_rec->job_details.job_header.jobnumber, local_active_rec->job_header.jobnumber, 
                    sizeof(job_header_def) );
            recordnum = ALERTS_read_alert( local_alert_rec );
            if (recordnum == -1) { /* no alert record found */
			   myprintf(  "WARN: SW008-----    %s NO ALERT REC FOUND, ONE HAS BEEN CREATED\n", local_active_rec->job_header.jobname );
			   ALERTS_generic_system_alert( (job_header_def *)&local_active_rec->job_header.jobnumber,
							                "IN FAILED STATE AT SCHED RESTART");
            }
         }
      }
   } /* while */

   /* For each job on the alert queue check that the corresponding job record
	* is actually in an alert state. */
   lasterror = fseek( alerts_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      MEMORY_free( local_active_rec );
      MEMORY_free( local_alert_rec );
      myprintf(  "*ERR: SE026-Alerts file %s is corrupt (seek failure), cannot start server !\n", alerts_file );
      exit(1);
   }
   while ( (!(feof(alerts_handle))) && (!(ferror(alerts_handle))) ) {
      lasterror = fread( local_alert_rec, sizeof(alertsfile_def), 1, alerts_handle  );
      if (ferror(alerts_handle)) {
         MEMORY_free( local_active_rec );
         MEMORY_free( local_alert_rec );
         myprintf(  "*ERR: SE027-Alerts file %s is corrupt (read failure), cannot start server !\n", alerts_file );
         exit(1);
      }
      if ( (!(feof(alerts_handle))) && (!(ferror(alerts_handle))) ) {
         if (local_alert_rec->job_details.job_header.info_flag != 'D') { /* not a deleted record */
            memcpy( local_active_rec->job_header.jobnumber, local_alert_rec->job_details.job_header.jobnumber, 
                    sizeof(job_header_def) );
            recordnum = SCHED_ACTIVE_read_record( local_active_rec );
            if (recordnum == -1) {  /* not on active queue */
			   myprintf(  "WARN: SW009-----    %s ALERT REC FOUND, NO MATCHING JOB ON QUEUE SO DELETED ALERT\n",
                          local_alert_rec->job_details.job_header.jobname );
               recordnum = ALERTS_delete_alert( local_alert_rec );
			   /* If OK we are positioned correctly for next read */
			   if (recordnum == -1) {  /* delete failed */
                  MEMORY_free( local_active_rec );
                  MEMORY_free( local_alert_rec );
                  myprintf(  "*ERR: SE028-Alerts file %s is corrupt (update failure), cannot start server !\n", alerts_file );
                  exit(1);
			   }
		    }
            else {  /* the job record was read ok */
               if (local_active_rec->current_status != '3') {
                  /* An alert record, but job not in alert state */
			      myprintf(  "WARN: SW010-----    %s ALERT REC FOUND, JOB ADJUSTED TO FAIL STATE\n", local_active_rec->job_header.jobname );
                  local_active_rec->current_status = '3';
                  recordnum = SCHED_ACTIVE_write_record( local_active_rec, recordnum );
                  if (recordnum == -1) { /* not corrected */
                     MEMORY_free( local_active_rec );
                     MEMORY_free( local_alert_rec );
                     myprintf(  "*ERR: SE029-Active Job queue file %s is corrupt (update failure), cannot start server !\n", active_file );
                     exit(1);
                  }
               }
            }
         }  /* not deleted */
      }
   } /* while */

   /* All done, release the memory we allocated */
   MEMORY_free( local_active_rec );
   MEMORY_free( local_alert_rec );

   myprintf(  "INFO: SI012----- Completed database consistency checks\n" );
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving consistency_check_on_restart\n" );
   }
   return;
} /* consistency_check_on_restart */

/* =========================================================
 * In a job status display I now want to include the pid
 * of the job that is running. So lookup the job by
 * jobname and return the pid.
 * function returns 0 on error, 1 if ok
   ========================================================= */
int get_pid_of_job( job_header_def * job_header, pid_t * pid_result ) {
   int i, addr_found;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: entering get_pid_of_job\n" );
   }

   i = 0;
   addr_found = -1;
   while( (i < MAX_RUNNING_JOBS) && (addr_found < 0) ) {
		   if ( strncmp(job_header->jobname, running_job_table.running_jobs[i].job_header.jobname, JOB_NAME_LEN) == 0) {
			  addr_found = i;
		   }
		   else {
		      i++;
		   }
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving get_pid_of_job\n" );
   }
   if (addr_found < 0) {
		     return( 0 );
   }
   else {
		   *pid_result = running_job_table.running_jobs[addr_found].job_pid;
		   return( 1 );
   }
} /* get_pid_of_job */

/* =========================================================
 * Count the number of jobs currently in our running
 * job table.
   ========================================================= */
long INTERNAL_count_executing( void ) {
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: entering INTERNAL_count_executing, and leaving (only one line)\n" );
   }
   return( (long)running_job_table.number_of_jobs_running );
} /* INTERNAL_count_executing */

/* =========================================================
 *  get_sock_addr: 
 * 
 *  From a passed socket file handle, obtain the socket address
 *  and port number.
 *  input:  buf - pointer to a buffer to hold the result
 *          bufsize - maximum size allowed for result
 *  output: buf - updated with result
 *  returns: buf if ok, NULL if an error
   ========================================================= */
char * get_sock_addr( int sockfh, char *buf, size_t bufsiz ) {
   int z;
   struct sockaddr_in adr_inet;    /* AF_INET */
   int len_inet;                   /* length */

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: entering get_sock_addr\n" );
   }

   len_inet = sizeof adr_inet;
#ifdef GCC_MAJOR_VERSION3
   /* The below works ok with GCC version 3.4.2 (not with 4.1.1) */
   z = getsockname( sockfh, (struct sockaddr *)&adr_inet, &len_inet );
#else                  
   /* else version 4 */
   z = getsockname( sockfh, (struct sockaddr *)&adr_inet, (socklen_t *)&len_inet );
#endif
   if (z == -1) return NULL;

   /* convert address into a string form for display */
   snprintf(buf,bufsiz,"%s:%u",inet_ntoa(adr_inet.sin_addr),
            (unsigned)ntohs(adr_inet.sin_port));

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving get_sock_addr\n" );
   }

   return buf;
}

/* =========================================================
 *  From a passed socket file handle, obtain the socket address
 *  and port number of the remote (peer) end of the connection.
 *  That is... find out who is talking to us.
 *  input:  buf - pointer to a buffer to hold the result
 *          bufsize - maximum size allowed for result
 *  output: buf - updated with result
 *  returns: buf if ok, NULL if an error
   ========================================================= */
char * get_remote_addr( int sockfh, char * buf, size_t bufsize ) {
   int z;
   struct sockaddr_in adr_inet;    /* AF_INET */
   int len_inet;                   /* length */

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: entering get_remote_addr\n" );
   }

   len_inet = sizeof adr_inet;
#ifdef GCC_MAJOR_VERSION3
   /* The below works ok with GCC version 3.4.2 (not with 4.1.1) */
   z = getpeername( sockfh, (struct sockaddr *)&adr_inet, &len_inet );
#else        
   /* else version 4 or above */
   z = getpeername( sockfh, (struct sockaddr *)&adr_inet, (socklen_t *)&len_inet );
#endif
   if (z == -1) return NULL;

   /* convert address into a string form for display */
   z = snprintf(buf,bufsize,"%s:%u",inet_ntoa(adr_inet.sin_addr),
                (unsigned)ntohs(adr_inet.sin_port));
   if ( z == -1 ) return NULL;     /* buffer to small */

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving get_remote_addr\n" );
   }

   return buf;
} /* char * get_remote_addr */

/* ------------------------------------------------------------------
 * ------------------------------------------------------------------ */
void SERVER_roll_log_files() {
		time_t time_number;
		struct tm time_var;
		struct tm *time_var_ptr;
		char datenow[10];

		/* Work out the date for the logfile name */
		time_var_ptr = &time_var;
		time_number = time(0);
	    time_var_ptr = localtime(&time_number);	
        time_var_ptr->tm_year = time_var_ptr->tm_year + 1900;  /* years returned as count from 1900 */
        time_var_ptr->tm_mon = time_var_ptr->tm_mon + 1;       /* months returned as 0-11 */
        snprintf( datenow, 9, "%.4d%.2d%.2d",
                  time_var_ptr->tm_year, time_var_ptr->tm_mon, time_var_ptr->tm_mday );
        snprintf( msg_logfile_name, 25, "logs/server_%s.log", datenow );

		/* Close any open logfile */
		if (msg_log_handle != NULL) {
				myprintf( "INFO: SI013-Closing log file, rolling over to a new days log\n" );
				fclose( msg_log_handle );
		}

        /* Open the new logfile */
        if ( (msg_log_handle = fopen( msg_logfile_name, "a+" )) == NULL ) {
				myprintf( "*ERR: SE030-Unable to open log file %s, continuing using STDOUT for messages\n", msg_logfile_name );
				return;
		}

		/* Work out the next logfile roll time */
        time_number = time(0);
		time_number = time_number + (60 * 60 * 24);
        time_var_ptr = localtime(&time_number); 
        time_var_ptr->tm_hour = 0; 
        time_var_ptr->tm_min = 0; 
        time_var_ptr->tm_sec = 1; 
		logfile_roll_time = mktime( time_var_ptr );
} /* SERVER_roll_log_files */

/* ------------------------------------------------------------------
 * ------------------------------------------------------------------ */
void SERVER_Open_All_Files( void ) {
   /*
    * Scheduler Interfaces
    * Initialise each of the subsystem libraries we use. This will also open
    * all of the files each library is in charge of.
    */
   if (CONFIG_Initialise( (internal_flags *) &pSCHEDULER_CONFIG_FLAGS ) == 0) {
      myprintf(  "WARN: SW011-Programmer missed removing a license check somewhere, continuing.\n" );
   }

   /* Record the log level we will be using */
   if (pSCHEDULER_CONFIG_FLAGS.log_level == 0) {
	   myprintf(  "INFO: SI014-SCHEDULER CONFIGURED TO ONLY LOG ERROR MESSAGES\n" );
   }
   else if (pSCHEDULER_CONFIG_FLAGS.log_level == 1) {
	   myprintf(  "INFO: SI015-SCHEDULER CONFIGURED TO LOG WARNING OR HIGHER SEVERITY MESSAGES ONLY\n" );
   }
   else {
	   myprintf(  "INFO: SI016-SCHEDULER CONFIGURED AS LOG LEVEL INFO (log all messages)\n" );
   }

   /* A messy way, but we want to shut all files cleanly if something goes wrong */
   if (ALERTS_initialise() != 1) {
      myprintf(  "*ERR: SE031-One or more fatal errors detected, TERMINATING ABNORMALLY\n" );
      exit(1);
   }
   if (CALENDAR_Initialise() != 1) {
      myprintf(  "*ERR: SE032-One or more fatal errors detected, TERMINATING ABNORMALLY\n" );
      ALERTS_terminate();
      exit(1);
   }
   if (JOBS_Initialise() != 1) {
      myprintf(  "*ERR: SE033-One or more fatal errors detected, TERMINATING ABNORMALLY\n" );
      ALERTS_terminate();
      CALENDAR_Terminate();
      exit(1);
   }
   if (SCHED_Initialise() != 1) {
      myprintf(  "*ERR: SE034-One or more fatal errors detected, TERMINATING ABNORMALLY\n" );
      ALERTS_terminate();
      CALENDAR_Terminate();
      JOBS_Terminate();
      exit(1);
   }
   if (USER_Initialise() != 1) {
      myprintf(  "*ERR: SE035-One or more fatal errors detected, TERMINATING ABNORMALLY\n" );
      ALERTS_terminate();
      CALENDAR_Terminate();
      JOBS_Terminate();
	  SCHED_Terminate();
      exit(1);
   }
} /* SERVER_Open_All_Files */

/* ------------------------------------------------------------------
 * Note: we log all info messages in this proc reguardless of whether
 * the server is configured to suppress info messages or not. These
 * ones are important.
 * ------------------------------------------------------------------ */
void SERVER_newday_cleanup( void ) {
   int unlink_err;
   jobsfile_def datarec;   /* to get offsets from */
   USER_record_def user_datarec; /* likewise */

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: entering SERVER_newday_cleanup\n" );
   }

   /* Oops, this works beter BEFORE I close the file it's using */
   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
      myprintf(  "INFO: SI017-Scanning calendar file for obsolete entries\n" );
   }
   CALENDAR_checkfor_obsolete_entries();

   /* CLOSE all the database files, we are doing work on them */

   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
      myprintf(  "INFO: SI018-NEWDAY, closing all database files\n" );
   }
   ALERTS_terminate();
   CALENDAR_Terminate();
   JOBS_Terminate();
   SCHED_Terminate();
   USER_Terminate();

   /* Delete all the databases we can start afresh with, this is
	* a lot faster and safer than compressing files we know should
	* be empty by the end of the compress */

   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
      myprintf(  "INFO: SI019-NEWDAY, deleting daily queue files\n" );
      myprintf(  "INFO: SI020-Deleting file %s\n", alerts_file );
   }
   unlink_err = unlink( alerts_file );
   if (unlink_err != 0) {
		   myprintf(  "*ERR: SE036-Unable to delete alerts file, continuing (SERVER_newday_cleanup)...\n" );
   }
   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
      myprintf(  "INFO: SI020-Deleting file %s\n", active_file );
   }
   unlink_err = unlink( active_file );
   if (unlink_err != 0) {
		   myprintf(  "*ERR: SE037-Unable to delete active jobs file, continuing (SERVER_newday_cleanup)...\n" );
   }
   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
      myprintf(  "INFO: SI020-Deleting file %s\n", dependency_file );
   }
   unlink_err = unlink( dependency_file );
   if (unlink_err != 0) {
		   myprintf(  "*ERR: SE038-Unable to delete job dependency file, continuing (SERVER_newday_cleanup)...\n" );
   }

   /* The files that will still have data in them that we need to save will now
	* need to be compressed to remove any deleted entries from them */

   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
      myprintf(  "INFO: SI023-Compressing the jobs file\n" );
   }
   if ( UTILS_compress_file( job_file, sizeof(jobsfile_def),
             ( (char *)&datarec.job_header.info_flag - (char *)&datarec.job_header.jobnumber ),
             'D', "jobs file" ) != 1 )
   {
       myprintf(  "*ERR: SE039-Compress of jobs database failed !; FAST SHUTDOWN (SERVER_newday_cleanup)...\n" );
	   shutdown_immed();
   }
   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
      myprintf(  "INFO: SI024-Compressing the calendar file\n" );
   }
   if (UTILS_compress_file( calendar_file, sizeof(calendar_def),
                            CALENDAR_NAME_LEN + 1, '9', "calendar file") != 1)
   {
       myprintf(  "*ERR: SE040-Compress of calendar database failed !; FAST SHUTDOWN (SERVER_newday_cleanup)...\n" );
	   shutdown_immed();
   }
   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
      myprintf(  "INFO: SI025-Compressing the user file\n" );
   }
   if ( UTILS_compress_file( user_file, sizeof(USER_record_def),
             ( (char *)&user_datarec.record_state_flag - (char *)&user_datarec.userrec_version ),
             'D', "user file" ) != 1 )
   {
       myprintf(  "*ERR: SE041-Compress of user database failed !; FAST SHUTDOWN (SERVER_newday_cleanup)...\n" );
	   shutdown_immed();
   }

   /* We are done with the database cleanups now, we must OPEN ALL the files
	* again so the newday job can continue using them */

   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
      myprintf(  "INFO: SI026-NEWDAY, re-opening all database files\n" );
   }
   SERVER_Open_All_Files();		

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving SERVER_newday_cleanup\n" );
   }
} /* SERVER_newday_cleanup */

/* ------------------------------------------------------------------
 * Simulate the running of a job. We do this internally for our own
 * control tasks that we cannot start an external job for.
 * ------------------------------------------------------------------ */
void SERVER_spawnsimulate_task( active_queue_def * datarec ) {
   long active_recordnum;
   int alerts_count, active_count;
   time_t time_value;
   time_t time_now;
   char * ptr;
			
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: entering SERVER_spawnsimulate_task\n" );
   }

   /*
    * Ensure we have a good active record definition.
    */
   active_recordnum = SCHED_ACTIVE_read_record( datarec );
   if (active_recordnum < 0) {
      UTILS_set_message( "Unable to access active jobs file, job not scheduled on" );
	  myprintf(  "*ERR: SE042-Unable to read SCHEDULER* job entry from active jobs file, SUSPENDING SERVICES\n" );
	  pSCHEDULER_CONFIG_FLAGS.enabled = '0';
	  ALERTS_generic_system_alert( (job_header_def *)datarec, "CAN'T READ ACTIV-Q FILE, SCHED SUSPENDED" );
      return;
   }
				  
   /*
    * Determine the internal job type
    */
   /* -----------------------------------------------------------------
	* The scheduler newday job
	* ----------------------------------------------------------------- */
   if (memcmp( datarec->job_header.jobname, "SCHEDULER-NEWDAY", 16 ) == 0) {  /* newday processing */
      /* we can only run if no other jobs are still waiting to run */
      alerts_count = ALERTS_count_alerts( );
      active_count = SCHED_count_active( );
      if ((active_count > 0) || (alerts_count > 0)) {
         if (pSCHEDULER_CONFIG_FLAGS.newday_action == '1') {   /* use queueing method */ 
		    if (SCHED_internal_add_some_dependencies( datarec, active_recordnum ) != 1) {
					myprintf( "*ERR: SE043-Suspending Server activity until the problem is resolved\n" );
					pSCHEDULER_CONFIG_FLAGS.enabled = '0';
			}
            if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARS) {
               myprintf(  "DEBUG: Setting SCHED_init_required = 1 (SERVER_spawnsimulate_task)\n" ); 
            }
		    SCHED_init_required = 1; 
            /* If using external alerting raise one */
            if ((pSCHEDULER_CONFIG_FLAGS.use_central_alert_console == 'E') ||
                (pSCHEDULER_CONFIG_FLAGS.use_central_alert_console == 'Y'))
            {
            ALERTS_generic_system_alert_MSGONLY( (job_header_def *)datarec, "OUTSTANDING JOBS TO BE PROCESSED, INVESTIGATE" );
            GLOBAL_newday_alert_outstanding = 1;  /* Record, so we know to cancel it later */
            }
			return;
	     }
         else {   /* use alert method */ 
            datarec->current_status = '3';  
            active_recordnum = SCHED_ACTIVE_write_record( datarec, active_recordnum );
            if (active_recordnum < 0) {
               myprintf(  "*ERR: SE044-Unable to update SCHEDULER-NEWDAY to failed status in active jobs file, SUSPENDING SERVICES\n" );
               pSCHEDULER_CONFIG_FLAGS.enabled = '0';
            }
            myprintf(  "*ERR: SE045-SCHEDULER-NEWDAY, JOBS ARE STILL ON THE ACTIVE QUEUE, RESTART WHEN ALL JOBS COMPLETED\n" );
            ALERTS_generic_system_alert( (job_header_def *)datarec, "JOBS PENDING, RESTART WHEN SCHED-Q EMPTY" );
            if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARS) {
               myprintf(  "DEBUG: Setting SCHED_init_required = 1 (SERVER_spawnsimulate_task)\n" ); 
            }
            SCHED_init_required = 1; 
            return;
	     } /* end of if alert method */
	  }
	  else if ((active_count == -1) || (alerts_count == -1)) { /* errors reading active job queue or alert queue */
          myprintf(  "*ERR: SE072-SCHEDULER-NEWDAY, DATABASE PROBLEMS PREVENT EXECUTION\n" );
          ALERTS_generic_system_alert_MSGONLY( (job_header_def *)datarec, "CANNOT RUN, I-O ERRORS ON ACTIVE JOB OR ALERT QUEUE" );
          pSCHEDULER_CONFIG_FLAGS.enabled = '0';
          return;
	  }
	  if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
	     myprintf(  "INFO: SI027-SCHEDULER-NEWDAY EXEUCUTING\n" );
	  }

      /* If we issued an alert for the scheduler newday being in a alert
       * state, clear it now */
      if (GLOBAL_newday_alert_outstanding != 0) {
         ALERTS_generic_clear_remote_alert( (job_header_def *)datarec, "SCHEDULER-NEWDAY NOW RUNNING" );
         GLOBAL_newday_alert_outstanding = 0;  /* Record, so we know not to cancel again */
      } /* if alert outstanding */

      /* first record we have run the newday job in the config file */
	  if (pSCHEDULER_CONFIG_FLAGS.catchup_flag == '2') { /* we need to run for each day
															so use the date supposed to run at */
         if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
            myprintf( "INFO: SI028-Scheduling next NEWDAY job, catchup flag is ON\n" );
		 }
		 strncpy(pSCHEDULER_CONFIG_FLAGS.last_new_day_run, datarec->next_scheduled_runtime,JOB_DATETIME_LEN);
		 UTILS_space_fill_string( pSCHEDULER_CONFIG_FLAGS.last_new_day_run, JOB_DATETIME_LEN );
	     time_value = UTILS_make_timestamp( (char *)&pSCHEDULER_CONFIG_FLAGS.last_new_day_run );
		 time_value = time_value + (60 * 60 * 24);
	     UTILS_datestamp( (char *) &pSCHEDULER_CONFIG_FLAGS.next_new_day_run, time_value );
	  }
	  else {  /* else set that we are running at the current time */
         if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
            myprintf( "INFO: SI029-Scheduling next NEWDAY job, catchup flag is OFF\n" );
		 }
	     UTILS_datestamp( (char *) &pSCHEDULER_CONFIG_FLAGS.last_new_day_run, 0 );
	     /* We have a new field used for scheduling now, a next_new_day_run. Set
	      * this up now also. */
	     time_now = time(0);
		 time_now = time_now + (60 * 5); /* fudge 5 minutes forward */
	     time_value = UTILS_make_timestamp( (char *)&pSCHEDULER_CONFIG_FLAGS.last_new_day_run );
	     time_value = time_value - (60 * 60 * 5); /* Allow for it running up to 5 hours late */
	     UTILS_datestamp( (char *) &pSCHEDULER_CONFIG_FLAGS.next_new_day_run, time_value );
	     ptr = (char *)&pSCHEDULER_CONFIG_FLAGS.next_new_day_run;
	     ptr = ptr + 9; /* skip yyyymmdd_ */
	     strncpy( ptr, pSCHEDULER_CONFIG_FLAGS.new_day_time, 5 );
	     time_value = UTILS_make_timestamp( (char *)&pSCHEDULER_CONFIG_FLAGS.next_new_day_run );
	     while (time_value <= time_now) {
		     time_value = time_value + (60 * 60 * 24); /* increment by days */
	     }
	     UTILS_datestamp( (char *) &pSCHEDULER_CONFIG_FLAGS.next_new_day_run, time_value );
      }

	  /* Save the changes to the config record */
	  if ( CONFIG_update( (internal_flags * )&pSCHEDULER_CONFIG_FLAGS ) == 0) { /* error */
	     myprintf(  "*ERR: SE046-Unable to update config record with last newday run time, continuing...\n" );
	  }
	  else {
        if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
            myprintf(  "INFO: SI030-New day job last run time now set to %s\n",
                    pSCHEDULER_CONFIG_FLAGS.last_new_day_run );
            myprintf(  "INFO: SI031-New day job will next run at %s\n",
                    pSCHEDULER_CONFIG_FLAGS.next_new_day_run );
        }
	  }

	  /* for a newday we want to cleanup all the files we have been growing */
	  /* this is why this block of code is in the main server, we need all the
	   * files */
	  SERVER_newday_cleanup();
	  
	  /* Then do the normal newdays processing */
	  time_value = UTILS_make_timestamp( (char *)&datarec->next_scheduled_runtime );
	  JOBS_newday_scheduled( time_value );
	  SCHED_Submit_Newday_Job();
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARS) {
         myprintf(  "DEBUG: Setting SCHED_init_required = 1 (SERVER_spawnsimulate_task)\n" ); 
      }
	  SCHED_init_required = 1;
      SCHED_init_memcounters_required = 1;
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
	     myprintf(  "INFO: SI032-SCHEDULER-NEWDAY COMPLETED\n" );
      }
   }	  

   /* -----------------------------------------------------------------
	* An Unknown internal job type
	* ----------------------------------------------------------------- */
   else {
      myprintf(  "*ERR: SE047-INTERNAL JOB TASK %s IS NOT RECOGNISED, DELETING IT !\n",
              datarec->job_header.jobname );
      active_recordnum = SCHED_ACTIVE_delete_record( datarec, 1 );
      if (active_recordnum < 0) {
         myprintf(  "*ERR: SE048-DELETE OF JOB %s FAILED\n", datarec->job_header.jobname );
      }
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving SERVER_spawnsimulate_task\n" );
   }
} /* SERVER_spawnsimulate_task */

/* =========================================================
 * process_client_request_alert_delete:
 * 
 * called from process_client_request_alert when an alert is
 * to be deleted.
 * It is a cut down version of a normal job completion handling.
 *
 * This is in its own proc as it needs to do a lot of work,
 *   - either set to completed and delete the associated
 *     job record with the correct completed time, or if the
 *     job is a repeating job reschedule it back onto the active
 *     queue and reactivete it.
 *   - delete the associated alert record.
 *   - release any job dependencies if any
 *   - check & write logging info as required.
   ========================================================= */
void process_client_request_alert_delete( char *buffer ) {
   int operation_status;
   long recnum, mem_needed;
   API_Header_Def * api_bufptr;
   alertsfile_def * pAlerts_Rec;
   active_queue_def * pActive_Job_Rec;

   api_bufptr = (API_Header_Def *)buffer;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG:in server.c, process_client_request_alert_delete\n" );
   }

   /* Determine how much memory we need, we want to fit in both these
	* structures so allocate the larger amount */
   if (sizeof(active_queue_def) > sizeof(alertsfile_def)) {
		   mem_needed = sizeof(active_queue_def);
   }
   else {
		   mem_needed = sizeof(alertsfile_def);
   }
   if ( (pActive_Job_Rec = (active_queue_def *)MEMORY_malloc(mem_needed,"process_client_request_alert_delete")) != NULL) {
         memcpy( pActive_Job_Rec->job_header.jobnumber, api_bufptr->API_Data_Buffer, sizeof(job_header_def) );
         if (pSCHEDULER_CONFIG_FLAGS.log_level > 0) {
		    myprintf(  "WARN: SW012-Received request to clear alert information and set completed OK for %s\n", pActive_Job_Rec->job_header.jobname );
         }
		 /* saved api buffer data, so ok to overwrite now, with defaults */
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
         strcpy( api_bufptr->API_System_Name, "ALERT" );
	     strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
         /* change the active record to completed/deleted */
         recnum = SCHED_ACTIVE_read_record( pActive_Job_Rec );
	     if (recnum >= 0) {
              /* adjust the job completed time, check if we re-queue it also */
              if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
		         myprintf(  "INFO: SI033-     Adjusting completion timestamp for %s\n", pActive_Job_Rec->job_header.jobname );
              }
              operation_status = JOBS_completed_job_time_adjustments( pActive_Job_Rec );
              if (operation_status == 1) { /* job needs requeueing */
                 if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARS) {
                    myprintf(  "DEBUG: Setting SCHED_init_required = 1 (process_client_request_alert_delete)\n" ); 
                 }
                 SCHED_init_required = 1;
                 pActive_Job_Rec->job_header.info_flag = 'A';   /* reactivate it */
	             pActive_Job_Rec->current_status = '0';         /* back to time wait */
		         myprintf(  "JOB-:SI044-    Job %s, requeued to %s", pActive_Job_Rec->job_header.jobname, ctime( (time_t *)&pActive_Job_Rec->run_timestamp ) );
			  }
			  else {
                 if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
		            myprintf(  "INFO:SI045-    Job %s being set to completed OK\n", pActive_Job_Rec->job_header.jobname );
                 }
			     pActive_Job_Rec->job_header.info_flag = 'D';  /* deleted */
			     pActive_Job_Rec->current_status = '4';        /* completed */
			  }
              recnum = SCHED_ACTIVE_write_record( pActive_Job_Rec, recnum );
			  if (recnum >= 0) {
					  /* set the alerts rec to the active rec header, we have
					   * overwritten the alerts rec we were provided with by
					   * now */
					  pAlerts_Rec = (alertsfile_def *)&pActive_Job_Rec->job_header.jobnumber;
                      if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
		                 myprintf(  "INFO:SI046-    Deleting all alerts for Job %s\n", pActive_Job_Rec->job_header.jobname );
                      }
					  recnum = ALERTS_delete_alert( pAlerts_Rec );
					  if (recnum >= 0) {
	                     strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
						 strcpy( api_bufptr->API_Data_Buffer, "*I* Alert details cleared, job now in completed state.\n" );
					  }
					  else { /* alert delete failed */
						 strcpy( api_bufptr->API_Data_Buffer, "*E* Unable to delete Alert record, active queue record cleared OK however\n" );
					  }
					  /* Do the dependency deltions for the job now */
                      if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
		                 myprintf(  "INFO:SI047-    Adjusting jobs that were dependant upon Job %s\n", pActive_Job_Rec->job_header.jobname );
                      }
					  SCHED_DEPEND_delete_all_relying_on_job( (job_header_def *)&pActive_Job_Rec->job_header.jobnumber );
			  }
			  else { /* active write record failed */
				 strcpy( api_bufptr->API_Data_Buffer, "*E* Unable to update active queue record, no changes made !\n" );
			  }
	     }
		 else { /* active read record failed */
			 strcpy( api_bufptr->API_Data_Buffer, "*E* Unable to read active queue record, no changes made !\n" );
		 }
		 MEMORY_free( pActive_Job_Rec );
	  }
	  else { /* malloc failed */
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
         strcpy( api_bufptr->API_System_Name, "ALERT" );
	     strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
		 snprintf( api_bufptr->API_Data_Buffer, (MAX_API_DATA_LEN - 1),
                 "*E* Unable to allocate %d bytes of memory on server, request failed !\n",
				 (int)mem_needed );
  }
  API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG:Leaving process_client_request_alert_delete\n" );
   }

  return;
} /* process_client_request_alert_delete */

/* =========================================================
 * process_client_request_alert:
 * 
 * called from process_client_request when the request is for the alert 
 * subsystem. Determine if it is a command we can process, if so call the
 * appropriate support library routines.
   ========================================================= */
void process_client_request_alert( FILE *tx, char *buffer ) {
   int datalen;
   long recnum;
   API_Header_Def * api_bufptr;
   alertsfile_def * pAlerts_Rec;
   active_queue_def * pActive_Job_Rec;
   char local_jobname[JOB_NAME_LEN+1];

   api_bufptr = (API_Header_Def *)buffer;
   pAlerts_Rec = (alertsfile_def *)api_bufptr->API_Data_Buffer;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG:in server.c, process_client_request_alert\n" );
   }

   /* We use the API data area to read the alerts record into, make sure
    * it is going to be big enough */
   if (MAX_API_DATA_LEN < sizeof(alertsfile_def)) {
      myprintf(  "*ERR:SE049-process_client_request_alert, the data buffer is too small to use this procedure, PROGRAMMER ERROR\n" );
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "ALERT" );
	  strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	  strcpy( api_bufptr->API_Data_Buffer, "*E* BUFFER TO SMALL FOR OPERATION: PROGRAMMER ERROR\n" );
      API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
      if (API_flush_buffer( api_bufptr, tx ) != 0) {
         die("process_client_request_alert", DIE_INTERNAL);
      }
	  return;
   }

   strncpy( local_jobname, pAlerts_Rec->job_details.job_header.jobname, JOB_NAME_LEN );
   UTILS_space_fill_string( (char *)&local_jobname, JOB_NAME_LEN );
   UTILS_space_fill_string( (char *)&pAlerts_Rec->job_details.job_header.jobname, JOB_NAME_LEN );

   /* see what command we need to process */
   if (memcmp(api_bufptr->API_Command_Number, API_CMD_LISTALL, API_CMD_LEN) == 0) {
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "ALERT" );
	  strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
	  ALERTS_display_alerts( api_bufptr, tx ); /* note: set datalen done by display_alerts */
   }
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_RETRIEVE, API_CMD_LEN) == 0) {
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "ALERT" );
	  strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
      datalen = ALERTS_show_alert_detail( (char *)&local_jobname, api_bufptr->API_Data_Buffer, MAX_API_DATA_LEN - 1  );
      API_set_datalen( (API_Header_Def *)api_bufptr, datalen );
   }
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_ALERT_FORCEOK, API_CMD_LEN) == 0) {
		   process_client_request_alert_delete( buffer );
   }
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_ALERT_ACK, API_CMD_LEN) == 0) {
      recnum = ALERTS_read_alert( pAlerts_Rec );
	  if (recnum < 0) {
         myprintf(  "*ERR:SE050-Unable to read alert record for %s !\n", pAlerts_Rec->job_details.job_header.jobname );
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
         strcpy( api_bufptr->API_System_Name, "ALERT" );
	     strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
         strcpy( api_bufptr->API_Data_Buffer, "*E* UNABLE TO READ ALERT RECORD !\n" );
	  }
	  else {
	     pAlerts_Rec->acknowledged = 'Y';
	     pAlerts_Rec->severity = '1';
		 recnum = ALERTS_write_alert_record( pAlerts_Rec, recnum );
		 if (recnum < 0) {
            myprintf(  "*ERR:SE051-Unable to update alert record for %s!\n", pAlerts_Rec->job_details.job_header.jobname );
            API_init_buffer( (API_Header_Def *)api_bufptr ); 
            strcpy( api_bufptr->API_System_Name, "ALERT" );
	        strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
            strcpy( api_bufptr->API_Data_Buffer, "*E* UNABLE TO UPDATE ALERT RECORD !\n" );
		 }
		 else {
            API_init_buffer( (API_Header_Def *)api_bufptr ); 
            strcpy( api_bufptr->API_System_Name, "ALERT" );
	        strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
            strcpy( api_bufptr->API_Data_Buffer, "ALERT ACKNOWLEDGED !\n" );
		 }
	  }
	  API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
   }
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_SCHEDULE_ON, API_CMD_LEN) == 0) {
      if ( (pActive_Job_Rec = (active_queue_def *)MEMORY_malloc(sizeof(active_queue_def),"process_client_request_alert")) == NULL) {
		 myprintf(  "*ERR:SE052-process_client_request_alert, Unable to allocate %d bytes of memory\n", sizeof(active_queue_def) );
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
	     strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
         strcpy( api_bufptr->API_Data_Buffer, "*ERR: INSUFFICIENT MEMORY ON SERVER FOR THIS OPERATION !\n" );
	  }
	  else { /* malloc was ok */
         /* put a header into it for the job we are playing with */
         memcpy( pActive_Job_Rec->job_header.jobnumber, pAlerts_Rec->job_details.job_header.jobnumber, sizeof(job_header_def) );
		 /* delete the alert record */
         recnum = ALERTS_delete_alert( pAlerts_Rec );
	     if (recnum < 0) {
		    myprintf(  "*ERR:SE053-process_client_request_alert, Unable to delete alert for %s\n",
			        pAlerts_Rec->job_details.job_header.jobname );
            API_init_buffer( (API_Header_Def *)api_bufptr ); 
	        strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
            strcpy( api_bufptr->API_Data_Buffer, "*E* UNABLE TO DELETE ALERT RECORD, SEE SERVER LOG FOR ERRORS !\n" );
	     }
	     else {   /* alert record was deleted ok */
            recnum = SCHED_ACTIVE_read_record( pActive_Job_Rec );
            if (recnum < 0) {
		       myprintf(  "*ERR:SE054-process_client_request_alert, Unable to read active queue for %s to reset failure flag\n",
					   pActive_Job_Rec->job_header.jobname );
               API_init_buffer( (API_Header_Def *)api_bufptr ); 
	           strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
               strcpy( api_bufptr->API_Data_Buffer, "*E* UNABLE TO READ ACTIVE QUEUE RECORD, SEE SERVER LOG FOR ERRORS !\n" );
		    }
            else {  
	           /* mark the active queue record as runable */
	           pActive_Job_Rec->current_status = '0'; /* runnable */
               recnum = SCHED_ACTIVE_write_record( pActive_Job_Rec, recnum );
               if (recnum < 0) {
		          myprintf(  "*ERR:SE055-process_client_request_alert, Unable to reset failure flag on %s in active queue file\n",
						  pActive_Job_Rec->job_header.jobname );
                  API_init_buffer( (API_Header_Def *)api_bufptr ); 
	              strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
                  strcpy( api_bufptr->API_Data_Buffer, "*E* UNABLE TO RESET ACTIVE QUEUE RECORD, SEE SERVER LOG FOR ERRORS !\n" );
		       } /* else active rec was updated ok */
            } /* else active rec was read ok */
            API_init_buffer( (API_Header_Def *)api_bufptr ); 
	        strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
            strcpy( api_bufptr->API_Data_Buffer, "JOB RESCHEDULED ONTO ACTIVE QUEUE\n" );
            if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARS) {
               myprintf(  "DEBUG: Setting SCHED_init_required = 1 (process_client_request_alert)\n" ); 
            }
			SCHED_init_required = 1;  /* lets refresh our nextrun info */
		 }  /* else alert record was deleted ok */ 

		 MEMORY_free( pActive_Job_Rec );

      } /* else malloc was OK */
		   
      strcpy( api_bufptr->API_System_Name, "ALERT" );
	  API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
   }
   else {	  
      myprintf(  "*ERR:SE056-API request for %s, request code %s not recognised !\n",
         	   api_bufptr->API_System_Name, api_bufptr->API_Command_Number );
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "ALERT" );
	  strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
      strcpy( api_bufptr->API_Data_Buffer, "API ALERT request code was not recognised by the server\n" );
	  API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
   }

   /* write the reponse */
   if (API_flush_buffer( api_bufptr, tx ) != 0) {
      die("process_client_request_alert", DIE_INTERNAL);
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving process_client_request_alert\n" );
   }
} /* process_client_request_alert */

/* =========================================================
 * process_client_request_calendar:
 * 
 * called from process_client_request when the request is for the calendar
 * subsystem. Determine if it is a command we can process, if so call the
 * appropriate support library routines.
   ========================================================= */
void process_client_request_calendar( FILE *tx, char *buffer ) {
   long rqst_result;
   API_Header_Def * api_bufptr;
   calendar_def * datarec;

   api_bufptr = (API_Header_Def *)buffer;
   datarec = (calendar_def *)api_bufptr->API_Data_Buffer;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG:in server.c, process_client_request_calendar\n" );
   }

   /* We use the API data area to read the alerts record into, make sure
    * it is going to be big enough */
   if (MAX_API_DATA_LEN < sizeof(calendar_def)) {
      myprintf(  "*ERR:SE057-process_client_request_calendar, the data buffer is too small to use this procedure, PROGRAMMER ERROR\n" );
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "CAL" );
	  strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	  strcpy( api_bufptr->API_Data_Buffer, "*E* BUFFER TO SMALL FOR OPERATION: PROGRAMMER ERROR\n" );
      API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
      if (API_flush_buffer( api_bufptr, tx ) != 0) {
         die("process_client_request_calendar", DIE_INTERNAL);
      }
	  return;
   }

   if ( (memcmp(api_bufptr->API_Command_Number, API_CMD_CALENDAR_ADD, API_CMD_LEN) == 0) ||
        (memcmp(api_bufptr->API_Command_Number, API_CMD_CALENDAR_MERGE, API_CMD_LEN) == 0) ||
        (memcmp(api_bufptr->API_Command_Number, API_CMD_CALENDAR_UNMERGE, API_CMD_LEN) == 0) ||
        (memcmp(api_bufptr->API_Command_Number, API_CMD_CALENDAR_DELETE, API_CMD_LEN) == 0)
      ) {
      if (memcmp(api_bufptr->API_Command_Number, API_CMD_CALENDAR_ADD, API_CMD_LEN) == 0) {
         rqst_result = CALENDAR_add( datarec );  /* must set rqst_result, used outside block */
      }
	  else if (memcmp(api_bufptr->API_Command_Number, API_CMD_CALENDAR_MERGE, API_CMD_LEN) == 0) {
         rqst_result = CALENDAR_merge( datarec );  /* must set rqst_result, used outside block */
      }
	  else if (memcmp(api_bufptr->API_Command_Number, API_CMD_CALENDAR_UNMERGE, API_CMD_LEN) == 0) {
         rqst_result = CALENDAR_unmerge( datarec );  /* must set rqst_result, used outside block */
      }
	  else { /* only delete left */
         rqst_result = CALENDAR_delete( datarec );  /* must set rqst_result, used outside block */
      }
      if (rqst_result != -1) {
         /* If there was a calendar change check any jobs that may have been affected */
         if (datarec->calendar_type == '0') { /* Job calendar only */
            JOBS_recheck_job_calendars( datarec ); /* any scheduling change ? */
         }
         else if (datarec->calendar_type == '1') { /* Holiday calendar, may affect all jobs */
            JOBS_recheck_every_job_calendar(); /* any scheduling change ? */
         }
         /* and then format the response */
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
         strcpy( api_bufptr->API_System_Name, "CAL" );
	     strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
         strcpy( api_bufptr->API_Data_Buffer, "Calendar request has been processed by the server\n" );
	     API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
	  }
      else {
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
         strcpy( api_bufptr->API_System_Name, "CAL" );
	     strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
		 UTILS_get_message( api_bufptr->API_Data_Buffer, MAX_API_DATA_LEN, 1 );
	     API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
      }
   }
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_LISTALL, API_CMD_LEN) == 0) {
      CALENDAR_format_list_for_display( api_bufptr, tx );
   }
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_RETRIEVE, API_CMD_LEN) == 0) {
      myprintf(  "*ERR:SE058-process_client_request_calendar\nRetrieval for display not yet implemented\n" );
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "CAL" );
	  strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
      strcpy( api_bufptr->API_Data_Buffer, "Calendar retrieval requests have not yet been implemented, use RAW method\n" );
	  API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
   }
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_RETRIEVE_RAW, API_CMD_LEN) == 0) {
      /* Fully tested, used prior to the retrive request above when the client
	   * did the formatting of the data */
      CALENDAR_put_record_in_API( api_bufptr );
   }
   else {	  
      myprintf(  "*ERR:SE059-API request for %s, request code %s not recognised !\n",
         	   api_bufptr->API_System_Name, api_bufptr->API_Command_Number );
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "CAL" );
	  strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
      strcpy( api_bufptr->API_Data_Buffer, "API CALENDAR request code was not recognised by the server\n" );
	  API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
   }

   /* write the reponse */
   if (API_flush_buffer( api_bufptr, tx ) != 0) {
      die("process_client_request_alert", DIE_INTERNAL);
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving process_client_request_calendar\n" );
   }
} /* process_client_request_calendar */

/* =========================================================
 * Needs to be in the main server body to update the
 * client table without a lot of hassle.
 * auto-auth form is : "auto-login PID-AUTH %s",pid_string
 * manual form is    : "%s %s %s", user_name, user_pswd, pid_string
   ========================================================= */
void process_logon_request( API_Header_Def * api_bufptr, int connect_index ) {
   char *ptr, *ptr2;
   long pid_number;
   int rejected, auto_allowed;
   USER_record_def * local_user_rec;   /* We use user records now */
   long user_record_pos;
   char user_pswd[USER_MAX_PASSLEN+1];
   char local_saved_user[USER_MAX_NAMELEN+1];

   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) ||
        (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_PROC) ) {
      myprintf(  "DEBUG: Entered process_logon_request\n" );
   }

   if ( (local_user_rec = (USER_record_def *)MEMORY_malloc(sizeof(USER_record_def),"process_logon_request")) == NULL) {
      myprintf( "*ERR:SE060-Insufficient memory to allocate %d bytes of memory (process_logon_request)\n",
                sizeof(USER_record_def) );
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "SCHED" );
      strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
      strcpy( api_bufptr->API_Data_Buffer, "*ERR: Insufficient free memory available on server for request\n\0" );
      API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
      return;
   }

   rejected = 0;  /* not rejected yet */
   auto_allowed = 0;  /* not auto-logged on yet */

   /* find the end of the user name and null terminate it */
   ptr = (char *)&api_bufptr->API_Data_Buffer;
   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARS) ||
        (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_VARS) ) {
      myprintf(  "DEBUG: Data passed = '%s'\n", ptr );
   }

   while ((*ptr != ' ') && (*ptr != '\0')) { ptr++; }
   *ptr = '\0';
   strncpy(local_user_rec->user_name, api_bufptr->API_Data_Buffer, USER_MAX_NAMELEN);
   UTILS_space_fill_string( (char *)&local_user_rec->user_name, USER_MAX_NAMELEN );
   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARS) ||
        (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_VARS) ) {
      myprintf(  "DEBUG: Search key about to pass to USER_read is '%s'\n", local_user_rec->user_name );
   }
   strncpy( local_saved_user, local_user_rec->user_name, (USER_MAX_NAMELEN+1) ); /* +1, include null */
   user_record_pos = USER_read(local_user_rec);
   if (user_record_pos == -1) {
      if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARS) ||
           (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_VARS) ) {
         myprintf(  "DEBUG: No user record found for user %s\n", api_bufptr->API_Data_Buffer );
      }
      rejected = 1; /* No user record, reject */
   }
   else {  /* Record was read OK */
      /* Got username, skip spaces to the password */
      ptr++;   /* skip the null we put at the end of the username */
      while ((*ptr == ' ') && (*ptr != '\0')) { ptr++; }
      ptr2 = ptr;
      while ((*ptr2 != ' ') && (*ptr2 != '\0')) { ptr2++; }
      *ptr2 = '\0';
      strncpy(user_pswd, ptr, USER_MAX_PASSLEN);
      UTILS_space_fill_string( (char *)&user_pswd, USER_MAX_PASSLEN );

	  /* we have saved the password, find the pid provided now */
      ptr = ptr2; /* where we put the null */
	  ptr++;      /* skip the null */
      while ((*ptr == ' ') && (*ptr != '\0')) { ptr++; } /* skip spaces */
      ptr2 = ptr;
      while ((*ptr2 != ' ') && (*ptr2 != '\0') && (*ptr2 != '\n')) { ptr2++; }
      *ptr2 = '\0';
      pid_number = atoi(ptr);

      /* Check the password, PID-AUTH is a special case */
      /* allow the logon if theuser record allows local_auto_auth */
      if ( (strncmp(user_pswd,"PID-AUTH",8) == 0) && (local_user_rec->local_auto_auth == 'Y') ) {
         strncpy(client[connect_index].user_name, local_user_rec->user_name, USER_MAX_NAMELEN);
         UTILS_space_fill_string( (char *)&client[connect_index].user_name, USER_MAX_NAMELEN );
         client[connect_index].user_pid = pid_number;
         client[connect_index].access_level = local_user_rec->user_auth_level;
		 auto_allowed = 1;
      }
      else if (strncmp(user_pswd,"PID-AUTH",8) == 0) {
         rejected = 2;  /* no autoauth access */
      }
      else {
         UTILS_encrypt_local( (char *)&user_pswd, USER_MAX_PASSLEN );
         if (memcmp(user_pswd,local_user_rec->user_password,USER_MAX_PASSLEN) == 0) {
            strncpy(client[connect_index].user_name, local_user_rec->user_name, USER_MAX_NAMELEN);
            UTILS_space_fill_string( (char *)&client[connect_index].user_name, USER_MAX_NAMELEN );
            client[connect_index].user_pid = pid_number;
            client[connect_index].access_level = local_user_rec->user_auth_level;
	     }
         else {
            if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARS) ||
                 (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_VARS) ) {
               myprintf(  "DEBUG: Password mismatch for user %s\n", local_user_rec->user_name );
            }
            rejected = 1; /* rejected */
	     }
      }
      if (rejected == 0) { /* logon will be allowed */
          UTILS_datestamp( local_user_rec->last_logged_on, UTILS_timenow_timestamp() );
          if (USER_update_admin_allowed( local_user_rec, user_record_pos) != 1) { /* failed to update */
             myprintf( "*ERR:SE061-Unable to update last logged on timestamp for '%s'\n", local_saved_user );
          }
      }
   }  /* record was read OK */

   /* Free the local user record now */
   MEMORY_free( local_user_rec );

   /* Log the correct message and format the response to the client */
   API_init_buffer( (API_Header_Def *)api_bufptr ); 
   strcpy( api_bufptr->API_System_Name, "SCHED" );
   if (rejected == 0) {
       if (auto_allowed != 0) {
          myprintf( "AUTH:SI048-Auto-Logon to level %c by %s from %s (pid=%d)\n",
                      client[connect_index].access_level,
                      client[connect_index].user_name,
                      client[connect_index].remoteaddr, pid_number );
       }
       else {
          myprintf( "AUTH:SI049-Manual Logon to level %c by %s from %s (pid=%d)\n",
                       client[connect_index].access_level,
                       client[connect_index].user_name,
                       client[connect_index].remoteaddr, pid_number );
       }
       strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
	   if (client[connect_index].access_level == 'A') {
          strcpy( api_bufptr->API_Data_Buffer, "AUTH GRANTED: Access level now ADMIN\n\n\0" );
	   }
	   else if (client[connect_index].access_level == 'O') {
          strcpy( api_bufptr->API_Data_Buffer, "AUTH GRANTED: Access level now OPERATOR\n\n\0" );
	   }
	   else if (client[connect_index].access_level == 'J') {
          strcpy( api_bufptr->API_Data_Buffer, "AUTH GRANTED: Access level now JOB Definition Management\n\n\0" );
	   }
	   else if (client[connect_index].access_level == 'S') {
          strcpy( api_bufptr->API_Data_Buffer, "AUTH GRANTED: Access level now SECURITY Profile Management\n\n\0" );
	   }
	   else if (client[connect_index].access_level == '0') {
          strcpy( api_bufptr->API_Data_Buffer, "AUTH GRANTED: Access level now BROWSE-ONLY\n\n\0" );
	   }
	   else {
          snprintf( api_bufptr->API_Data_Buffer, (MAX_API_DATA_LEN - 1), "AUTH GRANTED: Access level now '%c'\n\n",
                  client[connect_index].access_level);
	   }
   }
   else { /* Was rejected */
       myprintf( "SEC-:SW013-REFUSED Logon for '%s' from %s\n",
                    local_saved_user,
                    client[connect_index].remoteaddr );
       strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
       if (rejected == 2) {
          strcpy( api_bufptr->API_Data_Buffer, "*ERR: You do not have autologin privalidges, use a username and password\n\n\0" );
       }
       else {
          strcpy( api_bufptr->API_Data_Buffer, "*ERR: Authorisation details were incorrect, rejected\n\n\0" );
       }
   }

   /* Set the data length of the reply */
   API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
   /* DO NOT write a response, that is done by our caller */

   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) ||
        (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_PROC) ) {
      myprintf(  "DEBUG: leaving process_logon_request\n" );
   }
} /* process_logon_request */


/* =========================================================
 * Needs to be in the main server body to update the
 * client table without a lot of hassle.
 * Reset the auth flag to guest/browse access only.
 * Added so servlet pages can release an entry from the
 * connection pool without having to close the connection.
   ========================================================= */
void process_logoff_request( API_Header_Def * api_bufptr, int connect_index ) {
   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) ||
        (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_PROC) ) {
      myprintf(  "DEBUG: Entered process_logoff_request\n" );
   }

   /* Log the correct message */
   myprintf( "AUTH:SI082-Logoff to default guest level by %s from %s\n",
             client[connect_index].user_name,
             client[connect_index].remoteaddr );

   /* Uodate the server connection table to show the new logged off state */
   client[connect_index].access_level = '0';
   strncpy(client[connect_index].user_name, "guest", USER_MAX_NAMELEN);
   UTILS_space_fill_string( (char *)&client[connect_index].user_name, USER_MAX_NAMELEN );

   /* format the response to the client */
   API_init_buffer( (API_Header_Def *)api_bufptr ); 
   strcpy( api_bufptr->API_System_Name, "SCHED" );
   strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
   strcpy( api_bufptr->API_Data_Buffer, "AUTH GRANTED: Access level now BROWSE-ONLY\n\n\0" );

   /* Set the data length of the reply */
   API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
   /* DO NOT write a response, that is done by our caller */

   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) ||
        (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_PROC) ) {
      myprintf(  "DEBUG: leaving process_logoff_request\n" );
   }
} /* process_logoff_request */


/* =========================================================
   ========================================================= */
void show_connected_sessions( API_Header_Def * api_bufptr, FILE * tx ) {
   int z, longest_name, longest_ipaddr, datalen;
   char username[USER_MAX_NAMELEN+1];
   char ipaddress[51];
   char process_id[51];
   char bigbuf[170];

   longest_name = 0;
   longest_ipaddr = 0;
   for ( z = 0; z < MAX_CLIENTS; ++z ) { 
      if (client[z].tx != NULL) { 
         if (strlen(client[z].remoteaddr) > longest_ipaddr) { longest_ipaddr = strlen(client[z].remoteaddr); }
         strncpy(username, client[z].user_name, USER_MAX_NAMELEN);
         username[USER_MAX_NAMELEN] = '\0';
         UTILS_remove_trailing_spaces( (char *)&username );
         if (strlen(username) > longest_name) { longest_name = strlen(username); }
      }
   }

   for ( z = 0; z < MAX_CLIENTS; ++z ) { 
      if (client[z].tx != NULL) { 
         strncpy(username, client[z].user_name, USER_MAX_NAMELEN);
         UTILS_remove_trailing_spaces( (char *)&username );
         UTILS_space_fill_string( (char *)&username, longest_name );
         strncpy(ipaddress, client[z].remoteaddr, 50);
         UTILS_space_fill_string( (char *)&ipaddress, longest_ipaddr );

         if (client[z].user_pid > 2) {
            snprintf( process_id, 50, "PID=%d", client[z].user_pid );
         }
		 else if (client[z].user_pid == 2) { /* special case JSP socket pools */
            strcpy( process_id, "JSP socket" );
         }
		 else if (client[z].user_pid == 1) { /* special case non-unix clients */
            strcpy( process_id, "PC client" );
         }
         else { /* must be zero, which is not logged on/guest */
            strcpy( process_id, "not logged on yet" );
         }
		 /* access level 0 (zero) looks too much like O (oh) so
          * display access level zero as a - for browse/no access */
         if (client[z].access_level == '0') {
            datalen = snprintf( bigbuf, 169, "%s %s (access level -) %s\n",
                      ipaddress, username, process_id );
         }
         else {
            datalen = snprintf( bigbuf, 169, "%s %s (access level %c) %s\n",
                      ipaddress, username, client[z].access_level, process_id );
         }
         API_add_to_api_databuf( api_bufptr, bigbuf, datalen, tx );
         if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
            myprintf(  "INFO:SI052- Show Sessions request, echoed for posterity\n" );
            myprintf(  "INFO:SI050-   %s", bigbuf );
         }
      }
   }
} /* show_connected_sessions */


/* =========================================================
 * process_client_request_sched:
 * 
 * called from process_client_request when the request is for the scheduler 
 * subsystem. Determine if it is a command we can process, if so call the
 * appropriate support library routines.
 * This has started to get quite big as I plug additional things in, I
 * will eventually need to clean this up and split things out.
   ========================================================= */
void process_client_request_sched( FILE *tx, char *buffer, int *shutdown_flag, int connect_index ) {
   long recnum;
   int datalen, runnow_status, shutdown_is_forced, z;
   char bigbuf[170];
   API_Header_Def * api_bufptr;
   job_header_def * job_header;
   char saved_job[JOB_NAME_LEN+1];

   api_bufptr = (API_Header_Def *)buffer;
   job_header = (job_header_def *)&api_bufptr->API_Data_Buffer;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG:in server.c, process_client_request_sched\n" );
   }

   if ( (memcmp(api_bufptr->API_Command_Number, API_CMD_SHUTDOWN, API_CMD_LEN) == 0) ||
        (memcmp(api_bufptr->API_Command_Number, API_CMD_SHUTDOWNFORCE, API_CMD_LEN) == 0) ) {
       *shutdown_flag = 1;
       myprintf(  "AUTH:SW014-SHUTDOWN REQUEST ACCEPTED, REQUESTED BY USER %s\n", client[connect_index].user_name ); 
	   /* save this before overwriting the buffer */
       if (memcmp(api_bufptr->API_Command_Number, API_CMD_SHUTDOWNFORCE, API_CMD_LEN) == 0) {
	      shutdown_is_forced = 1;
	   }
	   else {
          shutdown_is_forced = 0;
	   }
       API_init_buffer( (API_Header_Def *)api_bufptr ); 
       strcpy( api_bufptr->API_System_Name, "SCHED" );
	   strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
	   if (INTERNAL_count_executing() > 0L) {
          if (shutdown_is_forced == 1) {
             datalen = snprintf( api_bufptr->API_Data_Buffer, MAX_API_DATA_LEN,
                                 "SCHEDULER SHUTDOWN BEING FORCED, SENDING KILL TO %d CURRENTLY RUNNING JOBS\n",
                                 (int)INTERNAL_count_executing() );
		  }
		  else {
             datalen = snprintf( api_bufptr->API_Data_Buffer, MAX_API_DATA_LEN,
                                 "SCHEDULER SHUTDOWN QUEUED, WAITING FOR %d JOBS TO FINISH RUNNING\n",
                                 (int)INTERNAL_count_executing() );
		  }
		  pSCHEDULER_CONFIG_FLAGS.enabled = '2'; /* shutdown state */
	   }
	   else {
	      strcpy( api_bufptr->API_Data_Buffer, "*I* SHUTDOWN ACCEPTED, SCHEDULER TERMINATING\n" );
	   }
	   API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
       if (API_flush_buffer( api_bufptr, tx ) != 0) {
          /* ignore errors here, as the client (sched_cmd anyway)  will have
		   * exited after the issuing the shutdown request without hanging
		   * around for the response */
          /* die("process_client_request_sched", DIE_INTERNAL); */
       }
	   /* We print these even is the server is set to suppress INFO level
		* messages, this is something they may be glad to see later */
       if (shutdown_is_forced == 1) {
          myprintf(  "WARN:SW015- -*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-\n" );
		  myprintf(  "WARN:SW015- SHUTDOWN  F O R C E  REQUEST RECEIVED\n" );
		  myprintf(  "WARN:SW015-   SENDING KILL SIGNAL TO %d CURRENTLY RUNNING JOBS\n", (int)INTERNAL_count_executing() );
		  myprintf(  "WARN:SW015-   PENDING FAST TERMINATION...\n" );
          myprintf(  "WARN:SW015- -*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-\n" );
		  shutdown_on_force();
	   }
	   else {
          if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
             myprintf(  "INFO:SI051- -*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-\n" );
             myprintf(  "INFO:SI051- SHUTDOWN REQUEST HAS BEEN QUEUED !!!\n" );
	         myprintf(  "INFO:SI051-    NO MORE JOBS WILL BE PERMITTED TO START EXECUTION\n" );
	         myprintf(  "INFO:SI051-    SERVER WILL SHUTDOWN WHEN LAST EXECUTING JOB COMPLETES\n" );
		  }
          if (pSCHEDULER_CONFIG_FLAGS.log_level > 0) {
	         myprintf(  "WARN:SW016-    WAITING ON %d JOBS TO COMPLETE\n", (int)INTERNAL_count_executing() );
		  }
          if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
             myprintf(  "INFO:SI051- -*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-\n" );
		  }
	   }
       if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
          myprintf(  "DEBUG: leaving process_client_request_sched\n" );
       }
       return;
   }
  
   /* Normal command processing now */
   
   /* show the scheduler status flags */
   if (memcmp(api_bufptr->API_Command_Number, API_CMD_STATUS, API_CMD_LEN) == 0) {
       if (memcmp(api_bufptr->API_Data_Buffer,"ALERTINFO",9) == 0) {
          SCHED_display_server_status_alertsonly( api_bufptr, (internal_flags *)&pSCHEDULER_CONFIG_FLAGS, tx, (int)INTERNAL_count_executing() );
       }
       else if (memcmp(api_bufptr->API_Data_Buffer,"MEMINFO",7) == 0) {
          MEMORY_log_memory_useage();
          API_init_buffer( (API_Header_Def *)api_bufptr ); 
          strcpy( api_bufptr->API_System_Name, "SCHED" );
          strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
          strcpy( api_bufptr->API_Data_Buffer, "MEMORY INFORMATION WRITTEN TO LOGFILE AS REQUESTED\n" );
       }
       else { /* assume STANDARD, the original behaviour */
          SCHED_display_server_status( api_bufptr, (internal_flags *)&pSCHEDULER_CONFIG_FLAGS, tx, (int)INTERNAL_count_executing() );
       }
   }
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_STATUS_SHORT, API_CMD_LEN) == 0) {
       SCHED_display_server_status_short( api_bufptr, (internal_flags *)&pSCHEDULER_CONFIG_FLAGS, tx );
   }
   
   /* list all jobs on the scheduler active queue */
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_LISTALL, API_CMD_LEN) == 0) {
      /* Do NOT initialise the API buffer. SCHED_show_schedule wants to check
       * the data area. It will initialise the buffer ! */
	  if (SCHED_show_schedule( api_bufptr, tx  ) != 0) {
			  die( "SCHED_show_schedule", DIE_INTERNAL );
	  }
	  return; /* the show schedule does the writes as it may need multiple buffers */
   }
  
   /* Show connected sessions to this server */
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_SHOWSESSIONS, API_CMD_LEN) == 0) {
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "SCHED" );
      strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
	  if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
			  myprintf(  "INFO:SI052- Show Sessions request, echoed for posterity\n" );
	  }
      show_connected_sessions( api_bufptr, tx );
   }

   /* A Logon request */
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_LOGON, API_CMD_LEN) == 0) {
      process_logon_request( api_bufptr, connect_index );
   }

   /* A Logoff request */
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_LOGOFF, API_CMD_LEN) == 0) {
      process_logoff_request( api_bufptr, connect_index );
   }

   /* delete a job from the scheduler active queue */
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_DELETE, API_CMD_LEN) == 0) {
          if ( memcmp(job_header->jobname,"SCHEDULER",9) == 0) { 
             API_init_buffer( (API_Header_Def *)api_bufptr ); 
             strcpy( api_bufptr->API_System_Name, "SCHED" );
	         strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
             strcpy( api_bufptr->API_Data_Buffer, "*E* JOBS IN THE SCHEDULER* RANGE CANNOT BE MODIFIED\n" );
	      }
	      else {
		       /* SCHED_delete_job removes alerts and dependencies, so we don't
		        * need to worry about them here. */
             strncpy( saved_job, job_header->jobname, JOB_NAME_LEN );
             saved_job[JOB_NAME_LEN] = '\0';
    		 recnum = SCHED_delete_job( (job_header_def *)&api_bufptr->API_Data_Buffer );
             API_init_buffer( (API_Header_Def *)api_bufptr ); 
             strcpy( api_bufptr->API_System_Name, "SCHED" );
		     if (recnum < 0) {
    	        strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
    			UTILS_get_message( api_bufptr->API_Data_Buffer, MAX_API_DATA_LEN, 0 );
    			if (strlen(api_bufptr->API_Data_Buffer) < 10) {  /* nothing meaningful */
    		       strcpy( api_bufptr->API_Data_Buffer, "*E* UNABLE TO DELETE JOB, SEE LOG\n" );
    			}
    		 }
    		 else {
                myprintf("JOB-:SI053-Job %s deleted from scheduler by %s\n", saved_job, client[connect_index].user_name );
    	        strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
    		    strcpy( api_bufptr->API_Data_Buffer, "JOB DELETED FROM ACTIVE QUEUE, ANY DEPENDANT JOBS RELEASED\n" );
    		 }
    	  }
   }

   /* is it a request to run the selected job immediately */
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_RUNJOBNOW, API_CMD_LEN) == 0) {
          if (strncmp(job_header->jobname,"SCHEDULER",9) != 0) { /* scheduler jobs are for internal use */
               strncpy( saved_job, job_header->jobname, JOB_NAME_LEN );
               saved_job[JOB_NAME_LEN] = '\0';
		       runnow_status = SCHED_run_job_now( (active_queue_def *)job_header );
    		   if ( runnow_status == 0 ) {
                  if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
                     myprintf(  "INFO:SI054-RE-QUEUE (RUNNOW) completed for %s\n", job_header->jobname );
		    	  }
                  myprintf("JOB-:SI055-Job %s runnow executed by %s\n", saved_job, client[connect_index].user_name );
    	          strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
    		      strcpy( api_bufptr->API_Data_Buffer, "JOB HAS BEEN REQUEUED TO THE CURRENT TIME AND WILL RUN AS SOON AS POSSIBLE\n" );
    		   }
               else if ( runnow_status == 1 ) {
                  myprintf(  "*ERR:SE062-REQUEUE (RUNNOW) FAILED FOR JOB %s\n", job_header->jobname );
	              strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
		          strcpy( api_bufptr->API_Data_Buffer, "THE JOB WAS NOT ABLE TO BE RE-QUEUED, check the scheduler log file for errors\n" );
		       }
    		   else { /* must be waiting on something */
    	          strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
    		      strcpy( api_bufptr->API_Data_Buffer, "JOB IS NOT IN TIME WAIT !, WHILE DEPENDENCIES OR ALERTS EXIST YOU CANNOT RUNNOW.\n" );
    		   }
          }
	      else { /* was an internal SCHEDULER name in the request */
             strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
             strcpy( api_bufptr->API_Data_Buffer, "SCHEDULER* JOBS CANNOT BE MODIFIED !.\n" );
    	  }
   }

   /* job hold on or off request for job on the queue */
   else if (
    		     (memcmp(api_bufptr->API_Command_Number, API_CMD_JOBHOLD_ON, API_CMD_LEN) == 0) ||
                 (memcmp(api_bufptr->API_Command_Number, API_CMD_JOBHOLD_OFF, API_CMD_LEN) == 0)
	    	   )
       {
                 strncpy( saved_job, job_header->jobname, JOB_NAME_LEN );
                 saved_job[JOB_NAME_LEN] = '\0';
                 if (memcmp(api_bufptr->API_Command_Number, API_CMD_JOBHOLD_ON, API_CMD_LEN) == 0) {
	                z = SCHED_hold_job( (char *)&job_header->jobname, 1 /* 1 is turn on */ );
                 }
    			 else {
    	            z = SCHED_hold_job( (char *)&job_header->jobname, 0 /* 0 is turn off */ );
    			 }
                 API_init_buffer( (API_Header_Def *)api_bufptr ); 
                 strcpy( api_bufptr->API_System_Name, "SCHED" );
                 if (z != 0) {
                    strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
    		        strcpy( api_bufptr->API_Data_Buffer, "*E* JOB HOLD STATE CHANGE REQUEST FAILED, SEE SERVER LOG\n" );
    			 }
    			 else {
                    if (memcmp(api_bufptr->API_Command_Number, API_CMD_JOBHOLD_ON, API_CMD_LEN) == 0) {
                       myprintf("JOB-:SI056-Job %s placed in hold state by %s\n", saved_job, client[connect_index].user_name );
                    }
                    else {
                       myprintf("JOB-:SI057-Job %s cleared from hold state by %s\n", saved_job, client[connect_index].user_name );
                    }
                    strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
	    	        strcpy( api_bufptr->API_Data_Buffer, "JOB HOLD STATE CHANGE REQUEST COMPLETED\n" );
	    			SCHED_init_required = 1;  /* May need requeueing on or off */
	    		 }
   }

   /* Either pause of resume the scheduler */
   /* This should probably be changed to a config command as I now save the
      execjobs state back to the config file as there was a user request to retain
      the state over server restarts. But I'm being sloppy for now as leaving it
      here only required the addition of the config_update request.
   */
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_EXECJOB_STATE, API_CMD_LEN) == 0) {
          strncpy( bigbuf, api_bufptr->API_Data_Buffer, 5 ); /* save data we want */
          API_init_buffer( (API_Header_Def *)api_bufptr ); 
          strcpy( api_bufptr->API_System_Name, "SCHED" );
          strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
    	  if (memcmp(bigbuf,"OFF",3) == 0) {
             myprintf("JOB-:SW017-Job execution disabled for all jobs by %s\n", client[connect_index].user_name );
             pSCHEDULER_CONFIG_FLAGS.enabled = '0';
	    	 strcpy( api_bufptr->API_Data_Buffer, "SCHEDULER EXECJOBS DISABLED, NO MORE JOBS WILL START EXECUTING\n" );
             CONFIG_update( &pSCHEDULER_CONFIG_FLAGS );
    	  }
    	  else if (memcmp(bigbuf,"ON",2) == 0) {
             myprintf("JOB-:SW018-Job execution enabled for all jobs by %s\n", client[connect_index].user_name );
             pSCHEDULER_CONFIG_FLAGS.enabled = '1';
	    	 strcpy( api_bufptr->API_Data_Buffer, "SCHEDULER EXECJOBS ENABLED, JOBS CAN START EXECUTING WHEN READY\n" );
             CONFIG_update( &pSCHEDULER_CONFIG_FLAGS );
    	  }
    	  else {
             strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	    	 strcpy( api_bufptr->API_Data_Buffer, "*E* Invalid data in EXECJOBS API request, ignored\n" );
    	  }
   }

   /* else we don't know about this command */
   else {	  
      myprintf(  "WARN:SW019-Command %s either unknown or a restricted command being used by an unauthorised user!\n",
         	   api_bufptr->API_System_Name, api_bufptr->API_Command_Number );
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "SCHED" );
	  strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
      strcpy( api_bufptr->API_Data_Buffer, "API request not yet implemented !\n" );
   }

   /* write the reponse */
   API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
   if (API_flush_buffer( api_bufptr, tx ) != 0) {
      die("process_client_request_sched", DIE_INTERNAL);
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving process_client_request_sched\n" );
   }
} /* process_client_request_sched */


/* =========================================================
 * process_client_request_config:
 * 
 * called from process_client_request when the request is for the scheduler 
 * subsystem that is a configuration request.
   ========================================================= */
void process_client_request_config( FILE *tx, char *buffer, int connect_index ) {
   int z;
   char smallbuf[10];
   API_Header_Def * api_bufptr;
   job_header_def * job_header;
   time_t time_now;
   time_t test_time;
   char *ptr, *ptr2;
   char savetype;

   api_bufptr = (API_Header_Def *)buffer;
   job_header = (job_header_def *)&api_bufptr->API_Data_Buffer;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG:in server.c, process_client_request_config\n" );
   }

   /* set a new scheduler newday time */
   if (memcmp(api_bufptr->API_Command_Number, API_CMD_SCHEDNEWDAYSET, API_CMD_LEN) == 0) {
		   if (UTILS_legal_time( api_bufptr->API_Data_Buffer )) {
               if (pSCHEDULER_CONFIG_FLAGS.log_level > 0) {
			      myprintf(  "AUTH:SI058-Newday time change initiated by %s\n", client[connect_index].user_name );
			      myprintf(  "WARN:SW020-Newday time set request in progress, time request is %s\n", api_bufptr->API_Data_Buffer );
			   }
		       strncpy(pSCHEDULER_CONFIG_FLAGS.new_day_time, api_bufptr->API_Data_Buffer, 5 );
			   UTILS_space_fill_string(pSCHEDULER_CONFIG_FLAGS.new_day_time, 5 );
               /* must adjust new next run day field */
			   time_now = time(0);
			   UTILS_datestamp( (char *)&pSCHEDULER_CONFIG_FLAGS.next_new_day_run, time_now );
			   ptr = (char *)&pSCHEDULER_CONFIG_FLAGS.next_new_day_run;
			   ptr = ptr + 9; /* skip over yyyymmdd_ */
			   strncpy( ptr, pSCHEDULER_CONFIG_FLAGS.new_day_time, 5 );
			   test_time = UTILS_make_timestamp( (char *)&pSCHEDULER_CONFIG_FLAGS.next_new_day_run );
			   while (test_time < time_now) { test_time = test_time + (60 * 60 * 24); }
			   UTILS_datestamp( (char *)&pSCHEDULER_CONFIG_FLAGS.next_new_day_run, test_time );
			   /* save the new config */
			   CONFIG_update( &pSCHEDULER_CONFIG_FLAGS );
			   /* delete the existing newday job */
               job_header = (job_header_def *)&api_bufptr->API_Data_Buffer;
			   strcpy( job_header->jobname, "SCHEDULER-NEWDAY" );
			   UTILS_space_fill_string( (char *)&job_header->jobname, JOB_NAME_LEN );
			   if (SCHED_ACTIVE_delete_record( (active_queue_def *)&job_header->jobnumber, 1 /* 1 = internal */ ) < 0) {
                  API_init_buffer( (API_Header_Def *)api_bufptr ); 
                  strcpy( api_bufptr->API_System_Name, "CONFG" );
	              strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
			      strcpy( api_bufptr->API_Data_Buffer, "Configuration updated ok,\nBUT unable to requeue the existing SCHEDULER-NEWDAY\nIt may be running\n" );
			   }
			   else {
                  SCHED_Submit_Newday_Job();
                  API_init_buffer( (API_Header_Def *)api_bufptr ); 
                  strcpy( api_bufptr->API_System_Name, "CONFG" );
	              strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
			      strcpy( api_bufptr->API_Data_Buffer, "Request completed\nconfiguration updated and SCHEDULER-NEWDAY requeued\n" );
				  SCHED_init_required = 1;  /* Update scheduler nexttorun timers */
			   }
		   }
		   else {
               API_init_buffer( (API_Header_Def *)api_bufptr ); 
               strcpy( api_bufptr->API_System_Name, "CONFG" );
	           strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
			   snprintf( api_bufptr->API_Data_Buffer, (MAX_API_DATA_LEN - 1),
                        "*E* The time provided in the request is invalid (%s)\n",
                        api_bufptr->API_Data_Buffer);
		   }
   }

   /* change the server log level */
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_SETLOGLEVEL, API_CMD_LEN) == 0) {
           myprintf(  "AUTH:SW021-Log level change initiated by %s\n", client[connect_index].user_name );
		   memcpy( smallbuf, api_bufptr->API_Data_Buffer, 4 );
           API_init_buffer( (API_Header_Def *)api_bufptr ); 
           strcpy( api_bufptr->API_System_Name, "CONFG" );
           strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
		   if (memcmp(smallbuf,"ERR",3) == 0) {
                   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
				      myprintf(  "INFO:SI059-Server log level changed to ERR\n" );
				   }
				   pSCHEDULER_CONFIG_FLAGS.log_level = 0;
				   CONFIG_update( &pSCHEDULER_CONFIG_FLAGS );
		           strcpy( api_bufptr->API_Data_Buffer, "Server log level now set to ERRORs only\n" );
		   }
		   else if (memcmp(smallbuf,"WARN",4) == 0) {
                   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
				      myprintf(  "INFO:SI060-Server log level changed to WARN\n" );
				   }
				   pSCHEDULER_CONFIG_FLAGS.log_level = 1;
				   CONFIG_update( &pSCHEDULER_CONFIG_FLAGS );
		           strcpy( api_bufptr->API_Data_Buffer, "Server log level now set to ERRORs and WARNINGs only\n" );
		   }
		   else if (memcmp(smallbuf,"INFO",4) == 0) {
				   pSCHEDULER_CONFIG_FLAGS.log_level = 2;
				   CONFIG_update( &pSCHEDULER_CONFIG_FLAGS );
		           strcpy( api_bufptr->API_Data_Buffer, "Server log level now set to all INFO, ERRORs and WARNINGs\n" );
                   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
				      myprintf(  "INFO:SI061-Server log level changed to INFO\n" );
				   }
		   }
		   else {
	           strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
			   strcpy( api_bufptr->API_Data_Buffer, "*E* The log level provided in the request is invalid (%s)\n");
		   }
   }

   /* change the newday jobs still running failure action */
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_NEWDAYFAILACT, API_CMD_LEN) == 0) {
		   if (memcmp(api_bufptr->API_Data_Buffer, "ALERT",5) == 0) {
                   myprintf(  "AUTH:SW022-New day fail action changed to ALERT by %s\n", client[connect_index].user_name );
                   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
						   myprintf(  "INFO:SI062-SCHEDULER-NEWDAY PAUSE ACTION CHANGED TO RAISE ALERT\n" );
				   }
				   pSCHEDULER_CONFIG_FLAGS.newday_action = '0';
				   CONFIG_update( &pSCHEDULER_CONFIG_FLAGS );
                   API_init_buffer( (API_Header_Def *)api_bufptr ); 
                   strcpy( api_bufptr->API_System_Name, "CONFG" );
	               strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
				   strcpy( api_bufptr->API_Data_Buffer, "Scheduler updated, will raise an alert if jobs still exist at newday time\n" );
		   }
		   else if (memcmp(api_bufptr->API_Data_Buffer, "DEPWAIT",7) == 0) {
                   myprintf(  "AUTH:SW023-New day fail action changed to DEPWAIT by %s\n", client[connect_index].user_name );
                   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
						   myprintf(  "INFO:SI063-SCHEDULER-NEWDAY PAUSE ACTION CHANGED TO DEPENDECY WAIT\n" );
				   }
				   pSCHEDULER_CONFIG_FLAGS.newday_action = '1';
				   CONFIG_update( &pSCHEDULER_CONFIG_FLAGS );
                   API_init_buffer( (API_Header_Def *)api_bufptr ); 
                   strcpy( api_bufptr->API_System_Name, "CONFG" );
	               strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
				   strcpy( api_bufptr->API_Data_Buffer, "Scheduler updated, will set dependencies on a pending job if jobs still exist at newday time\n" );
		   }
		   else {
                   API_init_buffer( (API_Header_Def *)api_bufptr ); 
                   strcpy( api_bufptr->API_System_Name, "CONFG" );
	               strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
				   strcpy( api_bufptr->API_Data_Buffer, "*E* Command rejected, server will only accept ALERT or DEPWAIT for this request\n" );
		   }
   }

   /* Adjust the catchup flag settings */
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_CATCHUPFLAG, API_CMD_LEN) == 0) {
           strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
		   if ( (memcmp(api_bufptr->API_Data_Buffer,"ALLDAYS",7) == 0)  ||
		        (memcmp(api_bufptr->API_Data_Buffer,"NONE",4) == 0) 
			  )
		   {
		      if (memcmp(api_bufptr->API_Data_Buffer,"ALLDAYS",7) == 0) {
                   myprintf(  "AUTH:SW024-Scheduler catchup flag changed to ALLDAYS by %s\n", client[connect_index].user_name );
                   pSCHEDULER_CONFIG_FLAGS.catchup_flag = '2';
                   z = CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
		      }
		      else if (memcmp(api_bufptr->API_Data_Buffer,"NONE",4) == 0) {
                   myprintf(  "AUTH:SW025-Scheduler catchup flag changed to NONE by %s\n", client[connect_index].user_name );
                   pSCHEDULER_CONFIG_FLAGS.catchup_flag = '0';
                   z = CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
			  }
			  if (z == 0) {
                   strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
                   strcpy( api_bufptr->API_Data_Buffer, "SERVER IO ERROR ON CONFIGURATION FILE, NO CHANGE MADE, CHECK LOGS\n" );
			  }
			  else {
                 strcpy( api_bufptr->API_Data_Buffer, "CONFIGURATION FILE UPDATED\n" );
			  }
		   }
		   else {
				   strncpy( smallbuf, api_bufptr->API_Data_Buffer, 8 );
                   strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
                   snprintf( api_bufptr->API_Data_Buffer, (MAX_API_DATA_LEN - 1),
                             "REQUEST PARAMETER IN THE API CALL IS ILLEGAL (%s)\n", smallbuf );
		   }
   }

   /* controls whether deleted jobs are displayed in a sched listall output */
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_FULLSCHEDDISP, API_CMD_LEN) == 0) {
      if (memcmp(api_bufptr->API_Data_Buffer, "ON", 2) == 0) {
         myprintf(  "AUTH:SW026-Scheduler job full display changed to ON by %s\n", client[connect_index].user_name );
         pSCHEDULER_CONFIG_FLAGS.debug_level.show_deleted_schedjobs = 1;
         z = CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
         strcpy( api_bufptr->API_System_Name, "CONFG" );
         strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
         strcpy( api_bufptr->API_Data_Buffer, "COMPLETED/DELETED JOBS WILL NOW BE DISPLAYED\n" );
      }
	  else if (memcmp(api_bufptr->API_Data_Buffer, "OFF", 3) == 0) {
         myprintf(  "AUTH:SW027-Scheduler job full display changed to OFF by %s\n", client[connect_index].user_name );
         pSCHEDULER_CONFIG_FLAGS.debug_level.show_deleted_schedjobs = 0;
         z = CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
         strcpy( api_bufptr->API_System_Name, "CONFG" );
         strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
         strcpy( api_bufptr->API_Data_Buffer, "COMPLETED/DELETED JOBS NO LONGER BE DISPLAYED\n" );
      }
      else {
          API_init_buffer( (API_Header_Def *)api_bufptr ); 
          strcpy( api_bufptr->API_System_Name, "CONFG" );
          strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
          strcpy( api_bufptr->API_Data_Buffer, "*E* Invalid parameter, must be ON or OFF\n" );
      }
   }

   /* remote alert forwarding values --- need to add some sanity checks here */
   else if (memcmp(api_bufptr->API_Command_Number, API_CMD_ALERTFORWARDING, API_CMD_LEN) == 0) {
      if (memcmp(api_bufptr->API_Data_Buffer, "FORWARDING", 10) == 0) { /* may be Y, N or E */
         ptr = (char *)&api_bufptr->API_Data_Buffer;
         ptr = ptr + 10;
         while (*ptr == ' ') { ptr++; }
         if (*ptr == 'Y') {       /* yes */
/*
            API_init_buffer( (API_Header_Def *)api_bufptr ); 
            strcpy( api_bufptr->API_System_Name, "CONFG" );
            strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
            sprintf( api_bufptr->API_Data_Buffer, "ALERT FORWARDING NOW TO IP ADDRESS %s, PORT %s\n",
                     pSCHEDULER_CONFIG_FLAGS.remote_console_ipaddr,
                     pSCHEDULER_CONFIG_FLAGS.remote_console_port );
            pSCHEDULER_CONFIG_FLAGS.use_central_alert_console = 'Y';
            CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
*/
            API_init_buffer( (API_Header_Def *)api_bufptr ); 
            strcpy( api_bufptr->API_System_Name, "CONFG" );
            strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
            sprintf( api_bufptr->API_Data_Buffer, "DIRECT TCPIP ALERT FORWARDING IS NOT AVAILABLE IN THIS RELEASE." );
         }
		 else if (*ptr == 'N') {  /* no */
            API_init_buffer( (API_Header_Def *)api_bufptr ); 
            strcpy( api_bufptr->API_System_Name, "CONFG" );
            strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
            strcpy( api_bufptr->API_Data_Buffer, "NO ALERTS WILL BE FORWARDED TO REMOTE MONITORING APPLICATIONS" );
            pSCHEDULER_CONFIG_FLAGS.use_central_alert_console = 'N';
            CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         }
		 else if (*ptr == 'E') {  /* use external pgm */
            API_init_buffer( (API_Header_Def *)api_bufptr ); 
            strcpy( api_bufptr->API_System_Name, "CONFG" );
            strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
            sprintf( api_bufptr->API_Data_Buffer, "USING EXTERNAL PROGRAME FOR REMOTE ALERTING...\n %s\n %s\n",
                     pSCHEDULER_CONFIG_FLAGS.local_execprog_raise,
                     pSCHEDULER_CONFIG_FLAGS.local_execprog_cancel );
            pSCHEDULER_CONFIG_FLAGS.use_central_alert_console = 'E';
            CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         }
		 else {                   /* not legal */
            savetype = *ptr;
            API_init_buffer( (API_Header_Def *)api_bufptr ); 
            strcpy( api_bufptr->API_System_Name, "CONFG" );
            strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
            sprintf( api_bufptr->API_Data_Buffer, "*E* Invalid alert forwarding type (%c), command ignored\n", savetype );
         }
      } /* forwarding */
	  else if (memcmp(api_bufptr->API_Data_Buffer, "IPADDR", 6) == 0) { /* forwarding ip address */
         ptr = (char *)&api_bufptr->API_Data_Buffer;
         ptr = ptr + 6;
         while (*ptr == ' ') { ptr++; }
		 ptr2 = ptr;
         while ((*ptr2 != '\0') && (*ptr2 != '\n')) { ptr2++; }
         if (*ptr2 == '\n') { *ptr2 = '\0'; }
         strncpy( pSCHEDULER_CONFIG_FLAGS.remote_console_ipaddr, ptr, 37 );
         pSCHEDULER_CONFIG_FLAGS.remote_console_ipaddr[36] = '\0';
		 CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
         strcpy( api_bufptr->API_System_Name, "CONFG" );
         strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
         sprintf( api_bufptr->API_Data_Buffer, "ALERT FORWARDING NOW TO IP ADDRESS %s, PORT %s\n",
                  pSCHEDULER_CONFIG_FLAGS.remote_console_ipaddr,
                  pSCHEDULER_CONFIG_FLAGS.remote_console_port );
      } /* ipaddr */
	  else if (memcmp(api_bufptr->API_Data_Buffer, "PORT", 4) == 0) { /* forwarding ip port number */
         ptr = (char *)&api_bufptr->API_Data_Buffer;
         ptr = ptr + 4;
         while (*ptr == ' ') { ptr++; }
		 ptr2 = ptr;
         while ((*ptr2 != '\0') && (*ptr2 != '\n')) { ptr2++; }
         if (*ptr2 == '\n') { *ptr2 = '\0'; }
         strncpy( pSCHEDULER_CONFIG_FLAGS.remote_console_port, ptr, 5 );
         pSCHEDULER_CONFIG_FLAGS.remote_console_port[4] = '\0';
		 CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
         strcpy( api_bufptr->API_System_Name, "CONFG" );
         strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
         sprintf( api_bufptr->API_Data_Buffer, "ALERT FORWARDING NOW TO IP ADDRESS %s, PORT %s\n",
                  pSCHEDULER_CONFIG_FLAGS.remote_console_ipaddr,
                  pSCHEDULER_CONFIG_FLAGS.remote_console_port );
      } /* port */
	  else if (memcmp(api_bufptr->API_Data_Buffer, "PGMRAISE", 8) == 0) { /* external command to raise an alert  */
         ptr = (char *)&api_bufptr->API_Data_Buffer;
         ptr = ptr + 8;
         while (*ptr == ' ') { ptr++; }
		 ptr2 = ptr;
         while ((*ptr2 != '\0') && (*ptr2 != '\n')) { ptr2++; }
         if (*ptr2 == '\n') { *ptr2 = '\0'; }
         strncpy( pSCHEDULER_CONFIG_FLAGS.local_execprog_raise, ptr, 90 );
         pSCHEDULER_CONFIG_FLAGS.local_execprog_raise[89] = '\0';
		 CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
         strcpy( api_bufptr->API_System_Name, "CONFG" );
         strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
         sprintf( api_bufptr->API_Data_Buffer, "EXTERNAL PROGRAM TO RAISE ALERTS NOW...\n %s\n",
                  pSCHEDULER_CONFIG_FLAGS.local_execprog_raise );
      } /* pgmraise */
	  else if (memcmp(api_bufptr->API_Data_Buffer, "PGMCANCEL", 9) == 0) { /* external command to cancel an alert  */
         ptr = (char *)&api_bufptr->API_Data_Buffer;
         ptr = ptr + 9;
         while (*ptr == ' ') { ptr++; }
		 ptr2 = ptr;
         while ((*ptr2 != '\0') && (*ptr2 != '\n')) { ptr2++; }
         if (*ptr2 == '\n') { *ptr2 = '\0'; }
         strncpy( pSCHEDULER_CONFIG_FLAGS.local_execprog_cancel, ptr, 90 );
         pSCHEDULER_CONFIG_FLAGS.local_execprog_cancel[89] = '\0';
		 CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
         strcpy( api_bufptr->API_System_Name, "CONFG" );
         strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
         sprintf( api_bufptr->API_Data_Buffer, "EXTERNAL PROGRAM TO CANCEL ALERTS NOW...\n %s\n",
                  pSCHEDULER_CONFIG_FLAGS.local_execprog_cancel );
      } /* pgmcancel */
	  else { /* we don't know */
          API_init_buffer( (API_Header_Def *)api_bufptr ); 
          strcpy( api_bufptr->API_System_Name, "CONFG" );
          strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
          strcpy( api_bufptr->API_Data_Buffer, "*E* Invalid alert forwarding keyword, command ignored\n" );
      }
   }
   /* else we don't know about this command */
   else {	  
      myprintf(  "*ERR:SE064-API request for %s, request code %s not yet implemented !\n",
         	   api_bufptr->API_System_Name, api_bufptr->API_Command_Number );
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "CONFG" );
	  strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
      strcpy( api_bufptr->API_Data_Buffer, "API request not yet implemented !\n" );
   }

   /* write the reponse */
   API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
   if (API_flush_buffer( api_bufptr, tx ) != 0) {
      die("process_client_request_config", DIE_INTERNAL);
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving process_client_request_config\n" );
   }
} /* process_client_request_config */

/* =========================================================
 * process_client_request_job:
 * 
 * called from process_client_request when the request is for the job
 * subsystem. Determine if it is a command we can process, if so call the
 * appropriate support library routines.
   ========================================================= */
void process_client_request_job( FILE *tx, char *buffer, int connect_index ) {
   int datalen, recordnum, recnum;
   char local_command[API_CMD_LEN+1];
   API_Header_Def * api_bufptr;
   job_header_def * job_header;
   active_queue_def * active_queue_rec;
   jobsfile_def * pJobRecord;
   jobsfile_def * pJobRecord2;
   time_t time_number;

   api_bufptr = (API_Header_Def *)buffer;
   job_header = (job_header_def *)&api_bufptr->API_Data_Buffer;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG:in server.c, process_client_request_job\n" );
   }

   /* TODO, check access level to see if user can play with jobs */

   strncpy( local_command, api_bufptr->API_Command_Number, API_CMD_LEN );
   local_command[API_CMD_LEN] = '\0';

   /* ----------------------------
    * Sanity check....
    * Jobs beginning with SCHEDULER are restricted to internal scheduler
    * functions and must not be allowed to be modified by any client interface. 
    * -------------------------- */
   if ( (memcmp(api_bufptr->API_Command_Number, API_CMD_LISTALL, API_CMD_LEN) != 0) &&
        (memcmp(job_header->jobname, "SCHEDULER", 9) == 0) 
      ) {
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
         strcpy( api_bufptr->API_System_Name, "JOB" );
	     strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	     strcpy( api_bufptr->API_Data_Buffer, "*E* JOBS NAMED SCHEDULER* ARE RESERVED FOR SCHEDULER USE\n" );
         API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
   }

   /*
    * Are we adding a new job ?.
    */ 
   else if ( memcmp(api_bufptr->API_Command_Number, API_CMD_JOB_ADD, API_CMD_LEN) == 0 ) {
      /* A job add command has been received */
      /*   one special check, as the request has been recieved via API we
	   *   cannot guarantee that the client set the date correctly on jobs
	   *   supposed to run only on specific days, or on calendar days; so we
	   *   need to adjust the time just to be safe. */
      pJobRecord = (jobsfile_def *)&api_bufptr->API_Data_Buffer;
      pJobRecord->calendar_error = 'N';  /* No error yet */
      if ( pJobRecord->use_calendar == '2' ) { /* adjust the supplied date if not legal */
         time_number = UTILS_make_timestamp( pJobRecord->next_scheduled_runtime );
         JOBS_find_next_run_day( pJobRecord, time_number );
      }
	  else if ( pJobRecord->use_calendar == '1' ) {   /* using a calendar */
         UTILS_space_fill_string( pJobRecord->calendar_name, CALENDAR_NAME_LEN ); /* Sanity check */
         if (CALENDAR_next_runtime_timestamp( pJobRecord->calendar_name, pJobRecord->next_scheduled_runtime, 0 ) == 0) {
	        strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	        UTILS_get_message( api_bufptr->API_Data_Buffer, MAX_API_DATA_LEN-1, 1 );
            API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
            myprintf(  "*ERR:SE065-Unable to workout start date from the calendar.\n" );
            if (API_flush_buffer( api_bufptr, tx ) != 0) {
               die("process_client_request_job", DIE_INTERNAL);
            }
            return;  /* quit now */
         }
	  }
	  /* Check it again, jobs must be correct */
	  if (BULLETPROOF_jobsfile_record( (jobsfile_def *)&api_bufptr->API_Data_Buffer ) != 1) { /* not OK */
         strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
         strcpy( api_bufptr->API_Data_Buffer, "*E* JOB FAILED SANITY CHECKS, REVIEW LOGS FOR ERRORS\n" );
         API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
	  }
	  else {
	     /* Then write the record */
	     recordnum = JOBS_write_record( (jobsfile_def *)&api_bufptr->API_Data_Buffer, -1 );
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
         strcpy( api_bufptr->API_System_Name, "JOB" );
	     if (recordnum < 0) {
	        strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	        strcpy( api_bufptr->API_Data_Buffer, "*E* JOB ALREADY EXISTS IN JOBS DATABASE OR DBS FILE ERROR\n" );
            API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
	     }
	     else {
            myprintf( "AUTH:SI066-Job %s added by user %s\n", pJobRecord->job_header.jobname, client[connect_index].user_name );
	        strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
	        strcpy( api_bufptr->API_Data_Buffer, "JOB HAS BEEN ADDED\n" );
            API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
   	     }
	  } /* else was OK from bulletproof */
   }
   /*
    * Are we deleting an existing job ?.
    */ 
   else if ( memcmp(api_bufptr->API_Command_Number, API_CMD_JOB_DELETE, API_CMD_LEN) == 0 ) {
		   /* A job delete command has been received */
		   /* -- sanity check, don't delete if it is on the active queue, the
			* active queue will need to read it at execution time -- */
      if ( (active_queue_rec = (active_queue_def *)MEMORY_malloc(sizeof(active_queue_def),"process_client_request_job")) == NULL) {
         UTILS_set_message( "Insufficient memory to check active queue entries !" );
		 myprintf(  "*ERR:SE066-process_client_request_job, unable to allocate memory for job activity check\n" );
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
         strcpy( api_bufptr->API_System_Name, "JOB" );
	     strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	     strcpy( api_bufptr->API_Data_Buffer, "*E* INSUFFICIENT MEMORY ON SERVER TO PROCESS REQUEST\n" );
         API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
      }
	  else { /* memory allocated OK */
         memcpy( active_queue_rec->job_header.jobnumber, api_bufptr->API_Data_Buffer, sizeof( job_header_def ) );
	     recnum = SCHED_ACTIVE_read_record( active_queue_rec );
	     MEMORY_free( active_queue_rec );
	     if (recnum == -1) {
            if (JOBS_delete_record( (jobsfile_def *)&api_bufptr->API_Data_Buffer ) < 0) {
               API_init_buffer( (API_Header_Def *)api_bufptr ); 
               strcpy( api_bufptr->API_System_Name, "JOB" );
	           strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	           strcpy( api_bufptr->API_Data_Buffer, "*E* JOB WAS NOT FOUND IN THE DATABASE\n" );
               API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
	        }
	        else {
               pJobRecord2 = (jobsfile_def *)&api_bufptr->API_Data_Buffer;
               myprintf( "AUTH:SI067-Job %s deleted by user %s\n",
                          pJobRecord2->job_header.jobname, client[connect_index].user_name );
               API_init_buffer( (API_Header_Def *)api_bufptr ); 
               strcpy( api_bufptr->API_System_Name, "JOB" );
	           strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
	           strcpy( api_bufptr->API_Data_Buffer, "JOB DELETED\n" );
               API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
	        }
         } /* if not on active queue */
		 else {  /* else record is on active queue */
            API_init_buffer( (API_Header_Def *)api_bufptr ); 
            strcpy( api_bufptr->API_System_Name, "JOB" );
	        strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	        strcpy( api_bufptr->API_Data_Buffer, "*E* JOB IS ON ACTIVE TASK QUEUE, DELETE FROM ACTIVE QUEUE FIRST\n" );
            API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
		 }
	  }    /* if memory allocated OK */
   }

   /*
    * Are we updating an existing job ?.
    */
   else if ( memcmp(api_bufptr->API_Command_Number, API_CMD_UPDATE, API_CMD_LEN) == 0 ) {
		   /* A job update command has been recieved */
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 0) {
         myprintf(  "WARN:SW028-JOB update not yet coded ! TODO\n" );
	  }
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "JOB" );
	  strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	  strcpy( api_bufptr->API_Data_Buffer, "*E* JOB UPDATE NOT YET CODED\n" );
      API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
   }
   /*
    * Are we displaying job details for an existing job in the database ?.
    */
   else if ( memcmp(api_bufptr->API_Command_Number, API_CMD_RETRIEVE, API_CMD_LEN) == 0 ) {
		   /* a Job info command has been received */
      JOBS_format_for_display_API( api_bufptr );
   }
   /*
    * Are we displaying details for a job on the active scheduler queue ?.
    */
   else if ( memcmp(api_bufptr->API_Command_Number, API_CMD_STATUS, API_CMD_LEN) == 0 ) {
		   /* A job status has been recieved */
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 0) {
         myprintf(  "WARN:SW029-JOB status not yet coded ! TODO\n" );
	  }
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "JOB" );
	  strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	  strcpy( api_bufptr->API_Data_Buffer, "*E* JOB STATUS NOT YET CODED\n" );
      API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
   }
   else if ( memcmp(api_bufptr->API_Command_Number, API_CMD_LISTALL, API_CMD_LEN) == 0 ) {
		   /* A Job listall has been recieved */
	  JOBS_format_listall_for_display_API( api_bufptr, tx );
   }
   /*
    * Are we scheduling a job on ? 
   */
   else if ( memcmp(api_bufptr->API_Command_Number, API_CMD_SCHEDULE_ON, API_CMD_LEN) == 0 ) {
		   /* A Job schedule on request has been received */
	  if (JOBS_schedule_on( (jobsfile_def *)api_bufptr->API_Data_Buffer ) == 0) {
          API_init_buffer( (API_Header_Def *)api_bufptr ); 
          strcpy( api_bufptr->API_System_Name, "JOB" );
	      strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
	      strcpy( api_bufptr->API_Data_Buffer, "JOB HAS BEEN ADDED TO THE ACTIVE SCHEDULE\n" );
          API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
	  }
	  else {
          API_init_buffer( (API_Header_Def *)api_bufptr ); 
          strcpy( api_bufptr->API_System_Name, "JOB" );
	      strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	      strcpy( api_bufptr->API_Data_Buffer, "*E* UNABLE TO SCHEDULE JOB ON, CHECK SYSTEM LOG\n" );
          API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
	   }
   }
   /*
    * Or if we get here, a command we know nothing about for the job subsystem.
    */
   else {
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
         myprintf(  "INFO:SW030-API request for %s, command %s not recognised !\n",
		      	  api_bufptr->API_System_Name, api_bufptr->API_Command_Number );
	  }
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "JOB" );
	  strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	  datalen = snprintf( api_bufptr->API_Data_Buffer, (MAX_API_DATA_LEN - 1),
                         "*E* Command request %s is not recognised\n", local_command );
      API_set_datalen( (API_Header_Def *)api_bufptr, datalen );
   }

   /* write the repose */
   if (API_flush_buffer( api_bufptr, tx ) != 0) {
      die("process_client_request_job", DIE_INTERNAL);
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving process_client_request_job\n" );
   }
} /* process_client_request_job */

/* =========================================================
 * process_client_request_dep:
 * 
 * called from process_client_request when the request is for the dependency 
 * subsystem. Determine if it is a command we can process, if so call the
 * appropriate support library routines.
   ========================================================= */
void process_client_request_dep( FILE *tx, char *buffer ) {
   int datalen;
   char local_command[API_CMD_LEN+1];
   char dependency_key[JOB_DEPENDTARGET_LEN+1];
   API_Header_Def * api_bufptr;
   job_header_def * local_header_rec;

   api_bufptr = (API_Header_Def *)buffer;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG:in server.c, process_client_request_dep\n" );
   }

   strncpy( local_command, api_bufptr->API_Command_Number, API_CMD_LEN );
   local_command[API_CMD_LEN] = '\0';

   /*
    * Is it a listall jobs waiting on a specific dependency ?.
    */ 
   if ( memcmp(api_bufptr->API_Command_Number, API_CMD_LISTALL, API_CMD_LEN) == 0 ) {
	  SCHED_DEPEND_listall( api_bufptr, tx );
   }
   else if ( memcmp(api_bufptr->API_Command_Number, API_CMD_LISTWAIT_BYDEPNAME, API_CMD_LEN) == 0 ) {
      strncpy( dependency_key, api_bufptr->API_Data_Buffer, JOB_DEPENDTARGET_LEN );
	  SCHED_DEPEND_listall_waiting_on_dep( api_bufptr, tx, (char *)&dependency_key );
   }
   else if ( memcmp(api_bufptr->API_Command_Number, API_CMD_DELETEALL_DEP, API_CMD_LEN) == 0 ) {
      if ( (local_header_rec = (job_header_def *)MEMORY_malloc(sizeof(job_header_def),"process_client_request_dep")) == NULL ) {
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
         strcpy( api_bufptr->API_System_Name, "DEP" );
	     strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	     datalen = snprintf( api_bufptr->API_Data_Buffer, (MAX_API_DATA_LEN - 1),
                           "UNABLE TO ALLOCATE %d BYTES OF MEMORY NEEDED FOR THIS OPERATION\n",
						    sizeof(job_header_def)
						  );
	  }
	  else {
         strncpy( local_header_rec->jobname, api_bufptr->API_Data_Buffer, JOB_NAME_LEN );
		 UTILS_space_fill_string( (char *)&api_bufptr->API_Data_Buffer, JOB_DEPENDTARGET_LEN );
         SCHED_DEPEND_delete_all_relying_on_dependency( (char *)&api_bufptr->API_Data_Buffer ); 
		 MEMORY_free( local_header_rec );
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
         strcpy( api_bufptr->API_System_Name, "DEP" );
	     strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
	     strcpy( api_bufptr->API_Data_Buffer, "Removal of all matching dependencies is being executed by the server\n" );
      }
   }
   else if ( memcmp(api_bufptr->API_Command_Number, API_CMD_DELETEALL_JOB, API_CMD_LEN) == 0 ) {
      if ( (local_header_rec = (job_header_def *)MEMORY_malloc(sizeof(job_header_def),"process_client_request_dep")) == NULL ) {
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
         strcpy( api_bufptr->API_System_Name, "DEP" );
	     strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	     datalen = snprintf( api_bufptr->API_Data_Buffer, (MAX_API_DATA_LEN - 1),
                           "UNABLE TO ALLOCATE %d BYTES OF MEMORY NEEDED FOR THIS OPERATION\n",
						    sizeof(job_header_def)
						  );
	  }
	  else {
         strncpy( local_header_rec->jobname, api_bufptr->API_Data_Buffer, JOB_NAME_LEN );
		 UTILS_space_fill_string( local_header_rec->jobname, JOB_NAME_LEN );
		 UTILS_uppercase_string( local_header_rec->jobname ); /* now safe to uppercase */
         SCHED_DEPEND_freeall_for_job( local_header_rec );
		 MEMORY_free( local_header_rec );
         API_init_buffer( (API_Header_Def *)api_bufptr ); 
         strcpy( api_bufptr->API_System_Name, "DEP" );
	     strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
	     strcpy( api_bufptr->API_Data_Buffer, "Server is processing the request.\n" );
	  }
   }

   /*
    * Or if we get here, a command we know nothing about for the dep subsystem.
    */
   else {
      myprintf(  "WARN:SW031-API request for %s, command %s not recognised !\n",
			  api_bufptr->API_System_Name, api_bufptr->API_Command_Number );
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "DEP" );
	  strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	  datalen = snprintf( api_bufptr->API_Data_Buffer, (MAX_API_DATA_LEN - 1),
                         "Command request %s is not recognised\n", local_command );
   }

   API_set_datalen( (API_Header_Def *)api_bufptr, strlen( api_bufptr->API_Data_Buffer ) );

   /* write any remaining response data */
   if (API_flush_buffer( api_bufptr, tx ) != 0) {
      die("process_client_request_dep", DIE_INTERNAL);
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving process_client_request_dep\n" );
   }
} /* process_client_request_dep */

/* =========================================================
 * process_client_request_debug:
 * 
 * called from process_client_request when the request is for the debug 
 * subsystem.
   ========================================================= */
void process_client_request_debug( FILE *tx, char *buffer ) {
   API_Header_Def * api_bufptr;
   char *ptr1, *ptr2;
   int debuglevel, junk;

   api_bufptr = (API_Header_Def *)buffer;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG:in server.c, process_client_request_debug\n" );
   }

   strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );

   if ( memcmp(api_bufptr->API_Command_Number, API_CMD_DEBUGSET, API_CMD_LEN) != 0 ) {
	  /* NOT a true debug command */
	  strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	  strcpy( api_bufptr->API_Data_Buffer, "IMPROPERLY FORMATTED API BUFFER FOR DEBUG REQUEST\n" );
   }
   else {
	  ptr1 = (char *)api_bufptr->API_Data_Buffer;
	  while (*ptr1 != ' ') ptr1++;  /* find the space separator between moduleid and level */
	  while (*ptr1 == ' ') ptr1++;  /* skip spaces to get to the subsys */
	  ptr2 = ptr1;
	  /* Now at 'subsys level' */
	  while ((*ptr2 != ' ') && (ptr2 != '\0')) ptr2++;  /* skip spaces to get to the level */
	  while ((*ptr2 == ' ') && (ptr2 != '\0')) ptr2++;  /* skip spaces to get to the level */
	  if (ptr2 == ptr1) {
	     strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	     strcpy( api_bufptr->API_Data_Buffer, "NO DEBUGLEVEL PROVIDED IN DEBUG REQUEST\n" );
	  }
	  else {
	     debuglevel = atoi(ptr2);
		 if ( memcmp(ptr1, "SERVER", 6) == 0 ) {
            pSCHEDULER_CONFIG_FLAGS.debug_level.server = debuglevel;
	        strcpy( api_bufptr->API_Data_Buffer, "SERVER DEBUG LEVEL UPDATED\n" );
			junk = CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         }
		 else if ( memcmp(ptr1, "UTILS", 5) == 0 ) {
            pSCHEDULER_CONFIG_FLAGS.debug_level.utils = debuglevel;
	        strcpy( api_bufptr->API_Data_Buffer, "UTILS DEBUG LEVEL UPDATED\n" );
			junk = CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         }
		 else if ( memcmp(ptr1, "JOBSLIB", 7) == 0 ) {
            pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib = debuglevel;
	        strcpy( api_bufptr->API_Data_Buffer, "JOBSLIB DEBUG LEVEL UPDATED\n" );
			junk = CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         }
		 else if ( memcmp(ptr1, "APILIB", 6) == 0 ) {
            pSCHEDULER_CONFIG_FLAGS.debug_level.api = debuglevel;
	        strcpy( api_bufptr->API_Data_Buffer, "APILIB DEBUG LEVEL UPDATED\n" );
			junk = CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         }
		 else if ( memcmp(ptr1, "ALERTLIB", 8) == 0 ) {
            pSCHEDULER_CONFIG_FLAGS.debug_level.alerts = debuglevel;
	        strcpy( api_bufptr->API_Data_Buffer, "ALERTLIB DEBUG LEVEL UPDATED\n" );
			junk = CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         }
		 else if ( memcmp(ptr1, "CALENDAR", 8) == 0 ) {
            pSCHEDULER_CONFIG_FLAGS.debug_level.calendar = debuglevel;
	        strcpy( api_bufptr->API_Data_Buffer, "CALENDAR DEBUG LEVEL UPDATED\n" );
			junk = CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         }
		 else if ( memcmp(ptr1, "SCHEDLIB", 8) == 0 ) {
            pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib = debuglevel;
	        strcpy( api_bufptr->API_Data_Buffer, "SCHEDLIB DEBUG LEVEL UPDATED\n" );
			junk = CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         }
		 else if ( memcmp(ptr1, "BULLETPROOF", 11) == 0 ) {
            pSCHEDULER_CONFIG_FLAGS.debug_level.bulletproof = debuglevel;
	        strcpy( api_bufptr->API_Data_Buffer, "BULLETPROOF DEBUG LEVEL UPDATED\n" );
			junk = CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         }
		 else if ( memcmp(ptr1, "USER", 4) == 0 ) {
            pSCHEDULER_CONFIG_FLAGS.debug_level.user = debuglevel;
	        strcpy( api_bufptr->API_Data_Buffer, "USER DEBUG LEVEL UPDATED\n" );
			junk = CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         }
		 else if ( memcmp(ptr1, "MEMORY", 6) == 0 ) {
            pSCHEDULER_CONFIG_FLAGS.debug_level.memory = debuglevel;
	        strcpy( api_bufptr->API_Data_Buffer, "MEMORY DEBUG LEVEL UPDATED\n" );
			junk = CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         }
		 else if ( memcmp(ptr1, "ALL", 3) == 0 ) {
            pSCHEDULER_CONFIG_FLAGS.debug_level.server = debuglevel;
            pSCHEDULER_CONFIG_FLAGS.debug_level.utils = debuglevel;
            pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib = debuglevel;
            pSCHEDULER_CONFIG_FLAGS.debug_level.api = debuglevel;
            pSCHEDULER_CONFIG_FLAGS.debug_level.alerts = debuglevel;
            pSCHEDULER_CONFIG_FLAGS.debug_level.calendar = debuglevel;
            pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib = debuglevel;
            pSCHEDULER_CONFIG_FLAGS.debug_level.bulletproof = debuglevel;
            pSCHEDULER_CONFIG_FLAGS.debug_level.user = debuglevel;
            pSCHEDULER_CONFIG_FLAGS.debug_level.memory = debuglevel;
	        strcpy( api_bufptr->API_Data_Buffer, "ALL MODULE DEBUG LEVELS HAVE BEEN UPDATED\n" );
			junk = CONFIG_update( (internal_flags *)&pSCHEDULER_CONFIG_FLAGS );
         }
		 else {
	        strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
	        strcpy( api_bufptr->API_Data_Buffer, "UNRECOGNIZED SUBSYSTEM IN DEBUG REQUEST\n" );
		 }
	  }
   }

   /* write the reponse */
   API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
   if (API_flush_buffer( api_bufptr, tx ) != 0) {
      die("process_client_request_debug", DIE_INTERNAL);
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving process_client_request_debug\n" );
   }
} /* process_client_request_debug */

/* =========================================================
 * process_client_request:
 * 
 * called from process_client we have recieved data from a socket connection.
 * If the data is in a valid API format we check to see if it is a subsystem we
 * support, and process the request if so.
 * If the data is not in API format we only allow a shutdown command to be
 * accepted (debugging backdoor here).
   ========================================================= */
static void process_client_request( FILE *tx, char * buffer, int *shutdown_flag, int connect_index ) {
   int z;
   API_Header_Def * api_bufptr;
   char *ptr;
   
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG:in server.c, entered process_client_request\n" );
   }

   /* notes: added connect index so can check access level,
	* this is still to be propogated thru the code. */

   clearerr( tx );
   api_bufptr = (API_Header_Def *)buffer;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARDUMP) {
      API_DEBUG_dump_api( api_bufptr );
   }

   if (memcmp(api_bufptr->API_EyeCatcher, API_EYECATCHER, 4) == 0) {
      API_Nullterm_Fields( api_bufptr );

	  /* Access level checks needed */
      ptr = (char *)&api_bufptr->API_Command_Number;
	  if ( 
           (
             (*ptr == 'O') &&    /* At least operator access needed for command */
             ( (client[connect_index].access_level != 'O') && (client[connect_index].access_level != 'A') )
           ) ||
           (
             (*ptr == 'A') &&   /* At least admin access needed for command */
             (client[connect_index].access_level != 'A')
           ) ||
           (
             (*ptr == 'S') &&   /* At least security access needed for command */
             ( (client[connect_index].access_level != 'S') && (client[connect_index].access_level != 'A') )
           ) ||
           (
             (*ptr == 'J') &&   /* At least job auth access needed for command */
             ( (client[connect_index].access_level != 'J') && (client[connect_index].access_level != 'A') )
           )
         )
      {
         myprintf(  "AUTH:SW032-Restricted command rejected, source = %s\n", client[connect_index].remoteaddr );
         strcpy( api_bufptr->API_Data_Buffer, "*E* YOU ARE NOT AUTHORISED FOR THE COMMAND\nTry logging up.\n" );
         strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
         API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
	     /* write the reponse */
	     if (API_flush_buffer( api_bufptr, tx ) != 0) {
	        die("process_client_request", DIE_INTERNAL);
	     }
		 return;
      } /* End of access level checks */

	  /* Check the subsystem we want now we know we can run the commands */
	  if (memcmp(api_bufptr->API_System_Name, "SCHED", 5) == 0) {   /* command to scheduler subsystem */
          process_client_request_sched( tx, buffer, shutdown_flag, connect_index );
      }
	  else if (memcmp(api_bufptr->API_System_Name, "CONFG", 5) == 0) {   /* command for config change */
          process_client_request_config( tx, buffer, connect_index );
	  }
	  else if (memcmp(api_bufptr->API_System_Name, "JOB", 3) == 0) { /* command to job subsystem */
		  process_client_request_job( tx, buffer, connect_index );
	  }
	  else if (memcmp(api_bufptr->API_System_Name, "DEP", 3) == 0) { /* command to dependency subsystem */
		  process_client_request_dep( tx, buffer );
	  }
	  else if (memcmp(api_bufptr->API_System_Name, "DEBUG", 5) == 0) { /* command to debug subsystem */
		  process_client_request_debug( tx, buffer );
	  }
	  else if (memcmp(api_bufptr->API_System_Name, "ALERT", 5) == 0) { /* command to alert subsystem */
		  process_client_request_alert( tx, buffer );
	  }
	  else if (memcmp(api_bufptr->API_System_Name, "USER", 4) == 0) {  /* command to user subsystem */
		  USER_process_client_request( tx, buffer, (char *)&client[connect_index].user_name );
	  }
	  else if (memcmp(api_bufptr->API_System_Name, "CAL", 3) == 0) {  /* command to calendar subsystem */
		  process_client_request_calendar( tx, buffer );
	  }
      else {
         myprintf(  "DEBUG: API request recieved, NOT YET IMPLEMENTED, Subsys=%s\n", api_bufptr->API_System_Name );
         strcpy( api_bufptr->API_Data_Buffer, "*E* SUBSYSTEM IS NOT SUPPORTED YET\n" );
         strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
         API_set_datalen( (API_Header_Def *)api_bufptr, 35 );
         if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARDUMP) {
            myprintf(  "DEBUG: writing response\n" );
	        API_DEBUG_dump_api( (API_Header_Def *)api_bufptr );
         }
	     /* write the repose */
	     if (API_flush_buffer( api_bufptr, tx ) != 0) {
	        die("process_client_request", DIE_INTERNAL);
	     }
      }
   }
   /* If the message did not contain an eyecatcher then it is a non-api
    * (possibly telnet) request for service, we only allow a shutdown command
    * through this back door.
    */
   else {
#ifdef USE_BACKDOOR
      if (memcmp(buffer,"SHUTDOWN NOW",12) == 0) {
         z = fprintf( tx, "SHUTDOWN REQUEST ACCEPTED, QUEUED FOR EXECUTION\n" );
         if (ferror(tx)) { ; }
         *shutdown_flag = 1;
         pSCHEDULER_CONFIG_FLAGS.enabled = '2'; /* shutdown state */
         myprintf(  "INFO:SI068- SHUTDOWN REQUESTED VIA TELNET(?), ACCEPTED\n" );
         myprintf(  "INFO:SI068- -*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-\n" );
         myprintf(  "INFO:SI068- BACK-DOOR SHUTDOWN REQUEST HAS BEEN QUEUED !!!\n" );
	     myprintf(  "INFO:SI068-    WAITING ON %d JOBS TO COMPLETE\n", (int)INTERNAL_count_executing() );
         myprintf(  "INFO:SI068- -*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-\n" );
      }
      else {
#endif
         if (strlen(buffer) > 0) {
            /* catchall, echo it back */
            z = fprintf( tx, "*E* %s IS NOT IN THE EXPECTED API FORMAT\n",buffer );
            if (ferror(tx))
               die("process_client_request", DIE_INTERNAL);
		 }
		 else {
            if (pSCHEDULER_CONFIG_FLAGS.log_level > 0) {
               myprintf(  "WARN:SW033-null buffer read from socket\n" );
			}
         }
#ifdef USE_BACKDOOR
      }
#endif
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving process_client_request\n" );
   }
}  /* process_client_request */

/* =========================================================
 * process_client:
 * 
 * Called when data has been received by the mainline. We
 * identify who sent the data and select the correct stack.
 * If the caller sends data call the procedure to process it.
 * Then check to see if the caller sent an EOF. If they did
 * they are disconnecting so close the socket connection we
 * have to the client and free up the stack entry. 
   ========================================================= */
static int process_client( int sock_id, int *shutdown_flag ) {
   char buf[8192];   /* io buffer */
   FILE *rx = client[sock_id].rx;
   FILE *tx = client[sock_id].tx;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: Entered process_client, Data received from %s\n", client[sock_id].remoteaddr );
   }

   /* select the correct stack */
   stack = (char *) client[sock_id].stack;
   sp = client[sock_id].sp;

   /* if not EOF of stack process one line */
   if (!feof(rx) && (fgets(buf,sizeof buf,rx) != NULL) ) {
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARS) {
         myprintf(  "DEBUG: process_client, Read %u bytes into buffer\n", UTILS_find_newline( (char *)&buf ) );
         /* 2011/03/06 - futher debugging needed, added below */
         myprintf(  "DEBUG: %s \n", (char *)&buf );
      }
      process_client_request( tx, buf, shutdown_flag, sock_id );
   }

   /* is still stuff stacked just return */
   if (!feof(rx) ) {
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARS) {
         myprintf(  "DEBUG: process_client, Not EOF on data stream, so saving stack pointer and returning...\n" );
	  }
      client[sock_id].sp = sp;
      return 0;
   }

   /* this could be a normal exit/disconnect from the service so don't treat as
    * an error !. But it is an EOF on the data stream so close sockets */
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARS) {
      myprintf(  "DEBUG: process_client, Fell thru data check, so closing sockets...\n" );
   }

   /* if we get here the client sent an EOF so we are done with it */
   fclose( tx );
   shutdown(fileno(rx),SHUT_RDWR);
   fclose( rx );

/*   myprintf(  "INFO: Disconnect received from %s\n", client[sock_id].remoteaddr ); */

   client[sock_id].rx = client[sock_id].tx = NULL;
   while (sp > 0) {
      free( &stack[--sp] );
   }
   free(stack);
   client[sock_id].stack = NULL;
   client[sock_id].sp = 0;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving process_client\n" );
   }

   return EOF;
} /* process_client */

/* =========================================================
 * main:
 * 
 * Initialise all the function libraries.
 * If OK establish a socket connection for the port we listen for requests on.
 * Loop waiting for client activity.
 *   On client activity process the client request.
 *   On inactivity timer see if we have any other work to do before listening
 *   again.
   ========================================================= */
int main (int argc, char **argv, char **envp) {
   int z, i;
   int use_specific_interface = 0;  /* default is bind to any interface */
   struct sockaddr_in adr_inet;    /* AF_INET */
   int len_inet;                   /* length */

   struct sockaddr_in connect_addr;    /* AF_INET */
   int connect_addr_len;
   int connect_sock;      /* incomming connect socket file handle */
   struct utsname u_name;
   int srvr_port;         /* port number to use */

   /* for getsockettest */
   char buf[64];

   /* for writes */
   char workbuf[128];
   int  workbufdatalen;
   time_t td;

   fd_set rx_set;      /* read set */
   fd_set work_set;    /* work set */
#ifdef SOLARIS
   /* SOLARIS timestamp value */
   struct timespec tv;   /* timeout value SOLARIS */
#else
   /* LINUX assumed */
   struct timeval tv;  /* timeout value LINUX */
#endif
   int mx;   /* Max fd + 1 */
   int n;    /* socket/file activity */

   long status_flag;
   int job_status;
   long file_check_counter;
   
   /* for checking is a forked child has ended */
   pid_t completed_job_pid;

   /* determines if a client evere asks us to leave the main loop and terminate */
   int shutdown_flag;

   /* Used to change socket options */
   int reuse_addr;

   /* to throttle job queuing messages */
   int queue_message_written, jobspawn_status, disabled_message_written;
   queue_message_written = 0;
   disabled_message_written = 0;

   /* Before anything else, initialise the memory library */
   MEMORY_initialise_malloc_counters();
   SCHED_init_memcounters_required = 0; /* counters done above, set local flag to OK for now */

   /* check for option overrides of the default port 9002 */
   if (argc >= 2 ) {
      srvr_port = atoi(argv[1]);
   }
   else {
      srvr_port = 9002;
   }
   if (argc >= 3) {
      use_specific_interface = 1; /* use whats in argv[2] */
   }

   /* We should open out log file first, so we can actually use it */
   SERVER_roll_log_files();

   /* Then startup the server */
   myprintf(  "INFO:SI034-Job Scheduler - Open Source software (C)Mark Dickinson, 2002, all rights reserved\n" );
   myprintf(  "----:SI035 Server initialisation beginning\n" );
   shutdown_flag = 0;     /* of course we don't want to shutdown yet */
   SCHED_init_required = 1;  /* redo the next job to run checks */
   GLOBAL_newday_alert_outstanding = 0;

   /* initialise the running job table first, we have no running jobs at this
    * point. */
   running_job_table.number_of_jobs_running = 0;
   clear_job_entry_table_from( 0 );

   /* we have no jobs requiring exclusive access running */
   running_job_table.exclusive_job_running = 0;
   
   /* Set file_check_counter back to zero */
   file_check_counter = 0;

   /* clear the alert table */
   for (i = 0; i < MAX_RUNNING_ALERTS; i++) { running_alert_table.running_alerts[i].alert_pid = 0; }
   /* Try and open all files and clear/fix the other tables */
   SERVER_Open_All_Files();
   clean_up_job_entry_table_on_restart(); /* added to recover from kill while jobs are running */
   consistency_check_on_restart();        /* added to recover from kill while files were being updated */
   SCHED_Submit_Newday_Job();
   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
      myprintf(  "INFO:SI069-All scheduler databases available.\n" );
      myprintf(  "INFO:SI070-Opening TCP-IP services\n" );
   }

   /*
    * TCPIP stuff 
    * Initialise the client table we use to manage sessions. Open a socket
    * session to tcp-ip services and listen on the configured port. Indicate
    * read interest on the socket.
    */

   /* Initialise client structure */
   for ( z = 0; z < MAX_CLIENTS; ++z ) {
      client[z].stack = NULL;
      client[z].sp = 0;
      client[z].rx = NULL;
      client[z].tx = NULL;
   }

   /* IPv4 internet socket */
   listen_sock = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
   if ( listen_sock == -1 ) die( "socket()", DIE_TCPIP_SETUP );

   /* Reuse the socket (when stopped/started it was complaining about port
	* in use for up to two hours, a linux config issue we don't want to
	* play around with, so allow the port to be re-used in software. */
   reuse_addr = 1;
   if (setsockopt( listen_sock, SOL_SOCKET, SO_REUSEADDR, (int *)&reuse_addr, sizeof(reuse_addr) ) != 0) {
      myprintf( "WARN:SW034-Unable to set SO_REUSEADDR for socket, port %d (ok if listening on all), continuing\n", srvr_port );
   }

   /* create an inet address */
   memset( &adr_inet, 0, sizeof adr_inet );
   adr_inet.sin_family = AF_INET;
   adr_inet.sin_port = htons(srvr_port);

   /* Check to see if we allow binding on any local 
	* interface, or if we are retricted to a single
	* interface */
   if (use_specific_interface != 0) {
      /* bind only to the address provided */
      myprintf( "INFO:SI071-Server configured to bind to interface %s, port %d only\n", argv[2], srvr_port );
      adr_inet.sin_addr.s_addr = inet_addr(argv[2]);
   }
   else {
      /* ---- allow binding on any local interface ---- */
      /* (also allows to connect out via any interface) */
      myprintf( "INFO:SI072-Server configured to bind to all interfaces, port %d\n", srvr_port );
      adr_inet.sin_addr.s_addr = htonl(INADDR_ANY);
   }

#ifdef SOLARIS
   /* Solaris does not have a INADDR_NONE defined
    * but returns -1 for a bad address */
   if (adr_inet.sin_addr.s_addr == -1 ) {
#else
   if ( adr_inet.sin_addr.s_addr == INADDR_NONE ) {
#endif
      die( "Translating IP address", DIE_TCPIP_SETUP );
   }

   len_inet = sizeof adr_inet;

   /* bind the address to the socket */
   z = bind( listen_sock, (struct sockaddr *)&adr_inet, len_inet );
   if ( z == -1 ) {
      myprintf( "*ERR:SE067-Unable to bind to configured interface(s)\n" );
      snprintf( workbuf, 127, "bind() port %d", srvr_port );
      die(workbuf, DIE_TCPIP_SETUP); 
   }

   /* listen on the port looking for connections */
   z = listen( listen_sock, 10 );  /* allow up to 10 connect requests to be queued */
   if ( z == -1 ) die("listen()", DIE_TCPIP_SETUP );

   /* pass the socket around to ensure we can get port info */
   if ( !get_sock_addr(listen_sock,buf,sizeof buf) ) die("get_sock_addr", DIE_TCPIP_SETUP );
   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
      myprintf(  "INFO:SI073-Server listening on port address '%s'\n", buf );
   }

   /* Express interest in the socket file for READ events */
   FD_ZERO( &rx_set );
   FD_SET( listen_sock, &rx_set );
   mx = listen_sock + 1;   /* max fd + 1 */

   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
      myprintf(  "INFO:SI074-TCP-IP Services now available\n" );
   }

#ifdef SOLARIS
   myprintf(  "INFO:SI075-Server has initialised\n" );
#else
   myprintf(  "INFO:SI076-Server has initialised, going daemon now\n" );
   /* Switch ourselves to run as a deamon now */
   z = daemon( 1, 1 ); /* don't change directory to /, dont redirect stderr and stdout to /dev/null */
   if (z == -1) {
		   perror( "Request to run as a daemon" );
		   myprintf(  "*ERR:SE068-Unable to run as a deamon task, running as a normal task !\n" );
   }
#endif

   /* If execjobs was off at the last shutdown, retain that state but
      also raise an alert so that state is known.
   */
   if (pSCHEDULER_CONFIG_FLAGS.enabled != '1' ) {
      myprintf(  "*ERR:SE073-Server has EXECJOBS OFF, retained from last shutdown\n" );
   }

   /* start a server loop, for is more efficient than checking shutdown_flag each loop */
   for (;;) {
      /* copy the rx set to the work set */
      FD_ZERO( &work_set );
      for ( z = 0; z < mx; ++z ) {
         if ( FD_ISSET(z,&rx_set) ) FD_SET( z, &work_set );
      }

      /* set a timeout of 1.3 seconds, and wait for activity */
      tv.tv_sec = 1;
#ifdef SOLARIS
      /* Solaris generates a warning here, I don't know why ? */
#endif
      tv.tv_usec = 30000;
      n = select( mx, &work_set, NULL, NULL, &tv );
      if ( n == -1 ) {
         die( "select()", DIE_TCPIP_SETUP );
      }
      /* -ELSE----------------------------------------------------------------
       *  Our 1.3 second timer expired without any activity.
       *  We use this inactivity trigger to check the scheduler activity tasks
       *  to see if there is anything we need to do; run jobs, start newday
       *  etc.
       * ----------------------------------------------------------------------*/
      else if ( !n ) {
         /* --- timer expired, check scheduler stuff */
         if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_TIMERS) {
            myprintf(  "DEBUG: Timer expired, checking scheduler work\n" ); 
         }
         /* --- FIRST (new) see if we are pending a shutdown, and if we can */
         if ( (shutdown_flag == 1) && (INTERNAL_count_executing() == 0) ) {
            if (pSCHEDULER_CONFIG_FLAGS.log_level > 0) {
                myprintf(  "WARN:SW035-SHUTDOWN NOW POSSIBLE, Server is now no longer acception connections...\n" );
            }
            shutdown_immed();
         }

         /* --- first print any message saved first */
         UTILS_print_last_message();
         /* --- see if any child jobs have completed */
         if (running_job_table.number_of_jobs_running > 0) {
             if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_TIMERS) {
                myprintf(  "DEBUG: Checking if any running jobs have completed.\n" ); 
             }
             completed_job_pid = waitpid( -1, (int *)&status_flag, WNOHANG || WUNTRACED );
             if (completed_job_pid > 0) {
                 /* check to see if it was an alert we can discard */
                 if (clean_up_alert_table( (pid_t *)&completed_job_pid ) == 0) { /* it wasn't */
                     /* else a child job has completed, free up the running job table
                      * entry for it */
                     clean_up_job_entry_table( (pid_t *)&completed_job_pid, status_flag );
                  }
             }
         }
         /*
          * If the newday has just run it will want us to reset the memory
          * counters in the memory library, check for that (needed to be done
          * from the mainline to ensure the stack is (should be) empty.
         */
         /* --- See if the server recheck flag has been set for anything, this may
          * be something simple like a manual recheck request, or pherhaps a
          * new job submitted etc., but we do a recheck if set. */
         if (SCHED_init_required != 0) {
            /* this check is here as the newday triggers a sched_init, and we do not
               want to do this check every loop but only when a newday runs, so the
               minimises the overhead a litte. May need to revisit if ever need to
               reset anywhere but from the newday job.
               Note: also requested by the server startup.
            */
            if (SCHED_init_memcounters_required > 0) { /* do we need to clear the memory counters,
                                                          currently only requested by the newday job. */
                MEMORY_reset_counters_on_newday();
            }
            if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARS) {
               myprintf(  "DEBUG: Server scheduling order recheck requested.\n" ); 
            }
            if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARS) {
               myprintf(  "DEBUG: Setting SCHED_init_required = 0 (main), processed it\n" ); 
            }
            SCHED_init_required = 0;
            job_status = SCHED_ACTIVE_get_next_jobtorun( &next_job_to_run );
            if (job_status == 0) {
               if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
                  myprintf( "INFO:SI077-There are no jobs available to run at present.\n" );
               }
               UTILS_zero_fill((char *)&next_job_to_run.next_scheduled_runtime, JOB_DATETIME_LEN );
               UTILS_zero_fill((char *)&next_job_to_run.job_header.jobname, JOB_NAME_LEN);
               UTILS_zero_fill((char *)&next_job_to_run.job_header.jobnumber, JOB_NUMBER_LEN );
               next_job_to_run.run_timestamp = 0;
            }
            else if (job_status == 1) {
               /*myprintf( "INFO:SI078-Next job to be run has been selected from the queue\n" ); */
            }
            else {
               myprintf( "*ERR:SE069-error finding next job to run, job processing suspended until error is corrected\n" );
               UTILS_zero_fill((char *)&next_job_to_run.next_scheduled_runtime, JOB_DATETIME_LEN );
               UTILS_zero_fill((char *)&next_job_to_run.job_header.jobname, JOB_NAME_LEN);
               UTILS_zero_fill((char *)&next_job_to_run.job_header.jobnumber, JOB_NUMBER_LEN );
               next_job_to_run.run_timestamp = 0;
            }
         }  /* end if sched init required */
         /* --- Check to see if any jobs are to be run now */
         if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_TIMERS) {
            myprintf(  "DEBUG: Starting next job to run timestamp check\n" );
         }
         if ( (pSCHEDULER_CONFIG_FLAGS.enabled == '1') && (shutdown_flag == 0) ) { 
            if (next_job_to_run.run_timestamp != 0) {
               if ( next_job_to_run.run_timestamp <= UTILS_timenow_timestamp() ) {
                  if (pSCHEDULER_CONFIG_FLAGS.enabled == '1') {
                      if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARS) {
                         myprintf(  "DEBUG: Now ready to run job %s\n", next_job_to_run.job_header.jobname );
                      }
                      if (memcmp(next_job_to_run.job_header.jobname,"SCHEDULER",9) == 0) { /* internal task */
                         SERVER_spawnsimulate_task( &next_job_to_run );
                      }
                      else if (  (jobspawn_status = SCHED_spawn_job( &next_job_to_run )) != 1 ) {
                         if (jobspawn_status == 2) {
                            if (queue_message_written == 0) {
                               myprintf(  "WARN:SW036-Maximum jobs running, jobs queueing, examine schedule and try to spread out jobs\n" );
                               myprintf(  "WARN:SW037-Maximum of %d simultaneous jobs has been reached\n", MAX_RUNNING_JOBS );
                               myprintf(  "WARN:SW038-The job trying to run has been requeued and will run next.\n" );
                               queue_message_written = 1;
                            }
                      }
                      else {
                         myprintf(  "*ERR:SE070-job %s failed to run, job processing suspended until problem corrected !\n",
                         next_job_to_run.job_header.jobname );
                         /* TODO - generate a generic system alert */
                         pSCHEDULER_CONFIG_FLAGS.enabled = '0';
                      }
                   }
                   else {
                      /* we need a new job to queue on */
                      if (pSCHEDULER_CONFIG_FLAGS.debug_level.server >= DEBUG_LEVEL_VARS) {
                         myprintf(  "DEBUG: Setting SCHED_init_required = 0 (main), job spawned, need new one\n" ); 
                      }
                      SCHED_init_required = 1;
                      queue_message_written = 0;
                      disabled_message_written = 0;
                   }
                }
                else {
                   if (disabled_message_written == 0) {
                      myprintf(  "WARN:SW040-JOBS ARE READY TO RUN BUT ARE QUEUEING as SCHEDULER EXECJOBS OFF is set\n" );
                      disabled_message_written = 1;
                   }
                }
             }
          } /* if timestamp of next job <> 0 */
       } /* if pSCHEDULER_CONFIG_FLAGS.enabled == 1 */
       /* --- every 5 minutes aproximately check to see if a file we are
       * waiting on has appeared. */
       file_check_counter++;
       if (file_check_counter > 290) { /* is we havn't done a check in five minutes, do one */
          SCHED_DEPEND_delete_all_relying_on_files();
          file_check_counter = 0;
       }
       /* --- see if we need to roll over the current logfile yet --- */
       if ( logfile_roll_time <= UTILS_timenow_timestamp() ) {
          SERVER_roll_log_files();
       }
       /* --- last but not least, see if there are any internal system tasks
        * we need to do, such as the newday scheduling. 
        * At this point I am doing that by submitting a SCHEDULER-NEWDAY job
        * at server startup, so no additional checks are needed here. This
        * may be revisited later. */ 
       /* and we are finally done with internal checks */
       continue;
    }

    /* -------------------------------------------------------------------
     * We will always check to see if any new clients are trying to connect.
     * ------------------------------------------------------------------- */
     /* check if a connect has occurred */
      if ( FD_ISSET( listen_sock, &work_set ) ) {
         /* wait for a connect */
         connect_addr_len = sizeof connect_addr;
#ifdef GCC_MAJOR_VERSION3
         /* The below works ok with GCC version 3.4.2 (not with 4.1.1) */
         connect_sock = accept( listen_sock, (struct sockaddr *)&connect_addr,
                                &connect_addr_len );
#else 
		 /* version 4 or above */
         connect_sock = accept( listen_sock, (struct sockaddr *)&connect_addr,
                                (socklen_t *)&connect_addr_len );
#endif
         if ( connect_sock == -1 ) die("accept()", DIE_TCPIP_SETUP );

         /* check to ensure we havn't exceeded capacity */
         if ( connect_sock > MAX_CLIENTS ) {
	        myprintf(  "WARN:SW041-Client connection refused, server alrady has maximum connections allowed !\n" );
            close( connect_sock );   /* at capacity */
            continue;
         }

         /* if we have a connection  we need to open read and write streams on
            that socket connection */
         client[connect_sock].rx = fdopen( connect_sock, "r" );
         if ( client[connect_sock].rx == NULL ) die( "fdopen()", DIE_TCPIP_SETUP );
         client[connect_sock].tx = fdopen(dup(connect_sock), "w" );
         if ( client[connect_sock].tx == NULL ) {
            fclose( client[connect_sock].rx );
            die( "fdopen( dup() )", DIE_TCPIP_SETUP );
         }
         if ( connect_sock + 1 > mx ) mx = connect_sock + 1;

         /* set both streams to line buffered mode */
         setlinebuf( client[connect_sock].rx );
         setlinebuf( client[connect_sock].tx );

         /* allocate a stack */
         client[connect_sock].sp = 0;
         client[connect_sock].stack = (mpz_t **) malloc(sizeof (mpz_t *) * MAX_STACK);
       
         FD_SET( connect_sock, &rx_set );

         /* do work  */

		 /* log the connection we have received */
         if (get_remote_addr( connect_sock, (char *) client[connect_sock].remoteaddr, 50 ) != NULL) {
/*		    myprintf(  "INFO:SI079-Connection accepted from %s\n", client[connect_sock].remoteaddr ); */
		 }
		 else {
		    myprintf(  "INFO:SI080-Connection from a host that hides its ip address\n" );
			strcpy( client[connect_sock].remoteaddr, "Hidden\n\0" );
		 }

		 /* default logon access at this time is operator rights */
		 client[connect_sock].access_level = '0';
		 strcpy(client[connect_sock].user_name, "guest");
         UTILS_space_fill_string((char *)&client[connect_sock].user_name, USER_MAX_NAMELEN);
		 client[connect_sock].accept_unsol = 'N';
		 client[connect_sock].user_pid = 0; /* unknown until connection logs on */

         z = uname( &u_name );
         if ( z == -1 ) {
            myprintf("WARN:SW042-uname error\n" );
            strcpy( u_name.nodename, "UN-NAMED HOST" );
         }
		 
		 /* write the server banner to the connection port
		  * this ensures we can write to the client. */
         time(&td);
         workbufdatalen = (int) strftime( workbuf, sizeof workbuf,
                                         "%A %b %d %H:%M:%S %Y",
                                         localtime(&td)
                                        );
		 z = strlen(workbuf) - 1;
		 strncpy( workbuf, workbuf, z );
         strcat( workbuf, " - READY" );
		 if (shutdown_flag != 0) {
            strcat( workbuf, " - *** SERVER IS SHUTTING DOWN ***\n" );
		 }
         else {
            strcat( workbuf, "\n" );
         }
         z = write( connect_sock, workbuf, strlen(workbuf) ); 
         if ( z == -1 ) die( "write()", DIE_INTERNAL );
     }  /* end if FD_ISSET */            

      /* -------------------------------------------------------------------
       *  And we will always check to see if any clients have asked us to do
       * anything.
       * ------------------------------------------------------------------- */
      /* check for client activity */
      for ( connect_sock = 0; connect_sock < mx; ++connect_sock ) {
         if ( connect_sock == listen_sock ) 
            continue;      /* don't worry about listen_sock */
         if ( FD_ISSET(connect_sock, &work_set) ) {
            if ( process_client(connect_sock, &shutdown_flag) == EOF ) {
               FD_CLR(connect_sock, &rx_set);
            }
            /* Check if the client issued a shutdown request that we accepted */
            if ( (shutdown_flag == 1) && (INTERNAL_count_executing() == 0) ) {
               shutdown(listen_sock,SHUT_RDWR);
               close( listen_sock );
			   myprintf(  "INFO:SI081-Server is now no longer acception connections...\n" );
               shutdown_immed();
            }
         }
      }
   }  /* end of for (;;) */

   /* should never get here, but manage anyway */

   /* clean up and exit */
   shutdown(listen_sock,SHUT_RDWR);
   close( listen_sock );
   myprintf(  "*ERR:SE071-SHUTTING DOWN DUE TO UNEXPECTED MAIN TERMINATION\n" );
   shutdown_immed();
   return 0;
}
