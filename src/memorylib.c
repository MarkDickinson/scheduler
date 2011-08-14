#include <string.h>
#include <stdlib.h>
#include "memorylib.h"
#include "debug_levels.h"
#include "utils.h"
#include "schedlib.h"

/* Globals managed by the memory library */
int  mem_alert_outstanding;
long malloc_count, free_count;

/* use to manage our memory tables */
typedef struct {
   void * memptr;
   int  memused;
   char tag_owner[MEMORY_TAGOWNER_LEN+1];
} memory_history_def;

memory_history_def memory_history_table[MEMORY_MAX_CHAIN_LEN];

/* --------------------------------------------------
 * Initialise the memory values we are using.
   -------------------------------------------------- */
void MEMORY_initialise_malloc_counters( void ) {
   int i;
   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.memory >= DEBUG_LEVEL_PROC) &&
        (!(pSCHEDULER_CONFIG_FLAGS.debug_level.memory == DEBUG_LEVEL_MEMONLY)) ) {
      myprintf( "DEBUG: Entered MEMORY_initialise_malloc_counters\n" );
   }
   mem_alert_outstanding = 0;
   malloc_count = free_count = 0;
   for (i = 0; i < MEMORY_MAX_CHAIN_LEN; i++ ) {
      memory_history_table[i].memptr = NULL;
      memory_history_table[i].memused = 0;
	  strncpy( memory_history_table[i].tag_owner, "NULL", MEMORY_TAGOWNER_LEN );
   }
   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.memory >= DEBUG_LEVEL_PROC) &&
        (!(pSCHEDULER_CONFIG_FLAGS.debug_level.memory == DEBUG_LEVEL_MEMONLY)) ) {
      myprintf( "DEBUG: Leaving MEMORY_initialise_malloc_counters\n" );
   }
} /* end MEMORY_initialise_malloc_counters */

/* --------------------------------------------------
 * Display the current memory chain to the log file.
 * Will generally only be called internally, but is
 * made externally avaiable so a user may request
 * a display sent to the log as they wish.
   -------------------------------------------------- */
void MEMORY_log_memory_useage( void ) {
   int i, usedslots;
   long total_memory_malloced;

   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.memory >= DEBUG_LEVEL_PROC) &&
        (!(pSCHEDULER_CONFIG_FLAGS.debug_level.memory == DEBUG_LEVEL_MEMONLY)) ) {
      myprintf( "DEBUG: Entered MEMORY_log_memory_useage\n" );
   }

   usedslots = 0;
   total_memory_malloced = 0;

   myprintf( "INFO: MI001-Memory display requested\n" );
   for (i = 0; i < MEMORY_MAX_CHAIN_LEN; i++ ) {
      if (memory_history_table[i].memptr != NULL) {
         usedslots++;
		 myprintf( "INFO: MI002-  Caller=%s, Memory used=%d bytes\n",
                    memory_history_table[i].tag_owner,
                    memory_history_table[i].memused );
         total_memory_malloced = total_memory_malloced + memory_history_table[i].memused;
	  }
   }
   if (usedslots == 0) {
      myprintf( "INFO: MI005-No memory slots are currently in use.\n" );
   }
   myprintf( "INFO: MI003-Using %d slots, out of %d total slots, currently malloced %d bytes in total\n",
              usedslots, MEMORY_MAX_CHAIN_LEN, total_memory_malloced );
   myprintf( "INFO: MI006-Since last newday - %d malloc and %d free calls from stack calls\n", malloc_count, free_count );
   myprintf( "INFO: MI004-Memory display ended\n" );

   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.memory >= DEBUG_LEVEL_PROC) &&
        (!(pSCHEDULER_CONFIG_FLAGS.debug_level.memory == DEBUG_LEVEL_MEMONLY)) ) {
      myprintf( "DEBUG: Leaving MEMORY_log_memory_useage\n" );
   }
} /* MEMORY_log_memory_useage */

/* --------------------------------------------------
 * Reset the malloc counters on each newday.
   -------------------------------------------------- */
void MEMORY_reset_counters_on_newday( void ) {
   malloc_count = free_count = 0;
} /* MEMORY_reset_counters_on_newday */

/* --------------------------------------------------
 * Called internally by MEMORY_malloc.
 * Return a free memory_history slot if one is 
 * available, else return a number > max slots for
 * the MEMORY_malloc call to test on.
   -------------------------------------------------- */
int MEMORY_find_free_slot( void ) {
  int i, found;
   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.memory >= DEBUG_LEVEL_PROC) &&
        (!(pSCHEDULER_CONFIG_FLAGS.debug_level.memory == DEBUG_LEVEL_MEMONLY)) ) {
      myprintf( "DEBUG: Entered MEMORY_find_free_slot\n" );
   }
  found = MEMORY_MAX_CHAIN_LEN + 1;
  i = 0;
  while ((i < MEMORY_MAX_CHAIN_LEN) && (found > MEMORY_MAX_CHAIN_LEN)) {
     if (memory_history_table[i].memptr == NULL) { found = i; }
	 else { i++; }
  }
   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.memory >= DEBUG_LEVEL_PROC) &&
        (!(pSCHEDULER_CONFIG_FLAGS.debug_level.memory == DEBUG_LEVEL_MEMONLY)) ) {
      myprintf( "DEBUG: Leaving MEMORY_find_free_slot\n" );
   }
  return (found);
} /* MEMORY_find_free_slot */

/* --------------------------------------------------
 * Called internally by MEMORY_free.
 * Return the memory slot matching the memory pointer,
 * else return a number > max slots for
 * the MEMORY_free call to test on.
   -------------------------------------------------- */
int MEMORY_find_pointers_slot( void * memptr ) {
  int i, found;
   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.memory >= DEBUG_LEVEL_PROC) &&
        (!(pSCHEDULER_CONFIG_FLAGS.debug_level.memory == DEBUG_LEVEL_MEMONLY)) ) {
      myprintf( "DEBUG: Entered MEMORY_find_pointers_slot\n" );
   }
  found = MEMORY_MAX_CHAIN_LEN + 1;
  i = 0;
  while ((i < MEMORY_MAX_CHAIN_LEN) && (found > MEMORY_MAX_CHAIN_LEN)) {
     if (memory_history_table[i].memptr == memptr) { found = i; }
	 else { i++; }
  }
   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.memory >= DEBUG_LEVEL_PROC) &&
        (!(pSCHEDULER_CONFIG_FLAGS.debug_level.memory == DEBUG_LEVEL_MEMONLY)) ) {
      myprintf( "DEBUG: Leaving MEMORY_find_pointers_slot\n" );
   }
  return (found);
} /* MEMORY_find_pointers_slot */

/* --------------------------------------------------
 * Only ever called internally when memory threshold
 * checks are being done. These are only done when
 * memory is allocated or freed, and only if the
 * loglevel is allowing warning messages.
   -------------------------------------------------- */
long MEMORY_total_used( void ) {
   int i;
   long mem_used;

   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.memory >= DEBUG_LEVEL_PROC) &&
        (!(pSCHEDULER_CONFIG_FLAGS.debug_level.memory == DEBUG_LEVEL_MEMONLY)) ) {
      myprintf( "DEBUG: Entered MEMORY_total_used\n" );
   }

   mem_used = 0;
   for (i = 0; i < MEMORY_MAX_CHAIN_LEN; i++ ) {
      if (memory_history_table[i].memptr != NULL) {
         mem_used = mem_used + memory_history_table[i].memused;
	  }
   }

   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.memory >= DEBUG_LEVEL_PROC) &&
        (!(pSCHEDULER_CONFIG_FLAGS.debug_level.memory == DEBUG_LEVEL_MEMONLY)) ) {
      myprintf( "DEBUG: Leaving MEMORY_total_used\n" );
   }

   return( mem_used );
} /* MEMORY_total_used */

/* --------------------------------------------------
 * Allocate the memory requested by the caller and
 * return the pointer to the memory block (or NULL
 * if an error).
 * Record the details about the memory we have allocated,
 * this provides a debugging toolkit if needed.
 *
 * Notes: memory allocated this way is expected to be
 * released when the caller has finished with it by the
 * caller using MEMORY_free.
   -------------------------------------------------- */
void * MEMORY_malloc( int mem_needed, char *caller ) {
   void *memptr;
   int slottouse;
   long total_mem_used;

   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.memory >= DEBUG_LEVEL_PROC) &&
        (!(pSCHEDULER_CONFIG_FLAGS.debug_level.memory == DEBUG_LEVEL_MEMONLY)) ) {
      myprintf( "DEBUG: Entered MEMORY_malloc\n" );
   }

   memptr = NULL;
 
   slottouse = MEMORY_find_free_slot();
   if (slottouse >= MEMORY_MAX_CHAIN_LEN) {
      myprintf( "*ERR: ME001-maximum malloc chain exceeded, check code.\n" );
	  MEMORY_log_memory_useage();
   }
   else {
      memptr = (void *)malloc(mem_needed);
      if (memptr == NULL) {
         myprintf( "*ERR: ME002-malloc of %d bytes failed for %s\n", mem_needed, caller );
      }
      else {
         /* record memory use */
         memory_history_table[slottouse].memptr = memptr;
         memory_history_table[slottouse].memused = mem_needed;
         strncpy( memory_history_table[slottouse].tag_owner, caller, MEMORY_TAGOWNER_LEN );
         if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.memory >= DEBUG_LEVEL_MEM) ||
              (pSCHEDULER_CONFIG_FLAGS.debug_level.memory == DEBUG_LEVEL_MEMONLY)) {
            myprintf( "DEBUG: malloc of %d bytes %s OK\n", mem_needed, caller );
         }
	 malloc_count++;
	 /* prevent an overflow */
         if (malloc_count >= MEMORY_RESET_COUNTERS_AT) {
            malloc_count = 0;
            if (pSCHEDULER_CONFIG_FLAGS.log_level > 0) { /* allowed to log warnings */
               myprintf( "WARN: MW003-malloc counter reset to 0 to avoid overflow, reached %d\n", MEMORY_RESET_COUNTERS_AT );
            }
         }
      } /* mem was allocated */
   } /* else slot was free */

   /* Always check memory thresholds against what we expect the max ever to be */
   if (pSCHEDULER_CONFIG_FLAGS.log_level > 0) { /* allowed to log warnings */
      total_mem_used = MEMORY_total_used();
      if (total_mem_used >= MEMORY_MALLOC_WARN) {
         myprintf( "WARN: MW001-Memory useage now at %d, warn threshold is %d\n",
                   total_mem_used, MEMORY_MALLOC_WARN );
	     MEMORY_log_memory_useage();
	     mem_alert_outstanding = 1;
      }
   }

   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.memory >= DEBUG_LEVEL_PROC) &&
        (!(pSCHEDULER_CONFIG_FLAGS.debug_level.memory == DEBUG_LEVEL_MEMONLY)) ) {
      myprintf( "DEBUG: Leaving MEMORY_malloc\n" );
   }

   return( memptr ); /* let the caller know where its memory is */
} /* end MEMORY_malloc */

/* --------------------------------------------------
 * Release memory allocated by a prior call to the
 * MEMORY_malloc procedure.
   -------------------------------------------------- */
void MEMORY_free( void * memptr ) {
   int slottouse;
   long total_mem_used;

   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.memory >= DEBUG_LEVEL_PROC) &&
        (!(pSCHEDULER_CONFIG_FLAGS.debug_level.memory == DEBUG_LEVEL_MEMONLY)) ) {
      myprintf( "DEBUG: In MEMORY_free\n" );
   }

   if (memptr != NULL) {
      slottouse = MEMORY_find_pointers_slot( memptr );
      if (slottouse >= MEMORY_MAX_CHAIN_LEN) {
         myprintf( "*ERR: ME003-No slot matching memory pointer, freeing memory anyway (unknown mem size released).\n" );
         free( memptr );  
      }
      else {
         free( memptr );  
         /* record memory released */
         if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.memory >= DEBUG_LEVEL_MEM) ||
              (pSCHEDULER_CONFIG_FLAGS.debug_level.memory == DEBUG_LEVEL_MEMONLY)) {
            myprintf( "DEBUG: free   of %d bytes %s OK\n",
                      memory_history_table[slottouse].memused,
                      memory_history_table[slottouse].tag_owner );
         }
         /* record memory slot is free */
         memory_history_table[slottouse].memptr = NULL;
         memory_history_table[slottouse].memused = 0;
         strncpy( memory_history_table[slottouse].tag_owner, "NULL", MEMORY_TAGOWNER_LEN );
	 free_count++;
	 /* prevent an overflow */
         if (free_count >= MEMORY_RESET_COUNTERS_AT) {
            free_count = 0;
            if (pSCHEDULER_CONFIG_FLAGS.log_level > 0) { /* allowed to log warnings */
               myprintf( "WARN: MW004-free counter reset to 0 to avoid overflow, reached %d\n", MEMORY_RESET_COUNTERS_AT );
            }
         }
      }
   } /* if memptr != NULL */
   else {
      myprintf( "*ERR: ME004-null pointer passed to MEMORY_free, ignored.\n" );
   }

   /* Always check memory thresholds against what we expect the max ever to be */
   if (mem_alert_outstanding != 0) {
      /* will onlyhave been recorded if we are logging warnings, so ok to clear */
      total_mem_used = MEMORY_total_used();
      if (total_mem_used < MEMORY_MALLOC_WARN) {
         myprintf( "WARN: MW002-Memory useage now below warning level at %d, previous warning cancelled\n",
                   total_mem_used );
         mem_alert_outstanding = 0;
      } 
   } /* if mem_alert_outstanding */

   if ( (pSCHEDULER_CONFIG_FLAGS.debug_level.memory >= DEBUG_LEVEL_PROC) &&
        (!(pSCHEDULER_CONFIG_FLAGS.debug_level.memory == DEBUG_LEVEL_MEMONLY)) ) {
      myprintf( "DEBUG: Leaving MEMORY_free\n" );
   }
} /* end MEMORY_free */
