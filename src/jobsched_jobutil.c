/*
    JOBSCHED_JOBUTIL.C

	Tool to work with the job database file. Can do the following operations...
	[ see help screen ]

    Scheduler is by Mark Dickinson, 2001,2002,2003,2004
*/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include "scheduler.h"

#define MAX_CHAR_LEN 254
#define MAX_DIR_LEN (MAX_CHAR_LEN - 15)

/* Global needed to use the utils library */
FILE *msg_log_handle = NULL;

/* #define DEBUG */

/* ---------------------------------------------------
 * show_help_screen:
 * called for any invalid input to show the correct
 * syntax required for the program.
 * --------------------------------------------------- */
void show_help_screen() {
   printf( "\n \nSyntax: jobsched_jobutil -d <directory> -c <command>\n" );
   printf( "\nwhere\n \n" );
   printf( "<directory> is the directory the job scheduler\n" );
   printf( "database files reside in\n \n" );
   printf( "<command> is one of...\n" );
   printf( "  execreport - show jobs in next execution order, scheduler may be active\n" );
   printf( "  namereport - show jobs in jobname order, scheduler may be active\n" );
   printf( "  deletelist - show deleted jobs, scheduler may be active\n" );
   printf( "  sortdbs    - sort the job database file. SCHEDULER MUST BE SHUTDOWN !\n" );
   printf( "  resyncdbs  - resync job dates from current date. SCHEDULER MUST BE SHUTDOWN !\n" );
   printf( "\nExample: jobsched_jobutil -d /opt/dickinson/job_scheduler -c execreport\n \n" );
} /* show_help_screen */

/* ---------------------------------------------------
 * ensure_file_does_not_exist:
 * Called if we must do an internal sort to ensure that
 * no other invokation of the program has already started
 * and created the work file.
 * If the work file exists write an error and abort out
 * of the program immediately.
 * --------------------------------------------------- */
void ensure_file_does_not_exist( char * filename ) {
   struct stat * stat_buf;
   int lasterror, record_check;
   long size_check;
   jobsfile_def jobrec;

   if ( (stat_buf = malloc(sizeof(struct stat))) == NULL ) {
      printf( "ERROR: Insufficient free memory, cannot allocate %d bytes\n", sizeof(struct stat) );
      exit(1);
   }
   lasterror = stat( filename, stat_buf );   /* if <> 0 an error, ENOENT if not there */
   if (lasterror != ENOENT) {
      free( stat_buf );
      printf( "ERROR: workfile %s exists, another jobsched_jobutil may be running.\n", filename );
	  exit( 1 );
   }
   /* Else it does not exist so all OK */
   free( stat_buf );
} /* ensure_file_does_not_exist */

/* ---------------------------------------------------
 * check_job_file:
 *
 * check that the job file exists and appears to be
 * a legal job database file.
 *
 * returns: 0 - errors, n records in file
 *
 * Note: ctime puts in a linefeed so we don't need to
 *       include a newline on those lines.
 * --------------------------------------------------- */
int check_job_file( char * filename ) {
   struct stat * stat_buf;
   int lasterror, record_check;
   long size_check;
   jobsfile_def jobrec;

   if ( (stat_buf = malloc(sizeof(struct stat))) == NULL ) {
      printf( "# ------> E R R O R <----- Insufficient free memory, cannot allocate %d bytes\n", sizeof(struct stat) );
      return(0);
   }
   lasterror = stat( filename, stat_buf );   /* if <> 0 an error, ENOENT if not there */
   if (lasterror == ENOENT) {
      free( stat_buf );
      printf( "# ------> E R R O R <----- file %s does not exist, nothing to do.\n", filename );
	  return( 0 );
   }
   else if (lasterror != 0) {
      perror( "Unable to stat file" );
      free( stat_buf );
      printf( "# ------> E R R O R <----- %s cannot be accessed !\n", filename );
      return( 0 );
   }

   /* a quick sanity check, is the filesize correct for our record length */
   record_check = (int)(stat_buf->st_size / sizeof(jobrec));
   size_check = record_check * sizeof(jobrec);
   if (size_check != (long)stat_buf->st_size) {
      free( stat_buf );
      printf( "# ------> E R R O R <----- %s does not appear to be a job database file (bad record length)\n", filename );
      return( 0 );
   }

/*   printf( "#  File last modified : %s", ctime(&stat_buf->st_mtime) ); */
   free( stat_buf );

   /* file exists, so assume all OK */
   if (record_check == 0) {
      printf( "# ------> WARNING <----- %s is an empty file, nothing to do.\n", filename );
   }
   return( record_check ); 
} /* check_job_file */


/* ---------------------------------------------------
 * produce_execorder_report:
 * Produce a listing of all jobs in the job databse
 * in the order they will execute.
   --------------------------------------------------- */
void produce_execorder_report( char *jobsfile, int numrecs ) {
   int items_read, i, j, recs_buffered, checkrecs;
   FILE *job_handle = NULL;
   jobsfile_def * jobbuffer;
   char * membuffer, *ptr1, *ptr2;

   /* for internal sorting if possible */
   char workbuf_dataline[JOB_NAME_LEN + JOB_DATETIME_LEN + 2 + 1]; /* +1 for null in display buffer */
   char *memory_array;
#ifdef DEBUG
   char test1[JOB_DATETIME_LEN+1], test2[JOB_DATETIME_LEN+1];
   char *ptr3;
#endif

   /* for external sorting if required */
   int external_sort;
   char workfile_name[] = "/tmp/jobsched_jobutil_sort.tmp";
   FILE *work_handle;

#ifdef DEBUG
printf( "DEBUG=produce_report: processing %d records\n", numrecs );
#endif

   /* Allocate memory for the jobsfile records */
   if ( (jobbuffer = malloc(sizeof(jobsfile_def))) == NULL) { 
      printf(" *ERR: Unable to allocate %d bytes of memory\n", sizeof(jobsfile_def) );
      return;
   }

   /* Open the jobs file we are to read from */
   if ( (job_handle = fopen( jobsfile, "rb" )) == NULL ) {
	  perror( "jobsfile" );
      printf( "*ERR: Unable to open job database file %s for job dumping, err=%d\n",
              jobsfile, errno );
	  free( jobbuffer );
	  return;
   }

#ifdef DEBUG
printf( "DEBUG=produce_report: starting jobsfile scan\n" );
#endif
   /* Scan the job file for possible invalid records now, this keeps
	* the checks out of our main processing loop. It doesn't make the code
	* any faster (in fact slower) but it's a lot more manageable. */
   checkrecs = 0;
   clearerr( job_handle );
   fseek( job_handle, 0, SEEK_SET );
   while ( (!(ferror(job_handle))) && (!(feof(job_handle))) ) {
      items_read = fread( jobbuffer, sizeof(jobsfile_def), 1, job_handle );
	  if (items_read > 0) {
         checkrecs++;
         if ( (jobbuffer->job_header.info_flag != 'A') &&
              (jobbuffer->job_header.info_flag != 'D') &&
              (jobbuffer->job_header.info_flag != 'S') ) {
            free( jobbuffer );
            fclose( job_handle );
			printf( "------> E R R O R <----- file %s does not appear to be a legal job data file (bad data in record)\n", jobsfile );
			return;
		 }
	  }
   } /* while */

   /* As we did a full file scan above and could count the records see
	* how they map out against what we calculated earlier. */
   if (checkrecs != numrecs) {
      free( jobbuffer );
      fclose( job_handle );
      printf( "ERROR: file %s has %d recs, expected %d\n", jobsfile, checkrecs, numrecs );
      return;
   }

#ifdef DEBUG
printf( "DEBUG=produce_report: starting memory_array_malloc\n" );
#endif
   /* Allocate memory for our sorting if possible, else a work file */
   external_sort = 0;  /* false by default */
   if ( (memory_array = malloc( (JOB_NAME_LEN + JOB_DATETIME_LEN + 2) * (numrecs + 2) )) == NULL ) {
      external_sort = 1;  /* must use external sort */
      /* TODO - remove this abort when external sorting is coded */
      printf("ERROR: Insufficient free memory to buffer entire file keylist in memory\n" );
      printf("       This is an error as using external sorting has not yet been implemented.\n" );
      printf("       You will not be able to use the utiliy in this release.\n" );
      return;
   }
   /* If we are forced to use an external sort create the work file */
   if (external_sort == 1) {
      ensure_file_does_not_exist( workfile_name );
      /* TODO create and open file */
   }
#ifdef DEBUG
printf( "DEBUG=produce_report: ended malloc attempt, external_sort=%d\n", external_sort );
#endif

   ptr1 = memory_array;       /* memory address if we got it */

#ifdef DEBUG
printf( "DEBUG=produce_report: starting buffering records\n" );
#endif
   /* read the file, storing in memory or our work file */
   recs_buffered = 0;
   clearerr( job_handle );
   fseek( job_handle, 0, SEEK_SET );
   while ( (!(ferror(job_handle))) && (!(feof(job_handle))) ) {
      items_read = fread( jobbuffer, sizeof(jobsfile_def), 1, job_handle );
	  if ( (items_read > 0) && (jobbuffer->job_header.info_flag != 'D') ) {
         /* Sorted by next rundate for this report */
         ptr2 = (char *)&workbuf_dataline;
         memcpy( ptr2, jobbuffer->next_scheduled_runtime, JOB_DATETIME_LEN );
         ptr2 = ptr2 + JOB_DATETIME_LEN;
         *ptr2 = ' ';
         ptr2++;
         memcpy( ptr2, jobbuffer->job_header.jobname, JOB_NAME_LEN );
         ptr2 = ptr2 + JOB_NAME_LEN;
         *ptr2 = '\n';
         if (external_sort == 0) {
            /* store the record in the memory buffer */
            memcpy( ptr1, workbuf_dataline, (JOB_NAME_LEN + JOB_DATETIME_LEN + 2) );
            ptr1 = ptr1 + (JOB_NAME_LEN + JOB_DATETIME_LEN + 2);
            recs_buffered++;
         } else {
            /* TODO write the record to the work file */
         }
	  } /* if not a deleted entry */
   } /* while no errors and not eof on input */

#ifdef DEBUG
printf( "DEBUG=produce_report: starting data sort\n" );
#endif
   /* Sort the data we have now */
   if (external_sort == 0) {   /* internal bubble sort */
      ptr1 = (char *)memory_array;
/*      while (position_changed > 0) { replaced be below two lines */
	  for (j = 0; j < (recs_buffered - 1); j++) {
         ptr2 = ptr1 + (JOB_NAME_LEN + JOB_DATETIME_LEN + 2);
         for (i = j; i < (recs_buffered - 1); i++ ) {
            if (memcmp(ptr1,ptr2,JOB_DATETIME_LEN) > 0) {
#ifdef DEBUG
memcpy( test1, ptr1, JOB_DATETIME_LEN );
memcpy( test2, ptr2, JOB_DATETIME_LEN );
ptr3 = (char *)&test1;
ptr3 = ptr3 + JOB_DATETIME_LEN;
*ptr3 = '\0';
ptr3 = (char *)&test2;
ptr3 = ptr3 + JOB_DATETIME_LEN;
*ptr3 = '\0';
printf( "DEBUG=produce_report: %s > %s, switching at pass %d,%d\n", test1, test2, j, i );
#endif
               memcpy( workbuf_dataline, ptr1, (JOB_NAME_LEN + JOB_DATETIME_LEN + 2) );
               memcpy( ptr1, ptr2, (JOB_NAME_LEN + JOB_DATETIME_LEN + 2) );
               memcpy( ptr2, workbuf_dataline, (JOB_NAME_LEN + JOB_DATETIME_LEN + 2) );
            } /* if */
            ptr2 = ptr2 + (JOB_NAME_LEN + JOB_DATETIME_LEN + 2);
         } /* for */
         ptr1 = ptr1 + (JOB_NAME_LEN + JOB_DATETIME_LEN + 2);
      } /* while */
   } else {                    /* external sort */
      /* TODO - the external sort stuff */
      /* close work file */
      /* invoke external sort program, in place sort */
      /* check RC of sort to ensure OK */
      /* open work file */
   }

#ifdef DEBUG
printf( "DEBUG=produce_report: starting report output\n" );
#endif
   /* Now display the sorted results */
   printf( "\n \nJobs in execution order\n" );
   ptr1 = memory_array;
   for (i = 0; i < recs_buffered; i++) {
      memcpy( workbuf_dataline, ptr1, (JOB_NAME_LEN + JOB_DATETIME_LEN + 2) );
      workbuf_dataline[JOB_NAME_LEN + JOB_DATETIME_LEN + 2] = '\0';
      printf( "%2.0d. %s", (i + 1), workbuf_dataline );
      ptr1 = ptr1 + (JOB_NAME_LEN + JOB_DATETIME_LEN + 2);
   } /* for */
   printf( "\n%d jobs defined\n", recs_buffered );

#ifdef DEBUG
printf( "DEBUG=produce_report: cleaning up\n" );
#endif
   /* free the memory we have allocated */
   if (external_sort == 0) {
      free(memory_array);
   } else {
      /* close work file */
   }
   free( jobbuffer );
   /* close the file we opened */
   fclose( job_handle );
   /* All done */
   return;
} /* produce_execorder_report */

/* ---------------------------------------------------
 * sort_the_job_datafile:
 * The SCHEDULER MUST BE STOPPED so we have to prompt
 * the user to ensure they know what they are doing.
 * This will be run infrequently as part of database
 * maintenance to sort the jobs into name order so
 * they are easier to locate when displaying jobs (as
 * the scheduler itself doesn't do any sorting as that
 * would slow it down too much).
   --------------------------------------------------- */
void sort_the_job_datafile( char *jobsfile, int numrecs ) {
   printf("ERROR: The sortdbs function has not been implemented in this version.\n" );
   printf("       Refer to the manuals, which state it is not yet implemented !.\n" );
   printf("       You should refer to the manuals before trying to run any of\n" );
   printf("       the potentially damaging (to the application) utilities provided.\n" );
   return;
} /* sort_the_job_datafile */

/* ---------------------------------------------------
 * resync_the_job_datafile:
 * The SCHEDULER MUST BE STOPPED !
 * Ocassionally I leave the scheduler shutdown for long
 * periods, it is a pain when I start it and it tries to
 * 'catch up' a months worth of jobs. I need a function
 * that will go through the job file and update all dates
 * to be relevant to the current date. I havn't coded
 * the function yet though.
 * Current workaround is to manually dump the database
 * using jobsched_take_snapshot then edit the output,
 * delete the job dbs and reload the output. A bit too
 * manual for me.
   --------------------------------------------------- */
void resync_the_job_datafile( char *jobsfile, int numrecs ) {
   printf("ERROR: The resyncdbs function has not been implemented in this version.\n" );
   printf("       Refer to the manuals, which state it is not yet implemented !.\n" );
   printf("       You should refer to the manuals before trying to run any of\n" );
   printf("       the potentially damaging (to the application) utilities provided.\n" );
   return;
} /* resync_the_job_datafile */

/* ---------------------------------------------------
 * produce_deleted_jobs_report:
 * Show all the records in the job database file that
 * are flagged as deleted entries. There should be
 * very few as these should be deleted each day by the
 * scheduler newday job (it did help identify a bug so
 * I run it occasionally).
   --------------------------------------------------- */
void produce_deleted_jobs_report( char *jobsfile ) {
   int items_read, deleted_count;
   FILE *job_handle = NULL;
   jobsfile_def * jobbuffer;
   char *ptr1;

   /* for internal sorting if possible */
   char workbuf_dataline[JOB_NAME_LEN + 2]; /* +2 for null in display buffer */

   /* Allocate memory for the jobsfile records */
   if ( (jobbuffer = malloc(sizeof(jobsfile_def))) == NULL) { 
      printf(" ERROR: Unable to allocate %d bytes of memory\n", sizeof(jobsfile_def) );
      return;
   }

   /* Open the jobs file we are to read from */
   if ( (job_handle = fopen( jobsfile, "rb" )) == NULL ) {
	  perror( "jobsfile" );
      printf( "*ERR: Unable to open job database file %s for job dumping, err=%d\n",
              jobsfile, errno );
	  free( jobbuffer );
	  return;
   }

   printf( "\n \nRecently deleted jobs...\n \n" );
   deleted_count = 0;
   clearerr( job_handle );
   fseek( job_handle, 0, SEEK_SET );
   while ( (!(ferror(job_handle))) && (!(feof(job_handle))) ) {
      items_read = fread( jobbuffer, sizeof(jobsfile_def), 1, job_handle );
	  if ( (items_read > 0) && (jobbuffer->job_header.info_flag == 'D') ) {
         memcpy( workbuf_dataline, jobbuffer->job_header.jobname, JOB_NAME_LEN );
         ptr1 = (char *)&workbuf_dataline;
		 ptr1 = ptr1 + JOB_NAME_LEN;
		 *ptr1 = '\0';
		 printf( "%s\n", workbuf_dataline );
		 deleted_count++;
	  }
   } /* while */

   printf( "\nThere are %d entries that should be removed in the next newday\n \n", deleted_count );

   fclose( job_handle );
   free( jobbuffer );
   return;
} /* produce_deleted_jobs_report */

/* ---------------------------------------------------
 * produce_jobname_report:
 * Produce a listing of all jobs in the job databse
 * sorted by name.
   --------------------------------------------------- */
void produce_jobname_report( char *jobsfile, int numrecs ) {
   int items_read, i, j, recs_buffered, checkrecs;
   FILE *job_handle = NULL;
   jobsfile_def * jobbuffer;
   char * membuffer, *ptr1, *ptr2;

   /* for internal sorting if possible */
   char workbuf_dataline[JOB_NAME_LEN + 2];
   char *memory_array;

   /* for external sorting if required */
   int external_sort;
   char workfile_name[] = "/tmp/jobsched_jobutil_sort.tmp";
   FILE *work_handle;

   /* Allocate memory for the jobsfile records */
   if ( (jobbuffer = malloc(sizeof(jobsfile_def))) == NULL) { 
      printf(" *ERR: Unable to allocate %d bytes of memory\n", sizeof(jobsfile_def) );
      return;
   }

   /* Open the jobs file we are to read from */
   if ( (job_handle = fopen( jobsfile, "rb" )) == NULL ) {
	  perror( "jobsfile" );
      printf( "*ERR: Unable to open job database file %s for job dumping, err=%d\n",
              jobsfile, errno );
	  free( jobbuffer );
	  return;
   }

   /* Scan the job file for possible invalid records now, this keeps
	* the checks out of our main processing loop. It doesn't make the code
	* any faster (in fact slower) but it's a lot more manageable. */
   checkrecs = 0;
   clearerr( job_handle );
   fseek( job_handle, 0, SEEK_SET );
   while ( (!(ferror(job_handle))) && (!(feof(job_handle))) ) {
      items_read = fread( jobbuffer, sizeof(jobsfile_def), 1, job_handle );
	  if (items_read > 0) {
         checkrecs++;
         if ( (jobbuffer->job_header.info_flag != 'A') &&
              (jobbuffer->job_header.info_flag != 'D') &&
              (jobbuffer->job_header.info_flag != 'S') ) {
            free( jobbuffer );
            fclose( job_handle );
			printf( "------> E R R O R <----- file %s does not appear to be a legal job data file (bad data in record)\n", jobsfile );
			return;
		 }
	  }
   } /* while */

   /* Allocate memory for our sorting if possible, else a work file */
   external_sort = 0;  /* false by default */
   if ( (memory_array = malloc( (JOB_NAME_LEN + 2) * (numrecs + 2) )) == NULL ) {
      external_sort = 1;  /* must use external sort */
      /* TODO - remove this abort when external sorting is coded */
      printf("ERROR: Insufficient free memory to buffer entire file keylist in memory\n" );
      printf("       This is an error as using external sorting has not yet been implemented.\n" );
      printf("       You will not be able to use the utiliy in this release.\n" );
      return;
   }
   /* If we are forced to use an external sort create the work file */
   if (external_sort == 1) {
      ensure_file_does_not_exist( workfile_name );
      /* TODO create and open file */
   }

   ptr1 = memory_array;       /* memory address if we got it */

   /* read the file, storing in memory or our work file */
   recs_buffered = 0;
   clearerr( job_handle );
   fseek( job_handle, 0, SEEK_SET );
   while ( (!(ferror(job_handle))) && (!(feof(job_handle))) ) {
      items_read = fread( jobbuffer, sizeof(jobsfile_def), 1, job_handle );
	  if ( (items_read > 0) && (jobbuffer->job_header.info_flag != 'D') ) {
         if (external_sort == 0) {
            /* store the record in the memory buffer */
            memcpy( ptr1, jobbuffer->job_header.jobname, JOB_NAME_LEN );
            ptr1 = ptr1 + (JOB_NAME_LEN + 2);
            recs_buffered++;
         } else {
            /* TODO write the record to the work file */
         }
	  } /* if not a deleted entry */
   } /* while no errors and not eof on input */

   /* Sort the data we have now */
   if (external_sort == 0) {   /* internal bubble sort */
      ptr1 = (char *)memory_array;
	  for (j = 0; j < (recs_buffered - 1); j++) {
         ptr2 = ptr1 + (JOB_NAME_LEN + 2);
         for (i = j; i < (recs_buffered - 1); i++ ) {
            if (memcmp(ptr1,ptr2,JOB_NAME_LEN) > 0) {
               memcpy( workbuf_dataline, ptr1, JOB_NAME_LEN );
               memcpy( ptr1, ptr2, JOB_NAME_LEN );
               memcpy( ptr2, workbuf_dataline, JOB_NAME_LEN );
            } /* if */
            ptr2 = ptr2 + (JOB_NAME_LEN + 2);
         } /* for */
         ptr1 = ptr1 + (JOB_NAME_LEN + 2);
      } /* while */
   } else {                    /* external sort */
      /* TODO - the external sort stuff */
      /* close work file */
      /* invoke external sort program, in place sort */
      /* check RC of sort to ensure OK */
      /* open work file */
   }

   /* Now display the sorted results */
   printf( "\n \nJobs in name order\n" );
   ptr1 = memory_array;
   for (i = 0; i < recs_buffered; i++) {
	  ptr2 = ptr1 + JOB_NAME_LEN;
      *ptr2 = '\0';
      printf( "%2.0d. %s\n", (i + 1), ptr1 );
      ptr1 = ptr1 + (JOB_NAME_LEN + 2);
   } /* for */
   printf( "\n%d jobs defined\n", recs_buffered );

   /* free the memory we have allocated */
   if (external_sort == 0) {
      free(memory_array);
   } else {
      /* close work file */
   }
   free( jobbuffer );
   /* close the file we opened */
   fclose( job_handle );
   /* All done */
   return;
} /* produce_jobname_report */

/* ---------------------------------------------------
 * M A I N L I N E
 * Check the parms are OK
 * Check the job database file indicated is OK
 * Run the report/function required
   --------------------------------------------------- */
int main(int argc, char **argv) {
   char directory_name[MAX_DIR_LEN+1];
   char job_database_name[MAX_CHAR_LEN+1];
   int utility_command = 0; /* 1 = report, 2 = sort */
   int  i, num_records;
   char *ptr;

   /* Must have the parms we need */
   if (argc < 2) {
      show_help_screen();
      exit (1);
   }

   /* This is also used by utils */
   msg_log_handle = stdout;

   /* Get all the parms from the command line */
   utility_command = 0;   /* ensure it is 0 (not set) */
   strcpy( job_database_name, "" );   /* ensure it is not set */
   i = 1;
   while (i < argc) {
      if (memcmp(argv[i], "-d", 2) == 0) {      /* directory flag */
         i++;
         /* Get the directory, and ensure it has a trailing / on it */
         strncpy( directory_name, argv[i], MAX_DIR_LEN );
         ptr = (char *)&directory_name;
         ptr = ptr + (strlen(directory_name) - 1);
         if (*ptr != '/') {
            strcat( directory_name, "/" );
         }
         snprintf( job_database_name, MAX_CHAR_LEN, "%sjob_details.dbs", directory_name );
         i++;
      } else if (memcmp(argv[i], "-c", 2) == 0) { /* command flag */
         i++;
         /* What was the command */
         if (memcmp(argv[i], "execreport", 10) == 0) {        /* run exectime sorted report */
             utility_command = 1;
         } else if (memcmp(argv[i], "sortdbs", 7) == 0) {  /* sort database */
             utility_command = 2;
         } else if (memcmp(argv[i], "deletelist", 10) == 0) {  /* show recs pending delete */
             utility_command = 3;
         } else if (memcmp(argv[i], "namereport", 10) == 0) {  /* run jobname sorted report */
             utility_command = 4;
         } else if (memcmp(argv[i], "resyncdbs", 9) == 0) {  /* resync times in job dbs */
             utility_command = 5;
         } else {                                        /* illegal cmd, show help */
            show_help_screen();
            exit (1);
         }
         i++;
      } else {                                  /* illegal flag, show help */
         show_help_screen();
         exit (1);
      }
   }


   /* we must have a command and a job database name */
   if ((utility_command == 0) || (strlen(job_database_name) == 0)) {
      show_help_screen();
      exit (1);
   }

   /* The job file must be legal */
   if ((num_records = check_job_file( job_database_name )) == 0) {
      exit (1);
   }

   /* Execute the required commands */
   if (utility_command == 1) { produce_execorder_report( (char *)&job_database_name, num_records ); }
   else if (utility_command == 2) { sort_the_job_datafile( (char *)&job_database_name, num_records ); }
   else if (utility_command == 3) { produce_deleted_jobs_report( (char *)&job_database_name ); }
   else if (utility_command == 4) { produce_jobname_report( (char *)&job_database_name, num_records ); }
   else if (utility_command == 5) { resync_the_job_datafile( (char *)&job_database_name, num_records ); }
   else { show_help_screen(); }
   exit( 0 );
} /* end main */
