#ifndef UTILS_DEFS
#define UTILS_DEFS

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <pwd.h>
#include <sys/types.h>

#define UTILS_MAX_ERRORTEXT 100
char UTILS_errortext[UTILS_MAX_ERRORTEXT];

#ifndef GLOBAL_DATA
   extern FILE *msg_log_handle; 
#endif

void         UTILS_set_message( const char *msgstring );
void         UTILS_format_message( const char *format, int number );
void         UTILS_clear_message( void );
void         UTILS_print_last_message( void );
void         UTILS_get_message( char *msgstring, int buflen, int clearflag );
void         UTILS_space_fill_string( char * strptr, int fieldlen );
void         UTILS_datestamp( char * strptr, time_t datetouse );
long         UTILS_write_record_safe( FILE *filehandle, char * datarec,
                              long sizeofdatarec, long recordnum, char *filetype, char *caller );
long         UTILS_write_record( FILE *filehandle, char * datarec,
                         long sizeofdatarec, long recordnum, char *filetype );
long         UTILS_read_record( FILE *filehandle, char * datarec,
                        long sizeofdatarec, char *filetype, char *caller, int err_if_fail );
short int    UTILS_compress_file( char *diskfilename, int recsize, int delflagoffset, char delflagind, char *filecomment );
void         UTILS_number_to_string( int numval, char *dest, int fieldlen );
int          UTILS_parse_string( char *inputstr, char delim,
                        char * outstr, int maxoutstrlen,
                        int * leadingpadsfound, int * overrun );
int          UTILS_count_delims( char *sptr, char delimchar );
unsigned int UTILS_find_newline( char *sptr );
void         UTILS_zero_fill( char *databuf, int datalen );
time_t       UTILS_make_timestamp( char * timestr /* yyyymmdd hh:mm:ss */ );
time_t       UTILS_timenow_timestamp( void );
void         UTILS_remove_trailing_spaces( char *sptr );
uid_t        UTILS_obtain_uid_from_name( char * username );
void         UTILS_get_dayname( int daynum, char * dayname );
int          UTILS_get_daynumber( char * datastr );
void         UTILS_strncpy( char * dest, char * src, int len );
void         UTILS_uppercase_string( char * datastr );
int          UTILS_legal_time( char * timestr );
void         UTILS_encrypt_local( char * datastr, int datalen );
void         myprintf( const char *fmt, ... );
int          UTILS_wildcard_parse_string( char *buf, char *searchmask );
#endif
