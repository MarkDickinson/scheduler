/*
 * THESE SHOULD ONLY BE USED BY JOBSCHED_CMD OR OTHER MODULES THAT CAN
 * WRITE TO STDOUT.  DO  N O T  INCLUDE IN THE SERVER CODE.
 *
 * These are helper tools for jobsched_cmd to populate calendar
 * records. They report errors to stdout. This can crash the server
 * if they are used from within the server.
 */
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "scheduler.h"

/* ---------------------------------------------------
 * Origionally for debugging.
 * --------------------------------------------------- */
void DEBUG_CALENDAR_dump_to_stdout( calendar_def * calbuffer ) {
   int i, j;
   char type[8];

   if (calbuffer->calendar_type == '0') {
      strcpy(type,"JOB");
   }
   else if (calbuffer->calendar_type == '1') {
      strcpy(type,"HOLIDAY");
   }
   else if (calbuffer->calendar_type == '9') {
      strcpy(type,"DELETED");
   }
   else {
      strcpy(type,"*ERROR*");
   }

   printf( "Calendar: %s  Type : %s  YEAR %s\n", calbuffer->calendar_name, type, calbuffer->year );
   printf( "Description: %s\n", calbuffer->description );
   printf( "Mth 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31\n" );
   for (i = 0; i < 12; i++ ) {
      if (i == 0) { printf( "JAN" ); }
	  else if (i == 1) { printf( "FEB" ); }
	  else if (i == 2) { printf( "MAR" ); }
	  else if (i == 3) { printf( "APR" ); }
	  else if (i == 4) { printf( "MAY" ); }
	  else if (i == 5) { printf( "JUN" ); }
	  else if (i == 6) { printf( "JUL" ); }
	  else if (i == 7) { printf( "AUG" ); }
	  else if (i == 8) { printf( "SEP" ); }
	  else if (i == 9) { printf( "OCT" ); }
	  else if (i == 10) { printf( "NOV" ); }
	  else if (i == 11) { printf( "DEC" ); }
      for (j = 0; j < 31; j++ ) {
         printf( "  %c", calbuffer->yearly_table.month[i].days[j] );
      }
	  printf( "\n" );  /* to wrap each line in the loop, don't move down again */
   }
   if (calbuffer->observe_holidays == 'Y') {
      printf( "Uses holiday calendar %s\n\n", calbuffer->holiday_calendar_to_use );
   }
   else {
      printf( "Does not use a holiday calendar override\n\n" );
   }
} /* DEBUG_CALENDAR_dump_to_stdout */

/* ---------------------------------------------------
 * Fill in a calendar record based on day numbers.
 *
 * daylist is a string of 7 characters, representing days from Sun thru Sat,
 * each entry in the list set to Y will have the day set.
 * calbuffer is the calendar record being updated. The year is going to
 * be extracted from that.
   --------------------------------------------------- */
int CALENDAR_UTILS_populate_calendar_by_day( char * daylist, calendar_def * calbuffer ) {
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
      printf( "*E* Calendar set by days rejected, either no days or year < current year, days %d, year %d (%s)!\n",
                days_requested, (year_to_use + 1900), calbuffer->year );
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
} /* CALENDAR_UTILS_populate_calendar_by_day */
