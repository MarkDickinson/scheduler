#ifndef CALENDAR_DEFS
#define CALENDAR_DEFS

/*
    CALENDAR.H ( header definition file )

    Manage the calendar file(s) used by the scheduler.

    Scheduler is by Mark Dickinson, 2001
*/

/*
  Notes: we use char all the way through these definitions. This is 
         because the API calls may go across the TCP-IP connections
         we support which use new-line as end of data request, so
         I don't want to use binary daya anywhere in case it
         contains a newline value anywhere.
*/

#include "scheduler.h"
#include "api.h"

/* -------------------------------------------------
   Files used.
   ------------------------------------------------- */
#ifndef GLOBAL_DATA
   extern char calendar_file[];
   extern FILE *calendar_handle;
#endif

#define CALENDAR_ADD 0
#define CALENDAR_MERGE 1
#define CALENDAR_UNMERGE 2
#define CALENDAR_DELETE 3
#define CALENDAR_UPDATE 4

int  CALENDAR_Initialise( void );
void CALENDAR_Terminate( void );
long CALENDAR_write_record( calendar_def * datarec, long recpos );
long CALENDAR_read_record( calendar_def * datarec );
long CALENDAR_update_record( calendar_def * datarec, int updatetype );
long CALENDAR_add( calendar_def * datarec );
long CALENDAR_update( calendar_def * datarec );
long CALENDAR_delete( calendar_def * datarec );
long CALENDAR_merge_worker( calendar_def * datarec, int mergeflag );
long CALENDAR_merge( calendar_def * datarec );
long CALENDAR_unmerge( calendar_def * datarec );
int  CALENDAR_next_runtime_timestamp( char *calname, char *timestampout, int catchupon );
int  CALENDAR_next_runtime_timestamp_calchange( char *calname, char *timestampout, int catchupon );
int  CALENDAR_next_runtime_timestamp_original( char *calname, char *timestampout, int catchupon, int usetodayallowed );
int  CALENDAR_check_holiday_setting( char * calname, int year, int month, int day );
void CALENDAR_put_record_in_API( API_Header_Def * api_bufptr );
int  CALENDAR_format_entry_for_display( char * calname, char * caltype, char * year, API_Header_Def * api_bufptr, FILE * tx );
void CALENDAR_format_list_for_display( API_Header_Def * api_bufptr, FILE * tx );
int  CALENDAR_calendar_exists( char * calname, char * year, char type );
long CALENDAR_count_entries_using_holcal( char * calname );
void CALENDAR_checkfor_obsolete_entries( void );
#endif
