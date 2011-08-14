#ifndef MEMORY_LIB_EXISTS
#define MEMORY_LIB_EXISTS
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define MEMORY_MALLOC_WARN 40960      /* 10 blocks of 4K buffered may be a problem */
#define MEMORY_TAGOWNER_LEN 30
#define MEMORY_MAX_CHAIN_LEN 20
#define MEMORY_RESET_COUNTERS_AT 65000

void MEMORY_initialise_malloc_counters( void );
void MEMORY_log_memory_useage( void );
void * MEMORY_malloc( int mem_needed, char *caller );
void MEMORY_free( void * memptr );
void MEMORY_reset_counters_on_newday( void );

#endif
