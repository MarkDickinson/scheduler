/*
 * filesize : C
 * 
 * Return the size of a file in bytes.
 * 
 * I need this for some of my batch jobs as trying to parse
 * the output of a unix 'stat' command doesn't work for me.
 */
#include <stdio.h>
#include <stdlib.h>            /* exit defined here for gcc 4.1.1 */
#include <sys/stat.h>

int main( int argc, char **argv, char **envp ) {
   struct stat stbuf;

   /*
    * Check to see we have a filename
    */
   if (argc < 1) {
      printf( "Please provide a filename\n" );
      exit( 1 );
   }

   /* The manual indicates it returns -1 on an error. It
      doesn't. Assume an error if <> 0.
   */
   if (stat( argv[1], &stbuf ) != 0) {
      printf( "0\n" );  /* file does not exist, return 0 */
	  printf( "DEBUG: File %s does not exist\n", argv[1] );
   }
   else {
      printf( "%d\n", stbuf.st_size );
   }

   exit( 0 );
} /* end main */
