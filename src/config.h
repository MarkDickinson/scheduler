#ifndef CONFIG_DEFS
#define CONFIG_DEFS

#include <stdio.h>

/*
  Notes: we use char all the way through these definitions. This is 
         because the API calls may go across the TCP-IP connections
         we support which use new-line as end of data request, so
         I don't want to use binary daya anywhere in case it
         contains a newline value anywhere.
*/

/* -------------------------------------------------
   Internal controls.
   The record structure used to keep track of how
   the scheduler is configured.
   ------------------------------------------------- */
   typedef struct { /* trace levels per module, 0 = none thru 9 = all */
		   int server;
		   int utils;
		   int jobslib;
		   int api;
		   int alerts;
		   int calendar;
		   int schedlib;
		   int bulletproof;
		   int user;
		   int memory;
		   /* added to suppress the deleted jobs from the sched listall display
			* as a default, but able to turn back on */
		   int show_deleted_schedjobs;
   } debug_level_def;

   typedef struct {
      char  version[41];      /* Version info */
      char  catchup_flag;     /* '0' = none, '2' = run for every missed day */
      int   log_level;        /* 0 = error, 1 = error/warn, 2=error/warn/info, 9 = all+trace */
      char  new_day_time[6];  /* hh:mm */
      char  last_new_day_run[18]; /* = JOB_DATETIME_LEN + 1, len used by UTILS_timestamp routines */
      char  next_new_day_run[18]; /* = JOB_DATETIME_LEN + 1, len used by UTILS_timestamp routines */
      char  enabled;          /* user control '0' = scheduler paused, '1' = jobs can run, '2' = shutdown queued */
      char  newday_action;    /* raise alert = '0', add waiton job entry = '1' */
      debug_level_def debug_level; /* trace levels from 0 = none thru 9 = all */
      int   isdst_flag;       /* is daylight savings time in effect; set from localtime calls */
  /* Implemented forexternal alert generation */
      char  use_central_alert_console;    /* Y = yes, N = no, E = execprog */
      int   central_alert_console_active; /* 0=no, 1=we copy alerts to a remote location (0 means its down) */
      char  remote_console_ipaddr[37];    /* xxx.xxx.xxx.xxx but larger to allow for IPv6 later */
      char  remote_console_port[5];       /* remote machine port */
      int   remote_console_socket;        /* socket to remote console system if in use, only ever written to */
      char  local_execprog_raise[90];     /* local pgm to use if use_central_alert_console = E, raise alert */
      char  local_execprog_cancel[90];    /* local pgm to use if use_central_alert_console = E, cancel alert */
   } internal_flags;

/* -------------------------------------------------
   Files used.
   ------------------------------------------------- */
#ifndef GLOBAL_DATA
   extern char config_version[];
   extern char config_file[];
   extern FILE *config_handle;
#endif

internal_flags pSCHEDULER_CONFIG_FLAGS; /* config flags */

/* Procedure interfaces */

int CONFIG_Initialise( internal_flags * flagbuffer );
int CONFIG_update( internal_flags * flagbuffer );
int CONFIG_check_dst_flag( void );

#endif
