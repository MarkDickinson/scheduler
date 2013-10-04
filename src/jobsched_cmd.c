#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pwd.h>
#include "api.h"
#include "utils.h"
#include "bulletproof.h"
#include "config.h"
#include "system_type.h"
#include "calendar_utils.h" 
#include "memorylib.h"

/* #define DEBUG_MODE_ON 1 */

/* Added to try to figure out why segfaults occur when a non root
 * user tries to autolin but works fine for root; will move to the
 * constants file after testing as this should be a constant anyway
*/
#define MAX_WORKBUF_LEN_JS 4096

/* Global variable required, this is used bu UTILS.H. In normal utils use
 * this would be an external variable managed by the server process, but as
 * we are using the UTILS library from a standalone module we must set this
 * up and initialise it ourselves. */
FILE *msg_log_handle = NULL;
/* #define msg_log_handle STDOUT */

/* -----------------------------------------------------------------------------

                        H e l p     R o u t i n e s

   ----------------------------------------------------------------------------- */

/* *****************************************************************************
 * Added as an afterthought, there may be some people who can't be bothered
 * to read the manual, and I guess it doesn't hurt to have programatic help as
 * I havn't supplied man pages yet.
   ***************************************************************************** */
void help_request_interface( char * sHelpRequest ) {
   char *ptr;

   /* uppercase the whole request */
   UTILS_uppercase_string( sHelpRequest );

   /* See if it is a generic help request or a specific subsystem */
   ptr = sHelpRequest;
   while ( (*ptr != ' ') && (*ptr != '\n') && (*ptr != '\0') ) { ptr++; }

   if (*ptr != ' ') {  /* no additional data, generic request */
       ptr = sHelpRequest;
	   strcpy( ptr, "ALL" );
   }
   else {
      /* space found, skip all spaces to the specific request */
      while (*ptr == ' ') { ptr++; }
   }

   printf("Job Scheduler, Mark Dickinson, 2002\n");
   printf("\n");
   printf("These help screens display the syntax of the commands only\n");
   printf("to be used as a guide. Do not use them without refering to\n");
   printf("the manual. NEVER use commands marked (***READ THE MANUAL FIRST***)\n");
   printf("unless you have read the manual.\n");
   printf("\n");

   /* see what the request was */
   if (memcmp(ptr,"JOB",3) == 0) {
      printf("The JOB commands interact directly with the job definition\n");
      printf("database. They ARE NOT USED to control jobs queued for\n");
      printf("execution by the scheduler.\n");
      printf("\n");
      printf("JOB ADD <jobname>,OWNER <user>,TIME <yyyymmdd hh:mm>,\n");
      printf("        CMD <cmd>[,PARM \"<parm>\"]\n");
      printf("       [,DEP \"JOB <jobn>|FILE <filen>\" total of five dependencies]\n");
      printf("       [,REPEATEVERY nn,]\n");
      printf("       [,DAYS \"MON,TUE...,SUN\"][,DESC desc test with no commas]\n");
      printf("       [,CATCHUP YES|NO][,CALENDAR <calname>]\n");
      printf("JOB DELETE <jobname>     (*** READ THE MANUAL FIRST ***)\n");
      printf("JOB INFO <jobname>\n");
      printf("JOB LISTALL [<wildcard mask>]\n");
      printf("JOB SUBMIT <jobname>     (*** READ THE MANUAL FIRST ***)\n");
   }
   else if (memcmp(ptr,"ALERT",5) == 0) {
      printf("The alert commands are used to manage jobs that have\n");
      printf("been placed onto the alert queue.\n");
      printf("\n");
      printf("ALERT ACK <jobname>          ALERT INFO <jobname>\n");
      printf("ALERT LISTALL                ALERT RESTART <jobnname>\n");
      printf("ALERT FORCEOK <jobname> (*** READ THE MANUAL FIRST ***)\n");
   }
   else if (memcmp(ptr,"CAL",3) == 0) {
      printf("The calendar commands manage the calendars that can be\n");
      printf("used to control job scheduling.\n\n");
      printf("CALENDAR ADD <name>,DESC \"XXX\",TYPE JOB|HOLIDAY,YEAR YYYY[,HOLCAL <name>],<format-string>\n");
      printf("CALENDAR MERGE <name>,TYPE JOB|HOLIDAY,YEAR YYYY,<format-string>\n");
      printf("CALENDAR SUBDELETE|UNMERGE <name>,TYPE JOB|HOLIDAY,YEAR YYYY,<format-string>\n");
      printf("CALENDAR DELETE <name>,TYPE JOB|HOLIDAY,YEAR YYYY\n");
      printf("CALENDAR LISTALL\n");
      printf("CALENDAR INFO <name>,TYPE JOB|HOLIDAY,YEAR YYYY\n");
      printf("\n<format-string> may be one of...\n");
      printf("     FORMAT MONTHSDAYS \"MM/DD,MM/DD,MM/DD,MM/DD,MM/DD,MM/DD...\"\n");
      printf("     FORMAT DAYS ALL|JAN|FEB...DEC \"DD,DD,DD,DD...\"\n");
      printf("     FORMAT DAYNAMES \"SUN,MON,TUE,WED,THU,FRI,SAT\"\n");
      printf("\nExample:\n");
      printf("  CALENDAR ADD TESTCAL,DESC \"Test Calendar\",TYPE JOB,YEAR 2002,FORMAT DAYS ALL \"01\"\n");
      printf("    creates a calendar that will run any job using on the 1st of every month in 2002\n");
   }
   else if (memcmp(ptr,"DEBUG",5) == 0) {
      printf("The debug command enables debugging on the job scheduler\n");
      printf("server modules. n is a number from 0 to 9. If you ever\n");
      printf("use 9 you better have gigabytes of space in the log\n");
      printf("filesystem. ONLY USE IF REQUESTED BY SUPPORT STAFF.\n");
      printf("\n");
      printf("DEBUG SERVER n      ( server main code block )\n");
      printf("DEBUG UTILS n       ( utility library )\n");
      printf("DEBUG JOBSLIB n     ( job definition database functions )\n");
      printf("DEBUG APILIB n      ( api calls )\n");
      printf("DEBUG ALERTLIB n    ( alert queue management )\n");
      printf("DEBUG CALENDAR n    ( calendar processing )\n");
      printf("DEBUG SCHEDLIB n    ( active queue and dependency queue functions )\n");
      printf("DEBUG BULLETPROOF n ( data structure checking library )\n");
      printf("DEBUG USER n        ( user management library )\n");
      printf("DEBUG MEMORY n      ( memory management library )\n");
      printf("DEBUG ALL n\n");
   }
   else if (memcmp(ptr,"DEP",3) == 0) {
      printf("The DEP commands are used to manage entries in the dependency\n");
      printf("queue database.\n");
      printf("\n");
      printf("DEP LISTALL\n");
      printf("DEP LISTWAIT <dependencyname>\n");
      printf("DEP CLEARALL JOB <jobname>          (*** READ THE MANUAL FIRST ***)\n");
      printf("DEP CLEARALL DEP <dependencyname>   (*** READ THE MANUAL FIRST ***)\n");
   }
   else if (memcmp(ptr,"SCHED",5) == 0) {
      printf("The scheduler commands interact with both jobs on the\n");
      printf("active scheduler queue, and can also change some of\n");
      printf("the server configuration values.\n");
      printf("ONLY the commands for jobs on the active queue are\n");
      printf("shown here (those at operator level).\n");
      printf("For SCHED commands available to ADMIN level users USE\n");
      printf("THE MANUAL.\n");
      printf("\n");
      printf("SCHED LISTALL [<wildcard mask>]\n");
      printf("SCHED STATUS [ SHORT | MEM | ALERTCFG ]\n");
      printf("SCHED HOLD-ON <jobname>           SCHED HOLD-OFF <jobname>\n");
      printf("SCHED EXECJOBS OFF                SCHED EXECJOBS ON\n");
      printf("SCHED SHOWSESSIONS\n");
      printf("SCHED DELETE <jobname> (*** READ THE MANUAL FIRST ***)\n");
      printf("SCHED RUNNOW <jobname> (*** READ THE MANUAL FIRST ***)\n");
      printf("SCHED NEWDAYTIME HH:MM\n");
      printf("SCHED LOGLEVEL INFO|WARN|ERROR (caution:info is very verbose)\n");
   }
   else if (memcmp(ptr,"USER",4) == 0) {
      printf("Users are managed by Security authority users or Administrators.\n" );
      printf("USER ADD <name>, PASSWORD <password>,\n" );
      printf("     AUTH ADMIN|OPERATOR|JOB|SECURITY,\n" );
      printf("     AUTOAUTH YES|NO, SUBSETADDAUTH YES|NO|ADMIN,\n" );
      printf("     USERLIST \"<1>,<2>...<10>\",DESC \"<description>\"\n" );
      printf("USER DELETE <name>\n" );
      printf("USER PASSWORD <name>,<password>  (comma is required)\n" );
      printf("USER LISTALL\n" );
      printf("USER INFO <name>\n" );
   }
   else if (
             (memcmp(ptr,"LOGIN",5) == 0) ||
             (memcmp(ptr,"LOGON",5) == 0) ||
             (memcmp(ptr,"AUTOLOGON",9) == 0) ||
             (memcmp(ptr,"AUTOLOGIN",9) == 0) 
           ) {
      printf("LOGIN <username> <password>\n" );
      printf("AUTOLOGIN is available onto to native unix users who have\n" );
      printf("          that attribute permitted on their user records.\n" );
   }
   else if (memcmp(ptr,"OPEN",4) == 0) {
      printf("OPEN is not really a subsystem. It is a command that may be used\n" );
      printf("to connect to another server without needing to exit and restart\n" );
      printf("the jobsched_cmd program.\n\n" );
      printf("OPEN <ip-address> [<port number>]\n" );
      printf("OPEN INFO\n\n" );
      printf("Used with an ip-address will connect to a new server. If the\n" );
      printf("port number is not provided it defaults to  the port used to\n" );
      printf("connect with the current server.\n\n" );
      printf("Used with INFO will display the current connection details.\n" );
   }
   else if (
             (memcmp(ptr,"LOGOFF",6) == 0) ||
             (memcmp(ptr,"LOGOUT",6) == 0) 
           ) {
      printf("LOGOFF\n" );
      printf("This will change you access level back to guest/browse,\n" );
      printf("it will not exit the program, use EXIT for that.\n" );
   }
   else {   /* must be ALL or a typo we catch here */
      printf("You need to specify what subsystem you wish help for.\n");
      printf("Defined subsystems are...\n");
      printf("  JOB, ALERT, CAL, DEP, SCHED, USER, DEBUG, LOGIN, LOGOUT or OPEN.\n");
      printf("Syntax: HELP <subsystem>\n");
      printf("    ie: HELP DEP\n");
   }
   return;  
} /* help_request_interface */

/* -----------------------------------------------------------------------------

                    C o m m o n     R o u t i n e s

   ----------------------------------------------------------------------------- */

/* *****************************************************************************
   Called when something unrecoverable has happened. It logs the cause and we 
   then stop immediatly.
   ***************************************************************************** */
static void die(const char *on_what) {
   printf( "%s: %s\n", strerror(errno), on_what );
   exit(1);
} /* die */


/* *****************************************************************************
   LISTALL is common across all subsystems. There is no user data to be passed.
   Fill in the request with the subsystem type (passed) and issue the request.
   No validation is required for this other than a subsystem name check.
   ***************************************************************************** */
int generic_listall_cmd( char *requesttype, API_Header_Def *pAPI_buffer, char * mask ) {
#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in generic_listall_cmd\n" );
#endif
   API_init_buffer( pAPI_buffer );
   strcpy( pAPI_buffer->API_System_Name, requesttype );
   strcpy( pAPI_buffer->API_Command_Number, API_CMD_LISTALL );
   if (strlen(mask) == 0) {
      API_set_datalen( pAPI_buffer, 0 );
   }
   else {
      strncpy( pAPI_buffer->API_Data_Buffer, mask, JOB_NAME_LEN );
      pAPI_buffer->API_Data_Buffer[JOB_NAME_LEN] = '\0';
      API_set_datalen( pAPI_buffer, strlen(pAPI_buffer->API_Data_Buffer) );
   }
   return( 1 );   /* all ok */
} /* generic_listall_cmd */


/* -----------------------------------------------------------------------------

                         Server reconnection routines

   ----------------------------------------------------------------------------- */

/* *****************************************************************************
 * Return 1 if passes syntax checks and reply fields built, else return 0.
   ***************************************************************************** */
int parse_open_command( char *cmdbuf, int *portnum, char *next_addr ) {
   /* OPEN <ipaddr> [<port>]
	* note: reversal of normal order is simply because a site will
	*       normally use the same port for all production servers
	*       so we will allow it to default to the previous port
	*       used if one is not provided. */
   char sTestParmName[100];
   int nFieldLen, nLeadingPads, nOverrunFlag, newport, i, dotcount, numerrcount;
   char *pSptr, *pTestPtr;
   pSptr = cmdbuf;

#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in parse_open_command\n" );
#endif

   /* We know we have the OPEN field so skip it */
   nFieldLen = UTILS_parse_string( pSptr, ' ', (char *)&sTestParmName, 99, 
                                   &nLeadingPads, &nOverrunFlag );
   if (nFieldLen == 0) {
      printf( "*E* No data in open command string\n" );
      return( 0 );
   }
   pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;
   pSptr = pSptr + UTILS_count_delims( pSptr, ' ' );

   /* We require an IP address */
   nFieldLen = UTILS_parse_string( pSptr, ' ', (char *)&sTestParmName, 99, 
                                   &nLeadingPads, &nOverrunFlag );
   if (nFieldLen == 0) {
      printf( "*E* No IP Address provided in open command\n" );
      return( 0 );
   }
   pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;

   /* Check it looks like an ipv4 address */
   pTestPtr = (char *)&sTestParmName;
   dotcount = 0;
   numerrcount = 0;
   for (i = 0; i < nFieldLen; i++) {
      if (*pTestPtr == '.') {
         dotcount++;
      }
	  else if ((*pTestPtr < '0') || (*pTestPtr > '9')) {
         numerrcount++;
      }
	  pTestPtr++;
   }
   /* need to check for no last number also, ie: 1.1.1. passes otherwise */
   pTestPtr--;
   if ((dotcount != 3) || (numerrcount > 0) || (*pTestPtr == '.')) {
      printf( "*E* IP Address %s in OPEN command is not legal\n", sTestParmName );
      return( 0 );
   }
   strncpy( next_addr, sTestParmName, 59 );

   /* Check to see if a port nmber was provided */
   nFieldLen = UTILS_parse_string( pSptr, ' ', (char *)&sTestParmName, 99, 
                                   &nLeadingPads, &nOverrunFlag );
   if (nFieldLen == 0) {
       /* no port number, leave alone */
   }
   else { /* else get the port number */
      newport = atoi(sTestParmName);
      if (newport == 0) {
         printf( "*E* Non numeric port number provided in open command\n" );
         return( 0 );
      }
      else {
         *portnum = newport;
      }
   }

   return( 1 );  /* looks ok so far */
} /* parse_open_command */

/* *****************************************************************************
 * Open and Close of sockets moved to seperate procedures, to support the
 * implementation of an open command to allow users to switch between job
 * scheduler servers without having to restart the application.
 * Use *sockptr to pass back the socket we create.
 * Return 1 if OK, 0 if sockptr needed to be set to null
   ***************************************************************************** */
int connect_to_server(int portnum, char *ipaddr, int *sockptr) {
   int z;
   char   workbuf[4096];
   struct sockaddr_in adr_srvr; /* AF_INET */
   int    len_inet;           /* length */

   /* create server socket address */
   memset(&adr_srvr,0,sizeof adr_srvr);
   adr_srvr.sin_family = AF_INET;
   adr_srvr.sin_port = htons(portnum);
   adr_srvr.sin_addr.s_addr = inet_addr(ipaddr);
#ifdef SOLARIS
   /* Solaris does not have a INADDR_NONE defined
	* but returns -1 for a bad address */
   if (adr_srvr.sin_addr.s_addr == -1 )
#else
   if (adr_srvr.sin_addr.s_addr == INADDR_NONE )
#endif
      die("bad network address");
   len_inet = sizeof adr_srvr;

   /* create a tcpip socket to use */
   *sockptr = socket(PF_INET,SOCK_STREAM,0);
   if (*sockptr == -1) {
      printf( "%s: %s\n", strerror(errno), "Unable to create TCPIP socket" );
	  return( 0 ); /* failed */ 
   }

   /* connect to the server */
   z = connect(*sockptr,&adr_srvr,len_inet);
   if (z == -1) {
      snprintf( workbuf, 4095, "connect(2), host %s nothing listening on port %d", ipaddr, portnum );
      printf( "%s: %s\n", strerror(errno), workbuf );
	  return( 0 ); /* failed */
   }

   /* read server banner info */
   z = read(*sockptr,&workbuf,sizeof workbuf - 1);
   if (z == -1) {
      printf( "%s: %s\n", strerror(errno), "No server banner was read, not a scheduler ?" );
      close(*sockptr);
      putchar('\n');
	  return( 0 ); /* failed */
   }
   workbuf[z] = '\0';
#ifdef DEBUG_MODE_ON
   /* display the banner */
   printf( "%s\n", workbuf );
#endif
   return( 1 );  /* All OK */
} /* connect_to_server*/

/* *****************************************************************************
 * Use *sockptr to pass the socket to close.
 * Return 1 if OK, 0 if sockptr needed to be set to null
   ***************************************************************************** */
void disconnect_from_server( int sockptr ) {
   /* close the socket and exit */
#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in main (closing socket and ending)\n" );
#endif
   close(sockptr);
   putchar('\n');
   return;
} /* disconnect_from_server */

/* -----------------------------------------------------------------------------

        J O B     C o m m a n d     P r o c e s s i n g     R o u t i n e s

   ----------------------------------------------------------------------------- */

/* *****************************************************************************
   Called from a job add or update request.
   return 0 if an error.
   NOTE: The input data from the command line is still in an unknown case here.
         We need to uppercase for the commands we check but MUST leave the
         data values untouched.
   ***************************************************************************** */
int job_updatejobbuffer( char *pCmdstring, jobsfile_def *pJobRecord ) {

/* JOB ADD|UPDATE <jobname>,OWNER <user>,TIME <yyyymmdd hh:mm>,CMD <cmd>
   [,PARM "<parm>"][,CAL <cal>]
   [,DEP "JOB <jobn>|FILE <filen>[,...]" total of five dependencies]  
   REPEATEVERY <mins>, DAYS "MON,TUE,...SUN",DESC dddddd
   CATCHUP YES|NO,CALENDAR <calname>
*/

/*   char sTestField[255]; */
   char sTestParmName[100];
   char sTestParmValue[150];
   char sTestParmValue2[100];
   int nFieldLen, nLeadingPads, nOverrunFlag, endofdata, depcount, i, j;
   char *pSptr, *pSptr2, *workptr;

   pSptr = pCmdstring;

#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_updatejobbuffer (about to loop)\n" );
#endif

   /* loop until we reach the end of the command string passed */
   endofdata = 0;
   while (endofdata == 0) {
      /* --- find the field type --- */
      nFieldLen = UTILS_parse_string( pSptr, ' ', (char *)&sTestParmName, 99, 
                                      &nLeadingPads, &nOverrunFlag );
      if (nFieldLen == 0) {
         endofdata = 1;
      }
      else {
        pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;
      }

#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_updatejobbuffer (end of parse 1, endofdata=%d), parm=%s\n", endofdata, sTestParmName );
#endif

      /* --- find the field data --- */

      /* uppercase the parameter value to search on */
      workptr = (char *)&sTestParmName;
      while ( (*workptr != '\0') && (*workptr != '\n') ) {
         *workptr = toupper( *workptr );
         workptr++;
      }

	  /*
	   * most fields are up until the next comma. These are processed in
	   * this (large) block of code.
	   */
      if (
           (memcmp(sTestParmName, "OWNER", 5) == 0) ||
           (memcmp(sTestParmName, "TIME", 4) == 0) ||
           (memcmp(sTestParmName, "CMD", 3) == 0) ||
           (memcmp(sTestParmName, "CAL", 3) == 0) ||
           (memcmp(sTestParmName, "DESC", 4) == 0) ||
		   (memcmp(sTestParmName, "REPEATEVERY", 11) == 0) ||
		   (memcmp(sTestParmName, "CATCHUP", 7) == 0)
         ) {
#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_updatejobbuffer (starting parse 2)\n" );
#endif
         pSptr = pSptr + UTILS_count_delims( pSptr, ' ' );
         nFieldLen = UTILS_parse_string( pSptr, ',', (char *)&sTestParmValue, 99, 
                                         &nLeadingPads, &nOverrunFlag );
         if (nFieldLen == 0) {
            printf( "*E* Missing parameter value for field %s\n", sTestParmName );
            return( 0 );
         }
         else {
#ifdef DEBUG_MODE_ON
  printf( "DEBUG: Got parm %s, value %s\n", sTestParmName, sTestParmValue );
#endif
            pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;
            pSptr = pSptr + UTILS_count_delims( pSptr, ',' );
            if (memcmp(sTestParmName, "OWNER", 5) == 0) {
               strncpy( pJobRecord->job_owner, sTestParmValue, JOB_OWNER_LEN );
            }
            else if (memcmp(sTestParmName, "TIME", 4) == 0) {
               strncpy( pJobRecord->next_scheduled_runtime, sTestParmValue, JOB_DATETIME_LEN );
			   if (strlen(pJobRecord->next_scheduled_runtime) <= (JOB_DATETIME_LEN - 3)) {
			      strcat(pJobRecord->next_scheduled_runtime, ":00");
			   }
			   if (strlen(pJobRecord->next_scheduled_runtime) < JOB_DATETIME_LEN) {
                  printf( "*E* TIME MUST BE YYYYMMDD HH:MM, %s IS INVALID\n", pJobRecord->next_scheduled_runtime );
                  return( 0 );
			   }
            }
            else if (memcmp(sTestParmName, "CMD", 3) == 0) {
               strncpy( pJobRecord->program_to_execute, sTestParmValue, JOB_EXECPROG_LEN );
            }
            else if (memcmp(sTestParmName, "CAL", 3) == 0) {
               strncpy( pJobRecord->calendar_name, sTestParmValue, JOB_CALNAME_LEN );
               UTILS_space_fill_string( pJobRecord->calendar_name, CALENDAR_NAME_LEN );
               pJobRecord->use_calendar = '1';   /* using a calendar */
            }
            else if (memcmp(sTestParmName, "DESC", 4) == 0) {
               strncpy( pJobRecord->description, sTestParmValue, JOB_DESC_LEN );
            }
            else if (memcmp(sTestParmName, "REPEATEVERY", 11) == 0) {
#ifdef DEBUG_MODE_ON
               printf( "DEBUG: in job_updatejobbuffer (checking repeatevery), value %s\n", sTestParmValue );
#endif
               i = atoi( sTestParmValue );
			   if (i < 5) {
					   printf( "*E* REPEATEVERY VALUE IGNORED, CANNOT BE < 5 (MINUTES)\n" );
			   }
			   else {
					   pJobRecord->use_calendar = '3';  /* repeat type */
					   /* pJobRecord->requeue_mins = i; a string now */
                       UTILS_number_to_string( i, (char *)&pJobRecord->requeue_mins, JOB_REPEATLEN );
                       pJobRecord->requeue_mins[JOB_REPEATLEN] = '\0'; /* null terminate it */
			   }
#ifdef DEBUG_MODE_ON
               printf( "DEBUG: in job_updatejobbuffer (checking repeatevery), integer was %d\n", i );
#endif
			}
            else if (memcmp(sTestParmName, "CATCHUP", 7) == 0) {
               UTILS_uppercase_string( (char *)&sTestParmValue );
               if (memcmp(sTestParmValue, "NO", 2) == 0) {
                  pJobRecord->job_catchup_allowed = 'N';
               }
			   else if (memcmp(sTestParmValue, "YES", 3) == 0) {
                  pJobRecord->job_catchup_allowed = 'Y';
               }
			   else {
                  printf( "*E* Invalid CATCHUP value, defaulting to yes\n" );
                  pJobRecord->job_catchup_allowed = 'Y';
               }
            }
         }
      }

	  /*
	   * The other fields have data between " characters. These are
	   * parm, dep and days. These are handled in this block of code.
	  */
      else if (
                (memcmp(sTestParmName, "PARM", 4) == 0) ||
                (memcmp(sTestParmName, "DEP", 3) == 0) ||
				(memcmp(sTestParmName, "DAYS", 4) == 0)
         ) {
#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_updatejobbuffer (starting parse3)\n" );
#endif
         pSptr = pSptr + UTILS_count_delims( pSptr, ' ' );
         nFieldLen = UTILS_parse_string( pSptr, '"', (char *)&sTestParmValue, 149, 
                                         &nLeadingPads, &nOverrunFlag );
         if (nFieldLen == 0) {
            printf( "*E* Missing parameter value for field %s\n", sTestParmName );
            return( 0 );
         }
         else {
#ifdef DEBUG_MODE_ON
  printf( "DEBUG: Got parm %s, value '%s'\n", sTestParmName, sTestParmValue );
#endif
            pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;
            pSptr = pSptr + UTILS_count_delims( pSptr, '"' );
			/* MID: added now desc is after parm to fix bug here */
            pSptr = pSptr + UTILS_count_delims( pSptr, ',' );

			/* parm is easy, its the entire field without case shifting needed */
            if (memcmp(sTestParmName, "PARM", 4) == 0) {
#ifdef DEBUG_MODE_ON
  printf( "DEBUG: Setting program parameters to %s\n", sTestParmValue );
#endif
               strncpy( pJobRecord->program_parameters, sTestParmValue, JOB_PARM_LEN );
            }

			/* days is the next easiest, a list of days that should be
			 * all in uppercase */
			else if (memcmp(sTestParmName, "DAYS", 4) == 0) {
               UTILS_uppercase_string( (char *)&sTestParmValue );
               /* scan for each comma seperated field */
               pSptr2 = (char *)&sTestParmValue;
               pSptr2 = pSptr2 + UTILS_count_delims( pSptr2, ' ' );
			   nFieldLen = 1; /* to start loop */
			   while (nFieldLen > 0) {
                  nFieldLen = UTILS_parse_string( pSptr2, ',', (char *)&sTestParmValue2, 99, 
                                                  &nLeadingPads, &nOverrunFlag );
                  if (nFieldLen > 0) {
                     i = UTILS_get_daynumber( (char *)&sTestParmValue2 );
					 if (i < 7) { pJobRecord->crontype_weekdays[i] = '1'; }
					 else {
							 printf( "*E* Day name %s not recognised, ignored !\n", sTestParmValue2 );
					 }
				  }
                  pSptr2 = pSptr2 + nFieldLen + nLeadingPads + nOverrunFlag;
                  pSptr2 = pSptr2 + UTILS_count_delims( pSptr2, ',' );
                  pSptr2 = pSptr2 + UTILS_count_delims( pSptr2, ' ' );
			   }
			   /* sanity check */
               j = 0;
			   for (i = 0; i < 7; i++) { if (pJobRecord->crontype_weekdays[i] == '1') { j++; } }
			   if (j > 6) { /* every day selected, just use normal processing without this table */
					   pJobRecord->use_calendar = '0'; /* no calendar, run every day */
					   for (i = 0; i < 7; i++) { pJobRecord->crontype_weekdays[i] = '0'; } /* clear table */
			   }
			   else if (j == 0) { /* no days selected, error, don't use this table */
					   printf( "*E* DAYS parameter used, but no days provided\n" );
					   return( 0 );
			   }
			   else {
					   pJobRecord->use_calendar = '2'; /* day selection processing */
			   }
			}

			/* dep is the hardest, if the dependency is a job it MUST be
			 * uppercased as we force uppercasing of job names; if it is a file
			 * we must NOT alter the case as unix filenames can be mixed case.
			 */
            else { /* must be DEP */
               /* scan for each comma seperated field and add to dependency fields */
               pSptr2 = (char *)&sTestParmValue;
               depcount = 0;
               while (depcount < MAX_DEPENDENCIES) {
#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_updatejobbuffer (starting parse4), pointer at %s\n", pSptr2 );
#endif
                  nFieldLen = UTILS_parse_string( pSptr2, ' ', (char *)&sTestParmName, 99, 
                                                  &nLeadingPads, &nOverrunFlag );
                  if (nFieldLen == 0) {
                     depcount = MAX_DEPENDENCIES + 99;   /* end of dep parms */
                  }
                  else {
                     pSptr2 = pSptr2 + nFieldLen + nLeadingPads + nOverrunFlag;
                     pSptr2 = pSptr2 + UTILS_count_delims( pSptr2, ',' );
                     pSptr2 = pSptr2 + UTILS_count_delims( pSptr2, ' ' );
#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_updatejobbuffer (starting parse5), testparmname is %s\n", sTestParmName );
#endif
                     nFieldLen = UTILS_parse_string( pSptr2, ',', (char *)&sTestParmValue2, 99, 
                                                     &nLeadingPads, &nOverrunFlag );
                     if (nFieldLen == 0) {
                        printf( "*E* Missing value for dependency %s\n", sTestParmName );
                        return( 0 );
                     }
                     else {
                        UTILS_uppercase_string( (char *)&sTestParmName );
                        if (memcmp(sTestParmName, "FILE", 4) == 0) {
#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_updatejobbuffer, setting dependency %d to FILE %s\n", depcount, sTestParmValue2 );
#endif
                           strncpy( pJobRecord->dependencies[depcount].dependency_name, sTestParmValue2, JOB_DEPENDTARGET_LEN );
                           pJobRecord->dependencies[depcount].dependency_type = '2';
                           depcount++;
                        }
                        else if (memcmp(sTestParmName, "JOB", 3) == 0) {
#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_updatejobbuffer, setting dependency %d to JOB %s\n", depcount, sTestParmValue2 );
#endif
                           UTILS_uppercase_string( (char *)&sTestParmValue2 );
                           strncpy( pJobRecord->dependencies[depcount].dependency_name, sTestParmValue2, JOB_DEPENDTARGET_LEN );
                           pJobRecord->dependencies[depcount].dependency_type = '1';
                           depcount++;
                        }
                        else {
                           printf( "*E* %s is not a recognised dependency type, need FILE or JOB\n", sTestParmValue2 );
                           return( 0 );
                        }
                     }
                  }
				   /* MID:added to fix dep parsing */
				  pSptr2 = pSptr2 + nFieldLen;
                  pSptr2 = pSptr2 + UTILS_count_delims( pSptr2, ',' );
                  pSptr2 = pSptr2 + UTILS_count_delims( pSptr2, ' ' );
               } /* while */
            }  /* dep */
         }  /* else parm value found */
      }   /* parm or dep */
      else {
         printf( "*E* Parameter %s is not recognised as a job parameter\n", sTestParmName );
         return( 0 );
      }
      if ((*pSptr == '\0') || (*pSptr == '\n')) { endofdata = 1; }
   }

   /* ----- If we got here we filled the data record with stuff ------ */
   /* Sanity check it before continuing */
#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_updatejobbuffer (starting bulletproof)\n" );
#endif
   if (BULLETPROOF_jobsfile_record( pJobRecord ) == 0 ) {
      /* it will have logged an error */
      return( 0 );
   }

   return( 1 ); /* all OK */
} /* job_updatejobbuffer */


/* *****************************************************************************
   Called for an add job request.
   return 0 if an error.
   ***************************************************************************** */
int job_definition_add( char *pCmdstring, char *pJobname, API_Header_Def *pAPI_buffer ) {

  jobsfile_def *pJobRecord;
  int status, depcount, i;

/* JOB ADD|UPDATE <jobname>,OWNER <user>,TIME <yyyymmdd hh:mm>,CMD <cmd>
   [,PARM "<parm>"][,CAL <cal>]
   [,DEP "JOB <jobn>|FILE <filen>[,...]" total of five dependencies]  
   REPEATEVERY <mins>, DAYS "MON,TUE,...SUN",DESC dddddd
   CATCHUP YES|NO
*/

#ifdef DEBUG_MODE_ON
   status = sizeof(jobsfile_def);
   printf( "DEBUG: in job_definition_add (about to malloc %d bytes)\n", status );
#endif
   if ( (pJobRecord = malloc( sizeof(jobsfile_def ) )) == NULL) {
      printf( "*E* Insufficient free memory available on server to process the job command\n" );
      return( 0 );
   }
   UTILS_zero_fill( (char *)pJobRecord, sizeof(jobsfile_def) );

   /* setup the defaults */
#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_definition_add (setting defaults)\n" );
#endif
   strncpy( pJobRecord->job_header.jobnumber, "000000", JOB_NUMBER_LEN );
   strncpy( pJobRecord->job_header.jobname, pJobname, JOB_NAME_LEN );
   UTILS_space_fill_string( pJobRecord->job_header.jobname, JOB_NAME_LEN );
   pJobRecord->job_header.info_flag = 'A';   /* Active */
   strcpy( pJobRecord->last_runtime, "00000000 00:00:00" );
   strcpy( pJobRecord->next_scheduled_runtime, "00000000 00:00:00" );
   strncpy( pJobRecord->job_owner, "nobody", JOB_OWNER_LEN );             /* default  owner */
   pJobRecord->use_calendar = '0';          /* default is no calendar or day list */
   for (depcount = 0; depcount < MAX_DEPENDENCIES; depcount++) {
      pJobRecord->dependencies[depcount].dependency_type = '0';   /* default is no dependency */
      strcpy( pJobRecord->dependencies[depcount].dependency_name, "" );
   }
   for (i = 0; i < 7; i++) {
	   pJobRecord->crontype_weekdays[i] = '0';
   }
   for (i = 0; i < JOB_REPEATLEN; i++ ); {
      pJobRecord->requeue_mins[i] = '0';
   }
   pJobRecord->job_catchup_allowed = 'Y';
   strcpy( pJobRecord->description, "No description entered" );
   pJobRecord->use_calendar = '0';
   pJobRecord->calendar_error = 'N';
   strcpy( pJobRecord->program_parameters, "" );

#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_definition_add (calling job_updatejobbuffer)\n" );
#endif
   status = job_updatejobbuffer( pCmdstring, pJobRecord );
   /* check for an error */
   if (status == 0) {
#ifdef DEBUG_MODE_ON
printf ("DEBUG:Got error on job_updatejobbuffer\n" );
#endif
      free( pJobRecord );
      return( 0 );
   }

   /* stuff everything into the API buffer now */
   API_init_buffer( pAPI_buffer );
   strcpy( pAPI_buffer->API_System_Name, "JOB" );
   strcpy( pAPI_buffer->API_Command_Number, API_CMD_JOB_ADD );
   memcpy( pAPI_buffer->API_Data_Buffer, pJobRecord, sizeof(jobsfile_def) );
   API_set_datalen( pAPI_buffer, sizeof(jobsfile_def) );

   /* All OK */
   free( pJobRecord );
   return( 1 );
} /* job_definition_add */


/* *****************************************************************************
   Called for an update job request.
   return 0 if an error.
   NOTE: THIS HAS NOT YET BEEN IMPLEMENTED
   ***************************************************************************** */
int job_definition_update( char *pCmdstring, char *pJobname, API_Header_Def *pAPI_buffer ) {

   jobsfile_def *pJobRecord;
   int status;

   printf( "*E* JOB UPDATE is not supported, use DELETE then ADD !\n" );
   return( 0 );

/* JOB UPDATE <jobname>,OWNER <user>,TIME <hh:mm>,CMD <cmd>
   [,PARM "<parm>"][,CAL <cal>]
   [,DEP "JOB <jobn>|FILE <filen>" total of five dependencies]  */

#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_definition_update (about to malloc)\n" );
#endif
   if ((pJobRecord = malloc( sizeof(jobsfile_def))) == NULL) {
      printf( "*E* Insufficient free memory available on server to process the job command\n" );
      return( 0 );
   }
#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_definition_update (done malloc)\n" );
#endif
   UTILS_zero_fill( (char *)pJobRecord, sizeof(jobsfile_def) );

   /* read the existing record - TO DO  maybe i should do this wally */
   
   
   
   
   /* then update from the read record */
   strncpy( pJobRecord->job_header.jobnumber, "000000", JOB_NUMBER_LEN );
   strncpy( pJobRecord->job_header.jobname, pJobname, JOB_NAME_LEN );
   pJobRecord->job_header.info_flag = 'A';   /* Active */
#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_definition_update (calling job_updatejobbuffer)\n" );
#endif
   status = job_updatejobbuffer( pCmdstring, pJobRecord );
   /* check for an error */
   if (status == 0) {
      free( pJobRecord );
      return( 0 );
   }

   /* ---- ok, we are all set to go, lets add it ---- */
   /* stuff everything into the API buffer now */
#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_definition_update (initing buffer)\n" );
#endif
   API_init_buffer( pAPI_buffer );
   strcpy( pAPI_buffer->API_System_Name, "JOB" );
   strcpy( pAPI_buffer->API_Command_Number, API_CMD_UPDATE );
   memcpy( pAPI_buffer->API_Data_Buffer, pJobRecord, sizeof(jobsfile_def) );
   API_set_datalen( pAPI_buffer, sizeof(jobsfile_def) );

   /* all done now, API buffer is built */
   free( pJobRecord );
   return( 1 ); /* all OK */
} /* job_definition_update */


/* *****************************************************************************
   Called to show a job entry in the job databases details.
   return 0 if an error.
   Note: All data will be uppercased OK by now.
   ***************************************************************************** */
int job_definition_showdef( char *pCmdstring, char *pJobname, API_Header_Def *pAPI_buffer ) {

   jobsfile_def *pJobRecord;

/* JOB INFO <jobname> */

#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_definition_showdef (about to malloc)\n" );
#endif
   if ((pJobRecord = malloc( sizeof(jobsfile_def))) == NULL) {
      printf( "*E* Insufficient free memory available on server to process the job command\n" );
      return( 0 );
   }
#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_definition_showdef (done malloc)\n" );
#endif
   UTILS_zero_fill( (char *)pJobRecord, sizeof(jobsfile_def) );

   strncpy( pJobRecord->job_header.jobnumber, "000000", JOB_NUMBER_LEN );
   strncpy( pJobRecord->job_header.jobname, pJobname, JOB_NAME_LEN );
   pJobRecord->job_header.info_flag = 'A';   /* Active */

   /* ---- ok, we are all set to go, lets add it ---- */
   /* stuff everything into the API buffer now */
#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_definition_showdef (initing buffer)\n" );
#endif
   API_init_buffer( pAPI_buffer );
   strcpy( pAPI_buffer->API_System_Name, "JOB" );
   strcpy( pAPI_buffer->API_Command_Number, API_CMD_RETRIEVE );
   memcpy( pAPI_buffer->API_Data_Buffer, pJobRecord, sizeof(jobsfile_def) );
   API_set_datalen( pAPI_buffer, sizeof(jobsfile_def) );

   /* all done now, API buffer is built */
   free( pJobRecord );
   return( 1 ); /* all OK */
} /* job_definition_showdef */


/* *****************************************************************************
   Called to show a job entry in the job databases details.
   return 0 if an error.
   Note: All data will be uppercased OK by now.
   ***************************************************************************** */
int job_submit( char *pCmdstring, char *pJobname, API_Header_Def *pAPI_buffer ) {

   jobsfile_def *pJobRecord;

/* JOB INFO <jobname> */

#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_submit (about to malloc)\n" );
#endif
   if ((pJobRecord = malloc( sizeof(jobsfile_def))) == NULL) {
      printf( "*E* Insufficient free memory available on server to process the job command\n" );
      return( 0 );
   }
#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_submit (done malloc)\n" );
#endif
   UTILS_zero_fill( (char *)pJobRecord, sizeof(jobsfile_def) );

   strncpy( pJobRecord->job_header.jobnumber, "000000", JOB_NUMBER_LEN );
   strncpy( pJobRecord->job_header.jobname, pJobname, JOB_NAME_LEN );
   pJobRecord->job_header.info_flag = 'A';   /* Active */

   /* ---- ok, we are all set to go, lets add it ---- */
   /* stuff everything into the API buffer now */
#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_submit (initing buffer)\n" );
#endif
   API_init_buffer( pAPI_buffer );
   strcpy( pAPI_buffer->API_System_Name, "JOB" );
   strcpy( pAPI_buffer->API_Command_Number, API_CMD_SCHEDULE_ON );
   memcpy( pAPI_buffer->API_Data_Buffer, pJobRecord, sizeof(jobsfile_def) );
   API_set_datalen( pAPI_buffer, sizeof(jobsfile_def) );

   /* all done now, API buffer is built */
   free( pJobRecord );
   return( 1 ); /* all OK */
} /* job_submit */


/* *****************************************************************************
   Called to delete a job entry from the job databases details.
   return 0 if an error.
   Note: All data will be uppercased OK by now.
   ***************************************************************************** */
int job_definition_delete( char *pCmdstring, char *pJobname, API_Header_Def *pAPI_buffer ) {

   jobsfile_def *pJobRecord;

/* JOB DELETE <jobname> */

#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in job_definition_delete\n" );
#endif
   pJobRecord = (jobsfile_def *)&pAPI_buffer->API_Data_Buffer;
   UTILS_zero_fill( (char *)pJobRecord, sizeof(jobsfile_def) );

   API_init_buffer( pAPI_buffer );
   strcpy( pAPI_buffer->API_System_Name, "JOB" );
   strcpy( pAPI_buffer->API_Command_Number, API_CMD_JOB_DELETE );
   strncpy( pJobRecord->job_header.jobnumber, "000000", JOB_NUMBER_LEN );
   strncpy( pJobRecord->job_header.jobname, pJobname, JOB_NAME_LEN );
   pJobRecord->job_header.info_flag = 'A';   /* Active */
   API_set_datalen( pAPI_buffer, sizeof(jobsfile_def) );

   /* all done now, API buffer is built */
   return( 1 ); /* all OK */
} /* job_definition_delete */


/* *****************************************************************************
   return 0 if an error.
   ***************************************************************************** */
int job_command( char *cmdstring, API_Header_Def *pAPI_buffer ) {
   char sTestField[100];
   char sJobName[JOB_NAME_LEN+1];
   int nFieldLen, nLeadingPads, nOverrunFlag;
   char *pSptr;

   pSptr = cmdstring;

   /* skip the job header, we know we are a job request */
   nFieldLen = UTILS_parse_string( pSptr, ' ', (char *)&sTestField, 99, 
                                   &nLeadingPads, &nOverrunFlag );
   if (nFieldLen == 0) {   /* failed */
      printf( "*E* Failed to extract JOB command header.\n" );
      return( 0 );
   }
   pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;

   /* get the job command type */
   nFieldLen = UTILS_parse_string( pSptr, ' ', (char *)&sTestField, 99, 
                                   &nLeadingPads, &nOverrunFlag );
   if (nFieldLen == 0) {  /* failed */
      printf( "*E* Failed to extract command type\n" );
      return( 0 );
   }
   pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;

   /* The command type is safe to uppercase so do so now */
   UTILS_uppercase_string( (char *)&sTestField );

   /* --- if listall then no more work needed --- */
   /* 27th July 2002, added more work, we allow a partial selection mask now */
   if (memcmp(sTestField,"LISTALL",7) == 0) {
      while (*pSptr == ' ') { pSptr++; } /* skip the space thats still in there */
      if ((*pSptr == '\0') || (*pSptr == '\n')) {
         return( generic_listall_cmd( "JOB", pAPI_buffer, "" ) );
      }
      else {
         return( generic_listall_cmd( "JOB", pAPI_buffer, pSptr ) );
      }
   }

   /* --- else all other commands must have a jobname --- */
   while (*pSptr == ' ') { pSptr++; } /* skip the space thats still in there */
   nFieldLen = UTILS_parse_string( pSptr, ',', (char *)&sJobName, JOB_NAME_LEN, 
                                   &nLeadingPads, &nOverrunFlag );
   if (nFieldLen == 0) {
      /* failed */
      printf( "*E* A jobname value is required for job commands.\n" );
      return( 0 );
   }
   pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;

   /* The jobname we enforce as uppercase ALWAYS, so do it now */
   UTILS_uppercase_string( (char *)&sJobName );

   /* --- Call the required function now --- */
   pSptr = pSptr + UTILS_count_delims( pSptr, ',' );
   if (memcmp(sTestField,"ADD",3) == 0) {
      return( job_definition_add( pSptr, (char *)&sJobName, pAPI_buffer ) );
   }
   else if (memcmp(sTestField,"UPDATE",6) == 0) {
      return( job_definition_update( pSptr, (char *)&sJobName, pAPI_buffer ) );
   }
   else if (memcmp(sTestField,"INFO",4) == 0) {
      return( job_definition_showdef( pSptr, (char *)&sJobName, pAPI_buffer ) );
   }
   else if (memcmp(sTestField,"DELETE",6) == 0) {
      return( job_definition_delete( pSptr, (char *)&sJobName, pAPI_buffer ) );
   }
   else if (memcmp(sTestField,"SUBMIT",6) == 0) {
      return( job_submit( pSptr, (char *)&sJobName, pAPI_buffer ) );
   }
   else {
      printf( "*E* That JOB Command (%s) is not supported yet\n", sTestField );
	  printf( "    Options are : ADD <jobname>, UPDATE <jobname>, INFO <jobname>, DELETE <jobname>, SUBMIT <jobname>\n" );
      return( 0 );
   }
} /* job_command */

/* -----------------------------------------------------------------------------

    C a l e n d a r     C o m m a n d     P r o c e s s i n g     R o u t i n e s

   ----------------------------------------------------------------------------- */

/* *****************************************************************************
   return 0 if an error.
   buffer_type = : 9 listall, 0 info, 1 delete, 2 retrieve-raw
   ***************************************************************************** */
int calendar_header_send( char *pCmdstring, char *pCalname, API_Header_Def *pAPI_buffer, int buffer_type ) {

  calendar_def *calbuffer;
  char *pSptr, *workptr;
  char caltype;
  char year[5]; /* 4 + pad */
  char sTestParmName[100];
  char sTestParmValue[150];
  int nFieldLen, nLeadingPads, nOverrunFlag, endofdata, testnum;

 /*   CALENDAR INFO|DELETE <calname>,TYPE JOB|HOLIDAY,YEAR YYYY */
 /*   CALENDAR LISTALL,TYPE JOB|HOLIDAY [,YEAR YYYY] - note year ignored for
  *   listall at present */
/*
printf( "DEBUG: calendar_header_send : Calname = %s, buffer_type = %d, data=%s\n", pCalname, buffer_type, pCmdstring );
*/

   if (buffer_type == 9) { /* listall is a special case */
      API_init_buffer( pAPI_buffer );
      strcpy( pAPI_buffer->API_System_Name, "CAL" );
      calbuffer = (calendar_def *)&pAPI_buffer->API_Data_Buffer;
      strcpy(calbuffer->calendar_name, "" );
      calbuffer->calendar_type = '0';  /* ignored by the server listall */
      strcpy(calbuffer->year, "0000" ); /* ignored by the listall */
      strcpy( pAPI_buffer->API_Command_Number, API_CMD_LISTALL );
      API_set_datalen( pAPI_buffer, sizeof(calendar_def) );
      return( 1 );  /* OK to send it off */
   }

   /* loop until we reach the end of the command string passed */
   strcpy( year, "2002" ); /* default to ensure not uninitialised as listall doesn't provide one */
   endofdata = 0;
   pSptr = pCmdstring;
   while (endofdata == 0) {
      /* --- find the field type --- */
      nFieldLen = UTILS_parse_string( pSptr, ' ', (char *)&sTestParmName, 99, 
                                      &nLeadingPads, &nOverrunFlag );
      if (nFieldLen == 0) {
         endofdata = 1;
      }
      else {
        pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;
      }

      /* --- find the field data --- */
      /* uppercase the parameter value to search on */
      workptr = (char *)&sTestParmName;
      while ( (*workptr != '\0') && (*workptr != '\n') ) {
         *workptr = toupper( *workptr );
         workptr++;
      }

	  /*
	   * all fields here are up until the next comma.
	   */
      pSptr = pSptr + UTILS_count_delims( pSptr, ' ' );
      nFieldLen = UTILS_parse_string( pSptr, ',', (char *)&sTestParmValue, 99, 
                                      &nLeadingPads, &nOverrunFlag );
      if (nFieldLen == 0) {
         printf( "*E* Missing parameter value for field '%s'\n", sTestParmName );
         return( 0 );
      }
      else {
           pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag; /* move ptr past value */
           pSptr = pSptr + UTILS_count_delims( pSptr, ',' );        /* skip any additional commas */
           workptr = (char *)&sTestParmValue;
           while ( (*workptr != '\0') && (*workptr != '\n') ) {
              *workptr = toupper( *workptr );
              workptr++;
           }
           if (memcmp(sTestParmName, "TYPE", 4) == 0) {
              if (memcmp(sTestParmValue,"JOB",3) == 0) {
                 caltype = '0'; /* job record type */
              }
			  else if (memcmp(sTestParmValue,"HOL",3) == 0) {
                 caltype = '1'; /* holiday record type */
              }
			  else {
                 printf( "*E* '%s' is an invalid value for TYPE\n", sTestParmValue );
                 return( 0 );
              }
           }
		   else if (memcmp(sTestParmName, "YEAR", 4) == 0) {
              if (strlen(sTestParmValue) != 4) {
                 printf( "*E* '%s' is an invalid value for YEAR\n", sTestParmValue );
                 return( 0 );
              }
              testnum = atoi(sTestParmValue);
              if ( (testnum < 2000) || (testnum > 9999) ) {
                 printf( "*E* '%s' is an out of range value for YEAR\n", sTestParmValue );
                 return( 0 );
              }
              strncpy( year, sTestParmValue, 4 );
           }
		   else {
              printf( "*E* Paremeter '%s' is unexpected in this command\n", sTestParmName );
              return( 0 );
           }
      }
      pSptr = pSptr + UTILS_count_delims( pSptr, ' ' );  /* skip any trailing spaces */
      if ((*pSptr == '\0') || (*pSptr == '\n')) { endofdata = 1; }  /* end of command now */
   } /* while endofdata == 0 */

   /*
	* we now have all the parameters we want so send build the buffer and
	* return OK
	*/
   API_init_buffer( pAPI_buffer );
   strcpy( pAPI_buffer->API_System_Name, "CAL" );
   calbuffer = (calendar_def *)&pAPI_buffer->API_Data_Buffer;
   strncpy(calbuffer->calendar_name, pCalname, CALENDAR_NAME_LEN );
   calbuffer->calendar_type = caltype;
   strncpy(calbuffer->year, year, 4 );
   if (buffer_type == 0) {
      strcpy( pAPI_buffer->API_Command_Number, API_CMD_RETRIEVE );
   }
   else if (buffer_type == 1) {
      strcpy( pAPI_buffer->API_Command_Number, API_CMD_CALENDAR_DELETE );
   }
   else if (buffer_type == 2) {
      strcpy( pAPI_buffer->API_Command_Number, API_CMD_RETRIEVE_RAW );
   }
   else {
      printf( "*E* Invalid parameters in call to calendar_send_header, programmer error\n" );
      return( 0 );
   }
   API_set_datalen( pAPI_buffer, sizeof(calendar_def) );

   /* all done now, API buffer is built */
   return( 1 ); /* all OK */
} /* calendar_header_send */

/* *****************************************************************************
 * Datelist = MM/DD,MM/DD,MM/DD,MM/DD,MM/DD,MM/DD...
 * Return 0 if an error
 * Return 1 if OK
   ***************************************************************************** */
int calendar_process_datelist( calendar_def * calbuffer, char * datelist ) {
   char *ptr;
   char strmonth[3], strday[3];
   int  month,day;
   int eoline;

/* Tested OK
printf( "DEBUG: calendar_process_datelist : Calname = %s, date list = %s\n", calbuffer->calendar_name, datelist );
*/

   eoline = 0;
   ptr = datelist;
   while (eoline == 0) {
      if (ptr[1] == '/') {
         strncpy( strmonth, ptr, 1 );
         strmonth[1] = '\0';
		 ptr = ptr + 2;
         if ( (ptr[1] == ',') || (ptr[1] == '\n')  || (ptr[1] == '\0') ) {
            strncpy( strday, ptr, 1 );
            strday[1] = '\0';
		    ptr = ptr + 1;
         }
		 else if ( (ptr[2] == ',') || (ptr[2] == '\n')  || (ptr[2] == '\0') ) {
            strncpy( strday, ptr, 2 );
            strday[2] = '\0';
		    ptr = ptr + 2;
         }
		 else {
            printf( "*E* Bad date format, found at %s\n", ptr );
            return( 0 );
         }
      }
      else if (ptr[2] == '/') {
         strncpy( strmonth, ptr, 2 );
         strmonth[2] = '\0';
		 ptr = ptr + 3;
         if ( (ptr[1] == ',') || (ptr[1] == '\n')  || (ptr[1] == '\0') ) {
            strncpy( strday, ptr, 1 );
            strday[1] = '\0';
		    ptr = ptr + 1;
         }
		 else if ( (ptr[2] == ',') || (ptr[2] == '\n')  || (ptr[2] == '\0') ) {
            strncpy( strday, ptr, 2 );
            strday[2] = '\0';
		    ptr = ptr + 2;
         }
		 else {
            printf( "*E* Bad date format, found at %s\n", ptr );
            return( 0 );
         }
      }
      else {  /* No / found within range */
         printf( "*E* No / found in date, entry found at %s\n", ptr );
         return( 0 );
      }
      /* Check the numbers */
      month = atoi(strmonth);
      day = atoi(strday);
      if ((month < 1) || (month > 12)) {
         printf( "*E* Month value is out of range, entry %s/%s\n", strmonth, strday );
         return( 0 );
      }
      if ((day < 1) || (day > 31)) {
         printf( "*E* Day value is out of range, entry %s/%s\n", strmonth, strday );
         return( 0 );
      }
      day = day - 1;
      month = month - 1;
      calbuffer->yearly_table.month[month].days[day] = 'Y';
	  /* Were we done ? */
	  while ( (*ptr == ',') || (*ptr == ' ') ) { ptr++; }  /* skip to next field of null */
      if ( (*ptr == '\n')  || (*ptr == '\0') ) { eoline = 1; }
   } /* while eoline == 0 */

   /* DEBUG_CALENDAR_dump_to_stdout( calbuffer ); done testing */

   /* Completed */
   return( 1 );
} /* calendar_process_datelist */

/* *****************************************************************************
 * Monthname = ALL|JAN|FEB...DEC
 * Daylist =  DD,DD,DD,DD...
 * Return 0 if an error
 * Return 1 if OK
   ***************************************************************************** */
int calendar_monthly_daynumbers( calendar_def * calbuffer, char * monthname, char * daylist ) {
   int eolist, i, monthnum, daynum;
   char *ptr, strday[3];

   /*
   printf( "DEBUG: calendar_monthly_daynumbers: CalName = %s, month = %s, list = %s\n",
				   calbuffer->calendar_name, monthname, daylist );
   */

   if (memcmp(monthname,"ALL",3) == 0) {  /* for all months */
      monthnum = 99;
   }
   else if (memcmp(monthname,"JAN",3) == 0) { 
      monthnum = 0;
   }
   else if (memcmp(monthname,"FEB",3) == 0) { 
      monthnum = 1;
   }
   else if (memcmp(monthname,"MAR",3) == 0) { 
      monthnum = 2;
   }
   else if (memcmp(monthname,"APR",3) == 0) { 
      monthnum = 3;
   }
   else if (memcmp(monthname,"MAY",3) == 0) { 
      monthnum = 4;
   }
   else if (memcmp(monthname,"JUN",3) == 0) { 
      monthnum = 5;
   }
   else if (memcmp(monthname,"JUL",3) == 0) { 
      monthnum = 6;
   }
   else if (memcmp(monthname,"AUG",3) == 0) { 
      monthnum = 7;
   }
   else if (memcmp(monthname,"SEP",3) == 0) { 
      monthnum = 8;
   }
   else if (memcmp(monthname,"OCT",3) == 0) { 
      monthnum = 9;
   }
   else if (memcmp(monthname,"NOV",3) == 0) { 
      monthnum = 10;
   }
   else if (memcmp(monthname,"DEC",3) == 0) { 
      monthnum = 11;
   }
   else { 
     printf( "*E* %s is not a legal month\n", monthname );
     return( 0 );
   }

   eolist = 0;
   ptr = daylist;
   while (eolist == 0) {
      if ( (ptr[1] == ',') || (ptr[1] == ' ') || (ptr[1] == '\0') || (ptr[1] == '\n') ) {
         strncpy(strday,ptr,1);
         strday[1] = '\0';
         ptr++;
      }
	  else if ( (ptr[2] == ',') || (ptr[2] == ' ') || (ptr[2] == '\0') || (ptr[2] == '\n') ) {
         strncpy(strday,ptr,2);
         strday[2] = '\0';
         ptr = ptr + 2;
      }
      else { 
        printf( "*E* Illegal day number found, position %s\n", ptr );
        return( 0 );
      }
      daynum = atoi(strday);
      if ( (daynum < 1) || (daynum > 31) ) { 
        printf( "*E* Illegal day number found, value %s\n", strday );
        return( 0 );
      }
      daynum = daynum - 1;   /* our table offsets are from 0 */
      if (monthnum < 12) {
         calbuffer->yearly_table.month[monthnum].days[daynum] = 'Y';
      }
      else {
         for (i = 0; i < 12; i++) {
            calbuffer->yearly_table.month[i].days[daynum] = 'Y';
         }
      }

	  while ( (*ptr == ',') || (*ptr == ' ') ) { ptr++; }
	  if ( (*ptr == '\0') || (*ptr == '\n') ) { eolist = 1; }
   } /* while eolist == 0 */

   return( 1 );
} /* calendar_monthly_daynumbers */

/* *****************************************************************************
 * calbuffer = calendar_def record, MUST have year already set
 * Daylist = SUN,MON,TUE,WED,THU,FRI,SAT
 * Return 0 if an error
 * Return 1 if OK
   ***************************************************************************** */
int calendar_process_daylist( calendar_def * calbuffer, char * daylist ) {
   char days_required[7];
   int i;
   char *ptr;

   /*
   printf( "DEBUG: calendar_process_datelist : Calname = %s, type %c, year %s, date list = %s\n",
           calbuffer->calendar_name, calbuffer->calendar_type, calbuffer->year, daylist );
   */

   /* Initialise the table to contain no days required, then we go on to set
	* them */
   for (i = 0; i < 7; i++) { days_required[i] = 'N'; }

   /* Parse the day list provided and build the list of days we want */
   ptr = daylist;
   while (*ptr == ' ') { ptr++; }
   while ( (*ptr != '\0') && (*ptr != '\n') ) {
      if (memcmp(ptr,"SUN",3) == 0) { i = 0; }
	  else if (memcmp(ptr,"MON",3) == 0) { i = 1; }
	  else if (memcmp(ptr,"TUE",3) == 0) { i = 2; }
	  else if (memcmp(ptr,"WED",3) == 0) { i = 3; }
	  else if (memcmp(ptr,"THU",3) == 0) { i = 4; }
	  else if (memcmp(ptr,"FRI",3) == 0) { i = 5; }
	  else if (memcmp(ptr,"SAT",3) == 0) { i = 6; }
	  else {
         printf( "*E* Day name not recognised, found at...\n   %s\n", ptr );
         return( 0 );
      }
      days_required[i] = 'Y';                                        /* set that we want this day */
      while ( (*ptr != ',') && (*ptr != '\0') && (*ptr != '\n') ) { ptr++; } /* find end of field */
      while ( (*ptr == ' ') || (*ptr == ',') ) { ptr++; }                /* skip , and any spaces */
   }

   if (CALENDAR_UTILS_populate_calendar_by_day( (char *)&days_required, calbuffer ) != 1) {
      /* If the dayname list is provided before the year we don't have a legal
	   * year to work with which may cause this error */
      printf( "*E* Request is not valid, check daynames and year (year must be before daynames)\n" );
      return( 0 );
   }

   return( 1 );
} /* calendar_process_daylist */

/* *****************************************************************************
   return 0 if an error.
   buffer_type = 0 add, 1 merge, 2 un-merge, 3 rawdata-retrieve
   ***************************************************************************** */
int calendar_definition_send( char *pCmdstring, char *pCalname, API_Header_Def *pAPI_buffer, int buffer_type ) {

  calendar_def *calbuffer;
  char sTestParmName[100];
  char sTestParmValue[150];
  int nFieldLen, nLeadingPads, nOverrunFlag, endofdata, testnum;
  char *pSptr, *workptr;

   pSptr = pCmdstring;
/*
 *       CALENDAR ADD|MERGE|UNMERGE <calname>,TYPE JOB|HOLIDAY,YEAR YYYY,
 *            DESC "XXXXX",              (only with add)
 *            HOLCAL <holiday-cal-name>, (only with add)
 *            FORMAT MONTHSDAYS "MM/DD,MM/DD,MM/DD,MM/DD,MM/DD,MM/DD..."
 *            FORMAT DAYS ALL|JAN|FEB...DEC "DD,DD,DD,DD..."
 *            FORMAT DAYNAMES "SUN,MON,TUE,WED,THU,FRI,SAT"
 */

   if ( (calbuffer = malloc( sizeof(calendar_def ) )) == NULL) {
      printf( "*E* Insufficient free memory available to process the calendar command\n" );
      return( 0 );
   }
   UTILS_zero_fill( (char *)calbuffer, sizeof(calendar_def) );

   /* setup the defaults */
   BULLETPROOF_CALENDAR_init_record_defaults( calbuffer );
   strncpy( calbuffer->calendar_name, pCalname, CALENDAR_NAME_LEN );
   UTILS_space_fill_string( calbuffer->calendar_name, CALENDAR_NAME_LEN );

   /* Now set some values from what we were passed */

   /* loop until we reach the end of the command string passed */
   endofdata = 0;
   while (endofdata == 0) {
      /* --- find the field type --- */
      nFieldLen = UTILS_parse_string( pSptr, ' ', (char *)&sTestParmName, 99, 
                                      &nLeadingPads, &nOverrunFlag );
      if (nFieldLen == 0) {
         endofdata = 1;
      }
      else {
        pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;
      }
      /* --- find the field data --- */
      workptr = (char *)&sTestParmName;
      while ( (*workptr != '\0') && (*workptr != '\n') ) {
         *workptr = toupper( *workptr );
         workptr++;
      }

	  /*
	   * Eveything but format and desc is up until the next comma.
	   */
      if ( (memcmp(sTestParmName, "FORMAT", 6) != 0) &&
           (memcmp(sTestParmName, "DESC", 4) != 0) ) {
         /* Add this check as I kept forgetting to enter FORMAT */
         if ( (memcmp(sTestParmName, "TYPE",4) != 0) &&
              (memcmp(sTestParmName, "YEAR",4) != 0) &&
              (memcmp(sTestParmName, "HOLCAL",6) != 0) 
            ) { 
            printf( "*E* Paremeter %s is not recognised in this context (format option ?)\n", sTestParmName );
            free(calbuffer);
            return( 0 );
         }
         /* Then the original stuff */
         pSptr = pSptr + UTILS_count_delims( pSptr, ' ' );
         nFieldLen = UTILS_parse_string( pSptr, ',', (char *)&sTestParmValue, 99, 
                                         &nLeadingPads, &nOverrunFlag );
         if (nFieldLen == 0) {
            printf( "*E* Missing parameter value for field %s\n", sTestParmName );
            free(calbuffer);
            return( 0 );
         }
         else {
            pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;
            pSptr = pSptr + UTILS_count_delims( pSptr, ',' );
            workptr = (char *)&sTestParmValue;
            while ( (*workptr != '\0') && (*workptr != '\n') ) {
               *workptr = toupper( *workptr );
               workptr++;
            }
            if (memcmp(sTestParmName, "TYPE", 4) == 0) {
              if (memcmp(sTestParmValue,"JOB",3) == 0) {
                 calbuffer->calendar_type = '0'; /* job record type */
              }
			  else if (memcmp(sTestParmValue,"HOL",3) == 0) {
                 calbuffer->calendar_type = '1'; /* holiday record type */
              }
			  else {
                 printf( "*E* '%s' is an invalid value for TYPE\n", sTestParmValue );
                 free(calbuffer);
                 return( 0 );
              }
            } /* TYPE */
			else if (memcmp(sTestParmName, "YEAR", 4) == 0) {
              if (strlen(sTestParmValue) != 4) {
                 printf( "*E* '%s' is an invalid value for YEAR\n", sTestParmValue );
                 free(calbuffer);
                 return( 0 );
              }
              testnum = atoi(sTestParmValue);
              if ( (testnum < 2000) || (testnum > 9999) ) {
                 printf( "*E* '%s' is an out of range value for YEAR\n", sTestParmValue );
                 free(calbuffer);
                 return( 0 );
              }
              strncpy( calbuffer->year, sTestParmValue, 4 );
            } /* YEAR */
			else if (memcmp(sTestParmName, "HOLCAL", 6) == 0) {
              strncpy( calbuffer->holiday_calendar_to_use, sTestParmValue, CALENDAR_NAME_LEN );
              UTILS_space_fill_string( calbuffer->holiday_calendar_to_use, CALENDAR_NAME_LEN );
              calbuffer->observe_holidays = 'Y';
            } /* HOLCAL */
         }  /* fieldlen <> 0 */
      }   /* if <> FORMAT and <> DESC */

	  /*
	   * A DESC request has comma seperated fields between " bytes
	  */
	  else if (memcmp(sTestParmName, "DESC", 4) == 0) {
         pSptr = pSptr + UTILS_count_delims( pSptr, ' ' );
         nFieldLen = UTILS_parse_string( pSptr, '"', (char *)&sTestParmValue, 149, 
                                         &nLeadingPads, &nOverrunFlag );
         if (nFieldLen == 0) {
            printf( "*E* Missing parameter value for field %s\n", sTestParmName );
            free(calbuffer);
            return( 0 );
         }
         else {
            pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;
            pSptr = pSptr + UTILS_count_delims( pSptr, '"' );
            pSptr = pSptr + UTILS_count_delims( pSptr, ',' );
            strncpy( calbuffer->description, sTestParmValue, CALENDAR_DESC_LEN );
         }  /* else parm value found */
      }   /* DESC */

	  /*
	   * Format commands are tricky, use seperate procs
	   */
	  else if (memcmp(sTestParmName, "FORMAT", 4) == 0) {
         nFieldLen = UTILS_parse_string( pSptr, ' ', (char *)&sTestParmName, 99, 
                                         &nLeadingPads, &nOverrunFlag );
         if (nFieldLen == 0) { endofdata = 1; }
		 else { pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag; }
         workptr = (char *)&sTestParmName;
         while ( (*workptr != '\0') && (*workptr != '\n') ) {
            *workptr = toupper( *workptr );
            workptr++;
         }
/*
 *            FORMAT MONTHSDAYS "MM/DD,MM/DD,MM/DD,MM/DD,MM/DD,MM/DD..."
 *            FORMAT DAYS ALL|JAN|FEB...DEC "DD,DD,DD,DD..."
 *            FORMAT DAYNAMES "SUN,MON,TUE,WED,THU,FRI,SAT"
 */
		 if (memcmp(sTestParmName,"MONTHSDAYS",10) == 0) {
            pSptr = pSptr + UTILS_count_delims( pSptr, ' ' );
            nFieldLen = UTILS_parse_string( pSptr, '"', (char *)&sTestParmValue, 149, 
                                            &nLeadingPads, &nOverrunFlag );
            if (nFieldLen == 0) {
               printf( "*E* Missing parameter value for field %s\n", sTestParmName );
               free(calbuffer);
               return( 0 );
            }
            else {
		        pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;
                pSptr = pSptr + UTILS_count_delims( pSptr, '"' );
                pSptr = pSptr + UTILS_count_delims( pSptr, ' ' );
                pSptr = pSptr + UTILS_count_delims( pSptr, ',' );
                pSptr = pSptr + UTILS_count_delims( pSptr, ' ' );
                /* Process the day list */
                if (calendar_process_datelist( calbuffer, (char *)&sTestParmValue ) == 0) {
                   free(calbuffer);
                   return( 0 );
                }
            }
         }
         else if (memcmp(sTestParmName,"DAYS",4) == 0) {
               /* Days has another keyword we need to pull out */
               nFieldLen = UTILS_parse_string( pSptr, ' ', (char *)&sTestParmName, 99, 
                                               &nLeadingPads, &nOverrunFlag );
               if (nFieldLen == 0) { endofdata = 1; }
		       pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;
               pSptr = pSptr + UTILS_count_delims( pSptr, '"' );
               workptr = (char *)&sTestParmName;
               while ( (*workptr != '\0') && (*workptr != '\n') ) {
                  *workptr = toupper( *workptr );
                  workptr++;
               }
               pSptr = pSptr + UTILS_count_delims( pSptr, ' ' );
               nFieldLen = UTILS_parse_string( pSptr, '"', (char *)&sTestParmValue, 149, 
                                               &nLeadingPads, &nOverrunFlag );
               if (nFieldLen == 0) {
                  printf( "*E* Missing parameter value for field FORMAT %s\n", sTestParmName );
                  free(calbuffer);
                  return( 0 );
               }
               else {
                   /* Process the day list */
		           pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;
                   pSptr = pSptr + UTILS_count_delims( pSptr, '"' );
                   pSptr = pSptr + UTILS_count_delims( pSptr, ',' );
                   if (calendar_monthly_daynumbers( calbuffer, (char *)&sTestParmName, (char *)&sTestParmValue ) == 0) {
                      free(calbuffer);
                      return( 0 );
                   }
               }
         }
         else if (memcmp(sTestParmName,"DAYNAMES",8) == 0) {
            pSptr = pSptr + UTILS_count_delims( pSptr, ' ' );
            nFieldLen = UTILS_parse_string( pSptr, '"', (char *)&sTestParmValue, 149, 
                                            &nLeadingPads, &nOverrunFlag );
            if (nFieldLen == 0) {
               printf( "*E* Missing parameter value for field %s\n", sTestParmName );
               free(calbuffer);
               return( 0 );
            }
            else {
                /* Process the day list */
                workptr = (char *)&sTestParmValue;
                while ( (*workptr != '\0') && (*workptr != '\n') ) {
                   *workptr = toupper( *workptr );
                   workptr++;
                }
                if (calendar_process_daylist( calbuffer, (char *)&sTestParmValue ) == 0) {
                   free(calbuffer);
                   return( 0 );
                }
            }
         }
         else {
            printf( "*E* Unknown parameter name, %s\n", sTestParmName );
            free(calbuffer);
            return( 0 );
         }
      }

	  /*
	   * else an unknown 
	   */
      else {
         printf( "*E* Parameter %s is not recognised as a calendar parameter\n", sTestParmName );
         free(calbuffer);
         return( 0 );
      }
      if ((*pSptr == '\0') || (*pSptr == '\n')) { endofdata = 1; }
   } /* while not endofdata */


   /*
	* stuff everything into the API buffer now
	*/
   API_init_buffer( pAPI_buffer );
   strcpy( pAPI_buffer->API_System_Name, "CAL" );
   if (buffer_type == 0) {
      strcpy( pAPI_buffer->API_Command_Number, API_CMD_CALENDAR_ADD );
   }
   else if (buffer_type == 1) {
      strcpy( pAPI_buffer->API_Command_Number, API_CMD_CALENDAR_MERGE );
   }
   else if (buffer_type == 2) {
      strcpy( pAPI_buffer->API_Command_Number, API_CMD_CALENDAR_UNMERGE );
   }
   else {
      printf( "*E* Invalid parameters passed to calendar_definition_send, programmer error\n" );
      free( calbuffer );
      return( 0 );
   }
   if (BULLETPROOF_CALENDAR_record( calbuffer ) == 0) {
      printf( "*E* Incomplete or invalid data in calendar entry\n" );
      free( calbuffer );
      return( 0 );
   }

   /* All OK */
   memcpy( pAPI_buffer->API_Data_Buffer, calbuffer, sizeof(calendar_def) );
   API_set_datalen( pAPI_buffer, sizeof(calendar_def) );
   free( calbuffer );
   return( 1 );
} /* calendar_definition_send */

/* *****************************************************************************
   return 0 if an error.
   ***************************************************************************** */
int calendar_command( char *cmdstring, API_Header_Def *pAPI_buffer ) {
   char sTestField[100];
   char sCalName[CALENDAR_NAME_LEN+1];
   int nFieldLen, nLeadingPads, nOverrunFlag;
   char *pSptr;

   pSptr = cmdstring;

   /* skip the job header, we know we are a job request */
   nFieldLen = UTILS_parse_string( pSptr, ' ', (char *)&sTestField, 99, 
                                   &nLeadingPads, &nOverrunFlag );
   if (nFieldLen == 0) {   /* failed */
      printf( "*E* Failed to extract CALENDAR command header.\n" );
      return( 0 );
   }
   pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;

   /* get the job command type */
   nFieldLen = UTILS_parse_string( pSptr, ' ', (char *)&sTestField, 99, 
                                   &nLeadingPads, &nOverrunFlag );
   if (nFieldLen == 0) {  /* failed */
      printf( "*E* Failed to extract command type\n" );
      return( 0 );
   }
   pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;

   /* The command type is safe to uppercase so do so now */
   UTILS_uppercase_string( (char *)&sTestField );

   /* --- if listall then no calendar name so call now --- */
   if (memcmp(sTestField,"LISTALL",7) == 0) {
      while (*pSptr == ',') { pSptr++; } /* skip the comma */
      while (*pSptr == ' ') { pSptr++; } /* skip any imbedded spaces */
      strcpy( sCalName, "" );  /* No calendar name for the listall command */
      return( calendar_header_send( pSptr, (char *)&sCalName, pAPI_buffer, 9 /* listall */ ) );
   }

   /* --- else all other commands must have a calendar name --- */
   while (*pSptr == ' ') { pSptr++; } /* skip the space thats still in there */
   nFieldLen = UTILS_parse_string( pSptr, ',', (char *)&sCalName, CALENDAR_NAME_LEN, 
                                   &nLeadingPads, &nOverrunFlag );
   if (nFieldLen == 0) {
      /* failed */
      printf( "*E* A calendar name is required for calendar commands.\n" );
      return( 0 );
   }
   pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;

   /* The calendar name we enforce as uppercase ALWAYS, so do it now */
   UTILS_uppercase_string( (char *)&sCalName );

   /* --- Call the required function now --- */
   pSptr = pSptr + UTILS_count_delims( pSptr, ',' );
   if (memcmp(sTestField,"ADD",3) == 0) {
      return( calendar_definition_send( pSptr, (char *)&sCalName, pAPI_buffer, 0 /* add */ ) );
   }
   else if (memcmp(sTestField,"MERGE",5) == 0) {
      return( calendar_definition_send( pSptr, (char *)&sCalName, pAPI_buffer, 1 /* merge */ ) );
   }
   else if ( (memcmp(sTestField,"UNMERGE",7) == 0) || (memcmp(sTestField,"SUBDELETE",9) == 0) ) {
      return( calendar_definition_send( pSptr, (char *)&sCalName, pAPI_buffer, 2 /* un-merge */ ) );
   }
   else if (memcmp(sTestField,"INFO",4) == 0) {
/*      return( calendar_header_send( pSptr, (char *)&sCalName, pAPI_buffer, 0 ) ); 0=info */
      return( calendar_header_send( pSptr, (char *)&sCalName, pAPI_buffer, 2 /* retrieve-raw */ ) ); 
   }
   else if (memcmp(sTestField,"DELETE",6) == 0) {
      return( calendar_header_send( pSptr, (char *)&sCalName, pAPI_buffer, 1 /* delete */ ) );
   }
   else {
      printf( "*E* That CALENDAR Command (%s) is not supported yet\n", sTestField );
	  printf( "    Options are : ADD, MERGE, UNMERGE | SUBDELETE, DELETE, INFO or LISTALL\n" );
      return( 0 );
   }
} /* calendar_command */

/* -----------------------------------------------------------------------------

    S c h e d u l e r   C o m m a n d     P r o c e s s i n g     R o u t i n e s

   ----------------------------------------------------------------------------- */

/* *****************************************************************************
   return 0 if an error.
   ***************************************************************************** */
int sched_command( char *cmdstring, API_Header_Def *pApi_buffer ) {
   int nStatus;
   char *pSptr;
   job_header_def * job_header;
   
   pSptr = cmdstring;
   nStatus = 1;

   if (memcmp(cmdstring,"SCHED ALERT FORWARDING",22) != 0) { /* no case sensitive parms */
       UTILS_uppercase_string( cmdstring );          /* so uppercase the whole string now    */
   }

   strcpy( pApi_buffer->API_System_Name, "SCHED" );
   if (memcmp(cmdstring,"SCHED SHUTDOWN FORCEDOWN",24) == 0) {
      printf( "Issuing forced shutdown, all running scheduler jobs will be killed at put into\nalert status...\n" );
      strcpy( pApi_buffer->API_Command_Number, API_CMD_SHUTDOWNFORCE );
   }
   else if (memcmp(cmdstring,"SCHED SHUTDOWN",14) == 0) {
      strcpy( pApi_buffer->API_Command_Number, API_CMD_SHUTDOWN );
   }
   else if (memcmp(cmdstring,"SCHED STATUS SHORT",18) == 0) {
      strcpy( pApi_buffer->API_Command_Number, API_CMD_STATUS_SHORT );
   }
   else if (memcmp(cmdstring,"SCHED STATUS MEM",16) == 0) {
      strcpy( pApi_buffer->API_Command_Number, API_CMD_STATUS );
      strcpy( pApi_buffer->API_Data_Buffer, "MEMINFO" );
	  API_set_datalen( pApi_buffer, sizeof(job_header_def) );
   }
   else if (memcmp(cmdstring,"SCHED STATUS ALERTCFG",21) == 0) {
      strcpy( pApi_buffer->API_Command_Number, API_CMD_STATUS );
      strcpy( pApi_buffer->API_Data_Buffer, "ALERTINFO" );
	  API_set_datalen( pApi_buffer, sizeof(job_header_def) );
   }
   else if (memcmp(cmdstring,"SCHED STATUS",12) == 0) {
      strcpy( pApi_buffer->API_Command_Number, API_CMD_STATUS );
      strcpy( pApi_buffer->API_Data_Buffer, "STANDARD" );
	  API_set_datalen( pApi_buffer, sizeof(job_header_def) );
   }
   else if (memcmp(cmdstring,"SCHED LISTALL",13) == 0) {
      /* 27th July 2002, added more work, we allow a partial selection mask now */
      pSptr = pSptr + 13;
	  while (*pSptr == ' ') { pSptr++; }
      if ((*pSptr == '\0') || (*pSptr == '\n')) {
         return( generic_listall_cmd( "SCHED", pApi_buffer, "" ) );
      }
      else {
         return( generic_listall_cmd( "SCHED", pApi_buffer, pSptr ) );
      }
   }
   else if (memcmp(cmdstring,"SCHED DELETE",12) == 0) {
      pSptr = pSptr + 12;
	  while (*pSptr == ' ') { pSptr++; }
      strcpy( pApi_buffer->API_Command_Number, API_CMD_DELETE );
	  job_header = (job_header_def *)&pApi_buffer->API_Data_Buffer;
	  strncpy(job_header->jobnumber,"000000",JOB_NUMBER_LEN);
	  strncpy(job_header->jobname, pSptr, JOB_NAME_LEN );
	  UTILS_space_fill_string( (char *)&job_header->jobname, JOB_NAME_LEN );
	  job_header->info_flag = 'A';
	  API_set_datalen( pApi_buffer, sizeof(job_header_def) );
   }
   else if (memcmp(cmdstring,"SCHED RUNNOW",12) == 0) {
      pSptr = pSptr + 12;
	  while (*pSptr == ' ') { pSptr++; }
      strcpy( pApi_buffer->API_Command_Number, API_CMD_RUNJOBNOW );
	  job_header = (job_header_def *)&pApi_buffer->API_Data_Buffer;
	  strncpy(job_header->jobnumber,"000000",JOB_NUMBER_LEN);
	  strncpy(job_header->jobname, pSptr, JOB_NAME_LEN );
	  UTILS_space_fill_string( (char *)&job_header->jobname, JOB_NAME_LEN );
	  job_header->info_flag = 'A';
	  API_set_datalen( pApi_buffer, sizeof(job_header_def) );
   }
   else if (memcmp(cmdstring,"SCHED HOLD-ON",13) == 0) {
      pSptr = pSptr + 13;
	  while (*pSptr == ' ') { pSptr++; }
      strcpy( pApi_buffer->API_Command_Number, API_CMD_JOBHOLD_ON );
	  job_header = (job_header_def *)&pApi_buffer->API_Data_Buffer;
	  strncpy(job_header->jobnumber,"000000",JOB_NUMBER_LEN);
	  strncpy(job_header->jobname, pSptr, JOB_NAME_LEN );
	  UTILS_space_fill_string( (char *)&job_header->jobname, JOB_NAME_LEN );
	  job_header->info_flag = 'A';
	  API_set_datalen( pApi_buffer, sizeof(job_header_def) );
   }
   else if (memcmp(cmdstring,"SCHED HOLD-OFF",14) == 0) {
      pSptr = pSptr + 14;
	  while (*pSptr == ' ') { pSptr++; }
      strcpy( pApi_buffer->API_Command_Number, API_CMD_JOBHOLD_OFF );
	  job_header = (job_header_def *)&pApi_buffer->API_Data_Buffer;
	  strncpy(job_header->jobnumber,"000000",JOB_NUMBER_LEN);
	  strncpy(job_header->jobname, pSptr, JOB_NAME_LEN );
	  UTILS_space_fill_string( (char *)&job_header->jobname, JOB_NAME_LEN );
	  job_header->info_flag = 'A';
	  API_set_datalen( pApi_buffer, sizeof(job_header_def) );
   }
   else if (memcmp(cmdstring,"SCHED NEWDAYPAUSEACTION",23) == 0) {
      strcpy( pApi_buffer->API_System_Name, "CONFG" );
      pSptr = pSptr + 23;
	  while (*pSptr == ' ') { pSptr++; }
	  if ( (memcmp(pSptr,"ALERT",5) != 0) && (memcmp(pSptr,"DEPWAIT",7) != 0) ) {
			  printf( "*E* Invalid option (1), READ THE MANUAL !, you get no help  for internal flags here.\n" );
			  nStatus = 0;
	  }
	  else {
         strcpy( pApi_buffer->API_Command_Number, API_CMD_NEWDAYFAILACT );
	     strncpy( pApi_buffer->API_Data_Buffer, pSptr, 9 );
	     API_set_datalen( pApi_buffer, strlen(pApi_buffer->API_Data_Buffer) );
	  }
   }
   else if (memcmp(cmdstring,"SCHED NEWDAYTIME",16) == 0) {
      strcpy( pApi_buffer->API_System_Name, "CONFG" );
      pSptr = pSptr + 16;
	  while (*pSptr == ' ') { pSptr++; }
	  if (UTILS_legal_time(pSptr)) {
         strcpy( pApi_buffer->API_Command_Number, API_CMD_SCHEDNEWDAYSET );
	     strncpy( pApi_buffer->API_Data_Buffer, pSptr, 6 );
	     API_set_datalen( pApi_buffer, strlen(pApi_buffer->API_Data_Buffer) );
	  }
	  else {
			  printf( "*E* Invalid option (2), READ THE MANUAL !, you get no help  for internal flags here.\n" );
			  nStatus = 0;
	  }
   }
   else if (memcmp(cmdstring,"SCHED LOGLEVEL",14) == 0) {
      strcpy( pApi_buffer->API_System_Name, "CONFG" );
      pSptr = pSptr + 14;
	  while (*pSptr == ' ') { pSptr++; }
	  if ( (memcmp(pSptr,"ERR",3) != 0) && (memcmp(pSptr,"WARN",4) != 0) && (memcmp(pSptr,"INFO",4) != 0) ) {
			  printf( "*E* Invalid option, LOGLEVEL must be one of ERR, WARN or INFO.\n" );
			  nStatus = 0;
	  }
	  else {
         strcpy( pApi_buffer->API_Command_Number, API_CMD_SETLOGLEVEL );
	     strncpy( pApi_buffer->API_Data_Buffer, pSptr, 7 );
	     API_set_datalen( pApi_buffer, strlen(pApi_buffer->API_Data_Buffer) );
	  }
   }
   else if (memcmp(cmdstring,"SCHED SHOWSESSIONS",18) == 0) {
         strcpy( pApi_buffer->API_Command_Number, API_CMD_SHOWSESSIONS );
   }
   else if (memcmp(cmdstring,"SCHED EXECJOBS",14) == 0) {
      pSptr = pSptr + 14;
	  while (*pSptr == ' ') { pSptr++; }
      if ( (memcmp(pSptr, "ON", 2) == 0) || (memcmp(pSptr,"OFF",3) == 0) ) {
         strcpy( pApi_buffer->API_Command_Number, API_CMD_EXECJOB_STATE );
	     strncpy( pApi_buffer->API_Data_Buffer, pSptr, 5 ); /* include null or cr */
	     API_set_datalen( pApi_buffer, strlen(pApi_buffer->API_Data_Buffer) );
	  }
	  else {
         printf( "*E* The EXECJOBS command expexts ON or OFF\n" );
         nStatus = 0;
	  }
   }
   else if (memcmp(cmdstring,"SCHED CATCHUP",13) == 0) {
      strcpy( pApi_buffer->API_System_Name, "CONFG" );
      pSptr = pSptr + 13;
	  while (*pSptr == ' ') { pSptr++; }
	  if ( (memcmp(pSptr,"ALLDAYS",7) == 0) || (memcmp(pSptr,"NONE",4) == 0) ) {
              strcpy( pApi_buffer->API_Command_Number, API_CMD_CATCHUPFLAG );
			  strncpy( pApi_buffer->API_Data_Buffer, pSptr, MAX_API_DATA_LEN );
	          API_set_datalen( pApi_buffer, strlen(pApi_buffer->API_Data_Buffer) );
	  }
	  else {
			  printf("*E* Catchup parameters are either ALLDAYS or NONE\n");
			  nStatus = 0;
	  }
   }
   else if (memcmp(cmdstring,"SCHED FULLDETAILS",17) == 0) {
      strcpy( pApi_buffer->API_System_Name, "CONFG" );
      pSptr = pSptr + 17;
	  while (*pSptr == ' ') { pSptr++; }
	  if ( (memcmp(pSptr,"ON",2) == 0) || (memcmp(pSptr,"OFF",3) == 0) ) {
              strcpy( pApi_buffer->API_Command_Number, API_CMD_FULLSCHEDDISP );
			  strncpy( pApi_buffer->API_Data_Buffer, pSptr, MAX_API_DATA_LEN );
	          API_set_datalen( pApi_buffer, strlen(pApi_buffer->API_Data_Buffer) );
	  }
	  else {
			  printf("*E* Full scheduler display parameters are either ON or OFF\n");
			  nStatus = 0;
	  }
   }
   /* MID: New 19Feb2003 - Alert forwarding interface patched in
	* put all tests in one block so only one check in mainline pass as 
	* these options will be used infrequently. */
   else if (memcmp(cmdstring,"SCHED ALERT FORWARDING",22) == 0) {
      strcpy( pApi_buffer->API_System_Name, "CONFG" );
      strcpy( pApi_buffer->API_Command_Number, API_CMD_ALERTFORWARDING );
      /* alert forwarding on, off or external */
      if (memcmp(cmdstring,"SCHED ALERT FORWARDING ACTION",29) == 0) {
         pSptr = pSptr + 29;
	     while (*pSptr == ' ') { pSptr++; }
         if (memcmp(pSptr,"ON",2) == 0) {
            strncpy( pApi_buffer->API_Data_Buffer, "FORWARDING Y", MAX_API_DATA_LEN );
         }
		 else if (memcmp(pSptr,"OFF",3) == 0) {
            strncpy( pApi_buffer->API_Data_Buffer, "FORWARDING N", MAX_API_DATA_LEN );
         }
		 else if (memcmp(pSptr,"EXTERNALCMD",11) == 0) {
            strncpy( pApi_buffer->API_Data_Buffer, "FORWARDING E", MAX_API_DATA_LEN );
         }
         else {
            printf("*E* SCHED ALERT FORWARDING ACTION: Expecting YES, NO or EXTERNALCMD\n");
            nStatus = 0;
         }
      } /* alert forwarding on. off or external */ 
	  /* IP address to forward to */
	  else if (memcmp(cmdstring,"SCHED ALERT FORWARDING IPADDR",29) == 0) {
         pSptr = pSptr + 29;
	     while (*pSptr == ' ') { pSptr++; }
         snprintf( pApi_buffer->API_Data_Buffer, MAX_API_DATA_LEN, "IPADDR %s", pSptr );
      } /* IP address to forward to */
	  /* IP port number to forward to */
	  else if (memcmp(cmdstring,"SCHED ALERT FORWARDING PORT",27) == 0) {
         pSptr = pSptr + 27;
	     while (*pSptr == ' ') { pSptr++; }
         snprintf( pApi_buffer->API_Data_Buffer, MAX_API_DATA_LEN, "PORT %s", pSptr );
      } /* IP port number forward to */
	  /* External command to raise alerts */
      else if (memcmp(cmdstring,"SCHED ALERT FORWARDING EXECRAISECMD",35) == 0) {
         pSptr = pSptr + 35;
	     while (*pSptr == ' ') { pSptr++; }
         snprintf( pApi_buffer->API_Data_Buffer, MAX_API_DATA_LEN, "PGMRAISE %s", pSptr );
      } /* External command to raise alerts */
	  /* External command to cancel alerts */
      else if (memcmp(cmdstring,"SCHED ALERT FORWARDING EXECCANCELCMD",36) == 0) {
         pSptr = pSptr + 36;
	     while (*pSptr == ' ') { pSptr++; }
         snprintf( pApi_buffer->API_Data_Buffer, MAX_API_DATA_LEN, "PGMCANCEL %s", pSptr );
      } /* External command to cancel alerts */
      /* else unknown */
      else {
            printf("*E* SCHED ALERT FORWARDING : Unknown command, refer to manual\n");
            printf("*E* SCHED ALERT FORWARDING : ACTION, IPADDR, PORT, EXECRAISECMD, EXECCANCELCMD\n");
            nStatus = 0;
      } /* unknown */
      API_set_datalen( pApi_buffer, strlen(pApi_buffer->API_Data_Buffer) );
   } /* End 19Feb2003 changes */
   else {
      printf( "*E* That SCHED command (%s) is not supported yet\n", cmdstring );
	  printf( "    Options are : LISTALL, STATUS [SHORT], RUNNOW <jobname>, DELETE <jobname>, SHUTDOWN\n" );
	  printf( "                  LOGLEVEL INFO | WARN | ERR, NEWDAYPAUSEACTION ALERT | DEPWAIT,\n" );
	  printf( "                  NEWDAYTIME hh:mm, SHOWSESSIONS, EXECJOBS ON|OFF\n" );
	  printf( "                  HOLD-ON, HOLD-OFF or FULLDETAILS ON | OFF\n" );
	  printf( "    There are also alert forwarding configuration commands,\n    refer to the manual before using.\n" );
      nStatus = 0;
   }
   return( nStatus );
} /* sched_command */

/* -----------------------------------------------------------------------------

    D e p e n d e n c y   C o m m a n d     P r o c e s s i n g     R o u t i n e s

   ----------------------------------------------------------------------------- */

/* *****************************************************************************
 * Note: String will NOT be fully uppercased before we are called. This is 
   because the dependency name may be a filename which can be mixed case.
   return 0 if an error.
   ***************************************************************************** */
int dependency_command( char *cmdstring, API_Header_Def *pApi_buffer ) {
   char *ptr1, savechar;
   int sanity; /* don't overrun scans if user has screwed up */
   
   /*
   DEP CLEARALL JOB <jobname>           remove all dependencies from this one job 
   DEP CLEARALL DEP <dependencyname>    remove dependency from all jobs waiting on it 
   DEP LISTWAIT <depedencyname>
   DEP LISTALL
   */

   /* We can't uppercase the whole string as the dependenccy may be a file
	* which is case sensitive. We will uppercase the command portion for out
	* checks */
   ptr1 = cmdstring;
   ptr1 = ptr1 + 4;  /* skip the DEP */
   while ((*ptr1 != '\0') && (*ptr1 != ' ') && (*ptr1 != '\n')) { ptr1++; }
   savechar = *ptr1;
   *ptr1 = '\0';
   UTILS_uppercase_string( cmdstring );
   *ptr1 = savechar;
   if (memcmp(cmdstring,"DEP CLEARALL",12) == 0) { /* one more parm to uppercase */
      ptr1 = cmdstring;
      ptr1 = ptr1 + 12;
      while (*ptr1 == ' ') { ptr1++; }
      while ((*ptr1 != '\0') && (*ptr1 != ' ') && (*ptr1 != '\n')) { ptr1++; }
      savechar = *ptr1;
      *ptr1 = '\0';
      UTILS_uppercase_string( cmdstring );
      *ptr1 = savechar;
   }

   /* If a listall, theats easy so get it out of the way */
   if (memcmp(cmdstring,"DEP LISTALL",11) == 0) {
      sanity = generic_listall_cmd( "DEP", pApi_buffer, "" );
	  return( 1 );
   }

   ptr1 = cmdstring;
   strcpy( pApi_buffer->API_System_Name, "DEP" );
   if (memcmp(cmdstring,"DEP LISTWAIT",12) == 0) {
      strcpy( pApi_buffer->API_Command_Number, API_CMD_LISTWAIT_BYDEPNAME );
      ptr1 = ptr1 + 12;
   }
   else if (memcmp(cmdstring,"DEP CLEARALL",12) == 0) {
      ptr1 = ptr1 + 12;
      sanity = 0;
      while (*ptr1 == ' ') {
		   ptr1++;
		   sanity++;
		   if (sanity > 50) {   /* nobody would deliberately put in more than 50 spaces would they ? */
			  printf( "*E* There doesn't appear to be any required parameters in %s, more that 50 spaces ?\n", cmdstring );
			  return( 0 );
		   }
	  }
	  if (memcmp(ptr1,"JOB",3) == 0) {
         strcpy( pApi_buffer->API_Command_Number, API_CMD_DELETEALL_JOB );
	  }
	  else if (memcmp(ptr1,"DEP",3) == 0) {
         strcpy( pApi_buffer->API_Command_Number, API_CMD_DELETEALL_DEP );
	  }
	  else {
		 printf( "*E* Command requires DEP or JOB indicator, %s is invalid\n", cmdstring );
		 return( 0 );
	  }
	  ptr1 = ptr1 + 3;
   }
   else {
      printf( "*E* The dependency %s functionality is not in sched_cmd yet\n", cmdstring );
	  printf( "    Options are : CLEARALL JOB <jobname>,CLEARALL DEP <depwait jobname>, LISTWAIT\n" );
	  return( 0 );
   }
   
   /* as every command for DEP must have additional parameters copy them to the
    * api data buffer now. */
   sanity = 0;
   while (*ptr1 == ' ') {
		   ptr1++;
		   sanity++;
		   if (sanity > 50) {   /* nobody would deliberately put in more than 50 spaces would they ? */
			  printf( "*E* There doesn't appear to be any required parameters in %s, more that 50 spaces ?\n", cmdstring );
			  return( 0 );
		   }
   }
   strncpy( pApi_buffer->API_Data_Buffer, ptr1, MAX_API_DATA_LEN );
   if ( strlen(pApi_buffer->API_Data_Buffer) == 0 ) {
	   printf( "*E* Paremeters are required to be entered for the command %s\n", cmdstring );
	   return( 0 );
   }
   API_set_datalen( pApi_buffer, strlen(pApi_buffer->API_Data_Buffer) );

   return( 1 );  /* All OK */
} /* dependency_command */

/* -----------------------------------------------------------------------------

    D e b u g   C o m m a n d     P r o c e s s i n g     R o u t i n e s

   ----------------------------------------------------------------------------- */

/* *****************************************************************************
 * Note: String will be fully uppercased before we are called.
   return 0 if an error.
   ***************************************************************************** */
int debug_command( char *cmdstring, API_Header_Def *pApi_buffer ) {
		/* DEBUG <subsys> <number 0-9> */
   char *ptr1;
   int sanity; /* don't overrun scans if user has screwed up */
   
   ptr1 = cmdstring;

   strcpy( pApi_buffer->API_Command_Number, API_CMD_DEBUGSET );
   strcpy( pApi_buffer->API_System_Name, "DEBUG" );

   /* as every command for DEBUG must have additional parameters copy them to the
    * api data buffer now. */
   sanity = 0;
   while (*ptr1 == ' ') {
		   ptr1++;
		   sanity++;
		   if (sanity > 50) {   /* nobody would deliberately put in more than 50 spaces would they ? */
			  printf( "*E* There doen't appear to be any required parameters in %s, more that 50 spaces ?\n", cmdstring );
			  return( 0 );
		   }
   }
   strncpy( pApi_buffer->API_Data_Buffer, ptr1, MAX_API_DATA_LEN );
   if ( strlen(pApi_buffer->API_Data_Buffer) == 0 ) {
	   printf( "*E* Parameters are required to be entered for the command %s\n", cmdstring );
	   printf( "    DEBUG <subsys> <number 0-9>\n   If you don't know what the subsystems are don't play with this command !\n");
	   return( 0 );
   }
   API_set_datalen( pApi_buffer, strlen(pApi_buffer->API_Data_Buffer) );

   return( 1 );  /* All OK */
} /* debug_command */

/* -----------------------------------------------------------------------------

    A l e r t   C o m m a n d     P r o c e s s i n g     R o u t i n e s

   ----------------------------------------------------------------------------- */

/* *****************************************************************************
 * Note: String will be fully uppercased before we are called.
   return 0 if an error.
   ***************************************************************************** */
int alert_command( char *cmdstring, API_Header_Def *pApi_buffer ) {
   int nStatus;
   char *pSptr;
   alertsfile_def *pAlert_Rec;
   
   pAlert_Rec = (alertsfile_def *)&pApi_buffer->API_Data_Buffer;
   nStatus = 1;  /* default is issue command to server */

   /* listall will be most commonly used so put it first */
   if (memcmp(cmdstring,"ALERT LISTALL",13) == 0) {
      nStatus = generic_listall_cmd( "ALERT", pApi_buffer, "" );
   }
   else if ( (memcmp(cmdstring, "ALERT RESTART", 13) == 0) ||
        (memcmp(cmdstring, "ALERT INFO", 10) == 0) ||
        (memcmp(cmdstring, "ALERT ACK", 9) == 0) ||
		(memcmp(cmdstring, "ALERT FORCEOK", 13) == 0)
	) {
	  pSptr = cmdstring;
      API_init_buffer( pApi_buffer );
      strcpy( pApi_buffer->API_System_Name, "ALERT" );
      strncpy( pAlert_Rec->job_details.job_header.jobnumber, "000000", JOB_NUMBER_LEN );
      UTILS_space_fill_string( pAlert_Rec->job_details.job_header.jobname, JOB_NAME_LEN );
      pAlert_Rec->job_details.job_header.info_flag = 'A';   /* Active */
      if (memcmp(cmdstring,"ALERT RESTART",13) == 0) {
         pSptr = pSptr + 13;
         strcpy( pApi_buffer->API_Command_Number, API_CMD_SCHEDULE_ON );
      }
      else if (memcmp(cmdstring,"ALERT INFO",10) == 0) {
         pSptr = pSptr + 10;
         strcpy( pApi_buffer->API_Command_Number, API_CMD_RETRIEVE );
	  }
      else if (memcmp(cmdstring,"ALERT ACK",9) == 0) {
         pSptr = pSptr + 9;
         strcpy( pApi_buffer->API_Command_Number, API_CMD_ALERT_ACK );
      }
      else if (memcmp(cmdstring,"ALERT FORCEOK",13) == 0) {
         pSptr = pSptr + 13;
         strcpy( pApi_buffer->API_Command_Number, API_CMD_ALERT_FORCEOK );
      }
      while (*pSptr == ' ') { pSptr++; }  /* skip spaces between command and jobname */
      strncpy( pAlert_Rec->job_details.job_header.jobname, pSptr, JOB_NAME_LEN );
      if (
            (strlen(pAlert_Rec->job_details.job_header.jobname) < 2) /* \n is 1 byte */ ||
            (strncmp( pAlert_Rec->job_details.job_header.jobname, "  ", 1 ) == 0)
         ) {
         printf( "*E* a jobname must be provided in this command\n" );
         nStatus = 0;   /* do not pass to the server */
      } else {
         API_set_datalen( pApi_buffer, sizeof(alertsfile_def) );
         nStatus = 1;   /* Pass to the server */
      }
   }
   else if (memcmp(cmdstring,"ALERT LISTALL",13) == 0) {
      nStatus = generic_listall_cmd( "ALERT", pApi_buffer, "" );
   }
   else {
      printf( "*E* The ALERT command (%s) is not in sched_cmd yet\n", cmdstring );
      printf( "    Options are : LISTALL, ACK <jobname>, INFO <jobname>, RESTART <jobname>\n" );
      nStatus = 0;
   }
   return( nStatus );
} /* alert_command */

/* -----------------------------------------------------------------------------

    U S E R   C o m m a n d     P r o c e s s i n g     R o u t i n e s

   ----------------------------------------------------------------------------- */

/* *****************************************************************************
   Called from a user add.
   return 0 if an error.
   NOTE: The input data from the command line is still in an unknown case here.
   ***************************************************************************** */
int user_add_parse_cmddata( char * username, char *pCmdstring, USER_record_def * userrec ) {
/*
 *  USER ADD <name>, PASSWORD <password>,
 *       AUTH ADMIN|OPERATOR|JOB|SECURITY|BROWSE,
 *       AUTOAUTH YES|NO, SUBSETADDAUTH YES|NO|ADMIN,
 *       USERLIST "<1>,<2>...<10>",DESC "<description>"
 */
   char sTestParmName[100];
   char sTestParmValue[150];
   char sTestParmValue2[100];
   int nFieldLen, nLeadingPads, nOverrunFlag, endofdata, i;
   char *pSptr, *pSptr2, *workptr;

   pSptr = pCmdstring;

   /* set the defaults here ! */
   BULLETPROOF_USER_init_record_defaults( userrec );
   strncpy( userrec->user_name, username, USER_MAX_NAMELEN );
   UTILS_space_fill_string( (char *)&userrec->user_name, USER_MAX_NAMELEN );

   /* loop until we reach the end of the command string passed */
   endofdata = 0;
   while (endofdata == 0) {
      /* --- find the field type --- */
      nFieldLen = UTILS_parse_string( pSptr, ' ', (char *)&sTestParmName, 99, 
                                      &nLeadingPads, &nOverrunFlag );
      if (nFieldLen == 0) {
         endofdata = 1;
      }
      else {
        pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;
      }

      /* --- find the field data --- */

      /* uppercase the parameter name to search on */
      workptr = (char *)&sTestParmName;
      while ( (*workptr != '\0') && (*workptr != '\n') ) {
         *workptr = toupper( *workptr );
         workptr++;
      }

	  /*
	   * most fields are up until the next comma. These are processed in
	   * this (large) block of code.
	   */
      if (
           (memcmp(sTestParmName, "PASSWORD", 8) == 0) ||
           (memcmp(sTestParmName, "AUTH", 4) == 0) ||
           (memcmp(sTestParmName, "AUTOAUTH", 8) == 0) ||
		   (memcmp(sTestParmName, "SUBSETADDAUTH", 13) == 0)
         ) {
         pSptr = pSptr + UTILS_count_delims( pSptr, ' ' );
         nFieldLen = UTILS_parse_string( pSptr, ',', (char *)&sTestParmValue, 99, 
                                         &nLeadingPads, &nOverrunFlag );
         if (nFieldLen == 0) {
            printf( "*E* Missing parameter value for field %s\n", sTestParmName );
            return( 0 );
         }
         else {
            pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;
            pSptr = pSptr + UTILS_count_delims( pSptr, ',' );
            if (memcmp(sTestParmName, "PASSWORD", 8) == 0) {
               strncpy( userrec->user_password, sTestParmValue, USER_MAX_PASSLEN );
			   UTILS_space_fill_string( (char *)&userrec->user_password, USER_MAX_PASSLEN );
               /* encryption is done by the add, we pass in clear text */
            }
            else if (memcmp(sTestParmName, "AUTH", 4) == 0) {
               /* uppercase the parameter value */
               workptr = (char *)&sTestParmValue;
               while ( (*workptr != '\0') && (*workptr != '\n') ) {
                  *workptr = toupper( *workptr );
                  workptr++;
               }
               if (memcmp( sTestParmValue, "ADMIN", 5) == 0) {
                   userrec->user_auth_level = 'A';
               }
               else if (memcmp( sTestParmValue, "OPERATOR", 5) == 0) {
                   userrec->user_auth_level = 'O';
               }
               else if (memcmp( sTestParmValue, "JOB", 5) == 0) {
                   userrec->user_auth_level = 'J';
               }
               else if (memcmp( sTestParmValue, "SECURITY", 5) == 0) {
                   userrec->user_auth_level = 'S';
               }
               else {
                   userrec->user_auth_level = '0';  /* default is browse */
               }
            }
            else if (memcmp(sTestParmName, "AUTOAUTH", 8) == 0) { /* YES or NO */
               UTILS_uppercase_string( (char *)&sTestParmValue );
               if (memcmp( sTestParmValue, "YES", 3) == 0) {
                   userrec->local_auto_auth = 'Y';  /* allowed autoauth */
               }
			   else if (memcmp( sTestParmValue, "NO", 2) == 0) {
                   userrec->local_auto_auth = 'N';  /* allowed autoauth */
               }
			   else {
                   printf( "WARN: Invalid AUTOAUTH value, set to NO\n" );
                   userrec->local_auto_auth = 'N';  /* allowed autoauth */
               }
            }
            else if (memcmp(sTestParmName, "SUBSETADDAUTH", 13) == 0) {
               UTILS_uppercase_string( (char *)&sTestParmValue );
               if (memcmp( sTestParmValue, "YES", 3) == 0) {
                   userrec->allowed_to_add_users = 'Y';  /* allowed to add within own group */
               }
			   else if (memcmp( sTestParmValue, "ADMIN", 5) == 0) {
                   userrec->allowed_to_add_users = 'S';  /* can create users outside own group */
               }
			   else if (memcmp( sTestParmValue, "NO", 2) == 0) {
                   userrec->allowed_to_add_users = 'N';  /* can create users outside own group */
               }
			   else {
                   printf( "WARN: Invalid SUBSETADDAUTH value, set to NO\n" );
                   userrec->allowed_to_add_users = 'N';  /* not allowed */
               }
            }
         }
      }

	  /*
	   * The other fields have data between " characters. These are
	   * userlist and desc.
	  */
      else if (
                (memcmp(sTestParmName, "USERLIST", 8) == 0) ||
				(memcmp(sTestParmName, "DESC", 4) == 0)
         ) {
         pSptr = pSptr + UTILS_count_delims( pSptr, ' ' );
         nFieldLen = UTILS_parse_string( pSptr, '"', (char *)&sTestParmValue, 149, 
                                         &nLeadingPads, &nOverrunFlag );
         if (nFieldLen == 0) {
            printf( "*E* Missing parameter value for field %s\n", sTestParmName );
            return( 0 );
         }
         else {
            pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;
            pSptr = pSptr + UTILS_count_delims( pSptr, '"' );
            pSptr = pSptr + UTILS_count_delims( pSptr, ',' );

			/* desc is easy, its the entire field without case shifting needed */
            if (memcmp(sTestParmName, "DESC", 4) == 0) {
               strncpy( userrec->user_details, sTestParmValue, 49 );
            }

			/* Then userlist, a comma seperated list of users */
			else if (memcmp(sTestParmName, "USERLIST", 4) == 0) {
               i = 0;
               /* scan for each comma seperated field */
               pSptr2 = (char *)&sTestParmValue;
               pSptr2 = pSptr2 + UTILS_count_delims( pSptr2, ' ' );
			   nFieldLen = 1; /* to start loop */
			   while (nFieldLen > 0) {
                  nFieldLen = UTILS_parse_string( pSptr2, ',', (char *)&sTestParmValue2, 99, 
                                                  &nLeadingPads, &nOverrunFlag );
                  if (nFieldLen > 0) {
                     strncpy( userrec->user_list[i].userid, sTestParmValue2, USER_MAX_NAMELEN );
                     i++;
				  }
                  if ( i > USERS_MAX_USERLIST) {
                      printf( "*ERR: Too many users in list, only %d allowed\n", USERS_MAX_USERLIST );
                      return( 0 );
                  }
                  pSptr2 = pSptr2 + nFieldLen + nLeadingPads + nOverrunFlag;
                  pSptr2 = pSptr2 + UTILS_count_delims( pSptr2, ',' );
                  pSptr2 = pSptr2 + UTILS_count_delims( pSptr2, ' ' );
			   }
			}
         }  /* else parm value found */
      }   /* desc or userlist */
      else {
         printf( "*E* Parameter %s is not recognised as a user parameter\n", sTestParmName );
         return( 0 );
      }
      if ((*pSptr == '\0') || (*pSptr == '\n')) { endofdata = 1; }
   }

   /* ----- If we got here we filled the data record with stuff ------ */
   /* Sanity check it before continuing */
   if (BULLETPROOF_user_record( userrec, 0 ) == 0 ) {
      /* it will have logged an error */
      return( 0 );
   }

   return( 1 ); /* all OK */
} /* user_add_parse_cmddata */


/* *****************************************************************************
 * pApi_buffer is the API buffer area we update
 *
 * Expect to be passed user details in logon line as...
 *  USER DELETE <name>
 *  USER ADD <name>,PASSWORD <password>, AUTH ADMIN|OPERATOR|JOB|SECURITY,
 *       AUTOAUTH YES|NO, SUBSETADDAUTH YES|NO|ADMIN,
 *       USERLIST "<1>,<2>...<10>",DESC "<description>"
 *  USER PASSWORD <name>,<password>
 *  USER INFO <name>
 *
 * return 0 if an error, 1 to process, 2 no action but don't exit program.
   ***************************************************************************** */
int user_command( API_Header_Def * pApi_buffer, char * dataline ) {
   char sTestField[100];
   char sUserName[USER_MAX_NAMELEN+1];
   int nFieldLen, nLeadingPads, nOverrunFlag;
   char *pSptr;
   USER_record_def * user_rec;

   pSptr = dataline;
   user_rec = (USER_record_def *)&pApi_buffer->API_Data_Buffer; /* User info in api data buffer */

   /* skip the 'user' header, we know we are a user request */
   nFieldLen = UTILS_parse_string( pSptr, ' ', (char *)&sTestField, 99, 
                                   &nLeadingPads, &nOverrunFlag );
   if (nFieldLen == 0) {   /* failed */
      printf( "*E* Failed to extract USER command header.\n" );
      return( 0 );
   }
   pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;

   /* get the user command type */
   nFieldLen = UTILS_parse_string( pSptr, ' ', (char *)&sTestField, 99, 
                                   &nLeadingPads, &nOverrunFlag );
   if (nFieldLen == 0) {  /* failed */
      printf( "*E* Failed to extract user command type\n" );
      return( 0 );
   }
   pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;

   /* The command type is safe to uppercase so do so now */
   UTILS_uppercase_string( (char *)&sTestField );

   /* --- if listall then no more work needed --- */
   if (memcmp(sTestField,"LISTALL",7) == 0) {
      return( generic_listall_cmd( "USER", pApi_buffer, "" ) );
   }

   /* --- else all other commands must have a username --- */
   while (*pSptr == ' ') { pSptr++; } /* skip the space thats still in there */
   nFieldLen = UTILS_parse_string( pSptr, ',', (char *)&sUserName, USER_MAX_NAMELEN, 
                                   &nLeadingPads, &nOverrunFlag );
   if (nFieldLen == 0) {
      /* failed */
      printf( "*E* A user name value is required for user commands.\n" );
      return( 0 );
   }
   pSptr = pSptr + nFieldLen + nLeadingPads + nOverrunFlag;


   /* --- Call the required function now --- */
   if (memcmp(sTestField,"ADD",3) == 0) {
      /* we need to extract out all the fields expected */
      pSptr = pSptr + UTILS_count_delims( pSptr, ',' );
      API_init_buffer( pApi_buffer );
      strcpy( pApi_buffer->API_System_Name, "USER" );
      strcpy( pApi_buffer->API_Command_Number, API_CMD_USER_ADD );
      API_set_datalen( pApi_buffer, sizeof(USER_record_def) );
      return( user_add_parse_cmddata( sUserName, pSptr, user_rec ) ) ;
   }
   else if ( (memcmp(sTestField,"PASSWORD",8) == 0) ||
             (memcmp(sTestField,"PASSWD",6) == 0) ) {
      /* put the user name in the user record and send it off */
      API_init_buffer( pApi_buffer );
      strcpy( pApi_buffer->API_System_Name, "USER" );
      strcpy( pApi_buffer->API_Command_Number, API_CMD_USER_NEWPSWD );
      BULLETPROOF_USER_init_record_defaults( user_rec );
      strncpy( user_rec->user_name, sUserName, USER_MAX_NAMELEN );
      UTILS_space_fill_string( user_rec->user_name, USER_MAX_NAMELEN );
      pSptr = pSptr + UTILS_count_delims( pSptr, ',' );
      strncpy( user_rec->user_password, pSptr, USER_MAX_PASSLEN );
      UTILS_space_fill_string( user_rec->user_password, USER_MAX_PASSLEN );
      API_set_datalen( pApi_buffer, sizeof(USER_record_def) );
      return( 1 );
   }
   else if ( (memcmp(sTestField,"DELETE",6) == 0) || (memcmp(sTestField,"INFO",4) == 0) ) {
      /* put the user name in the user record and send it off */
      API_init_buffer( pApi_buffer );
      strcpy( pApi_buffer->API_System_Name, "USER" );
      if (memcmp(sTestField,"DELETE",6) == 0) {
         strcpy( pApi_buffer->API_Command_Number, API_CMD_USER_DELETE );
      }
      else { /* info request */
         strcpy( pApi_buffer->API_Command_Number, API_CMD_RETRIEVE );
      }
      BULLETPROOF_USER_init_record_defaults( user_rec );
      strncpy( user_rec->user_name, sUserName, USER_MAX_NAMELEN );
      API_set_datalen( pApi_buffer, sizeof(USER_record_def) );
      return( 1 );
   }
   else {
      printf( "*E* That USER Command (%s) is not supported yet\n", sTestField );
	  printf( "    Options are : 'ADD <user>,<parms>', 'DELETE <user>', INFO <user> or" );
	  printf( "                  or 'PASSWORD <user> <newpass>'\n" );
      return( 0 );
   }
} /* user_command */

/* *****************************************************************************
 * pApi_buffer is the API buffer area we update
 * Expect to be passed "login username password" in dataline
 * return 0 if an error.
   ***************************************************************************** */
int logon_command( API_Header_Def * pApi_buffer, char * dataline ) {
   pid_t my_pid;
   char pid_string[21];
   char user_name[40];
   char user_pswd[40];
   char *ptr1, *ptr2;

   ptr1 = dataline;
   ptr1 = ptr1 + 5;  /* skip the login/logon keyword */
   /* skip any spaces before the username */
   while ( (*ptr1 == ' ') && (*ptr1 != '\n') && (*ptr1 != '\0') ) { ptr1++; }
   /* find the end of the username */
   ptr2 = ptr1;
   while ( (*ptr2 != ' ') && (*ptr2 != '\n') && (*ptr2 != '\0') ) { ptr2++; }
   *ptr2 = '\0';
   if (ptr1 == ptr2) {
      printf( "*ERR: A username must be provided !. Syntax: login username password\n" );
      return( 0 );
   }
   /* save the username */
   strncpy( user_name, ptr1, 39 );
   /* skip any spaces before the password */
   ptr1 = ptr2 + 1; /* after the null we put in */
   while ( (*ptr1 == ' ') && (*ptr1 != '\n') && (*ptr1 != '\0') ) { ptr1++; }
   /* find the end of the password */
   ptr2 = ptr1;
   while ( (*ptr2 != ' ') && (*ptr2 != '\n') && (*ptr2 != '\0') ) { ptr2++; }
   *ptr2 = '\0';
   if (ptr1 == ptr2) {
      printf( "*ERR: A password must be provided !. Syntax: login username password\n" );
      return( 0 );
   }
   /* save the password */
   strncpy( user_pswd, ptr1, 39 );

   /* We need the pid of the process also */
   my_pid = getpid();
   UTILS_number_to_string( (int)my_pid, (char *)&pid_string, 20 );

   /* Initialise the API buffer and OK to send the request */
   API_init_buffer( pApi_buffer );
   strcpy( pApi_buffer->API_System_Name, "SCHED" );
   strcpy( pApi_buffer->API_Command_Number, API_CMD_LOGON );
   snprintf( pApi_buffer->API_Data_Buffer, (MAX_API_DATA_LEN - 1), "%s %s %s", user_name, user_pswd, pid_string );
   API_set_datalen( pApi_buffer, strlen(pApi_buffer->API_Data_Buffer) );
   return( 1 ); /* send request */
} /* logon_command */

/* *****************************************************************************
 * pApi_buffer is the API buffer area we update
 * return 0 if an error.
   ***************************************************************************** */
void logoff_command( API_Header_Def * pApi_buffer ) {
   API_init_buffer( pApi_buffer );
   strcpy( pApi_buffer->API_System_Name, "SCHED" );
   strcpy( pApi_buffer->API_Command_Number, API_CMD_LOGOFF );
   strcpy( pApi_buffer->API_Data_Buffer, "" );
   API_set_datalen( pApi_buffer, 0 );
} /* logoff_command */

/* *****************************************************************************
 * If theuser is root we try to autologin with the reserved user name of
 * auto-login. For any other user we get the real user name from the local
 * servers password file and use that as the name for the autologin.
 * The server does the security checking on the userid as it holds the
 * details of which users are permitted to autologin.
 *
 * return 0 if an error.
 * Note: we always return 1 as nothing fails here.
   ***************************************************************************** */
int autologon_command( API_Header_Def * pApi_buffer ) {
   uid_t effective_user;
   uid_t real_user;
   pid_t my_pid;
   char pid_string[21];
   char user_name[40];
#ifdef GCC_MAJOR_VERSION3
   struct passwd * user_details;
#else
   struct passwd user_details;
   struct passwd *result;
   char *buf;
   size_t bufsize;
   int s;
#endif

#ifdef DEBUG_MODE_ON
   printf("DEBUG: entered autologon_command \n");
#endif

   API_init_buffer( pApi_buffer );
   strcpy( pApi_buffer->API_System_Name, "SCHED" );
   strcpy( pApi_buffer->API_Command_Number, API_CMD_LOGON );

   /* Only supporting the PID auth request at present */
   effective_user = geteuid();
   real_user = getuid();
   my_pid = getpid();
#ifdef DEBUG_MODE_ON
   printf("DEBUG: effectiveuid=%d, realuid=%d, pid=%d \n", effective_user, real_user, my_pid );
#endif
   UTILS_number_to_string( (int)my_pid, (char *)&pid_string, 20 );
   if ((effective_user == 0) || (real_user == 0)) {    /* the user is root */
      snprintf( pApi_buffer->API_Data_Buffer, (MAX_API_DATA_LEN - 1), "auto-login PID-AUTH %s", pid_string );
   }
   else {
#ifdef DEBUG_MODE_ON
      printf("DEBUG: about to call getpwuid with %d \n", real_user );
#endif
/* MIDMID This is the problem. 
 * The parm format has completely changed between comoiler versions
 * Not it is apparantly
 *        int getpwuid_r(uid_t uid, struct passwd *pwd,
 *                           char *buf, size_t buflen, struct passwd **result);
 * investigating */
#ifdef GCC_MAJOR_VERSION3
      user_details = getpwuid(real_user);
      strncpy( user_name, user_details->pw_name, 39 );
#else
/* getpwuid crashed constantly under later GCC versions,
 * do it the hard way */
      bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
      if (bufsize == -1)          /* Value was indeterminate */
          bufsize = 16384;        /* Should be more than enough */
      buf = malloc(bufsize);
      if (buf == NULL) {
          perror("malloc");
          exit(1);
      }
      s = getpwuid_r((int)real_user, &user_details, buf, bufsize, &result);
      if (result == NULL) {
         if (s == 0)
             printf("Your userid is no longer in the unox system password file\n");
         else {
             errno = s;
             perror("getpwnam_r");
         }
         free(buf);
         exit(1);
      }
      strncpy( user_name, user_details.pw_name, 39 );
      free(buf);
#endif
#ifdef DEBUG_MODE_ON
   printf("DEBUG: username retrieved from passwd file via getpwuid is %s \n", user_name );
#endif
      snprintf( pApi_buffer->API_Data_Buffer, (MAX_API_DATA_LEN - 1), "%s PID-AUTH %s", user_name, pid_string );
   }
   API_set_datalen( pApi_buffer, strlen(pApi_buffer->API_Data_Buffer) );
#ifdef DEBUG_MODE_ON
        printf("DEBUG: leaving autologon_command with user %s %s \n", user_name, pid_string);
#endif
   return( 1 );
} /* autologon_command */

/* -----------------------------------------------------------------------------

                     M a i n l i n e      L o g i c

   ----------------------------------------------------------------------------- */

/* *****************************************************************************
   returns
   0 if error,
   1 if OK to send it to server,
   2 if no send action needed (internally handled)
   3 open request, no send, internally handled [ not used here but reserved by calling routine ]
   ***************************************************************************** */
int build_request_buffer( char *pCmdstring, API_Header_Def *pApibuffer ) {
   int nStatus;
   char *ptr, *ptrmax;

#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in build_request_buffer (init API)\n" );
#endif

   API_init_buffer( pApibuffer );
   API_set_datalen( pApibuffer, 0 );
   strncpy( pApibuffer->API_EyeCatcher, API_EYECATCHER, 4 );

#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in build_request_buffer (init API complete)\n" );
#endif

   /* Uppercase the first part of the command, to the  first ' ' to make searches easier */
   /* note: cannot use strncasecmp as the string matches check for a matching
	* null character at the end also, and we don't always have that. Also may
	* not be fully portable */
   ptr = pCmdstring;
   ptrmax = ptr + MAX_WORKBUF_LEN_JS;
   while ( (*ptr != '\0') && (*ptr != ',') && (*ptr != ' ') && (*ptr != '\n')
           && (ptr < ptrmax) ) {
       *ptr = toupper(*ptr);
	   ptr++;
   }

#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in build_request_buffer (testing command type)\n" );
#endif

   /* then determine what type of request it was */
   if (memcmp(pCmdstring,"JOB",3) == 0) {      /* job */
      /* MUST REMAIN Mixed Case data. */
      nStatus = job_command( pCmdstring, pApibuffer );
   }
   else if (memcmp(pCmdstring,"CAL",3) == 0) {      /* calendar update */
      /* MUST REMAIN Mixed Case data. */
      nStatus = calendar_command( pCmdstring, pApibuffer );
   }
   else if (memcmp(pCmdstring,"SCHED",5) == 0) {    /* scheduler core activity */
      /* MUST REMAIN Mixed Case data (for the execprogs). */
      nStatus = sched_command( pCmdstring, pApibuffer );
   }
   else if (memcmp(pCmdstring,"ALERT",5) == 0) {    /* alert */
      /* Can be uppercased now */
      UTILS_uppercase_string( ptr );
	  nStatus = alert_command( pCmdstring, pApibuffer );
   }
   else if (memcmp(pCmdstring,"DEP",3) == 0) {      /* dependency */
      /* Cannot be uppercased, may contain a filename */
      strncpy( pApibuffer->API_System_Name, "DEP", 3 );
	  nStatus = dependency_command( pCmdstring, pApibuffer );
   }
   else if (memcmp(pCmdstring,"DEBUG",5) == 0) {      /* debugging */
      /* Can be uppercased now */
      UTILS_uppercase_string( ptr );
	  nStatus = debug_command( pCmdstring, pApibuffer );
   }
   else if ( (memcmp(pCmdstring,"AUTOLOGON",9) == 0) ||  /* logon based on pid request */
             (memcmp(pCmdstring,"AUTOLOGIN",9) == 0) )   /* logon based on pid request */
   {
      /* nothing additional to uppercase */
	  nStatus = autologon_command( pApibuffer );
   }
   else if ( (memcmp(pCmdstring,"LOGON",5) == 0) ||     /* logon based on user/password request */
             (memcmp(pCmdstring,"LOGIN",5) == 0) )      /* logon based on user/password request */
   {
      /* MUST REMAIN Mixed Case data (for the logon). */
	  nStatus = logon_command( pApibuffer, pCmdstring );
   }
   else if ( (memcmp(pCmdstring,"LOGOFF",6) == 0) ||    /* logoff, reset to guest */
             (memcmp(pCmdstring,"LOGOUT",6) == 0) )
   {
	  logoff_command( pApibuffer );
	  nStatus = 1;   /* send it */
   }
   else if (memcmp(pCmdstring,"USER",4) == 0)           /* user add or delete request */
   {
      /* MUST REMAIN Mixed Case data. */
	  nStatus = user_command( pApibuffer, pCmdstring );
   }
   else if (memcmp(pCmdstring,"HELP",4) == 0) {      /* help */
      /* leave uppercasing for now */
      help_request_interface( pCmdstring );
      nStatus = 2;
   }
   else {
      printf( "*E* Error: unknown subsystem, %s", pCmdstring  );
      printf( "*I* Syntax is <subsystem> <command> [<parameters>]\n" );
      nStatus = 0;
   }

   return( nStatus );   /* done */
} /* build_request_buffer */

/* *****************************************************************************
   returns
      1 - command issued, response expected from socket
      2 - no response from socket expected
      3 - open request, process in mainline
      0 - we need to stop
   ***************************************************************************** */
int command_process( int iSockid, char *pCmdstring ) {
   int z, x, respcode;
   API_Header_Def pApibuffer;

#ifdef DEBUG_MODE_ON
   printf( "DEBUG: in command_process (testing command type)\n" );
#endif
   /* ignore comments */
   if (memcmp(pCmdstring,"#",1) == 0) { return( 2 ); } /* 2 is no action, but don't stop */

   /* is it an exit request */
   if ( (memcmp(pCmdstring,"EXIT",4) == 0) || (memcmp(pCmdstring,"QUIT",4) == 0) ||
        (memcmp(pCmdstring,"exit",4) == 0) || (memcmp(pCmdstring,"quit",4) == 0) ||
        (memcmp(pCmdstring,"q",1) == 0) || (memcmp(pCmdstring,"Q",1) == 0) ) {
      return( 0 );
   }

   /* is it an open request */
   if ( (memcmp(pCmdstring,"OPEN",4) == 0) || (memcmp(pCmdstring,"open",4) == 0) ) {
      return( 3 );
   }

   /* Determine what type of command we are to work with */
   respcode = build_request_buffer(pCmdstring, &pApibuffer);
   if (respcode == 1) {
      /* If ok check the buffer, and write to server if OK */
      if ( (x = BULLETPROOF_API_Buffer( &pApibuffer )) > 0 ) {
         /* write the command */
#ifdef DEBUG_MODE_ON
        printf( "DEBUG: in command_process (writing %d bytes to socket)\n", x );
        API_DEBUG_dump_api( &pApibuffer );
#endif
         z = write( iSockid, &pApibuffer, x );
         if (z == -1) die("failed to write to socket");   
         return( 1 );
      }
      else {
         printf( "*E* Command buffer failed the sanity check, check your parameters.\n" );
         return( 0 );
      }
   }
   else {
      return( respcode );
   }
} /* command_process */

/* *****************************************************************************
 * Invoked if the server sends back data that requires an intelligent handling
 * of the response, that is the server returned actual raw data rather than a
 * response field that can just be displayed.
   ***************************************************************************** */
void data_response_process( API_Header_Def * api_bufptr ) {
   calendar_def * calbuffer;

   if (memcmp(api_bufptr->API_System_Name,"CAL",3) == 0) {
      if (memcmp(api_bufptr->API_Command_Number,API_CMD_DUMP_FORMATTED,API_CMD_LEN) == 0) {
         calbuffer = (calendar_def *)&api_bufptr->API_Data_Buffer;
         /* Raw fields may not be etrminated as valid display fields, do so now */
         UTILS_space_fill_string( calbuffer->calendar_name, CALENDAR_NAME_LEN );
         UTILS_space_fill_string( calbuffer->description, CALENDAR_DESC_LEN );
         UTILS_space_fill_string( calbuffer->year, 4 );
         DEBUG_CALENDAR_dump_to_stdout( calbuffer );
      }
      else {
         printf( "*E* Raw data command %s for subsystem %s\n", api_bufptr->API_Command_Number, api_bufptr->API_System_Name );
         printf( "    A Response dor the data command number does not exist in jobsched_cmd\n" );
         printf( "    The reply has been discarded.\n" );
      }
   }
   else {
      printf( "*E* Raw data returned from server for %s subsystem request\n", api_bufptr->API_System_Name );
      printf( "    Handling of raw data is not supported in jobsched_cmd for that subsystem yet.\n" );
      printf( "    The reply has been discarded.\n" );
   }
} /* data_response_process */

/* *****************************************************************************
   ***************************************************************************** */
int main(int argc, char **argv) {
   int z, stop_now, command_result;
   char *zptr;
   int    s;                  /* socket */
   char *srvr_addr = NULL;
   int  srvr_port;
   char   workbuf[MAX_WORKBUF_LEN_JS];
   API_Header_Def * api_bufptr;
   int last_port, next_port;
   char last_addr[60], next_addr[60];

   /* The below three flags are used in the UTILS and API library so we
	* need to set them. These are global variables in config.h */
   pSCHEDULER_CONFIG_FLAGS.log_level = 0;
   pSCHEDULER_CONFIG_FLAGS.debug_level.utils = 0;
   pSCHEDULER_CONFIG_FLAGS.debug_level.api = 0;
   /* This is also used by utils */
   msg_log_handle = stdout;
   MEMORY_initialise_malloc_counters();

   api_bufptr = (API_Header_Def *)&workbuf;

   /* --- check for overrides of the defaults --- */
   /* if we have been provided with a port number use it */
   if (argc >= 2 ) {
      srvr_port = atoi(argv[1]);
   }
   else {
      srvr_port = 9002;
   }
   /* if we have been provided with a server address
      then we will use it. If not use localhost 127.0.0.1 */
   if (argc >= 3 ) {
      srvr_addr = argv[2];
   }
   else {
      srvr_addr = "127.0.0.1";
   }

   /* save fields for recovery if an open fails */
   last_port = srvr_port;
   strncpy( last_addr, srvr_addr, 59 );

   /* Connect to the job scheduler server */
   if ((command_result = connect_to_server( srvr_port, srvr_addr, &s )) == 0) {
      /* An error message should already have been written */
      exit( 1 );
   }

   /* GPL Banner */
   printf( "GPL V2 Release - this program is not warranted in any way, you use this\n" );
   printf( "                 application at your own risk. Refer to the GPL V2 license.\n" );
   /* read commands */
   stop_now = 0;
   while (stop_now == 0) {
      printf( "command:" );
      zptr = fgets(workbuf,sizeof workbuf - 1,stdin);
      printf( "\n" );
      if (zptr != NULL) {
			  /* Warning: I stupidly put an uppercase of commands here. That
			   * really screwed up the directory paths on job adds. Now
			   * uupercasing is only done within the appropriate command
			   * processing routines. */
         command_result = command_process( s, (char *)&workbuf );
         if (command_result == 1) {       /* response expected */
			   api_bufptr->API_More_Data_Follows = '1';
			   while (api_bufptr->API_More_Data_Follows == '1') {
                  z = read(s, &workbuf, 4095);
#ifdef DEBUG_MODE_ON
                  printf("DEBUG: dumping server response\n" );
                  API_DEBUG_dump_api( (API_Header_Def *)api_bufptr );
#endif
                  if (z == -1) die("read(2) - command response");
                  workbuf[z] = '\0';
			      /* Check for API response or text response */
                  if (memcmp(api_bufptr->API_EyeCatcher, API_EYECATCHER, 4) == 0) {
				     if (memcmp(api_bufptr->API_Command_Response,API_RESPONSE_ERROR, API_RESPCODE_LEN) == 0) {
					    API_Set_LF_Fields( api_bufptr );
			            printf("%s\n", api_bufptr->API_Data_Buffer);
				     }
				     else if (memcmp(api_bufptr->API_Command_Response,API_RESPONSE_DISPLAY, API_RESPCODE_LEN) == 0) {
				        /* Display data returned, no other action needed */
					    API_Set_LF_Fields( api_bufptr );
			            printf("%s\n", api_bufptr->API_Data_Buffer);
				     }
				     else if (memcmp(api_bufptr->API_Command_Response,API_RESPONSE_DATA, API_RESPCODE_LEN) == 0) {
					    API_Set_LF_Fields( api_bufptr );
				        data_response_process( api_bufptr );
				     }
				     else {
			            /* ERROR, response returned is not legal */
					    printf( "****ERROR**** Illegal response code %s in reply from server task !\n",
                                api_bufptr->API_Command_Response );
				     }
		          }
			      else {
                     printf("*E* No API eyecatcher returned, displaying data received and stopping...\n" );
                     printf( "%s\n", workbuf );
					 printf( "*E* Server has probably been stopped !\n" );
					 stop_now = 1;
					 /* it didn't exit the loop with stop_now set, so exit
					  * ourselves ! */
                     disconnect_from_server( s ); /* close old connection */
					 return( 1 ); /* something wrong */
			      }
			   } /* while more data follows */
         }  /* if command result == 1 */
		 else if (command_result == 2) {    /* no response expected, keep going */
				 /* no action needed */
		 }
		 else if (command_result == 3) {    /* Open request to be processed */
            if ((memcmp(workbuf,"OPEN INFO",9) == 0) || (memcmp(workbuf,"open info",9) == 0) ) {
               printf( "*I*: Currently connected to %s on port %d\n", last_addr, last_port );
            }
            else {
               next_port = last_port;    /* allow a default on port number */
			   strcpy( next_addr, "" );  /* no default on next address */
			   command_result = parse_open_command( (char *)&workbuf, &next_port, (char *)&next_addr );
			   if (command_result == 1) { /* passes syntax checks */
                   disconnect_from_server( s ); /* close old connection */
                   if ((command_result = connect_to_server( next_port, next_addr, &s )) == 0) {
                      /* connection failed, open the last connection again */
                      if ((command_result = connect_to_server( last_port, last_addr, &s )) == 0) {
                         printf( "*E*: Unable to reconnect to original server, aborting interface program\n" );
                         exit( 1 ); /* failed, die out */
                      }
                   }
				   else {
                      /* connected to new server, save new recover defaults */
                      strncpy( last_addr, next_addr, 59 );
                      last_port = next_port;
                      printf( "*I*: Now connected to %s on port %d\n", last_addr, last_port );
                   }
               }
            } /* not an INFO */
		 }
         else {                             /* we should shutdown */
            stop_now = 1;
         }
      }          /* not NULL */
      else {     /* else if null, assume eof and stop */
         stop_now = 1;
      }
   }      /* while */

   /* close the socket and exit */
   disconnect_from_server( s );
   return(0);
}
