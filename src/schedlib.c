#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "debug_levels.h"
#include "scheduler.h"
#include "schedlib.h"
#include "alerts.h"
#include "utils.h"
#include "api.h"
#include "bulletproof.h"
#include "memorylib.h"
#include "system_type.h"

/* The below has been defined in server.c specifically for 
 * procedure SCHED_show_schedule. */
extern int get_pid_of_job( job_header_def * job_header, pid_t * pid_result );

#ifdef SOLARIS
/* In solaris the environ variable needs to be explicitly declared
 * extern. This is not needed for Linux */
extern char **environ;
#endif

/* ---------------------------------------------------
   SCHED_Initialise
   Perform any initialisation required by the scheduling
   component of this application.
   --------------------------------------------------- */
int SCHED_Initialise( void ) {
   /*
    *  The active job queue file.
    */
   if ( (active_handle = fopen( active_file, "rb+" )) == NULL ) {
	  if (errno == 2) {
         if ( (active_handle = fopen( active_file, "ab+" )) == NULL ) {
		    myprintf( "*ERR: LE001-Unable to create active jobs file %s, errno %d\n", active_file, errno );
		    perror( "Open of active jobs file failed" );
		    return 0;
		 }
		 else {
	        /* created, close and open the way we want */
            if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
		       myprintf( "INFO: LI001-Created active jobs file %s\n", active_file );
			}
	        fclose( active_handle );
			active_handle = NULL;
            if ( (active_handle = fopen( active_file, "rb+" )) == NULL ) {
               myprintf( "*ERR: LE002-Open of active jobs file %s failed, errno %d (I created it OK though)\n", active_file, errno );
               perror( "Open of active jobs file failed:" );
               return 0;
			}
	     }
	  }
	  else {
         myprintf( "*ERR: LE003-Open of active jobs file %s failed, errno %d\n", active_file, errno );
         perror( "Open of active jobs file failed:" );
         return 0;
	  }
   }
   else {
      fseek( active_handle, 0, SEEK_SET );
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
         myprintf( "INFO: LI002-Open of active jobs file %s completed\n", active_file );
	  }
   }

   /*
    *  Job dependency queue file.
    */
   if ( (dependency_handle = fopen( dependency_file, "rb+" )) == NULL ) {
	  if (errno == 2) {
         if ( (dependency_handle = fopen( dependency_file, "ab+" )) == NULL ) {
		    myprintf( "*ERR: LE004-Unable to create dependency queue file %s, errno %d\n", dependency_file, errno );
		    perror( "Open of dependency queue file failed" );
		    return 0;
		 }
		 else {
	        /* created, close and open the way we want */
	        fclose( dependency_handle );
			dependency_handle = NULL;
            if ( (dependency_handle = fopen( dependency_file, "rb+" )) == NULL ) {
               myprintf( "*ERR: LE005-Open of dependency queue file %s failed, errno %d (I created it OK though)\n", dependency_file, errno );
               perror( "Open of dependency queue file failed:" );
               return 0;
			}
	     }
	  }
	  else {
         myprintf( "*ERR: LE006-Open of dependency queue file %s failed, errno %d\n", dependency_file, errno );
         perror( "Open of dependency queue file failed:" );
         return 0;
	  }
   }
   else {
      fseek( dependency_handle, 0, SEEK_SET );
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
         myprintf( "INFO: LI003-Open of dependency queue file %s completed\n", dependency_file );
	  }
   }

   /*
    * We wish the scheduler main code to refresh the next active job
    * to be executed details.
    */
   SCHED_init_required = 1;

   /*
    *  Everything opened OK
    */
   return 1;
} /* SCHED_Initialise */


/* ---------------------------------------------------
   SCHED_Terminate
   Perform any cleanup required by the scheduling
   components.
   --------------------------------------------------- */
void SCHED_Terminate( void ) {
   if (active_handle != NULL) {
      fclose( active_handle );
	  active_handle = NULL;
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
         myprintf( "INFO: LI004-Closed: Active jobs file\n" );
	  }
   }
   else {
      myprintf( "*ERR: LE007-SCHED_Terminate called when no active jobs file was open, fclose ignored !\n" );
   }
   if (dependency_handle != NULL) {
      fclose( dependency_handle );
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
         myprintf( "INFO: LI005-Closed: Dependency Queue file\n" );
	  }
   }
   else {
      myprintf( "*ERR: LE008-SCHED_Terminate called when no dependency file was open, fclose ignored !\n" );
   }
} /* SCED_Terminate */


/* **********************************************************
   **********************************************************

   Scheduler interface/control routines.

   **********************************************************
   ********************************************************** */


/* ---------------------------------------------------
   SCHED_schedule_on
   --------------------------------------------------- */
int SCHED_schedule_on( jobsfile_def * datarec ) {
   int i;
   int create_dependency_record;
   active_queue_def *local_active_rec;
   dependency_queue_def *local_dependency_rec;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
	   myprintf("DEBUG: entered SCHED_schedule_on\n" );
   }

  /* 
   check to ensure job is not already scheduled
   add the job record passed to the active scheduler file
   if necessary add to the dependencies file
  */

  /*
   *  Check to ensure the job is not already scheduled.
   */
   if ((local_active_rec = (active_queue_def *)MEMORY_malloc( sizeof( active_queue_def ),"SCHED_schedule_on" )) == NULL) {
      myprintf( "*ERR: LE009-Unable to allocate memory required for read operation\n" );
      UTILS_set_message("Unable to allocate memory required for read operation");
      return( -1 );
   }

   strcpy( local_active_rec->job_header.jobname,
           datarec->job_header.jobname );
   if (SCHED_ACTIVE_read_record( local_active_rec ) != -1) {
      myprintf( "*ERR: LE010-A job with this jobname is already scheduled on.\n" );
      UTILS_set_message("A job with this jobname is already scheduled on.");
      MEMORY_free( local_active_rec );
      return( -1 );
   }

   /* ...................................................
    * Build an active file record for the job
    * ................................................... */
   create_dependency_record = 0;
   memcpy( local_active_rec->job_header.jobnumber,
           datarec->job_header.jobnumber,
           sizeof(job_header_def) );
   strncpy( local_active_rec->next_scheduled_runtime,
           datarec->next_scheduled_runtime, JOB_DATETIME_LEN );
   UTILS_space_fill_string( (char *)&local_active_rec->next_scheduled_runtime, JOB_DATETIME_LEN );
   local_active_rec->current_status = '0'; /* time wait */
   memcpy( local_active_rec->failure_code, "000", JOB_STATUSCODE_LEN );
   local_active_rec->hold_flag = 'N';
   for (i = 0; i < MAX_DEPENDENCIES; i++) {
      if (datarec->dependencies[i].dependency_type != '0') {
        local_active_rec->current_status = '1'; /* dependency wait */
        create_dependency_record = 1;
      }
   }
   local_active_rec->run_timestamp = UTILS_make_timestamp( (char *)&local_active_rec->next_scheduled_runtime );
   strncpy( local_active_rec->job_owner, datarec->job_owner, JOB_OWNER_LEN );
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_VARS) {
      myprintf("DEBUG: SCHED_schedule_on, Active queue details at this point\n" );
      SCHED_DEBUG_dump_active_queue_def( local_active_rec );
   }
   memcpy( local_active_rec->started_running_at, "00000000 00:00:00", JOB_DATETIME_LEN );
   local_active_rec->started_running_at[JOB_DATETIME_LEN] = '\0';
   if (SCHED_ACTIVE_write_record( local_active_rec, -1 ) == -1) {
      MEMORY_free( local_active_rec );
      myprintf( "*ERR: LE011-Job could not be added to active jobs file.\n" );
      UTILS_set_message("Job could not be added to active jobs file.");
      return( -1 );
   }

   /*
    * free the memory allocated when we are done with active rec.
    */
   MEMORY_free( local_active_rec );
   
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_VARS) {
	  myprintf( "DEBUG: SCHED_schedule_on, active queue record for %s created\n", datarec->job_header.jobname ); 
   }

  /* ....................................................
   * If we need a dependency record for the job build one
   * .................................................... */
  if (create_dependency_record != 0) {
     if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_VARS) {
	    myprintf( "DEBUG: SCHED_schedule_on, creating dependancy records for %s created\n", datarec->job_header.jobname ); 
     }
     if ((local_dependency_rec = (dependency_queue_def *)MEMORY_malloc( sizeof( dependency_queue_def ),"SCHED_schedule_on" )) == NULL) {
        myprintf( "*ERR: LE012-Unable to allocate memory required for dependency storage\n" );
        UTILS_set_message( "Unable to allocate memory required for dependency storage" );
        i = SCHED_delete_job( (job_header_def *) datarec );  /* do not leave it scheduled */
        return( -1 );
     }
     memcpy( &local_dependency_rec->job_header,
             &datarec->job_header,
             sizeof( job_header_def ) );
     memcpy( local_dependency_rec->dependencies,
             datarec->dependencies,
             (sizeof( dependency_info_def ) * MAX_DEPENDENCIES) );
	  /* sanity check the dependency values, that is space fill job names */
	  for (i = 0; i < MAX_DEPENDENCIES; i++ ) {
			  if (local_dependency_rec->dependencies[i].dependency_type == '1') {
					  UTILS_space_fill_string( (char *)&local_dependency_rec->dependencies[i].dependency_name,
									  JOB_NAME_LEN );
			  }
	  }
	  /* write the record */
      if (SCHED_DEPEND_write_record( local_dependency_rec, -1 ) == -1) {
         MEMORY_free( local_dependency_rec );
         myprintf( "*ERR: LE013-Unable to write dependency record\n" );
         i = SCHED_delete_job( (job_header_def *) datarec );  /* do not leave it scheduled */
         return( -1 );
      }
      MEMORY_free( local_dependency_rec );  
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_VARS) {
	     myprintf( "DEBUG: SCHED_schedule_on, created dependancy records for %s\n", datarec->job_header.jobname ); 
      }
  } /* if a depency record was needed */
  
  /* Change the flag state to request the server mainline to refresh its next
   * job to run information in case we have subitted a job with an earlier time
   * than the nexr job to run it already is keeping an eye on the time for.
   */
  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_VARS) {
      myprintf( "DEBUG: Setting SCHED_init_required = 1 (SCHED_schedule_on)" ); 
  }
  SCHED_init_required = 1;  /* lets refresh our nextrun info */
  
  /* record that we have added a new job */
  if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
     myprintf( "INFO: LI006-SCHED_schedule_on, job %s scheduled on\n", datarec->job_header.jobname ); 
  }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
	   myprintf("DEBUG: Leaving SCHED_schedule_on\n" );
   }
  return ( 0 );   /* no error */
} /* SCHED_schedule_on */


/* ---------------------------------------------------
   SCHED_run_job_now

   Change a jobs next runtime to make it run immediately,
   clear any alerts or dependencies on the job to 
   ensure it can run, set the scheduler check flag so
   it will rescan the job list for the next job to
   run.
   Return 1 if failed, 0 if ok, 
          2 if job is in failed state or jobwait state
   --------------------------------------------------- */
int SCHED_run_job_now( active_queue_def * job_name ) {
  /*
   * Update the time in the active record file to indicate
   * that the job should run NOW !.
   */
  long recnum;
  time_t time_now;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
	   myprintf("DEBUG: Entered SCHED_run_job_now\n" );
   }

  recnum = SCHED_ACTIVE_read_record( job_name );
  if (recnum == -1) {
     return( 1 );
  }

  /* check the job is a candidate to be 'runnow' */
  if (job_name->current_status != '0') {
		  return( 2 );   /* is either completed or waiting on something */
  }

  /* set the next runtime we check to the current time */
  time_now = time(0);
  job_name->run_timestamp = time_now;
  job_name->hold_flag = 'N';       /* ensure it is not held */
  if (SCHED_ACTIVE_write_record( job_name, recnum ) == -1) {
     return( 1 );
  }

  /*
   * active_queue and dependency_queue share the same job_header so just
   * pass that to the depencency queue routine.
   */
  if (SCHED_DEPEND_delete_record( (dependency_queue_def *) job_name ) == -1) {
     /* not found so don't worry about it */
  }

  /*
   * active_queue and alerts_queue share the same job_header so just
   * pass that to the depencency queue routine.
   */
  if (ALERTS_delete_alert( (alertsfile_def *) job_name ) == -1) {
     /* not found so don't worry about it */
  }

  /*
   * Set the flag to indicate the scheduler mainline should
   * update its next job to run details.
   */
  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_VARS) {
      myprintf( "DEBUG: Setting SCHED_init_required = 1 (SCHED_run_job_now)" ); 
  }
  SCHED_init_required = 1;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
	   myprintf("DEBUG: Leaving SCHED_run_job_now\n" );
   }

  return ( 0 );
} /* SCHED_run_job_now */


/* ---------------------------------------------------
   SCHED_delete_job
   --------------------------------------------------- */
int SCHED_delete_job( job_header_def * job_name ) {
   char *workbuffer;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
	   myprintf("DEBUG: Entered SCHED_delete_job\n" );
   }

   /* WARNING: 8192 is assumed to be bigger than the largest file record
               structure, adjust if this is not the case */
   if ((workbuffer = (char *)MEMORY_malloc( 8192,"SCHED_delete_job" )) == NULL) {
      myprintf( "*ERR: LE014-Unable to allocate memory required for workspace\n" );
      UTILS_set_message("Unable to allocate memory required for workspace");
      return( -1 );
   }

  /*
   * Delete from active file first so we can determine
   * whether the record actually exists or not.
   */
  memcpy( workbuffer, job_name, sizeof(job_header_def) );
  if (SCHED_ACTIVE_delete_record( (active_queue_def *) workbuffer, 0 /* 0 = not job completion triggered */ ) == -1) {
     MEMORY_free( workbuffer );
     myprintf( "WARN: LW001-Job not found in active jobs file.\n" );
     UTILS_set_message( "Job not found to be deleted.\n" );
     return( -1 );
  }

  /* SCHED_active_delete_record deletes any dependency record for this job */
  /* SCHED_active_delete record also deletes any alerts */
  /* SCHED_ACTIVE_delete record clears all dependencies */

  /*
   * free the large chunk of memory we were using.
   */
  MEMORY_free( workbuffer );

  /*
   * Set the flag to indicate the scheduler mainline should
   * update its next job to run details.
   */
  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_VARS) {
      myprintf( "DEBUG: Setting SCHED_init_required = 1 (SCHED_delete_job)" ); 
  }
  SCHED_init_required = 1;

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
     myprintf("DEBUG: Leaving SCHED_delete_job\n" );
  }
  return ( 0 );  /* no errors */
} /* SCHED_delete_job */


/* ---------------------------------------------------
   return length of display string if ok, -1 if error
   Allow mask matching for start of jobname in selection.
   --------------------------------------------------- */
int SCHED_show_schedule( API_Header_Def * api_bufptr, FILE *tx  ) {
   /*
     Scan the active file and display each job on it
     For each job...
        check and display any details for the job in the
        dependencies file.
   */
  int lasterror, sinkreply, suppressed;
  active_queue_def * local_rec;
  char smallworkbuf[61];
  char workbuf[129];
  int  worklen;
  dependency_queue_def local_depend;
  long dependnum;
  int loopcounter;
  pid_t result_pid;
  int pid_request_result;
  long next_recordnum;
  int corrupt_count;
  time_t record_time;
  time_t time_now;
  char mask[JOB_NAME_LEN+1];
  int masklen;

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
     myprintf("DEBUG: Entered SCHED_show_schedule\n" );
  }

   if ( (api_bufptr->API_Data_Buffer[0] != '\0') && (api_bufptr->API_Data_Buffer[0] != '\n') ) {
      strncpy( mask, api_bufptr->API_Data_Buffer, JOB_NAME_LEN );
      mask[JOB_NAME_LEN] = '\0';
      masklen = strlen(mask);
      UTILS_uppercase_string((char *)&mask);
   }       
   else {
      masklen = 0;
  }

  API_init_buffer( (API_Header_Def *)api_bufptr ); 
  strcpy( api_bufptr->API_System_Name, "SCHED" );
  strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
  strcpy(api_bufptr->API_Data_Buffer, "" );

  if (MAX_API_DATA_LEN < 100) {
     myprintf( "*ERR: LE015-MAX_API_DATA_LEN too small for display, cant even return an error in it\n" );
     return( 1 );  /* cant even return an error */
  };

  /* Allocate memory for a local copy of the record */
  if ((local_rec = (active_queue_def *)MEMORY_malloc( sizeof( active_queue_def ),"SCHED_show_schedule" )) == NULL) {
     myprintf( "*ERR: LE016-Unable to allocate %d bytes of memory for a record buffer\n", sizeof(active_queue_def) );
     UTILS_set_message( "Unable to allocate memory required for read operation" );
	 strcpy( api_bufptr->API_Data_Buffer, "UNABLE TO ALLOCATE MEMORY ON SERVER FOR THIS REQUEST" );
     API_set_datalen( api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
     api_bufptr->API_More_Data_Follows = '0';
     sinkreply = API_flush_buffer( api_bufptr, tx );
     return( 1 );
  }

  /* Position to the begining of the file */
  lasterror = fseek( active_handle, 0, SEEK_SET );
  if (lasterror != 0) {
     perror( "active_handle" );
     MEMORY_free( local_rec );
     UTILS_set_message( "Seek to start of active jobs file failed" );
	 strcpy( api_bufptr->API_Data_Buffer, "UNABLE TO ACCESS SCHEDULER ACTIVE QUEUE DATABASE, SEE LOG" );
     API_set_datalen( api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
     api_bufptr->API_More_Data_Follows = '0';
     sinkreply = API_flush_buffer( api_bufptr, tx );
     return( 1 );
  }
  next_recordnum = 0;
  corrupt_count = 0;

  /* Loop reading through the file */
  while ( (!(feof(active_handle))) && (!(ferror(active_handle))) ) {
      suppressed = 0; /* we want to write results */
      lasterror = fseek( active_handle, next_recordnum, SEEK_SET );
      lasterror = fread( local_rec, sizeof(active_queue_def), 1, active_handle  );
      if (ferror(active_handle)) {
         perror( "active_handle" );
         UTILS_set_message( "Read error on active job queue file" );
         MEMORY_free( local_rec );
	     strcpy( api_bufptr->API_Data_Buffer, "UNABLE TO READ SCHEDULER ACTIVE QUEUE DATABASE, SEE LOG" );
         API_set_datalen( api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
         api_bufptr->API_More_Data_Follows = '0';
         sinkreply = API_flush_buffer( api_bufptr, tx );
         return( 1 );
      }

	  if (feof(active_handle)) {
		   MEMORY_free( local_rec );
           API_set_datalen( api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
           api_bufptr->API_More_Data_Follows = '0';
           if (API_flush_buffer( api_bufptr, tx ) != 0) {
              return( 1 );
           }
           return( 0 );
	  }
      next_recordnum = ftell(active_handle);  /* save next record position */

	  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_VARDUMP) {
         SCHED_DEBUG_dump_active_queue_def( local_rec );
      }

      /* we have the record, format it into our display buffer */

      /* smallworkbuf is 60 data bytes plus 1 pad  */
	  if (
             (local_rec->job_header.info_flag == 'D') &&
             (pSCHEDULER_CONFIG_FLAGS.debug_level.show_deleted_schedjobs == 1)
		 ) {
		  strcpy( smallworkbuf, "DELETED" );
		  /* add additional info for audit/debugging. List the state it was in
		   * when it was deleted. */
		  if (local_rec->current_status == '0') { strcat(smallworkbuf,"/TIMEWAIT"); }
		  else if (local_rec->current_status == '1') { strcat(smallworkbuf,"/DEPWAIT"); }
		  else if (local_rec->current_status == '2') { strcat(smallworkbuf,"/EXEC"); }
		  else if (local_rec->current_status == '3') { strcat(smallworkbuf,"/FAILED"); }
		  else if (local_rec->current_status == '4') { strcat(smallworkbuf,"/COMPLETED"); }
		  else { strcat(smallworkbuf,"/BAD-STATE"); }
	  }
      else if (local_rec->job_header.info_flag == 'D') {
         /* a deleted entry, but the show deleted entry flag is off
          * so we don't want output for it */
         suppressed = 1;
      }
	  else if (local_rec->current_status == '0') {        /* time wait */
         /* strncpy(smallworkbuf, local_rec->next_scheduled_runtime, JOB_DATETIME_LEN ); */
		 if (local_rec->hold_flag == 'R') {
		    snprintf( smallworkbuf, 60, "PENDING (REQUEUE WAIT)" );
		 }
		 else if (local_rec->hold_flag == 'Y') {
		    snprintf( smallworkbuf, 60, "HOLD IS ON :  %s", local_rec->next_scheduled_runtime );
		 }
		 else if (local_rec->hold_flag == 'C') {
		    snprintf( smallworkbuf, 60, "CALENDAR ERROR, HELD" );
		 }
		 else {
		    snprintf( smallworkbuf, 60, "SCHEDULED FOR %s", local_rec->next_scheduled_runtime );
		 }
      }
      else if (local_rec->current_status == '1') {   /* in dependency queue */
         /* Dependency wait, but show the time if the time is in the future
          * as its not truely in dependency wait if its runtime has not yet
          * been reached */
         record_time = UTILS_make_timestamp( local_rec->next_scheduled_runtime );
         time_now = UTILS_timenow_timestamp();
         if (record_time < time_now) {
             strcpy(smallworkbuf, "DEPENDENCY WAIT  " );
		 }
         else {
		    snprintf( smallworkbuf, 60, "SCHEDULED FOR %s", local_rec->next_scheduled_runtime );
         }
      }
      else if (local_rec->current_status == '2') {   /* running */
         pid_request_result = get_pid_of_job( (job_header_def *)&local_rec->job_header.jobnumber, &result_pid );
		 if (pid_request_result == 0) {
            strcpy(smallworkbuf, "EXECUTING (Orphaned, shutdown recomended)" );
		 }
		 else {
            snprintf( smallworkbuf, 50, "EXECUTING (PID=%d) START:%s", result_pid, local_rec->started_running_at );
		 }
      }
      else if (local_rec->current_status == '3') {   /* failed, will be in alerts file */
         strcpy(smallworkbuf, "FAILED:SEE ALERTS" );
      }
      else {
         snprintf(smallworkbuf, 50, "CORRUPT JOB ENTRY, STATUS=%c", local_rec->current_status );
		 corrupt_count++;
		 if (corrupt_count > 5) {
				 strcpy( workbuf, "TOO MANY BAD JOBS, ENDING LIST, SUGGEST CLEANING DBS" );
                 sinkreply = API_add_to_api_databuf( api_bufptr, (char *)&workbuf, strlen(workbuf), tx );
                 API_set_datalen( api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
                 api_bufptr->API_More_Data_Follows = '0';
                 sinkreply = API_flush_buffer( api_bufptr, tx );
		         MEMORY_free( local_rec );
				 return( 0 );
		 }
      }

/* replace below with the wildcard mask search */
/*      if ( (masklen != 0) && (memcmp(local_rec->job_header.jobname,mask,masklen) != 0) ) { */
      if ( (masklen != 0) && (UTILS_wildcard_parse_string(local_rec->job_header.jobname,mask) == 0) ) {
         suppressed = 1;  /* mask provided, doesn't match mask */
      }

      if (!(suppressed == 1)) {
    	  /* The format is done here */
          worklen = snprintf( workbuf, 128, "%s %s\n",
                              local_rec->job_header.jobname,
                              smallworkbuf  );
          if (worklen != -1) {
             sinkreply = API_add_to_api_databuf( api_bufptr, (char *)&workbuf, strlen(workbuf), tx );
          }

          /* if the job is in dependency wait we need some more information */
          if ( (local_rec->current_status == '1') &&     /* in dependency queue */
               (local_rec->job_header.info_flag != 'D')  /* and not deleted */
    		 ) 
    	  {
             if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_VARS) {
    			  myprintf( "DEBUG: SCHED_show_schedule, JOB HEADER SIZE = %d\n", sizeof( job_header_def ) );
    		 }
             memcpy( local_depend.job_header.jobnumber, (char *)local_rec->job_header.jobnumber, sizeof( job_header_def ) );
             dependnum = SCHED_DEPEND_read_record( &local_depend );
             if (dependnum < 0) {
		    		 myprintf( "*ERR: LE017-UNABLE TO READ DEPENDENCY DETAILS FOR %s\n", local_depend.job_header.jobname );
             }
             else {
                for (loopcounter = 0; loopcounter < MAX_DEPENDENCIES; loopcounter++) {
                   if (local_depend.dependencies[loopcounter].dependency_type != '0') {
                      worklen = snprintf( workbuf, 128, "    WAITING(%d):%s\n",
	    				  loopcounter,
                          local_depend.dependencies[loopcounter].dependency_name );
                      if (worklen != -1) {
                         sinkreply = API_add_to_api_databuf( api_bufptr, (char *)&workbuf, strlen(workbuf), tx );
                      }
                   } /* if type == 0 */
                }    /* for */
             }       /* else */
          }       /* if not deleted */
       }  /* if not suppressed */
   }
  
   /* free the work record */
   MEMORY_free( local_rec );
   
   /* write any remaining reponse */
   API_set_datalen( api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
   api_bufptr->API_More_Data_Follows = '0';
   if (API_flush_buffer( api_bufptr, tx ) != 0) {
      return( 1 );
   }

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
     myprintf("DEBUG: Leaving SCHED_show_schedule\n" );
  }

   return( 0 );
} /* SCHED_show_schedule */

/* ---------------------------------------------------
   Spawn off a subtask to run the job and wait for
   it's completion.
   We can't hang around waiting fot it to finish in
   our code as we have other things to do.

   Returns 1 (true) if the task was started, or will
   return 0 (false) if we could not start the job.
   Return 2 if no resource to start job now, try later.
   --------------------------------------------------- */
int  SCHED_spawn_job( active_queue_def * datarec ) {
   extern currently_running_jobs_def running_job_table;
   long job_recordnum, active_recordnum;
   pid_t pid_number;
   time_t time_number = 0;
   jobsfile_def local_rec;   /* not a pointer, can't risk
                                mallocing as child would get
                                buffer also */
   joblog_def local_logrec;  /* not a pointer, can't risk
                                mallocing as child would get
                                buffer also */
   alertsfile_def local_alert; /* not a pointer, can't risk
                                mallocing as child would get
                                buffer also */
   int junk;
   char commandline[256];
   int run_blocked;
   char *ptr;
   uid_t newuid;
   char *argv[4];

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf("DEBUG: Entered SCHED_spawn_job\n" );
   }

   /* Special handling for NULL* jobs, these can only run if no
	* other jobs are running at the same time */
   if (
         (strncmp(datarec->job_header.jobname,"NULL",4) == 0) &&
         (running_job_table.number_of_jobs_running > 0)
	  )
   {
	   /* bump up the active queue def runtime by 5 minutes FROM NOW and requeue */
       if ( (active_recordnum = SCHED_ACTIVE_read_record( datarec )) >= 0) {
          datarec->run_timestamp = time(0);
          datarec->run_timestamp = datarec->run_timestamp + (60 * 5);
		  datarec->hold_flag = 'R';    /* requeue advise for displays */
          active_recordnum = SCHED_ACTIVE_write_record( datarec, active_recordnum );
          if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_VARS) {
             myprintf( "DEBUG: Setting SCHED_init_required = 1 to requeue NULL* job (SCHED_spawn_job)" ); 
          }
	      SCHED_init_required = 1;
          if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
		     myprintf( "INFO: LI007-Job required exclusive access but other jobs are running, requeued +5mins %s\n",
                      datarec->job_header.jobname );
		  }
	   }
       if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
           myprintf("DEBUG: Leaving SCHED_spawn_job with cc=2, null job needs exlusive control, try later\n" );
       }
	   return( 2 );
   }

   /* Ensure we have spare job slots */
   if (running_job_table.number_of_jobs_running >= MAX_RUNNING_JOBS) {
           if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
              myprintf("DEBUG: Leaving SCHED_spawn_job with cc=2, max jobs, try later\n" );
           }
		   return( 2 );   /* job not started, too many jobs are running */
   }

   /*
    * Ensure we have a good active record definition.
    */
   active_recordnum = SCHED_ACTIVE_read_record( datarec );
   if (active_recordnum == -1) {
      UTILS_set_message( "Unable to access active jobs file, job not scheduled on" );
	  /* if there was an error we probably don't have a good record apart from
	   * jobname to format an alert on so fudge it.
	   * Notes: if we were called normally we should have, if we were called
	   * from a runnow we will only have the jobname, assume only jobname is
	   * correct.
	   */ 
      strncpy( local_logrec.job_header.jobnumber, datarec->job_header.jobnumber, JOB_NUMBER_LEN );
      strncpy( local_logrec.job_header.jobname, datarec->job_header.jobname, JOB_NAME_LEN );
      memcpy( local_logrec.msgid, "0000000000", JOB_LOGMSGID_LEN );
      memcpy( local_logrec.status_code, "000", JOB_STATUSCODE_LEN );
      local_alert.severity = '3'; /* page support */
      local_alert.acknowledged = 'N';
      memcpy( local_alert.failure_code, ALERTS_SCHED_QUEUEREAD_ERR, JOB_STATUSCODE_LEN );
      snprintf( local_logrec.text, JOB_LOGMSG_LEN, "JOB %s: ACTIVE QUEUE FILE ERROR, SCHEDULING SUSPENDED !", datarec->job_header.jobname );
      memcpy( (char *) &local_alert.job_details, (char *)&local_logrec, sizeof(joblog_def) );
      junk = ALERTS_write_alert_record( &local_alert, -1 );
	  /* return no job started */
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
         myprintf("DEBUG: Leaving SCHED_spawn_job with cc=0, cannot read active queue entry\n" );
      }
      return( 0 );
   }


   /*
    * start formatting our log record.
    */
   strncpy( local_logrec.job_header.jobnumber, datarec->job_header.jobnumber, JOB_NUMBER_LEN );
   strncpy( local_logrec.job_header.jobname, datarec->job_header.jobname, JOB_NAME_LEN );
   memcpy( local_logrec.msgid, "0000000000", JOB_LOGMSGID_LEN );
   memcpy( local_logrec.status_code, "000", JOB_STATUSCODE_LEN );
   snprintf( local_logrec.text, JOB_LOGMSG_LEN, "JOB %s started\n", datarec->job_header.jobname );
   /*
    * and in case its needed our alert record
    */
   local_alert.severity = '3'; /* page support */
   memcpy( local_alert.failure_code, ALERTS_NO_ERROR, JOB_STATUSCODE_LEN );
   local_alert.acknowledged = 'N';


   /*
    * Read the job definition information.
    */
   strncpy( local_rec.job_header.jobnumber, datarec->job_header.jobnumber, JOB_NUMBER_LEN );
   strncpy( local_rec.job_header.jobname, datarec->job_header.jobname, JOB_NAME_LEN );
   job_recordnum = JOBS_read_record( &local_rec );
   if (job_recordnum == -1) {
      snprintf( local_logrec.text, JOB_LOGMSG_LEN, "JOB %s: UNABLE TO READ JOB RECORD", datarec->job_header.jobname );
      memcpy( (char *) &local_alert.job_details, (char *)&local_logrec, sizeof(local_logrec) );
      memcpy( local_alert.failure_code, ALERTS_JOB_FILEREAD_ERR, JOB_STATUSCODE_LEN );
      junk = ALERTS_write_alert_record( &local_alert, -1 );
      UTILS_set_message( "Unable to read job record" );
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
         myprintf("DEBUG: Leaving SCHED_spawn_job with cc=0, cannot read job record\n" );
      }
      return( 0 );
   }


   /*
    * Adjust the active queue record accordingly 
    */
/*   junk = SCHED_DEPEND_delete_record( (dependency_queue_def *) datarec );  should be none left but clear anyway */
   datarec->current_status = '2';  /* running */
   UTILS_datestamp( (char *)&datarec->started_running_at, time(0) ); /* record time started */
   if (SCHED_ACTIVE_write_record( datarec, active_recordnum ) == -1) {
      snprintf( local_logrec.text, JOB_LOGMSG_LEN, "JOB %s: NOT RUN, UNABLE TO WRITE TO ACTIVE JOB QUEUE", datarec->job_header.jobname );
      memcpy( (char *)&local_alert.job_details, (char *)&local_logrec, sizeof(local_logrec) );
      memcpy( local_alert.failure_code, ALERTS_SCHED_QUEUEWRITE_ERR, JOB_STATUSCODE_LEN );
      junk = ALERTS_write_alert_record( &local_alert, -1 );
      myprintf( "*ERR: LE018-Unable to access active jobs file, job %s not scheduled on\n", datarec->job_header.jobname );
      UTILS_set_message( "Unable to access active jobs file, job not scheduled on" );
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
         myprintf("DEBUG: Leaving SCHED_spawn_job with cc=0, update of active queue failed\n" );
      }
      return( 0 );
   }

   /* 
    * This is a sanity check in case a user manually submitted a job that
    * should not be scheduled (our automatic daily job should check this but a
    * manual submission is not checked at submission time yet.
    */
   run_blocked = JOBS_schedule_calendar_check( (jobsfile_def *) &local_rec, time_number );
   if (run_blocked != 0) {
      snprintf( local_logrec.text, JOB_LOGMSG_LEN, "JOB %s: NOT RUN, EXCLUDED BY CALENDAR", datarec->job_header.jobname );
      memcpy( (char *)&local_alert.job_details, (char *)&local_logrec, sizeof(local_logrec) );
      memcpy( local_alert.failure_code, ALERTS_CALENDAR_BLOCK, JOB_STATUSCODE_LEN );
      junk = ALERTS_write_alert_record( &local_alert, -1 );
      UTILS_set_message( "Job blocked by calendar exclusion dates" );
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
         myprintf( "INFO: LI008-Job %s not run, prevented by calendar checks\n", datarec->job_header.jobname );
	  }
      junk = SCHED_ACTIVE_delete_record( datarec, 1 /* 1 = internally called, we must delete it */ );
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
         myprintf("DEBUG: Leaving SCHED_spawn_job with cc=0, blocked by calendar\n" );
      }
      return( 0 );
   }

   /*
    * Make sure the user is still on the system
    */
   UTILS_remove_trailing_spaces( (char *)&local_rec.job_owner );
   newuid = UTILS_obtain_uid_from_name( (char *)&local_rec.job_owner ); 
   if (newuid < 0) {
      snprintf( local_logrec.text, JOB_LOGMSG_LEN, "JOB %s: NOT RUN, USER %s IS NO LONGER ON THE SYSTEM",
			    datarec->job_header.jobname,
			    local_rec.job_owner
			  );
      memcpy( (char *)&local_alert.job_details, (char *)&local_logrec, sizeof(local_logrec) );
      memcpy( local_alert.failure_code, ALERTS_USERNOTFOUND, JOB_STATUSCODE_LEN );
      junk = ALERTS_write_alert_record( &local_alert, -1 );
      UTILS_set_message( "Job owner has been deleted from the system !" );
      myprintf( "*ERR: LE019-Job %s not run, user %s no longer exists\n", datarec->job_header.jobname, local_rec.job_owner );
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
         myprintf("DEBUG: Leaving SCHED_spawn_job with cc=0, user %s not found\n", local_rec.job_owner );
      }
	  return( 0 );
   }
   
   /*
    * Spawn a copy of ourself to run the task.
    */
   pid_number = fork( );
   if (pid_number == -1) {
      /* failed to fork */
      snprintf( local_logrec.text, JOB_LOGMSG_LEN, "JOB %s: UNABLE TO FORK. JOB NOT STARTED", datarec->job_header.jobname );
      memcpy( (char *)&local_alert.job_details, (char *)&local_logrec, sizeof(local_logrec) );
      memcpy( local_alert.failure_code, ALERTS_SCHED_FORK_ERR, JOB_STATUSCODE_LEN );
      junk = ALERTS_write_alert_record( &local_alert, -1 );
      UTILS_format_message( "Unable to fork a child process, errno ", errno );
      datarec->current_status = '3';  /* failed */
      snprintf( datarec->failure_code, JOB_STATUSCODE_LEN, "%d", errno );
      junk = SCHED_ACTIVE_write_record( datarec, active_recordnum );
	  myprintf( "*ERR: LE020-Unable to fork a new process to run the job\n" );
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
         myprintf("DEBUG: Leaving SCHED_spawn_job with cc=0, fork failed\n" );
      }
      return( 0 );
   }

   /* We are the parent if pid_number is not 0, so update the running job table,
	* then we gave finished */
   if (pid_number != 0) {
	  memcpy( running_job_table.running_jobs[running_job_table.number_of_jobs_running].job_header.jobnumber,
	          datarec->job_header.jobnumber, sizeof(job_header_def) );
	  running_job_table.running_jobs[running_job_table.number_of_jobs_running].job_pid = pid_number;
      running_job_table.number_of_jobs_running++; 
      myprintf( "JOB-: LI009-Forked PID %d for job %s\n", pid_number, datarec->job_header.jobname );
      if (strncmp(local_rec.job_header.jobname,"NULL",4) == 0) {
	     running_job_table.exclusive_job_running = 1;    /* don't let other jobs start yet */
         if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
	        myprintf( "INFO: LI010-JOB %s, now has excusive exection, other jobs will wait\n", local_rec.job_header.jobname );
		 }
      }
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
         myprintf("DEBUG: Leaving SCHED_spawn_job\n" );
      }
      return( 1 );  /* the task has started, our work is done */
   }


   /* Otherwise, we are the child process so have a bit more work to do now */

   /*
    * Change UID of our child process now
    */
   junk = setuid( newuid );
   if (junk != 0) {
      _exit( 99 );  /* specific error for uid problem */
   }

   /*
    * Run the job
    */
   UTILS_remove_trailing_spaces( (char *)&local_rec.job_header.jobname );
   /* need this field sanity check for some reason
    * fields didn't seem to be getting here null terminated earlier, so do so 
    * (note: triggers a source and destination fields overlap in valgrind, is OK) */
   strncpy( local_rec.program_to_execute, local_rec.program_to_execute, JOB_EXECPROG_LEN );
   strncpy( local_rec.program_parameters, local_rec.program_parameters, JOB_PARM_LEN );

   /* Any special PARM fields to override ? */
   ptr = (char *)&local_rec.program_parameters;
   while ( (*ptr != '~') && (*ptr != '\0') ) { ptr++; };
   if (*ptr == '~') {
      /* Replace the date with the supposed to have run on date */
      if (strncmp(ptr,"~~DATE~~",8) == 0) {
         strncpy(ptr,local_rec.next_scheduled_runtime,8);  /* use date in record to overlay ~~DATE~~ */
      }
   }

   /* Jobs of class NULL* write output to /dev/null rather than a log file */
   /* MID:2003/06/02 cat /dev/null to all commands. This has been added to
	* prevent jobs that prompt for input from hanging. */
   if (strncmp(local_rec.job_header.jobname,"NULL",4) == 0) {
      snprintf( commandline, 255, "cat /dev/null | %s %s >> /dev/null 2>&1",
                local_rec.program_to_execute, local_rec.program_parameters );
   }
   else {
      snprintf( commandline, 255, "cat /dev/null | %s %s >> joblogs/%s.log 2>&1",
                local_rec.program_to_execute, local_rec.program_parameters,
                local_rec.job_header.jobname );
   }
/*
   myprintf( "INFO: SYSTEM CALL PARMS = %s\n", commandline );
   junk = system( commandline );
*/
   argv[0] = "/bin/bash";
   argv[1] = "-c";
   argv[2] = (char *)&commandline;
   argv[3] = 0;
   execve( "/bin/bash", argv, environ );
   exit( 127 );
} /* SCHED_spawn_job */

/* -------------------------------------------------------
   SCHED_count_active
   Returns the number of jobs in the active jobs file or
   will return -1 on an error.
   ------------------------------------------------------- */
int  SCHED_count_active( void ) {
   int  recordcounter;
   int  lasterror;
   active_queue_def * local_rec;
   
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf("DEBUG: Entered SCHED_count_active\n" );
   }

   /*
    * read through each record in the active job file and count them.
    */
   recordcounter = 0;   
   lasterror = fseek( active_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      myprintf( "*ERR: LE021-Seek to start of active jobs file failed\n" );
      UTILS_set_message( "Seek to start of active jobs file failed" );
      return( -1 );
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG:Seek to start of active queue file\n" );
   }

   if ( (local_rec = (active_queue_def *)MEMORY_malloc(sizeof(active_queue_def),"SCHED_count_active") ) == NULL) {
      myprintf( "*ERR: LE022-Unable to allocate %d bytes of memory for record buffer.\n",sizeof(active_queue_def) );
      UTILS_set_message( "Unable to allocate memory for active job read" );
      return ( -1 );
   }

   while ( (!(feof(active_handle))) && (!ferror(active_handle)) ) {
      lasterror = fread( local_rec, sizeof(active_queue_def), 1, active_handle  );
      if (ferror(active_handle)) {
         perror( "active_handle" );
         myprintf( "*ERR: LE023-Read error occurred on active jobs file.\n" );
         UTILS_set_message( "Read error on active jobs file" );
         MEMORY_free( local_rec );
         return( -1 );
      }
	  if (!feof(active_handle)) {
		 /* count only if not Deleted or not System */
		 if ( (local_rec->job_header.info_flag != 'D') && (local_rec->job_header.info_flag != 'S') ) {
            recordcounter++;
		 }
	  }
   } /* end while not eof */

   MEMORY_free( local_rec );
   
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf("DEBUG: Leaving SCHED_count_active\n" );
   }
   return( recordcounter );
} /* SCHED_count_active */


/* -------------------------------------------------------
   SCHED_internal_add_some_dependencies

   This is called from the main server by the newday 
   procedure. It will be called if jobs are still on
   the active queue, which will prevent it from running.
   In order to avoid the newday job having to go into
   a failed state in this situation this routine will
   add a dependency record onto the newday job.

   The effects of this will be
   * the newday job will not have to be alerted/manually requeued
     if jobs are still waiting to run at the newday time
   * it will run automatically as soon as the last job holding
     it up completes

   It has pen put in the scheduler library simply because
   I like to keep anything that does a lot of IO on the 
   active queue file in this one library.
   ------------------------------------------------------- */
int SCHED_internal_add_some_dependencies( active_queue_def * active_entry, long recordnum ) {
   int  lasterror, i, itemsread;
   long dependency_recnum;
   active_queue_def * local_rec;
   dependency_queue_def *local_dependency_rec;
   
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf("DEBUG: Entered SCHED_internal_add_some_dependencies\n" );
   }

   /* seek to the start of the active job queue file */
   lasterror = fseek( active_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      myprintf( "*ERR: LE024-Seek to start of active jobs file failed (SCHED_internal_add_some_dependencies)\n" );
      UTILS_set_message( "Seek to start of active jobs file failed" );
      return( 0 );
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG: Seek to start of active queue file\n" );
   }

   if ( (local_rec = (active_queue_def *)MEMORY_malloc(sizeof(active_queue_def),"SCHED_internal_add_some_dependencies") ) == NULL) {
      myprintf( "*ERR: LE025-Unable to allocate memory for active job read (SCHED_internal_add_some_dependencies)\n" );
      UTILS_set_message( "Unable to allocate memory for active job read" );
      return( 0 );
   }

   while ( (!(feof(active_handle))) && (!ferror(active_handle)) ) {
      itemsread = fread( local_rec, sizeof(active_queue_def), 1, active_handle  );
      if (ferror(active_handle)) {
         perror( "active_handle" );
         myprintf( "*ERR: LE026-Read error occurred on active jobs file (SCHED_internal_add_some_dependencies).\n" );
         MEMORY_free( local_rec );
         UTILS_set_message( "Read error on active jobs file" );
         return( 0 );
      }
	  if ( (!feof(active_handle)) && (itemsread > 0) ) {
		 /* count only if not Deleted, not System, and not the trigering job */
		 if ( (local_rec->job_header.info_flag != 'D') && (local_rec->job_header.info_flag != 'S') &&
              (strncmp(active_entry->job_header.jobname,local_rec->job_header.jobname,JOB_NAME_LEN) != 0)
	 	    )
		 {
				 /* if executing, failed or timewait its a candidate */
				 if ( (local_rec->current_status == '2') || (local_rec->current_status == '3') ||
				      ( local_rec->current_status == '0') || (local_rec->current_status == '1')
					)
				 {
                    /* --- create the dependency record first --- */
                    if ((local_dependency_rec = (dependency_queue_def *)MEMORY_malloc( sizeof( dependency_queue_def ),
                                                "SCHED_internal_add_some_dependencies" )) == NULL) {
		                  myprintf( "*ERR: LE027-Unable to allocate memory required for dependency storage\n" );
				          UTILS_set_message( "Unable to allocate memory required for dependency storage" );
						  MEMORY_free( local_rec );
						  return( 0 );
                    }

					/* ...try to read any existing dpendency record first */
                    memcpy( local_dependency_rec->job_header.jobnumber,
				            active_entry->job_header.jobnumber,
                            sizeof( job_header_def ) );
					dependency_recnum = SCHED_DEPEND_read_record( local_dependency_rec );
					/* if we can't read one, there just isnt one which is ok */
					if (dependency_recnum < 0L) { dependency_recnum = -1; };
                    /* ... and re-initialise it the way we want */
                    memcpy( local_dependency_rec->job_header.jobnumber,
				            active_entry->job_header.jobnumber,
                            sizeof( job_header_def ) );
                    for (i = 0; i < MAX_DEPENDENCIES; i++) {
							local_dependency_rec->dependencies[i].dependency_type = '0';
					}
                    local_dependency_rec->dependencies[0].dependency_type = '1'; /* job wait */
                    UTILS_strncpy( local_dependency_rec->dependencies[0].dependency_name,
                             local_rec->job_header.jobname, JOB_NAME_LEN );
                    UTILS_space_fill_string( (char *)&local_dependency_rec->dependencies[0].dependency_name,
                                             JOB_DEPENDTARGET_LEN );
/*	MID				if (SCHED_DEPEND_update_record( local_dependency_rec, dependency_recnum ) < 0) {  
 *	The update doesn't work, write does ??? */
					if (SCHED_DEPEND_write_record( local_dependency_rec, dependency_recnum ) < 0) { 
							myprintf( "*ERR: LE028-Unable to write dependency record (SCHED_internal_add_some_dependencies)\n" );
							MEMORY_free( local_dependency_rec );
							MEMORY_free( local_rec );
							return( 0 );
					}
                    if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
                       myprintf( "INFO: LI011-REQUEUE - Dependency record for job %s created, value %s\n",
                               local_dependency_rec->job_header.jobname,
                               local_dependency_rec->dependencies[0].dependency_name );
					}
                    MEMORY_free( local_dependency_rec );
                    /* --- then update the job flag to dependency wait --- */
			        active_entry->current_status = '1'; /* dependency wait */
                    if ( SCHED_ACTIVE_write_record( active_entry, recordnum ) < 0 ) {
							myprintf( "*ERR: LE029-Unable to set job %s to dependency wait state\n",
                                    active_entry->job_header.jobname );
							MEMORY_free( local_rec );
							return( 0 );
					}
                    if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
					       myprintf( "INFO: LI012-REQUEUE - Dependency wait flag set for job %s\n",
                                   active_entry->job_header.jobname );
					}
                    MEMORY_free(local_rec);
                    if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
                       myprintf("DEBUG: Leaving SCHED_internal_add_some_dependencies\n" );
                    }
                    return( 1 );
				 }
		 }
	  }
   } /* end while not eof */

   MEMORY_free( local_rec );

   /* if we get here we didn't find any active entries ? */

   /* have found an issue with system generated alerts here, they may not be
	* directly related to jobs. So we need to check for alerts and manage
	* appropriately. */
   myprintf( "*ERR: LE030-Unable to find any jobs to add as a dependency ? (SCHED_INTERNAL_add_some_dependencies)\n" );
   myprintf( "*ERR: LE030-...CHECKING ALERT QUEUE\n" );
   if (ALERTS_count_alerts() > 0) {
		   myprintf( "*ERR: LE031...There are alerts on the alert queue, please investigate\n" );
   }
   else {
		   /* Should not be possible, but allow for */
		   myprintf( "*ERR: LE032-...Unable to find any alerts either; investigate, scheduler may need 'bouncing'\n" );
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf("DEBUG: Leaving SCHED_internal_add_some_dependencies\n" );
   }
   return( 0 );
} /* SCHED_internal_add_some_dependencies */


/* **********************************************************
   **********************************************************

   Active job file management routines.

   **********************************************************
   ********************************************************** */

/* ---------------------------------------------------
   Returns the next job on the queue to be scheduled
   to start running, also returns 1 if a job is available
   to run.

   If there are no jobs available to run return -1.
   --------------------------------------------------- */
long SCHED_ACTIVE_get_next_jobtorun( active_queue_def * datarec ) {
   int  lasterror;
   active_queue_def *local_rec;
   time_t compare_date;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf("DEBUG: Entering SCHED_ACTIVE_get_next_job_to_run\n" );
   }

  /* seek to the start of the active job file, and read through
     each of the jobs waiting to run. We return the information
     for the next job scheduled to run. */
   lasterror = fseek( active_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      myprintf( "*ERR: LE033-Seek to start of active job file failed\n" );
      return( -1 );
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_IO) {
		   myprintf("DEBUG: Seek to start of active queue file\n" );
   }

   if ((local_rec = (active_queue_def *)MEMORY_malloc( sizeof( active_queue_def ),"SCHED_ACTIVE_get_next_jobtorun" )) == NULL) {
      myprintf( "*ERR: LE034-Unable to allocate memory required for read operation (SCHED_ACTIVE_get_next_job_to_run)\n" );
      return( -1 );
   }

   /* Date checks to be done against the run timestamp as this is the number
	* incremented when jobs need to be delayed for some reason */
   compare_date = 0;
   /* check every record on the active job queue now */
   while (!(feof(active_handle))) {
      lasterror = fread( local_rec, sizeof(active_queue_def), 1, active_handle  );
      if (ferror(active_handle)) {
         perror( "active_handle" );
         myprintf( "*ERR: LE035-Read error occurred on active jobs file.\n" );
         MEMORY_free( local_rec );
         return( -1 );
      }
      /* compare it against most recent found */
      if (
              ( ( local_rec->run_timestamp < compare_date ) || ( compare_date == 0 ) ) &&
			  (local_rec->current_status == '0') &&        /* time wait */
			  (local_rec->hold_flag != 'Y') &&             /* not held by operator */
			  (local_rec->hold_flag != 'C') &&             /* not held due to calendar error */
			  (local_rec->job_header.info_flag != 'D')     /* not already completed/deleted */
		 ) {
         /* if the scheduled run-time is less than the last lowest
            next runtime copy this record to the callers buffer */
         memcpy( datarec, local_rec, sizeof( active_queue_def ) );
		 compare_date = local_rec->run_timestamp;
      }
   }
   MEMORY_free( local_rec );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf("DEBUG: About to leave SCHED_ACTIVE_get_next_job_to_run (one possible info msg to throw)\n" );
   }
   if (compare_date == 0) {
      /* no job found */
      return( 0 );
   }
   else {
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
         myprintf( "INFO: LI013-Job %s moved to top of run queue at %s\n", datarec->job_header.jobname,
		      	 datarec->next_scheduled_runtime );
	  }
      return ( 1 );
   }
} /* SCHED_ACTIVE_get_next_jobtorun */


/* ---------------------------------------------------
   --------------------------------------------------- */
long SCHED_ACTIVE_read_record( active_queue_def * datarec ) {
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf("DEBUG: Entered SCHED_ACTIVE_read_record, and leaving (two calls)\n" );
   }
   /* MID: Ensure the jobname is always space padded */
   UTILS_space_fill_string( (char *)&datarec->job_header.jobname, JOB_NAME_LEN );
   return( UTILS_read_record( active_handle, (char *) datarec,
                              sizeof(active_queue_def),
                              "Active jobs file","SCHED_ACTIVE_read_record", 0 ) );
}

/* ---------------------------------------------------
   SCHED_ACTIVE_write_record
   Returns -1 if an error.
   --------------------------------------------------- */
long SCHED_ACTIVE_write_record( active_queue_def * datarec, long recordnum ) {
		/* DEBUG */
    if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
       myprintf( "DEBUG: Entered SCHED_ACTIVE_write_record, job %s, reclen %d, and leaving (two calls)\n",
          		datarec->job_header.jobname,
		    	sizeof( active_queue_def )
		     );
	}
	/* Ensure we have the full length for the job name */
	UTILS_space_fill_string( (char *)&datarec->job_header.jobname, JOB_NAME_LEN );
	/* return the result from the common write procedure */
   return( UTILS_write_record( active_handle, (char *) datarec, sizeof(active_queue_def),
                               recordnum, "active_handle" )
         );
} /* SCHED_ACTIVE_write_record */


/* ---------------------------------------------------
   SCHED_ACTIVE_delete_record
   Returns -1 if an error.
   --------------------------------------------------- */
long SCHED_ACTIVE_delete_record( active_queue_def * datarec, int internal ) {
   long recordnum;
   dependency_queue_def * dep_rec;

    if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
       myprintf( "DEBUG: Entered SCHED_ACTIVE_delete_record, job %s, internal flag=%d\n",
          		datarec->job_header.jobname,
		    	internal
		     );
	}

   recordnum = SCHED_ACTIVE_read_record( datarec );
   if (recordnum < 0) {
      return( -1 );
   }

   if (
         (datarec->current_status == '2') &&     /* executing */
		 (internal == 0)                         /* not internally (job completion) called */
      ) {
      UTILS_set_message( "Delete request ignored, job is executing" );
      myprintf( "*ERR: LE036-Cannot delete %s, this job is now executing\n", datarec->job_header.jobname );
      return( -1 );  
   }

   datarec->job_header.info_flag = 'D';  /* set to deleted state */
   recordnum = SCHED_ACTIVE_write_record( datarec, recordnum );
   if (recordnum == -1) {
      myprintf( "*ERR: LE037-UNABLE TO DELETE JOB FROM ACTIVE QUEUE, LOOPING POTENTIAL FOR %s\n",
                datarec->job_header.jobname );
      return( -1 );
   }
   
   /* Attempt to delete any alerts that exist for the job, if we get
    * an error ignore it, it hopefully just means there were no alerts
    * for this job in the file. */
   recordnum = ALERTS_delete_alert( (alertsfile_def *)datarec );
  
   /* Pass the job being deleted to the remove dependencies routine, if
    * any jobs in the dependency queue are waiting on this job we will
    * remove the dependencies for this job. */
   SCHED_DEPEND_delete_all_relying_on_job( (job_header_def *)datarec );

   /* If its in the dependencies file delete from there. */
   if ( (dep_rec = (dependency_queue_def *)MEMORY_malloc(sizeof(dependency_queue_def),"SCHED_ACTIVE_delete_record")) != NULL) {
      memcpy( dep_rec->job_header.jobnumber, datarec->job_header.jobnumber, sizeof(job_header_def) );
      if (SCHED_DEPEND_delete_record( dep_rec ) == -1) {
         /* not found so don't worry about it */
      }
	  MEMORY_free( dep_rec );
   }
   else {
      myprintf( "*ERR: LE038-Unable to malloc %d bytes, dependency record not checked for %s (SCHED_ACTIVE_delete_record)\n",
                sizeof(dependency_queue_def), datarec->job_header.jobname );
   }

   /* note: we do not compress the job file. at this time I like the
	* command interface to be able to display deleted jobs and what state
	* they were in prior to the delete; I find it usefull */

   /* all OK */
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving SCHED_ACTIVE_delete_record\n" );
   }
   return( 0 );
} /* SCHED_ACTIVE_delete_record */


/* **********************************************************
   **********************************************************

   Dependency file management routines.

   **********************************************************
   ********************************************************** */

/* ---------------------------------------------------
   SCHED_DEPEND_write_record
   Returns -1 if an error.
   --------------------------------------------------- */
long SCHED_DEPEND_write_record( dependency_queue_def * datarec, long recordnum ) {
   int junk;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entering SCHED_DEPEND_write_record, and leaving (two calls)\n" );
   }

   /* MID:Somewhere the dependency field is being truncated,
    * dunno where, but we better fix it here ! */
   junk = BULLETPROOF_dependency_record( datarec );
   return( UTILS_write_record( dependency_handle, (char *) datarec, sizeof(dependency_queue_def),
                               recordnum, "dependency_handle" )
         );
} /* SCHED_DEPEND_write_record */

/* ---------------------------------------------------
   --------------------------------------------------- */
long SCHED_DEPEND_read_record( dependency_queue_def * datarec ) {
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entering SCHED_DEPEND_read_record, and leaving (one call)\n" );
   }
   return( UTILS_read_record( dependency_handle, (char *) datarec, sizeof(dependency_queue_def),
                              "dependency_handle","SCHED_DEPEND_read_record", 0 ) 
         );
} /* SCHED_DEPEND_read_record */

/* ---------------------------------------------------
   --------------------------------------------------- */
long SCHED_DEPEND_delete_record( dependency_queue_def * datarec ) {
   long recordnum;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entering SCHED_DEPEND_delete_record\n" );
   }

   recordnum = SCHED_DEPEND_read_record( datarec );
   if (recordnum < 0) {
      return( -1 );
   }
   datarec->job_header.info_flag = 'D';  /* set to deleted state */
   recordnum = SCHED_DEPEND_write_record( datarec, recordnum );
   if (recordnum == -1) {
      return( -1 );
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving SCHED_DEPEND_delete_record, all OK\n" );
   }
   return( 0 ); /* All OK */
} /* SCHED_DEPEND_delete_record */

/* ---------------------------------------------------
   --------------------------------------------------- */
long SCHED_DEPEND_update_record( dependency_queue_def * datarec, long dependencynum ) {
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entering SCHED_DEPEND_read_update, and leaving (one call)\n" );
   }
   return( UTILS_write_record( dependency_handle, (char *) datarec, sizeof(datarec),
                               dependencynum, "dependency_handle" )
         );
} /* SCHED_DEPEND_update_record */

/* ---------------------------------------------------
 * Scan all the dependency records and see if any contain
 * entries for the jobname passed; if they have mark the
 * dependency as satisfied.
 * If we adjusted any entries call the routine to
 * check to see if any jobs have all dependencies
 * satisfied yet.
   --------------------------------------------------- */
void SCHED_DEPEND_delete_all_relying_on_job( job_header_def * job_identifier ) {
   char local_name[JOB_DEPENDTARGET_LEN+1]; 

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entering SCHED_DEPEND_delete_all_relying_on_job\n" );
   }

   strncpy( local_name, job_identifier->jobname, JOB_DEPENDTARGET_LEN );
   UTILS_space_fill_string( (char *)&local_name, JOB_DEPENDTARGET_LEN );
   UTILS_uppercase_string( (char *)&local_name );  /* Jobnames always uppercase */
   SCHED_DEPEND_delete_all_relying_on_dependency( (char *)&local_name );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving SCHED_DEPEND_delete_all_relying_on_job\n" );
   }
} /* SCHED_DEPEND_delete_all_relying_on_job */

/* ---------------------------------------------------
 * This is used to alter a job from dependency wait
 * to time wait, overriding any dependency records
 * that may exist. If dependency records do exist
 * the will be deleted.
   --------------------------------------------------- */
void SCHED_DEPEND_freeall_for_job( job_header_def * job_identifier ) {
   long rec_num;
   int mem_needed;
   active_queue_def * local_rec;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered SCHED_DEPEND_freeall_for_job\n" );
   }

   /* there is the potential that this buffer will be used to read records
	* from both the active queue file and the dependency queue file, so
	* make it large enough. */
   if ( (sizeof(active_queue_def)) > (sizeof(dependency_queue_def)) ) {
		   mem_needed = sizeof(active_queue_def);
   }
   else {
		   mem_needed = sizeof(dependency_queue_def);
   }
   if ( (local_rec = (active_queue_def *)MEMORY_malloc(mem_needed,"SCHED_DEPEND_freeall_for_job")) == NULL ) {
		   myprintf( "*ERR: LE039-Unable to allocate memory for record buffer (SCHED_DEPEND_freeall_for_job)\n" );
		   return;
   }

   /* Delete the dependency record as it is no longer required */
   memcpy( local_rec->job_header.jobnumber, job_identifier->jobnumber, sizeof( job_header_def ) );
   rec_num = SCHED_DEPEND_delete_record( (dependency_queue_def *)local_rec );
   /* note: we deliberately ignore errors. The objective here is to
	* get the job on the active queue into time wait rather than
	* dependency wait, which we can do even if the dependency
	* record can't be cleared. The newday job will clean up the
	* dependency file for us. */

   /* now update the active queue records from dependency wait state
    * to time wait state. */
    memcpy( local_rec->job_header.jobnumber, job_identifier->jobnumber,
					sizeof(job_header_def) );  /* we need to identify what we are updating */
    rec_num = SCHED_ACTIVE_read_record( local_rec );
    if (rec_num < 0) {
       myprintf( "*ERR: LE040-Unable to read active queue record for job %s (SCHED_DEPEND_freeall_for_job)\n",
               local_rec->job_header.jobname );
    }
	else {
       if (local_rec->current_status == '1') {  /* in dependency wait */
	      local_rec->current_status = '0';  /* switch to time wait now */
          rec_num = SCHED_ACTIVE_write_record( local_rec, rec_num );
	      if (rec_num < 0) {
		    	myprintf( "*ERR: LE041-Unable to update active queue record for job %s (SCHED_DEPEND_freeall_for_job)\n",
			    		local_rec->job_header.jobname );
	      }
	      else {
                if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_VARS) {
                   myprintf( "DEBUG: Setting SCHED_init_required = 1 (SCHED_DEPEND_freeall_for_job)" ); 
                }
                SCHED_init_required = 1; /* server to recheck jobs in time_wait status */
	      }
	   }
	   else {
			   myprintf( "WARN: LW002-Job %s is not in dependency wait, not altered (SCHED_DEPEND_freeall_for_job)\n",
                       local_rec->job_header.jobname );
	   }
	}

	MEMORY_free( local_rec );

    if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
       myprintf( "DEBUG: Leaving SCHED_DEPEND_freeall_for_job\n" );
    }
	return;
} /* SCHED_DEPEND_freeall_for_job */

/* ---------------------------------------------------
 * Check all the records in the dependency file, if any
 * records are now clear then alter the associated waiting
 * job on the active queue to be runable, and delete the
 * dependency record. 
   --------------------------------------------------- */
void SCHED_DEPEND_check_job_dependencies( void ) {
   dependency_queue_def * local_rec;
   active_queue_def * local_active_rec;
   long rec_num;
   int  i, dependencies_left, datacount;

    if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
       myprintf( "DEBUG: Entered SCHED_DEPEND_check_job_dependencies\n" );
    }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_IO) {
      SCHED_DEBUG_stat_dependency_file();
   }

   if ( (local_rec = (dependency_queue_def *)MEMORY_malloc(sizeof(dependency_queue_def),"SCHED_DEPEND_check_job_dependencies")) == NULL ) {
		   myprintf( "*ERR: LE042-SCHED_depend_check_job_dependencies, insufficient free memory to malloc %d bytes\n",
				   sizeof(dependency_queue_def)
			     );
		   myprintf( "*ERR: LE042-...unable to check dependency completion.\n" );
		   return;
   }
   if ( (local_active_rec = (active_queue_def *)MEMORY_malloc(sizeof(active_queue_def),"SCHED_DEPEND_check_job_dependencies")) == NULL ) {
           MEMORY_free( local_rec );
		   myprintf( "*ERR: LE042-SCHED_depend_check_job_dependencies, insufficient free memory to malloc %d bytes\n",
				   sizeof(active_queue_def)
			     );
		   myprintf( "*ERR: LE042-...unable to check dependency completion.\n" );
		   return;
   }

   fseek( dependency_handle, 0L, SEEK_SET );
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_IO) {
		   myprintf( "DEBUG: Seek to start of dependency file (SCHED_DEPEND_check_job_dependencies)\n");
   }
   while ( (!(feof(dependency_handle))) && (!(ferror(dependency_handle))) ) {
      datacount = fread( local_rec, sizeof(dependency_queue_def), 1, dependency_handle );
      if ( (!(ferror(dependency_handle))) && (datacount > 0) ) {
	     if (local_rec->job_header.info_flag == 'D') {  /* deleted record to be ignored */
		    dependencies_left = 9;                      /* say dependencies left to avoid processing */
	     }
		 else {
	        dependencies_left = 0;
            for (i = 0; i < MAX_DEPENDENCIES; i++) {
               if (local_rec->dependencies[i].dependency_type != '0') {
			      dependencies_left++;
			   }
		    }
		 }
		 if (dependencies_left == 0) {
            if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_VARS) ||
		         (pSCHEDULER_CONFIG_FLAGS.log_level > 1)
			   )
		   	{
               myprintf( "INFO: LI014-JOB %s, HAS ALL DEPENDENCIES SATISFIED\n",
							   local_rec->job_header.jobname );
			}
			
			/* Delete the dependency record as it is no longer required */
			rec_num = SCHED_DEPEND_delete_record( local_rec );
			if (rec_num < 0) {
					myprintf( "*ERR: LE043-Unable to delete dependency record for job %s, continuing\n",
							local_rec->job_header.jobname );
			        /* we will have adjusted the file pointer, set it back */
					fseek( dependency_handle, 0L, SEEK_SET );
                    if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_IO) {
		                myprintf( "DEBUG: Seek to start of dependency file (SCHED_DEPEND_check_job_dependencies)\n" );
                    }
					myprintf( "WARN: LW003-Restarting at start of file due to bad return from SCHED_DEPEND_delete_record\n" );
			}
			else {
			        /* we will have adjusted the file pointer, set it back */
					fseek( dependency_handle, (rec_num * sizeof(dependency_queue_def)), SEEK_SET );
                    if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_IO) {
		                myprintf( "DEBUG: Seek to record %d in dependency file (SCHED_DEPEND_check_job_dependencies)\n",
								(int)rec_num );
                    }
			}

            /* now update the active queue records from dependency wait state
             * to time wait state. */
			memcpy( local_active_rec->job_header.jobnumber, local_rec->job_header.jobnumber,
					sizeof(job_header_def) );  /* we need to identify what we are updating */
			rec_num = SCHED_ACTIVE_read_record( local_active_rec );
			if (rec_num < 0) {
					myprintf( "*ERR: LE044-Unable to read active queue record for job %s, CANNOT FREE JOB TO RUN\n",
							local_active_rec->job_header.jobname );
			}
			else {
			   local_active_rec->current_status = '0';  /* switch to time wait now */
		       rec_num = SCHED_ACTIVE_write_record( local_active_rec, rec_num );
			   if (rec_num < 0) {
					myprintf( "*ERR: LE055-Unable to update active queue record for job %s, CANNOT FREE JOB TO RUN\n",
							local_active_rec->job_header.jobname );
			   }
			   else {
                  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_VARS) {
                     myprintf( "DEBUG: Setting SCHED_init_required = 1 (SCHED_DEPEND_check_job_dependencies)" ); 
                  }
                  SCHED_init_required = 1; /* server to recheck jobs in time_wait status */
			   }
			}
		 }
      }
   }

   MEMORY_free( local_active_rec );
   MEMORY_free( local_rec );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_IO) {
      SCHED_DEBUG_stat_dependency_file();
   }

    if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
       myprintf( "DEBUG: Leaving SCHED_DEPEND_check_job_dependencies\n" );
    }
} /* SCHED_DEPEND_check_job_dependencies */

/* ---------------------------------------------------
 * Check all records in the dependency queue to see if any
 * are waiting on the creation of a file. If so check to
 * see if the file exists, and has not been updated in the
 * last 5 minutes. If so release the dependency.
   --------------------------------------------------- */
void SCHED_DEPEND_delete_all_relying_on_files( void ) {
   int lasterror;
   struct stat * stat_buf;
   dependency_queue_def local_rec;
   long record_number;
   int  i, changed;
   time_t time_five_minutes_ago;
		
/* stat buffer defined for LINUX
      struct stat
        {
         dev_t         st_dev;      device
         ino_t         st_ino;      inode
         mode_t        st_mode;     protection
         nlink_t       st_nlink;    number of hard links
         uid_t         st_uid;      user ID of owner
         gid_t         st_gid;      group ID of owner
         dev_t         st_rdev;     device type (if inode device)
         off_t         st_size;     total size, in bytes
	     unsigned long st_blksize;  blocksize for filesystem I/O
	     unsigned long st_blocks;   number of blocks allocated
	     time_t        st_atime;    time of last access
	     time_t        st_mtime;    time of last modification
	     time_t        st_ctime;    time of last change
	    };
*/
		
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entering SCHED_DEPEND_delete_all_relying_on_files\n" );
   }

   if ( (stat_buf = (struct stat *)MEMORY_malloc(sizeof(struct stat),"SCHED_DEPEND_delete_all_relying_on_file")) == NULL ) {
	  myprintf( "*ERR: LE056-SCHED_DEPEND_delete_all_relying_on_files, unable to allocate memory for file checks\n" );
	  return;
   }

   time_five_minutes_ago = UTILS_timenow_timestamp();
   time_five_minutes_ago = time_five_minutes_ago - (60 * 5); /* 60 seconds * 5 minutes */
   
   fseek( dependency_handle, 0L, SEEK_SET );
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_IO) {
		   myprintf("DEBUG: Seek to start of dependency queue file\n" );
   }
   record_number = 0;
   while ( (!(feof(dependency_handle))) && (!(ferror(dependency_handle))) ) {
      fread( (char *)&local_rec, sizeof(dependency_queue_def), 1, dependency_handle );
      if (!(ferror(dependency_handle))) {
	     changed = 0;
         for (i = 0; i < MAX_DEPENDENCIES; i++) {
            if (local_rec.dependencies[i].dependency_type == '2') /* file dependency */
		   	{
               UTILS_remove_trailing_spaces( (char *)&local_rec.dependencies[i].dependency_name );
		       lasterror = stat( local_rec.dependencies[i].dependency_name, stat_buf );   /* if <> 0 an error, ENOENT if not there */
			   if (lasterror == ENOENT) {
					   /* file does not exist yet, this is OK */
			   }
			   else if (lasterror == 0) {
			      /* Just because the file exists doesn't mean we can use it
			       * yet, we need to check the last modified time to see if the
			       * file is still being written to. */
                  if ( (time_five_minutes_ago > stat_buf->st_mtime) &&
                       (time_five_minutes_ago > stat_buf->st_ctime) )
				  {
			         local_rec.dependencies[i].dependency_type = '0'; /* dependency is now satisfied, OFF */
			         changed = 1; /* we have changed the record at least once */
                     myprintf( "JOB-: LI015-JOB %s, DEPENDENCY WAIT FOR FILE %s SATISFIED\n",
							   local_rec.job_header.jobname,
					           local_rec.dependencies[i].dependency_name );
				  }
				  else {  /* file exists but is still being written to */
                     if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_VARS) ||
		                 (pSCHEDULER_CONFIG_FLAGS.log_level > 1)
			            )
		   	         {
                        myprintf( "INFO: LI016-WATCHED FILE %s on system, still being written to so. Recheck in five minutes\n",
					         		   local_rec.dependencies[i].dependency_name );
			         }
				  }
			   }
			   else { /* an error */
					   if (errno != 2) {  /* should have been trapped by ENOENT but isn't, 2 = no file exists */
					      myprintf( "*ERR: LE057-Unable to stat file %s (error %d)\n",
						      	  local_rec.dependencies[i].dependency_name, errno );
					   }
			   }
			}
		 }
		 if (changed != 0) {
            if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_IO) {
		       myprintf("DEBUG: Seek to and write of record %d in dependency queue file\n", (int)record_number );
            }
		    fseek( dependency_handle, (record_number * sizeof(dependency_queue_def)), SEEK_SET );
		    fwrite( (char *)&local_rec, sizeof(dependency_queue_def), 1, dependency_handle );
		    if (!(ferror(dependency_handle))) {
			   /* seek to the next record to read */
               if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_IO) {
		          myprintf("DEBUG: Write ok, seek to record %d now, in dependency queue file\n", (int)(record_number + 1) );
               }
		       fseek( dependency_handle, ((record_number + 1) * sizeof(dependency_queue_def)), SEEK_SET );
		    }
		 }
         record_number++;
      }
   }
   
   MEMORY_free( stat_buf );

   /* See if we have freed up any job dependency records */
   SCHED_DEPEND_check_job_dependencies();

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving SCHED_DEPEND_delete_all_relying_on_files\n" );
   }
} /* SCHED_DEPEND_delete_all_relying_on_files */

/* ---------------------------------------------------
   --------------------------------------------------- */
void SCHED_DEPEND_listall_waiting_on_dep( API_Header_Def * pApi_buffer, FILE *tx, char * jobname ) {
  dependency_queue_def * local_rec;
  int i, itemsread, matchfound;
  char namefound[JOB_NAME_LEN+3];
  char local_jobname[JOB_DEPENDTARGET_LEN+2]; /* local copy, refreshed each loop, as we truncate it for job checks
												and we DON'T want to do that to the supplied name */

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: Entering SCHED_DEPEND_listall_waiting_on_dep\n" );
  }

  API_init_buffer( pApi_buffer );
  strcpy( pApi_buffer->API_System_Name, "DEP" );
  memcpy( pApi_buffer->API_Command_Response, API_RESPONSE_DISPLAY, API_RESPCODE_LEN );

  /* if we don't have a 2K buffer don't even attempt it */
  if (MAX_API_DATA_LEN < 2048) {
     myprintf( "*ERR: LE058-Data buffer is < 2K, programmer error (SCHED_DEPEND_listall_waiting_on_dep)\n");
     memcpy( pApi_buffer->API_Command_Response, API_RESPONSE_ERROR, API_RESPCODE_LEN );
     strcpy( pApi_buffer->API_Data_Buffer,
                        "*ERROR* : MAX_API_BUFFER < 2K, programmer error\n" );
     API_set_datalen( pApi_buffer, strlen(pApi_buffer->API_Data_Buffer) );     
     return;
  }
  
  if ( ( local_rec = (dependency_queue_def *)MEMORY_malloc(sizeof(dependency_queue_def),"SCHED_DEPEND_listall_waiting_on_dep") ) == NULL ) {
		   myprintf( "*ERR: LE059-Unable to malloc %d bytes (SCHED_DEPEND_listall_waiting_on_dep)\n",
				   sizeof(dependency_queue_def)
			     );
           memcpy( pApi_buffer->API_Command_Response, API_RESPONSE_ERROR, API_RESPCODE_LEN );
           strcpy( pApi_buffer->API_Data_Buffer, "*E* Unable to allocate memory for the request, see log\n" );
           API_set_datalen( pApi_buffer, strlen(pApi_buffer->API_Data_Buffer) );     
		   return;
  }

  /* seek to the start of the dependency queue */
  fseek( dependency_handle, 0 , SEEK_SET );
  clearerr( dependency_handle );

  /* read each record */
  while ( (!(feof(dependency_handle))) && (!(ferror(dependency_handle))) ) {
     matchfound = 0;
	 itemsread = fread( (char *)local_rec, sizeof(dependency_queue_def), 1, dependency_handle );
	 if ( (!(feof(dependency_handle))) && (itemsread > 0) ) {
        /* loop through each dependency looking for a match on the dependecy against
         * the key */
        for (i = 0; i < MAX_DEPENDENCIES; i++) {
			if (local_rec->dependencies[i].dependency_type != '0') {
                strncpy(local_jobname, jobname, JOB_DEPENDTARGET_LEN );
                if (local_rec->dependencies[i].dependency_type == '1') {
                   UTILS_space_fill_string( (char *)&local_jobname, JOB_NAME_LEN );
				   UTILS_space_fill_string( (char *)&local_rec->dependencies[i].dependency_name, JOB_NAME_LEN );
				}
				else {
                   UTILS_space_fill_string( (char *)&local_jobname, JOB_DEPENDTARGET_LEN );
				}

				/* use strcmp rather than memcpy as input key could be a
				 * filename or a jobname which are different lengths, we know
				 * a jobname is fully padded but a filename may not be. */
				if (strcmp(local_rec->dependencies[i].dependency_name, local_jobname) == 0) { matchfound++; }
			}
		}
        /* if found include the jobname in the response */
		if (matchfound > 0) {
		   UTILS_space_fill_string( (char *)&local_rec->job_header.jobname, JOB_NAME_LEN );
           i = snprintf( namefound, JOB_NAME_LEN+2, "%s\n", local_rec->job_header.jobname );
		   API_add_to_api_databuf( pApi_buffer, namefound, i, tx );
		}
	 }
  }

  MEMORY_free( local_rec );

  API_set_datalen( pApi_buffer, strlen(pApi_buffer->API_Data_Buffer) );     

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: Leaving SCHED_DEPEND_listall_waiting_on_dep\n" );
  }
} /* SCHED_DEPEND_listall_waiting_on_dep */

/* ---------------------------------------------------
   --------------------------------------------------- */
void SCHED_DEPEND_listall( API_Header_Def * pApi_buffer, FILE *tx ) {
   int sinkreply, i, datacount;
   char workbuf[JOB_DEPENDTARGET_LEN+13];
   dependency_queue_def * local_rec;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entering SCHED_DEPEND_listall\n" );
   }

   API_init_buffer( (API_Header_Def *)pApi_buffer );
   strcpy( pApi_buffer->API_System_Name, "DEP" );
   strcpy( pApi_buffer->API_Command_Response, API_RESPONSE_DISPLAY );

   if ( (local_rec = MEMORY_malloc(sizeof(dependency_queue_def),"SCHED_DEPEND_listall")) == NULL ) {
		   myprintf( "*ERR: LE060-SCHED_depend_listall, insufficient free memory to malloc %d bytes\n",
				   sizeof(dependency_queue_def)
			     );
           strcpy( pApi_buffer->API_Command_Response, API_RESPONSE_ERROR );
           strcpy( pApi_buffer->API_Data_Buffer, "*E* Insufficient free memory on server to execute this command\n" );
           API_set_datalen( (API_Header_Def *)pApi_buffer, strlen(pApi_buffer->API_Data_Buffer) );
		   return;
   }

   fseek( dependency_handle, 0L, SEEK_SET );
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_IO) {
		   myprintf("DEBUG: Seek to start of file on dependency queue file\n" );
   }
   while ( (!(feof(dependency_handle))) && (!(ferror(dependency_handle))) ) {
      datacount = fread( local_rec, sizeof(dependency_queue_def), 1, dependency_handle );
      if ( (!(ferror(dependency_handle))) && (datacount > 0) && (!(feof(dependency_handle))) ) {
	     if (local_rec->job_header.info_flag != 'D') {  /* not deleted record  */
            snprintf( workbuf, JOB_DEPENDTARGET_LEN + 10, "Job %s is waiting on...\n",
                      local_rec->job_header.jobname );
            sinkreply = API_add_to_api_databuf( pApi_buffer, (char *)&workbuf, strlen(workbuf), tx );
			for (i = 0; i < MAX_DEPENDENCIES; i++) {
               if (local_rec->dependencies[i].dependency_type == '1') {
                  snprintf( workbuf, JOB_DEPENDTARGET_LEN + 12, "    JOB  %s\n",
                            local_rec->dependencies[i].dependency_name );
                  sinkreply = API_add_to_api_databuf( pApi_buffer, (char *)&workbuf, strlen(workbuf), tx );
			   }
			   else if (local_rec->dependencies[i].dependency_type == '2') {
                  snprintf( workbuf, JOB_DEPENDTARGET_LEN + 12, "    FILE %s\n",
                            local_rec->dependencies[i].dependency_name );
                  sinkreply = API_add_to_api_databuf( pApi_buffer, (char *)&workbuf, strlen(workbuf), tx );
			   }
			   /* else ignore, it must be 0 (no dependency) */
			} /* end of for loop */
		 }    /* if != D */
		 else {
				 /*
            snprintf( workbuf, JOB_DEPENDTARGET_LEN + 12, "Job %s dependencies satisfied, record flagged as deleted\n",
                      local_rec->job_header.jobname );
            sinkreply = API_add_to_api_databuf( pApi_buffer, (char *)&workbuf, strlen(workbuf), tx );
			*/
		 }
      }       /* if not error */
   }          /* while no errors */

   MEMORY_free( local_rec );

   API_set_datalen( (API_Header_Def *)pApi_buffer, strlen(pApi_buffer->API_Data_Buffer) );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving SCHED_DEPEND_listall\n" );
   }
   return;
} /* SCHED_DEPEND_listall */

/* ---------------------------------------------------
   --------------------------------------------------- */
void SCHED_display_server_status( API_Header_Def * pApi_buffer, internal_flags * GLOBAL_flags, FILE *tx, int exjobs ) {
/*
 * OUTPUT FORMAT WE WILL USE
 * 
  Host Name      :
  Server Version :
  Company Name   :

  Server is Enabled/Disabled

  Log Level :              Trace Level : 

  Catch up flag    New day Time    Last new day runtime

  Total USER Jobs on scheduler queue :
  Number of jobs on the alert queue :
*/

  int  datalen, junk, x1, x2;
  char *local_buffer;

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: Entering SCHED_display_server_status\n" );
  }

  /* I know init is done by caller, do again in case some wally
     changes that. */
  API_init_buffer( pApi_buffer );
  strcpy( pApi_buffer->API_System_Name, "SCHED" );
  memcpy( pApi_buffer->API_Command_Response, API_RESPONSE_DISPLAY, API_RESPCODE_LEN );

  /* if we don't have a 2K buffer don't even attempt it */
  if (MAX_API_DATA_LEN < 2048) {
     memcpy( pApi_buffer->API_Command_Response, API_RESPONSE_ERROR, API_RESPCODE_LEN );
     datalen = snprintf( pApi_buffer->API_Data_Buffer, (MAX_API_DATA_LEN - 1),
                        "*ERROR* : MAX_API_BUFFER < 2K, programmer error\n" );
     API_set_datalen( pApi_buffer, datalen );     
     return;
  }

  /* allocate memory for a work buffer */
  if ( (local_buffer = (char *)MEMORY_malloc(MAX_API_DATA_LEN,"SCHED_display_server_status")) == NULL ) {
     myprintf( "*ERR: LE061-insufficient memory available to allocate %d bytes", MAX_API_DATA_LEN );
     junk = MAX_API_DATA_LEN;
     datalen = snprintf( pApi_buffer->API_Data_Buffer, (MAX_API_DATA_LEN - 1), 
					 "*E* Insufficient memory on server to allocate %d bytes for status command\n",
					 MAX_API_DATA_LEN );
     API_set_datalen( pApi_buffer, datalen );
     return;
  }

  strcpy(local_buffer, "Job Scheduler Server[v1.19] by Mark Dickinson 2001-2021 (GPLV2 Release)\n" );
  junk = API_add_to_api_databuf( pApi_buffer, local_buffer, strlen(local_buffer), tx );

  datalen = snprintf( local_buffer, (MAX_API_DATA_LEN - 1), "Server Version : %s\n", GLOBAL_flags->version );
  junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );

  if (GLOBAL_flags->enabled == '1') {
     datalen = snprintf( local_buffer, (MAX_API_DATA_LEN - 1), "Server Status  : ENABLED (running jobs)\n" );
  }
  else if (GLOBAL_flags->enabled == '2') {
     datalen = snprintf( local_buffer, (MAX_API_DATA_LEN - 1), "Server Status  : DISABLED as a SHUTDOWN request is queued\n" );
  }
  else {
     datalen = snprintf( local_buffer, (MAX_API_DATA_LEN - 1), "Server Status  : DISABLED (no jobs will run)\n" );
  }
  junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );

  if (GLOBAL_flags->log_level == 0) {
     datalen = snprintf( local_buffer, (MAX_API_DATA_LEN - 1), "Log Level      : ERROR" );
  }
  else   if (GLOBAL_flags->log_level == 1) {
     datalen = snprintf( local_buffer, (MAX_API_DATA_LEN - 1), "Log Level      : WARN+" );
  }
  else   if (GLOBAL_flags->log_level == 2) {
     datalen = snprintf( local_buffer, (MAX_API_DATA_LEN - 1), "Log Level      : INFO+" );
  }
  else   if (GLOBAL_flags->log_level == 9) {
     datalen = snprintf( local_buffer, (MAX_API_DATA_LEN - 1), "Log Level      : DEBUG" );
  }
  else {
     datalen = snprintf( local_buffer, (MAX_API_DATA_LEN - 1), "Log Level      : IN-ERROR (%d)", GLOBAL_flags->log_level );
  }
  junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );

  /* Display the debug levels in effect */
  datalen = snprintf( local_buffer, MAX_API_DATA_LEN,
				      "\nDebug Levels   : server %d,    utils %d,  jobslib %d,      apilib %d\n",
				       GLOBAL_flags->debug_level.server,
					   GLOBAL_flags->debug_level.utils,
					   GLOBAL_flags->debug_level.jobslib,
					   GLOBAL_flags->debug_level.api );
  junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );
  datalen = snprintf( local_buffer, MAX_API_DATA_LEN,
				      "                 alerts %d, calendar %d, schedlib %d, bulletproof %d\n",
				       GLOBAL_flags->debug_level.alerts,
					   GLOBAL_flags->debug_level.calendar,
					   GLOBAL_flags->debug_level.schedlib,
					   GLOBAL_flags->debug_level.bulletproof );
  junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );
  datalen = snprintf( local_buffer, MAX_API_DATA_LEN,
				      "                 user   %d,   memory %d\n",
					   GLOBAL_flags->debug_level.user,
					   GLOBAL_flags->debug_level.memory );
  junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );
  /* And the sched listall flag now */
  datalen = snprintf( local_buffer, MAX_API_DATA_LEN,
				      "                 show scheduler completed/deleted jobs also is %d\n",
				       GLOBAL_flags->debug_level.show_deleted_schedjobs );
  junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );

  /* whats the catchup flag set to */
  if (GLOBAL_flags->catchup_flag == '0') {
     datalen = snprintf( local_buffer, MAX_API_DATA_LEN, "Catchup Flag   : OFF\n" );
  }
  else   if (GLOBAL_flags->catchup_flag == '1') {
     datalen = snprintf( local_buffer, MAX_API_DATA_LEN, "Catchup Flag   : 24HRS\n" );
  }
  else   if (GLOBAL_flags->catchup_flag == '2') {
     datalen = snprintf( local_buffer, MAX_API_DATA_LEN, "Catchup Flag   : ALLDAYS\n" );
  }
  else {
     datalen = snprintf( local_buffer, MAX_API_DATA_LEN, "Catchup Flag   : IN-ERROR\n" );
  }
  junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );

  /* new day times */
  /* MID: Caution, ive found a \n\n combination does something funny to the
   * results of a strlen call, ie: xxxxxx\n\nyyyy will DROP the yyyy ?. So I
   * always use \n[space]\n. don't change this !
   * This has occurred repeatedly in the code so its not a one-off format error
   * here. */
  datalen = snprintf( local_buffer, MAX_API_DATA_LEN,
                     "New Day Time   : %s        Last New Day Runtime : %s\n",
                     GLOBAL_flags->new_day_time, GLOBAL_flags->last_new_day_run );
  junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );
  datalen = snprintf( local_buffer, MAX_API_DATA_LEN,
                     "                   Configured Next New Day Runtime : %s\n",
                     GLOBAL_flags->next_new_day_run );
  junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );

  /* Scheduler newday actions */
  if (GLOBAL_flags->newday_action == '0') {
     strcpy( local_buffer, "New Day Pause Action is to go into alert state\n" );
  }
  else if (GLOBAL_flags->newday_action == '1') {
     strcpy( local_buffer, "New Day Pause Action is to go dependant on a job waiting to run\n" );
  }
  else { /* must be in error */
     strcpy( local_buffer, "New Day Pause Action is UNRECOGNISED, new function ? \n" );
  }
  junk = API_add_to_api_databuf( pApi_buffer, local_buffer, strlen(local_buffer), tx );

  /* Whats on the queues at the moment */
  x1 = SCHED_count_active();
  x2 = ALERTS_count_alerts();
  datalen = snprintf( local_buffer, MAX_API_DATA_LEN,
                      "-- Total USER Jobs on scheduler queue = %d (%d executing currently)\n", x1, exjobs );
  junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );
  if (x2 > 0) {
     datalen = snprintf( local_buffer, MAX_API_DATA_LEN, "-- ********* jobs on the alert queue  = %d ********\n", x2 );
  }
  else {
     datalen = snprintf( local_buffer, MAX_API_DATA_LEN, "-- Number of jobs on the alert queue  = %d\n", x2 );
  }
  junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );

  /* free the memory we allocated */
  MEMORY_free( local_buffer );

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: Leaving SCHED_display_server_status\n" );
  }
} /* SCHED_display_server_status */

/* ---------------------------------------------------
   --------------------------------------------------- */
void SCHED_display_server_status_alertsonly( API_Header_Def * pApi_buffer, internal_flags * GLOBAL_flags, FILE *tx, int exjobs ) {
  int  datalen, junk;
  char *local_buffer;

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: Entering SCHED_display_server_status_alertsonly\n" );
  }

  /* I know init is done by caller, do again in case some wally
     changes that. */
  API_init_buffer( pApi_buffer );
  strcpy( pApi_buffer->API_System_Name, "SCHED" );
  memcpy( pApi_buffer->API_Command_Response, API_RESPONSE_DISPLAY, API_RESPCODE_LEN );

  /* if we don't have a 2K buffer don't even attempt it */
  if (MAX_API_DATA_LEN < 2048) {
     memcpy( pApi_buffer->API_Command_Response, API_RESPONSE_ERROR, API_RESPCODE_LEN );
     datalen = snprintf( pApi_buffer->API_Data_Buffer, (MAX_API_DATA_LEN - 1),
                        "*ERROR* : MAX_API_BUFFER < 2K, programmer error\n" );
     API_set_datalen( pApi_buffer, datalen );     
     return;
  }

  /* allocate memory for a work buffer */
  if ( (local_buffer = (char *)MEMORY_malloc(MAX_API_DATA_LEN,"SCHED_display_server_status_alertsonly")) == NULL ) {
     myprintf( "*ERR: LE061-insufficient memory available to allocate %d bytes", MAX_API_DATA_LEN );
     junk = MAX_API_DATA_LEN;
     datalen = snprintf( pApi_buffer->API_Data_Buffer, (MAX_API_DATA_LEN - 1), 
					 "*E* Insufficient memory on server to allocate %d bytes for status command\n",
					 MAX_API_DATA_LEN );
     API_set_datalen( pApi_buffer, datalen );
     return;
  }

  strcpy(local_buffer, "Job Scheduler Server[v1.19] by Mark Dickinson 2001-2021 (GPLV2 Release)\n" );
  junk = API_add_to_api_databuf( pApi_buffer, local_buffer, strlen(local_buffer), tx );

  /* MID: Caution, ive found a \n\n combination does something funny to the
   * results of a strlen call, ie: xxxxxx\n\nyyyy will DROP the yyyy ?. So I
   * always use \n[space]\n. don't change this !
   * This has occurred repeatedly in the code so its nit a one-off format error
   * here. */
  /* remote console stuff, added in preparation of it being used */
  datalen = snprintf( local_buffer, MAX_API_DATA_LEN, " \nAlert Forwarding information...\n" );
  junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );
  if (GLOBAL_flags->use_central_alert_console == 'Y') {
     datalen = snprintf( local_buffer, MAX_API_DATA_LEN, "  Alerts are configured to be forwarded\n" );
     junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );
     datalen = snprintf( local_buffer, MAX_API_DATA_LEN,
                        "  Remote central alert console is configured as %s, port %s\n",
                        GLOBAL_flags->remote_console_ipaddr, GLOBAL_flags->remote_console_port );
     junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );
	 if (GLOBAL_flags->central_alert_console_active != 0) {
			 datalen = snprintf( local_buffer, MAX_API_DATA_LEN, "     Remote site connection is established and working\n" );
             junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );
	 }
	 else {
			 datalen = snprintf( local_buffer, MAX_API_DATA_LEN,
                              "     Remote site is not connected. This feature is not available in standalone version\n" );
             junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );
	 }
  }
  else if (GLOBAL_flags->use_central_alert_console == 'E') {
     datalen = snprintf( local_buffer, MAX_API_DATA_LEN, "  Alerts are configured to be forwarded by an external program\n" );
     junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );
     datalen = snprintf( local_buffer, MAX_API_DATA_LEN,
                         "  external command used to raise alerts if configured for external forwarding\n    %s\n",
                         GLOBAL_flags->local_execprog_raise );
     junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );
     datalen = snprintf( local_buffer, MAX_API_DATA_LEN,
                         "  external command used to cancel alerts if configured for external forwarding\n    %s\n",
                         GLOBAL_flags->local_execprog_cancel );
     junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );
  }
  else {
		  /* note above comments, need a [space] before newline or the data is
		   * chopped off */
     datalen = snprintf( local_buffer, MAX_API_DATA_LEN,
                         "  Alert copying/forwarding to central alert monitor site is DISABLED\n" );
     junk = API_add_to_api_databuf( pApi_buffer, local_buffer, datalen, tx );
  }

  /* free the memory we allocated */
  MEMORY_free( local_buffer );

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: Leaving SCHED_display_server_status_alertsonly\n" );
  }
} /* SCHED_display_server_status_alertsonly */

/* ---------------------------------------------------
 * A quick one line banner display for some of the
 * status screens (they can use it as a banner).
   --------------------------------------------------- */
void SCHED_display_server_status_short( API_Header_Def * pApi_buffer, internal_flags * GLOBAL_flags, FILE *tx ) {
/*
OUTPUT FORMAT WE WILL USE
Host Name <hostname>, Server is Enabled/Disabled/Shutting Down, nnn running jobs, nnn alerts
*/
  int  datalen, junk;
  char serverstatus[60];
  char localbuffer[128];

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: Entering SCHED_display_server_status_short\n" );
  }

  /* I know init is done by caller, do again in case some wally
     changes that. */
  API_init_buffer( pApi_buffer );
  strcpy( pApi_buffer->API_System_Name, "SCHED" );
  memcpy( pApi_buffer->API_Command_Response, API_RESPONSE_DISPLAY, API_RESPCODE_LEN );

  if (GLOBAL_flags->enabled == '1') { strncpy( serverstatus, "ENABLED (running jobs)", 59 ); }
  else if (GLOBAL_flags->enabled == '2') { strncpy( serverstatus, "DISABLED : SHUTDOWN REQUEST QUEUED", 59 ); }
  else { strncpy( serverstatus, "DISABLED (intervention required)", 59 ); }
  datalen = snprintf( localbuffer, 127, "Server is %s\n%d jobs (non-system jobs) queued or running, %d in alert hold\n",
                     serverstatus, SCHED_count_active(), ALERTS_count_alerts() );
  junk = API_add_to_api_databuf( pApi_buffer, localbuffer, datalen, tx );

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: Leaving SCHED_display_server_status_short\n" );
  }
} /* SCHED_display_server_status_short */

/* ---------------------------------------------------
 * Submit the scheduler newday job if it is not already
 * scheduled onto the active queue.
 * return 1 if OK, 0 if not.
   --------------------------------------------------- */
void SCHED_Submit_Newday_Job( void ) {
   active_queue_def * datarec;
   long recnum;
   time_t next_time;

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: Entering SCHED_Submit_Newday_Job\n" );
  }

   if ( (datarec = (active_queue_def *)MEMORY_malloc(sizeof(active_queue_def),"SCHED_Submit_Newday_Job")) == NULL ) {
		   myprintf( "*ERR: LE062- --- INSUFFICIENT MEMORY TO SCHEDULE NEW DAY JOB CHECKS ---\n" );
		   return;
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
		   myprintf("DEBUG: malloc of %d bytes OK (SCHED_Submit_Newday_Job)\n", sizeof(active_queue_def) );
   }

   strcpy( datarec->job_header.jobname, "SCHEDULER-NEWDAY" );
   UTILS_space_fill_string( (char *)&datarec->job_header.jobname, JOB_NAME_LEN );
   memcpy( datarec->job_header.jobnumber, "000000", JOB_NUMBER_LEN );
   datarec->job_header.info_flag = 'S';    /* system job */

   /* first ensure we don't already have one scheduled */
   recnum = SCHED_ACTIVE_read_record( datarec );
   if (recnum >= 0) {
           if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
		      myprintf("DEBUG: releasing %d bytes of memory (SCHED_Submit_Newday_Job)\n", sizeof(active_queue_def) );
           }
		   MEMORY_free( datarec );
           if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
		      myprintf( "INFO: LI017-SCHEDULER-NEWDAY job already exists, no need to schedule on a new one.\n" );
		   }
		   return;
   }

   /* Find the new day time, schedule on the new job */
   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
      myprintf( "INFO: LI018-Scheduler last newday run time was %s\n", pSCHEDULER_CONFIG_FLAGS.last_new_day_run );
   }
   /*  -- workout timestamp of the nextnewday time to run */
   strncpy( datarec->next_scheduled_runtime, pSCHEDULER_CONFIG_FLAGS.next_new_day_run, JOB_DATETIME_LEN );
   UTILS_space_fill_string( (char *)&datarec->next_scheduled_runtime, JOB_DATETIME_LEN );
   next_time = UTILS_make_timestamp( (char *)&datarec->next_scheduled_runtime );
   datarec->run_timestamp = next_time;
   datarec->current_status = '0'; /* time wait */
   datarec->hold_flag = 'N';      /* not held */
   memcpy( datarec->failure_code, "000", JOB_STATUSCODE_LEN );
   strncpy( datarec->job_owner, "INTERNAL", JOB_OWNER_LEN );
   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
      myprintf( "INFO: LI019-Scheduler next newday run time is  %s\n", datarec->next_scheduled_runtime );
   }
   if (next_time < UTILS_timenow_timestamp()) {
      myprintf( "WARN: LW004-Newday job scheduled for time in past, catchup is in progress\n" );
   }
   recnum = SCHED_ACTIVE_write_record( datarec, -1 );
   if (recnum < 0) {
		   myprintf( "*ERR: LE063-Got an error return adding the newday job to the active job queue\n" );
   }
   else {
       if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
		   myprintf( "INFO: LI020-SCHEDULER-NEWDAY Job added to active job queue\n" );
	   }
   }

   /* free the memory we have allocated */
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf("DEBUG: releasing %d bytes of memory (SCHED_Submit_Newday_Job)\n", sizeof(active_queue_def) );
   }
   MEMORY_free( datarec );
  
  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: Leaving SCHED_Submit_Newday_Job\n" );
  }
} /* SCHED_Submit_Newday_Job */

/* ---------------------------------------------------
   --------------------------------------------------- */
void SCHED_DEBUG_dump_active_queue_def( active_queue_def * recordptr ) {
	myprintf( "DEBUG:===== active queue record dump =====\n" );
    API_DEBUG_dump_job_header( (char *) recordptr );
	myprintf( "Next runtime is %s, current status is %c, last failure code is %s\n",
			recordptr->next_scheduled_runtime, 
			recordptr->current_status,
			recordptr->failure_code
		  );
	myprintf( "DEBUG:==================================\n" );
} /* SCHED_DEBUG_dump_active_queue_def */

/* ---------------------------------------------------
   --------------------------------------------------- */
void SCHED_DEBUG_stat_dependency_file(void) {
   struct stat * stat_buf;
   long number_of_records;
   int lasterror;
		
/* stat buffer defined for LINUX
      struct stat
        {
         dev_t         st_dev;      device
         ino_t         st_ino;      inode
         mode_t        st_mode;     protection
         nlink_t       st_nlink;    number of hard links
         uid_t         st_uid;      user ID of owner
         gid_t         st_gid;      group ID of owner
         dev_t         st_rdev;     device type (if inode device)
         off_t         st_size;     total size, in bytes
	     unsigned long st_blksize;  blocksize for filesystem I/O
	     unsigned long st_blocks;   number of blocks allocated
	     time_t        st_atime;    time of last access
	     time_t        st_mtime;    time of last modification
	     time_t        st_ctime;    time of last change
	    };
*/
		
   if ( (stat_buf = (struct stat *)MEMORY_malloc(sizeof(struct stat),"SCHED_DEBUG_stat_dependency_file")) == NULL ) {
	  return;
   }
   lasterror = stat( dependency_file, stat_buf );   /* if <> 0 an error, ENOENT if not there */
   number_of_records = (int)(stat_buf->st_size / sizeof(dependency_queue_def) );
   myprintf( "DEBUG: Dependency file size %d bytes, %d records\n", (int)stat_buf->st_size, (int)number_of_records );
   MEMORY_free( stat_buf );
   return;
} /* SCHED_DEBUG_stat_dependency_file */

/* ---------------------------------------------------
 * Added because a client delete dependency request may
 * pass a filename as well as a jobname. The original
 * ...relying_on_job has been changed to call this
 * rather than doing the work itself.
   --------------------------------------------------- */
void SCHED_DEPEND_delete_all_relying_on_dependency( char * dependency_name ) {
   dependency_queue_def local_rec;
   long record_address;
   int  i, changed, itemsread;
   char workbuf[JOB_DEPENDTARGET_LEN+1];
   char workbuf2[JOB_DEPENDTARGET_LEN+1];

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entering SCHED_DEPEND_delete_all_relying_on_dependency\n" );
   }

   /* Added an extra info message as a debugging tool */
   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
/*      myprintf( "INFO: LI021-Clearing all dependency waits for %s\n", dependency_name ); */
/* seems to have a newline in the buffer */
      myprintf( "INFO: LI021-Clearing all dependency waits for %s", dependency_name );
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_IO) {
      SCHED_DEBUG_stat_dependency_file();
   }

   /* filenames may be mixed case, but jobnames always uppercase. Have a
	* uppercase test string also in case it's a job */
   strncpy( workbuf2, dependency_name, JOB_DEPENDTARGET_LEN );
   UTILS_space_fill_string( (char *)&workbuf2, JOB_DEPENDTARGET_LEN );
   UTILS_uppercase_string( (char *)&workbuf2 );

   clearerr( dependency_handle );
   fseek( dependency_handle, 0L, SEEK_SET );
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_IO) {
		   myprintf( "DEBUG: Seek to start of dependency queue file\n" );
   }
   itemsread = 1;
   while ( (!(feof(dependency_handle))) && (!(ferror(dependency_handle))) && (itemsread > 0) ) {
      record_address = ftell(dependency_handle);  /* the record we are about to read, in case it needs updating */
      itemsread = fread( (char *)&local_rec.job_header.jobnumber, sizeof(dependency_queue_def), 1, dependency_handle );
      if ( (!(ferror(dependency_handle))) && (itemsread == 1) ) {
		 if (feof(dependency_handle)) { 
			  itemsread = 0;
		 }
         if ( local_rec.job_header.info_flag != 'D' ) {
	        changed = 0;
            for (i = 0; i < MAX_DEPENDENCIES; i++) {
			   strncpy( workbuf, local_rec.dependencies[i].dependency_name, JOB_DEPENDTARGET_LEN );
			   UTILS_space_fill_string( (char *)&workbuf, JOB_DEPENDTARGET_LEN );
               if ( 
					   (local_rec.dependencies[i].dependency_type != '0') &&     /* dependency is active */
                       (
                          (memcmp(workbuf,dependency_name,JOB_DEPENDTARGET_LEN) == 0) ||
                          (memcmp(workbuf,workbuf2,JOB_DEPENDTARGET_LEN) == 0)
                       )
			      )
		   	   {
			      local_rec.dependencies[i].dependency_type = '0'; /* dependency is now satisfied, OFF */
			      changed = 1; /* we have changed the record at least once */
                  myprintf( "JOB-: LI022-JOB %s, DEPENDENCY WAIT FOR %s SATISFIED\n",
				          local_rec.job_header.jobname,
			  		      dependency_name );
			   }
		    } /* for i < MAX_DEPENDENCIES */
		    if (changed != 0) {
               if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_IO) {
		          myprintf( "DEBUG: Seek and write record %d (%d) in dependency queue file for %s\n",
                               (int)(record_address / sizeof(dependency_queue_def)), (int)record_address,
							   local_rec.job_header.jobname);
               }
			   clearerr( dependency_handle );
		       if (fseek( dependency_handle, record_address, SEEK_SET ) != 0) {
					   perror( "dependency queue file" );
					   myprintf( "*ERR: LE065-Seek error %d on dependency handle, ABORTING out of SCHED_DEPEND_delete_all_relying_on_dependency\n", errno );
					   return;
			   }
		       fwrite( (char *)&local_rec.job_header.jobnumber, sizeof(dependency_queue_def), 1, dependency_handle );
		       if (ferror(dependency_handle)) {
				  perror( dependency_file );
                  myprintf("*ERR: LE066-File write error on dependency handle, error %d\n (SCHED_DEPEND_delete_all_relying_on_dependency)", errno );
				  itemsread = 0; /* this will stop the while loop */
		       }
			   else {
                  if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_IO) {
						  SCHED_DEBUG_stat_dependency_file();
				  }
			   }
		    } /* if changed */
		 } /* if not info_flg == 'D' */
      }  /* if ! ferror and item was read */
   }     /* while */

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG: Done all check updates, final stat before calling SCHED_DEPEND_check_job_dependencies\n" );
      SCHED_DEBUG_stat_dependency_file();
   }

   /* see if any records are free now */
   SCHED_DEPEND_check_job_dependencies();

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_IO) {
      SCHED_DEBUG_stat_dependency_file();
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving SCHED_DEPEND_delete_all_relying_on_dependency\n" );
   }
} /* SCHED_DEPEND_delete_all_relying_on_dependency */

/* ---------------------------------------------------
 * Hold or release a job. 
 * Parm 1 - jobname
 * Parm 2 - 0 is turn hold off, 1 is turn hold on
 * Returns : 0 no error, 1 error occured
   --------------------------------------------------- */
int SCHED_hold_job( char * jobname, int actiontype ) {
   int respcode;
   long recnum;
   active_queue_def *local_active_rec;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered SCHED_hold_job with %s, action %d\n", jobname, actiontype );
   }

   respcode = 0;  /* default */

   if ((local_active_rec = (active_queue_def *)MEMORY_malloc( sizeof( active_queue_def ),"SCHED_hold_job" )) == NULL) {
      myprintf( "*ERR: LE067-Unable to allocate memory required for read operation\n" );
      UTILS_set_message("Unable to allocate memory required for read operation");
      return( 1 );
   }

   strncpy( local_active_rec->job_header.jobname, jobname, JOB_NAME_LEN );
   UTILS_space_fill_string( (char *)&local_active_rec->job_header.jobname, JOB_NAME_LEN );
   recnum = SCHED_ACTIVE_read_record( local_active_rec );
   if (recnum >= 0) {
      if (actiontype == 1) {
	     if (local_active_rec->hold_flag != 'Y') {
            local_active_rec->hold_flag = 'Y';       /* ensure it is not held */
		     if (SCHED_ACTIVE_write_record( local_active_rec, recnum ) == -1) {
                respcode = 1;
                myprintf( "*ERR: LE068-Job '%s' unable to be held, write error\n", local_active_rec->job_header.jobname );
		     }
			 else {
                myprintf( "JOB-: LI023-Hold turned ON  for job %s\n", local_active_rec->job_header.jobname );
			 }
	     }
	  }
	  else if (actiontype == 0) {
	     if ( (local_active_rec->hold_flag == 'Y') || (local_active_rec->hold_flag == 'C') ) {
            local_active_rec->hold_flag = 'N';       /* ensure it is not held */
		     if (SCHED_ACTIVE_write_record( local_active_rec, recnum ) == -1) {
                respcode = 1;
                myprintf( "*ERR: LE069-Job '%s' unable to be released, write error\n", local_active_rec->job_header.jobname );
		     }
			 else {
                myprintf( "JOB-: LI024-Hold turned off for job %s\n", local_active_rec->job_header.jobname );
			 }
	     }
	  }
	  else {
         respcode = 1;
         myprintf( "*ERR: LE070-Job hold actionflag illegal %d (SCHED_hold_job)\n", actiontype );
	  }
   }
   else {
      respcode = 1;  /* unable to read it */
      myprintf( "*ERR: LE071-Job '%s' unable to be held/released, not found\n", local_active_rec->job_header.jobname );
   }

   MEMORY_free( local_active_rec );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.schedlib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving SCHED_hold_job with response %d\n", respcode );
   }
   return( respcode );
} /* SCHED_hold_job */
