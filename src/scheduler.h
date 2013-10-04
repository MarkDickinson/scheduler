#ifndef SCHEDULER_DEFS
#define SCHEDULER_DEFS

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

/*
  Notes: we use char all the way through these definitions. This is 
         because the API calls may go across the TCP-IP connections
         we support which use new-line as end of data request, so
         I don't want to use binary daya anywhere in case it
         contains a newline value anywhere.
*/

#define MAX_DEPENDENCIES 5  /* the maximum number of
                               dependencies each job
                               is allowed to have.
                            */
#define MAX_RUNNING_JOBS 5 /* the maximum number of jobs that can
							  be running at any given time
							*/
#define MAX_RUNNING_ALERTS 10 /* the maximum number of external alerts
							  running at any given time. should never
                              be possible to exceed running jobs but be safe
                              as we may add more server internal alerts later.
							*/


/* Constants used to define data fields...
   these are used by the program modules to control the length of data moves
   so adjust these with caution. Any changes here will also require any existing
   databases to be deleted so the server can create them in the new format.
*/
#define CALENDAR_NAME_LEN 40
#define CALENDAR_DESC_LEN 40

#define JOB_NUMBER_LEN 6
#define JOB_NAME_LEN 30
#define JOB_DATETIME_LEN 17
#define JOB_DEPENDTARGET_LEN 80  /* MUST be larger than JOB_NAME_LEN as this len is used in
									dependecy queue records to store both jobname and filenames */
#define JOB_OWNER_LEN 30
#define JOB_EXECPROG_LEN 80
#define JOB_PARM_LEN 80
#define JOB_CALNAME_LEN CALENDAR_NAME_LEN
#define JOB_LOGMSGID_LEN 10
#define JOB_LOGMSG_LEN 70
#define JOB_STATUSCODE_LEN 3
#define JOB_JOBLOGFILENAME_LEN 80
#define JOB_DESC_LEN 40
#define JOB_REPEATLEN 3
#define JOB_REPEAT_MAXVALUE 999 /* max that fits in the three bytes above */

#define CALENDAR_NAME_LEN 40
#define CALENDAR_DESC_LEN 40

/* -------------------------------------------------
   JOB header record. Common to all records.
   NEVER MOVE jobnumber. It is used by name to address
   the start of the job header buffer for bulk memory
   copies.
   ------------------------------------------------- */
   typedef struct {
      char jobnumber[JOB_NUMBER_LEN+1]; /* jobnumber MUST ALWAYS be first, its used for addressing */
      char jobname[JOB_NAME_LEN+1];
      char info_flag;             /* A=active,D=deleted,S=system */
   } job_header_def;


/* -------------------------------------------------
   Dependency info.
   A standard structure for recoding job dependencies.
   ------------------------------------------------- */
   typedef struct {
      char dependency_type;      /* 0 = no dependency
                                    1 = waiting for a job to complete
                                    2 = waiting for a file to appear
                                 */
      char dependency_name[JOB_DEPENDTARGET_LEN+1];  /* job or filename we are waiting on */
   } dependency_info_def;

/* -------------------------------------------------
  The JOBS file record.
  This record contains detailed information about a
  job.
  This record is used as a reference point
  for all functions performed by the scheduler.
   ------------------------------------------------- */
   typedef struct {
      job_header_def job_header;
      char last_runtime[JOB_DATETIME_LEN+1];           /* yyyymmdd hh:mm:ss  */
      char next_scheduled_runtime[JOB_DATETIME_LEN+1];
      char job_owner[JOB_OWNER_LEN+1];
      char program_to_execute[JOB_EXECPROG_LEN+1];
      char program_parameters[JOB_PARM_LEN+1];
	  char description[JOB_DESC_LEN+1];   /* Job description field */
      dependency_info_def dependencies[MAX_DEPENDENCIES];
      char use_calendar;        /* 0=no, 1=yes, 2=crontype weekdays without calendar, 3=every nn minutes */
      char calendar_name[CALENDAR_NAME_LEN+1];
	  char crontype_weekdays[7]; /* fields 0-6 (0=off, 1=on) of Sun thru Sat if use_calendar=2 */
	  char requeue_mins[JOB_REPEATLEN+1]; /* number of minutes this job repeats at if use_calendar=3 */
	  char job_catchup_allowed; /* if Y job repeats if scheduler catchup set, if off then
                                   it is not a job that is allowed to be 'caught up'
                                   ie: disk cleanups that only need to run once to catchup anyway */
      char calendar_error;      /* Y=calendar error(expired or missing), N=all is OK with calendar */
	  /* Reserve some fields for further expansion */
	  char reserved_1;
	  char reserved_2;
	  char reserved_3;
	  char reserved_4;
	  char reserved_5;
	  char reserved_6;
   } jobsfile_def;


/* -------------------------------------------------
  The JOB log file record.
  This is a standard format for writing to the job
  log file to record job activity.
   ------------------------------------------------- */
   typedef struct {
      job_header_def job_header;
      char datestamp[JOB_DATETIME_LEN+1];
      char msgid[JOB_LOGMSGID_LEN+1]; /* 0000003000=Failed, 0's is OK */
      char text[JOB_LOGMSG_LEN+1];
      char status_code[JOB_STATUSCODE_LEN+1];  /* 000=OK, 003=FAILED */
   } joblog_def;



/* -------------------------------------------------
  The ALERTS file record.
  This record contains information on failed jobs.
  The alerts file should only contain records for
  jobs needing intervention (ie: if a rerun of a
  failed job starts then the alert record will be
  deleted).
   ------------------------------------------------- */
   typedef struct {
      joblog_def job_details;
      char       severity;        /* 0 = ready to be cleared
                                     1 = manual action required
                                     2 = rerun required
                                     3 = page support needed
                                  */
      char       failure_code[JOB_STATUSCODE_LEN+1]; /* as returned by job */
      char       acknowledged;    /* Y if operator acknowledged */
	  char       external_alert_sent;  /* Y=alert sent to external moniter so an ok must be sent
										  to the external monitor when alert is resolved. N=its all being
										  done standalone */
   } alertsfile_def;


/* -------------------------------------------------
  The ACTIVE JOB SCHEDULE file records.
  These records are used to track and manage jobs
  through the job life cycle.
   ------------------------------------------------- */

   /*
      The dependency queue is used for jobs that are
      waiting for events (non-time events) to happen
      before they can begin running.
   */
   typedef struct {
      job_header_def job_header;
      dependency_info_def dependencies[MAX_DEPENDENCIES];
   } dependency_queue_def;

   /* 
      The active queue file contains details of each job
      currently queued in the scheduler.
   */
   typedef struct {
      job_header_def job_header;
      char next_scheduled_runtime[JOB_DATETIME_LEN+1];
      char current_status;   /* 0 - waiting on time
                                 1 - in dependency queue
                                 2 - running
                                 3 - in failed state (in alerts file)
								 4 - completed
                              */
	  char hold_flag;         /* Y = job held, N = not held, R = requed time but not held */
      char  failure_code[JOB_STATUSCODE_LEN+1];    /* as returned by program */
	  time_t run_timestamp;    /* numeric timestamp of next_scheduled_runtime */
	  char job_owner[JOB_OWNER_LEN+1];
	  char started_running_at[JOB_DATETIME_LEN+1];
   } active_queue_def;

/* -------------------------------------------------
  The JOB completed file record.
  This record contains information about a job
  that has finished running and is no longer in
  the active system.
   ------------------------------------------------- */
   typedef struct {
      job_header_def job_header;
      char last_runtime[JOB_DATETIME_LEN+1];    /* yyyymmdd hh:mm:ss  */
      char completiontype;      /* 0 = normal, 1 = manually deleted */
      char joblog_filename[JOB_JOBLOGFILENAME_LEN+1]; /* where 2>&1 output went */
   } job_history_def;

/* -------------------------------------------------
 * This structure is used to store the PID numbers of
 * any children we have forked to do work for us. It is
 * needed to keep track of child processes we must
 * call waitpid for.
   ------------------------------------------------- */
   typedef struct {
		   job_header_def job_header;
		   pid_t job_pid;
   } running_job_info_def;
   typedef struct {
		   int number_of_jobs_running;
		   int exclusive_job_running;
		   running_job_info_def running_jobs[MAX_RUNNING_JOBS];
   } currently_running_jobs_def;

/* -------------------------------------------------
 * Added to support the running of external programs
 * to generate alerts. These were causing errors in
 * the main server when the forked alert task stopped
 * as obviously there was no match for a job stopping.
 * In the main server where we process tasks that have
 * finished running we now check this also.
 * It is updated whan an alert is raised.
   ------------------------------------------------- */
   typedef struct {
		   pid_t alert_pid;
   } running_alert_info_def;
   typedef struct {
		   running_alert_info_def running_alerts[MAX_RUNNING_ALERTS];
   } currently_running_alerts_def;

/* -------------------------------------------------
   CALENDAR record structure.
   ------------------------------------------------- */
   typedef struct {
      char days[31];
   } month_elements;
   typedef struct {
      month_elements month[12];
   } all_months;

   typedef struct {
      /* --- start record key --- */
      char calendar_name[CALENDAR_NAME_LEN+1];
      char calendar_type;       /* 0 = job
                                 * 1 = holidays
                                 * 9 = deleted
                                 */
      char year[5];  /* 4 + pad, to ensure when year rolls over dates are reset/obsolete */
      /* --- end record key --- */
      char observe_holidays;    /* holidays allowed to override dates Y or N */
      all_months yearly_table;
      char holiday_calendar_to_use[CALENDAR_NAME_LEN+1];
      char description[CALENDAR_DESC_LEN+1];
   } calendar_def;
#endif
