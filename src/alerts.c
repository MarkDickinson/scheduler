#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include "debug_levels.h"
#include "alerts.h"
#include "scheduler.h"
#include "jobslib.h"
#include "utils.h"
#include "schedlib.h"
#include "memorylib.h"

/* -------------------------------------------------------
   ALERTS_initialise
   ------------------------------------------------------- */
int ALERTS_initialise( void ) {
   if ( (alerts_handle = fopen( alerts_file, "rb+" )) == NULL ) {
	  if (errno == 2) {   /* doesn't exist */
         if ( (alerts_handle = fopen( alerts_file, "ab+" )) == NULL ) {
			 myprintf( "*ERR: AE001-Unable to create alerts file %s, error %d (ALERTS_initialise)\n", alerts_file, errno );
			 return( 0 );
		 }
		 else {
            if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
		       myprintf( "INFO: AI001-Created alerts file %s\n", alerts_file );
			}
		    /* created, close and reopen it the way we need it */
		    fclose( alerts_handle );
			alerts_handle = NULL;
            if ( (alerts_handle = fopen( alerts_file, "rb+" )) == NULL ) {
               myprintf( "*ERR: AE002-Open of alerts file %s failed, errno %d (ALERTS_initialise)\n", alerts_file, errno );
               perror( "Open of alerts file failed:" );
			   return( 0 );
			}
			else {
		       return( 1 );
			}
		 }   /* end of created ok */
	  }      /* end of if errno == 2 */
	  else {
         myprintf( "*ERR: AE003-Open of alerts file %s failed, errno %d (ALERTS_initialise)\n", alerts_file, errno );
         perror( "Open of alerts file failed:" );
         return 0;
	  }
   }
   /* initial open was OK */
   else {
      fseek( alerts_handle, 0, SEEK_SET );
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
         myprintf( "INFO: AI002-Open of alerts file %s completed\n", alerts_file );
	  }
      return 1;
   }
} /* ALERTS_initialise */

/* -------------------------------------------------------
   ALERTS_terminate
   ------------------------------------------------------- */
void ALERTS_terminate( void ) {
   if (alerts_handle != NULL) {
      fclose( alerts_handle );
	  alerts_handle = NULL;
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
         myprintf( "INFO: AI003-Closed: Alerts file\n" );
	  }
   }
   else { 
      myprintf( "*ERR: AE004-ALERTS_Terminate called with no open alerts files, fclose ignored (ALERTS_terminate).\n" );
   }
} /* ALERTS_terminate */


/* -------------------------------------------------------
   ALERTS_read_alert
   ------------------------------------------------------- */
long ALERTS_read_alert( alertsfile_def *datarec ) {
   int  lasterror;
   long recordfound;
   alertsfile_def *local_rec;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: In ALERTS_read_alert\n" );
   }

   /*
    * Position to the begining of the file
    */
   lasterror = fseek( alerts_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      myprintf( "*ERR: AE005-Seek to start of alerts file failed (ALERTS_read_alert)\n" );
      UTILS_set_message( "Seek to start of alerts file failed" );
      return( -1 );
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Positioned to start of file (ALERTS_read_alert)\n" );
   }

   /*
    * Allocate memory for a local copy of the record
    */
   if ( (local_rec = (alertsfile_def *)MEMORY_malloc( sizeof( alertsfile_def ), "ALERTS_read_alert" ) ) == NULL) {
      UTILS_set_message( "Unable to allocate memory required for read operation" );
      return( -1 );
   }

   /*
    * Loop reading through the file until we find the
    * record we are looking for.
    */
   recordfound = -1;
   while ((recordfound == -1) && !(feof(alerts_handle)) ) {
      lasterror = fread( local_rec, sizeof(alertsfile_def), 1, alerts_handle  );
      if (ferror(alerts_handle)) {
         if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_IO) {
            myprintf( "DEBUG: read error %d (ALERTS_read_alert)\n", errno );
         }
         MEMORY_free( local_rec );
         myprintf( "*ERR: AE007-Read error occurred on alerts file (ALERTS_read_alert).\n" );
         UTILS_set_message( "Read error on alerts file" );
         perror( "alerts_handle" );
         return( -1 );
      }

      /*
       * Check to see if this is the record we want.
       */
      if (!memcmp(local_rec->job_details.job_header.jobname,
                  datarec->job_details.job_header.jobname, JOB_NAME_LEN)) {
         if (local_rec->job_details.job_header.info_flag != 'D' ) {
            recordfound = ( ftell(alerts_handle) / sizeof(alertsfile_def) ) - 1;
		 }
      }
   }

   /*
    * If we found a record copy it to the callers buffer.
    */
   if (recordfound != -1) {
      memcpy( datarec, local_rec, sizeof( alertsfile_def ) );
	  /* MID:PATCH, I havn't found where this is being trashed when
	   * being tarred up and moved to another machine. */
      datarec->failure_code[JOB_STATUSCODE_LEN] = '\0';
   }

   /*
    * Free our temporary buffer
    */
   MEMORY_free( local_rec );

   /*
    * Return the record number we found.
    */
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving ALERTS_read_alert\n" );
   }
   return ( recordfound );
} /* ALERTS_read_alert */


/* -------------------------------------------------------
   ALERTS_add_alert
   ------------------------------------------------------- */
long ALERTS_add_alert( joblog_def *datarec, char severity, char *failcode ) {
  long recordnum;
  alertsfile_def * local_alerts_rec;

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: In ALERTS_add_alert\n" );
  }

  if ((local_alerts_rec = (alertsfile_def *)MEMORY_malloc(sizeof(alertsfile_def),"ALERTS_add_alert")) == NULL) {
     myprintf( "*ERR: AE008-Unable to allocate memory for alert read (ALERTS_add_alert)\n" );
     UTILS_set_message( "Unable to allocate memory for alert add" );
     return ( -1 );
  }
  memcpy( local_alerts_rec->job_details.job_header.jobnumber, datarec, JOB_NUMBER_LEN );
  local_alerts_rec->severity = severity;
  memcpy( local_alerts_rec->failure_code, failcode, JOB_STATUSCODE_LEN );
  local_alerts_rec->failure_code[JOB_STATUSCODE_LEN] = '\0';
  local_alerts_rec->acknowledged = 'N';
  recordnum = ALERTS_update_alert( (alertsfile_def *) datarec, severity, failcode, local_alerts_rec->acknowledged );
  MEMORY_free( local_alerts_rec );
  if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: Leaving ALERTS_add_alert\n" );
  }
  return( recordnum );
} /* ALERTS_add_alert */


/* -------------------------------------------------------
   ALERTS_update_alert
   ------------------------------------------------------- */
long ALERTS_update_alert( alertsfile_def * datarec, char severity, char *failcode, char acked ) {
  long recordnum;
  alertsfile_def * local_alerts_rec;

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: In ALERTS_update_alert\n" );
  }

  if ((local_alerts_rec = (alertsfile_def *)MEMORY_malloc(sizeof(alertsfile_def),"ALERTS_update_alert")) == NULL) {
     myprintf( "*ERR: AE009-Unable to allocate memory for alert read (ALERTS_update_alert)\n" );
     UTILS_set_message( "Unable to allocate memory for alert read" );
     return ( -1 );
  }
  memcpy( local_alerts_rec, datarec, sizeof(alertsfile_def) );
  /*
   * the read returns -1 if not found which is what we pass to the
   * write for a new rec anyway so we don't need to check if the
   * read failed.
  */
  recordnum = ALERTS_read_alert( local_alerts_rec );
  datarec->severity = severity;
  datarec->acknowledged = acked;
  memcpy( datarec->failure_code, failcode, JOB_STATUSCODE_LEN );
  datarec->failure_code[JOB_STATUSCODE_LEN] = '\0';
  recordnum = ALERTS_write_alert_record( datarec, recordnum );

  MEMORY_free( local_alerts_rec );

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: Leaving ALERTS_update_alert\n" );
  }
  return( recordnum );
} /* ALERTS_update_alert */


/* -------------------------------------------------------
   return 0 if failed 
   ------------------------------------------------------- */
long ALERTS_delete_alert( alertsfile_def *datarec ) {
  long recordnum;
  if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: In ALERTS_delete_alert\n" );
  }
  recordnum = ALERTS_read_alert( datarec );
  if (recordnum == -1) {
		  /* the below can occur in normal operation on a job delete cleanup,
		   * so don't store a message */
/*     UTILS_set_message( "ALERTS_delete_alert: Alerts file read error returned from ALERTS_read_alert" ); */
     return( -1 );  /* error */
  }

  datarec->job_details.job_header.info_flag = 'D';  /* being deleted */
  recordnum = ALERTS_write_alert_record( datarec, recordnum );
  if (recordnum == -1) {
     return( recordnum );   /* failed */
  }

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: Leaving ALERTS_delete_alert, all ok\n" );
  }
  return( 0 ); /* All OK */
} /* ALERTS_delete_alert */


/* -------------------------------------------------------
   ALERTS_write_alert_record
   if -1 create a new record
   if n update an existing record
   if -2, don't write to file (used for scheduler newday overdue)
   ------------------------------------------------------- */
long ALERTS_write_alert_record( alertsfile_def *alerts_rec, long recordnum ) {
   int lasterror, altype;
   long currentpos;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: In ALERTS_write_alert_record\n" );
   }

   /*
    * If the record number is -1 the caller wants us to
    * write to eof. Otherwise seek to the record number
    * specified and update it.
    */
   if (recordnum == -1) {
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_IO) {
         myprintf( "DEBUG: File seek to END of file (ALERTS_write_alert_record)\n" );
      }
      lasterror = fseek( alerts_handle, 0, SEEK_END );
   }
   else if (recordnum == -2) {
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_IO) {
         myprintf( "DEBUG: No file seek, no IO required (ALERTS_write_alert_record)\n" );
      }
      lasterror = 0; /* no action to take */
   }
   else {
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_IO) {
         myprintf( "DEBUG: File seek to record %d (ALERTS_write_alert_record)\n", (int)recordnum );
      }
      lasterror = fseek( alerts_handle, (recordnum * sizeof(alertsfile_def)), SEEK_SET );
   }
   if (lasterror != 0) {
      myprintf( "*ERR: AE010-File seek error (ALERTS_write_alert_record)\n" );
      UTILS_set_message( "Alerts file seek error" );
      perror( "alerts_handle" );
      return( -1 );
   }

   /*
    * get the current record position from the file system
    * we trust this rather than assuming the seek worked.
    * Also we need to know the eof record number to return
    * to the caller if we are writing to eof.
    */
   currentpos = ( ftell(alerts_handle) / sizeof(alertsfile_def) );
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG: File position is now at record %d (ALERTS_write_alert_record)\n", (int)currentpos );
   }

   /*
	*  If we are writing alerts to an external monitoring system also, check
	*  for that here now.
	*/
   if ( (pSCHEDULER_CONFIG_FLAGS.use_central_alert_console == 'Y') && (pSCHEDULER_CONFIG_FLAGS.central_alert_console_active != 0)) {
      if (alerts_rec->job_details.job_header.info_flag == 'D') { altype = 0; /* 0 is delete alert */ }
      else { altype = 1; /* 1 is raise alert */ }
      if (ALERTS_REMOTE_send_alert( alerts_rec, altype )  != 0) {
			  alerts_rec->external_alert_sent = 'Y';
	  }
	  else {
			  alerts_rec->external_alert_sent = 'N';
	  }
   }
   else if (pSCHEDULER_CONFIG_FLAGS.use_central_alert_console == 'E') {
      if (alerts_rec->job_details.job_header.info_flag == 'D') { altype = 0; /* 0 is delete alert */ }
      else { altype = 1; /* 1 is raise alert */ }
      if (ALERTS_LOCAL_run_alertprog( alerts_rec, altype ) != 0) {
			  alerts_rec->external_alert_sent = 'Y';
	  }
	  else {
			  alerts_rec->external_alert_sent = 'N';
	  }
      if (alerts_rec->external_alert_sent == 'N') {
         myprintf("*ERR: AE011-Unable to run external task to forward alert for %s\n",
                    alerts_rec->job_details.job_header.jobname );
      }
   }
   else {
      alerts_rec->external_alert_sent = 'N';
   }

   /* MID: Patch, code doesn't survive tar/untar without this */
   alerts_rec->failure_code[JOB_STATUSCODE_LEN] = '\0';

   /* No futher action if not writing the alert */
   if (recordnum == -2) {
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
         myprintf( "DEBUG: Leaving ALERTS_write_alert_record without IO\n" );
      }
      return( 0 );
   }

   /*
    * note: cannot rely on return code from a buffered write, use
    *       ferror to see if it worked.
    */
   lasterror = fwrite( alerts_rec, sizeof(alertsfile_def), 1, alerts_handle );
   if (ferror(alerts_handle)) {
      myprintf( "*ERR: AE012-File write error on alerts file (ALERTS_write_alert_record)\n" );
      UTILS_set_message( "Write error on alerts file" );
      perror( "alerts_handle" );
      return( -1 );
   }

   /*
    * return the recordnumber position we wrote the record into.
    */
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving ALERTS_write_alert_record\n" );
   }
   return( currentpos );
} /* ALERTS_write_alert_record */


/* -------------------------------------------------------
   ALERTS_display_alerts
   ------------------------------------------------------- */
void ALERTS_display_alerts( API_Header_Def * api_bufptr, FILE *tx ) {
   /*
      Format all the alerts into a display buffer.
      returns the length of the display string
   */
   char workbuf[129];
   int  worklen;
   int lasterror;
   alertsfile_def * local_rec;
   
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: In ALERTS_display_alerts\n" );
   }

   API_init_buffer( api_bufptr );
   strcpy( api_bufptr->API_System_Name, "DEP" );
   memcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY, API_RESPCODE_LEN );

   /*
    * read through each record in the alerts file and format
    * into a display buffer.
    */
   lasterror = fseek( alerts_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      myprintf( "*ERR: AE013-Seek to start of alerts file failed (ALERTS_display_alert)\n" );
      UTILS_set_message( "Seek to start of alerts file failed" );
      memcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR, API_RESPCODE_LEN );
	  strcpy( api_bufptr->API_Data_Buffer, "*E* Request failed, Server had an IO error on alerts file\n" );
      API_set_datalen( api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
      return;
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG: Postioned to start of file\n" );
   }

   if ((local_rec = (alertsfile_def *)MEMORY_malloc(sizeof(alertsfile_def),"ALERTS_display_alerts")) == NULL) {
	  myprintf( "*ERR: AE014-Unable to allocate %d bytes of memory (ALERTS_display_alert)\n", sizeof(alertsfile_def) );
      UTILS_set_message( "Unable to allocate memoryfor alert read" );
      memcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR, API_RESPCODE_LEN );
	  strcpy( api_bufptr->API_Data_Buffer, "*E* Request failed, Server unable to malloc required memory for request\n" );
      API_set_datalen( api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
      return;
   }

   clearerr( alerts_handle );
   while (  (!(feof(alerts_handle))) && (!(ferror(alerts_handle)))  ) {
      lasterror = fread( local_rec, sizeof(alertsfile_def), 1, alerts_handle  );
      if (ferror(alerts_handle)) {
         perror( "alerts_handle" );
         MEMORY_free( local_rec );
         myprintf( "*ERR: A015-Read error occurred on alerts file (ALERTS_display_alert).\n" );
         UTILS_set_message( "Read error on alerts file" );
         memcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR, API_RESPCODE_LEN );
	     strcpy( api_bufptr->API_Data_Buffer, "*E* Request failed, Server had an IO error on alerts file\n" );
         API_set_datalen( api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
         return;
      }
	  if ( (!feof(alerts_handle)) && (local_rec->job_details.job_header.info_flag != 'D') ) {
         local_rec->failure_code[JOB_STATUSCODE_LEN] = '\0';
         worklen = snprintf( workbuf, 128, "%s %s %c %s %c\n", 
						     local_rec->job_details.datestamp,
                             local_rec->job_details.job_header.jobname,
                             local_rec->severity,
                             local_rec->failure_code,
                             local_rec->acknowledged
                           );
	  }
	  else { worklen = 0; /* do nothing */ }
      if (worklen > 0) {
         API_add_to_api_databuf( api_bufptr, workbuf, worklen, tx );
      }
   } /* end while not eof */

   MEMORY_free( local_rec );

   API_set_datalen( api_bufptr, strlen(api_bufptr->API_Data_Buffer) );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving ALERTS_display_alerts\n" );
   }
   return;
} /* ALERTS_display_alerts */

/* -------------------------------------------------------
   ALERTS_count_alerts
   Returns the number of alerts in the alerts file or
   will return -1 on an error.
   ------------------------------------------------------- */
int  ALERTS_count_alerts( void ) {
   int  recordcounter;
   int lasterror;
   alertsfile_def * local_rec;
   
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: In ALERTS_count_alerts\n" );
   }

   /*
    * read through each record in the alerts file and count them.
    */
   recordcounter = 0;   
   lasterror = fseek( alerts_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      myprintf( "*ERR: AE016-Seek to start of alerts file failed (ALERTS_count_alerts)\n" );
      UTILS_set_message( "Seek to start of alerts file failed" );
      return( -1 );
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG: Positioned to start of file (ALERTS_count_alerts)\n" );
   }

   if ((local_rec = (alertsfile_def *)MEMORY_malloc(sizeof(alertsfile_def),"ALERTS_count_alerts")) == NULL) {
      myprintf( "*ERR: AE017-Unable to allocate memory for alert read (ALERTS_count_alerts)\n" );
      UTILS_set_message( "Unable to allocate memory for alert read" );
      return ( -1 );
   }

   while ( (!(feof(alerts_handle))) && (!ferror(alerts_handle)) ) {
      lasterror = fread( local_rec, sizeof(alertsfile_def), 1, alerts_handle  );
      if (ferror(alerts_handle)) {
         myprintf( "*ERR: AE018-Read error occurred on alerts file (ALERTS_count_alerts).\n" );
         UTILS_set_message( "Read error on alerts file" );
         perror( "alerts_handle" );
         MEMORY_free( local_rec );
         return( -1 );
      }
	  if (!(feof(alerts_handle))) {
	     if (local_rec->job_details.job_header.info_flag != 'D') {
            recordcounter++;
		 }
	  }
   } /* end while not eof */

   MEMORY_free( local_rec );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving ALERTS_count_alerts\n" );
   }
   return( recordcounter );
} /* ALERTS_count_alerts */

/* -------------------------------------------------------
   ALERTS_show_alert_detail
   ------------------------------------------------------- */
int ALERTS_show_alert_detail( char * jobname, char * buffer, int buffsize ) {
		char *pSptr, *pEptr;
		int buffleft;
		int datalen;
		char workbuffer[129];
		alertsfile_def * pAlert_Rec;
		
        if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
           myprintf( "DEBUG: In ALERTS_show_alert_detail\n" );
        }

		if ( (pAlert_Rec = (alertsfile_def *)MEMORY_malloc(sizeof(alertsfile_def),"ALERTS_show_detail")) == NULL ) {
           myprintf( "*ERR: AE019-Unable to allocate %d bytes of memory required (ALERTS_show_alert_detail)\n",
				    sizeof(alertsfile_def) );
           UTILS_set_message( "Unable to allocate memory required" );
		   strcpy( buffer, "*ERR: INSUFFICIENT FREE MEMORY ON SERVER FOR THIS OPERATION\n" );
           return( strlen(buffer) );	  
		}

		strncpy( pAlert_Rec->job_details.job_header.jobname, jobname, JOB_NAME_LEN );
		if (ALERTS_read_alert( pAlert_Rec ) < 0) {
		   MEMORY_free( pAlert_Rec );
		   strcpy( buffer, "*ERR: ALERT RECORD WAS NOT FOUND (ALERTS_show_alert_detail)\n" );
           return( strlen(buffer) );	  
		}

		pSptr = buffer;
		pEptr = buffer;
		buffleft = buffsize;
		
		datalen = snprintf( workbuffer, 128, "JOB NAME : %s\n", pAlert_Rec->job_details.job_header.jobname );
		if (buffleft > datalen) {
				strncpy( pEptr, workbuffer, datalen );
				pEptr = pEptr + datalen;
				buffleft = buffleft - datalen;
		}
		datalen = snprintf( workbuffer, 128, "JOB RUN TIME: %s, JOB STATUS CODE: %s\n",
						    pAlert_Rec->job_details.datestamp, pAlert_Rec->job_details.status_code );
		if (buffleft > datalen) {
				strncpy( pEptr, workbuffer, datalen );
				pEptr = pEptr + datalen;
				buffleft = buffleft - datalen;
		}
		datalen = snprintf( workbuffer, 128, "MSG TEXT: %s\n", pAlert_Rec->job_details.text );
		if (buffleft > datalen) {
				strncpy( pEptr, workbuffer, datalen );
				pEptr = pEptr + datalen;
				buffleft = buffleft - datalen;
		}
	    strcpy( workbuffer, "ALERT SEVERITY: " );
		if (pAlert_Rec->severity == '0') {
		   strcat( workbuffer, "READY TO BE CLEARED\n" );
		}
		else if (pAlert_Rec->severity == '1') {
		   strcat( workbuffer, "OPERATOR ACTION REQUIRED\n" );
		}
		else if (pAlert_Rec->severity == '2') {
		   strcat( workbuffer, "RERUN REQUIRED\n" );
		}
		else {  /* must be 3 */
		   strcat( workbuffer, "PAGE SUPPORT\n" );
		}
		datalen = strlen( workbuffer );
		if (buffleft > datalen) {
				strncpy( pEptr, workbuffer, datalen );
				pEptr = pEptr + datalen;
				buffleft = buffleft - datalen;
		}
        pAlert_Rec->failure_code[JOB_STATUSCODE_LEN] = '\0';
	    datalen = snprintf( workbuffer, 128, "FAILURE CODE: %s\n", pAlert_Rec->failure_code );	
		if (buffleft > datalen) {
				strncpy( pEptr, workbuffer, datalen );
				pEptr = pEptr + datalen;
				buffleft = buffleft - datalen;
		}

		MEMORY_free( pAlert_Rec );
		
        if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
           myprintf( "DEBUG: Leaving ALERTS_show_alert_detail\n" );
        }
		return( pEptr - pSptr );  /* datalen */
} /* ALERTS_show_alert_detail */

/* -------------------------------------------------------
   ALERTS_generic_system_alert 
   ------------------------------------------------------- */
void ALERTS_generic_system_alert( job_header_def * job_header, char * msgtext ) {
   alertsfile_def * local_alerts_rec;
   long junk;
 
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: In ALERTS_generic_system_alert\n" );
   }

   if ((local_alerts_rec = (alertsfile_def *)MEMORY_malloc(sizeof(alertsfile_def),"ALERTS_generic_system_alert")) == NULL) {
      UTILS_set_message( "Unable to allocate memory for alert creation" );
	  myprintf( "*ERR: AE022-UNABLE TO ALLOCATE MEMORY FOR ALERT: ALERT DETAILS FOLLOW...\n" );
	  myprintf( "*ERR: AE020...JOB %s\n", job_header->jobname );
	  myprintf( "*ERR: AE021...ALERT TEXT: %s\n", msgtext );
      return;
   }

   strncpy( local_alerts_rec->job_details.job_header.jobname, job_header->jobname, JOB_NAME_LEN );
   UTILS_space_fill_string( local_alerts_rec->job_details.job_header.jobname, JOB_NAME_LEN );
   strncpy( local_alerts_rec->job_details.job_header.jobnumber, "000000\0", JOB_NUMBER_LEN+1 ); /* include null */
   local_alerts_rec->job_details.job_header.info_flag = 'S';  /* System alert */
   UTILS_datestamp( (char *)&local_alerts_rec->job_details.datestamp, 0 );
   strncpy( local_alerts_rec->job_details.msgid, "0000000000\0", JOB_LOGMSGID_LEN+1  ); /* include null */
   strncpy( local_alerts_rec->job_details.text, msgtext, JOB_LOGMSG_LEN );
   strncpy( local_alerts_rec->job_details.status_code, "000\0", JOB_STATUSCODE_LEN+1 ); /* include null */
   local_alerts_rec->severity = '2';
   memcpy( local_alerts_rec->failure_code, "000", JOB_STATUSCODE_LEN ); 
   local_alerts_rec->failure_code[JOB_STATUSCODE_LEN] = '\0';
   local_alerts_rec->acknowledged = 'N';
   junk = ALERTS_update_alert( local_alerts_rec, '1', "000", 'N' );

   MEMORY_free( local_alerts_rec );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving ALERTS_generic_system_alert\n" );
   }
} /* ALERTS_generic_system_alert */


/* -------------------------------------------------------
   ALERTS_generic_system_alert_MSGONLY
   ------------------------------------------------------- */
void ALERTS_generic_system_alert_MSGONLY( job_header_def * job_header, char * msgtext ) {
   alertsfile_def * local_alerts_rec;
   long junk;
 
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: In ALERTS_generic_system_alert_MSGONLY\n" );
   }

   if ((local_alerts_rec = (alertsfile_def *)MEMORY_malloc(sizeof(alertsfile_def),"ALERTS_generic_system_alert_MSGONLY")) == NULL) {
      UTILS_set_message( "Unable to allocate memory for alert creation" );
	  myprintf( "*ERR: AE023-UNABLE TO ALLOCATE MEMORY FOR ALERT: ALERT DETAILS FOLLOW...\n" );
	  myprintf( "*ERR: AE020-...JOB %s\n", job_header->jobname );
	  myprintf( "*ERR: AE021-...ALERT TEXT: %s\n", msgtext );
      return;
   }

   strncpy( local_alerts_rec->job_details.job_header.jobname, job_header->jobname, JOB_NAME_LEN );
   UTILS_space_fill_string( local_alerts_rec->job_details.job_header.jobname, JOB_NAME_LEN );
   strncpy( local_alerts_rec->job_details.job_header.jobnumber, "000000\0", JOB_NUMBER_LEN+1 ); /* include null */
   local_alerts_rec->job_details.job_header.info_flag = 'S';  /* System alert */
   UTILS_datestamp( (char *)&local_alerts_rec->job_details.datestamp, 0 );
   strncpy( local_alerts_rec->job_details.msgid, "0000000000\0", JOB_LOGMSGID_LEN+1  ); /* include null */
   strncpy( local_alerts_rec->job_details.text, msgtext, JOB_LOGMSG_LEN );
   strncpy( local_alerts_rec->job_details.status_code, "000\0", JOB_STATUSCODE_LEN+1 ); /* include null */
   local_alerts_rec->severity = '2';
   memcpy( local_alerts_rec->failure_code, "000", JOB_STATUSCODE_LEN ); 
   local_alerts_rec->failure_code[JOB_STATUSCODE_LEN] = '\0';
   local_alerts_rec->acknowledged = 'N';
   junk = ALERTS_write_alert_record( local_alerts_rec, -2 );

   MEMORY_free( local_alerts_rec );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving ALERTS_generic_system_alert_MSGONLY\n" );
   }
} /* ALERTS_generic_system_alert_MSGONLY */


/* -------------------------------------------------------
   ALERTS_generic_clear_remote_alert
   Generic delete interface for remote alerts. While
   normal alerts are handled by the write alert routine this
   procedure is for those system alerts that are only
   forwarded remotely but never recorded in the local alert
   file.
   ------------------------------------------------------- */
void ALERTS_generic_clear_remote_alert( job_header_def * job_header, char * msgtext ) {
   alertsfile_def * local_alerts_rec;
   long junk;
 
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: In ALERTS_generic_clear_remote_alert\n" );
   }

   if ((local_alerts_rec = (alertsfile_def *)MEMORY_malloc(sizeof(alertsfile_def),"ALERTS_generic_clear_remote_alert")) == NULL) {
      UTILS_set_message( "Unable to allocate memory for alert creation" );
	  myprintf( "*ERR: AE024-UNABLE TO ALLOCATE MEMORY TO CLEAR ALERT: ALERT DETAILS FOLLOW...\n" );
	  myprintf( "*ERR: AE020-...JOB %s\n", job_header->jobname );
	  myprintf( "*ERR: AE021-...ALERT TEXT: %s\n", msgtext );
      return;
   }

   strncpy( local_alerts_rec->job_details.job_header.jobname, job_header->jobname, JOB_NAME_LEN );
   UTILS_space_fill_string( local_alerts_rec->job_details.job_header.jobname, JOB_NAME_LEN );
   strncpy( local_alerts_rec->job_details.text, msgtext, JOB_LOGMSG_LEN );

   /* Which type do we use ? */
   if ( (pSCHEDULER_CONFIG_FLAGS.use_central_alert_console == 'Y') &&
        (pSCHEDULER_CONFIG_FLAGS.central_alert_console_active != 0))
   {
      junk = ALERTS_REMOTE_send_alert( local_alerts_rec, 0 );
   }
   else if (pSCHEDULER_CONFIG_FLAGS.use_central_alert_console == 'E') {
      junk = ALERTS_LOCAL_run_alertprog( local_alerts_rec, 0 );
   }

   MEMORY_free( local_alerts_rec );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.alerts >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving ALERTS_generic_clear_remote_alert\n" );
   }
} /* ALERTS_generic_clear_remote_alert */


/* -------------------------------------------------------
   ALERTS_REMOTE_send_alert
   alerttype may be 1 a new alert, 0 delete an alert
   ------------------------------------------------------- */
int ALERTS_REMOTE_send_alert( alertsfile_def * alert_rec, int alerttype ) {
		return( 0 );  /* not yet implemented */
}

/* -------------------------------------------------------
   ALERTS_LOCAL_run_alertprog
   alerttype may be 1 a new alert, 0 delete an alert
   Run an external program on an alert condition change.
   Returns 0-failed, 1-all ok
   ------------------------------------------------------- */
int ALERTS_LOCAL_run_alertprog( alertsfile_def * alert_rec, int alerttype ) {
   extern currently_running_alerts_def running_alert_table;
   pid_t pid_number;
   int i, saved;
   char *argv[4];
   char command_line[255];
   char savedjobname[JOB_NAME_LEN+1];
   char savedalerttext[JOB_LOGMSG_LEN+1];

   if ((alerttype != 0) && (alerttype != 1)) {
      return( 0 );
   }

   /* take a local copy of key fields, pointers may not survive the fork as the
	* main program is going to be overwriting the addresses pointed to */
   strncpy( savedjobname, alert_rec->job_details.job_header.jobname, JOB_NAME_LEN );
   savedjobname[JOB_NAME_LEN] = '\0';
   strncpy( savedalerttext, alert_rec->job_details.text, JOB_LOGMSG_LEN );
   savedalerttext[JOB_LOGMSG_LEN] = '\0';

   /* fork a copy of ourselves to run the alert forwarding program or script */
   pid_number = fork( );
   if (pid_number == -1) { /* fork failed */
      myprintf( "*ERR: AE025-Failed to fork during alert program call\n" );
      return( 0 );
   }
   if (pid_number != 0) { /* we are the parent, so we are completed */
      /* try and save the alert task pid in a free slot, do not worry
       * if we cannot. */
      i = saved = 0;
      while ( (i < MAX_RUNNING_ALERTS) && (saved == 0) ) {
         if (running_alert_table.running_alerts[i].alert_pid == 0) {
            running_alert_table.running_alerts[i].alert_pid = pid_number;
            saved = 1;
         }
		 i++;
      }
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
         myprintf( "INFO: AI004-Running external alert forwarder for %s, alerttype %d.\n",
                  alert_rec->job_details.job_header.jobname, alerttype );
      }
      return( 1 ); /* done OK */
   }

   /* build the command line */
   if (alerttype == 0) { /* cancelling an alert */
     snprintf( command_line, 254, "%s %s \"Alert cleared on job scheduler server\"",
               pSCHEDULER_CONFIG_FLAGS.local_execprog_cancel,
               savedjobname );
   }
   else { /* raising an alert */
     snprintf( command_line, 254, "%s %s \"%s\"",
               pSCHEDULER_CONFIG_FLAGS.local_execprog_raise,
               savedjobname, savedalerttext );
   }

   /* run the program */
   argv[0] = "/bin/bash";
   argv[1] = "-c";
   argv[2] = (char *)&command_line;
   argv[3] = 0;
   execve( "/bin/bash", argv, environ );
   exit( 127 );  /* should never get here */
} /* ALERTS_LOCAL_run_alertprog */

/* -------------------------------------------------------
   ALERTS_CUSTOM_set_alert_text
   Allow the user to define the text of an alert message
   in alerts_custom.txt
   This routine reads the custom alerts file for a match
   of a line in the file with the exit_code number that
   has been passed to it.
   ------------------------------------------------------- */
void ALERTS_CUSTOM_set_alert_text( int exit_code, char * buffer, int bufferlen,
                                   char * sevflag, char * jobname ) {
   FILE * fhandle;
   char filename[] = "alerts_custom.txt";
   int lasterror, foundit, linebytes, number;
   char *linebuf, *ptr, *ptr2;

   /* Open the user customised alert message file as normal text input */
   if ( (fhandle = fopen( filename, "r" )) == NULL ) {
      lasterror = errno;
      if ( (lasterror == 2) || (lasterror == 3) ) {   /* doesn't exist */
         snprintf( buffer, bufferlen, "JOB %s: alerts_%s does not exist, cannot get text", jobname, filename );
      }
      else {
         snprintf( buffer, bufferlen, "JOB %s: error %d on %s, cannot get text", jobname, lasterror, filename );
      }
      return;
   }

   /* read through the file for the matching error number */
   foundit = 0;
   if ( (linebuf = (char *)MEMORY_malloc(2048,"ALERTS_CUSTOM_set_alert_text")) == NULL ) {
      myprintf(  "*ERR: AE026-Unable to allocate 2048 bytes of memory (in ALERTS_CUSTOM_set_alert_text)\n" );
      snprintf( buffer, bufferlen, "JOB %s: insufficient memory to get msgtext", jobname );
      fclose( fhandle );
      return;
   }
   linebytes = 1; /* so gets through start of loop */
   lasterror = 0;
   while ( (!(ferror(fhandle))) && (!(feof(fhandle))) && (foundit == 0) ) {
      if ( fgets( linebuf, 2048, fhandle ) != NULL ) {
         number = atoi( linebuf );
	     if ( (number >= 150) && (number < 199) ) { /* a valid line with a valid range */
            if (number == exit_code) {
                foundit = 1;   /* found it */
                ptr = linebuf;
                ptr = ptr + 4; /* skip nnn_  to get to sevflag */
				*sevflag = *ptr;
				ptr = ptr + 2; /* skip n_ to get to text */
            }
         }
      }
   }

   /* set the message */
   if (foundit == 0) {
      snprintf( buffer, bufferlen, "JOB %s: No text for cc=%d,%s", jobname, exit_code, filename );
   }
   else {
      /* try and remove the newline byte from end of message first */
      ptr2 = ptr;
      ptr2 = (ptr2 + strlen(ptr)) - 1;
      if (ptr2 > ptr) {
          *ptr2 = '\0';
      }
      snprintf( buffer, bufferlen, "JOB %s: %s", jobname, ptr );
   }

   /* free memory and close the file */
   MEMORY_free( linebuf );
   fclose( fhandle );

   /* done, so return */
   return;
}
