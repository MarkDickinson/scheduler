#ifndef DEBUG_LEVELS
#define DEBUG_LEVELS
/* -------------------------------------------------------------------------------
 * These values are used by all the modules to control the display of debug
 * information.
 * If a debug level is set to non-zero for an application these numbers control
 * what is displayed.
 * A higher debug level will include all below it, ie:
 *   debug level 2 shows all IO plus all proc calls
 *   debug level 6 displays mem, vars, io and proc calls
 *   debug level 1 displays only proc calls
 * The debug levels default to zero, but can be changed dynamically at a per
 * library unit level via properly formatted API messages (see jobsched_cmd
 * for examples as it can issue these requests).
  ---------------------------------------------------------------------------------
*/
#define DEBUG_LEVEL_PROC    1    /* report all proc calls */
#define DEBUG_LEVEL_IO      2    /* provide IO information */
#define DEBUG_LEVEL_VARS    4    /* display some variables being built */
#define DEBUG_LEVEL_MEMONLY 5    /* used only by the memorylib unit, log only memory malloc/free msgs */
#define DEBUG_LEVEL_MEM     6    /* display mallocs and frees */
#define DEBUG_LEVEL_VARDUMP 7    /* dump record header information */
#define DEBUG_LEVEL_RECIOCMP 8   /* display record compare info */
#define DEBUG_LEVEL_TIMERS   9   /* timer calls, about 1 each 1.5 secs so have a BIG log file */

#endif
