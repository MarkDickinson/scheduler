#include <stdlib.h>
#include <string.h>
#include "debug_levels.h"
#include "api.h"
#include "scheduler.h"
#include "utils.h"
#include "schedlib.h"
#include "users.h"

/* ---------------------------------------------------------
   Lots of checks to make sure the API buffer is valid.
   Return 0 if a bad API buffer, length of buffer if OK.
   Yet to be implemented so return 1 for now.
   -------------------------------------------------------- */
int BULLETPROOF_API_Buffer( API_Header_Def *pApibuffer ) {
   char *sptr, *endptr;
   int  totalreclen, variabledatalen;
   char workbuff[DATA_LEN_FIELD+1];

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.bulletproof >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered BULLETPROOF_API_Buffer\n" );
   }

   sptr = (char *)pApibuffer;
   endptr = (char *)&pApibuffer->API_Data_Buffer;
   totalreclen = (int)(endptr - sptr);

   strncpy( workbuff, pApibuffer->API_Data_Len, DATA_LEN_FIELD );
   workbuff[DATA_LEN_FIELD] = '\0';   /* as strncpy doesnt do it if string > copylen */
   variabledatalen = atoi( workbuff );
   
   if (variabledatalen > 4095) {
		   myprintf( "*ERR: BE001-INTERNAL ERROR IN BULLETPROOF_API_Buffer* variable datalen = %d\n", variabledatalen );
		   myprintf( "*ERR: BE002-   This is obviously in error, so resetting to 10 to assist debugging\n" );
		   myprintf( "*ERR: BE003-   it was worked out from length string '%s'", workbuff );
		   variabledatalen = 10;
   }
   totalreclen = totalreclen + variabledatalen;
   
   /* adjust the end pointer to allow for variable data */
   endptr = endptr + variabledatalen;

   /* pass through the buffer ensuring the only \0 or \n 
    * is the last character in the buffer.
    * */
   while (sptr < endptr) {
      if ((*sptr == '\n') || (*sptr == '\0')) {
	     *sptr = '^';
	  }
      sptr++;
   }
   
   *endptr = '\n';
   endptr++;
   endptr = '\0';

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.bulletproof >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving BULLETPROOF_API_Buffer\n" );
   }

   return( totalreclen + 1 ); /* +1 to include the null WHICH IS NEEDED */
} /* BULLETPROOF_API_Buffer */

/* ---------------------------------------------------------
   Lots of checks to make sure a jobsfile_def record for
   the job database is valid.
   Return 0 if a bad record, 1 if OK.
   WARNING: Only partial checks implemented at present.
   -------------------------------------------------------- */
int BULLETPROOF_jobsfile_record( jobsfile_def *pJobrec ) {
   int i, j, result, local_requeue_mins;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.bulletproof >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered BULLETPROOF_jobsfile_record\n" );
   }

   result = 1;  /* default is all ok */

   UTILS_space_fill_string( pJobrec->job_header.jobname, JOB_NAME_LEN );
   UTILS_space_fill_string( pJobrec->job_header.jobnumber, JOB_NUMBER_LEN );
   if ( (pJobrec->job_header.info_flag != 'A') &&   
        (pJobrec->job_header.info_flag != 'D') &&   
        (pJobrec->job_header.info_flag != 'S') ) {
	   myprintf( "*ERR: BE004-BULLETPROOF_jobsfile_record: Invalid info_flag in the job record definition\n" );
	   result = 0;
   }
   if ( (pJobrec->use_calendar != '0') && (pJobrec->use_calendar != '1') &&
        (pJobrec->use_calendar != '2') && (pJobrec->use_calendar != '3') ) {
	   myprintf( "*ERR: BE005-BULLETPROOF_jobsfile_record: Invalid use_calendar (%c) in the job record definition for %s\n",
               pJobrec->use_calendar, pJobrec->job_header.jobname );
	   result = 0;
   }
   if (pJobrec->use_calendar == '2') {
		   j = 0;
		   for (i = 0; i < 7; i++ ) {
				   if (pJobrec->crontype_weekdays[i] == '1') { j++; }
		   }
           if (j == 0) {
				   myprintf( "*ERR: BE006-BULLETPROOF_jobsfile_record: user_calendar set to use days, but no days selected\n" );
				   result = 0;
		   }
   }
   /* New check, was slipping through as 0 and causing an endless loop */
   if (pJobrec->use_calendar == '3') { /* repeating job */
        local_requeue_mins = atoi( pJobrec->requeue_mins );
        if (local_requeue_mins < 5) {
		   myprintf( "*ERR: BE023-BULLETPROOF_jobsfile_record: repeating job requeue interval is < 5\n" );
		   result = 0;
        }
		else if (local_requeue_mins > JOB_REPEAT_MAXVALUE) {
		   myprintf( "*ERR: BE024-BULLETPROOF_jobsfile_record: repeating job requeue interval is > %d\n",
                     JOB_REPEAT_MAXVALUE );
		   result = 0;
        }
   }
   /* new field added 1 Sep 2002, default behaviour for before this field was
    * that all jobs catchup so that will be the default. */
   if ( (pJobrec->job_catchup_allowed != 'Y') && (pJobrec->job_catchup_allowed != 'N') ) {
      pJobrec->job_catchup_allowed = 'Y';  /* default */
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.bulletproof >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving BULLETPROOF_jobsfile_record\n" );
   }
   return( result );
} /* BULLETPROOF_jobsfile_record */

/* ---------------------------------------------------------
   -------------------------------------------------------- */
int BULLETPROOF_dependency_record( dependency_queue_def *datarec ) {
   int i;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.bulletproof >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered BULLETPROOF_dependency_record\n" );
   }

   UTILS_space_fill_string( datarec->job_header.jobname, JOB_NAME_LEN );
   UTILS_space_fill_string( datarec->job_header.jobnumber, JOB_NUMBER_LEN );
   if (
				   ( datarec->job_header.info_flag != 'A' ) &&
				   ( datarec->job_header.info_flag != 'D' ) &&
				   ( datarec->job_header.info_flag != 'S' )
      )
   {
		   datarec->job_header.info_flag = 'D';
		   myprintf( "*ERR: BE007-BULLETPROOF_dependency_record: bad header info flag, set to DELETED\n" );
   }
   for (i = 0; i < MAX_DEPENDENCIES; i++ ) {
      if (datarec->dependencies[i].dependency_type != '0') {
         UTILS_space_fill_string( datarec->dependencies[i].dependency_name,
						          JOB_DEPENDTARGET_LEN );
		 if ( (memcmp(datarec->dependencies[i].dependency_name,
					  "  ", 2 )
			  ) == 0 ) {
		    myprintf( "*ERR: BE008-BULLETPROOF_dependency_record: Null dependecy key index %d, turning it off\n", i );
		    datarec->dependencies[i].dependency_type = '0';	
	        datarec->dependencies[i].dependency_name[0] = '\0';
		 }
	  }
	  else {
	     datarec->dependencies[i].dependency_name[0] = '\0';
	  }
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.bulletproof >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving BULLETPROOF_dependency_record\n" );
   }

   return( 1 ); /* OK */
} /* BULLETPROOF_dependency_record */

/* ---------------------------------------------------------
 * Check a user file record for sanity.
 * Return 1 if OK, 0 if errors.
 * NOTES: If serverside_checks is set then it is assumed this
 *        check is being called by the server itself before
 *        adding the user, so we check to ensure that the
 *        user actually exists on the system the server is
 *        running on. If it's not set we assume we are
 *        called by a task remote to the server system
 *        (ie: by a jobsched_cmd on a remote system) so
 *        we cannot check those fields.
   -------------------------------------------------------- */
int BULLETPROOF_user_record( USER_record_def * userrec, int serverside_checks ) {
   int error_count, i, j;
   uid_t uid_name;
   char usrname[USER_MAX_NAMELEN+1];
   char *ptr;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.bulletproof >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered BULLETPROOF_user_record\n" );
   }

   error_count = 0;
   strcpy( userrec->userrec_version, "01" );
   i = strlen(userrec->user_name);
   if ( (i < 3) || (i > USER_MAX_NAMELEN) ) {
       error_count++;
       myprintf( "*ERR: BE009-User name is not legal\n" );
   }
   else {
      UTILS_space_fill_string( userrec->user_name, USER_MAX_NAMELEN );
   }
   i = strlen(userrec->user_password);
   if ( (i < 6) || (i > USER_MAX_PASSLEN) ) {
       error_count++;
       myprintf( "*ERR: BE010-User password is not legal\n" );
   }
   else {
      UTILS_space_fill_string( userrec->user_name, USER_MAX_PASSLEN );
   }
   if ((userrec->record_state_flag != 'A') && (userrec->record_state_flag != 'D')) {
       error_count++;
       myprintf( "*ERR: BE011-User record state flag is not A or D\n" );
   }
   if ((userrec->user_auth_level != 'O') && 
       (userrec->user_auth_level != 'A') && 
       (userrec->user_auth_level != '0') && 
       (userrec->user_auth_level != 'J') && 
       (userrec->user_auth_level != 'S')) {
       error_count++;
       myprintf( "*ERR: BE012-User authority level is not legal\n" );
   }

   if (userrec->user_auth_level == 'A') {
      strcpy( userrec->user_list[0].userid, "*" ); /* admin has all users always */
   }

   if ((userrec->local_auto_auth != 'Y') && 
       (userrec->local_auto_auth != 'N')) {
       error_count++;
       myprintf( "*ERR: BE013-Local auto-login authority is not legal\n" );
   }
   if ((userrec->allowed_to_add_users != 'Y') && 
       (userrec->allowed_to_add_users != 'N') &&
       (userrec->allowed_to_add_users != 'S')) {
       error_count++;
       myprintf( "*ERR: BE014-Local allowed_to_add_users setting is not legal\n" );
   }
   i = strlen(userrec->user_details);
   if ( (i < 3) || (i > 49) ) {
       error_count++;
       myprintf( "*ERR: BE015-Meaningfull info in user details must be provided\n" );
   }
   for (j = 0; j < 10; j++ ) {  /* check the users the record can control jobs for fields */
      i = strlen(userrec->user_list[j].userid);
      ptr = (char *)&userrec->user_list[j].userid;
      if ((*ptr == '*') || (*ptr == '-')) { /* user masks acceptable */
         /* OK, no action needed */
      }
      else if (*ptr == '\0') { /* translate nulls to the - entry */
          *ptr = '-';
          ptr++;
          *ptr = '\0';
      }
      else if (serverside_checks != 0) { /* if we are doing server side checks the user must exist on server system */
          strncpy(usrname, userrec->user_list[j].userid, USER_MAX_NAMELEN);
          ptr = (char *)&usrname;
          i = 0;
          while ((*ptr != ' ') && (i < USER_MAX_NAMELEN)) { ptr++; }
          *ptr = '\0';
          uid_name = UTILS_obtain_uid_from_name( (char *)&usrname );
          if (uid_name == -1) {  /* no user of that name */
             error_count++;
             myprintf( "*ERR: BE016-In user add, uid table check, user %s does not exist on server\n" );
          }
      }
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.bulletproof >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving BULLETPROOF_user_record\n" );
   }
   if (error_count == 0) {
       return( 1 );  /* passed checks */
   }
   else {
       return( 0 );  /* failed checks */
   }
} /* BULLETPROOF_user_record */


/* ---------------------------------------------------------
   -------------------------------------------------------- */
void BULLETPROOF_USER_init_record_defaults( USER_record_def * local_user_rec ) {
   int i;

   strcpy( local_user_rec->userrec_version, USER_LIB_VERSION );
   strcpy( local_user_rec->user_name, "guest" );
   UTILS_space_fill_string( local_user_rec->user_name, USER_MAX_NAMELEN );
   strcpy( local_user_rec->user_password, "" );
   UTILS_space_fill_string( local_user_rec->user_password, USER_MAX_PASSLEN );
   local_user_rec->record_state_flag = 'A';
   local_user_rec->user_auth_level = '0';
   local_user_rec->local_auto_auth = 'N';
   local_user_rec->allowed_to_add_users = 'N';
   for ( i = 0; i < USERS_MAX_USERLIST; i++ ) {
      strcpy( local_user_rec->user_list[i].userid, "-" );
      UTILS_space_fill_string( local_user_rec->user_list[i].userid, USER_MAX_NAMELEN );
   }
   strcpy( local_user_rec->user_details, "Empty record" );
   UTILS_space_fill_string( local_user_rec->user_details, 49 );
} /* BULLETPROOF_USER_init_record_defaults */

/* ---------------------------------------------------------
   -------------------------------------------------------- */
int BULLETPROOF_CALENDAR_record( calendar_def * datarec ) {

   int checkcount, failed, i, j;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.bulletproof >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered BULLETPROOF_CALENDAR_record\n" );
   }

   failed = 0;  /* havn't failed yet */

   /*
    * Check the fields in the record are legal.
    */
   UTILS_space_fill_string( datarec->calendar_name, CALENDAR_NAME_LEN );
   if ( ( datarec->calendar_type != '0') &&
        ( datarec->calendar_type != '1') &&
        ( datarec->calendar_type != '9') ) {
      myprintf("*ERR: BE017-Illegal calendar type value\n" );
      failed++;  /* failed checks */
   }
   checkcount = 0;
   for (i = 0; i < 12; i++) {
      for (j = 0; j < 31; j++) {
         if (datarec->yearly_table.month[i].days[j] == 'Y') { checkcount++; }
      }
   }
   if (checkcount == 0) {
      myprintf( "*ERR: BE018-No days were selected for the calendar to run on\n" );
      failed++;
   }

   if (datarec->observe_holidays == 'Y') {
      if (*datarec->holiday_calendar_to_use == '\0') {
         myprintf("*ERR: BE019-Job is to observe holidays but no holiday calendar was selected\n" );
         failed++; /* failed checks */
      }
   }
   /* need to change this to actually check the year */
   if (strlen(datarec->year) < 4) {
      myprintf("*ERR: BE020-The year in the calendar is not legal.\n" );
      failed++; /* failed checks */
   }
   /* description is optional, but check if there is nothing there is
	* no field overrun */
   if (strlen(datarec->description) > CALENDAR_DESC_LEN) {
      myprintf("*ERR: BE022-The description field is not initialised, clearing it.\n" );
	  strcpy( datarec->description, "" );  /* clear in case the caller is actually using it for display */
      failed++; /* failed checks */
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.bulletproof >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving BULLETPROOF_CALENDAR_record\n" );
   }

   if (failed != 0) {
      myprintf("*ERR: BE021-Calendar record sanity checks failed, request discarded !\n" );
      return( 0 );  /* one or more checks failed */
   }
   else {
      return( 1 );  /* all checks were passed */
   }
} /* BULLETPROOF_CALENDAR_record */


/* ---------------------------------------------------------
   -------------------------------------------------------- */
void BULLETPROOF_CALENDAR_init_record_defaults( calendar_def * datarec ) {
   int i, j;
   strcpy( datarec->calendar_name, "DEFAULT" );
   datarec->calendar_type = '0';
   strcpy( datarec->year, "0000" );  /* 0 will allow some year sanity checks to fail if no real year provided */
   datarec->observe_holidays = 'N';
   strcpy( datarec->holiday_calendar_to_use, "" );
   strcpy( datarec->description, "Uninitialised entry" );
   for (i = 0; i < 12; i++ ) {
		   for (j = 0; j < 31; j++) {
				   datarec->yearly_table.month[i].days[j] = 'N';
		   }
   }
} /* BULLETPROOF_CALENDAR_init_record_defaults */
