#ifndef ALERTLIB
#define ALERTLIB
#include <stdio.h>
#include "scheduler.h"
#include "api.h"

#ifndef GLOBAL_DATA
   extern char alerts_file[];
   extern FILE *alerts_handle;
#endif

#define ALERTS_NO_ERROR             "900" /* nothing wrong */
#define ALERTS_SCHED_QUEUEWRITE_ERR "901" /* cannot write to scheduler active queue */
#define ALERTS_SCHED_QUEUEREAD_ERR  "902" /* cannot read scheduler active queue */
#define ALERTS_SCHED_FORK_ERR       "903" /* fork request failed */
#define ALERTS_SCHED_SYSEXEC_ERR    "904" /* system call failed */
#define ALERTS_JOB_FILEREAD_ERR     "905" /* cannot read jobs file */
#define ALERTS_JOB_FILEWRITE_ERR    "906" /* cannot write jobs file */
#define ALERTS_CALENDAR_BLOCK       "907" /* exclusion calendar stopped run */
#define ALERTS_JOBFAIL_ERR          "908" /* a job didn't return rc=0  */
#define ALERTS_USERNOTFOUND         "909" /* uid is not on the system */
#define ALERTS_PGMNOTFOUND          "910" /* jobs execprog not found */
#define ALERTS_OS_ERROR             "911" /* jobs execprog not found */
#define ALERTS_USER_CUSTOM          "912" /* A user customised alert text */
#define ALERTS_USER_GENERATED       "913" /* A user requested exit code */
#define ALERTS_SYSTEM_GENERATED     "914" /* A sigkill event or system crash */

int  ALERTS_initialise( void );
void ALERTS_terminate( void );
long ALERTS_read_alert( alertsfile_def *datarec );
long ALERTS_update_alert( alertsfile_def *datarec, char severity, char *failcode, char acked );
long ALERTS_add_alert( joblog_def *datarec, char severity, char *failcode );
long ALERTS_delete_alert( alertsfile_def *datarec );
long ALERTS_write_alert_record( alertsfile_def *alerts_rec, long recordnum );
void ALERTS_display_alerts( API_Header_Def * api_bufptr, FILE *tx );
int  ALERTS_count_alerts( void );
int  ALERTS_show_alert_detail( char *jobname, char *buffer, int buffsize );
void ALERTS_generic_system_alert( job_header_def * job_header, char * msgtext );
void ALERTS_generic_system_alert_MSGONLY( job_header_def * job_header, char * msgtext );
void ALERTS_generic_clear_remote_alert( job_header_def * job_header, char * msgtext );
int ALERTS_REMOTE_send_alert( alertsfile_def * alert_rec, int alerttype );
int ALERTS_LOCAL_run_alertprog( alertsfile_def * alert_rec, int alerttype );
void ALERTS_CUSTOM_set_alert_text( int exit_code, char * buffer, int bufferlen, char * sevflag, char * jobname );
#endif
