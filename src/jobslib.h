#ifndef JOBSLIB_DEFS
#define JOBSLIB_DEFS

/*
    JOBSLIB.H ( header definition file )

    The scheduler component that manages the job database.

    Scheduler is by Mark Dickinson, 2001
*/


#include <stdio.h>
#include <time.h>
#include "api.h"
#include "scheduler.h"

/* -------------------------------------------------
   Files used.
   ------------------------------------------------- */
#ifndef GLOBAL_DATA
   extern char job_file[];
   extern FILE *job_handle;
#endif

int  JOBS_Initialise( void );
void JOBS_Terminate( void );
void JOBS_set_jobname( char * strptr );
long JOBS_read_record( jobsfile_def * datarec );
long JOBS_write_record( jobsfile_def * datarec, long recpos );
short int JOBS_delete_record( jobsfile_def * datarec );
long JOBS_format_for_display( jobsfile_def * datarec, char * outbuf, int bufsize );
void JOBS_format_for_display_API( API_Header_Def * api_bufptr );
void JOBS_format_listall_for_display_API( API_Header_Def * api_bufptr, FILE *tx );
int  JOBS_schedule_on( jobsfile_def * datarec );
void JOBS_newday_scheduled( time_t time_number );
void JOBS_newday_forced( time_t rundate );
int JOBS_schedule_date_check( jobsfile_def *local_rec, time_t rundate );
int JOBS_schedule_calendar_check( jobsfile_def *local_rec, time_t rundate );
void JOBS_DEBUG_dump_jobrec( jobsfile_def * job_bufptr );
int JOBS_completed_job_time_adjustments( active_queue_def * datarec );
int JOBS_find_next_run_day( jobsfile_def * job_rec, time_t time_number );
long JOBS_count_entries_using_calendar( char * calname );
void JOBS_recheck_job_calendars( calendar_def * calname );
void JOBS_recheck_every_job_calendar( void );
#endif
