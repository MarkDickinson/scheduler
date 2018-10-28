#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "utils.h"
#include "config.h"
#include "scheduler.h"

/* ---------------------------------------------------
   CONFIG_update_noreload
   Update the configuration data. Same as CONFIG_update
   EXCEPT we do not call CONFIG_Initialise. This is
   only called from CONFIG_Initialise, which cannot
   use CONFIG_update or we loop until memory is
   exhausted.
   --------------------------------------------------- */
int CONFIG_update_noreload( internal_flags * flagbuffer ) {
   int lasterror;

   if ( (config_handle = fopen( config_file, "rb+" )) == NULL ) {
      UTILS_set_message( "Open of configuration file failed" );
      return( 0 );   
   }
   lasterror = fseek( config_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      fclose( config_handle );
	  config_handle = NULL;
      UTILS_set_message( "Seek to start of configuration file failed" );
      return( 0 );
   }
   lasterror = fwrite( flagbuffer, sizeof(internal_flags), 1, config_handle );
   if (ferror(config_handle)) {
      fclose( config_handle );
	  config_handle = NULL;
      UTILS_set_message( "Write error on configuration file" );
      return( 0 );
   }

   fflush( config_handle );
   fclose( config_handle );
   config_handle = NULL;

   return( 1 );  /* Done, we don't reload */
} /* CONFIG_update_noreload */

/* ---------------------------------------------------
   CONFIG_Initialise
   Read the configuration data in, close file when done.
   --------------------------------------------------- */
int CONFIG_Initialise( internal_flags * flagbuffer ) {
   int lasterror;
   char datetime[JOB_DATETIME_LEN+1];
   time_t time_work, time_work2;
   char *ptr1;

   if ( (config_handle = fopen( config_file, "ab+" )) == NULL ) {
      perror( "Open of configuration file failed:" );
      myprintf("*ERR: FE001-Unable to open config file %s\n", config_file );
      return( 0 );   
   }
   fseek( config_handle, 0L, SEEK_SET );
   lasterror = fread( flagbuffer, sizeof(internal_flags), 1, config_handle  );
   if (ferror(config_handle)) {
      fclose( config_handle );
	  config_handle = NULL;
      myprintf("*ERR: FE002-Read error on configuration file %s\n", config_file );
      return( 0 );
   }
   if (feof( config_handle )) {
      fclose( config_handle );
	  config_handle = NULL;
      myprintf( "WARN: FW001-Empty configuration file %s, initialising with defaults\n", config_file );
      strncpy( flagbuffer->version, config_version, 40 );
      flagbuffer->catchup_flag = '2';    /* default is catchup all days */
      flagbuffer->log_level = 2;
      memcpy( flagbuffer->new_day_time, "06:00", 5 ); 
      strncpy( flagbuffer->last_new_day_run, "20020101 06:00", 17 );
      time_work = UTILS_timenow_timestamp();
      /* then ensure its after the current time, newday is in the future */
      time_work2 = time(0);
      while (time_work < time_work2) {
         time_work = time_work + (60 * 60 * 24);
      }
      UTILS_datestamp( (char *)&datetime, time_work );
      ptr1 = (char *)&datetime;
      ptr1 = ptr1 + 9; /* skip yyyymmdd_ */
      strncpy( ptr1, flagbuffer->new_day_time, 5 );
      strncpy( flagbuffer->next_new_day_run, datetime, 17 );
      flagbuffer->enabled = '1';
      flagbuffer->newday_action = '1'; /* use queuing as default */
      /* default tracing flags are off */
      flagbuffer->debug_level.server = 0;
      flagbuffer->debug_level.utils = 0;
      flagbuffer->debug_level.jobslib = 0;
      flagbuffer->debug_level.api = 0;
      flagbuffer->debug_level.alerts = 0;
      flagbuffer->debug_level.calendar = 0;
      flagbuffer->debug_level.schedlib = 0;
      flagbuffer->debug_level.bulletproof = 0;
      flagbuffer->debug_level.user = 0;
      flagbuffer->debug_level.memory = 0;
      flagbuffer->debug_level.show_deleted_schedjobs = 0;

      /* remote console stuff for alerts */
      flagbuffer->central_alert_console_active = 0;
      flagbuffer->use_central_alert_console = 'N';
      strcpy(flagbuffer->remote_console_ipaddr ,"127.0.0.1" );
      strcpy(flagbuffer->remote_console_port,"9003" );
      flagbuffer->remote_console_socket = 0;
      /* stuff for if a local utility will raise alerts */
      strcpy(flagbuffer->local_execprog_raise, "/opt/dickinson/alert_management/scripts/raise_alert.sh" );
      strcpy(flagbuffer->local_execprog_cancel, "/opt/dickinson/alert_management/scripts/end_alert.sh" );

      /* Advise Last Newday time we set */
      myprintf( "INFO: FI001-Last newday job completed time is set to %s\n", flagbuffer->last_new_day_run ); 
      return( CONFIG_update_noreload(flagbuffer) );
   }
   fclose( config_handle );
   config_handle = NULL;
   if (flagbuffer->log_level > 1) {
      myprintf( "INFO: FI002-Configuration loaded from %s ok\n", config_file );
   }

   /* Check for a version update !, make all necessary changes and save new
    * version config file. */
   if (strncmp( flagbuffer->version, config_version, 40 ) != 0) {
      strncpy( flagbuffer->version, config_version, 40 );
      myprintf( "WARN: FW003-Version change detected, updating config version !\n" );
      lasterror = CONFIG_update_noreload(flagbuffer);
   }

   if (flagbuffer->log_level > 1) {
      myprintf( "INFO: FI004-Last completed newday execution was %s\n", flagbuffer->last_new_day_run ); 
   }

   flagbuffer->isdst_flag = CONFIG_check_dst_flag();
   lasterror = CONFIG_update_noreload( flagbuffer ); /* needed to save dst changes */
   return( 1 );
} /* CONFIG_Initialise */

/* ---------------------------------------------------
   CONFIG_update
   Update the configuration data.
   --------------------------------------------------- */
int CONFIG_update( internal_flags * flagbuffer ) {
   int lasterror;

   if ( (config_handle = fopen( config_file, "rb+" )) == NULL ) {
      UTILS_set_message( "Open of configuration file failed" );
      return( 0 );   
   }
   lasterror = fseek( config_handle, 0, SEEK_SET );
   if (lasterror != 0) {
      fclose( config_handle );
	  config_handle = NULL;
      UTILS_set_message( "Seek to start of configuration file failed" );
      return( 0 );
   }
   lasterror = fwrite( flagbuffer, sizeof(internal_flags), 1, config_handle );
   if (ferror(config_handle)) {
      fclose( config_handle );
	  config_handle = NULL;
      UTILS_set_message( "Write error on configuration file" );
      return( 0 );
   }

   fflush( config_handle );
   fclose( config_handle );
   config_handle = NULL;

   if (flagbuffer->log_level > 1) {
      myprintf( "INFO: FI005-Reloading configuration data\n" );
   }
   return( CONFIG_Initialise(flagbuffer) );  /* Reload global data */
} /* CONFIG_update */

/* ---------------------------------------------------
   CONFIG_check_dst_flag
   This mucking about is necessary as the external system
   daylight variable just doesn't provide what I need.
   --------------------------------------------------- */
int CONFIG_check_dst_flag( void ) {
   struct tm time_var;
   struct tm *time_var_ptr;
   time_t time_number;

   time_number = time(0);
   time_var_ptr = (struct tm *)&time_var;
   time_var_ptr = localtime( &time_number );    /* time now as tm */

   return( time_var_ptr->tm_isdst );
} /* CONFIG_check_dst_flag */
