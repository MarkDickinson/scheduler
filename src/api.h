#ifndef API_DEFS
#define API_DEFS
#include <stdio.h>

/* -------------------------------------------------   API Interfaces.   ------------------------------------------------- */
#define MAX_API_DATA_LEN 4046 /* 4096 less API header and 1 byte newline                */
#define DATA_LEN_FIELD 9      /* size of data length char value                         */
#define MAX_API_CHAINS 4      /* max of 4 packets per request (3 continuations allowed) */

/* VVVVSSSSS VersionSubversion */
#define API_VERSION "000100001"         /* 9 bytes (10 byte field but 1 is a trailer)    */
#define API_EYECATCHER "API*"           /* Eye Catcher                                   */
#define API_CMD_LEN 9

#define API_HOSTNAME_LEN 35             /* hostid generating API request/response        */

/* ---------------------------- Defined API requests --------------------- */

/*  Any API command functions with the first digits non-zero
 *  indicate that it is a restricted function request that the server
 *  must do access checking against. It will either be "O" operator
 *  access level needed, or "A" admin access level needed.
 *  26July2002: Added S (security) and J (jobauth) levels. 
*/

/*                      Generic across all subsystems                      */

#define API_CMD_ADD          "A00000001" /* add request                                   */
#define API_CMD_DELETE       "O00000002" /* delete request                                */
#define API_CMD_SPARENOW     "A00000002" /* no longer used                                */
#define API_CMD_UPDATE       "O00000003" /* update request                                */
#define API_CMD_RETRIEVE     "000000004" /* retrive data request                          */
#define API_CMD_NEWDAY       "A00000005" /* run new day procedures                        */
#define API_CMD_SCHEDULE_ON  "O00000006" /* schedule record on                            */
#define API_CMD_STATUS       "000000007" /* status of whatever is being requested         */
#define API_CMD_CHECK4CHANGE "000000008" /* see if any change since last check of subsys  */
                                         /*  the above is to keep ip overheads down, if   */
                                         /*  used properly things like job refresh lists  */
                                         /*  only need to be requested by the client when */
                                         /*  the server replies a change has occurred.    */
#define API_CMD_LISTALL      "000000009" /* listall all items in class as 1 liners        */
#define API_CMD_DEBUGSET     "O00000010" /* change a debug level for a module             */
#define API_CMD_RETRIEVE_RAW "000000011" /* retrieve a record rather than a display form  */

/*                      Restricted Job Commands                            */

#define API_CMD_JOB_ADD      "J00000031" /* updates the job database                      */
#define API_CMD_JOB_DELETE   "J00000032" /* updates the job database                      */

/*                      Dependency subsystem specific                      */

#define API_CMD_LISTWAIT_BYDEPNAME "000000101" /* dependency queries                      */
#define API_CMD_DELETEALL_DEP "O00000102" /* delete all matches, dep/dep implementation   */
#define API_CMD_DELETEALL_JOB "O00000103" /* delete all matches, dep/job implementation   */

/*                        Alert subsystem specific                         */

#define API_CMD_ALERT_ACK     "O00000201" /* acknowledge an alert                         */
#define API_CMD_ALERT_FORCEOK "O00000202" /* mark a failed job as completed normally      */

/*                             Server specific                              */

#define API_CMD_SHUTDOWN      "A00000301" /* shutdown server                               */
#define API_CMD_SHUTDOWNFORCE "A00000302" /* shutdown immediately, kill any running jobs  */
#define API_CMD_SHOWSESSIONS  "000000303" /* show connected sessions                      */
#define API_CMD_STATUS_SHORT  "000000304" /* a summarised one line status display         */
#define API_CMD_RUNJOBNOW     "O00000305" /* sched command to run immediately             */
#define API_CMD_JOBHOLD_ON    "O00000306" /* hold a job                                   */
#define API_CMD_JOBHOLD_OFF   "O00000307" /* release of job from hold                     */

/*                      Dynamic Configuration changes                       */

#define API_CMD_EXECJOB_STATE  "A00000401" /* Change the execjob state off/on to pause/resume scheduler */
#define API_CMD_SPARENOW2      "A00000402" /* now spare                                    */
#define API_CMD_CATCHUPFLAG    "A00000403" /* Adjust the catchup flag on or off            */
#define API_CMD_SCHEDNEWDAYSET "A00000404" /* change the newday time                      */
#define API_CMD_NEWDAYFAILACT  "A00000405" /* change the newday fail action if queue busy  */
#define API_CMD_SETLOGLEVEL    "A00000406" /* change the server logging level              */
#define API_CMD_FULLSCHEDDISP  "A00000407" /* display on/off deleted jobs in sched listall */
#define API_CMD_ALERTFORWARDING "A00000408" /* alert forwarding config changes             */

/*                           User Authentification                          */
#define API_CMD_LOGON         "000000501" /* change authorisation level                   */
#define API_CMD_AM_I_ADMIN_1  "000000502" /* respond in data YES/NO to admin privalidges  */
#define API_CMD_USER_ADD      "S00000503" /* Add a new user                               */
#define API_CMD_USER_DELETE   "S00000504" /* Delete a user                                */
#define API_CMD_USER_NEWPSWD  "S00000505" /* New Password                                 */
#define API_CMD_LOGOFF        "000000506" /* reset authorisation level to guest/browse    */

/*                      Calendar subsystem specific                        */

#define API_CMD_CALENDAR_ADD     "J00000601" /* add a new calendar                           */
#define API_CMD_CALENDAR_MERGE   "J00000602" /* merge with existing entry                    */
#define API_CMD_CALENDAR_UNMERGE "J00000603" /* remove matches from existing entry           */
#define API_CMD_CALENDAR_DELETE  "J00000604" /* merge with existing entry                    */

/* --------------------------- Defined API responses --------------------- */

#define API_RESPCODE_LEN     3
#define API_RESPONSE_DISPLAY "100"       /* response - data recipient to display response */
#define API_RESPONSE_DATA    "101"       /* response - data recipient to process response */
#define API_RESPONSE_ERROR   "102"       /* response - error data recipient to process response */

/* ------ Values that replace the original command request ------- 
 * for API_RESPONSE_DATA message blocks these tell the requester what it
 * is expected to do with the data (a description of the data I guess).
 * These responses, in conjunction with the subsystem that owns the
 * response give the client enough information to manage the response.
 */
#define API_CMD_DUMP_FORMATTED   "010000000" /* dump response in format relevant to subsys.  */

/* --------------------------- The API data structure -------------------- */

typedef struct {
   char API_EyeCatcher[5];       /* Eye Catcher                          */
   char API_System_Name[10];     /* JOB, SCHED, CAL etc                  */
   char API_System_Version[10];  /* Version of API sybsystem             */
   char API_Command_Number[API_CMD_LEN+1];  /* requested function        */
   char API_Command_Response[4]; /* returned where appropriate           */
   char API_hostname[API_HOSTNAME_LEN+1]; /* what host provided the data */
   char API_More_Data_Follows;   /* 0 = no, 1 = yes, 2 = continuation no, 3 = continuation yes */
   char API_Data_Len[DATA_LEN_FIELD+1];    /* data length exluding header          */
   char API_Data_Buffer[MAX_API_DATA_LEN]; /* data to pass around        */
/*    char API_Newline[1]; put in by our data write so leave out */ 
/*    char API_NULL[1];    put in by data writes also */
} API_Header_Def;

/* --------------------------- API interface calls ----------------------- */

void API_init_buffer( API_Header_Def *pAPI_buffer );
void API_sethostname_field( API_Header_Def *pAPI_buffer );
void API_set_datalen( API_Header_Def *pAPI_buffer, long bufflen );
void API_DEBUG_dump_api( API_Header_Def *pAPI_buffer );
void API_Nullterm_Fields( API_Header_Def * api_bufptr );
int API_add_to_api_databuf( API_Header_Def * pApi_buffer, char * datastr,
				            int datalen, FILE *tx );
int API_flush_buffer( API_Header_Def * pApi_buffer, FILE * fhandle );
void API_Set_LF_Fields( API_Header_Def * api_bufptr );
void API_DEBUG_dump_job_header( char * hdr_buffer );
#endif
