#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

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
