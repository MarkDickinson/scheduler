/*
    JOBSCHED_SECURITY_CHECKS.C

	Check user profiles are still legal, and jobs are secured correctly.
	Report on any exceptions.

    Scheduler is by Mark Dickinson, 2001-2011

    This program is intended to do two things...
    1. Ensure command files to be run by the scheduler are in fact owned
       by the userid the scheduler will run them as; and that the file
       permissions on the command files prevent anyone else 'slipping in'
       commands.
    2. That the users in the user database are still active users.
*/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include "scheduler.h"
#include "userrecdef.h"

#define MAX_CHAR_LEN 254
#define MAX_DIR_LEN (MAX_CHAR_LEN - 15)

/* Global needed to use the utils library */
FILE *msg_log_handle = NULL;


/* ---------------------------------------------------
 * convert_logintime_to_timestamp:
 *
 * only called from the user check function. The user
 * record stores the last login time in the form
 * yyyymmdd hh:mm:ss, we need it converted to a valid
 * timestamp of number of seconds since Jan 1 1970, 00:00
 * so we can compare the last login times against the
 * current time returned by time().
 *
 * This procedure does the above.
 * Returns
 *   last login time as a timestamp if OK.
 *   0 in the timestamp is we couldn't malloc.
   --------------------------------------------------- */
time_t convert_logintime_to_timestamp( char * timestr /* yyyymmdd hh:mm:ss */ ) {
    char * ptr1;
    char smallbuf[20];
    time_t time_number;
    struct tm *time_var;
    time_var = malloc(sizeof *time_var);
    if (time_var == NULL) {
       printf( "*ERR: Unable to allocate memory for last login checks\n" );
       return( 0 );  /* not possible */
    }
    ptr1 = timestr;
    strncpy(smallbuf,ptr1,4);
    smallbuf[4] = '\0';
    time_var->tm_year = atoi( smallbuf );
    ptr1 = ptr1 + 4;
    strncpy(smallbuf,ptr1,2);
    smallbuf[2] = '\0';
    time_var->tm_mon = atoi( smallbuf );
    ptr1 = ptr1 + 2;
    strncpy(smallbuf,ptr1,2);
    smallbuf[2] = '\0';
    time_var->tm_mday = atoi( smallbuf );
    ptr1 = ptr1 + 3;
    strncpy(smallbuf,ptr1,2);
    smallbuf[2] = '\0';
    time_var->tm_hour = atoi( smallbuf );
    ptr1 = ptr1 + 3;
    strncpy(smallbuf,ptr1,2);
    smallbuf[2] = '\0';
    time_var->tm_min = atoi( smallbuf );
    time_var->tm_sec = 0;
    /* Adjust the times we provided to what the structure expects */
    time_var->tm_year = time_var->tm_year - 1900;  /* years returned as count from 1900 */
    time_var->tm_mon = time_var->tm_mon - 1;       /* months returned as 0-11 */
    /* set these to 0 for now */
    time_var->tm_wday = 0;   /* week day */
    time_var->tm_yday = 0;   /* day in year */
    time_var->tm_isdst = 0;  /* a 60 day check, who cares about an hour each way */
    /* and workout the time */
    time_number = mktime( time_var );
    /* done, free memory */
    free( time_var );
    return( time_number );
} /* convert_logintime_to_timestamp */


/* ---------------------------------------------------
 * remove_trailing_spaces:
 *
 * just what it says. will scan back over any trailing
 * spaces padding out a string, and place a null after
 * the last non-space character in the string.
 *
 * Returns
 *  The string value passed as a pointer is updated.
   --------------------------------------------------- */
void remove_trailing_spaces( char *sptr ) {
   char *ptr1;
   int strlength;
   strlength = strlen(sptr);
   if (strlength < 1) {
      return;
   }
   ptr1 = (sptr + strlength) - 1;
   while ( (ptr1 > sptr) && (*ptr1 == ' ') ) {
      ptr1--;
   }
   ptr1++;
   *ptr1 = '\0';
} /* remove_trailing_spaces */


/* ---------------------------------------------------
 * print_user_name:
 *
 * Will print the name of the user associated with the
 * user number passed.
 *
 * Returns
 *   Nothing, output is written.
   --------------------------------------------------- */
void print_user_name( uid_t userid ) {
  struct passwd *local_pswd_rec;
   /* this call mallocs a passwd structure for us */
   if ( (local_pswd_rec = getpwuid( userid )) == NULL ) {
       printf( "unknown (not in passwd file)\n" );
   }
   else {
      printf( "%s\n", local_pswd_rec->pw_name );
   }
} /* print_user_name */


/* ---------------------------------------------------
 * check_user_exists:
 *
 * Used to return the uid number of a user passed to it.
 *
 * Returns
 *   -1 if the username passed is not in the password
 *      file for the current system.
 *  uid number if the user was found.
   --------------------------------------------------- */
uid_t check_user_exists( char * username ) {
  struct passwd *local_pswd_rec;
   /* this call mallocs a passwd structure for us */
   if ( (local_pswd_rec = getpwnam( username )) == NULL ) {
       return( -1 );
   }
   else {
      return( local_pswd_rec->pw_uid );
   }
} /* check_user_exists */


/* ---------------------------------------------------
 * check_banner:
 *
 * a helper proc for the check_job_file function.
 * It will write a banner for a job being checked if
 * one has not already been written.
 *
 * Returns
 *   1 to indicate banner is written
   --------------------------------------------------- */
int check_banner( int banner_done, char * jobname, char * filename, char * userid ) {
   if (banner_done == 0) {
      printf( "Job record %s\n  Command %s\n  Job runs under userid %s\n", jobname, filename, userid );
   }
   return( 1 );
} /* check_banner */


/* ---------------------------------------------------
 * check_file_permissions:
 *
 * Does most of the work for check_job_file.
 *
 * We check that the owner of the job (the user the
 * job runs as) is still on the system. This is a
 * little redundant as the scheduler itself does that,
 * but this report will advise if the user is no more.
 *
 * We check the physical command/script file on disk
 * that the job will run to ensure that
 *   1. The userid the the job scheduler is to run the
 *      job as is the owner of the file.
 *   2. The userid that the job scheduler is to run the
 *      job as is the only user that can update the file.
 * These checks provide a degree of confidence that commands
 * cannot be 'slipped' into poorly secured files to run
 * under restricted userids.
 *
 * Returns
 *   0 = no errors
 *   1 = file permissions error, or file error
 *   2 = file owner error
 *   3 = both in error
   --------------------------------------------------- */
int check_file_permissions( char * filename, char * userid, char * jobname ) {
   int return_code;
   int banner_written;
   uid_t userid_num;
   struct stat stbuf;

   return_code = 0; /* start with no error */
   banner_written = 0; /* no banner needed yet */

   /* The manual indicates it returns -1 on an error. It
    * doesn't. Assume an error if <> 0. */
   if (stat( filename, &stbuf ) != 0) {
      banner_written = check_banner( banner_written, jobname, filename, userid );
      printf( "    *** %s does not exist or permission denied\n", filename );
	  return_code = return_code + 1;
   }

   userid_num = check_user_exists( userid );
   if (userid_num < 0) {
      banner_written = check_banner( banner_written, jobname, filename, userid );
      printf( "    *** Job owner %s is not a valid user for the server\n", userid );
	  return_code = return_code + 2;
   }

   if (return_code != 0) {
      return( return_code );
   }

   /* if we are here, both the user and file exist so we have to check
    * that the file is tightly secured to the user. */
   if (stbuf.st_uid != userid_num) {
      banner_written = check_banner( banner_written, jobname, filename, userid );
      printf( "    *** Job command file is not owned by %s, owned by ", userid );
      print_user_name( stbuf.st_uid );
	  return_code = return_code + 2;
   }

   /* get the three octal bits for the file permission only, tested OK */
   stbuf.st_mode = stbuf.st_mode & 511;
   /* leave only the group and other write bits now */
   stbuf.st_mode = stbuf.st_mode & 18;
   if (stbuf.st_mode != 0) {  /* group or other also has write permissions */
      if (return_code >= 2) { return_code = 3; } else { return_code = 1; }
      banner_written = check_banner( banner_written, jobname, filename, userid );
      printf( "  *** Command file secured badly, non-owner users can change the file\n", filename );
   }

   if (banner_written != 0) { /* reported some errors */
      printf( "  note: file last modified %s\n", ctime( (time_t *)&stbuf.st_mtime ) );
   }

   return( return_code );
} /* check_file_permissions */


/* ---------------------------------------------------
 * check_job_file:
 *
 * Read through the jobs file and call check_file_permissions
 * for each active job record to ensure that the command file
 * being run by the job is as secure as it can be.
 *
 * Notes:
 * - skips deleted records
 * - checks each record has a valid job state flag to ensure
 *   what we are processing really is a job file.
 *
 *   Returns
 *     0 if no problems found
 *     non-zero if there were alerts/errors reported on.
   --------------------------------------------------- */
int check_job_file( char * jobsfile ) {
   int items_read, badfile, errors_found;
   FILE *job_handle = NULL;
   jobsfile_def * jobbuffer;

   errors_found = 0;

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

   /* -- Do all the work now, loop through the job file writing out the
	*    entries we read from it, ignore any deleted oned though. */
   clearerr( job_handle );
   badfile = 0;
   fseek( job_handle, 0, SEEK_SET );
   while ( (!(ferror(job_handle))) && (!(feof(job_handle))) && (badfile == 0) ) {
      items_read = fread( jobbuffer, sizeof(jobsfile_def), 1, job_handle );
      if ( (items_read > 0) &&
           (jobbuffer->job_header.info_flag != 'A') &&
           (jobbuffer->job_header.info_flag != 'S') &&
           (jobbuffer->job_header.info_flag != 'D') ) {
         badfile = 1;
         printf( "------> E R R O R <----- file %s does not appear to be a legal job data file (bad data in record)\n", jobsfile );
		 errors_found++;
      }
	  else if ( (items_read > 0) && (jobbuffer->job_header.info_flag != 'D') ) {
         remove_trailing_spaces( jobbuffer->job_header.jobname );
         check_file_permissions( jobbuffer->program_to_execute, jobbuffer->job_owner, jobbuffer->job_header.jobname );
	  } /* if not a deleted entry */
   } /* while no errors and not eof on input */

   /* free the memory we have allocated */
   free( jobbuffer );

   /* close the file we opened */
   fclose( job_handle );
   /* All done */
   return( errors_found );
} /* check_job_file */


/* ---------------------------------------------------
 * check_user_file:
 *
 * basically we just scan through the user file with
 * two objectives here, both to determine is a user 
 * record is no longer required due to not being used
 * 
 * 1. If the user has autologin privalidges there should
 *    be a unix level login userid for the user. If there
 *    is not the user may have left and been deleted from
 *    the unix server. They can still logon to the job
 *    scheduler from any other networked server if they
 *    are still in the job schduler user file so we report
 *    this condition as an error.
 * 2. If the user has not logged on to the job scheduler
 *    in the last 60 days, report it as an error. They
 *    may need to be deleted.
 *
 * Returns
 *   0 if no problems were found
 *   non-zero if something was reported.
   --------------------------------------------------- */
int check_user_file( char * userfile ) {
   int items_read, errors_found, user_banner_written;
   char autologin_mask[12];
   time_t last_logged_in, time_now;
   FILE *user_handle = NULL;
   USER_record_def * userbuffer;

   user_banner_written = 0;
   strcpy( autologin_mask, "auto-login" ); /* sothis way so we can verify null also in memcmp */

   /* Open the user file we are to read from */
   if ( (user_handle = fopen( userfile, "rb" )) == NULL ) {
	  perror( "userfile" );
      printf( "*ERR: Unable to open user database file %s for user dumping, err=%d\n",
              userfile, errno );
	  return( 1 );
   }

   /* Allocate memory for the user records */
   if ( (userbuffer = malloc(sizeof(USER_record_def))) == NULL) { 
      printf(" *ERR: Unable to allocate %d bytes of memory for user checks\n", sizeof(USER_record_def) );
      fclose( user_handle );
      return( 1 );
   }

   /* -- Do all the work now, loop through the user file writing out the
	*    entries we read from it, ignore any deleted ones though. */
   time_now = time(0);
   errors_found = 0;
   clearerr( user_handle );
   fseek( user_handle, 0, SEEK_SET );
   while ( (!(ferror(user_handle))) && (!(feof(user_handle))) ) {
      items_read = fread( userbuffer, sizeof(USER_record_def), 1, user_handle );
      if ( (items_read > 0) && (userbuffer->record_state_flag != 'D') ) {
         /* --- check the version flag, we use this to decide if we are
          * reading from a valis user file as the initial filesize test */
         if (memcmp(userbuffer->userrec_version, USER_LIB_VERSION, 2) != 0) {
            free( userbuffer );
            fclose( user_handle );
			printf( "------> E R R O R <----- file %s does not appear to be a legal user data file (bad version header)\n", userfile );
			return( 1 );
		 }

         remove_trailing_spaces( userbuffer->user_name );
         remove_trailing_spaces( userbuffer->last_logged_on );
         last_logged_in = convert_logintime_to_timestamp( userbuffer->last_logged_on );
	 if ( (time_now -last_logged_in) > (60 * 60 * 24 * 60) ) { /* secs, mins, hours, days */
            if (user_banner_written == 0) {
               printf( "Suspect USER records found in %s\n", userfile );    
               user_banner_written = 1;
            }
            if (strlen(userbuffer->last_logged_on) == 0) {
               strcpy( userbuffer->last_logged_on, "NEVER" );
            }
            printf( "  *** User %s has not logged in in 60 days, last login was %s\n",
                    userbuffer->user_name, userbuffer->last_logged_on );
            remove_trailing_spaces( userbuffer->user_details );
            printf( "      %s details : %s\n", userbuffer->user_name, userbuffer->user_details );
            errors_found++;
         } /* last logged in checks */

	 if (userbuffer->local_auto_auth == 'Y') { /* should have a matching unix signon */
            if (memcmp(userbuffer->user_name,"auto-login",11) != 0) { /* for 11 to check null also */
               if (check_user_exists( userbuffer->user_name ) == -1 ) {
                  if (user_banner_written == 0) {
                     printf( "Suspect USER records found in %s\n", userfile );    
                     user_banner_written = 1;
                  }
                  printf( "  *** User %s has AUTOAUTH=Y, but there is no matching unix user record\n",
                          userbuffer->user_name );
                  errors_found++;
               }
            } /* if not the auto-login */
         } /* autoauth checks */

      } /* if not a deleted entry */
   } /* while no errors and not eof on input */

   /* free the memory we have allocated */
   free( userbuffer );

   /* close the file we opened */
   fclose( user_handle );
   /* All done */
   return( errors_found );
} /* check_user_file */


/* ---------------------------------------------------
 *                 M A I N L I N E
 *
 * Yes there is one, bet you are supprised after all
 * the pre-work going on above.
 *
 *
   --------------------------------------------------- */
int main(int argc, char **argv) {
		char directory_name[MAX_DIR_LEN+1];
		char job_database_name[MAX_CHAR_LEN+1];
		char user_database_name[MAX_CHAR_LEN+1];
		int  i, errorcount;
		char *ptr;

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
                snprintf( job_database_name, MAX_CHAR_LEN, "%sjob_details.dbs", directory_name );
                snprintf( user_database_name, MAX_CHAR_LEN, "%suser_records.dbs", directory_name );
				if ( check_job_file( (char *)&job_database_name ) != 0 ) { errorcount++; }
				if ( check_user_file( (char *)&user_database_name ) != 0 ) { errorcount++; }
		}
		else {
				printf( "*** ERROR *** Missing parameters for scheduler jobsched_security_checks task.\n" );
				printf( "You need to provide me with the name of the scheduler database directory to snapshot from\n" );
				printf( "Example: jobsched_security_checks /opt/dickinson/scheduler\n" );
				errorcount = 1;
		}

   /* we either exit 0 (no problems) or 1 (problems). As this program
	* is intended to be run regularly from a script we expect that
	* script to take action (mail-out etc.) on a non-zero. We don't
	* do that ourself as this could be run interactively as well and
	* who wants to fill up mail queues. */
   if (errorcount > 0) { errorcount = 1; } /* only exit 0 or 1 */
   exit( errorcount );
} /* end main */
