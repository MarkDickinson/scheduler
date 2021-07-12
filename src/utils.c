#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "debug_levels.h"
#include "utils.h"
#include "schedlib.h"
#include "system_type.h"
#include "memorylib.h"

#ifdef SOLARIS
#define __STDC__
/* #include <sys/varargs.h> */
#include <stdarg.h>
/* #undef __STDC__ */
#else
#include <stdarg.h>
#endif

/* made extern in utils.h for gcc10 */
char UTILS_errortext[UTILS_MAX_ERRORTEXT];

/* --------------------------------------------------
   -------------------------------------------------- */
void UTILS_set_message( const char *msgstring ) {
  /* 
   *  note: we only set it if there is not already a message
   *        in the buffer. This way we ensure only the first
   *        'real' error message is propogated back up the
   *        chain to be displayed to the caller.
   */
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: In UTILS_set_message\n" );
   }
   if (UTILS_errortext[0] == '\0')
      strncpy( UTILS_errortext, msgstring, (UTILS_MAX_ERRORTEXT - 1) );
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving UTILS_set_message\n" );
   }
} /* UTILS_set_message */


/* --------------------------------------------------
   -------------------------------------------------- */
void UTILS_format_message( const char *format, int number ) {
  char buffer[101];

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: in utils.c, UTILS_format_message\n" );
  }

  snprintf( buffer, 100, format, number );
  UTILS_set_message( buffer );

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: Leaving UTILS_format_message\n" );
  }
} /* UTILS_set_message */


/* --------------------------------------------------
   -------------------------------------------------- */
void UTILS_clear_message( void ) {
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: in utils.c, UTILS_clear_message\n" );
   }
   UTILS_errortext[0] = '\0';
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving UTILS_clear_message\n" );
   }
} /* UTILS_clear_message */


/* --------------------------------------------------
   -------------------------------------------------- */
void UTILS_print_last_message( void ) {
	/* Use 8 here, as its called every second normally */
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_TIMERS) {
      myprintf( "DEBUG: in utils.c, UTILS_print_last_message\n" );
   }
   if (UTILS_errortext[0] != '\0') {
      myprintf("*ERR: ZE001-Original error was '%s'\n", UTILS_errortext );  /* its always an error */
      UTILS_clear_message();
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_TIMERS) {
      myprintf( "DEBUG: Leaving UTILS_print_last_message\n" );
   }
} /* UTILS_print_last_message */


/* --------------------------------------------------
   -------------------------------------------------- */
void UTILS_get_message( char *msgstring, int buflen, int clearflag ) {
   int maxlen;
   char *ptr;

   ptr = msgstring;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: in utils.c, UTILS_get_message\n" );
   }

   if (strlen(UTILS_errortext) > 0) {
      maxlen = strlen(UTILS_errortext);
      if (maxlen >= buflen) { maxlen = buflen; }
      strncpy( ptr, UTILS_errortext, maxlen );
   }
   else { strcpy(ptr,"" ); }
   if (clearflag != 0) {
      UTILS_clear_message();
   }
   else {
      UTILS_print_last_message();
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving UTILS_get_message\n" );
   }
} /* UTILS_get_message */


/* --------------------------------------------------
   Note... data buffer must be fieldlen+1 so we have
   room for the null terminator, the fieldlen is
   expected to be the buffer area less that 1 byte.
   -------------------------------------------------- */
void UTILS_space_fill_string( char * strptr, int fieldlen ) {
  char *ptr1;
  int  i, j;

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: in utils.c, UTILS_space_fill_string\n" );
  }

  /* find the string terminator */
  i = 0;
  ptr1 = strptr;
  while ((*ptr1 != '\0') && (*ptr1 != '\n') && (i < fieldlen)) {
     i++;
     ptr1++;
  }

  /* found a null ? */
  if ((*ptr1 != '\0') && (*ptr1 != '\n')) {
    /* no, just terminate at length given */
    *ptr1 = '\0';  /* we are where the null should be */
  }
  else {
    /* yes, pad out with spaces */
    for (j = i; j < fieldlen; j++) {
       *ptr1 = ' ';
       ptr1++;
    }
    *ptr1 = '\0';
  }

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: Leaving UTILS_space_fill_string : string='%s'\n", strptr );
  }
} /* UTILS_Space_Fill_String */


/* --------------------------------------------------
   -------------------------------------------------- */
void UTILS_datestamp( char * strptr, time_t datetouse ) {
   struct tm time_var;
   struct tm *time_var_ptr;
   time_t time_number;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: in utils.c, UTILS_datestamp\n" );
   }

   if (datetouse == 0) {
      time_number = time(0);
   }
   else {
      time_number = datetouse;
   }

   time_var_ptr = (struct tm *)&time_var;
   time_var_ptr = localtime( &time_number );    /* time now as tm */
 
   /* adjust for the quirks in the returned values */ 
   time_var_ptr->tm_year = time_var_ptr->tm_year + 1900;  /* years returned as count from 1900 */
   time_var_ptr->tm_mon = time_var_ptr->tm_mon + 1;       /* months returned as 0-11 */

   /* Use job_datetime_len as the snprintf wants to put a null at the end of
	* the string in this release of gcc. */
   snprintf( strptr, JOB_DATETIME_LEN+1, "%.4d%.2d%.2d %.2d:%.2d:00",
           time_var_ptr->tm_year, time_var_ptr->tm_mon, time_var_ptr->tm_mday,
           time_var_ptr->tm_hour, time_var_ptr->tm_min );
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_VARS) { 
      myprintf( "DEBUG: UTILS_datestamp, formatted Y=%d, M=%d, D=%d, h=%d, m=%d, s=%d\n" ,
           time_var_ptr->tm_year, time_var_ptr->tm_mon, time_var_ptr->tm_mday,
           time_var_ptr->tm_hour, time_var_ptr->tm_min, time_var_ptr->tm_sec );
      myprintf( "DEBUG: UTILS_datestamp, return string is %s\n", strptr );
   }

   /* check and adjust the daylight savings time in case it's changed */
   pSCHEDULER_CONFIG_FLAGS.isdst_flag = time_var_ptr->tm_isdst;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving UTILS_datestamp\n" );
   }
   return;
} /* UTILS_datestamp */

/* --------------------------------------------------
 * Checks to ensure we are not writing a duplicate record during a write
 * request. Fails if we are.
   -------------------------------------------------- */
long UTILS_write_record_safe( FILE *filehandle, char * datarec,
                              long sizeofdatarec, long recordnum, char *filetype, char * caller ) {
   job_header_def *local_rec;   /* always at start of record */
   char callernamebuf[128];
   
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: in utils.c, UTILS_write_record_safe for %s, caller=%s\n", filetype, caller );
   }

   if (recordnum == -1) {
       /* Allocate memory for a local copy of the record */
      if ((local_rec = (job_header_def *)MEMORY_malloc( sizeofdatarec,"UTILS_write_record_safe" )) == NULL) {
         UTILS_set_message( "Unable to allocate memory required for sanity read operation" );
         return( -1 );
      }
	  memcpy( local_rec, datarec, sizeofdatarec );
	  snprintf( callernamebuf, 127, "UTILS_write_record_safe:%s", caller );
	  if (UTILS_read_record( filehandle, (char *)local_rec, sizeofdatarec, filetype, callernamebuf, 0 ) != -1) {
	     MEMORY_free(local_rec);
		 if (pSCHEDULER_CONFIG_FLAGS.log_level > 0) { /* > error only */
		    myprintf( "WARN: ZW001-Attempt to add duplicate record rejected !\n" );
		 }
		 return( -1 );
	  }
	  MEMORY_free( local_rec );
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: About to leave UTILS_write_record_safe (after UTILS_write_record)\n" );
   }
   return( UTILS_write_record( filehandle, datarec, sizeofdatarec, recordnum, filetype ) );
} /* UTILS_write_record_safe */

/* --------------------------------------------------
 * WARN: ONLY EVER CALL FOR RECORDS THAT USE THE
 *       STANDARD HEADER_DEF AS WE WANT TO PAD IT.
 * Return -1 on error, else return record number written.
   -------------------------------------------------- */
long UTILS_write_record( FILE *filehandle, char * datarec,
                         long sizeofdatarec, long recordnum, char *filetype ) {
   int lasterror;
   long currentpos, actualpos;
   char errormsg[80];
   job_header_def * datarec_ptr;

   datarec_ptr = (job_header_def *)datarec;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: in utils.c, UTILS_write_record, reclen %d, file %s\n", (int)sizeofdatarec, filetype );
   }

   /* Some calls have short jobname records, sanitise here */
   UTILS_space_fill_string( datarec_ptr->jobname, JOB_NAME_LEN );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG: in utils.c, UTILS_write_record, recordnum=%d in filetype %s\n", (int)recordnum, filetype );
   }
/*
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils > DEBUG_LEVEL_VARDUMP) {
      API_DEBUG_dump_job_header( datarec );
   }
   */

   lasterror = fflush( filehandle );
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_IO) {
      if (lasterror != 0) {
         myprintf( "DEBUG: in utils.c, errno after initial fflush is %d\n", errno );
	  }
   }

   /*
    * If the record number is -1 the caller wants us to
    * write to eof. Otherwise seek to the record number
    * specified and update it.
    */
   if (recordnum == -1) {
      lasterror = fseek( filehandle, 0L, SEEK_END );
	  if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_IO) {
	     myprintf( "DEBUG: Positioned to end of file %s\n", filetype );
	  }
   }
   else {
      lasterror = fseek( filehandle, (long)(recordnum * sizeofdatarec), SEEK_SET );
	  if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_IO) {
	     myprintf( "DEBUG: Absolute seek on record %d, byte %d in %s\n",
                 (int)recordnum, (int)(recordnum * sizeofdatarec), filetype );
	  }
   }
   if ( (lasterror != 0)  || (ferror(filehandle)) ) {
	  if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_IO) {
			  myprintf( "DEBUG: File seek error %d on %s\n", errno, filetype );
	  }
      snprintf( errormsg, 79, "file seek error (%s):", filetype );
      UTILS_set_message( errormsg );
      perror( errormsg );
      return( -1 );
   }

   /*
    * get the current record position from the file system
    * we trust this rather than assuming the seek worked.
    * Also we need to know the eof record number to return
    * to the caller if we are writing to eof.
    */
   currentpos = ( ftell(filehandle) / sizeofdatarec ); 
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG: Current file pointer is now %d\n", (int)currentpos );
   }
   if ((recordnum != -1) && (recordnum != currentpos)) {
		   myprintf( "*ERR: ZE002-FATAL ERROR: FSEEK FAILED TO POSITION CORRECTLY\n" );
		   myprintf( "*ERR: ZE002-FATAL ERROR: The record has not been updated.\n" );
		   return( -1 );
   }

   /*
    * note: cannot rely on return code from a buffered write, use
    *       ferror to see if it worked.
    */
   lasterror = fwrite( datarec, sizeofdatarec, 1, filehandle );
   if ( (ferror(filehandle)) || (lasterror != 1) ) {
	  if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_IO) {
			  myprintf( "DEBUG: File write error %d on %s\n", errno, filetype );
	  }
      snprintf( errormsg, 79, "file write error (%s):", filetype );
      UTILS_set_message( errormsg );
      perror( errormsg );
      return( -1 );
   }
   lasterror = fflush( filehandle );
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_IO) {
      if (lasterror != 0) {
         myprintf( "DEBUG: in utils.c, errno after second fflush is %d\n", errno );
	  }
   }

   /* HAD A LOT OF PROBLEMS where even with the fseek to 0 and ftell saying
    * it was at 0 the fwrite still writes to EOF !?!?.
    * Thats why there are so many debug statements above.
    * I've added this
    * check to determine where exactly the record was written.
    */
    actualpos = ( ( ftell(filehandle) / sizeofdatarec ) - 1 );
    if ((recordnum != -1) && (recordnum != actualpos)) {
			myprintf( "*ERR: ZE003-*ALERT** Wanted to write to record %d, actually wrote at record %d\n",
					(int)recordnum, (int)actualpos );
	}
	else {
       if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_IO) {
          if (lasterror != 0) {
             myprintf( "DEBUG: in utils.c, errno after second fflush is %d\n", errno );
	      }
       }
	}

   /*
    * return the recordnumber position we wrote the record into.
    */

   /* return ( currentpos ); */

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving UTILS_write_record, reclen\n" );
   }
	return( actualpos );
} /* UTILS_write_record */

/* --------------------------------------------------
   -------------------------------------------------- */
long UTILS_read_record( FILE *filehandle, char * datarec,
                        long sizeofdatarec, char *filetype, char * caller, int err_if_fail ) {
   int  lasterror;
   long recordfound;
   job_header_def *local_rec;   /* always at start of record */
   job_header_def *datarec_ptr;
   char errormsg[80];

   datarec_ptr = (job_header_def *)datarec;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: in utils.c, UTILS_read_record, reclen %d, file %s\n", (int)sizeofdatarec, filetype );
   }

   /* Some calls have short jobname records, sanitise here */
   UTILS_space_fill_string( datarec_ptr->jobname, JOB_NAME_LEN );

   /*
    * Position to the begining of the file
    */
   lasterror = fseek( filehandle, 0, SEEK_SET );
   if (lasterror != 0) {
      snprintf( errormsg, 79, "file seek error (%s):", filetype );
      UTILS_set_message( errormsg );
	  myprintf( "*ERR: ZE004-%s (UTILS_read_record)\n", errormsg );
      perror( errormsg );
      return( -1 );
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG: Positioned to start of file (UTILS_read_record)\n" );
   }

   /*
    * Allocate memory for a local copy of the record
    */
   if ((local_rec = (job_header_def *)MEMORY_malloc( sizeofdatarec,"UTILS_read_record" )) == NULL) {
      UTILS_set_message( "Unable to allocate memory required for read operation" );
	  perror( "Insufficent memory for read operation" );
	  myprintf( "*ERR: ZE005-Unable to allocate memory for read operation (UTILS_read_record)\n" );
      return( -1 );
   }
   UTILS_zero_fill( (char *)local_rec, sizeofdatarec ); /* seems to overlay a previous record so wipe data */

   /*
    * Loop reading through the file until we find the
    * record we are looking for.
    */
   datarec_ptr = (job_header_def *) datarec;
   recordfound = -1;
   while ((recordfound == -1) && !(feof(filehandle)) ) {
      lasterror = fread( local_rec, sizeofdatarec, 1, filehandle  );
      if (ferror(filehandle)) {
         myprintf( "*ERR: ZE006-Read error occurred (UTILS_read_record)\n" );
         snprintf( errormsg, 79, "file read error (%s):", filetype );
         UTILS_set_message( errormsg );
         perror( errormsg );
         MEMORY_free( local_rec );
         return( -1 );
      }
      if (!feof(filehandle)) {
         /*
          * Check to see if this is the record we want.
          * check for 41 bytes, the name and the type flag
          */
	     if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_RECIOCMP) {
	       myprintf( "DEBUG: UTILS_read_record, checking against %s (read) for %s (provided)\n", local_rec->jobname, datarec_ptr->jobname );
	     }
         if (
				 (!(memcmp(local_rec->jobname,datarec_ptr->jobname, JOB_NAME_LEN ))) &&
				 (local_rec->info_flag != 'D')
			) {
             recordfound = ( ftell(filehandle) / sizeofdatarec ) - 1;
		    if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_RECIOCMP) {
               myprintf( "DEBUG: UTILS_read_record, matched, recordpos set to %d\n", (int)recordfound );
		    }
         } /* if record found */
	  }  /* if not feof */
   } /* while not eof and not found */

   /*
    * If we found a record copy it to the callers buffer.
    */
   if (recordfound != -1) {
      memcpy( datarec, local_rec, sizeofdatarec );
   }
   else {
      if (err_if_fail != 0) {  /* This may not be an error if the called was expecting a fail possibility */
         if (pSCHEDULER_CONFIG_FLAGS.log_level >= 1) {
            myprintf( "WARN: ZW002-Unable to find data record for '%s' (UTILS_read_record, called by %s)\n", datarec_ptr->jobname, caller );
		 }
	  }
   }

   /*
    * Free our temporary buffer
    */
   MEMORY_free( local_rec );

   /*
    * Return the record number we found.
    */
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_IO) {
      if (recordfound >= 0) {
         myprintf( "DEBUG: UTILS_read_record: Record was read successfully\n" );
      }
      else {
	     myprintf("DEBUG: UTILS_read_record: Record was not found\n" );
      }
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving UTILS_read_record\n" );
   }
   return ( recordfound );
} /* UTILS_read_record */

/* --------------------------------------------------
 * The file to be compressed MUST BE CLOSED.
 * This procedure is only ever called from the
 * SERVER_newday_cleanup as part of the SCHEDULER-NEWDAY
 * job. Don't change this.
 *
 * input: diskfilename - the physical file name
 *        recsize  - the size of a physical record
 *        delflagoffset - where in the record the delete flag indicator is
 *        found
 *        delflagind - the value that will be set in the del flag indicator if
 *        its a deleted record
 *        filecomment - a descriptive comment on file type for any error
 *        messages.
 *  Return true if OK, false if failed.
 * -------------------------------------------------- */
short int  UTILS_compress_file( char *diskfilename, int recsize, int delflagoffset, char delflagind, char *filecomment ) {

   FILE * original_file_handle;
   FILE * work_file_handle;
   char workfilename[] = "scheduler_workfile.tmp";
   char bkpfilename[] = "backup.tmp";
   long omitted_count, kept_count, records_in_file;
   int datacount, lasterr, errfatal;
   char * databuffer;
   char *ptr;
   struct stat * stat_buf;

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
     myprintf( "DEBUG: in utils.c, UTILS_compress_file\n" );
  }

   if ( (stat_buf = (struct stat *)MEMORY_malloc(sizeof(struct stat),"UTILS_compress_file")) == NULL ) {
      return( 1 );
   }
   lasterr = stat( diskfilename, stat_buf );
   if (lasterr == ENOENT) { /* file does not exist */
		   myprintf( "*ERR: ZE007-file %s to be compressed DOES NOT EXIST !, no compress done\n", diskfilename );
           MEMORY_free( stat_buf );
		   return( 1 );
   }
   else if (lasterr != 0) { /* another error */
		   myprintf( "*ERR: ZE008-Unable to stat file %s (stat err=%d, errno=%d), no compress done\n", diskfilename,
                     lasterr, errno);
           MEMORY_free( stat_buf );
		   return( 1 );
   }
   records_in_file = (long)(stat_buf->st_size / recsize);
   MEMORY_free( stat_buf );

   /* Allocate memory for the record buffer */
   if ( (databuffer = (char *)MEMORY_malloc(recsize,"UTILS_compress_file")) == NULL ) {  /* unable to allocate memory */
      myprintf( "*ERR: ZE009-Unable to allocate %d bytes of memory (UTILS_compress_file)\n", recsize );
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
         myprintf( "DEBUG: Leaving UTILS_compress_file\n" );
      }
      return( 1 );  /* but we havn't changed any files so scheduler can continue */
   }

   /* Address where the delflag is expected to be */
   ptr = databuffer;
   ptr = ptr + delflagoffset;

   /* Ensure we can open the input file, read only */
   if ( (original_file_handle = fopen( diskfilename, "rb" )) == NULL ) {
		   perror( diskfilename );
		   myprintf( "*ERR: ZE010-Unable to open %s (%s) for compression (err=%d), no compression done\n", diskfilename, filecomment, errno );
		   MEMORY_free( databuffer );
           if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
              myprintf( "DEBUG: Leaving UTILS_compress_file with errors\n" );
           }
		   return( 1 ); /* Failed, but continue processing at this point */
   }

   if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
      myprintf( "INFO: ZI001-Compressing %s, %d records in original file\n", diskfilename, (int)records_in_file );
   }

   /* Create the output file */
   if ( (work_file_handle = fopen( workfilename, "wb+" )) == NULL) {
		   perror( workfilename );
		   myprintf( "*ERR: ZE011-Unable to open %s for compression work (err=%d), no compress done for %s\n", workfilename, errno, filecomment );
		   MEMORY_free( databuffer );
		   fclose( original_file_handle );
           if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
              myprintf( "DEBUG: Leaving UTILS_compress_file with errors\n" );
           }
		   return( 1 ); /* failed but continue processing at this point */
   }

   /* Loop, copying all records without the delete indicator set
	* from the original file to the work file */
   clearerr( original_file_handle );
   clearerr( work_file_handle );
   errfatal = 0;
   omitted_count = 0L;  /* none omitted yet */
   kept_count = 0L;
   records_in_file = 0L;
   while (
           (!(feof(original_file_handle))) &&
		   (!(ferror(original_file_handle))) &&
           (!(ferror(work_file_handle))) &&
		   (datacount > 0) &&
		   (errfatal == 0)
         )
   {
      datacount = fread( databuffer, recsize, 1, original_file_handle );
	  if ( (datacount == 1) && (!(ferror(original_file_handle))) ) {
	     /* check to see if its a deleted record */
	     if (*ptr != delflagind) {
            datacount = fwrite( databuffer, recsize, 1, work_file_handle );
			if ( (datacount != 1) || (ferror(work_file_handle)) ) {     /* --- write failed --- */
		       myprintf( "*ERR: ZE012-Write error on %s (err=%d) for compression work, no compress done for %s\n", workfilename, errno, filecomment );
			   errfatal = 1;   /* Stop Loop now */
			} /* if datacount != 1 */
			else {
					kept_count++;
			}
		 }   /* if ptr != delflagind */
		 else {
		    omitted_count++;
		 }   /* if ptr != delflagind else */
	  }      /* if datacount == 1 */
	  else if (ferror(original_file_handle)) {
          myprintf( "*ERR: ZE013-Read error on %s (err=%d) for compression work, no compress done for %s\n", workfilename, errno, filecomment );
          errfatal = 1;   /* Stop Loop now */
	  }
   }         /* while */

   /* We are done with the file handles and memory buffer now */
   fclose( original_file_handle );
   fflush( work_file_handle );
   fclose( work_file_handle );
   MEMORY_free( databuffer );

   /* Check if we had a fatal condition */
   if (errfatal != 0) {                /* A fatal error occurred, bit still have original file */
		   unlink( workfilename );   /* so still safe to continue at this point */
           if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
              myprintf( "DEBUG: Leaving UTILS_compress_file with errors\n" );
           }
		   return( 1 );
   }

   /* If there were no omitted records there is nothing to do */
   if (omitted_count == 0L) {
      unlink( workfilename ); /* doesnt matter if it fails, we overwrite it next time */
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
         myprintf( "INFO: ZI002-No records need deleting, no compress required\n" );
	  }
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
         myprintf( "DEBUG: Leaving UTILS_compress_file, no work to be done\n" );
      }
	  return( 1 ); /* Ok, no action needed */
   }

   /* Changed file, rename out the original file */
   lasterr = rename( diskfilename, bkpfilename );
   if (lasterr != 0) {  /* rename failed */
      myprintf( "*ERR: ZE014-Unable to rename %s to %s for compression work, no compress done for %s\n",
                 diskfilename, bkpfilename, filecomment );
      myprintf( "*ERR: ZE014-...errno=%d, lasterror value was %d\n", errno, lasterr );
      unlink( workfilename ); /* doesnt matter if it fails, we overwrite it next time */
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
         myprintf( "DEBUG: Leaving UTILS_compress_file\n" );
      }
	  return( 1 ); /*  no action needed to recover as original file not renamed */
   }

   /* we must rename in the new file now */
   lasterr = rename( workfilename, diskfilename );
   if (lasterr == 0) { /* sucessfull, we are done */
      unlink( bkpfilename ); /* doesnt matter if it fails */
	  if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
         myprintf( "INFO: ZI003-Completed compression of file %s (%s)\n", diskfilename, filecomment );
         myprintf( "INFO: ZI004-Compressed %s, %d records dropped, %d records kept\n", diskfilename, (int)omitted_count, (int)kept_count );
      }
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
         myprintf( "DEBUG: Leaving UTILS_compress_file, all OK\n" );
      }
      return( 1 ); /* all is truely ok */
   }

   myprintf( "*ERR: ZE015-Unable to rename %s to %s for compression work, backing out changes\n",
             workfilename, diskfilename );
   myprintf( "*ERR: ZE015-...errno=%d, lasterror value was %d\n", errno, lasterr );

   /* If we failed to rename in the new file we must put the original
	* file back */
   lasterr = rename( bkpfilename, diskfilename );
   if (lasterr != 0) {  /* rename failed */
       myprintf( "*ERR: ZE016----FATAL--- unable to recover from compression error !\n" );
       myprintf( "*ERR: ZE016-...errno=%d, lasterror value was %d\n", errno, lasterr );
       myprintf( "*ERR: ZE016-...Unable to rename orginal %s to %s, manual recover needed !\n", bkpfilename, diskfilename );
       myprintf( "*ERR: ZE016-...Shutdown server and MANUALLY rename %s to %s !\n", bkpfilename, diskfilename );
       if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
          myprintf( "DEBUG: Leaving UTILS_compress_file\n" );
       }
       return( 0 ); /* something seriously wrong */
   }

   /* ELSE, the compress failed, but we recovered from it,
	* so return 1 so processing can continue. */
   unlink(workfilename);   /* don't need this anymore */
   myprintf( "*ERR: ZE017-Errors prevented compression of %s (%s), no compress done\n", diskfilename, filecomment );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving UTILS_compress_file\n" );
   }

   return( 1 );
} /* UTILS_compress_file */

/* --------------------------------------------------
 * BUG: Won't handle small fields (ie: 2 bytes)
   -------------------------------------------------- */
void UTILS_number_to_string( int numval, char *dest, int fieldlen ) {
   char *cPtr;
   char localstr[50];
   int i, localnum, localoffset;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Enetered UTILS_number_to_string, num=%d, maxlen=%d\n", numval, fieldlen );
   }

   if (fieldlen > 49) {
      /* our buffer is to small */
      myprintf( "*ERR: ZE018-UTILS_number_to_string, fieldlen %d to large for buffer\n", fieldlen );
      return;
   }


   /* put zeros in the target buffer */
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_VARS) {
      myprintf( "DEBUG: UTILS_number_to_string, zeroing target field\n" );
   }
   localnum = fieldlen;
   if (localnum > 49) { localnum = 49; }
   cPtr = dest;
   for (i = 0; i < localnum; i++) {
      *cPtr = '0';
      cPtr++;
   }


   /* now put the bloody number into it */

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_VARS) {
      myprintf( "DEBUG: UTILS_number_to_string, working out number\n" );
   }
   snprintf( localstr, localnum, "%d", numval );
   localoffset = fieldlen - strlen(localstr);
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_VARS) {
      myprintf( "DEBUG: UTILS_number_to_string, ofset in dest str worked out as %d to copy %s into %d byte field\n", localoffset, localstr, fieldlen );
   }
   cPtr = dest + localoffset;
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_VARS) {
      myprintf( "DEBUG: UTILS_number_to_string, memcpy to target[%d] string of '%s'\n", localoffset, localstr );
   }
   memcpy( cPtr, localstr, localoffset );
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving UTILS_number_to_string, target value set to %s\n", dest );
   }
} /* UTILS_number_to_string */

/* ************************************************
   UTILS_parse_string

   Input:
      inputstr     - pointer to the string to be parsed
      delim        - the field delimiter character
      outstr       - pointer to a buffer to hold the
                     found string
      maxoutstrlen - the maximum size of the output buffer

   Output:
      outstr       - will have the field found stored in it
                 
   Returns:
      the length of the string found, or 0 if nothing was found

   Notes:
      All strings are assumed to be NULL (\0) terminated.

   Example :
     #include <stdlib.h>
     #include "utils.h"
     int main( int argc, char *argv ) {
        #define MAXOUTLEN 40
        char test_string[] = "This is a verylarge test=string to parse\0";
        char test_parm[MAXOUTLEN+1];
        char delim;
        int parmlen, padlen;
        int overrunflag;
        char *ptr;

        myprintf( "Parsing : %s\n", test_string );
        ptr = (char *) &test_string;
        while ( (parmlen = UTILS_parse_string( ptr, ' ', test_parm, MAXOUTLEN, &padlen, &overrunflag )) != 0 ) {
           ptr = ptr + parmlen + padlen;
           myprintf( "  got length %3.d, pads %d, %s\n", parmlen, padlen, test_parm );
        }
        exit( 0 );
     }
 Example Output: (from above code snippet)
      Parsing : This is a verylarge test=string to parse
        got length   4, pads 0, This
        got length   2, pads 1, is
        got length   1, pads 1, a
        got length   9, pads 1, verylarge
        got length  11, pads 1, test=string
        got length   2, pads 1, to
        got length   5, pads 1, parse
   ************************************************ */
int UTILS_parse_string( char *inputstr, char delim,
                        char * outstr, int maxoutstrlen,
                        int * leadingpadsfound, int * overrun ) {
   char *p1, *p2;
   int  foundlen;
   int leadpadlen;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: in utils.c, UTILS_parse_string\n" );
   }

   p1 = inputstr;
   *overrun = 0;

   /* skip over any leading delimeters (ie:leading spaces) */
   leadpadlen = 0;
   while ((*p1 == delim) && (*p1 != '\0') && (*p1 != '\n')) {
       leadpadlen++;
       p1++;
   } 
    
   p2 = p1;

   /* find the next delimietr to define our string */
   while ((*p2 != delim) && (*p2 != '\0') && (*p2 != '\n')) {
      p2++;
   }

   /* see if we found anything, if so bounds check it
      and copy as much as we can to the callers outstr  */
   foundlen = p2 - p1;
   if (foundlen > (maxoutstrlen-1)) {
       *overrun = foundlen;
       foundlen = maxoutstrlen - 1;
   }
   if (foundlen > 0) {
      memcpy( outstr, p1, foundlen );
      /* MUST null terminate the string we return */
      p1 = outstr + foundlen;
      *p1 = '\0';
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving UTILS_parse_string\n" );
   }

   /* return the length of the string we selected */
   *leadingpadsfound = leadpadlen;
   return( foundlen );
} /* UTILS_parse_string */

/* ***********************************************
   UTILS_count_delims

   *********************************************** */
int UTILS_count_delims( char *sptr, char delimchar ) {
   int delimcount;
   char *p1;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: in utils.c, UTILS_count_delims\n" );
   }

   delimcount = 0;
   p1 = sptr;

   while ((*p1 == delimchar) && (*p1 != '\0')) {
       delimcount++;
       p1++;
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving UTILS_count_delims\n" );
   }

   return( delimcount );
} /* UTILS_count_delims */

/* ***********************************************
   UTILS_find_newline

   *********************************************** */
unsigned int UTILS_find_newline( char *sptr ) {
   char *sLptr;
   unsigned int iCounter;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: in utils.c, UTILS_find_newline\n" );
   }

   sLptr = sptr;
   iCounter = 0;
   while (*sLptr != '\n') {
		   sLptr++;
		   iCounter++;
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving UTILS_find_newline\n" );
   }

   return( iCounter );
} /* UTILS_find_newline */

/* ***********************************************
   UTILS_zero_fill

   *********************************************** */
void UTILS_zero_fill( char *databuf, int datalen ) {
   int i;
   char *sptr;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: in utils.c, UTILS_zero_fill\n" );
   }

   sptr = databuf;

   for (i = 0; i < datalen; i++ ) {
      sptr = '\0';
      sptr++;
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving UTILS_zero_fill\n" );
   }
} /* UTILS_zero_fill */

/* ***********************************************
 * Convert the time passed as a string formatted
 * yyyymmdd hh:mm:ss as the number of seconds
 * since Jan 1 1970, 00:00.
 * We return 0 if we fail, which all our processing
 * uses as a 'disabled' timestamp value.
   *********************************************** */
time_t UTILS_make_timestamp( char * timestr /* yyyymmdd hh:mm:ss */ ) {
   char * ptr1;
   char smallbuf[20];
   time_t time_number;
   struct tm *time_var;
   
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
		   myprintf( "DEBUG: entered UTILS_make_timestamp\n" );
   }

   time_var = (struct tm *)MEMORY_malloc(sizeof *time_var,"UTILS_make_timestamp");
   if (time_var == NULL) {
      myprintf( "*ERR: ZE019-Unable to allocate memory for time structure (UTILS_make_timestamp)\n" );
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

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_VARS) { 
      myprintf( "DEBUG: UTILS_make_timestamp Y=%d M=%d D=%d, hour=%d minute=%d second=%d\n", time_var->tm_year,
              time_var->tm_mon, time_var->tm_mday, time_var->tm_hour, time_var->tm_min,
              time_var->tm_sec );
   } 

   /* Adjust the times we provided to what the structure expects */
   time_var->tm_year = time_var->tm_year - 1900;  /* years returned as count from 1900 */
   time_var->tm_mon = time_var->tm_mon - 1;       /* months returned as 0-11 */

   /* set these to 0 for now */
   time_var->tm_wday = 0;   /* week day */
   time_var->tm_yday = 0;   /* day in year */
   time_var->tm_isdst = pSCHEDULER_CONFIG_FLAGS.isdst_flag;  /* use daylight savings ? */
   
   /* and workout the time */
   time_number = mktime( time_var );
   
   /* done, free memory */
   MEMORY_free( time_var );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
		   myprintf( "DEBUG: Leaving UTILS_make_timestamp, returning %s", ctime(&time_number) );
   }

   return( time_number );
} /* UTILS_make_timestamp */

/* ***********************************************
 * Return the current time in seconds since 
 * Jan 1st, 1970.
   *********************************************** */
time_t UTILS_timenow_timestamp( void ) {
		/* Use 8 here, as its called almost every second */
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_TIMERS) {
		   myprintf( "DEBUG: utils.c, entered and leaving UTILS_timenow_timestamp (its one line)\n" );
   }

   return( time(0) ); /* time now as seconds since Jan 1 1970, 00:00 */
} /* UTILS_timenow_timestamp */

/* ***********************************************
   UTILS_remove_trailing_spaces

   *********************************************** */
void UTILS_remove_trailing_spaces( char *sptr ) {
		char *ptr1;
		int strlength;
		
		/* do we have to do anything ? */
		strlength = strlen(sptr);
		if (strlength < 1) {
				return;
		}

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
		   myprintf( "DEBUG: utils.c, entered UTILS_remove_trailing_spaces\n" );
   }

		/* go to end of string */
		ptr1 = (sptr + strlength) - 1;
		/* decrement end of string pointer while spaces */
		while ( (ptr1 > sptr) && (*ptr1 == ' ') ) {
				ptr1--;
		}
		/* move back up to last space found */
        ptr1++;
		/* null terminate the field */
		*ptr1 = '\0';

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
		   myprintf( "DEBUG: Leaving UTILS_remove_trailing_spaces\n" );
   }
} /* UTILS_remove_trailing_spaces */

/* ***********************************************
   UTILS_obtain_uid_from_name
   return -1 if user not found, else the uid
INFO: this mallocs the structure, but it can't be free'd
must be in system reuseable space, need to run memory
monitoring on this call.
   *********************************************** */
uid_t UTILS_obtain_uid_from_name( char * username ) {
   struct passwd *local_pswd_rec;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
		   myprintf( "DEBUG: utils.c, entered, and leaving UTILS_obtain_uid_from_name (its one line)\n" );
   }

   /* this call mallocs a passwd structure for us */
   if ( (local_pswd_rec = getpwnam( username )) == NULL ) {
		   return( -1 );
   }
   else {
		   return( local_pswd_rec->pw_uid );
   }
} /* UTILS_obtain_uid_from_name */

/* ---------------------------------------------------
   UTILS_get_dayname
   Return the three character day name for the day 
   number passed.
   --------------------------------------------------- */
void UTILS_get_dayname( int daynum, char * dayname ) {
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
		   myprintf( "DEBUG: utils.c, entered UTILS_get_dayname with %d\n", daynum );
   }
   switch (daynum) {
	   case 0 :
           {
              strcpy( dayname, "SUN" );
              break;
           }
	   case 1 :
           {
              strcpy( dayname, "MON" );
              break;
           }
	   case 2 :
           {
              strcpy( dayname, "TUE" );
              break;
           }
	   case 3 :
           {
              strcpy( dayname, "WED" );
              break;
           }
	   case 4 :
           {
              strcpy( dayname, "THU" );
              break;
           }
	   case 5 :
           {
              strcpy( dayname, "FRI" );
              break;
           }
	   case 6 :
           {
              strcpy( dayname, "SAT" );
              break;
           }
	   default :
           {
              strcpy( dayname, "-?-" );
			  break;
           }
   } /* end switch */
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
		   myprintf( "DEBUG: Leaving UTILS_get_dayname with %s\n", dayname );
   }
} /* UTILS_get_dayname */

/* ---------------------------------------------------
   UTILS_get_daynumber
   Return a day number for a three character day 
   identifier passed to us.
   --------------------------------------------------- */
int UTILS_get_daynumber( char * datastr ) {
		int result;
        if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
		   myprintf( "DEBUG: utils.c, entered UTILS_get_daynumber with %s\n", datastr );
        }
		if (memcmp(datastr,"SUN",3) == 0) { result = 0; }
		else if (memcmp(datastr,"MON",3) == 0) { result = 1; }
		else if (memcmp(datastr,"TUE",3) == 0) { result = 2; }
		else if (memcmp(datastr,"WED",3) == 0) { result = 3; }
		else if (memcmp(datastr,"THU",3) == 0) { result = 4; }
		else if (memcmp(datastr,"FRI",3) == 0) { result = 5; }
		else if (memcmp(datastr,"SAT",3) == 0) { result = 6; }
		else { result = 7; }
        if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
		   myprintf( "DEBUG: Leaving UTILS_get_daynumber with %d\n", result );
        }
		return( result );
} /* UTILS_get_daynumber */

/* ---------------------------------------------------
   UTILS_strncpy
   The system supplied strncpy copies exactly the number
   of bytes requested. If there is no \0 in that length
   then no \0 byte will be appended to the string.
   This calls the system strncpy and ensures that in the
   byte after the string (len + 1) a null is added.
   The buffer passed MUST BE LEN+1.
   It will also pad the resultant string out to the
   full length.
   Notes: this is fine for my code as all my data fields
   are the standard DEFINE_LEN+1 bytes when declared to
   allow for those system calls that would append a null.
   --------------------------------------------------- */
void UTILS_strncpy( char * dest, char * src, int len ) {
   char * ptr;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: utils.c, entered UTILS_strncpy\n" );
   }

   ptr = dest;
   ptr = ptr + len;
   strncpy( dest, src, len );
   UTILS_space_fill_string( dest, len );
   *ptr = '\0';

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving UTILS_strncpy\n" );
   }
} /* UTILS_strncpy */

/* ---------------------------------------------------
   UTILS_uppercase_string
   This is very messy, as I'm not going to assume the
   ascii table is being used, so I'm going to check
   each character individually rather than the nornal
   way of detecting the lowercase range and decrementing
   to uppercase.
   It should not give a slow response time to users as
   at present it is only used on the command input string
   in sched_cmd.
   If this becomes a regularly called function this will
   need to be revisited.

MID: stopped useage. It was upshifting filenames etc
so this needs to be customised to handle automatic
stop characters (ie ") and lengths.
   --------------------------------------------------- */
void UTILS_uppercase_string( char * datastr ) {
   int i, j;
   char * ptr;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: utils.c, entered UTILS_uppercase_string\n" );
   }

   ptr = datastr;
   j = strlen(datastr);
   for (i = 0; i < j; i++ ) {
		   if (*ptr == 'a') { *ptr = 'A'; }
		   else if (*ptr == 'e') { *ptr = 'E'; }
		   else if (*ptr == 'i') { *ptr = 'I'; }
		   else if (*ptr == 'o') { *ptr = 'O'; }
		   else if (*ptr == 'u') { *ptr = 'U'; }
		   else if (*ptr == 'b') { *ptr = 'B'; }
		   else if (*ptr == 'c') { *ptr = 'C'; }
		   else if (*ptr == 'd') { *ptr = 'D'; }
		   else if (*ptr == 'f') { *ptr = 'F'; }
		   else if (*ptr == 'g') { *ptr = 'G'; }
		   else if (*ptr == 'h') { *ptr = 'H'; }
		   else if (*ptr == 'j') { *ptr = 'J'; }
		   else if (*ptr == 'k') { *ptr = 'K'; }
		   else if (*ptr == 'l') { *ptr = 'L'; }
		   else if (*ptr == 'm') { *ptr = 'M'; }
		   else if (*ptr == 'n') { *ptr = 'N'; }
		   else if (*ptr == 'p') { *ptr = 'P'; }
		   else if (*ptr == 'q') { *ptr = 'Q'; }
		   else if (*ptr == 'r') { *ptr = 'R'; }
		   else if (*ptr == 's') { *ptr = 'S'; }
		   else if (*ptr == 't') { *ptr = 'T'; }
		   else if (*ptr == 'v') { *ptr = 'V'; }
		   else if (*ptr == 'w') { *ptr = 'W'; }
		   else if (*ptr == 'x') { *ptr = 'X'; }
		   else if (*ptr == 'y') { *ptr = 'Y'; }
		   else if (*ptr == 'z') { *ptr = 'Z'; }
           /* else leave alone */
		   ptr = ptr + 1;
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving UTILS_uppercase_string\n" );
   }
} /* UTILS_uppercase_string */

/* ---------------------------------------------------
   --------------------------------------------------- */
int UTILS_legal_time( char * timestr ) {
  int  local_hour, local_minute;
  char local_timestr[5];  /* work locally as we put nulls in the test string and we
							 don't want to trash the value the caller will be using */
  char *ptr;

  if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
		  myprintf( "DEBUG: In UTILS_legal_time with %s\n", timestr );
  }

  memcpy(local_timestr,timestr,5);
  ptr = (char *)&local_timestr;
  ptr = ptr+2;
  if (memcmp(ptr,":",1) != 0) {
          if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
		     myprintf( "DEBUG: Leaving UTILS_legal_time, time is not a legal time, no ':' found at byte 3\n" );
		  }
		  return(0);
  }
  /* put a null where the ':' was to terminate the hour */
  *ptr = '\0';
  local_hour = atoi( local_timestr );
  /* put a null at +5 (hh:mm) to terminate the minute */
  ptr = (char *)&local_timestr;
  ptr = ptr + 5;
  *ptr = '\0';
  /* then get the minute value */
  ptr = timestr;
  ptr = ptr + 3; /* skip the hh: */
  local_minute = atoi( ptr );

  /* then sanity check the values */
  if ( (local_minute > 59) || (local_hour > 23) ) {
          if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
		     myprintf( "DEBUG: Leaving UTILS_legal_time, time is not a legal time, h=%d, m=%d\n", local_hour, local_minute );
		  }
		  return( 0 );
  }

  /* looks ok, so approve it */
  if (pSCHEDULER_CONFIG_FLAGS.debug_level.utils >= DEBUG_LEVEL_PROC) {
		  myprintf( "DEBUG: Leaving UTILS_legal_time, time is valid\n" );
  }
  return( 1 );
} /* UTILS_legal_time */

/* ---------------------------------------------------
   --------------------------------------------------- */
void UTILS_encrypt_local( char * datastr, int datalen ) {
   char *ptr, *ptr2;
   int i;

   ptr = datastr;
   ptr2 = ptr + 1;
   for ( i = 0; i < (datalen-1); i++ ) { 
		   *ptr = (*ptr + *ptr2) - i;
		   ptr++;
   }
   *ptr = *datastr - i;
} /* UTILS_encrypt_local */

/* ---------------------------------------------------
 * OK, I was lazy. As printf so nicely formats messages
 * I have created this so I can use myprintf where I
 * would normally have to use snprintf and then a 
 * write to the log file. By using this I can just
 * pretend I am using printf, which is much easier.
 * Syntax for the call is identical to the syntax for
 * printf (allowing me to easily make global changes
 * of printf to myprintf when I have forgotten myself
 * a few times).
 * WARNING: Don't use the memorylib modules or do any
 * logging from this procedure or we will end up recursivly
 * calling ourselves.
   --------------------------------------------------- */
void myprintf( const char *fmt, ... ) {
        int n;
		char *p, *p2;
		va_list ap;
		time_t time_now;
		char time_now_text[50];

		if ( (p = malloc(1024)) == 0 ) { /* don't expect, or allow msgs > 1023 bytes */
				fprintf( stdout, "Unable to allocate 1024 bytes of memory for message display\n" );
				return;  /* can't even log an error */
		}

		/* The va_* routines are used when passing an unknown number of
		 * variables, and also the variables are of unknown types 
		 * normally used as
		 *    void va_start(va_list,last-parm-before-list)
		 *    type va_arg(va_list,ap,type)
		 *    void va_end(va_list)
		 * we can replace the va_arg hard work with vsnprintf as we know
		 * we are being called with printf parameters
		 */
         va_start(ap,fmt);
         n = vsnprintf( p, 1023, fmt, ap );   /* allowing only 511 bytes so we can have a null in 1024 */
         va_end(ap);

         /* n will be -1 on errors from vsnprintf, or may be > buffsize to be
          * used if it is not going to fit in the buffer */
         if ((n > -1) && (n < 1023)) {
		    if (msg_log_handle != NULL) {  /* Log file is open */
		       time_now = time(0);
		       snprintf( time_now_text, 49, "%s", ctime(&time_now) );
			   p2 = (char *)&time_now_text;
			   p2 = p2 + (strlen(time_now_text) - 1);
			   *p2 = '\0';
               fprintf( msg_log_handle, "%s %s", time_now_text, p );
			   /* Flush to ensure its written to the log file immediately
				* as I've noticed that with caching on my machine with a low
				* number of messages it can take hours for the log to be
				* updated otherwise */
			   fflush( msg_log_handle );
			   clearerr( msg_log_handle );
			}
			else {  /* no log file open, use stdout */
               fprintf( stdout, "%s", p );
			}
         }
         else {
	     	myprintf( "*ERR: Message of > 1023 bytes passed, discarded\n" );
         }
	 free(p);
}  /* myprintf */

/* ---------------------------------------------------
 * Fill in a celendar record based on day numbers.
 *
 * daylist is a string of 7 characters, representing days from Sun thru Sat,
 * each entry in the list set to Y will have the day set.
 * calbuffer is the calendar record being updated. The year is going to
 * be extracted from that.
   --------------------------------------------------- */
int UTILS_populate_calendar_by_day( char * daylist, calendar_def * calbuffer ) {
   char year_buffer[5];
   int  days[7];        /* can have up to seven week day triggers, 0=sun...6=sat */
   int  i, year_now, days_requested;
   char *ptr;
   int  year_to_use;
   time_t time_number;
   struct tm time_var;
   struct tm *time_var_ptr;

   time_number = time(0);
   time_var_ptr = (struct tm *)&time_var;


   strncpy( year_buffer, calbuffer->year, 5 );
   year_buffer[4] = '\0';
   year_to_use = atoi( year_buffer );
   year_to_use = year_to_use - 1900;  /* adjust for timestamp values */
   days_requested = 0;  /* for sanity checks */
   for (i = 0; i < 7; i++ ) {  days[i] = 0; };  /* clear list */
   ptr = daylist;
   for (i = 0; i < 7; i++ ) {                   /* then set whats required */
      if (*ptr == 'Y') {
          days[i] = 1;
          days_requested++;  /* for sanity checks */
      } 
      ptr++;
   }

   /* Get the year now for sanity checks */
   time_number = time(0);
   time_var_ptr = localtime((time_t *)&time_number);
   year_now = time_var_ptr->tm_year;

   /* Now do some basic sanity checks */
   if ( (year_to_use < year_now) || (days_requested < 1) ) {
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 0) {
         myprintf( "WARN: ZW005-Calendar set by days rejected, either no days or year < current year, days %d, year %d !\n",
                   days_requested, (year_to_use + 1900) );
      }
      return( 0 );
   }

   /* setup the pointers we need, and init time_number to start of year
	* requested by the caller */
   time_var_ptr = (struct tm *)&time_var;
   time_var_ptr->tm_year = year_to_use;
   time_var_ptr->tm_mon = 0;
   time_var_ptr->tm_mday = 1;
   time_var_ptr->tm_hour = 1; /* these three must be set to something also */
   time_var_ptr->tm_min = 1;
   time_var_ptr->tm_sec = 1;
   time_number = mktime( (struct tm *)time_var_ptr );

   while (time_var_ptr->tm_year == year_to_use) { /* while in correct year */
      if ( days[time_var_ptr->tm_wday] == 1 ) { /* this day is required */
         calbuffer->yearly_table.month[time_var_ptr->tm_mon].days[(time_var_ptr->tm_mday - 1)] = 'Y';
      }
      /* and move on to check the next day */
      time_number = time_number + (60 * 60 * 24);
      time_var_ptr = localtime((time_t *)&time_number);
   }

   return( 1 );
} /* UTILS_populate_calendar_by_day */

/* --------------------------------------------------------------- 
   UTILS_wildcard_parse_string:

   Will test a string buffer against a string mask that is using
   the "*" character as a wildcard mask.

   Both input buffers must be null terminated strings

   Returns: 0 - no match, 1 - match passed tests.
   --------------------------------------------------------------- */
int UTILS_wildcard_parse_string( char *buf, char *searchmask ) {

   int foundmatch, searchlen;
   char *ptr, *ptr2, *ptr3;
   char savechar, savecharoffset;

   /* lets compare */
   ptr = buf;
   ptr2 = searchmask;
   foundmatch = 1; /* defaults is match, easier to turn off than on */
   /* some lines read from stdin can have a newline char, strip out */
   if (ptr[strlen(buf) - 1] == '\n') { ptr[strlen(buf) - 1] = '\0'; }

   /* inserted specifically for use in the scheduler library. As most fields
	* passed to us will be space padded we need to reverse scan through
	* the data buffer to eliminate the trailing spaces. AS THIS IS THE CALLERS
	* BUFFER, put the space we remove back in before we exit here.
	*/
   ptr3 = ptr;
   ptr3 = ptr3 + strlen(buf);
   while ((*ptr3 == ' ') || (*ptr3 == '\0')) { ptr3--; }
   ptr3++;
   savecharoffset = (ptr3 - ptr);
   ptr3 = ptr + savecharoffset;
   savechar = *ptr3;
   *ptr3 = '\0';

   /* special conditions being an * at the start or no * */
   ptr3 = ptr2;
   while ((*ptr3 != '*') && (*ptr3 != '\0')) { ptr3++; }
   searchlen = (ptr3 - ptr2);
   if (searchlen > 0) {
      if (memcmp(ptr, ptr2, searchlen) != 0) {
		  foundmatch = 0;
	  } else {
          ptr = ptr + searchlen;
	  }
   }
   while (*ptr3 == '*') { ptr3++; }
   ptr2 = ptr3;

   /* now loop for each remaining text block within * */
   while ( (*ptr != '\0') && (*ptr2 != '\0') && (foundmatch == 1) ) {
      /* 1. Find the next search string */
      ptr3 = ptr2;
	  while ((*ptr3 != '*') && (*ptr3 != '\0')) { ptr3++; }
	  searchlen = (ptr3 - ptr2);
      /* 2. find this string within the data buffer */
	  while ( (*ptr != '\0') && (memcmp(ptr, ptr2, searchlen) != 0) ) {
		  ptr++;
	  }
      if (*ptr == '\0') {  /* not found */
         foundmatch = 0;
	  } else {             /* found, skip it in input string */
         ptr = ptr + searchlen;
	     while (*ptr3 == '*') { ptr3++; }
	     ptr2 = ptr3;
	  }	  
   } /* while */

   /* The final special case, if the pattern had an * at the end */
   if ((*ptr2 == '\0') && (*ptr != '\0')) {
      ptr2--;
	  if (*ptr2 != '*') { foundmatch = 0; }
   }

   /* put back the byte we swiped from the callers buffer */
   ptr = buf;
   ptr3 = ptr + savecharoffset;
   *ptr3 = savechar;

   /* Done, return the result */
   return (foundmatch);
} /* UTILS_wildcard_parse_string */
