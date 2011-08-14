#ifndef USER_DEFS
#define USER_DEFS

#include <stdio.h>
#include "api.h"
#include "scheduler.h"

#include "userrecdef.h"

#ifndef GLOBAL_DATA
   extern char user_file[];
   extern FILE *user_handle;
   extern int USER_local_auth;
   extern char USER_remote_auth_addr[];
#endif

/* Procedure interfaces */
int  USER_Initialise( void );
void USER_Terminate( void );
int  USER_add( USER_record_def *userrec );
int  USER_update( USER_record_def *userrec, long recordpos );
int  USER_update_admin_allowed( USER_record_def *userrec, long recordpos );
int  USER_update_checkflag( USER_record_def *userrec, long recordpos, int flag );
long USER_read( USER_record_def *userrec );
int  USER_allowed_auth_for_id( char *userid, char auth_needed );
void USER_list_user( API_Header_Def * api_bufptr, FILE *tx );
void USER_list_allusers( API_Header_Def * api_bufptr, FILE *tx );
void USER_init_record_defaults( USER_record_def * local_user_rec );
void USER_process_client_request( FILE *tx, char *buffer, char * loggedonuser );

#endif
