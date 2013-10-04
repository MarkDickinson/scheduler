/*
    CALENDAR.C

    Manage the calendar file(s) used by the scheduler.

    Scheduler is by Mark Dickinson, 2001
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <memory.h>
#include <time.h>
#include "debug_levels.h"
#include "calendar.h"
#include "utils.h"
#include "schedlib.h"
#include "bulletproof.h"
#include "api.h"
#include "calendar_utils.h"
#include "jobslib.h"
#include "memorylib.h"

/* ---------------------------------------------------
   CALENDAR_Initialise
   Perform all initialisation needed by the calendar
   functions.
   --------------------------------------------------- */
int CALENDAR_Initialise( void ) {
   if ( (calendar_handle = fopen( calendar_file, "rb+" )) == NULL ) {
      if (errno == 2) {
         if ( (calendar_handle = fopen( calendar_file, "ab+" )) == NULL ) {
            myprintf( "*ERR: CE001-Unable to create Calendar file %s failed, errno %d (CALENDAR_Initialise)\n", calendar_file, errno );
            UTILS_set_message( "Create of calendar file failed" );
            perror( "Create of calendar file failed" );
            return 0;
         }
         else {
            /* created, close and open the way we want */
            fclose( calendar_handle );
            calendar_handle = NULL;
            return( CALENDAR_Initialise() );
         }
      }
      else {
         myprintf( "*ERR: CE002-Open of Calendar file %s failed, errno %d (CALENDAR_Initialise)\n", calendar_file, errno );
         UTILS_set_message( "Open of calendar file failed" );
         perror( "Open of calendar file failed" );
         return 0;
      }
   }
   else {
      fseek( calendar_handle, 0, SEEK_SET );
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
         myprintf( "INFO: CI001-Open of calendar file %s completed\n", calendar_file );
	  }
      return 1;
   }
} /* CALENDAR_Initialise */


/* ---------------------------------------------------
   CALENDAR_Terminate
   Perform all cleanup needed by the calendar functions.
   --------------------------------------------------- */
void CALENDAR_Terminate( void ) {
   if (calendar_handle != NULL) {
      fclose( calendar_handle );
      if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
         myprintf( "INFO: CI002-Closed: Calendar file\n" );
	  }
   }
   else {
      myprintf( "*ERR: CE003-CALENDAR_Terminate called with no calendar files open, fclose ignored ! (CALENDAR_Terminate)\n" );
   }
} /* CALENDAR_Terminate */


/* ---------------------------------------------------
   CALENDAR_read_record
   Read a calendar record that matches the calendar
   name and flag provided by the caller in datarec.

   If the record is found return the record number as 
   the result and place the record into the callers
   buffer (datarec).

   If the record is not found return -1, callers buffer
   is untouched.
   --------------------------------------------------- */
long CALENDAR_read_record( calendar_def * datarec ) {
   int  lasterror;
   long recordfound;
   calendar_def *local_rec;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered CALENDAR_read_record\n" );
   }

   UTILS_space_fill_string( datarec->calendar_name, CALENDAR_NAME_LEN );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG: Calendar search request for cal='%s', type=%c, year=%s\n",
                 datarec->calendar_name, datarec->calendar_type, datarec->year );
   }

   /*
    * Position to the begining of the file
    */
   lasterror = fseek( calendar_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      myprintf( "*ERR: CE004-Seek to start of calendar file %s failed (CALENDAR_read_record)\n", calendar_file );
      UTILS_set_message( "Seek to start of file failed" );
      return( -1 );
   }
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG: Positioned to start of file (CALENDAR_read_record)\n" );
   }

   /*
    * Allocate memory for a local copy of the record
    */
   if ((local_rec = (calendar_def *)MEMORY_malloc( sizeof( calendar_def ),"CALENDAR_read_record" )) == NULL) {
      myprintf( "*ERR: CE005-Unable to allocate memory required for read operation (CALENDAR_read_record)\n" );
      UTILS_set_message( "Unable to allocate memory required for read operation" );
      return( -1 );
   }

   /*
    * Loop reading through the file until we find the
    * record we are looking for.
    */
   recordfound = -1;
   while ((recordfound == -1) && (!(feof(calendar_handle))) ) {
      lasterror = fread( local_rec, sizeof(calendar_def), 1, calendar_handle  );
      if (ferror(calendar_handle)) {
		 myprintf( "*ERR: CE006-Read error on calendar file %s (CALENDAR_read_record)\n", calendar_file );
         UTILS_set_message( "Read error on calendar file" );
         MEMORY_free( local_rec );
         return( -1 );
      }

      /*
       * Check to see if this is the record we want.
       * check for calendar name len + type + year bytes, the name and the type flag
       */
      if ( (memcmp(local_rec->calendar_name, datarec->calendar_name, CALENDAR_NAME_LEN) == 0) &&
           (memcmp(local_rec->year, datarec->year, 4) == 0) &&
           (local_rec->calendar_type == datarec->calendar_type) ) {
         recordfound = ( ftell(calendar_handle) / sizeof(calendar_def) ) - 1;
      }
   }

   /*
    * If we found a record copy it to the callers buffer.
    */
   if (recordfound != -1) {
      memcpy( datarec->calendar_name, local_rec->calendar_name, sizeof( calendar_def ) );
   }
   else {
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_IO) {
	     myprintf( "DEBUG: Calendar %s (Type %c) for year %s was not found on server\n",
                   datarec->calendar_name, datarec->calendar_type, datarec->year );
      }
   }
   /*
    * Free our temporary buffer
    */
   MEMORY_free( local_rec );

   /*
    * Return the record number we found.
    */
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving CALENDAR_read_record\n" );
   }
   return ( recordfound );
} /* CALENDAR_read_record */


/* ---------------------------------------------------
   CALENDAR_write_record
   Write a record to the calendar file.

   If the user provides a record number we will write
   the record to the specified position in the file. 
   If the user provides a record number of -1 we will
   write the record as a new record to the end of the
   file.

   Return the record number that the record was 
   actually written to.
   --------------------------------------------------- */
long CALENDAR_write_record( calendar_def *datarec, long recpos ) {
   int lasterror;
   long currentpos;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered CALENDAR_write_record\n" );
   }

   /*
    * Check the fields in the record are legal.
    */
   UTILS_space_fill_string( datarec->calendar_name, CALENDAR_NAME_LEN );
   if (BULLETPROOF_CALENDAR_record( datarec ) != 1) {   /* failed checks if <> 1 */
      return( -1 );
   }

   /*
	* Sanity check any holiday calendar
	*/
   if ( (datarec->observe_holidays == 'Y') && (recpos == -1) ) { /* only check on initial creation */
      if (CALENDAR_calendar_exists(datarec->holiday_calendar_to_use, datarec->year, '1') == 0) {
         myprintf( "*ERR: CE007-Holiday calendar %s does not exist, calendar write rejected (CALENDAR_write_record)\n",
                   datarec->holiday_calendar_to_use );
         UTILS_set_message( "*E* Specified holiday calendar does not exist" );
         return( -1 );
      }
   }

   /*
    * If the record number is -1 the caller wants us to
    * write to eof. Otherwise seek to the record number
    * specified and update it.
    */
   if (recpos == -1) {
      lasterror = fseek( calendar_handle, 0, SEEK_END );
   }
   else {
      lasterror = fseek( calendar_handle, (recpos * sizeof(calendar_def)), SEEK_SET );
   }
   if (lasterror != 0) {
      myprintf( "*ERR: CE008-Seek error on calendar file %s (CALENDAR_write_record)\n", calendar_file );
      UTILS_set_message( "Calendar file seek error" );
      return( -1 );
   }

   /*
    * get the current record position from the file system
    * we trust this rather than assuming the seek worked.
    * Also we need to know the eof record number to return
    * to the caller if we are writing to eof.
    */
   currentpos = ( ftell(calendar_handle) / sizeof(calendar_def) );
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_IO) {
      myprintf( "DEBUG: Positioned to record %d (CALENDAR_write_record)\n", (int)currentpos );
   }

   /*
    * note: cannot rely on return code from a buffered write, use
    *       ferror to see if it worked.
    */
   lasterror = fwrite( datarec, sizeof(calendar_def), 1, calendar_handle );
   if (ferror(calendar_handle)) {
      myprintf( "*ERR: CE009-Write error on calendar file %s (CALENDAR_write_record)\n", calendar_file );
      UTILS_set_message( "Write error on Calendar file" );
      return( -1 );
   }

   /*
    * return the recordnumber position we wrote the record into.
    */
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving CALENDAR_write_record\n" );
   }
   return ( currentpos );
} /* CALENDAR_write_record */

/* ---------------------------------------------------
   CALENDAR_update_record
   --------------------------------------------------- */
long CALENDAR_update_record( calendar_def *datarec, int updatetype ) {
   long record_number;
   long result;
   calendar_def *local_rec;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered CALENDAR_update_record\n" );
   }

   result = 0;   /* default */

   /*
    * Allocate memory for a local copy of the record
    */
   if ((local_rec = (calendar_def *)MEMORY_malloc( sizeof( calendar_def ),"CALENDAR_update_record" )) == NULL) {
      myprintf( "*ERR: CE010-Unable to allocate memory for update operation (CALENDAR_update_record)\n" );
      UTILS_set_message( "Unable to allocate memory required for update operation" );
      return( -1 );
   }

   /*
    * Take a copy
    */
   memcpy( local_rec, datarec, sizeof( calendar_def ) );
   UTILS_space_fill_string( local_rec->calendar_name, CALENDAR_NAME_LEN );
   /*
    * Read using the copy. The copy will be overwritten if
    * a matching record is actually found.
    *
    * ADD
    * If a matching record is found it's an error. The caller
    * will need to do an update rather than an add.
    *
    * UPDATE and DELETE
    * If a matching record is not found it's an error.
    * The caller will need to do an add rather than an update.
    */
   record_number = -1; /* default */
   record_number = CALENDAR_read_record(local_rec);
   switch (updatetype) {
	   case CALENDAR_ADD :
           {
              if (record_number != -1) {
                 UTILS_set_message( "Attempt to add a duplicate record rejected" );
                 result = -1;
              }
              break;
           }
	   case CALENDAR_UPDATE :
           {
              if (record_number == -1) {
                 UTILS_set_message( "Record not found for update." );
                 result = -1;
              }
              break;
           }
	   case CALENDAR_DELETE :
           {
              if (record_number == -1) {
                 UTILS_set_message( "Record not found for delete." );
                 result = -1;
              }
              else {
                 memcpy( datarec, local_rec, sizeof(calendar_def) );
                 datarec->calendar_type = '9';
              }
              break;
           }
	   default :
           {
              UTILS_set_message( "Illegal update code passed to CALENDAR_update." );
              result = -1;
			  break;
           }
   } /* end switch */


   /*
    * Discard the memory we have allocated
    */
   MEMORY_free( local_rec );

   /* did we have any errors that prevent us continuing */
   if (result == -1) { return( -1 ); }

   /*
    * Write the new record.
    */
   record_number = CALENDAR_write_record(datarec, record_number);
   if (record_number < 0) {
      myprintf( "*ERR: CE011-Unable to write to calendar file %s (CALENDAR_update_record)\n", calendar_file );
      UTILS_set_message( "Unable to write record to Calendar file." );
      return( -1 );
   }

   /*
    * We return the record number of the new record.
    */
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving CALENDAR_update_record\n" );
   }
   return( record_number );
} /* CALENDAR_update_record */

/* ---------------------------------------------------
   CALENDAR_update
   Adds a new record to the calendar file.

   Return the record number the record was written to,
   or if we had an error return -1.
   --------------------------------------------------- */
long CALENDAR_update( calendar_def * datarec ) {
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entering and leaving CALENDAR_update (one line)\n" );
   }
   return( CALENDAR_update_record( datarec, CALENDAR_UPDATE ) );
} /* CALENDAR_update */

/* ---------------------------------------------------
   CALENDAR_add
   Adds a new record to the calendar file.

   Return the record number the record was written to,
   or if we had an error return -1.
   --------------------------------------------------- */
long CALENDAR_add( calendar_def * datarec ) {
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entering and leaving CALENDAR_add (one line)\n" );
   }
   return( CALENDAR_update_record( datarec, CALENDAR_ADD ) );
} /* CALENDAR_add */

/* ---------------------------------------------------
   CALENDAR_delete
   Mark a record as 'too be deleted' in the calendar
   file.

   Return the record number the record was written to,
   or if we had an error return -1.
   --------------------------------------------------- */
long CALENDAR_delete( calendar_def * datarec ) {
   long found_count, record_pos;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entering CALENDAR_delete\n" );
   }

   if (datarec->calendar_type == '1') { /* Holiday Calendar Type */
      found_count = CALENDAR_count_entries_using_holcal( datarec->calendar_name );
      if (found_count == -1) {
         myprintf( "*ERR: CE012-Unable to determine if other calendars refer to %s\n", datarec->calendar_name );
         UTILS_set_message( "Unable to determine if other calendars use this." );
         record_pos = -1;
      }
      else if (found_count > 0) {
         myprintf( "*ERR: CE013-Rejected DELETE of calendar %s, it is used by %d other calendars\n",
                   datarec->calendar_name, found_count );
         UTILS_set_message( "This calendar is refered to by other calendars, CANNOT DELETE." );
         record_pos = -1;
      }
      else { /* found_count = 0 for calendar search */
         found_count = JOBS_count_entries_using_calendar( datarec->calendar_name );
         if (found_count == -1) {
            myprintf( "*ERR: CE014-Unable to determine if any jobs refer to %s\n", datarec->calendar_name );
            UTILS_set_message( "Unable to determine if any jobs use this." );
            record_pos = -1;
         }
         else if (found_count > 0) {
            myprintf( "*ERR: CE015-Rejected DELETE of calendar %s, it is used by %d jobs\n",
                      datarec->calendar_name, found_count );
            UTILS_set_message( "This calendar is refered to by existing jobs, CANNOT DELETE." );
            record_pos = -1;
         }
         else {
            record_pos = CALENDAR_update_record( datarec, CALENDAR_DELETE );
         }
      }
   }
   else if (datarec->calendar_type == '0') { /* Job Calendar Type */
         found_count = JOBS_count_entries_using_calendar( datarec->calendar_name );
         if (found_count == -1) {
            myprintf( "*ERR: CE016-Unable to determine if any jobs refer to %s\n", datarec->calendar_name );
            UTILS_set_message( "Unable to determine if any jobs use this." );
            record_pos = -1;
         }
         else if (found_count > 0) {
            myprintf( "*ERR: CE017-Rejected DELETE of calendar %s, it is used by %d jobs\n",
                      datarec->calendar_name, found_count );
            UTILS_set_message( "This calendar is refered to by existing jobs, CANNOT DELETE." );
            record_pos = -1;
         }
         else {
            record_pos = CALENDAR_update_record( datarec, CALENDAR_DELETE );
         }
   }
   /* else type is 9, a deleted entry */

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving CALENDAR_delete\n" );
   }

   return( record_pos );
} /* CALENDAR_delete */

/* ---------------------------------------------------
   CALENDAR_merge_worker
   Depending on the flag passed either merge the
   passed record with the existing record, or delete
   entries matching the passed records.

   Return the record number the record was written to,
   or if we had an error return -1.
   --------------------------------------------------- */
long CALENDAR_merge_worker( calendar_def * datarec, int mergeflag ) {
   long op_result;
   int i, j;
   calendar_def * local_rec;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered CALENDAR_merge_worker\n" );
   }

   /*
    * Allocate memory for a local copy of the record
    */
   if ((local_rec = (calendar_def *)MEMORY_malloc( sizeof( calendar_def ),"CALENDAR_merge_worker" )) == NULL) {
      myprintf( "*ERR: CE018-Unable to allocate memory required for merge operation (CALENDAR_merge)\n" );
      UTILS_set_message( "Unable to allocate memory required for merge operation" );
      return( -1 );
   }


   /*
    * Read the existing record
    */
   memcpy( local_rec->calendar_name, datarec->calendar_name, sizeof(calendar_def) );
   op_result = CALENDAR_read_record( local_rec );
   if (op_result >= 0) {
      /* --- merge the two calendar entries now --- */
      for (i = 0; i < 12; i++) {
         for (j = 0; j < 31; j++) {
            if (mergeflag == 1) {   /* we are merging */
               if (datarec->yearly_table.month[i].days[j] == 'Y') {
                  local_rec->yearly_table.month[i].days[j] = 'Y';
			   }
			}
			else {                 /* we are un-merging */
               if ( (datarec->yearly_table.month[i].days[j] == 'Y') &&
                    (local_rec->yearly_table.month[i].days[j] == 'Y') ) {
                  local_rec->yearly_table.month[i].days[j] = 'N';
			   }
			}
		 }
	  }

      /* --- and do the update, skip the normal update logic as we have the
       * record number already --- */
      op_result = CALENDAR_write_record( local_rec, op_result );
   }

   /*
    * Discard the memory we have allocated
    */
   MEMORY_free( local_rec );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving CALENDAR_merge_worker\n" );
   }

   return( op_result );
} /* CALENDAR_merge_worker */

/* ---------------------------------------------------
   CALENDAR_merge
   Merge the data record passed with the existing
   data record for the calendar entry.

   Return the record number the record was written to,
   or if we had an error return -1.
   --------------------------------------------------- */
long CALENDAR_merge( calendar_def * datarec ) {
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entering and Leaving CALENDAR_merge (one line)\n" );
   }
   return( CALENDAR_merge_worker( datarec, 1 ) );  /* merge data */
} /* CALENDAR_merge */

/* ---------------------------------------------------
   CALENDAR_unmerge
   Remove matching entries in the record passed from
   the existing calendar record.

   Return the record number the record was written to,
   or if we had an error return -1.
   --------------------------------------------------- */
long CALENDAR_unmerge( calendar_def * datarec ) {
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entering and Leaving CALENDAR_unmerge (one line)\n" );
   }
   return( CALENDAR_merge_worker( datarec, 0 ) );  /* don't merge data, delete matches */
} /* CALENDAR_unmerge */

/* ---------------------------------------------------
 * CALENDAR_next_runtime_timestamp
 *
 * NOTE: DOES NOT SUPPORT CATCHUP AS YET
 *
 * On input timestampout will contain the date and time the user provided
 * for the job to run. We need to save the time part of this through any
 * adjustments we will make.
 *
 * start searching for calendar for current year
 * read the calendar record,
 * loop ---
 *  find the next day this month
 *  not found, check the next month.. thru the year
 *  not found check the next year... until end of calendar records matching
 *  name
 *
 *  put values found into time var
 *  convert to a timestamp
 *
 *  Returns 1 if a usable calendar date was found, 0 if not.
 *
 *  NOTES: I am using adjusted_year to hold the year
 *  as a debug display immediately after ->tm_year+1900
 *  showed the year had been adjusted+1900, but a debug
 *  line immediately after it (identical debug printf line)
 *  showed it had reverted to its original value (there
 *  were no intermediate steps between the two display lines).
 *  As such I save the year into a variable and use that as the
 *  time structure just cannot be trusted.
 * --------------------------------------------------- */
int CALENDAR_next_runtime_timestamp( char *calname, char *timestampout, int catchupon ) {
   /* Call as per the original processing */
   return( CALENDAR_next_runtime_timestamp_original( calname, timestampout, catchupon, 0 ) );
}
int CALENDAR_next_runtime_timestamp_calchange( char *calname, char *timestampout, int catchupon ) {
   /* Call as per the original processing, with extra flag set */
   return( CALENDAR_next_runtime_timestamp_original( calname, timestampout, catchupon, 1 ) );
}
int CALENDAR_next_runtime_timestamp_original( char *calname, char *timestampout, int catchupon, int usetodayallowed ) {
   int calendar_found, years_searched, i, j, calcheck, adjusted_year, z;
   int found_mon, found_day;
   char saved_time[9];  /* hh:mm:ss + pad */
   char year_to_use[5];  /* yyyy + pad */
   time_t timestamp;
   struct tm time_var;
   struct tm *time_var_ptr;
   char *ptr;
   calendar_def * calbuffer;
   char workbuffer[21];  /* to allow us to use UTILS_number_to_string which fails on small buffers */

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered CALENDAR_next_runtime_timestamp, usetoday=%d, catchup=%d\n", usetodayallowed, catchupon );
   }

   /* init our found values to a known illegal value */
   found_mon = found_day = 99;

   /* save the time part */
   ptr = timestampout;
   ptr = ptr + 9; /* skip 'YYYYMMDD ' */
   strncpy( saved_time, ptr, 8 );
   ptr[8] = '\0';

   /* The date we use as a base to check for the next later run
	* is dependant upon whether the caller wishes to catchup any
	* missed dates prior to the current date */
   if (catchupon != 0) {  /* next day from the day in the date passed */
      timestamp = UTILS_make_timestamp( timestampout ); /* time provided */
   }
   else { /* no catchup, next date from current day */
      time( (time_t *)&timestamp );                     /* time now */
   }

   time_var_ptr = (struct tm *)&time_var;
   time_var_ptr = localtime( &timestamp );
   /* adjust for the quirks in the returned values */ 
   adjusted_year = time_var_ptr->tm_year + 1900;  /* years returned as count from 1900 */
   /* tm_mon has mons as 0-11 wich is OK for us */
   /* tm_mday has days from 1-n, which is OK as while we use 0-(n-1) we want to
	* start at the next days entry anyway */
   /* MID: Change --- this is now called when a calendar is updated to
	* recschedule jobs using the calendar, so we must allow for it to be
	* rescheduled on in the current day also if needed. */
   if (usetodayallowed != 0) {
      time_var_ptr->tm_mday = time_var_ptr->tm_mday - 1;
   }

   /* start searching in the current year */
   UTILS_number_to_string( adjusted_year, (char *)&workbuffer, 20 );
   strncpy( year_to_use, (char *)&workbuffer[16], 4 );
   year_to_use[4] = '\0';

   /* Need a calendar buffer */
   if ((calbuffer = (calendar_def *)MEMORY_malloc( sizeof( calendar_def ),"CALENDAR_next_runtime_timestamp_original" )) == NULL) {
      myprintf( "*ERR: CE019-Unable to allocate memory required for reschedule operation (CALENDAR_next_runtime_timestamp_original)\n" );
      UTILS_set_message( "Unable to allocate memory required for calendar rescheduling operation" );
      return( 0 );  /* failed */
   }

   /* use to position to the current month record */
   strcpy(calbuffer->calendar_name, calname );
   calbuffer->calendar_type = '0';  /* Job calendar required */
   strcpy(calbuffer->year, year_to_use );

   calendar_found = 0;
   years_searched = 0;
   while (calendar_found == 0) {
      years_searched++;  /* count read attempts */
      if (CALENDAR_read_record( calbuffer ) < 0) {  /* read failed */
          if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_IO) {
             myprintf( "DEBUG: No record found for year %s\n", year_to_use );
          }
          adjusted_year++;
          UTILS_number_to_string( adjusted_year, (char *)&workbuffer, 20 );
          strncpy( year_to_use, (char *)&workbuffer[16], 4 );
          year_to_use[4] = '\0';
          strcpy(calbuffer->year, year_to_use );
      }
	  else { /* record was read, any more scheduled dates this year ? */
         if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_IO) {
            myprintf( "DEBUG: Record retrieved for year %s\n", year_to_use );
         }
		 if (time_var_ptr->tm_mday >= 31) {  /* if beyond end of current month go to next */
				 time_var_ptr->tm_mday = 0;
				 time_var_ptr->tm_mon++;
		 }
		 if (time_var_ptr->tm_mon >= 12) { /* if beyond end of current year go to next */
                 if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_IO) {
                    myprintf( "DEBUG: No scheduled dates remain for year %s, incrementing year\n", year_to_use );
                 }
				 time_var_ptr->tm_mon = 0;
				 time_var_ptr->tm_mday = 0;
                 time_var_ptr->tm_year++;
                 adjusted_year++;
                 UTILS_number_to_string( adjusted_year, (char *)&workbuffer, 20 );
                 strncpy( year_to_use, (char *)&workbuffer[16], 4 );
                 year_to_use[4] = '\0';
                 strcpy(calbuffer->year, year_to_use );
		 }
		 else {
            i = time_var_ptr->tm_mon;
            j = time_var_ptr->tm_mday;
			for (i = time_var_ptr->tm_mon; i < 12; i++) {
/* HMMM, A FUNNY BUG. THE BELOW BLOCK WRITES THE DEBUG MESSAGE WITH THE CORRECT
 * VALUE FROM TIME_VAR_PTR->TM_MDAY BUT THE J VALUE IN THE LOOP IS SET TO THE 
 * VALUE TIME_VAR_PTR->TM_MDAY WAS O U T S I D E THE I LOOP SO IT DOESN"T WORK.
 * SO, USE Z AS AN INTERMEDIARY.
 *             if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_VARS) {
 *                myprintf( "DEBUG: Scanning month entry %d, day entries %d to 30 for year %s\n", i, time_var_ptr->tm_mday, year_to_use );
 *             }
 *             for (j = time_var_ptr->tm_mday; j < 31; j++) {
*/
               z = time_var_ptr->tm_mday;
               if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_VARS) {
                  myprintf( "DEBUG: Scanning month entry %d, day entries %d to 30 for year %s\n", i, z, year_to_use );
               }
/* -- speed enhancement --, j loop only of not found */
if (calendar_found == 0) {
/* ------ */
               for (j = z; j < 31; j++) {
                  if (calbuffer->yearly_table.month[i].days[j] == 'Y') {
                      if (calendar_found == 0) {  /* only use the first found */
                         calendar_found = 1;
                         /* Do we need to check if a holiday calendar overrides
                          * the value ? */
                         if (calbuffer->observe_holidays == 'Y') {
                            calcheck = CALENDAR_check_holiday_setting( calbuffer->holiday_calendar_to_use,
                                                                       adjusted_year, i, j );
                            if (calcheck == 1) {
                               calendar_found = 0;   /* excluded, so cannot use */
                               if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_VARS) {
                                  myprintf( "DEBUG: Date found, but excluded by holiday calendar, continuing\n" );
                               }
                            }
							else if (calcheck == 2) {   /* holiday calendar error */
                               if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_VARS) {
                                  myprintf( "DEBUG: Error found in holiday override for year %s\n", year_to_use );
                               }
                               calendar_found = 3;
                            }
                         }
                         if (calendar_found == 1) {
                            if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_VARS) {
                               myprintf( "DEBUG: Next rundate found and accepted in year %s\n", year_to_use );
                            }
                            /* set the results found */
                            calendar_found = 1;
                            found_mon = i;
                            found_day = j + 1; /* +1, time days start at 1 not 0 */
                            if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_VARS) {
                               myprintf( "DEBUG: Found next date at Y=%s, month=%d, day=%d (in loop)\n",
                                          year_to_use, (found_mon + 1), found_day );
                            }
                         } /* if a true match, not overridden by holiday calendar */ 
                      } /* if not found already */
                  } /* if days match = Y */
                  else if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_VARS) {
                     myprintf( "DEBUG: entry %d %d = %c\n", i, j, calbuffer->yearly_table.month[i].days[j]);
                  }
               }  /* for j */
/* --- end speed enhancement --- */
}
/* ------ */
			   time_var_ptr->tm_mday = 0; /* for later months that the current start on the first of month */
            }  /* for i */
            if (calendar_found == 0) {  /* no more entries for this year, check the next */
                 if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_VARS) {
                    myprintf( "DEBUG: No scheduled dates remain in year %s, incrementing year\n", year_to_use );
                 }
				 time_var_ptr->tm_mon = 0;
				 time_var_ptr->tm_mday = 0;
                 time_var_ptr->tm_year++;
                 adjusted_year++;
                 UTILS_number_to_string( adjusted_year, (char *)&workbuffer, 20 );
                 strncpy( year_to_use, (char *)&workbuffer[16], 4 );
                 year_to_use[4] = '\0';
                 strcpy(calbuffer->year, year_to_use );
            }
		 }
      }
	  if (years_searched >= 5) {
         calendar_found = 2;   /* stop the loop with an error value */
	  }
   }  /* while calendar_found = 0 */

   if (calendar_found == 1) {
       if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_VARS) {
          myprintf( "DEBUG: Found final result of date Y=%s, M=%d, D=%d\n", 
                     year_to_use, found_mon, found_day );
       }
	   time_var_ptr->tm_mon = found_mon;
	   time_var_ptr->tm_mday = found_day;
       timestamp = mktime( time_var_ptr );
       UTILS_datestamp( timestampout, timestamp );
	   /* restore the time part we saved */
       ptr = timestampout;
       ptr = ptr + 9; /* skip 'YYYYMMDD ' */
       strncpy( ptr, saved_time, 8 ); 
       ptr = ptr + 8; /* skip 'HH:MM:SS' */
       *ptr = '\0'; /* always specifically terminate the result */
   }
   else if (calendar_found == 3) { /* holiday calendar error */
      calendar_found = 0;
	  myprintf("*ERR: CE020-Error on holiday calendar for %s, year %d\n",
                calbuffer->holiday_calendar_to_use, time_var_ptr->tm_year );
   }
   else {  /* must have dropped out with too many years, so not found */
      calendar_found = 0;
	  UTILS_set_message( "*E* No entries found for next 5 years of entries\n" );
	  myprintf("*ERR: CE021-CALENDAR %s has no scheduled run dates in the next 5 years !\n", calname );
   }

    /* Release the memory this proc has allocated */
    MEMORY_free( calbuffer );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered CALENDAR_next_runtime_timestamp\n" );
   }

    return( calendar_found );
} /* CALENDAR_next_runtime_timestamp */

/* ---------------------------------------------------
 * CALENDAR_check_holiday_setting
 *
 * Called by the application to see if a holiday calendar
 * entry is turned on for the holiday calendar at the
 * date to be checked.
 *
 * Return 1 if the value is set in the holiday calendar.
 * Return 0 if the value is not set and the job can run.
 * Return 2 if the holiday calender is unavailable.
 * --------------------------------------------------- */
int CALENDAR_check_holiday_setting( char * calname, int year, int month, int day ) {
   char yeartext[5]; /* 4 + pad */
   int  return_value;
   calendar_def * calbuffer;
   char workbuffer[10];

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered CALENDAR_check_holiday_setting, name %s, year %d, month %d, day %d\n",
                  calname, year, month, day );
   }

   /* messy workaround for bug in utils */
   UTILS_number_to_string( year, (char *)&workbuffer, 9 );
   strncpy( yeartext, (char *)&workbuffer[5], 4 );
   yeartext[4] = '\0';

/* read the holiday calendar entry,
 * if match then return 1
 * if no match then return 0
 * if error then return 2
 */
   /* Need a calendar buffer */
   if ((calbuffer = (calendar_def *)MEMORY_malloc( sizeof( calendar_def ),"CALENDAR_check_holiday_setting" )) == NULL) {
      myprintf( "*ERR: CE022-Unable to allocate memory required calendar check (CALENDAR_check_holiday_setting)\n" );
      UTILS_set_message( "Unable to allocate memory required for calendar check" );
      return( 2 );  /* failed */
   }

   strcpy(calbuffer->calendar_name, calname );
   calbuffer->calendar_type = '1';  /* Holiday calendar required */
   strcpy(calbuffer->year, yeartext );
   if (CALENDAR_read_record( calbuffer ) < 0) {  /* read failed */
      myprintf( "*ERR: CE023-Read of calendar %s, type %c, year %s failed\n",
                calbuffer->calendar_name, calbuffer->calendar_type, calbuffer->year );
      return_value = 2;
   }
   else {
      if (calbuffer->yearly_table.month[month].days[day] == 'Y') {
         if (pSCHEDULER_CONFIG_FLAGS.log_level > 1) {
            myprintf( "INFO: CI003-Holiday Calendar %s overrides date %d/%d/%d, date %d/%d/%d skipped for job\n",
                      calname, year, month, day, year, month, day );
         }
         return_value = 1;
      }
	  else {
         return_value = 0;
      }
   }

   /* Release the memory this proc has allocated */
   MEMORY_free( calbuffer );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving CALENDAR_check_holiday_setting\n" );
   }
   return( return_value );
} /* CALENDAR_check_holiday_setting */

/* ---------------------------------------------------
 * --------------------------------------------------- */
void CALENDAR_put_record_in_API( API_Header_Def * api_bufptr ) {
   calendar_def * calbuffer;
   char msgbuffer[101];

   strcpy( api_bufptr->API_System_Name, "CAL" );
   strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
   calbuffer = (calendar_def *)&api_bufptr->API_Data_Buffer;

   if (sizeof(calendar_def) > MAX_API_DATA_LEN) {
	  strcpy( api_bufptr->API_Data_Buffer, "API Data Buffer is to small to hold the response, programmer error !\n" );
      API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
      return;
   }

   if (CALENDAR_read_record(calbuffer) < 0) {
      /* For some reason the data in calbuffer is being overwritten ?, did we
	   * do it here ? 
	  snprintf( api_bufptr->API_Data_Buffer, MAX_API_DATA_LEN, "Calendar %s (Type %c) for year %s not found on server\n",
                calbuffer->calendar_name, calbuffer->calendar_type, calbuffer->year );
		*/
	  snprintf( msgbuffer, 100, "Calendar %s (Type %c) for year %s not found on server\n",
                calbuffer->calendar_name, calbuffer->calendar_type, calbuffer->year );
	  strcpy( api_bufptr->API_Data_Buffer, msgbuffer );
      API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
      return;
   }

   strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DATA );
   strcpy( api_bufptr->API_Command_Number, API_CMD_DUMP_FORMATTED );
   API_set_datalen( (API_Header_Def *)api_bufptr, sizeof(calendar_def) );
   return;
} /* CALENDAR_put_record_in_API */

/* ---------------------------------------------------
 * --------------------------------------------------- */
int CALENDAR_format_entry_for_display( char * calname, char * caltype, char * year, API_Header_Def * api_bufptr, FILE * tx ) {
   return( 0 );
} /* CALENDAR_format_entry_for_display */

/* ---------------------------------------------------
 * --------------------------------------------------- */
void CALENDAR_format_list_for_display( API_Header_Def * api_bufptr, FILE * tx ) {
   calendar_def * calbuffer;
   int lasterror, tempstrbuflen;
   char tempstrbuf[101]; /* 100 bytes + 1 pad */
   char type[8];

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered CALENDAR_format_list_for_display\n" );
   }

   API_init_buffer( (API_Header_Def *)api_bufptr );
   strcpy( api_bufptr->API_System_Name, "CAL" );
   strcpy( api_bufptr->API_Command_Response, API_RESPONSE_DISPLAY );

   if ( (calbuffer = (calendar_def *)MEMORY_malloc(sizeof(calendar_def),"CALENDAR_format_list_for_display")) == NULL) {
      strcpy( api_bufptr->API_Command_Response, API_RESPONSE_ERROR );
      strcpy( api_bufptr->API_Data_Buffer, "*E* Insufficient free memory on server to execute this command\n" );
      API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );
      myprintf( "*ERR: CE024-Insufficient free memory for requested CALENDAR operation\n" );
      return;
   }

   lasterror = fseek( calendar_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      UTILS_set_message( "file seek error (calendar file)" );
      perror( "file seek error (calendar file)" );
      MEMORY_free(calbuffer);
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
         myprintf( "DEBUG: Leaving CALENDAR_format_list_for_display\n" );
      }
      return;
   }

   /* loop until end of file */
   lasterror = 0;
   while ( (!(ferror(calendar_handle))) && (!(feof(calendar_handle))) ) {
      lasterror = fread( calbuffer, sizeof(calendar_def), 1, calendar_handle  );
      if (  (!(feof(calendar_handle))) && (calbuffer->calendar_type != '9')  ) {  /* 9 = deleted */
         if (calbuffer->calendar_type == '0') {
            strcpy( type, "JOB    " );
         }
		 else if (calbuffer->calendar_type == '1') {
            strcpy( type, "HOLIDAY" );
         }
         tempstrbuflen = snprintf( tempstrbuf, 100, "%s %s %s %s\n",
                                calbuffer->year, type, calbuffer->calendar_name, calbuffer->description );
         API_add_to_api_databuf( api_bufptr, tempstrbuf, tempstrbuflen, tx );
      }
   } /* while not eof */
   /* -- the calling proc flushed the last buffer block -- */
   API_set_datalen( (API_Header_Def *)api_bufptr, strlen(api_bufptr->API_Data_Buffer) );

   MEMORY_free( calbuffer );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving CALENDAR_format_list_for_display\n" );
   }
   return;
} /* CALENDAR_format_list_for_display */

/* ---------------------------------------------------
 * Return 0 if it does not exist
 * Return 1 if it does exist.
 * --------------------------------------------------- */
int CALENDAR_calendar_exists( char * calname, char * year, char type ) {
   calendar_def * calbuffer;
   int read_ok;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered CALENDAR_calendar_exists\n" );
   }
   if ( (calbuffer = (calendar_def *)MEMORY_malloc(sizeof(calendar_def),"CALENDAR_calendar_exists")) == NULL) {
      myprintf( "*ERR: CE025-Insufficient free memory for Holiday calendar check, allowing check to pass\n" );
      return( 1 );
   }

   strncpy( calbuffer->calendar_name, calname, CALENDAR_NAME_LEN );
   strncpy( calbuffer->year, year, 4 );
   calbuffer->calendar_type = type;
   if (CALENDAR_read_record( calbuffer ) < 0) {
      read_ok = 0;  /* failed to read record */
   }
   else {
      read_ok = 1;   /* got record */
   }

   MEMORY_free( calbuffer );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving CALENDAR_calendar_exists\n" );
   }
   return( read_ok );
} /* CALENDAR_calendar_exists */

/* ---------------------------------------------------
 * Purpose is to scan through the calendar file to
 * count how many calender entries have the supplied
 * calendar name as a holiday calendar. This is
 * called by the delete routines to ensure the
 * databases are kept in sync.
 * Returns -1 if unable to count entries, caller to
 *            decide how to manage that.
 * Returns count of entries if OK
 * --------------------------------------------------- */
long CALENDAR_count_entries_using_holcal( char * calname ) {
   calendar_def * calbuffer;
   int lasterror;
   long found_count;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered CALENDAR_count_entries_using_holcal\n" );
   }

   if ( (calbuffer = (calendar_def *)MEMORY_malloc(sizeof(calendar_def),"CALENDAR_count_entries_using_holcal")) == NULL) {
      myprintf( "*ERR: CE026-Insufficient free memory to count entries using calendar, returning -1\n" );
      return( -1 );
   }

   lasterror = fseek( calendar_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      perror( "file seek error (calendar file)" );
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
         myprintf( "DEBUG: Leaving CALENDAR_count_entries_using_holcal\n" );
      }
      MEMORY_free(calbuffer);
      UTILS_set_message( "file seek error (calendar file)" );
      return( -1 );
   }

   /* loop until end of file */
   found_count = 0;
   lasterror = 0;
   while ( (!(ferror(calendar_handle))) && (!(feof(calendar_handle))) ) {
      lasterror = fread( calbuffer, sizeof(calendar_def), 1, calendar_handle  );
      if (  (!(feof(calendar_handle))) && (calbuffer->calendar_type != '9')  ) {  /* 9 = deleted */
         if ( (calbuffer->observe_holidays == 'Y') &&
              (memcmp(calbuffer->holiday_calendar_to_use,calname,CALENDAR_NAME_LEN) == 0) ) {
            found_count++;
         }
      }
   } /* while not eof */

   MEMORY_free( calbuffer );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving CALENDAR_count_entries_using_holcal\n" );
   }
   return( found_count );
} /* CALENDAR_count_entries_using_holcal */

/* ---------------------------------------------------
 * Scan through the calendar database for any entries
 * belonging to a previous year, and if any are found
 * flag them for deletion. This is called during the
 * scheduler newday processing when databases are
 * being compressed.
 * Exits on error, but returns nothing as even if there
 * is an error we wish the caller to carry on and
 * compress what it can of the file.
 * --------------------------------------------------- */
void CALENDAR_checkfor_obsolete_entries( void ) {
   calendar_def * calbuffer;
   int lasterror, test_year, year_now;
   long record_num;
   struct tm timenow;
   struct tm * timenow_ptr;
   time_t system_time;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered CALENDAR_checkfor_obsolete_entries\n" );
   }

   if ( (calbuffer = (calendar_def *)MEMORY_malloc(sizeof(calendar_def),"CALENDAR_checkfor_obsolete_entries")) == NULL) {
      myprintf( "*ERR: CE027-Insufficient free memory to run obsolete entry check\n" );
      return;
   }

   /* get the time now so we can check for years prior to now */
   system_time = time( 0 ); /* time now */
   timenow_ptr = (struct tm *)&timenow;
   timenow_ptr = localtime( (time_t *)&system_time );

   /* WARN: I use year_now for checks as applying this +1900 to the pointer
	* struct gets lost and xxithe pointer value reverts to the original value
	* on the first reference (took ages to sort out so don't change) */
   year_now = timenow_ptr->tm_year + 1900;  /* convert to a real year */

   lasterror = fseek( calendar_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      perror( "file seek error (calendar file)" );
      if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
         myprintf( "DEBUG: Leaving CALENDAR_checkfor_obsolete_entries\n" );
      }
      MEMORY_free(calbuffer);
      UTILS_set_message( "file seek error (calendar file)" );
      return;
   }

   /* loop until end of file */
   lasterror = 0;
   record_num = 0;
   while ( (!(ferror(calendar_handle))) && (!(feof(calendar_handle))) ) {
      lasterror = fread( calbuffer, sizeof(calendar_def), 1, calendar_handle  );
      if (!(feof(calendar_handle))) { 
         if (calbuffer->calendar_type != '9') {  /* 9 = deleted */
            test_year = atoi(calbuffer->year);
            if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_VARS) {
               myprintf( "DEBUG: Comparing current %d against calendar %d\n",
                         year_now, test_year );
            }
            if (test_year == 0) {
               myprintf( "*ERR: CE028-Calendar %s has an invalid year entry of %s\n", calbuffer->calendar_name, calbuffer->year );
            }
            else if (year_now > test_year) {
               calbuffer->calendar_type = '9';
               lasterror = fseek( calendar_handle, (sizeof(calendar_def) * record_num), SEEK_SET );
               lasterror = fwrite( calbuffer, sizeof(calendar_def), 1, calendar_handle  );
               record_num++;
               lasterror = fseek( calendar_handle, (sizeof(calendar_def) * record_num), SEEK_SET );
               /* Always write this one now */
                  myprintf( "INFO: CI004-Calendar record %s for year %s expired, removing\n", 
                            calbuffer->calendar_name, calbuffer->year );
            }
            else {
               record_num++;
            }
         }
         else {
            record_num++;
         }
      }
   } /* while not eof */

   MEMORY_free( calbuffer );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.calendar >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving CALENDAR_checkfor_obsolete_entries\n" );
   }
   return;
} /* CALENDAR_checkfor_obsolete_entries */
