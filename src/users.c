#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "api.h"
#include "scheduler.h"
#include "users.h"
#include "utils.h"
#include "schedlib.h"
#include "debug_levels.h"
#include "bulletproof.h"
#include "memorylib.h"

/* ----------------------------------------------------------------------
 * The auto-login user must always exist. Some care has been taken
 * to ensure the server cannot delete the user but it's not beyond
 * the scope os a security department to figure out how to use it so
 * we always check and add it back in again if it has been deleted.
 *
 * The guest user is a nice to have. Users are by default guests
 * until they login, this record provides a default document of
 * the rules associated with guests. It also enforces the rules
 * if someone logs on as guest so this record should exist.
 *
 * Return 0 if errors, 1 if OK
 * ---------------------------------------------------------------------- */
int USER_check_required_users( void ) {
   USER_record_def * local_user_rec;
   int error_count;

   error_count = 0;  /* no failures yet */

   /* allocate some buffer space */

   if ( (local_user_rec = (USER_record_def *)MEMORY_malloc(sizeof(USER_record_def),"USER_check_required_users")) == NULL) {
       myprintf( "*ERR: UE001-Insufficient memory to open user file (USER_Initialise)\n" );
       perror( "Insuficient free memory" );
       return 0;
   }

   /* Check the auto-login user exists, create if not */

   strcpy( local_user_rec->user_name, "auto-login" );
   UTILS_space_fill_string( (char *)&local_user_rec->user_name, USER_MAX_NAMELEN );
   if (USER_read( local_user_rec ) == -1) {
      USER_init_record_defaults( local_user_rec );
      strcpy( local_user_rec->user_name, "auto-login" );
      UTILS_space_fill_string( local_user_rec->user_name, USER_MAX_NAMELEN );
      strcpy( local_user_rec->user_password, "" );  /* empty */
      strcpy( local_user_rec->user_details, "root auto-login user" );
      UTILS_space_fill_string( local_user_rec->user_details, 49 );
      local_user_rec->user_auth_level = 'A';
      local_user_rec->allowed_to_add_users = 'S'; /* full security authority */
      local_user_rec->local_auto_auth = 'Y';
      strcpy( local_user_rec->user_list[0].userid, "*" );
      strcpy( local_user_rec->last_logged_on, "00000000 00:00:00" );
      UTILS_space_fill_string( local_user_rec->user_list[0].userid, USER_MAX_NAMELEN );
      if (USER_add(local_user_rec) != 1) {
          myprintf( "*ERR: UE002-Unable to verify the auto-login user, file %s (USER_check_required_users)\n", user_file );
          error_count++;
       }
    }

   /* check the default guest user exists */
   strcpy( local_user_rec->user_name, "auto-login" );
   UTILS_space_fill_string( (char *)&local_user_rec->user_name, USER_MAX_NAMELEN );
   if (USER_read( local_user_rec ) == -1) {
      strcpy( local_user_rec->user_name, "guest" );
      UTILS_space_fill_string( local_user_rec->user_name, USER_MAX_NAMELEN );
      strcpy( local_user_rec->user_password, "guest" );  
      UTILS_space_fill_string( local_user_rec->user_password, USER_MAX_PASSLEN );
      strcpy( local_user_rec->user_details, "Default guest profile" );
      UTILS_space_fill_string( local_user_rec->user_details, 49 );
      local_user_rec->user_auth_level = '0';
      local_user_rec->local_auto_auth = 'N';
      local_user_rec->allowed_to_add_users = 'N';
      strcpy( local_user_rec->last_logged_on, "00000000 00:00:00" );
      if (USER_add(local_user_rec) != 1) {
          myprintf( "*ERR: UE003-Unable to verify the guest user, file %s (USER_check_required_users)\n", user_file );
          error_count++;
      }
   }

   /* free the memory we allocated */

   MEMORY_free( local_user_rec );

   /* see if there were any errors */

   if (error_count > 0) {
      return 0;
   }
   else {
      return 1;
   }
} /* USER_check_required_users */

/* ----------------------------------------------------------------------
 * ---------------------------------------------------------------------- */
int  USER_Initialise( void ) {
   USER_local_auth = 1;  /* Use local authorities */
   strcpy( USER_remote_auth_addr, "127.0.0.1" ); /* ignored in local auth but set to a default */

   if ( (user_handle = fopen( user_file, "rb+" )) == NULL ) {
	  if (errno == 2) {
         if ( (user_handle = fopen( user_file, "ab+" )) == NULL ) {
            myprintf( "*ERR: UE004-Unable to create user file %s, errno %d (USER_Initialise)\n", user_file, errno );
            perror( "Open of user file failed" );
            return 0;
		 }
		 else {
            /* Then close and open it the way we need it */
		    fclose( user_handle );
			user_handle = NULL;
		    return( USER_Initialise() );
		 }
	  }
	  else {
         myprintf( "*ERR: UE005-Unable to open user file %s, errno %d (USER_Initialise)\n", user_file, errno );
         perror( "Open of user file failed" );
         return 0;
	  }
   }
   else {
      if (USER_check_required_users() != 1) {
         myprintf( "*ERR: UE006-Required system users were unable to be verified (USER_Initialise)\n", user_file, errno );
         return 0;
      }
      fseek( user_handle, 0, SEEK_SET );
	  if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
         myprintf( "INFO: UI001-Open of user file %s completed\n", user_file );
	  }
      return 1;
   }
} /* USER_Initialise */

/* ----------------------------------------------------------------------
 * ---------------------------------------------------------------------- */
void USER_Terminate( void ) {
   if (user_handle != NULL) {
      fclose( user_handle );
      user_handle = NULL;
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
         myprintf( "INFO: UI002-Closed: User file\n" );
      }
   }
   else {
      myprintf( "*ERR: UE007-USER_Terminate called when no user files were open, fclose ignored ! (USER_Terminate)\n" );
   }
} /* USER_Terminate */

/* ----------------------------------------------------------------------
 * Always adds to the end of the file.
 * Note: Cannot use the UTILS_write_record as that wants to place pads
 *       over the header area of the record.
 * ---------------------------------------------------------------------- */
int USER_add( USER_record_def *userrec ) {
   int lasterror;

   /* It doesn't matter in how many places I do this before this proc
	* is called, the fields still get here truncated. Try to pad out
	* here */
   UTILS_space_fill_string( userrec->user_name, USER_MAX_NAMELEN );
   UTILS_space_fill_string( userrec->user_password, USER_MAX_PASSLEN );
   UTILS_datestamp( userrec->last_password_change, UTILS_timenow_timestamp() );
   UTILS_encrypt_local( (char *)&userrec->user_password, USER_MAX_PASSLEN );

   lasterror = fseek( user_handle, 0L, SEEK_END );
   if ( (lasterror != 0)  || (ferror(user_handle)) ) {
      myprintf( "*ERR: UE008-Seek error %d on user file\n", errno );
      return 0;
   }
   /* note: using last error to check if 1 record was written here */
   lasterror = fwrite( userrec, sizeof(USER_record_def), 1, user_handle );
   if ( (ferror(user_handle)) || (lasterror != 1) ) {
      myprintf( "*ERR: UE009-Write error %d on user file\n", errno );
      return 0;
   }

   myprintf( "SEC-: UI003-Added user record for '%s'\n", userrec->user_name );
   lasterror = fflush( user_handle );
   if ( (lasterror != 0)  || (ferror(user_handle)) ) {
      myprintf( "*ERR: UE010-Flush Cache error %d on user file\n", errno );
      return 0;
   }

   /* All OK, record added to end of file */
   return 1;
} /* USER_add */

/* ----------------------------------------------------------------------
 * ---------------------------------------------------------------------- */
int USER_update( USER_record_def *userrec, long recordpos ) {
    return USER_update_checkflag( userrec, recordpos, 0 );
}

int USER_update_admin_allowed( USER_record_def *userrec, long recordpos ) {
    return USER_update_checkflag( userrec, recordpos, 1 );
}

int USER_update_checkflag( USER_record_def *userrec, long recordpos, int flag ) {
   int lasterror;

   /* auto-login only allowed to be updated if flag set to 1 */
   if ( (memcmp( userrec->user_name, "auto-login", 10) == 0)  && (flag != 1) ) {
       myprintf( "*ERR: UE011-Attempt to Change auto-login rejected, reserved user\n" );
	   return 0; /* NOT allowed */
   }

   lasterror = fseek( user_handle, (sizeof(USER_record_def) * recordpos), SEEK_SET );
   if ( (lasterror != 0)  || (ferror(user_handle)) ) {
      myprintf( "*ERR: UE012-Seek error %d on user file\n", errno );
      /* need to report format an error response */
	  return 0; /* failed */
   }
   else {
      lasterror = fwrite( userrec, sizeof(USER_record_def), 1, user_handle );
      if ( (ferror(user_handle)) || (lasterror != 1) ) {
         myprintf( "*ERR: UE013-Write error %d on user file\n", errno );
         /* need to report format an error response */
         return 0; /* failed */
      }
      else {
         /* format an OK response */
         return 1; /* OK */
      }
   }
} /* USER_update */

/* ----------------------------------------------------------------------
 * Read the record for the user_name in the passed record buffer.
 * Return -1 if failed, else the record number read.
 * NOTES: TRASHES the callers record buffer if the record is not found
 *        (well, fills it wath last record read which will be the wrong
 *        one, the caller must check the result we return).
 * ---------------------------------------------------------------------- */
long USER_read( USER_record_def *userrec ) {
   int  lasterror;
   long recordfound;
   char errormsg[80];
   char usertofind[USER_MAX_NAMELEN+2];

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: in utils.c, USER_read\n" );
   }

   /* Save the key we are searching for */
   strncpy( usertofind, userrec->user_name, (USER_MAX_NAMELEN+1)); /* +1, copy the NULL here */

   /*
    * Position to the begining of the file
    */
   lasterror = fseek( user_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      strcpy( errormsg, "file seek error (User File):");
      UTILS_set_message( errormsg );
	  myprintf( "*ERR: UE014-%s (USER_read)\n", errormsg );
      perror( errormsg );
      return( -1 );
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG: Positioned to start of file (USER_read)\n" );
   }

   /*
    * Loop reading through the file until we find the
    * record we are looking for.
    */
   recordfound = -1;
   while ((recordfound == -1) && !(feof(user_handle)) ) {
      lasterror = fread( userrec, sizeof(USER_record_def), 1, user_handle  );
      if (ferror(user_handle)) {
         myprintf( "*ERR: UE015-Read error occurred (USER_read)\n" );
         strcpy( errormsg, "file read error (User File):" );
         UTILS_set_message( errormsg );
         perror( errormsg );
         return( -1 );
      }
      if (!feof(user_handle)) {
         /*
          * Check to see if this is the record we want.
          * check for 41 bytes, the name and the type flag
          */
	     if (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_RECIOCMP) {
	       myprintf( "DEBUG: USER_read, checking against\n ....+....1....+....2....+....3....+....4\n'%s' (read) for\n'%s' (provided)\n", userrec->user_name, usertofind );
	     }
         if (
				 (!(memcmp(userrec->user_name, usertofind, USER_MAX_NAMELEN ))) &&
				 (userrec->record_state_flag != 'D')
			) {
             recordfound = ( ftell(user_handle) / sizeof(USER_record_def) ) - 1;
		    if (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_RECIOCMP) {
               myprintf( "DEBUG: USER_read, matched, recordpos set to %d\n", (int)recordfound );
		    }
         } /* if record found */
	  }  /* if not feof */
   } /* while not eof and not found */

   /*
    * Return the record number we found.
    */
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_IO) {
      if (recordfound >= 0) {
         myprintf( "DEBUG: USER_read: Record was read successfully\n" );
      }
      else {
	     myprintf("DEBUG: USER_read: Record was not found\n" );
      }
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving USER_read\n" );
   }
   return ( recordfound );
} /* USER_read */

/* ----------------------------------------------------------------------
 * Checks the user record to see if the user is allowed to add/delete
 * a job record for the unix_userid passed.
 * ---------------------------------------------------------------------- */
int USER_allowed_auth_for_id( char *userid, char auth_needed ) {
   return 1;
} /* USER_allowed_auth_for_id */

/* ----------------------------------------------------------------------
 * ---------------------------------------------------------------------- */
void USER_list_user( API_Header_Def * api_bufptr, FILE *tx ) {
   USER_record_def * local_copy;
   USER_record_def * provided_user_rec;
   char workbuf[129];
   char smallbuf[40];
   int  datalen, i;

   provided_user_rec = (USER_record_def *)&api_bufptr->API_Data_Buffer;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered USER_list_user\n" );
   }

   /* allocate memory for a local copy of the record as we intend to
	* overwrite the api buffer here */
   if ( (local_copy = (USER_record_def *)MEMORY_malloc(sizeof(USER_record_def),"USER_list_user")) == NULL) {
       myprintf( "*ERR: UE016-Insufficient memory to allocate %d bytes of memory (USER_list_user)\n",
                 sizeof(USER_record_def) );
       API_init_buffer( (API_Header_Def *)api_bufptr );
       strcpy( api_bufptr->API_System_Name, "USER" );
       strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
       strcpy( api_bufptr->API_Data_Buffer, "*ERR: Insufficient free memory available on server for request\n\0" );
       API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
       return;
    }

    /* save what we are about to display */
    memcpy( local_copy->userrec_version, provided_user_rec->userrec_version, sizeof(USER_record_def) );

   /* initialise the reply api buffer */
   API_init_buffer( (API_Header_Def *)api_bufptr ); 
   strcpy( api_bufptr->API_System_Name, "USER" );
   strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
   
   /* generate the display */
   datalen = snprintf( workbuf, 128, "\nDetails for user %s\n", local_copy->user_name );
   API_add_to_api_databuf( api_bufptr, workbuf, datalen, tx );
   datalen = snprintf( workbuf, 128, "User is %s\n \n", local_copy->user_details );
   API_add_to_api_databuf( api_bufptr, workbuf, datalen, tx );
   if (local_copy->user_auth_level == 'A') {
      strcpy( smallbuf, "full administrator" );
   }
   else if (local_copy->user_auth_level == 'S') {
      strcpy( smallbuf, "user security admin only" );
   }
   else if (local_copy->user_auth_level == 'O') {
      strcpy( smallbuf, "operator functions" );
   }
   else if (local_copy->user_auth_level == 'J') {
      strcpy( smallbuf, "job definition only" );
   }
   else {
      strcpy( smallbuf, "browse only" );
   }
   datalen = snprintf( workbuf, 128, "Logged in authority level : %s\n \n", smallbuf );
   API_add_to_api_databuf( api_bufptr, workbuf, datalen, tx );

   if (local_copy->allowed_to_add_users == 'Y') {
      datalen = snprintf( workbuf, 128, "This user can add user records granting authority to define jobs for\n" );
      API_add_to_api_databuf( api_bufptr, workbuf, datalen, tx );
      datalen = snprintf( workbuf, 128, "users in their authorised user list.\n" );
   }
   else if (local_copy->allowed_to_add_users == 'S') {
      datalen = snprintf( workbuf, 128, "This user has been granted full security admin rights to create users\n" );
   }
   else {
      datalen = snprintf( workbuf, 128, "This user is not permitted to create new users\n" );
   }
   API_add_to_api_databuf( api_bufptr, workbuf, datalen, tx );

   if (local_copy->local_auto_auth == 'Y') {
      datalen = snprintf( workbuf, 128, " \nIf logged on at the unix level with this id the user is permitted\n" );
      API_add_to_api_databuf( api_bufptr, workbuf, datalen, tx );
      datalen = snprintf( workbuf, 128, "to logon without a password\n" );
   }
   else {
      datalen = snprintf( workbuf, 128, "This user can only logon with a userid/password combination\n" );
   }
   API_add_to_api_databuf( api_bufptr, workbuf, datalen, tx );

   /* a little bit of work to list out the user table */
   UTILS_remove_trailing_spaces( (char *)&local_copy->user_list[0].userid );
   if ( (strcmp(local_copy->user_list[0].userid,"*") == 0) ||
        (local_copy->user_auth_level == 'A') ) {
      datalen = snprintf( workbuf, 128, "This user is permitted to create jobs for any unix userid\n" );
      API_add_to_api_databuf( api_bufptr, workbuf, datalen, tx );
   }
   else if (strcmp(local_copy->user_list[0].userid,"-") == 0) {
      datalen = snprintf( workbuf, 128, "This user is not permitted to create any jobs\n" );
      API_add_to_api_databuf( api_bufptr, workbuf, datalen, tx );
   }
   else { 
      datalen = snprintf( workbuf, 128, "This user may create jobs for the following unix userid names...\n" );
      API_add_to_api_databuf( api_bufptr, workbuf, datalen, tx );
      for (i = 0; i < USERS_MAX_USERLIST; i++ ) {
         UTILS_remove_trailing_spaces( (char *)&local_copy->user_list[i].userid );
	     if (strcmp(local_copy->user_list[i].userid,"-") != 0 ) {
            datalen = snprintf( workbuf, 128, "%s ", local_copy->user_list[i].userid );
            API_add_to_api_databuf( api_bufptr, workbuf, datalen, tx );
         }
      }
      datalen = snprintf( workbuf, 5, "\n" );  // new line after the list
      API_add_to_api_databuf( api_bufptr, workbuf, datalen, tx );
   }

   /* Show the last logged on and password change details as well */ 
   datalen = snprintf( workbuf, 128, "Last logged on %s, password last changed %s\n",
                      local_copy->last_logged_on,local_copy->last_password_change );
   API_add_to_api_databuf( api_bufptr, workbuf, datalen, tx );

   /* And we need to cleanup */
   datalen = snprintf( workbuf, 128, "\nEnd of user details\n\n" );
   API_add_to_api_databuf( api_bufptr, workbuf, datalen, tx );
   API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );

   /* release the memory we have allocated */
   MEMORY_free( local_copy );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving USER_list_user\n" );
   }
   return;
} /* USER_list_user */

/* ----------------------------------------------------------------------
 * ---------------------------------------------------------------------- */
void USER_list_allusers( API_Header_Def * api_bufptr, FILE *tx ) {
   USER_record_def * workbuffer;
   int lasterror, datalen;
   char workbuf[129];
		
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered USER_listall_users\n" );
   }
   
   if ( (workbuffer = (USER_record_def *)MEMORY_malloc(sizeof(USER_record_def),"USER_list_allusers")) == NULL) { 
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "USER" );
      strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
      strcpy( api_bufptr->API_Data_Buffer, "*E* Insufficient free memory on server to execute this command\n" );
      API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
	  return;
   }
   
   /*
    * Position to the begining of the file
    */
   lasterror = fseek( user_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      MEMORY_free(workbuffer);
      UTILS_set_message( "file seek error (user database file)" );
      perror( "file seek error (user database file)" );
      return;
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG: seek to start of file, user file (USER_listall_users)\n" );
   }

   /* initialise the reply api buffer */
   API_init_buffer( (API_Header_Def *)api_bufptr ); 
   strcpy( api_bufptr->API_System_Name, "USER" );
   strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
   
   /* loop until end of file */
   lasterror = 0; 
   while ( (!(ferror(user_handle))) && (!(feof(user_handle))) ) {
      lasterror = fread( workbuffer, sizeof(USER_record_def), 1, user_handle  );
	  if (  (!(feof(user_handle))) && (workbuffer->record_state_flag != 'D')  ) {
		 datalen = snprintf( workbuf, 128, "%s", workbuffer->user_name );
         API_add_to_api_databuf( api_bufptr, workbuf, datalen, tx );
         if (workbuffer->user_auth_level == 'A') {
            strcpy( workbuf, "ADMIN    " );
         }
		 else if (workbuffer->user_auth_level == 'S') {
            strcpy( workbuf, "SECURITY " );
         }
		 else if (workbuffer->user_auth_level == 'O') {
            strcpy( workbuf, "OPERATOR " );
         }
		 else if (workbuffer->user_auth_level == 'J') {
            strcpy( workbuf, "JOBAUTH  " );
         }
		 else {
            strcpy( workbuf, "BROWSE   " );
         }
		 strcat( workbuf, workbuffer->user_details );
		 strcat( workbuf, "\n" );
		 UTILS_remove_trailing_spaces( (char *)&workbuf );
		 datalen = strlen(workbuf);
         API_add_to_api_databuf( api_bufptr, workbuf, datalen, tx );
	  } /* while not eof */
   } /* while not a file error or eof */

   API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
   
   MEMORY_free( workbuffer );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving USER_listall_users\n" );
   }
} /* USER_list_allusers */

/* ----------------------------------------------------------------------
 * ---------------------------------------------------------------------- */
void USER_init_record_defaults( USER_record_def * local_user_rec ) {
   BULLETPROOF_USER_init_record_defaults( local_user_rec );
} /* USER_init_record_defaults */


/* ----------------------------------------------------------------------
 * ---------------------------------------------------------------------- */
void USER_process_client_request( FILE *tx, char *buffer, char *loggedonuser ) {
   API_Header_Def * api_bufptr;
   USER_record_def * local_user_rec;
   USER_record_def * provided_user_rec;
   long record_num;
   char saved_userid[USER_MAX_NAMELEN+1];
   char saved_password[USER_MAX_PASSLEN+1];

   api_bufptr = (API_Header_Def *)buffer;
   provided_user_rec = (USER_record_def *)&api_bufptr->API_Data_Buffer;
   strncpy( saved_userid, provided_user_rec->user_name, USER_MAX_NAMELEN );
   saved_userid[USER_MAX_NAMELEN] = '\0';
   strncpy( saved_password, provided_user_rec->user_password, USER_MAX_PASSLEN );
   saved_password[USER_MAX_PASSLEN] = '\0';

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: Entered USER_process_client_request\n" );
   }

   /* Check for a LISTALL request first. This is the only one that doesn't need
	* the user record passed to be read. */
   if ( memcmp(api_bufptr->API_Command_Number, API_CMD_LISTALL, API_CMD_LEN) == 0 ) {
      /* do the request then return */
       API_init_buffer( (API_Header_Def *)api_bufptr );
       strcpy( api_bufptr->API_System_Name, "USER" );
       strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
       USER_list_allusers( api_bufptr, tx );
       if (API_flush_buffer( api_bufptr, tx ) != 0) {
          myprintf( "*ERR: UE017-Unable to reply to client request (USER_proces_client_request)\n" );
       }
      return;
   }

   /* All other commands require that we read the record first */
   if ( (local_user_rec = (USER_record_def *)MEMORY_malloc(sizeof(USER_record_def),"USER_process_client_request")) == NULL) {
       myprintf( "*ERR: UE018-Insufficient memory to allocate %d bytes of memory (USER_process_client_request)\n",
                 sizeof(USER_record_def) );
       API_init_buffer( (API_Header_Def *)api_bufptr );
       strcpy( api_bufptr->API_System_Name, "USER" );
       strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
       strcpy( api_bufptr->API_Data_Buffer, "*ERR: Insufficient free memory available on server for request\n\0" );
       API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
       if (API_flush_buffer( api_bufptr, tx ) != 0) {
          myprintf( "*ERR: UE019-Unable to reply to client request (USER_proces_client_request)\n" );
       }
       return;
    }

	/* Attempt to read the record, we need the result later */
	UTILS_space_fill_string( (char *)&provided_user_rec->user_name, USER_MAX_NAMELEN );  /* api initiator may not have done so */
    strncpy( local_user_rec->user_name, provided_user_rec->user_name, USER_MAX_NAMELEN+1 ); /* include null */
    if (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_RECIOCMP) {
         myprintf( "DEBUG: About to read user, userid passed to '%s'\n", provided_user_rec->user_name );
    }
	record_num = USER_read( local_user_rec );

    /* For anything but an add request overlay the passed buffer with the
	 * result. We want to get rid of the malloced memory. */
    if ( memcmp(api_bufptr->API_Command_Number, API_CMD_USER_ADD, API_CMD_LEN) != 0 ) {
       memcpy( provided_user_rec->userrec_version, local_user_rec->userrec_version, sizeof(USER_record_def) );
	}
	MEMORY_free( local_user_rec );
	/* The working record is now in the provided buffer */

	/* --- was it an add request ? --- */
    if ( memcmp(api_bufptr->API_Command_Number, API_CMD_USER_ADD, API_CMD_LEN) == 0 ) {
        if (record_num != -1) {   /* record already exists */
           API_init_buffer( (API_Header_Def *)api_bufptr );
           strcpy( api_bufptr->API_System_Name, "USER" );
           strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
           strcpy( api_bufptr->API_Data_Buffer, "*ERR: User record already exists !\n\0" );
    	}
        else {
            /* Now we know its a new record it is safe to add */
            /* Sanity check the input record */
            if (BULLETPROOF_user_record(provided_user_rec, 1) == 1) {
                /* and if Ok just add it */
                if (USER_add(provided_user_rec) != 1) {  /* add failed */
                   API_init_buffer( (API_Header_Def *)api_bufptr );
                   strcpy( api_bufptr->API_System_Name, "USER" );
                   strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
                   strcpy( api_bufptr->API_Data_Buffer, "*ERR: Error adding user record, review server log for details !\n\0" );
                }
                else { /* add completed */
                   myprintf( "SEC-: UI004-User %s added by %s\n", saved_userid, loggedonuser );
                   API_init_buffer( (API_Header_Def *)api_bufptr );
                   strcpy( api_bufptr->API_System_Name, "USER" );
                   strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
                   strcpy( api_bufptr->API_Data_Buffer, "User record has been added\n\0" );
                }
			}
            else { /* errors found in sanity check */
                   API_init_buffer( (API_Header_Def *)api_bufptr );
                   strcpy( api_bufptr->API_System_Name, "USER" );
                   strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
                   strcpy( api_bufptr->API_Data_Buffer, "*ERR: User Data failed sanity checks, check input for conflicts !\n\0" );
            }
        }
	}

    /* delete, info and password change all need an existing user */
    else if ( 
        ( memcmp(api_bufptr->API_Command_Number, API_CMD_USER_DELETE, API_CMD_LEN) == 0 ) ||
        ( memcmp(api_bufptr->API_Command_Number, API_CMD_RETRIEVE, API_CMD_LEN) == 0 ) ||
        ( memcmp(api_bufptr->API_Command_Number, API_CMD_USER_NEWPSWD, API_CMD_LEN) == 0 )
    ) {
        if (record_num == -1) {   /* record does not exist */
           API_init_buffer( (API_Header_Def *)api_bufptr );
           strcpy( api_bufptr->API_System_Name, "USER" );
           strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
           snprintf( api_bufptr->API_Data_Buffer, (MAX_API_DATA_LEN - 1), "*ERR: User record for %s does not exist !\n",
                   saved_userid );
    	}
    	/* --- Was it user info details request ? --- */
        else if ( memcmp(api_bufptr->API_Command_Number, API_CMD_RETRIEVE, API_CMD_LEN) == 0 ) {
           USER_list_user( api_bufptr, tx );
        }
    	/* --- Else it is a change request --- */
		else {
            /* check for delete first */
		    if ( memcmp(api_bufptr->API_Command_Number, API_CMD_USER_DELETE, API_CMD_LEN) == 0 ) {
               provided_user_rec->record_state_flag = 'D';  /* deleted */
            }
            else { /* a password change */
               strncpy( provided_user_rec->user_password, saved_password, USER_MAX_PASSLEN );
               UTILS_space_fill_string( (char *)&provided_user_rec->user_password, USER_MAX_PASSLEN );
               if (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_VARS) {
                  myprintf(  "DEBUG: password about to be set to '%s' for %s\n", provided_user_rec->user_password,
                             provided_user_rec->user_name );
               }
               UTILS_datestamp( provided_user_rec->last_password_change, UTILS_timenow_timestamp() );
               UTILS_encrypt_local( (char *)&provided_user_rec->user_password, USER_MAX_PASSLEN );
               if (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_VARS) {
                  myprintf(  "DEBUG: encrypted password is '%s'\n", provided_user_rec->user_password );
               }
            }
            if (USER_update( provided_user_rec, record_num ) != 1) { /* failed */
               API_init_buffer( (API_Header_Def *)api_bufptr );
               strcpy( api_bufptr->API_System_Name, "USER" );
               strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
               strcpy( api_bufptr->API_Data_Buffer, "*ERR: Update of user record failed, refer to server log for details !\n\0" );
            }
            else {
		       if ( memcmp(api_bufptr->API_Command_Number, API_CMD_USER_DELETE, API_CMD_LEN) == 0 ) {
                  myprintf( "SEC-: UI005-User %s deleted by %s\n", saved_userid, loggedonuser );
               }
               else {
                  myprintf( "SEC-: UI006-User %s password changed by %s\n", saved_userid, loggedonuser );
               }
               API_init_buffer( (API_Header_Def *)api_bufptr );
               strcpy( api_bufptr->API_System_Name, "USER" );
               strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
               snprintf( api_bufptr->API_Data_Buffer, (MAX_API_DATA_LEN - 1), "User request has been completed by the server\n" );
            }
        }
    }  /* else if delete, info or password change */

    /* --- else we don't know what it is --- */
    else {
       API_init_buffer( (API_Header_Def *)api_bufptr );
       strcpy( api_bufptr->API_System_Name, "USER" );
       strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
       strcpy( api_bufptr->API_Data_Buffer, "*ERR: USER request is not recognised by the server !\n\0" );
    }

    API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
    if (API_flush_buffer( api_bufptr, tx ) != 0) {
       myprintf( "*ERR: UE020-Unable to reply to client request (USER_proces_client_request)\n" );
    }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.user >= DEBUG_LEVEL_PROC) {
      myprintf(  "DEBUG: leaving USER_process_client_request\n" );
   }
} /* USER_process_client_request */
