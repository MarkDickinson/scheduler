/*
 * daysago : C
 * 
 * When passed a date in YYYYMMDD format it will return how many
 * days in the past from the current date the provided date is.
 * If the date is the current date or in the future it will
 * return 0.
 * The output is written to stdout for capture by scripts.
 *
 * Parameters accepted are...
 *    YYYYMMDD - the date to be compared against the current date
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>     /* in GCC 4.1.1+ strncpy and strcpy are defined in here */

#define MAX_DATE_LEN 8

/* This is the tm time structure defined in time.h
 * documented here for clarity...
 * DO NOT RELY ON IT as this changes between versions
 * of operating systems.
struct  tm {     see ctime(3) 
        int     tm_sec;       seconds
        int     tm_min;       minutes
        int     tm_hour;      hour
        int     tm_mday;      day
        int     tm_mon;       month 
        int     tm_year;      year
        int     tm_wday;      week day number
        int     tm_yday;      forgot ?
        int     tm_isdst;     is dst being used
};
*/

#define MAX_MASK_LEN 20
#define MAX_PARM_LEN 20

/* -------------------------------------------------------------
 * show_syntax
 * This procedure is called if any of the input parameters are
 * in error, or if the user entered no paremeters.
 * It displays the parameters expected.
 * ------------------------------------------------------------- */
void show_syntax( void ) {
   printf( "\n" );
   printf( "The purpose of this program is to return to stdout the number\n" );
   printf( "of days in the past a provided date of YYYYMMDD occurred.\n" );
   printf( "\n" );
   printf( "Syntax: \n" );
   printf( "   daysago YYYYMMDD\n" );
   printf( "Example:run on 20020722\n" );
   printf( "   daysago 20020721\n" );
   printf( "      [ returns the number 1 ]\n" );
   printf( "\n" );
} /* show_syntax */

/* -------------------------------------------------------------
 * make_time
 * This procedure  makes a timestamp based on the datetime
 * value passed to it.
 * Date is expected to be YYYYMMDD, we don't check.
 * ------------------------------------------------------------- */
time_t make_time( char * timestr ) {
   char * ptr1;
   char smallbuf[20];
   time_t time_number;
   struct tm *time_var;

   time_var = malloc(sizeof *time_var);
   if (time_var == NULL) {
      printf( "*ERR: Unable to allocate %d bytes of memory for time structure\n", (sizeof *time_var) );
      exit(1);  /* not possible */
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

   time_var->tm_hour = 3;  /* use 3, don't need to worry about dst flag then */
   time_var->tm_min = 0;
   time_var->tm_sec = 0;

   /* Adjust the times we provided to what the structure expects */
   time_var->tm_year = time_var->tm_year - 1900;  /* years returned as count from 1900 */
   time_var->tm_mon = time_var->tm_mon - 1;       /* months returned as 0-11 */

   /* set these to 0 for now */
   time_var->tm_wday = 0;   /* week day */
   time_var->tm_yday = 0;   /* day in year */
   time_var->tm_isdst = 0;  /* ignore, we use hour 3 rather than 0 */

   /* and workout the time */
   time_number = mktime( time_var );

   free( time_var );
   return( time_number );
} /* make_time */

/* -------------------------------------------------------------
 * valid_date
 * check that the string passed is a valid YYYYMMDD string.
 * In a seperate proc as we may wish to add additional formats
 * later and this proc can then build them into the correct
 * format.
 * ------------------------------------------------------------- */
int valid_date( char * strtotest ) {
   char *ptr;
   int datalen, i, errorcount;
   datalen = strlen(strtotest);
   if (datalen == 8) {  /* OK so far */
      ptr = strtotest;
      errorcount = 0;
      for (i = 0; i < datalen; i++ ) {
         if ( (*ptr < '0') || (*ptr > '9') ) {
            errorcount++;
         }
         ptr++;
      } /* for */
      if (errorcount == 0) {
         return( 1 ); /* looks OK */
      }
      else {
         return( 0 );  /* not numeric */
      }
   } /* if len = 8 */
   else {
      return( 0 ); /* not the correct length */
   }
} /* valid_date */

/* -------------------------------------------------------------
 * MAIN:
 * Check all parameters passed on the command line, calculate
 * the date to be displayed, and display it.
 * ------------------------------------------------------------- */
int main( int argc, char **argv, char **envp ) {
   char date_to_use[MAX_DATE_LEN+1];
   time_t time_now;
   time_t time_number;
   char *ptr;
   long difference;

   /*
    * Check to see what command line parameters we have
    */
   if (argc < 2) {
      printf( "%s: (c)Mark Dickinson, 2001\n", argv[0] );
      show_syntax();
      exit(1);
   }

   ptr = argv[1];
   strncpy( date_to_use, ptr, MAX_DATE_LEN );
   date_to_use[MAX_DATE_LEN] = '\0';
   if (valid_date((char *)&date_to_use) == 1) {
      time_now = ( time(0) / (60 * 60 * 24) );
      time_number = ( make_time( (char *)date_to_use ) / (60 * 60 * 24) );
      difference = (time_now - time_number);
      if (difference < 0) { difference = 0; }
      printf( "%d\n", difference );
      exit( 0 );  /* all OK */
   }
   else {
     printf( "\n\n%s is not a valid YYYYMMDD date\n\n", date_to_use );
     show_syntax();
     exit( 1 );  /* Error in input */
   }

   exit( 0 );
} /* end main */
