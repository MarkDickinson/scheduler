/*
    JOBSLIB.C

    The scheduler component that manages the job database.

    Scheduler is by Mark Dickinson, 2001
*/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include "debug_levels.h"
#include "scheduler.h"
#include "api.h"
#include "jobslib.h"
#include "schedlib.h"
#include "utils.h"
#include "calendar.h"
#include "memorylib.h"

/* ---------------------------------------------------
   JOBS_Initialise
   Perform all the initialisation required for the
   component that manages the jobs definition file.
   --------------------------------------------------- */
int JOBS_Initialise( void ) {
   if ( (job_handle = fopen( job_file, "rb+" )) == NULL ) {
	  if (errno == 2) {
         if ( (job_handle = fopen( job_file, "ab+" )) == NULL ) {
            perror( "Create of jobs file failed" );
            myprintf( "*ERR: JE001-Unable to create jobs file %s, errno %d (JOBS_Initialise)\n", job_file, errno );
            return 0;
		 }
		 else {
		    /* created, close and open the way we want */
		    fclose( job_handle );
			job_handle = NULL;
		    return( JOBS_Initialise() );
		 }
	  }
	  else {
         myprintf( "*ERR: JE002-Unable to open jobs file %s, errno %d (JOBS_Initialise)\n", job_file, errno );
         perror( "Open of jobs file failed" );
         return 0;
	  }
   }
   else {
      fseek( job_handle, 0, SEEK_SET );
	  if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
         myprintf( "INFO: JI001-Open of jobs file %s completed\n", job_file );
	  }
      return 1;
   }
} /* JOBS_Initialise */


/* ---------------------------------------------------
   JOBS_Terminate
   Perform all cleanup required on the jobs file.
   --------------------------------------------------- */
void JOBS_Terminate( void ) {
   if (job_handle != NULL) {
      fclose( job_handle );
	  job_handle = NULL;
	  if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
         myprintf( "INFO: JI002-Closed: Jobs file\n" );
	  }
   }
   else {
      myprintf( "*ERR: JE003-JOBS_Terminate called when no jobs files were open, fclose ignored ! (JOBS_Terminate)\n" );
   }
} /* JOBS_Terminate */


/* ---------------------------------------------------
   JOBS_read_record
   Read an entry from the job definition file.

   If the record requested is not found return -1.
   If it was found return the record number and place
   the record found in the callers datarec buffer.
   --------------------------------------------------- */
long JOBS_read_record( jobsfile_def * datarec ) {
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered JOBS_read_record, and leaving (one line)\n" );
   }
   return( UTILS_read_record( job_handle, (char *) datarec, sizeof(jobsfile_def), "Job definition file", "JOBS_read_record", 1 ) );
} /* JOBS_read_record */


/* ---------------------------------------------------
   JOBS_write_record
   Write a record to the job definition file.

   If recpos is -1 we write the record to the end of
   the file, otherwise we write the record (update)
   the record at the location specified.

   We return the record number the record was written
   to if everything was ok. If there were any errors
   we return -1.
   --------------------------------------------------- */
long JOBS_write_record( jobsfile_def * datarec, long recpos ) {
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered JOBS_write_record, and leaving (three lines)\n" );
   }
   /* Check the fields in the record are legal */
   UTILS_space_fill_string( datarec->job_header.jobname, JOB_NAME_LEN );
   strncpy( datarec->job_header.jobnumber, "000000", JOB_NUMBER_LEN );
   datarec->description[JOB_DESC_LEN] = '\0';

   /* Write it */
   return( UTILS_write_record_safe( job_handle, (char *) datarec, sizeof(jobsfile_def), recpos, "Job definition file", "JOBS_write_record" ) );
} /* JOBS_write_record */

/* ---------------------------------------------------
   JOBS_delete_record
   Delete a record from the job definition file.

   We return -1 on an error, recnum altered on success.
   Notes: NOW Compress is being called recornum returned 
          should never be used by the caller.
   --------------------------------------------------- */
short int JOBS_delete_record( jobsfile_def * datarec ) {
   long recordnum;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered JOBS_delete_record\n" );
   }

   recordnum = UTILS_read_record( job_handle, (char *)datarec, sizeof(jobsfile_def), "Job definition file", "JOBS_delete_record", 1 );
   if (recordnum < 0) {
       UTILS_set_message( "RECORD TO BE DELETED WAS NOT FOUND !\n" );
       return( -1 );
   }
   datarec->job_header.info_flag = 'D';  /* flag as deleted */
   recordnum =  UTILS_write_record( job_handle, (char *) datarec, sizeof(jobsfile_def), recordnum, "Job definition file" );
   
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving JOBS_delete_record\n" );
   }
   return( recordnum );
} /* JOBS_delete_record */

/* ---------------------------------------------------
   Format the contents of a job record into a buffer
   as a large multiline display datablock.

   The buffer passed should be at least MAX_API_DATA_LEN
   in size.

   Note: we use a work area to calculate line sizes
   to avoid overflowing the target output buffer, we
   continuously check the work line size against the
   remaining buffer space before adding the line.
   --------------------------------------------------- */
long JOBS_format_for_display( jobsfile_def * datarec, char * outbuf, int bufsize ) {
   /*
      returns the length of the display string
   */
   char *ptr1, *ptr2;
   long buffspaceleft;
   char workbuf[129];
   int  worklen;
   int i;
   char daystr[4];

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered JOBS_format_for_display\n" );
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_VARDUMP) {
      JOBS_DEBUG_dump_jobrec( datarec );
   }

   ptr1 = outbuf;
   ptr2 = outbuf;
   
   if (bufsize < 100) {
      myprintf( "*ERR: JE004-Buffer size passed in call is too small, contact programmer (JOBS_format_for_display)\n" );
      return ( 0 );  /* cant even return an error */
   }
   else {
      /* we need one byte of this thanks */
      buffspaceleft = bufsize - 1;
   }

   /* --- jobname --- */
   worklen = snprintf( workbuf, 128, "Job Name      : %s\n", datarec->job_header.jobname );
   if (worklen != -1) {
      if (worklen < buffspaceleft) {
         strncpy( ptr2, workbuf, worklen );
         ptr2 = ptr2 + worklen;
         buffspaceleft = buffspaceleft - worklen;
      }
      else {
         myprintf( "*ERR: JE005-Buffer space exhausted, contact programmer (JOBS_format_for_display)\n" );
         worklen = snprintf( ptr1, bufsize, "Insufficient server buffer space for command\n, contact programmer\n" );
         UTILS_set_message( "Insufficient server buffer space for command\n, contact programmer" );
         return (worklen);
      }
   }

   /* --- jobnumber --- */
   worklen = snprintf( workbuf, 128, "Job Number    : %s\n", datarec->job_header.jobnumber );
   if (worklen != -1) {
      if (worklen < buffspaceleft) {
         strncpy( ptr2, workbuf, worklen );
         ptr2 = ptr2 + worklen;
         buffspaceleft = buffspaceleft - worklen;
      }
      else {
         myprintf( "*ERR: JE005-Buffer space exhausted, contact programmer (JOBS_format_for_display)\n" );
         worklen = snprintf( ptr1, bufsize, "Insufficient server buffer space for command\n, contact programmer\n" );
         UTILS_set_message( "Insufficient server buffer space for command\n, contact programmer" );
         return (worklen);
      }
   }

   /* --- last run time --- */
   worklen = snprintf( workbuf, 128, "Last ran at   : %s\n", datarec->last_runtime );
   if (worklen != -1) {
      if (worklen < buffspaceleft) {
         strncpy( ptr2, workbuf, worklen );
         ptr2 = ptr2 + worklen;
         buffspaceleft = buffspaceleft - worklen;
      }
      else {
         myprintf( "*ERR: JE005-Buffer space exhausted, contact programmer (JOBS_format_for_display)\n" );
         worklen = snprintf( ptr1, bufsize, "Insufficient server buffer space for command\n, contact programmer\n" );
         UTILS_set_message( "Insufficient server buffer space for command\n, contact programmer" );
         return (worklen);
      }
   }

   /* --- next run time --- */
   worklen = snprintf( workbuf, 128, "Scheduled for : %s\n", datarec->next_scheduled_runtime );
   if (worklen != -1) {
      if (worklen < buffspaceleft) {
         strncpy( ptr2, workbuf, worklen );
         ptr2 = ptr2 + worklen;
         buffspaceleft = buffspaceleft - worklen;
      }
      else {
         myprintf( "*ERR: JE005-Buffer space exhausted, contact programmer (JOBS_format_for_display)\n" );
         worklen = snprintf( ptr1, bufsize, "Insufficient server buffer space for command\n, contact programmer\n" );
         UTILS_set_message( "Insufficient server buffer space for command\n, contact programmer" );
         return (worklen);
      }
   }

   /* --- job owner --- */
   worklen = snprintf( workbuf, 128, "Job User Id   : %s\n", datarec->job_owner );
   if (worklen != -1) {
      if (worklen < buffspaceleft) {
         strncpy( ptr2, workbuf, worklen );
         ptr2 = ptr2 + worklen;
         buffspaceleft = buffspaceleft - worklen;
      }
      else {
         myprintf( "*ERR: JE005-Buffer space exhausted, contact programmer (JOBS_format_for_display)\n" );
         worklen = snprintf( ptr1, bufsize, "Insufficient server buffer space for command\n, contact programmer\n" );
         UTILS_set_message( "Insufficient server buffer space for command\n, contact programmer" );
         return (worklen);
      }
   }

   /* --- program to execute --- */
   worklen = snprintf( workbuf, 128, "Program Name  : %s\n", datarec->program_to_execute );
   if (worklen != -1) {
      if (worklen < buffspaceleft) {
         strncpy( ptr2, workbuf, worklen );
         ptr2 = ptr2 + worklen;
         buffspaceleft = buffspaceleft - worklen;
      }
      else {
         myprintf( "*ERR: JE005-Buffer space exhausted, contact programmer (JOBS_format_for_display)\n" );
         worklen = snprintf( ptr1, bufsize, "Insufficient server buffer space for command\n, contact programmer\n" );
         UTILS_set_message( "Insufficient server buffer space for command\n, contact programmer" );
         return (worklen);
      }
   }

   /* --- program parameters --- */
   worklen = snprintf( workbuf, 128, "Program Parms : %s\n", datarec->program_parameters );
   if (worklen != -1) {
      if (worklen < buffspaceleft) {
         strncpy( ptr2, workbuf, worklen );
         ptr2 = ptr2 + worklen;
         buffspaceleft = buffspaceleft - worklen;
      }
      else {
         myprintf( "*ERR: JE005-Buffer space exhausted, contact programmer (JOBS_format_for_display)\n" );
         worklen = snprintf( ptr1, bufsize, "Insufficient server buffer space for command\n, contact programmer\n" );
         UTILS_set_message( "Insufficient server buffer space for command\n, contact programmer" );
         return (worklen);
      }
   }

   /* loop through the dependency chain */
   for (i = 0; i < MAX_DEPENDENCIES; i++) {
      if (datarec->dependencies[i].dependency_type != '0') {
         if (datarec->dependencies[i].dependency_type == '1') {
            worklen = snprintf( workbuf, 128, "Depends upon Job %s completing\n", datarec->dependencies[i].dependency_name );
         }
         else if (datarec->dependencies[i].dependency_type == '2') {
            worklen = snprintf( workbuf, 128, "Depends upon File %s being produced\n", datarec->dependencies[i].dependency_name );
         }
         else {
            worklen = snprintf( workbuf, 128, "**** UNSUPPORTED DEPENDENCY TYPE : %c ****\n", datarec->dependencies[i].dependency_type );
         }
         if (worklen != -1) {
            if (worklen < buffspaceleft) {
               strncpy( ptr2, workbuf, worklen );
               ptr2 = ptr2 + worklen;
               buffspaceleft = buffspaceleft - worklen;
            }
            else {
               myprintf( "*ERR: JE005-Buffer space exhausted, contact programmer (JOBS_format_for_display)\n" );
               worklen = snprintf( ptr1, bufsize, "Insufficient server buffer space for command\n, contact programmer\n" );
               UTILS_set_message( "Insufficient server buffer space for command\n, contact programmer" );
               return (worklen);
            }
         } 
      } /* if dependency type != 0 */
   }    /* for loop */

   /* does this job use a calendar */
   if (datarec->use_calendar == '0') {      /* no special handling */
      worklen = snprintf( workbuf, 128, "This job runs daily, and does not use a calendar file\n" );
   }
   else if (datarec->use_calendar == '1') { /* uses a calendar */
      worklen = snprintf( workbuf, 128, "This job uses calendar %s\n", datarec->calendar_name );
   }
   else if (datarec->use_calendar == '2') {  /* weekday selection */
      strcpy( workbuf, "Job runs on the days: " );
	  for (i = 0; i < 7; i++) {
			  if (datarec->crontype_weekdays[i] == '1') {
					  UTILS_get_dayname( i, (char *)&daystr );
					  strcat( workbuf, daystr );
					  strcat( workbuf, " " );
			  }
	  }
	  strcat( workbuf, "\n" );
	  worklen = strlen(workbuf);
   }
   else if (datarec->use_calendar == '3') {  /* repeating job */
      worklen = snprintf( workbuf, 128, "This job repeats execution aproximately every %s minutes\n",
                          datarec->requeue_mins );
   }
   else {
      worklen = snprintf( workbuf, 128, "**WARNING** The calendar flag for this job is illegal\n" );
   }
   if (worklen != -1) {
      if (worklen < buffspaceleft) {
         strncpy( ptr2, workbuf, worklen );
         ptr2 = ptr2 + worklen;
         buffspaceleft = buffspaceleft - worklen;            
      }
      else {
         myprintf( "*ERR: JE005-Buffer space exhausted, contact programmer (JOBS_format_for_display)\n" );
         worklen = snprintf( ptr1, bufsize, "Insufficient server buffer space for command\n, contact programmer\n" );
         UTILS_set_message( "Insufficient server buffer space for command\n, contact programmer" );
         return (worklen);
      }
   }

   /* --- job description --- */
   datarec->description[JOB_DESC_LEN] = '\0';  /* Force field termination, missed in earlier adds some some records iffy */
   worklen = snprintf( workbuf, 128, "Description   : %s\n", datarec->description );
   if (worklen != -1) {
      if (worklen < buffspaceleft) {
         strncpy( ptr2, workbuf, worklen );
         ptr2 = ptr2 + worklen;
         buffspaceleft = buffspaceleft - worklen;
      }
      else {
         myprintf( "*ERR: JE005-Buffer space exhausted, contact programmer (JOBS_format_for_display)\n" );
         worklen = snprintf( ptr1, bufsize, "Insufficient server buffer space for command\n, contact programmer\n" );
         UTILS_set_message( "Insufficient server buffer space for command\n, contact programmer" );
         return (worklen);
      }
   }

   /* Show the catchup flag for this job */
   if (datarec->job_catchup_allowed == 'N') {
      worklen = snprintf( workbuf, 128, "** This job is excluded from scheduler catchup processing.\n" );
      if (worklen != -1) {
         if (worklen < buffspaceleft) {
            strncpy( ptr2, workbuf, worklen );
            ptr2 = ptr2 + worklen;
            buffspaceleft = buffspaceleft - worklen;
         }
         else {
            myprintf( "*ERR: JE005-Buffer space exhausted, contact programmer (JOBS_format_for_display)\n" );
            worklen = snprintf( ptr1, bufsize, "Insufficient server buffer space for command\n, contact programmer\n" );
            UTILS_set_message( "Insufficient server buffer space for command\n, contact programmer" );
            return (worklen);
         }
      }
   }

   worklen = ptr2 - ptr1;
   *ptr2 = '\0';

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_VARDUMP) {
      myprintf( "DEBUG: ---------------------------------------------------------\n" );
      myprintf( "DEBUG:JOBS_format_for_display, dumping buffer built...\n" );
      myprintf( "%s\n", ptr1 );
      myprintf( "DEBUG: ---------------------------------------------------------\n" );
      myprintf( "DEBUG:Leaving JOBS_format_for_display, buffer size is %d\n", worklen );
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving JOBS_format_for_display\n" );
   }
   return(worklen);
} /* JOBS_format_for_display */

/* ---------------------------------------------------
   JOBS_format_for_display_API
   Use the data in an API request data buffer to read a job for display and place the display into
   the API buffer.
   --------------------------------------------------- */
void JOBS_format_for_display_API( API_Header_Def * api_bufptr ) {
   jobsfile_def * workbuffer;
   int datalen, recordnum;
		
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered JOBS_format_for_display_API\n" );
   }

   if ( (workbuffer = (jobsfile_def *)MEMORY_malloc(sizeof(jobsfile_def),"JOBS_format_for_display_API")) == NULL) { 
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "JOB" );
      strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
      strcpy( api_bufptr->API_Data_Buffer, "*E* Insufficient free memory on server to execute this command\n" );
      API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
   }
   else {  
      memcpy( workbuffer, api_bufptr->API_Data_Buffer, sizeof(jobsfile_def) );
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "JOB" );
      strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
	  UTILS_space_fill_string( workbuffer->job_header.jobname, JOB_NAME_LEN );
      recordnum = JOBS_read_record( workbuffer );
      if (recordnum < 0) {
         if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_IO) {
		    myprintf( "DEBUG: JOBS_format_for_display_API: JOBS_read_record failed to find record\n" );
		 }
         strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
         strcpy( api_bufptr->API_Data_Buffer, "*E* JOB RECORD NOT FOUND, OR JOB DATABASE ERROR\n" );
         API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
      }       
	  else if (workbuffer->job_header.info_flag == 'D') {
         if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_IO) {
		    myprintf( "DEBUG: JOBS_format_for_display_API: Record found but deleted flag set\n" );
		 }
         strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
         strcpy( api_bufptr->API_Data_Buffer, "*E* JOB RECORD IS FLAGGED AS DELETE PENDING, NOT DISPLAYED\n" );
         API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
	  }
      else {  
         if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_IO) {
			  myprintf( "DEBUG: JOBS_format_for_display_API: Record found and being processed\n" );
		 }
         datalen = JOBS_format_for_display( workbuffer, api_bufptr->API_Data_Buffer, MAX_API_DATA_LEN );
         API_set_datalen( (API_Header_Def *)api_bufptr, datalen );
       }
	  MEMORY_free( workbuffer );
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving JOBS_format_for_display_API\n" );
   }
}	/* JOBS_format_for_display_API */

/* ---------------------------------------------------
   JOBS_format_listall_for_display_API
   Read all job records and summarise in the API buffer
   display area.
   Allow mask matching for start of jobname in selection.
   --------------------------------------------------- */
void JOBS_format_listall_for_display_API( API_Header_Def * api_bufptr, FILE *tx ) {
   jobsfile_def * workbuffer;
   int lasterror;
   char *ptr3;
   char workbuf[129];
   char workbuf2[50];
   int  worklen;
   time_t time_number;
   char mask[JOB_NAME_LEN+1];
   int masklen;
   char extratext[10];
		
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered JOBS_format_listall_for_display_API\n" );
   }

   if ((api_bufptr->API_Data_Buffer[0] != '\0') && (api_bufptr->API_Data_Buffer[0] != '\n')) {
      strncpy( mask, api_bufptr->API_Data_Buffer, JOB_NAME_LEN );
      mask[JOB_NAME_LEN] = '\0';
      masklen = strlen(mask);
      UTILS_uppercase_string((char *)&mask);
   }
   else {
      masklen = 0;
   }
   
   if ( (workbuffer = (jobsfile_def *)MEMORY_malloc(sizeof(jobsfile_def),"JOBS_format_listall_for_display_API")) == NULL) { 
      API_init_buffer( (API_Header_Def *)api_bufptr ); 
      strcpy( api_bufptr->API_System_Name, "JOB" );
      strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
      strcpy( api_bufptr->API_Data_Buffer, "*E* Insufficient free memory on server to execute this command\n" );
      API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
      myprintf( "*ERR: JE006-Insufficicent memory for requested JOB operation (JOBS_format_listall_for_display_API)\n" );
	  return;
   }
   
   /*
    * Position to the begining of the file
    */
   lasterror = fseek( job_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      MEMORY_free(workbuffer);
      UTILS_set_message( "file seek error (job definition file)" );
      perror( "file seek error (job definition file)" );
      return;
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG: seek to start of file, jobs file (JOBS_format_listall_for_display_API)\n" );
   }

   /* initialise the reply api buffer */
   API_init_buffer( (API_Header_Def *)api_bufptr ); 
   strcpy( api_bufptr->API_System_Name, "JOB" );
   strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );
   
   /* loop until end of file */
   lasterror = 0; 
   while ( (!(ferror(job_handle))) && (!(feof(job_handle))) ) {
      lasterror = fread( workbuffer, sizeof(jobsfile_def), 1, job_handle  );
      if (  (!(feof(job_handle))) && (workbuffer->job_header.info_flag != 'D')  ) {
/* replace below with wildcard search */
/*         if ( (masklen == 0) || (memcmp(workbuffer->job_header.jobname,mask,masklen) == 0) ) { */
         if ( (masklen == 0) || (UTILS_wildcard_parse_string(workbuffer->job_header.jobname,mask) == 1) ) {
            if (workbuffer->calendar_error == 'Y') {
               strncpy( workbuf2, "CALENDAR ERROR(EXPIRED?)", 48 );
            }
            else {
               time_number = UTILS_make_timestamp( workbuffer->next_scheduled_runtime );
               snprintf( workbuf2, 48, "%s", ctime((time_t *)&time_number) );
               ptr3 = (char *)&workbuf2; /* the ctime puts a \n on the line, remove it */
               while( (*ptr3 != '\n') && (*ptr3 != '\0') ) { ptr3++; }
               *ptr3 = '\0';
            }
            if ( workbuffer->job_header.info_flag == 'S' ) {
               strcpy( extratext, "SYSTEM" );
            }
            else {
               strcpy( extratext, "" );
            }
            worklen = snprintf( workbuf, 121, "%s %s %s %s\n", workbuffer->job_header.jobname,
                      workbuf2, workbuffer->description, extratext );
            API_add_to_api_databuf( api_bufptr, workbuf, worklen, tx );
         }
	  } /* while not eof */
   } /* while not a file error or eof */

   API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
   
   MEMORY_free( workbuffer );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving JOBS_format_listall_for_display_API\n" );
   }
}	/* JOBS_format_listall_for_display_API */

/* ---------------------------------------------------
   JOBS_schedule_on
   Copy a record from the jobs definition file onto
   the schedulers active job queue file for pending
   execution.
   --------------------------------------------------- */
int JOBS_schedule_on( jobsfile_def * datarec ) {
   char msgbuffer[UTILS_MAX_ERRORTEXT];

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG:Entered JOBS_schedule_on, and leaving (one line if ok)\n" );
   }

   if (JOBS_read_record( datarec ) >= 0) {
      return (SCHED_schedule_on( datarec ));
   }

   /* else */
   snprintf( msgbuffer, UTILS_MAX_ERRORTEXT, "Unable to schedule on job %s", datarec->job_header.jobname );
   UTILS_set_message( msgbuffer );
   myprintf( "*ERR: JE007-%s\n", msgbuffer );
   return ( -1 );
} /* JOBS_schedule_on */


/* ---------------------------------------------------
   --------------------------------------------------- */
void JOBS_newday_scheduled( time_t time_number ) {
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG:Entered JOBS_newday_scheduled\n" );
   }
  /* run the newday forced, with the time of the newday job */
  JOBS_newday_forced( time_number );
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG:Leaving JOBS_newday_scheduled\n" );
   }
} /* JOBS_newday_scheduled */

/* ---------------------------------------------------
   --------------------------------------------------- */
void JOBS_newday_forced( time_t rundate ) {
  time_t time_number, store_rundate;
  int  lasterror, itemsread;
  jobsfile_def *local_rec;
  int  check_fail_count;
  long recordcount;  /* keep track or recnum as routines we call
						will reposition the file pointer */
  int done; /* as the procs we call change file pointers we can't rely on a valid eof in our loop */

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG:Entered JOBS_newday_forced\n" );
   }

  store_rundate = rundate;

  /*
   * the date we are to use for processing has been provided as rundate.
   * Check to see it's not in the future as that will be illegal.
   */
  time_number = time(0);     /* time now in seconds from 00:00, Jan 1 1970 */
  if (time_number < rundate) {
     UTILS_set_message( "New day request failed, requested date is in the future !" );
     myprintf( "*ERR: JE008-JOBS_Newday_Forced, New day request failed, requested date is in the future !\n" );
     return;
  }

  /*
   * for each record in the jobs file read the record, convert the next
   * run time to a time_t and compare it against the rundate time_t.
   */

  /* Position to the begining of the file */
  lasterror = fseek( job_handle, 0, SEEK_SET );
  if (lasterror != 0) {
     UTILS_set_message( "Seek to start of job definiton file failed" );
     myprintf("*ERR: JE009-JOBS_Newday_Forced, Seek to start of job definiton file failed\n" );
     return;
  }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG:Seek to start of jobs file (JOBS_newday_forced)\n" );
   }

  /* Allocate memory for a local copy of the record */
  if ((local_rec = (jobsfile_def *)MEMORY_malloc( sizeof( jobsfile_def ),"JOBS_newday_forced" )) == NULL) {
     UTILS_set_message( "Unable to allocate memory required for read operation" );
     myprintf( "*ERR: JE010-JOBS_Newday_Forced, Unable to allocate memory required for read operation\n" );
     return;
  }

  /* Loop reading through the file */
  /* Note: we use itemsread to determine if we are at eof, if 0 then no data
   * was read. I have found I need to do this as the seek reposition to after a
   * record submitted goes and clears the error and feof flags. We keep the
   * feof and ferror checks in as if itemsread is 0 we expect to fall back on
   * these checks. */
  recordcount = 0;
  itemsread = 1;
  done = 0;
  while (  (!(feof(job_handle)))  &&  (!(ferror(job_handle))) && (itemsread > 0)  && (done == 0)  ) {
      itemsread = fread( local_rec, sizeof(jobsfile_def), 1, job_handle  );
      if (ferror(job_handle)) {
         UTILS_set_message( "Read error on jobs definition file" );
         myprintf( "*ERR: JE011-JOBS_Newday_Forced, Read error on jobs definition file\n" );
         perror( "job_handle" );
         MEMORY_free( local_rec );
         return;
      }
	  /* if we get an eof it will be reset by some of the procedures we call,
	   * so indicate we are done NOW. */
      if (  (feof(job_handle)) || (ferror(job_handle)) || (itemsread == 0)  ) {
			  done = 1;   /* we definately stop after submiting this job */
	  }
	  if (itemsread > 0) { /* if its 0 then we didn't read anything */
	     /* remember, we want to point at the next record if we need
	      * to reposition */
	     recordcount++;
         /* we have the record, do we submit it ? */
         if (local_rec->job_header.info_flag != 'D') {  /* if D its in deletion state */
            check_fail_count = 0;
            check_fail_count = check_fail_count + JOBS_schedule_date_check( local_rec, store_rundate );
            check_fail_count = check_fail_count + JOBS_schedule_calendar_check( local_rec, store_rundate );
            if (local_rec->calendar_error == 'Y') { check_fail_count++; } /* above check didn't work because I obsoleted it */
            if (check_fail_count == 0) {
               lasterror = JOBS_schedule_on( local_rec );
               lasterror = fseek( job_handle, (recordcount * sizeof(jobsfile_def)), SEEK_SET );
            }
         } /* if <> D */
	  } /* if itemsread > 0 */
   } /* while not feof */

   MEMORY_free( local_rec );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG:Leaving JOBS_newday_forced\n" );
   }
   return;
} /* JOBS_newday_forced */


/* ---------------------------------------------------
   * if next runtime is less than the rundate time or is scheduled
   * to be run before the next newdays cutover date then it's a candidate
   * for being submitted.
   --------------------------------------------------- */
int JOBS_schedule_date_check( jobsfile_def *local_rec, time_t rundate ) {
  time_t record_nextrundate, localrundate;
  char msgbuffer[256];

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG:Entered JOBS_schedule_date_check\n" );
   }

  record_nextrundate = UTILS_make_timestamp( (char *)&local_rec->next_scheduled_runtime );
  if (record_nextrundate == -1) {
     snprintf( msgbuffer, 255, "Job %s has an invalid next rundate %s, not scheduled on",
              local_rec->job_header.jobname, local_rec->next_scheduled_runtime);
     UTILS_set_message( msgbuffer );
	 myprintf( "INFO: JI003-%s\n", msgbuffer );
     return( 1 );
  }

  /* localrundate is the end of the days window (+24hrs to date passed) */
  localrundate = rundate + (60 * 60 * 24); /* secs * min * hours */

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG:About to leave JOBS_schedule_date_check\n" );
  }
  if (record_nextrundate < localrundate) {
     if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
        myprintf( "INFO: JI004-Job due to run, cleared for schedule on; %s\n", local_rec->job_header.jobname );
	 }
     return( 0 );  /* needs to be scheduled on */
  }
  else {
     return( 1 );  /* do not schedule on */
  }
} /* JOBS_schedule_date_check */


/* ---------------------------------------------------
   * if the job is a candidate check to see if it is excluded from running
   * by a holidays calendar file. If not excluded schedule the job to run.
   --------------------------------------------------- */
int JOBS_schedule_calendar_check( jobsfile_def *local_rec, time_t rundate ) {

/*
 * This should be done at the job add time and the next legal time
 * also worked out when the job completes (correct timestamp for
 * next legal run should be inthe job record).
 * Even thoughthis should be obsolete I will leave it around and
 * put code in it later as an additional check.
 */
  if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG:Entered JOBS_schedule_calendar_check (non-functional/obsolete)\n" );
  }

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG:Leaving JOBS_schedule_calendar_check (non-functional/obsolete)\n" );
  }

  /* Allow the job scheduling on to proceed */
  return( 0 );
} /* JOBS_schedule_calendar_check */

/* ---------------------------------------------------
   --------------------------------------------------- */
void JOBS_DEBUG_dump_jobrec( jobsfile_def * job_bufptr ) {
		myprintf( "  job_header...\n" );
		myprintf( "     jobnumber : %s\n", job_bufptr->job_header.jobnumber );
		myprintf( "     jobname   : %s\n", job_bufptr->job_header.jobname );
		myprintf( "     info_flag : %c\n", job_bufptr->job_header.info_flag );
		myprintf( "  job datafields...\n" );
		myprintf( "    last_runtime          : %s\n", job_bufptr->last_runtime );
		myprintf( "    next_scheduled_runtime: %s\n", job_bufptr->next_scheduled_runtime );
		myprintf( "    job_owner             : %s\n", job_bufptr->job_owner );
		myprintf( "    program_to_execute    : %s\n", job_bufptr->program_to_execute );
		myprintf( "    program_parameters    : %s\n", job_bufptr->program_parameters );
		myprintf( "    calendar flag         : %c\n", job_bufptr->use_calendar );
		myprintf( "------------------------------------------------------\n" );
} /* JOBS_DEBUG_dump_jobrec */

/* ---------------------------------------------------
 * Called by the server when a job has completed
 * execution. It will update the last run-date,
 * and work out the new run date.
 * If the job is setup to repeat execute we will
 * return 1 to the server, and a new run time so
 * the server can adjust the existing queue
 * record to the new time.
 * If the job is an ordinary job we return 0 to
 * advise the server there is no more work needed
 * by it for this job.
 * If we return 2 the server needs to raise an alert.
   --------------------------------------------------- */
int JOBS_completed_job_time_adjustments( active_queue_def * datarec ) {
   int return_flag;
   int local_requeue_mins;
   long job_recordnum;
   time_t time_number, time_number2;
   time_t next_scheduler_configured_newday, next_scheduler_real_newday;
   time_t job_last_scheduled_time, job_next_scheduled_time;
   jobsfile_def local_rec;   /* not a pointer */
   char saved_prefered_time[9]; /* to use when the time is set to avoid the datestamp
								   being replaced with the end of job runtime timestamp */
   char *ptr1;  /* pointer index into datestamp */
   char msgbuffer[71];
   int probably_skipped_due_to_catchup_off;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG:Entered JOBS_completed_job_time_adjustments\n" );
   }

   /* -- default return flag is no action required by scheduler, if set to 1
    * then the job being processed will be resubmitted, this MUST ONLY be used
    * in the processing for 'repeating' jobs. */
   return_flag = 0;

   /* Used to indicate if we may have skipped one or more executions */
   probably_skipped_due_to_catchup_off = 0;

   /*
    * Read the job definition information.
    */
   strncpy( local_rec.job_header.jobnumber, datarec->job_header.jobnumber, JOB_NUMBER_LEN );
   strncpy( local_rec.job_header.jobname, datarec->job_header.jobname, JOB_NAME_LEN );
   job_recordnum = JOBS_read_record( (jobsfile_def *)&local_rec );
   if (job_recordnum == -1) {
      snprintf( msgbuffer, 70, "JOB %s: UNABLE TO READ JOB RECORD", datarec->job_header.jobname );
      UTILS_set_message( msgbuffer );
	  myprintf( "*ERR: JE012-%s\n", msgbuffer );
      return( 2 ); /* alert is needed */
   }

   /* For the job completion time use the time it was supposed to run rather
	* than the actual time for next rundate calculations so the next
	* scheduled time will be correct. as the job may have run late. */
   ptr1 = (char *)&local_rec.next_scheduled_runtime + 9; /* skip date and space, address time */
   strncpy( saved_prefered_time, ptr1, 8 );

   /* set some of the time values we need */
   next_scheduler_configured_newday = UTILS_make_timestamp( (char *)&pSCHEDULER_CONFIG_FLAGS.next_new_day_run );
   next_scheduler_real_newday = 0;
   job_last_scheduled_time = UTILS_make_timestamp( (char *)&local_rec.next_scheduled_runtime );
   job_next_scheduled_time = 0;

   if ( (pSCHEDULER_CONFIG_FLAGS.catchup_flag == '2') &&  /* catchup on, every date from last run */
        (local_rec.job_catchup_allowed == 'Y') ) {       /* job catchup allows it to be repeated */
      job_next_scheduled_time = UTILS_make_timestamp( (char *)&local_rec.next_scheduled_runtime );
      next_scheduler_real_newday = next_scheduler_configured_newday;
   }
   else {  /* catchup off, next scheduled time after current time */
      /* Check current date against newday date, if newday is in the past it is
       * still doing catchups, so find out when the newday will be when the
       * catchups have finished and use that as the basis of the job next
       * runtime. */
      time_number = time(0);
      time_number2 = UTILS_make_timestamp( (char *)&pSCHEDULER_CONFIG_FLAGS.next_new_day_run );
	  time_number2 = time_number2 + (60 * 60 * 23); /* allow possibility of newday running 23hrs late */
      if (time_number2 < time_number) {
         probably_skipped_due_to_catchup_off = 1;
      }
      while (time_number2 < time_number) {
         /* walk up the newday time from what it is actually set to to what it
          * will next be set to */
         time_number2 = time_number2 + (60 * 60 * 24); /* increment one day */
      }
      next_scheduler_real_newday = time_number2 - (60 * 60 * 23); /* remember to adjust back 23hrs added above */
      UTILS_datestamp( (char *) &local_rec.next_scheduled_runtime, next_scheduler_real_newday );
      strncpy( ptr1, saved_prefered_time, 8 ); 
      job_next_scheduled_time = UTILS_make_timestamp( (char *)&local_rec.next_scheduled_runtime );

      /* If a repeating job with catchup off it needs special handling. This is because
       * the time we now have is the hour:minute it was last supposed to run, but a repeating
       * job probably wants to run earlier.. ie: if scheduler previously shutdown at 20:00 on 17th
       * and started again on 20th at 10:00 the repeat job will be set to 20:00 on the 20th; if it
       * runs every 15 mins for eaxample we want it requeued to just after the newday time (assume 6:00)
       * rater than it not starting until 20:00. */
      if (local_rec.use_calendar == '3') { /* its a repeating job at nn minute intervals */
         /* BUG FIX: an error in input where repeat = 0 will go into an endless
		  * loop here, do not allow that. */
         local_requeue_mins = atoi( local_rec.requeue_mins );
         if (local_requeue_mins < 1) {
             myprintf( "*ERR: JE026-ILLEGAL REPEAT INTERVAL=0, JOB %s (JOBS_completed_job_time_adjustments)",
                       local_rec.job_header.jobname );
	         return ( 2 ); /* alert needed, die out immediately */
             myprintf( "*ERR: JE027-FATAL, SERVER IMMEDIATE ABORT (JOBS_completed_job_time_adjustments)" );
             exit( 1 ); /* if we don't return, crash scheduler to stop loop */
		 }
         /* count back at repeat intervals until before the adjusted newday time, then add
          * one interval to put it just past it. time_number2 has the expected next valid
          * newday time, time_number the current exptected next runtime. */
         while( job_next_scheduled_time > next_scheduler_real_newday ) {
            job_next_scheduled_time = job_next_scheduled_time - (60 * local_requeue_mins);  /* count back to newdaytime */
         }

         /* now is set to first possible time just before next real newday */
         /* will be inc'ed past the newday by the normal processing below */ 

         /* MID: 2003/07/21-additional check, if configured newday < 24hrs
          * before the real newday move it back 24hrs, this way the repeating
          * jobs will start executing as soon as possible after the
          * delayed/pending newday completes so we will miss as few as possible
          * of them, they can run in the window between when the scheduler
          * finally executes the outstanding newday processing and the time the
          * real next newday processing is done, which could be up to 23hrs
          * 59minutes so we don't want them queued out to the real day all the
          * time here. */
          if ((next_scheduler_real_newday - next_scheduler_configured_newday) <= (60 * 60 * 24)) {
             job_next_scheduled_time = job_next_scheduled_time - (60 * 60 * 24);
          }
      }

      /* Note: in the normal processing of a job we add 24hrs to get it to the
       * next days runtime, so delete now so it works later. */
      if (job_next_scheduled_time > next_scheduler_real_newday ) {
         job_next_scheduled_time = job_next_scheduled_time - (60 * 60 * 24);
      }

      /* store for later reference */
      UTILS_datestamp( (char *) &local_rec.next_scheduled_runtime, job_next_scheduled_time );

      /* If a job was scheduled catchup-off due to the job flag
       * rather than to the global flag we should let the users
       * know that just in case they expected it to run. */
      if (local_rec.job_catchup_allowed != 'Y') {
         if (probably_skipped_due_to_catchup_off == 1) {
            myprintf( "JOB-: JI008-%s has catchup off at job level, skipping catchup\n",local_rec.job_header.jobname );
         }
      }
   } /* else catchup off, next time after current time */

   /* NOW: at this point job_next_scheduled_time contains an adjusted value of
    * when the job would have last run if the scheduler had been running all the 
    * time. scheduler configured and real newday times are correct. */
   if ( (job_next_scheduled_time == 0) || (job_last_scheduled_time == 0) ||
        (next_scheduler_configured_newday == 0) || (next_scheduler_real_newday == 0)) {
      /* well I made a mistake somewhere didn't I. RUN AS IF CATCHUP WAS ALLOWED */   
      myprintf( "ERR-: JE013-INTERNAL CATCHUP PROCESSING LOGIC ERROR, KEY=CATCHP_001\n" );
      myprintf( "ERR-: JE013-JOB %s WILL BE PROCESSED WITH CATCHUP ALLOWED\n",local_rec.job_header.jobname );
      myprintf( "ERR-: JE013-CONTACT VENDOR, DETAILS %d %d %d %d\n",
                next_scheduler_configured_newday, next_scheduler_real_newday,
                job_last_scheduled_time, job_next_scheduled_time );
      next_scheduler_configured_newday = UTILS_make_timestamp( (char *)&pSCHEDULER_CONFIG_FLAGS.next_new_day_run );
      next_scheduler_real_newday = next_scheduler_configured_newday;
      job_last_scheduled_time = UTILS_make_timestamp( (char *)&local_rec.next_scheduled_runtime );
      job_next_scheduled_time = job_last_scheduled_time;
      myprintf( "ERR-: JE014-CONTACT VENDOR, SANITY FORCED DETAILS %d %d %d %d\n",
                next_scheduler_configured_newday, next_scheduler_real_newday,
                job_last_scheduled_time, job_next_scheduled_time );
   }

   /* --------------------------------------------------------------------------------------
	* At this point we can do normal rescheduling of the job to the next
	* runtime as any adjustments required for 'catchup off' have been set in
	* our time variables correctly.
    * -------------------------------------------------------------------------------------- */
   if (local_rec.use_calendar == '1') { /* its a job using  a calendar file */
      UTILS_datestamp( (char *) &local_rec.next_scheduled_runtime, job_next_scheduled_time );
      if ( (pSCHEDULER_CONFIG_FLAGS.catchup_flag == '2') &&  /* catchup on, every date from last run */
           (local_rec.job_catchup_allowed == 'Y') ) {       /* job catchup allows it to be repeated */
	     return_flag = CALENDAR_next_runtime_timestamp( (char *)&local_rec.calendar_name,
                                                        (char *) &local_rec.next_scheduled_runtime, 1 );
      }
      else {
	     return_flag = CALENDAR_next_runtime_timestamp( (char *)&local_rec.calendar_name,
                                                        (char *)&local_rec.next_scheduled_runtime, 0 );
      }
      if (return_flag == 0) {  /* calendar checking error */
         local_rec.calendar_error = 'Y';
		 myprintf( "*ERR: JE015-JOB %s CALENDAR ERROR, Will not be able to reschedule on until resolved\n", local_rec.job_header.jobname );
      }
	  return_flag = 0; /* we re-used this, set it back to what this procedure requires */
   }
   else if (local_rec.use_calendar == '2') { /* its a job using specific days */
      job_next_scheduled_time = job_next_scheduled_time + (60 * 60 * 24);  /* at least one day forward */
	  if (JOBS_find_next_run_day( (jobsfile_def *)&local_rec, job_next_scheduled_time ) != 0) {
         UTILS_set_message( "Unable to find a next run DAY, check server log (JOBS_completed_job_time_adjustments)" );
         myprintf( "*ERR: JE016-Unable to find a next run DAY, check server log (JOBS_completed_job_time_adjustments)" );
	     return_flag = 2; /* alert needed */
	  }
   }
   else if (local_rec.use_calendar == '3') { /* its a repeating job at nn minute intervals */
      /* BUG FIX: an error in input where repeat = 0 will go into an endless
	   * loop here, do not allow that. */
      local_requeue_mins = atoi( local_rec.requeue_mins );
      if (local_requeue_mins < 1) {
          myprintf( "*ERR: JE028-ILLEGAL REPEAT INTERVAL=0, JOB %s (JOBS_completed_job_time_adjustments)",
                     local_rec.job_header.jobname );
	      return ( 2 ); /* alert needed, die out immediately */
          myprintf( "*ERR: JE029-FATAL, SERVER IMMEDIATE ABORT (JOBS_completed_job_time_adjustments)" );
          exit( 1 ); /* if we don't return, crash scheduler to stop loop */
      }
      /* don't loop these during catchup processing as I ended up with one job
	   * trying all day to catchup which is not whats desired. Move it to the
	   * next interval after now. */
      time_number2 = time(0);
      time_number = job_next_scheduled_time + (60 * local_requeue_mins);  /* 60 seconds * nn minutes */
	  while( time_number < time_number2 ) {
              time_number = time_number + (60 * local_requeue_mins);  /* inc over any catchups */
	  }
      job_next_scheduled_time = time_number;
      UTILS_datestamp( (char *) &local_rec.next_scheduled_runtime, job_next_scheduled_time );
	  /* Do NOT set return flag for a rerun, we do that in later work */
   }
   else {   /* a normal job, repeating every 24hrs */
      /* first save the time part of the original to prevent getting the time
       * now in the next runtime in case we were kicked off by a runnow. */
      /* get the time now information, and adjust 24hrs forward */
      job_next_scheduled_time = job_next_scheduled_time + (60 * 60 * 24);  /* 60 seconds * 60 minutes * 24 hours */
      UTILS_datestamp( (char *) &local_rec.next_scheduled_runtime, job_next_scheduled_time );
   }

   /* NOW: at this point the job record next run date will be set correctly */

   /* the last runtime field will be updated with the true completion time now */
   time_number = time(0);
   UTILS_datestamp( (char *) &local_rec.last_runtime, time_number );

   /* NOW: update the job definition file with the new times */
   job_recordnum = JOBS_write_record( &local_rec, job_recordnum );
   if (job_recordnum == -1) {
      snprintf( msgbuffer, 70, "JOB %s: UNABLE TO UPDATE JOB MASTER RECORD (JOBS_completed_job_time_adjustments)", datarec->job_header.jobname );
      UTILS_set_message( msgbuffer );
	  myprintf( "*ERR: JE017-%s\n", msgbuffer );
	  return_flag = 2;
   }

   /* We need to check if the job is a repeat job that will run next before
	* the new day time, is so we must ask the server to schedule it on again.
	* */
   if ((local_rec.use_calendar == '3') && (return_flag != 2)) {    /* a repeating job and no alerts yet */
      /* If repeating job time < newday time, need to resubmit it */
	  time_number = UTILS_make_timestamp( (char *)&local_rec.next_scheduled_runtime );
      if (time_number <= next_scheduler_configured_newday ) {
		   /* store the nextruntime where server can find it */
           strncpy(datarec->next_scheduled_runtime,local_rec.next_scheduled_runtime, JOB_DATETIME_LEN);
           datarec->run_timestamp = time_number;
		   /* advise we want the job requeued */
           return_flag = 1;
      }
      else {
         if (pSCHEDULER_CONFIG_FLAGS.log_level > 0) {
            myprintf( "WARN: JW001-Repeating job next runtime after newday time, defered scheduling for %s\n",
                     datarec->job_header.jobname );
		 }
	  }
   } /* if a repeating job with no alerts at start of block */
   else if (local_rec.use_calendar == '3') {
      myprintf( "*ERR: JE018-Alerts raised for repeating job %s, not requeued on\n", local_rec.job_header.jobname );
   } /* else a repeating job with an alert */

   /* All done */
   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
      myprintf( "INFO: JI005-%s next runtime scheduled for %s\n",local_rec.job_header.jobname,
                local_rec.next_scheduled_runtime );
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG:Leaving JOBS_completed_job_time_adjustments\n" );
   }
   return( return_flag );
} /* JOBS_completed_job_time_adjustments */

/* ---------------------------------------------------
 * Called when a job is scheduled to run on specific
 * days identified in the job record. We increment
 * the time number until we find a matching day.
   --------------------------------------------------- */
int JOBS_find_next_run_day( jobsfile_def * job_rec, time_t time_number ) {
   int day_count, good_hour;
   struct tm time_var;
   struct tm *time_var_ptr;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG:Entered JOBS_find_next_run_day\n" );
   }

   time_var_ptr = (struct tm *)&time_var;
   time_var_ptr = localtime( &time_number );
   good_hour = time_var_ptr->tm_hour;   /* daylight savings offset checks */
   day_count = 0;
   while (job_rec->crontype_weekdays[time_var_ptr->tm_wday] != '1') {
		   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_VARDUMP) {
		      myprintf( "DEBUG: Checked day number %d, %s", time_var_ptr->tm_wday, ctime(&time_number) );
		   }
		   time_number = time_number + (60 * 60 * 24);
           time_var_ptr = localtime( &time_number );
		   /* see if our hour has been adjusted either way */
           if (good_hour != time_var_ptr->tm_hour) { /* adjust for daylight savings */
		      if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_VARDUMP) {
                 myprintf( "DEBUG: --Adjusting for daylight savings time change\n" );
			  }
              if (time_var_ptr->tm_hour > good_hour) { time_number = time_number - (60 * 60); }
              else {
					  if (time_var_ptr->tm_hour == 0) {
                         /* assume was 23:nn but became 00:nn so we need
                          * to go backward in this one instance */
                         time_number = time_number - (60 * 60);
					  }
					  else {
                         /* else we need to adjust forward in normal
                          * instances */
					     time_number = time_number + (60 * 60);
					  }
			  }
		      if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_VARDUMP) {
                 myprintf( "DEBUG: --Adjusted %d for %s", time_var_ptr->tm_wday, ctime(&time_number) );
			  }
              time_var_ptr = localtime( &time_number );
           }
		   day_count++;
		   if (day_count >= 7) {
              myprintf("*ERR: JE019-Unable to find a next day to run job on after checking 7 days (JOBS_find_next_run_day)\n" );
			  myprintf("*ERR: JE019-...Job name %s, Job day flags are %c %c %c %c %c %c %c\n",
							  job_rec->job_header.jobname, job_rec->crontype_weekdays[0],
							  job_rec->crontype_weekdays[1], job_rec->crontype_weekdays[2],
							  job_rec->crontype_weekdays[3], job_rec->crontype_weekdays[4],
							  job_rec->crontype_weekdays[5], job_rec->crontype_weekdays[6] );
			  return( 1 );
		   }
   }

   UTILS_datestamp( job_rec->next_scheduled_runtime, time_number );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG:Leaving JOBS_find_next_run_day\n" );
   }
   return( 0 );
} /* JOBS_find_next_run_day */

/* ---------------------------------------------------
 * Called from the CALENDAR_delete proc to ensure that
 * no jobs are using the calendar about to be deleted.
 * Return -1 if an error
 * Return the number of jobs refering to the calendar
 * if no error.
   --------------------------------------------------- */
long JOBS_count_entries_using_calendar( char * calname ) {
   jobsfile_def * workbuffer;
   int lasterror;
   long found_count;
		
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered JOBS_count_entries_using_calendar\n" );
   }

   if ( (workbuffer = (jobsfile_def *)MEMORY_malloc(sizeof(jobsfile_def),"JOBS_count_entries_using_calendar")) == NULL) { 
      myprintf( "*ERR: JE020-Insufficicent memory to allocate %d bytes (JOBS_count_entries_using_calendar)\n",
                sizeof(jobsfile_def) );
      myprintf( "*ERR: JE021-Insufficicent memory to determine if any jobs use the calender, DELETE REJECTED\n" );
	  return( -1 );
   }
   
   /*
    * Position to the begining of the file
    */
   lasterror = fseek( job_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      MEMORY_free(workbuffer);
      UTILS_set_message( "file seek error (job definition file)" );
      perror( "file seek error (job definition file)" );
      return( -1 );
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG: seek to start of file, jobs file (JOBS_count_entries_using_calendar)\n" );
   }
   
   /* loop until end of file */
   lasterror = 0; 
   found_count = 0;
   while ( (!(ferror(job_handle))) && (!(feof(job_handle))) ) {
      lasterror = fread( workbuffer, sizeof(jobsfile_def), 1, job_handle  );
      if (  (!(feof(job_handle))) && (workbuffer->job_header.info_flag != 'D')  ) {
         if ( (workbuffer->use_calendar == '1') &&
              (workbuffer->job_header.info_flag != 'D') &&
              (memcmp(workbuffer->calendar_name,calname,CALENDAR_NAME_LEN) == 0)
            ) {
            found_count++;
         }
	  } /* while not eof */
   } /* while not a file error or eof */

   MEMORY_free( workbuffer );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving JOBS_count_entries_using_calendar\n" );
   }
   return( found_count );
} /* JOBS_count_entries_using_calendar */

/* ---------------------------------------------------
   JOBS_recheck_job_calendars
   Read all job records, if they use the calendar name 
   passed they may have a new runtime now.        
   Called when a job calendar is added, updated or 
   deleted to ensure we keep our jobs in sync.
   --------------------------------------------------- */
void JOBS_recheck_job_calendars( calendar_def * calname ) {
   jobsfile_def * workbuffer;
   int lasterror;
   long recordnumber;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered JOBS_recheck_job_calendars, calendar %s\n", calname->calendar_name );
   }
   if (calname->calendar_type != '0') {   /* not a job calendar */
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
         myprintf( "DEBUG: Leaving JOBS_recheck_job_calendars, not job cal, no action needed\n" );
      }
      return; /* Nothing needed */
   }
   if ( (workbuffer = (jobsfile_def *)MEMORY_malloc(sizeof(jobsfile_def),"JOBS_recheck_job_calendars")) == NULL) { 
      myprintf( "*ERR: JE022-Insufficicent memory to recheck calendars, needed %d bytes  (JOBS_recheck_job_calendars)\n",
                 sizeof(jobsfile_def) );
	  return;
   }
   
   /*
    * Position to the begining of the file
    */
   lasterror = fseek( job_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      MEMORY_free(workbuffer);
      UTILS_set_message( "file seek error (job definition file)" );
      perror( "file seek error (job definition file)" );
      return;
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG: seek to start of file, jobs file (JOBS_recheck_job_calendars)\n" );
   }

   /* loop until end of file */
   lasterror = 0; 
   recordnumber = 0;
   while ( (!(ferror(job_handle))) && (!(feof(job_handle))) ) {
      lasterror = fseek( job_handle, (recordnumber * sizeof(jobsfile_def)), SEEK_SET );
      if (lasterror != 0) {
         UTILS_set_message( "file seek error (job definition file)" );
         perror( "file seek error (job definition file)" );
         MEMORY_free(workbuffer);
         if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
            myprintf( "DEBUG: Leaving JOBS_recheck_job_calendars due to file seek errors\n" );
         }
         return;
      }
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_IO) {
         myprintf( "DEBUG: seek to record number %d (JOBS_recheck_job_calendars)\n", recordnumber );
      }

      lasterror = fread( workbuffer, sizeof(jobsfile_def), 1, job_handle  );
      if (  (!(feof(job_handle))) &&
            (!(ferror(job_handle))) &&
            (workbuffer->job_header.info_flag != 'D') && /* not deleted */
            (workbuffer->use_calendar == '1') &&        /* uses a calendar */
            (memcmp(workbuffer->calendar_name, calname->calendar_name, CALENDAR_NAME_LEN) == 0) /* job uses this cal */
          ) {
             if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_VARS) {
                myprintf( "DEBUG: Job %s (rec %d) uses calendar %s, checking (JOBS_recheck_job_calendars)\n",
                           workbuffer->job_header.jobname, recordnumber, calname->calendar_name );
             }
             if (CALENDAR_next_runtime_timestamp_calchange( (char *)&workbuffer->calendar_name,
                                                 (char *)&workbuffer->next_scheduled_runtime, 0 ) != 0) {
               workbuffer->calendar_error = 'N';
               /* commit the change */
               if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_IO) {
                  myprintf( "DEBUG: writing to record %d (JOBS_recheck_job_calendars)\n", recordnumber );
               }
               if (JOBS_write_record( workbuffer, recordnumber ) != -1) {
	              if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
                     myprintf( "INFO: JI006-Job %s rescheduled due to calendar %s being updated\n",
                               workbuffer->job_header.jobname, workbuffer->calendar_name );
                  }
               }
               else {
                  myprintf( "*ERR: JE023-Unable to update job %s with new runtime (calendar change), error %d\n",
                            workbuffer->job_header.jobname, errno );
               } /* change commitment */
            } /* if calendar next runtime timestamp */
            else {    /* change must have removed legal rundays, adjust to calerr */
               workbuffer->calendar_error = 'Y';
               /* commit the change */
               if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_IO) {
                  myprintf( "DEBUG: writing to record %d (JOBS_recheck_job_calendars)\n", recordnumber );
               }
               if (JOBS_write_record( workbuffer, recordnumber ) != -1) {
	              if (pSCHEDULER_CONFIG_FLAGS.log_level > 0) {
                     myprintf( "WARN: JW002-Job %s placed into calendar error state due to calendar %s update (no days left to run)\n",
                               workbuffer->job_header.jobname, workbuffer->calendar_name );
                  }
               }
            }
	  } /* if not eof and checks pass */

      recordnumber++; /* move on to the next record */
   } /* while not a file error or eof */

   MEMORY_free( workbuffer );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving JOBS_recheck_job_calendars, calendar %s\n", calname->calendar_name );
   }
}	/* JOBS_recheck_job_calendars */

/* ---------------------------------------------------
   JOBS_recheck_every_job_calendar
   Read all job records and calculate a new runtime if 
   the job uses any calendar. Called when a holiday
   calendar has been updated, as we need to check runtimes
   of all calendar jobs then in case their calendar uses
   the holiday calendar.
   --------------------------------------------------- */
void JOBS_recheck_every_job_calendar( void ) {
   jobsfile_def * workbuffer;
   int lasterror;
   long recordnumber;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered JOBS_recheck_every_job_calendar\n" );
   }
   if ( (workbuffer = (jobsfile_def *)MEMORY_malloc(sizeof(jobsfile_def),"JOBS_recheck_every_job_calendar")) == NULL) { 
      myprintf( "*ERR: JE024-Insufficicent memory to recheck calendars, needed %d bytes  (JOBS_recheck_every_job_calendar)\n",
                 sizeof(jobsfile_def) );
	  return;
   }
   
   /*
    * Position to the begining of the file
    */
   lasterror = fseek( job_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      MEMORY_free(workbuffer);
      UTILS_set_message( "file seek error (job definition file)" );
      perror( "file seek error (job definition file)" );
      return;
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG: seek to start of file, jobs file (JOBS_recheck_every_job_calendar)\n" );
   }

   /* loop until end of file */
   lasterror = 0; 
   recordnumber = 0;
   while ( (!(ferror(job_handle))) && (!(feof(job_handle))) ) {
      lasterror = fseek( job_handle, (recordnumber * sizeof(jobsfile_def)), SEEK_SET );
      if (lasterror != 0) {
         UTILS_set_message( "file seek error (job definition file)" );
         perror( "file seek error (job definition file)" );
         MEMORY_free(workbuffer);
         if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
            myprintf( "DEBUG: Leaving JOBS_recheck_every_job_calendar due to file seek errors\n" );
         }
         return;
      }
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_IO) {
         myprintf( "DEBUG: seek to record number %d (JOBS_recheck_every_job_calendar)\n", recordnumber );
      }

      lasterror = fread( workbuffer, sizeof(jobsfile_def), 1, job_handle  );
      if (  (!(feof(job_handle))) &&
            (!(ferror(job_handle))) &&
            (workbuffer->job_header.info_flag != 'D') && /* not deleted */
            (workbuffer->use_calendar == '1')         /* uses a calendar */
          ) {
             if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_VARS) {
                myprintf( "DEBUG: Job %s (rec %d) uses  a calendar, checking (JOBS_recheck_every_job_calendar)\n",
                           workbuffer->job_header.jobname, recordnumber );
             }
             if (CALENDAR_next_runtime_timestamp_calchange( (char *)&workbuffer->calendar_name,
                                                 (char *)&workbuffer->next_scheduled_runtime, 0 ) != 0) {
               workbuffer->calendar_error = 'N';
               /* commit the change */
               if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_IO) {
                  myprintf( "DEBUG: writing to record %d (JOBS_recheck_every_job_calendar)\n", recordnumber );
               }
               if (JOBS_write_record( workbuffer, recordnumber ) != -1) {
	              if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
                     myprintf( "INFO: JI007-Job %s rescheduled due to holiday calendar being updated\n",
                               workbuffer->job_header.jobname );
                  }
               }
               else {
                  myprintf( "*ERR: JE025-Unable to update job %s with new runtime (calendar change), error %d\n",
                            workbuffer->job_header.jobname, errno );
               } /* change commitment */
            } /* if calendar next runtime timestamp */
            else {    /* change must have removed legal rundays, adjust to calerr */
               workbuffer->calendar_error = 'Y';
               /* commit the change */
               if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_IO) {
                  myprintf( "DEBUG: writing to record %d (JOBS_recheck_every_job_calendar)\n", recordnumber );
               }
               if (JOBS_write_record( workbuffer, recordnumber ) != -1) {
	              if (pSCHEDULER_CONFIG_FLAGS.log_level > 0) {
                     myprintf( "WARN: JW003-Job %s placed into calendar error state due to holiday calendar update (no days left to run)\n",
                               workbuffer->job_header.jobname );
                  }
               }
            }
	  } /* if not eof and checks pass */

      recordnumber++; /* move on to the next record */
   } /* while not a file error or eof */

   MEMORY_free( workbuffer );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.jobslib >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving JOBS_recheck_every_job_calendar\n" );
   }
   return; /* not yet implemented */
}   /* JOBS_recheck_every_job_calendar */
