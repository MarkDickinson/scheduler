#ifndef BULLETPROOF
#define BULLETPROOF
#include "api.h"
#include "scheduler.h"
#include "users.h"
int BULLETPROOF_API_Buffer( API_Header_Def *pApibuffer );
int BULLETPROOF_jobsfile_record( jobsfile_def *pJobrec );
int BULLETPROOF_dependency_record( dependency_queue_def * datarec );
int BULLETPROOF_user_record( USER_record_def * userrec, int serverside_checks );
void BULLETPROOF_USER_init_record_defaults( USER_record_def * local_user_rec );
int BULLETPROOF_CALENDAR_record( calendar_def * datarec );
void BULLETPROOF_CALENDAR_init_record_defaults( calendar_def * datarec );
#endif
