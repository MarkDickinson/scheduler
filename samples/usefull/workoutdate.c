/*
 * workoutdate : C
 * 
 * This program will determine the date nn days in the past or future 
 * depending on the parameters provided to it.
 * The format the date is returned in defaults to YYYYMMDD unless
 * overridden using the -format option.
 * 
 * Parameters accepted are...
 * -daysback nn
 * -daysforward nn
 * -hoursback nn
 * -hoursforward nn
 * -format YYYYMMDD | YYMMDD | YYYYMMDDHHMMSS | HHMMSS | HHMM
 * -workingdate YYYYMMDDHHMMSS
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <string.h>     /* in GCC 4.1.1+ strncpy and strcpy are in here */

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
 * This procedure is called if any of the input parameters are
 * in error, or if the user entered no paremeters.
 * It displays the parameters expected.
 * ------------------------------------------------------------- */
void show_syntax( void ) {
   printf( "\n" );
   printf( "Parameters available are\n" );
   printf( "  -daysback nn\n" );
   printf( "  -daysforward nn\n" );
   printf( "  -hoursback nn\n" );
   printf( "  -hoursforward nn\n" );
   printf( "  -format YYYYMMDD | YYMMDD | YYYYMMDDHHMM | HHMM | CTIME\n" );
   printf( "  -workingdate YYYYMMDDHHMM  (only use if you don't want calculations from time now)\n" );
   printf( "\nIf a -format is not provided the default is YYYYMMDD\n" );
   printf( "If a -workingdate is not provided the current date and time will be used\n" );
   printf( "The -format CTIME displays the time as per the ctime function on your machine\n" );
   printf( "\nParameters may be mixed, ie: -daysback 5 -daysforward 2 is legal.\n" );
   printf( "\nThe -workingdate parameter was added in case you wish calculations to\n");
   printf( "be done from a start date other than the current time. NOTE: it was\n");
   printf( "added as an afterthought, no sanity checking has been implemented for\n");
   printf( "the -workingdate value.\n");
   printf( "\n" );
} /* show_syntax */

/* -------------------------------------------------------------
 * This procedure  makes a timestamp based on the datetime
 * value passed to it. Called if the user wants to override
 * the default start time for time adjustments.
 * Date is expected to be YYYYMMDDHHMMSS, we don't check.
 * ------------------------------------------------------------- */
time_t make_time( char * timestr, int dst_flag ) {
   char * ptr1;
   char smallbuf[20];
   time_t time_number;
   struct tm *time_var;

   time_var = malloc(sizeof *time_var);
   if (time_var == NULL) {
      printf( "*ERR: Unable to allocate %d bytes of memory for time structure\n", (sizeof *time_var) );
      exit( 1 );  /* not possible */
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
   ptr1 = ptr1 + 2;
   strncpy(smallbuf,ptr1,2);
   smallbuf[2] = '\0';
   time_var->tm_hour = atoi( smallbuf );
   ptr1 = ptr1 + 2;
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
   time_var->tm_isdst = dst_flag;  /* use daylight savings ? */

   /* and workout the time */
   time_number = mktime( time_var );

   free( time_var );
   return( time_number );
} /* make_time */

/* -------------------------------------------------------------
 * This procedure is called to check parameters that are
 * expected to have a numeric value.
 * It will return the value the time needs to be adjusted
 * for as appropriate for the parameter passed.
 * ------------------------------------------------------------- */
signed long check_parameter( char *parm, char *value ) {
   long worknum;
   long one_day;
   worknum = strtol(value, (char **)NULL, 10);
   if (worknum == 0) {
      printf( "Invalid value for parameter %s : %s\n", parm, value );
      exit( 1 );
   }

   one_day = 60 * 60 * 24;    /* seconds * minutes * hours in a day */

   if (strncmp("-daysback",parm,9) == 0) {
      return( (60 * 60 * 24 * worknum) * -1 );
   }
   else if (strncmp("-daysforward",parm,12) == 0) {
      return( 60 * 60 * 24 * worknum );
   }
   else if (strncmp("-hoursback",parm,10) == 0) {
      return( (60 * 60 * worknum) * -1 );
   }
   else if (strncmp("-hoursforward",parm,13) == 0) {
      return( 60 * 60 * worknum );
   }
   else {
      printf( "Flag %s is not a valid -format option\n", parm );
	  show_syntax();
	  exit( 1 );
   }
} /* check_parameter */

/* -------------------------------------------------------------
 * This procedure is called to check that the -format value
 * provided by the caller is a legal mask.
 * ------------------------------------------------------------- */
void validate_format( char *mask, char *savebuffer ) {
   /* Added an additional check of formatlen to first space as
    * things like YYYYMMDDHHMMSS were being OKed in YYYYMMDDHHMM
    * check etc, and we want to advise its an error */

   char *ptr;
   int  formatlen;

   ptr = mask;
   formatlen = 0;
   while ( (*ptr != ' ') && (*ptr != '\n') && (*ptr != '\0') ) {
      ptr++;
      formatlen++;
   }

   if ( ( (strncmp("YYYYMMDDHHMM",mask,12) == 0) && (formatlen == 12) ) ||
        ( (strncmp("YYYYMMDD",mask,8) == 0) && (formatlen == 8) ) || 
        ( (strncmp("YYMMDD",mask,6) == 0) && (formatlen == 6) ) ||
        ( (strncmp("HHMM",mask,4) == 0)  && (formatlen == 4) ) ||
        ( (strncmp("CTIME",mask,5) == 0)  && (formatlen == 5) )
      ) {
      strncpy( savebuffer, mask, MAX_MASK_LEN );
   }
   else {
      printf( "%s is not a valid format option\n", mask );
	  printf( "Use one of YYYYMMDDHHMM, YYYYMMDD, YYMMDD, HHMM or CTIME\n" );
	  exit( 1 );
   }
} /* validate_format */

/* -------------------------------------------------------------
 * This function prints the final computed time in the format
 * that has been requested.
 * ------------------------------------------------------------- */
void print_time( struct tm *time_def, char *mask ) {
   long worknum;
   time_def->tm_year = time_def->tm_year + 1900;  /* adjust to correct year */
   time_def->tm_mon  = time_def->tm_mon  + 1;     /* adjust, months start at 0 */
   if (strncmp("YYYYMMDDHHMM",mask,12) == 0) {
      printf( "%.4d%.2d%.2d%.2d%.2d\n",
              time_def->tm_year, time_def->tm_mon, time_def->tm_mday,
              time_def->tm_hour, time_def->tm_min );
   }
   else if (strncmp("YYYYMMDD",mask,8) == 0) {
      printf( "%.4d%.2d%.2d\n",
              time_def->tm_year, time_def->tm_mon, time_def->tm_mday );
   }
   else if (strncmp("YYMMDD",mask,6) == 0) {
      worknum = time_def->tm_year - ((time_def->tm_year / 100) * 100);
      printf( "%.2d%.2d%.2d\n",
              worknum, time_def->tm_mon, time_def->tm_mday );
   }
   else if (strncmp("HHMM",mask,4) == 0) {
      printf( "%.2d%.2d\n",
              time_def->tm_hour, time_def->tm_min );
   }
   else {
      printf( "The code for the format %s has not been put in place\n", mask );
	  free( time_def );
      exit( 1 ); 
   }
} /* print_time */

/* -------------------------------------------------------------
 * MAIN:
 * Check all parameters passed on the command line, calculate
 * the date to be displayed, and display it.
 * ------------------------------------------------------------- */
int main( int argc, char **argv, char **envp ) {
   struct tm *time_var;
   time_t time_number;
   signed long time_offset;
   char *ptr;
   int i, date_override;
   char saved_command[MAX_PARM_LEN+1];
   char saved_format[MAX_MASK_LEN+1];
   char saved_startdate_override[13]; /* YYYYMMDDHHMM_ */

   /*
    * Check to see what command line parameters we have
    */
   if (argc < 2) {
      printf( "%s: (c)Mark Dickinson, 2001\n", argv[0] );
      show_syntax();
      exit( 1 );
   }

   time_offset = 0;     /* default, and start point for adjustments */
   date_override = 0;   /* use current system date and time */
   strcpy(saved_format,"YYYYMMDD"); /* default */
   i = 1;
   /* use a while loop instead of a for loop as we may
    * increment the counter ourselves */
   while ( i < argc ) {
      ptr = argv[i];
      i++;
	  if (i >= argc) {
		 printf( "Missing value for %s\n", ptr );
		 exit( 1 );
      }
      strncpy( saved_command, ptr, MAX_PARM_LEN ); 
      ptr = argv[i];
      if (strncmp("-format",saved_command,7) == 0) {
         validate_format( ptr, saved_format );
      }
	  else if (strncmp("-workingdate",saved_command,12) == 0) {
         date_override = 1;
		 strncpy( saved_startdate_override, ptr, 12 ); /* YYYYMMDDHHMM */
      }
      else {
         time_offset = time_offset + check_parameter( saved_command, ptr );
      }
      i++;
   }
    
   /*
    * Work out the new time and print the result.
    */
   if (date_override == 1) {
      /* have to get the dst flag setting for this */
      time_number = time(0);
      time_var = localtime( &time_number );
	  /* then workout the callers passed time */
      time_number = make_time( (char *)&saved_startdate_override, time_var->tm_isdst );
   }
   else {
      time_number = time(0);     /* time now in seconds from 00:00, Jan 1 1970 */
   }
   time_number = time_number + time_offset;
   if (strcmp("CTIME",saved_format) == 0) {
      printf( "%s", ctime( &time_number ) );
   }
   else {
     time_var = localtime( &time_number );   
     print_time( time_var, saved_format ); 
   }
   exit( 0 );
} /* end main */
