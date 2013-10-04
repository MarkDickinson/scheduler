#ifndef SCHEDLIB_DEFS
#define SCHEDLIB_DEFS

#include <stdio.h>
#include "scheduler.h"
#include "jobslib.h"
#include "config.h"

#ifndef GLOBAL_DATA
   extern char active_file[];
   extern FILE *active_handle;
   extern char dependency_file[];
   extern FILE *dependency_handle;
   extern char completed_file[];
   extern FILE *completed_handle;
   extern int  SCHED_init_required;
   extern char SCHED_new_day_time[];  /* yyyymmdd hh:mm */
#endif

int  SCHED_Initialise( void );
void SCHED_Terminate( void );
int  SCHED_schedule_on( jobsfile_def * datarec );
int  SCHED_run_job_now( active_queue_def * job_name );
int  SCHED_delete_job( job_header_def * job_name );
int  SCHED_show_schedule( API_Header_Def * api_bufptr, FILE *tx  );
int  SCHED_spawn_job( active_queue_def * datarec );
int  SCHED_count_active( void );
int  SCHED_internal_add_some_dependencies( active_queue_def * active_entry, long recordnum );
void SCHED_display_server_status( API_Header_Def * pApi_buffer, internal_flags *  GLOBAL_flags, FILE *tx, int exjobs  );
void SCHED_display_server_status_alertsonly( API_Header_Def * pApi_buffer, internal_flags *  GLOBAL_flags, FILE *tx, int exjobs  );
void SCHED_display_server_status_short( API_Header_Def * pApi_buffer, internal_flags * GLOBAL_flags, FILE *tx );
void SCHED_Submit_Newday_Job( void );
int  SCHED_hold_job( char * jobname, int actiontype );

long SCHED_ACTIVE_get_next_jobtorun( active_queue_def * datarec );
long SCHED_ACTIVE_read_record( active_queue_def * datarec );
long SCHED_ACTIVE_write_record( active_queue_def * datarec, long recordnum );
long SCHED_ACTIVE_delete_record( active_queue_def * datarec, int internal );

long SCHED_DEPEND_write_record( dependency_queue_def * datarec, long recordnum );
long SCHED_DEPEND_read_record( dependency_queue_def * datarec );
long SCHED_DEPEND_delete_record( dependency_queue_def * datarec );
long SCHED_DEPEND_update_record( dependency_queue_def * datarec, long dependencynum );
void SCHED_DEBUG_dump_active_queue_def( active_queue_def * recordptr );
void SCHED_DEPEND_delete_all_relying_on_job( job_header_def * job_identifier );
void SCHED_DEPEND_freeall_for_job( job_header_def * job_identifier );
void SCHED_DEPEND_check_job_dependencies( void );
void SCHED_DEPEND_listall_waiting_on_dep( API_Header_Def * pApi_buffer, FILE *tx, char * jobname );
void SCHED_DEPEND_listall( API_Header_Def * pApi_buffer, FILE *tx );
void SCHED_DEPEND_delete_all_relying_on_files( void );
void SCHED_DEBUG_stat_dependency_file(void);
void SCHED_DEPEND_delete_all_relying_on_dependency( char * dependency_name );   
#endif
