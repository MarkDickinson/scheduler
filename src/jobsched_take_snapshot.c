/*
    JOBSCHED_TAKE_SNAPSHOT.C

	Create a command file that can be used bu jobsched_cmd to...
	1) rebuild the job database file
	2) rebuild the scheduler configuration file
	This can be used for recovery purposes.

    Scheduler is by Mark Dickinson, 2001-2002
*/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include "scheduler.h"
#include "utils.h"
#include "config.h"
#include "userrecdef.h"
#include "memorylib.h"

#define MAX_CHAR_LEN 254
#define MAX_DIR_LEN (MAX_CHAR_LEN - 15)

/* Global needed to use the utils library */
FILE *msg_log_handle = NULL;

/* ---------------------------------------------------
   --------------------------------------------------- */
void common_banner( char * directory_path ) {
   time_t time_now;

   printf( "# ************************************************************************************************\n" );
   time_now = time(0);
   printf( "#\n#     Output from 'jobsched_take_snapshot', created on %s", ctime( &time_now ) );
   printf( "#              Part of Job Scheduler, Mark Dickinson 2001\n" );
   printf( "#\n# This command file can be used with jobsched_cmd to reload jobs into the server.\n#\n" );
   printf( "# This snapshot is from databases in %s\n#\n", directory_path );
   printf( "# ************************************************************************************************\n#\n#\n" );
} /* common_banner */

/* ---------------------------------------------------
   --------------------------------------------------- */
void end_banner( char * directory_path ) {
   printf( "#\n# ************************************************************************************************\n" );
   printf( "# End of configuration snapshot from %s\n", directory_path );
   printf( "# ************************************************************************************************\n" );
} /* end_banner */

/* ---------------------------------------------------
 * Generate a header banner for the records we are
 * about to output.
 *
 * Note: ctime puts in a linefeed so we don't need to
 *       include a newline on those lines.
   --------------------------------------------------- */
int generate_banner( char * filename, char * filetype, int reclen ) {
   struct stat * stat_buf;
   int lasterror, record_check;
   long size_check;

   printf( "#\n# ************************************************************************************************\n" );
   printf( "#\n# Extract of data records from %s\n", filetype );
   printf( "#           File name : %s\n", filename );

   if ( (stat_buf = malloc(sizeof(struct stat))) == NULL ) {
      printf( "# ------> E R R O R <----- Insufficient free memory, cannot allocate %d bytes\n", sizeof(struct stat) );
      return(1);
   }
   lasterror = stat( filename, stat_buf );   /* if <> 0 an error, ENOENT if not there */
   if (lasterror == ENOENT) {
      printf( "# ------> E R R O R <----- file %s does not exist, nothing to do.\n", filename );
	  return( 1 );
   }
   else if (lasterror != 0) {
      perror( "Unable to stat file" );
      printf( "# ------> E R R O R <----- %s cannot be accessed !\n", filename );
      return( 1 );
   }

   /* a quick sanity check, is the filesize correct for our record length */
   record_check = (int)(stat_buf->st_size / reclen );
   size_check = record_check * reclen;
   if (size_check != (long)stat_buf->st_size) {
      printf( "# ------> E R R O R <----- %s does not appear to be a %s file (bad record length)\n", filename, filetype );
      return( 1 );
   }

   printf( "#  File last modified : %s", ctime(&stat_buf->st_mtime) );
   printf( "#\n" );
   printf( "# ************************************************************************************************\n" );
   free( stat_buf );

   /* file exists, so assume all OK */
   return( 0 ); 
} /* generate_banner */

/* ---------------------------------------------------
   --------------------------------------------------- */
int CONFIG_dump_config_file( char * configfile ) {
   int items_read;
   FILE *config_handle = NULL;
   internal_flags config;
   char cmdline[MAX_CHAR_LEN+1];

   if (generate_banner(configfile, "Configuration File", sizeof(internal_flags) ) != 0) { /* check the file, write the banner */
		   return( 1 );
   }

   /* Open the config file we are to read from */
   if ( (config_handle = fopen( configfile, "rb" )) == NULL ) {
	  perror( "config file" );
      printf( "*ERR: Unable to open configuration file %s for configuration dumping, err=%d\n",
              configfile, errno );
	  return( 1 );
   }

   /* -- Do all the work now, loop through the job file writing out the
	*    entries we read from it, ignore any deleted oned though. */
   clearerr( config_handle );
   fseek( config_handle, 0, SEEK_SET );
   items_read = fread( (char *)&config, sizeof(internal_flags), 1, config_handle );
   fclose( config_handle );
   if (items_read < 1) {
		   printf( "Unable to read a complete configuration record !,\n configuration not extracted\n" );
		   return( 1 );
   }

   /* We must bypass the default of operator only access */
   printf( "# Default logon level is operator, we must switch to ADMIN access level\n" );
   printf( "# This will only work if you are logged on as root when loading these !\n" );
   printf( "AUTOLOGON\n" );

   /* Set the log levels */
   if (config.log_level == 0) {
      printf("SCHED LOGLEVEL ERROR\n" );
   }
   else if (config.log_level == 1) {
      printf("SCHED LOGLEVEL WARN\n" );
   }
   else {
      printf("SCHED LOGLEVEL INFO\n" );
   }

   /* the newday time */
   printf( "SCHED NEWDAYTIME %s\n", config.new_day_time );

   /* the scheduler-newday job action */
   if (config.newday_action == '1') {
      printf( "SCHED NEWDAYPAUSEACTION DEPWAIT\n" );
   }
   else {
      printf( "SCHED NEWDAYPAUSEACTION ALERT\n" );
   }

   /* the debug levels, turn all off for a recovery */
   printf( "DEBUG ALL 0\n" );

   if (config.catchup_flag == '0') {
      printf( "SCHED CATCHUP NONE\n" );
   }
   else if (config.catchup_flag == '2') {
      printf( "SCHED CATCHUP ALLDAYS\n" );
   }
   else {
      printf("# WARNING: A Catchup Flag value I know nothing about (new ?), setting cathup to OFF\n" );
      printf( "SCHED CATCHUP OFF\n" );
   }
   if (config.enabled == '0') {
		   printf("# **********************************************************\n" );
		   printf("# **** Caution, SCHED EXECJOBS was OFF at snapshot time ****\n" );
		   printf("# **********************************************************\n" );
		   printf("SCHED EXECJOBS OFF\n" );
   }
   else {   /* enabled or in a shutdown pending state */
		   printf( "SCHED EXECJOBS ON\n" );
   }

   /* remote console stuff */
   printf( "#\n#\n# Alert forwarding to remote application config parameters\n" );
   if (config.use_central_alert_console == 'Y') {
      strncpy(cmdline, "SCHED ALERT FORWARDING ACTION ON", MAX_CHAR_LEN );
   }
   else if (config.use_central_alert_console == 'E') {
      strncpy(cmdline, "SCHED ALERT FORWARDING ACTION EXTERNALCMD", MAX_CHAR_LEN );
   }
   else {
      strncpy(cmdline, "SCHED ALERT FORWARDING ACTION OFF", MAX_CHAR_LEN );
   }
   printf( "%s\n", cmdline );
   snprintf(cmdline, MAX_CHAR_LEN, "SCHED ALERT FORWARDING IPADDR %s", config.remote_console_ipaddr );
   printf( "%s\n", cmdline );
   snprintf(cmdline, MAX_CHAR_LEN, "SCHED ALERT FORWARDING PORT %s", config.remote_console_port );
   printf( "%s\n", cmdline );
   snprintf(cmdline, MAX_CHAR_LEN, "SCHED ALERT FORWARDING EXECRAISECMD %s", config.local_execprog_raise );
   printf( "%s\n", cmdline );
   snprintf(cmdline, MAX_CHAR_LEN, "SCHED ALERT FORWARDING EXECCANCELCMD %s", config.local_execprog_cancel );
   printf( "%s\n", cmdline );
   printf( "#  end of alert forwarding parameters\n#\n" );

   /* All done */
   return( 0 );
} /* CONFIG_dump_config_file */

/* ---------------------------------------------------
 * Read through each record in the file extracting
 * the job information we need to recreate the record
 * and writing it in a form that can be used by the
 * jobsched_cmd program to recreate the job datafile.
 *
 * NOTE: we use an 8K buffer, that we walk all over with
 * pointers here, so don't change the buffer size.
 * Basically the first 4K is building our line, the
 * last 4K is for a work area, however sometimes we
 * use the last 2K as an additional scratchpad where
 * I know that of the last 4K the work in it will not
 * reach the last 2K im overwriting.
   --------------------------------------------------- */
int JOBS_dump_job_file( char * jobsfile ) {
   int items_read, i, depcount;
   FILE *job_handle = NULL;
   jobsfile_def * jobbuffer;
   char * membuffer, *ptr1, *ptr2, *ptr3;

   if (generate_banner(jobsfile, "Job Database File", sizeof(jobsfile_def) ) != 0) { /* check the file, write the banner */
		   return( 1 );
   }

   /* Open the jobs file we are to read from */
   if ( (job_handle = fopen( jobsfile, "rb" )) == NULL ) {
	  perror( "jobsfile" );
      printf( "*ERR: Unable to open job database file %s for job dumping, err=%d\n",
              jobsfile, errno );
	  return( 1 );
   }

   /* Allocate memory for the jobsfile records */
   if ( (jobbuffer = malloc(sizeof(jobsfile_def))) == NULL) { 
      printf(" *ERR: Unable to allocate %d bytes of memory\n", sizeof(jobsfile_def) );
      fclose( job_handle );
      return( 1 );
   }
   /* Allocate memory for out string fomatting */
   if ( (membuffer = malloc(8192)) == NULL ) {  /* 1st 4K is string, 2nd is a work area */
      printf(" *ERR: Unable to allocate %d bytes of memory\n", (4096 * 2) );
	  free( jobbuffer );
      fclose( job_handle );
      return( 1 );
   }
   ptr1 = membuffer;          /* string output area */
   ptr2 = membuffer + 4096;   /* general purpose work area */

   /* -- Do all the work now, loop through the job file writing out the
	*    entries we read from it, ignore any deleted oned though. */
   clearerr( job_handle );
   fseek( job_handle, 0, SEEK_SET );
   while ( (!(ferror(job_handle))) && (!(feof(job_handle))) ) {
      items_read = fread( jobbuffer, sizeof(jobsfile_def), 1, job_handle );
	  if ( (items_read > 0) && (jobbuffer->job_header.info_flag != 'D') ) {
         /* --- check the info flag, we use this to decide if we are
          * reading from a non-job file as well as the initial filesize test */
         if ( (jobbuffer->job_header.info_flag != 'A') &&
              (jobbuffer->job_header.info_flag != 'S') ) {    /* note: checked for D above */
            free( membuffer );
            free( jobbuffer );
            fclose( job_handle );
			printf( "------> E R R O R <----- file %s does not appear to be a legal job data file (bad data in record)\n", jobsfile );
			return( 1 );
		 }

         /* format in the stuff all jobs will have first */
			  /* remove the trailing spaces from the jobname */
         UTILS_remove_trailing_spaces( jobbuffer->job_header.jobname );
		      /* change the time 00:00:00 value to 00:00 */
		 ptr3 = (char *)&jobbuffer->next_scheduled_runtime;
		 while ((*ptr3 != ':') && (*ptr3 != '\0')) { ptr3++; };
		 ptr3++;
		 while ((*ptr3 != ':') && (*ptr3 != '\0')) { ptr3++; };
		 *ptr3 = '\0';
         snprintf( ptr1, 4096, "JOB ADD %s,OWNER %s,TIME %s,CMD %s,DESC %s",
                   jobbuffer->job_header.jobname, jobbuffer->job_owner, jobbuffer->next_scheduled_runtime,
				   jobbuffer->program_to_execute, jobbuffer->description );

		 /* Are any parameters provided */
		 if (strlen(jobbuffer->program_parameters) > 0) {
				 snprintf( ptr2, (JOB_PARM_LEN + 8), ",PARM \"%s\"", jobbuffer->program_parameters );
				 strcat( ptr1, ptr2 );
		 }

		 /* check to see if we need to define dependencies */
		 depcount = 0;
		 for (i = 0; i < MAX_DEPENDENCIES; i++) {
				 if (jobbuffer->dependencies[i].dependency_type != '0') { depcount++; }
		 }
		 if (depcount > 0) { /* there are some dependencies to process */
				 strcat( ptr1, ",DEP \"" );
				 strcpy( ptr2, "" );
				 for (i = 0; i < MAX_DEPENDENCIES; i++) {
						 if (jobbuffer->dependencies[i].dependency_type == '1') {
								 snprintf( ptr2, (JOB_DEPENDTARGET_LEN + 6),
                                          "JOB %s,", jobbuffer->dependencies[i].dependency_name );
								 strcat( ptr1, ptr2 );
						 }
						 else if (jobbuffer->dependencies[i].dependency_type == '2') {
								 snprintf( ptr2, (JOB_DEPENDTARGET_LEN + 7),
                                          "FILE %s,", jobbuffer->dependencies[i].dependency_name );
								 strcat( ptr1, ptr2 );
						 }
						 /* else either 0 or unknown so ignore */
				 }
				 /* kill off the last comma we put in */
				 i = strlen(ptr1);
				 ptr3 = (ptr1 + i) - 1;
				 *ptr3 = '\0';
				 /* and close the dep command */
				 strcat( ptr1, "\"" );
		 } /* adding dependeny info */

		 /* Check to see if there is calendar information */
		 if (jobbuffer->use_calendar != '0') {
            if (jobbuffer->use_calendar == '1') {
                    UTILS_remove_trailing_spaces( jobbuffer->calendar_name );
					snprintf( ptr2, (JOB_CALNAME_LEN + 12), ",CALENDAR %s", jobbuffer->calendar_name );
					strcat( ptr1, ptr2 );
			}
			else if (jobbuffer->use_calendar == '2') {
					strcpy( ptr2, ",DAYS \"" );
					ptr3 = ptr2 + 2048;   /* use the tail end of the workspace for day names */
					for (i = 0; i < 7; i++ ) {
							if (jobbuffer->crontype_weekdays[i] != '0') {
								UTILS_get_dayname( i, ptr3 );
								strcat( ptr2, ptr3 );
								strcat( ptr2, "," );
							}
					}
					strcat( ptr1, ptr2 );
					/* remove the last comma */
				    i = strlen(ptr1);
				    ptr3 = (ptr1 + i) - 1;
				    *ptr3 = '\0';
				    /* and close the days command */
				    strcat( ptr1, "\"" );
			}
			else if (jobbuffer->use_calendar == '3') {
					/* ptr 2 is a 4096 byte buffer so 100 is safe here */
					snprintf( ptr2, 100, ",REPEATEVERY %s", jobbuffer->requeue_mins );
					strcat( ptr1, ptr2 );
			}
			/* else we don't know about it */
		 } /* adding calendar info */

         /* Check the job catchup flag */
         if (jobbuffer->job_catchup_allowed == 'N') {
            strcat( ptr1, ",CATCHUP NO" );
         }
         else {
            strcat( ptr1, ",CATCHUP YES" );
         }
	     strcat( ptr1, "\n" );
	     printf( "#\n# Job %s, %s\n", jobbuffer->job_header.jobname, jobbuffer->description );
         printf( ptr1 );
		 printf( "# JOB SUBMIT %s\n", jobbuffer->job_header.jobname );
	  } /* if not a deleted entry */
   } /* while no errors and not eof on input */

   /* free the memory we have allocated */
   free( membuffer );
   free( jobbuffer );

   /* close the file we opened */
   fclose( job_handle );
   /* All done */
   return( 0 );
} /* JOBS_dump_job_file */

/* ---------------------------------------------------
 * Read through each record in the file extracting
 * the user information we need to recreate the record
 * and writing it in a form that can be used by the
 * jobsched_cmd program to recreate the job datafile.
 *
 * NOTE: we use an 4K work buffer
   --------------------------------------------------- */
int USER_dump_user_file( char * userfile ) {
   int items_read, i;
   FILE *user_handle = NULL;
   USER_record_def * userbuffer;
   char * membuffer, *ptr1;

   /* check the file, write the banner */
   if (generate_banner(userfile, "User Database File", sizeof(USER_record_def) ) != 0) {
		   return( 1 );
   }

   /* Open the user file we are to read from */
   if ( (user_handle = fopen( userfile, "rb" )) == NULL ) {
	  perror( "userfile" );
      printf( "*ERR: Unable to open user database file %s for user dumping, err=%d\n",
              userfile, errno );
	  return( 1 );
   }

   /* Allocate memory for the user records */
   if ( (userbuffer = malloc(sizeof(USER_record_def))) == NULL) { 
      printf(" *ERR: Unable to allocate %d bytes of memory\n", sizeof(USER_record_def) );
      fclose( user_handle );
      return( 1 );
   }
   /* Allocate memory for out string fomatting */
   if ( (membuffer = malloc(4096)) == NULL ) { 
      printf(" *ERR: Unable to allocate 4096 bytes of memory\n");
	  free( userbuffer );
      fclose( user_handle );
      return( 1 );
   }
   ptr1 = membuffer;          /* string output area */

   /* -- Do all the work now, loop through the user file writing out the
	*    entries we read from it, ignore any deleted oned though. */
   clearerr( user_handle );
   fseek( user_handle, 0, SEEK_SET );
   while ( (!(ferror(user_handle))) && (!(feof(user_handle))) ) {
      items_read = fread( userbuffer, sizeof(USER_record_def), 1, user_handle );
	  if ( (items_read > 0) && (userbuffer->record_state_flag != 'D') ) {
         /* --- check the version flag, we use this to decide if we are
          * reading from a valis user file as the initial filesize test */
         if (memcmp(userbuffer->userrec_version, USER_LIB_VERSION, 2) != 0) {
            free( membuffer );
            free( userbuffer );
            fclose( user_handle );
			printf( "------> E R R O R <----- file %s does not appear to be a legal user data file (bad version header)\n", userfile );
			return( 1 );
		 }

         /* format in the stuff all user records will have first */
			  /* remove the trailing spaces from the jobname */
         UTILS_remove_trailing_spaces( userbuffer->user_name );
         UTILS_remove_trailing_spaces( userbuffer->user_details );
         snprintf( ptr1, 4096, "USER ADD %s,DESC \"%s\",PASSWORD default,",
                   userbuffer->user_name, userbuffer->user_details );

         /* User auth level */
         strcat( ptr1, "AUTH " );
         if (userbuffer->user_auth_level == 'S') {
            strcat( ptr1, "SECURITY," );
         }
         else if (userbuffer->user_auth_level == 'A') {
            strcat( ptr1, "ADMIN," );
         }
         else if (userbuffer->user_auth_level == 'O') {
            strcat( ptr1, "OPERATOR," );
         }
         else if (userbuffer->user_auth_level == 'J') {
            strcat( ptr1, "JOB," );
         }
         else {
            strcat( ptr1, "BROWSE," );
         }

         /* Auto-Auth access allowed */
         strcat( ptr1, "AUTOAUTH " );
         if (userbuffer->local_auto_auth == 'Y') {
            strcat( ptr1, "YES," );
         }
		 else {
            strcat( ptr1, "NO," );
         }

         /* Allowed to create other users */
         strcat( ptr1, "SUBSETADDAUTH " );
         if (userbuffer->user_auth_level == 'Y') {
            strcat( ptr1, "YES," );
         }
		 else if (userbuffer->user_auth_level == 'S') {
            strcat( ptr1, "ADMIN," );
         }
		 else {
            strcat( ptr1, "NO," );
         }

		 /* Now build the user list this user has access to */
         strcat( ptr1, "USERLIST \"" );
         for (i = 0; i < USERS_MAX_USERLIST; i++) {
              UTILS_remove_trailing_spaces( userbuffer->user_list[i].userid );
              strcat( ptr1, userbuffer->user_list[i].userid );
              if (i < (USERS_MAX_USERLIST - 1)) {
                  strcat( ptr1, "," );
              }
         }
         strcat( ptr1, "\"" );
         if (memcmp(userbuffer->user_name,"auto-login",10) == 0) {
             printf("# auto-login is a reserved system user");
             printf( "# %s\n", ptr1 );
         }
         else {
             printf( "%s\n", ptr1 );
         }
      } /* if not a deleted entry */
   } /* while no errors and not eof on input */

   /* free the memory we have allocated */
   free( membuffer );
   free( userbuffer );

   /* close the file we opened */
   fclose( user_handle );
   /* All done */
   return( 0 );
} /* USER_dump_user_file */

/* ---------------------------------------------------
 * Read through each record in the file extracting
 * the user information we need to recreate the record
 * and writing it in a form that can be used by the
 * jobsched_cmd program to recreate the job datafile.
 *
 * NOTE: we use an 4K work buffer
   --------------------------------------------------- */
int CALENDAR_dump_calendar_file( char * calendarfile ) {
   int items_read, i, j, discard, calendar_count, daysfound;
   FILE *calendar_handle = NULL;
   calendar_def * calbuffer;
   char * membuffer, *ptr1;
   char smallbuf[3];
   char datebuffer[100]; /* to hold at least 31 * 3 */
   char monthname[4];
   char caltype[8];
   int calendar_line_written;

   calendar_count = 0;

   /* check the file, write the banner */
   if (generate_banner(calendarfile, "Calendar Database File", sizeof(calendar_def) ) != 0) {
      return( 1 );
   }

   /* Open the calendar file we are to read from */
   if ( (calendar_handle = fopen( calendarfile, "rb" )) == NULL ) {
	  perror( "calendarfile" );
      printf( "*ERR: Unable to open calendar database file %s for user dumping, err=%d\n",
              calendarfile, errno );
	  return( 1 );
   }

   /* Allocate memory for the calendar records */
   if ( (calbuffer = malloc(sizeof(calendar_def))) == NULL) { 
      printf(" *ERR: Unable to allocate %d bytes of memory\n", sizeof(calendar_def) );
      fclose( calendar_handle );
      return( 1 );
   }
   /* Allocate memory for out string fomatting */
   if ( (membuffer = malloc(4096)) == NULL ) { 
      printf(" *ERR: Unable to allocate 4096 bytes of memory\n");
	  free( calbuffer );
      fclose( calendar_handle );
      return( 1 );
   }

   /* Opps, forgot to add this, it is needed */
   ptr1 = membuffer;

   /* -- Do all the work now, loop through the user file writing out the
	*    entries we read from it, ignore any deleted oned though. */
   clearerr( calendar_handle );
   fseek( calendar_handle, 0, SEEK_SET );
   while ( (!(ferror(calendar_handle))) && (!(feof(calendar_handle))) ) {
      items_read = fread( calbuffer, sizeof(calendar_def), 1, calendar_handle );
	  if ( (items_read > 0) && (calbuffer->calendar_type != '9') ) { /* 9 is deleted here */
         calendar_line_written = 0;
         discard = 0;
         UTILS_remove_trailing_spaces( calbuffer->calendar_name );
         UTILS_remove_trailing_spaces( calbuffer->description );
         if (calbuffer->calendar_type == '0') {
            strcpy( caltype, "JOB" );
         }
         else if (calbuffer->calendar_type == '1') {
            strcpy( caltype, "HOLIDAY" );
         }
         else {
            discard++;  /* Not legal */
            printf( "# ***ERROR*** Calendar %s below has an unrecognised type, discarded\n", calbuffer->calendar_name );
         }
         printf( "# Calendar %s, year %s, type %s, description %s\n",
                  calbuffer->calendar_name, calbuffer->year, caltype, calbuffer->description );
		 if (discard == 0) {
            calendar_count++;
			/* Placeholder created now */
			strcpy( ptr1, "" );  /* start new string */
   		    /* Format */
		    for (i = 0; i < 12; i++) {
		       daysfound = 0;
               strcpy( datebuffer, "" );
		       for (j = 0; j < 31; j++) {
				 if (calbuffer->yearly_table.month[i].days[j] == 'Y') {
                         snprintf( smallbuf, 3, "%.2d", (j + 1) ); /* offset is from 0, add 1 to get day number */
                         if (smallbuf[0] == ' ') { smallbuf[0] = '0'; }
						 strcat(datebuffer,smallbuf);
						 strcat(datebuffer,",");
						 daysfound++;
				 }
		       } /* 0-30 days */
               switch (i) {
                  case 0:
			          strcpy(monthname,"JAN");
			          break;
                  case 1:
			          strcpy(monthname,"FEB");
			          break;
                  case 2:
			          strcpy(monthname,"MAR");
			          break;
                  case 3:
			          strcpy(monthname,"APR");
			          break;
                  case 4:
			          strcpy(monthname,"MAY");
			          break;
                  case 5:
			          strcpy(monthname,"JUN");
			          break;
                  case 6:
			          strcpy(monthname,"JUL");
			          break;
                  case 7:
			          strcpy(monthname,"AUG");
			          break;
                  case 8:
			          strcpy(monthname,"SEP");
			          break;
                  case 9:
			          strcpy(monthname,"OCT");
			          break;
                  case 10:
			          strcpy(monthname,"NOV");
			          break;
                  case 11:
			          strcpy(monthname,"DEC");
			          break;
                  default:
			          strcpy(monthname,"---");
			          discard++;
			          break;
		       }
			   if (daysfound > 0) {  /* need a line */
				  if (discard == 0) {
		             if (calbuffer->calendar_type == '0') {
                        strcpy( caltype, "JOB" );
		             }
		             else if (calbuffer->calendar_type == '1') {
                        strcpy( caltype, "HOLIDAY" );
		             }
                     datebuffer[(strlen(datebuffer)-1)] = '\0'; /* replace last comma with null */
                     if (calendar_line_written == 0) {
                        calendar_line_written = 1;
                        snprintf( ptr1, 4096, "CALENDAR ADD %s,DESC \"%s\",TYPE %s,YEAR %s,FORMAT DAYS %s \"%s\"",
                            calbuffer->calendar_name, calbuffer->description,caltype,
                            calbuffer->year, monthname, datebuffer );
                     }
                     else {
                        snprintf( ptr1, 4096, "CALENDAR MERGE %s,TYPE %s,YEAR %s,FORMAT DAYS %s \"%s\"",
                            calbuffer->calendar_name, caltype,
                            calbuffer->year, monthname, datebuffer );
                     }
                     printf( "%s\n", ptr1 );
				  }
				  else {
					   printf( "#    Entry %d discarded for year %s, programmer error\n", i, calbuffer->year );
				  }
			   } /* days for month > 0 */
               /* else no entries for this month, do nothing. used to have a
                * printf line saying so but it just wasted space. */
		    } /* 0-11 months */
		 } /* if not discarded */
      } /* if not a deleted entry */
   } /* while no errors and not eof on input */

   /* free the memory we have allocated */
   free( membuffer );
   free( calbuffer );

   /* close the file we opened */
   fclose( calendar_handle );

   if (calendar_count == 0) {
      printf( "# There were calendar entries in the database %s\n", calendarfile );
   }

   /* All done */
   return( 0 );
} /* CALENDAR_dump_calendar_file */

/* ---------------------------------------------------
   --------------------------------------------------- */
int main(int argc, char **argv) {
		char directory_name[MAX_DIR_LEN+1];
		char job_database_name[MAX_CHAR_LEN+1];
		char config_database_name[MAX_CHAR_LEN+1];
		char user_database_name[MAX_CHAR_LEN+1];
		char calendar_database_name[MAX_CHAR_LEN+1];
		int  i, errorcount;
		char *ptr;

        /* The below two flags are used in the UTILS and API library so we
	     * need to set them. These are global variables in config.h */
	    pSCHEDULER_CONFIG_FLAGS.log_level = 0;
		pSCHEDULER_CONFIG_FLAGS.debug_level.utils = 0;
		/* This is also used by utils */
	    msg_log_handle = stdout;
		MEMORY_initialise_malloc_counters();

		errorcount = 0;

		if (argc >= 2) {
				/* Get the directory, and ensure it has a trailing / on it */
				strncpy( directory_name, argv[1], MAX_DIR_LEN );
				ptr = (char *)&directory_name;
				i = strlen(directory_name);
				ptr = ptr + (i - 1);
				if (*ptr != '/') {
					strcat( directory_name, "/" );
				}
				common_banner( directory_name );
                snprintf( job_database_name, MAX_CHAR_LEN, "%sjob_details.dbs", directory_name );
                snprintf( config_database_name, MAX_CHAR_LEN, "%sconfig.dat", directory_name );
                snprintf( user_database_name, MAX_CHAR_LEN, "%suser_records.dbs", directory_name );
                snprintf( calendar_database_name, MAX_CHAR_LEN, "%scalendar.dbs", directory_name );
				errorcount = errorcount + CONFIG_dump_config_file( (char *)&config_database_name );
				errorcount = errorcount + CALENDAR_dump_calendar_file( (char *)&calendar_database_name );
				errorcount = errorcount + JOBS_dump_job_file( (char *)&job_database_name );
				errorcount = errorcount + USER_dump_user_file( (char *)&user_database_name );
				end_banner( directory_name );
		}
		else {
				printf( "You need to provide me with the name of the scheduler database directory to snapshot from\n" );
				printf( "Example: jobsched_take_snapshot /opt/dickinson/scheduler\n" );
				errorcount = 1;
		}
        exit( errorcount );
} /* end main */
