#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "debug_levels.h"
#include "api.h"
#include "utils.h"
#include "bulletproof.h"
#include "schedlib.h"

/* *****************************************************************************
   Initialise an API buffer record to default values.
   ***************************************************************************** */
void API_init_buffer( API_Header_Def *pAPI_buffer ) {
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.api >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered API_init_buffer\n" );
   }
   strcpy( pAPI_buffer->API_EyeCatcher, API_EYECATCHER );
   strcpy( pAPI_buffer->API_System_Name, "" );               
   strcpy( pAPI_buffer->API_System_Version, API_VERSION ); 
   strcpy( pAPI_buffer->API_Command_Number, API_CMD_STATUS );
   strcpy( pAPI_buffer->API_Command_Response, "000" );  
   pAPI_buffer->API_More_Data_Follows = '0';
   pAPI_buffer->API_Data_Buffer[0] = '\0';
   API_set_datalen( pAPI_buffer, 0 );
   API_sethostname_field( pAPI_buffer );  /* added 20020920 */
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.api >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving API_init_buffer\n" );
   }
} /* API_init_buffer */

/* *****************************************************************************
   Set the hostname in an API buffer so the reciever knows which host the data
   is being sent from.
   ***************************************************************************** */
void API_sethostname_field( API_Header_Def *pAPI_buffer ) {
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.api >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered API_sethostname_field\n" );
   }
   gethostname( pAPI_buffer->API_hostname, API_HOSTNAME_LEN );
   pAPI_buffer->API_hostname[API_HOSTNAME_LEN] = '\0';
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.api >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving API_sethostname_field\n" );
   }
} /* API_sethostname_field */

/* *****************************************************************************
   Set the buffer length and terminate the buffer with a \0(null) character.
   ***************************************************************************** */
void API_set_datalen( API_Header_Def *pAPI_buffer, long bufflen ) {
   char *pEndPtr;
   char localbuffer[DATA_LEN_FIELD+1];

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.api >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered API_set_datalen\n" );
   }

   UTILS_number_to_string( bufflen, (char *)&localbuffer, DATA_LEN_FIELD );
   strncpy( pAPI_buffer->API_Data_Len, localbuffer, DATA_LEN_FIELD );
   pEndPtr = (char *)&pAPI_buffer->API_Data_Buffer;
   pEndPtr = pEndPtr + bufflen;
   *pEndPtr = '\0';

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.api >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving API_set_datalen\n" );
   }
} /* API_set_datalen */

/* *****************************************************************************
 * Used for DEBUGGING. Dump the contents of the API buffer.
 * NOTE: Do not use the memorylib routines here, as the api module is also
 * needed by jobsched_cmd which doesn't use the memory library.
   ***************************************************************************** */
void API_DEBUG_dump_api( API_Header_Def *pAPI_buffer ) {
		/* 
		 * api eyecatcher
		   char API_System_Name[10];
		   char API_System_Version[10];
		   char API_Command_Number[API_CMD_LEN+1];
		   char API_Command_Response[4];
		   char API_More_Data_Follows; 
		   char API_Data_Len[DATA_LEN_FIELD+1];
		   char API_Data_Buffer[MAX_API_DATA_LEN];
		*/
   char buffer[4097];
   int  i, iInt;
   API_Header_Def * local_API_buffer;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.api >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered API_DEBUG_dump_api\n" );
   }

   myprintf( "------------------------------------------------------\n" );
   myprintf( "API_DEBUG_dump_api...\n" );
   if (memcmp(pAPI_buffer->API_EyeCatcher, API_EYECATCHER, 4) != 0) { 
      myprintf( "Data buffer is NOT an API formatted buffer, displaying first string part...\n" );
      strncpy( buffer, pAPI_buffer->API_EyeCatcher, 4096 );
      myprintf( "%d bytes...\n%s\n", strlen(buffer), buffer );
      myprintf( "------------------------------------------------------\n" );
      return;
   }

   if ((local_API_buffer = malloc(sizeof(API_Header_Def))) == NULL) {
		   myprintf( "*ERR: SE004-Unable to allocate memory required to print API buffer\n" );
		   myprintf( "*ERR: SE004-API_DEBUG_dump_api not performed.\n" );
		   return;
   }

   memcpy( local_API_buffer->API_EyeCatcher, pAPI_buffer->API_EyeCatcher, sizeof(API_Header_Def) );
   API_Nullterm_Fields( local_API_buffer );

   /* note: strncpy does not put a null on the end of the string if the string
    * does not contain a null in the n bytes being copies, so we have to ensure
    * that we add one.
    * */
   strncpy( buffer, local_API_buffer->API_System_Name, 9 );
   buffer[9] = '\0';
   myprintf( "  System Name : %s\n", buffer );
   strncpy( buffer, local_API_buffer->API_System_Version, 9 );
   buffer[9] = '\0';
   myprintf( "  API Version : %s\n", buffer );
   strncpy( buffer, local_API_buffer->API_Command_Number, API_CMD_LEN );
   buffer[API_CMD_LEN] = '\0';
   myprintf( "  API Command : %s\n", buffer );
   strncpy( buffer, local_API_buffer->API_Command_Response, 3 );
   buffer[3] = '\0';
   myprintf( "  API Response: %s\n", buffer );
   myprintf( "  Data Follows: %c\n", local_API_buffer->API_More_Data_Follows );
   strncpy( buffer, local_API_buffer->API_Data_Len, DATA_LEN_FIELD ); 
   buffer[DATA_LEN_FIELD] = '\0';
   iInt = atoi( buffer );
   myprintf( "  databuf len : %d\n", iInt );
   if (iInt > 0) {
      if (iInt > 4096) {
         iInt = 4096;
      }
      myprintf( "  adjusted len: %d\n", iInt );
/*      strncpy( buffer, local_API_buffer->API_Data_Buffer, iInt );   */
      memcpy( buffer, local_API_buffer->API_Data_Buffer, iInt );
      for (i = 0; i < iInt; i++) {
         if (buffer[i] == '\0') { buffer[i] = '^'; }
      }
      buffer[iInt] = '\0';
      myprintf( "  Data Buffer : %s\n", buffer );
   }
   myprintf( "------------------------------------------------------\n" );
   
   free( local_API_buffer );

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.api >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving API_DEBUG_dump_api\n" );
   }
} /* API_DEBUG_dump_api */

/* *****************************************************************************
 * As we can't pass nulls through the TCPIP interface yet due to the way we
 * coded it we use this to add nulls to the end of strings we intend to do work
 * with, which is every string in the API buffer.
   ***************************************************************************** */
void API_Nullterm_Fields( API_Header_Def * api_bufptr ) {
		/* 
		   char API_System_Name[10];
		   char API_System_Version[10];
		   char API_Command_Number[API_CMD_LEN+1];
		   char API_Command_Response[4];
		   char API_More_Data_Follows; 
		   char API_Data_Len[DATA_LEN_FIELD+1];
		   char API_Data_Buffer[MAX_API_DATA_LEN];
		*/
		int i, j;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.api >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered API_Nullterm_Fields\n" );
   }

   api_bufptr->API_System_Name[9] = '\0';
   for (i = 0; i < 9; i++) {
		   if (api_bufptr->API_System_Name[i] == '^') {
				   api_bufptr->API_System_Name[i] = '\0';
		   }
   }

   api_bufptr->API_System_Version[9] = '\0';
   for (i = 0; i < 9; i++) {
		   if (api_bufptr->API_System_Version[i] == '^') {
				   api_bufptr->API_System_Version[i] = '\0';
		   }
   }

   api_bufptr->API_Command_Number[API_CMD_LEN] = '\0';
   api_bufptr->API_Command_Response[3] = '\0';
   api_bufptr->API_Data_Len[DATA_LEN_FIELD] = '\0';
   api_bufptr->API_Data_Buffer[MAX_API_DATA_LEN - 2] = '\0';

   j = atoi( api_bufptr->API_Data_Len );
   if (j >= (MAX_API_DATA_LEN - 1)) {
		   j = MAX_API_DATA_LEN - 1;
   }
   for (i = 0; i < j; i++) {
		   if (api_bufptr->API_Data_Buffer[i] == '^') {
				   api_bufptr->API_Data_Buffer[i] = '\0';
		   }
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.api >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving API_Nullterm_Fields\n" );
   }
} /* API_Nullterm_Fields */

/* --------------------------------------------------------------
   -------------------------------------------------------------- */
int API_add_to_api_databuf( API_Header_Def * pApi_buffer, char * datastr,
				            int datalen, FILE * tx ) {
   int iInt, z, replylen;
   char *sptr, *eptr;
   char local_buffer[DATA_LEN_FIELD + 1];
   char local_datastr[2049];
			  
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.api >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered API_add_to_api_databuf\n" );
   }

   if (datalen > 2048) {
		   myprintf( "*ERR: SE001-API_add_to_api_databuf: Reply data discarded, string to add was > 2K limit\n" );
		   return( 1 );
   }

   strncpy( local_buffer, pApi_buffer->API_Data_Len, DATA_LEN_FIELD );
   local_buffer[DATA_LEN_FIELD] = '\0';
   iInt = atoi( local_buffer );
   if ( (iInt + datalen) >= MAX_API_DATA_LEN ) {
       /* 0 = first buffer, indicate more data will follow now */
       if (pApi_buffer->API_More_Data_Follows == '0') {
           pApi_buffer->API_More_Data_Follows = '1';
       }
       /* else set to 3 which is continuation with still more data following */
       else {
           pApi_buffer->API_More_Data_Follows = '3';
       }
       sptr = (char *)&pApi_buffer->API_EyeCatcher;
       eptr = (char *)&pApi_buffer->API_Data_Buffer;
       replylen = (eptr - sptr) + iInt;
       API_sethostname_field( pApi_buffer ); /* set the hostname doing the write */
       z = fwrite( pApi_buffer, replylen, 1, tx );
       if (ferror(tx)) {
          myprintf( "*ERR: SE002-API_add_to_api_databuf, Unable to write data buffer block of a continuation stream\n" );
          return( 1 );  /* ERROR OCCURRED */
       }
       if (pSCHEDULER_CONFIG_FLAGS.debug_level.api >= DEBUG_LEVEL_IO) {
          myprintf( "DEBUG: API_add_to_api_databuf, flushed first buffer, more to write to client\n" );
       }
       /* if the write was OK clear the data buffer */
       pApi_buffer->API_Data_Buffer[0] = '\0';
       iInt = 0;
    }

    strncpy( local_datastr, datastr, datalen );
    local_datastr[datalen] = '\0';
    strcat( pApi_buffer->API_Data_Buffer, local_datastr );
    iInt = strlen( pApi_buffer->API_Data_Buffer );
    API_set_datalen( pApi_buffer, iInt );
	
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.api >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving API_add_to_api_databuf, bufferlen=%d \n", iInt );
   }

   return( 0 );  /* no error */
} /* API_add_to_api_databuf */

/* --------------------------------------------------------------
   -------------------------------------------------------------- */
int API_flush_buffer( API_Header_Def * pApi_buffer, FILE * fhandle ) {
   int replylen, z;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.api >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered API_flush_buffer\n" );
   }

    if (pApi_buffer->API_More_Data_Follows != '0') {
	   /* we are in a continuation stream, set to 2 as we are the
	   * last packet and no more follows */
	   pApi_buffer->API_More_Data_Follows = '2';
    }
	
    replylen = BULLETPROOF_API_Buffer( pApi_buffer );
    if (pSCHEDULER_CONFIG_FLAGS.debug_level.api >= DEBUG_LEVEL_IO) {
       myprintf( "DEBUG: Writing buffer final flush response of %d bytes (API_flush_buffer)\n", replylen );
	}
    API_sethostname_field( pApi_buffer ); /* set the hostname doing the write */
    z = fwrite( (char *)pApi_buffer, replylen, 1, fhandle );
    if (ferror(fhandle)) {
        return( 1 );  /* ERROR OCCURRED */
    }
	
   if (pSCHEDULER_CONFIG_FLAGS.debug_level.api >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving API_flush_buffer\n" );
   }
	return( 0 ); /* no error */
} /* API_flush_buffer */

/* *****************************************************************************
 * As we can't pass nulls through the TCPIP interface yet due to the way we
 * coded it we use this to add nulls to the end of strings we intend to do work
 * with, which is every string in the API buffer.
 * This differs from API_Nullterm_Fields in that we expect our buffer to be
 * a response display buffer, so we place a LF at the first char before null
 * filling rather than a total null fill done by API_Nullterm_Fields.
 * Newline fields are indicated by the ^ character in the data buffer, we set 
 * all ^ characters to '\n' in the response data buffer.
   ***************************************************************************** */
void API_Set_LF_Fields( API_Header_Def * api_bufptr ) {
		/* 
		   char API_System_Name[10];
		   char API_System_Version[10];
		   char API_Command_Number[API_CMD_LEN+1];
		   char API_Command_Response[4];
		   char API_More_Data_Follows; 
		   char API_Data_Len[DATA_LEN_FIELD+1];
		   char API_Data_Buffer[MAX_API_DATA_LEN];
		*/
		int i, j;
		int found_one;

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.api >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Entered API_Set_LF_Fields\n" );
   }

   api_bufptr->API_System_Name[9] = '\0';
   for (i = 0; i < 9; i++) {
		   if (api_bufptr->API_System_Name[i] == '^') {
				   api_bufptr->API_System_Name[i] = '\0';
		   }
   }

   api_bufptr->API_System_Version[9] = '\0';
   for (i = 0; i < 9; i++) {
		   if (api_bufptr->API_System_Version[i] == '^') {
				   api_bufptr->API_System_Version[i] = '\0';
		   }
   }

   api_bufptr->API_Command_Number[API_CMD_LEN] = '\0';
   api_bufptr->API_Command_Response[3] = '\0';
   api_bufptr->API_Data_Len[DATA_LEN_FIELD] = '\0';
/* CRITICAL !!!!  The below line causes segmentation faults in jobsched_cmd response processing !!!!
   api_bufptr->API_Data_Buffer[MAX_API_DATA_LEN - 2] = '\0';
*/

   j = atoi( api_bufptr->API_Data_Len );
   if (j == 0) { j = strlen(api_bufptr->API_Data_Len); }
   if (j >= (MAX_API_DATA_LEN - 2)) { j = MAX_API_DATA_LEN - 2; }
   api_bufptr->API_Data_Buffer[j+1] = '\0';
   for (i = 0; i < j; i++) {
      if (api_bufptr->API_Data_Buffer[i] == '^') {
         api_bufptr->API_Data_Buffer[i] = '\n';
      }
   }

   if (pSCHEDULER_CONFIG_FLAGS.debug_level.api >= DEBUG_LEVEL_PROC) {
      myprintf( "DEBUG: Leaving API_Set_LF_Fields\n" );
   }
} /* API_Set_LF_Fields */

/* *****************************************************************************
 * Used for DEBUGGING. Dump the contents of the header struct.
   ***************************************************************************** */
void API_DEBUG_dump_job_header( char * hdr_buffer ) {
		/* 
		 *  char jobnumber[JOB_NUMBER_LEN+1];
		 *  char jobname[JOB_NAME_LEN+1];
		 *  char info_flag; 
		*/
   job_header_def * local_header_ptr;
   local_header_ptr = (job_header_def *)hdr_buffer;

   myprintf( "------------------------------------------------------\n" );
   myprintf( "API_DEBUG_dump_job_header...\n" );
   myprintf( "JOB NAME:%s\nJOB NUMBER:%s\nFLAG:%c\n",
		   local_header_ptr->jobname,
		   local_header_ptr->jobnumber,
		   local_header_ptr->info_flag );
   myprintf( "------------------------------------------------------\n" );
} /* API_DEBUG_dump_job_header */
