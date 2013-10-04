#ifndef GLOBAL_DATA
#define GLOBAL_DATA

#include "scheduler.h"

/* used by alerts.h */
   char alerts_file[]      = "alerts.dbs\0";
   FILE *alerts_handle = NULL;

/* used by calendar.h */
   char calendar_file[]   = "calendar.dbs\0";
   FILE *calendar_handle = NULL;

/* used by config.h */
   char config_version[41] = "SCHEDULER V1.14 (21Dec2011)";
   char config_file[]     = "config.dat\0";
   FILE *config_handle = NULL;

/* used by jobslib.h */
   char job_file[]        = "job_details.dbs\0";
   FILE *job_handle = NULL;

/* used by schedlib.h */
  char active_file[]     = "scheduler_queue.dbs\0";
  FILE *active_handle = NULL;
  char dependency_file[] = "dependency_queue.dbs\0";
  FILE *dependency_handle = NULL;
  int  SCHED_init_required;
  char SCHED_new_day_time[15];  /* yyyymmdd hh:mm */
  active_queue_def next_job_to_run;  /* jobrec for next job to run */

/* used by server.c and schedlib.c */
  currently_running_jobs_def running_job_table;
  int                        exclusive_job_running;

/* used by server.c and alerts.c */
  currently_running_alerts_def running_alert_table;

/* used by all  */
  FILE *msg_log_handle = NULL;
  char msg_logfile_name[25] = "logs/server_YYYYMMDD.log\0"; 
  time_t logfile_roll_time = 0;

/* used by user.h */
   char user_file[]        = "user_records.dbs\0";
   FILE *user_handle = NULL;
   int USER_local_auth = 1;    /* 1 = use local auth, 0 = use remote auth,
                                  2 = remote not responding, use local but retry remote periodically */
   char USER_remote_auth_addr[50];  /* ip address of remote auth system */

/* fields for memory usage recording by all units, memory stack tracing information */
   long mem_malloc1;
   long mem_malloc2;
   long mem_malloc3;

#endif
