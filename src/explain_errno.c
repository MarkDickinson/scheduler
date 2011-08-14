#include <stdio.h>
#include <errno.h>

/* GCC version 4 or above requires the prototype for exit (3 did not) */
#include "system_type.h"
#ifndef GCC_MAJOR_VERSION3
#include <stdlib.h>
#endif

int main (int argc, char **argv, char **envp) {
		int local_errno;
		if (argc >= 2) {
				local_errno = atoi(argv[1]);
				errno = local_errno;
				perror( "Description" );
		}
		else {
				printf( "Syntax: errno <errno>\n" );
		}
		exit(0);
}
