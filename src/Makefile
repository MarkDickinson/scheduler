# --------------------------------------------------------------------------------------------------
# A minumal first attempt at a Makefile.
# It does not yet replicate all the functions of the install script, although can replace
# the compile_all_linux script.
# The 'uninstall' will remove the entire INSTALLDIR directory including any custom jobs
# you have created so use with caution.
# --------------------------------------------------------------------------------------------------
BINDIR="executables"
INSTALLDIR="/opt/dickinson/scheduler"

all: bindir jobsched_daemon jobsched_cmd jobsched_take_snapshot jobsched_security_checks jobsched_jobutil explain_errno
	echo "Files built into ${BINDIR}"

bindir:
	if [ ! -d ${BINDIR} ]; then mkdir ${BINDIR}; fi

install: bindir jobsched_daemon jobsched_cmd jobsched_take_snapshot jobsched_security_checks jobsched_jobutil explain_errno
	if [ ! -d ${INSTALLDIR} ]; then mkdir -p ${INSTALLDIR}; fi
	if [ ! -d ${INSTALLDIR}/bin ]; then mkdir ${INSTALLDIR}/bin; fi
	if [ ! -d ${INSTALLDIR}/jobs ]; then mkdir ${INSTALLDIR}/jobs; fi
	if [ ! -d ${INSTALLDIR}/logs ]; then mkdir ${INSTALLDIR}/logs; fi
	if [ ! -d ${INSTALLDIR}/joblogs ]; then mkdir ${INSTALLDIR}/joblogs; fi
	if [ ! -d ${INSTALLDIR}/etc ]; then mkdir ${INSTALLDIR}/etc; fi
	echo "# Used by scripts and systemd files" > ${INSTALLDIR}/etc/shell_vars
	echo "SCHED_BASE_DIR=${INSTALLDIR}" >> ${INSTALLDIR}/etc/shell_vars
	echo "SCHED_BIN_DIR=${INSTALLDIR}/bin" >> ${INSTALLDIR}/etc/shell_vars
	echo "SCHED_JOBLOG_DIR=${INSTALLDIR}/joblogs" >> ${INSTALLDIR}/etc/shell_vars
	echo "SCHED_PORT=9002" >> ${INSTALLDIR}/etc/shell_vars
	echo "# Default is listen on all interfaces rather than one ip-address" >> ${INSTALLDIR}/etc/shell_vars
	echo "SCHED_IPADDR=" >> ${INSTALLDIR}/etc/shell_vars
	echo "autologin" > ${INSTALLDIR}/etc/stop_cmds
	echo "sched shutdown" >> ${INSTALLDIR}/etc/stop_cmds
	cp -p ${BINDIR}/* ${INSTALLDIR}/bin
	echo "**********************************************************"
	echo "Install either the init.d script or the service file from"
	echo "the ../samples directory to start the scheduler."
	echo "**********************************************************"

uninstall:
	if [ -d ${INSTALLDIR} -a ${INSTALLDIR} != "." -a "${INSTALLDIR}." != "/." ]; then /bin/rm -rf ${INSTALLDIR}; fi

# ------------------------------------------------------
# The two core modules, the server and command interface
# ------------------------------------------------------
jobsched_daemon: alerts.o calendar.o config.o jobslib.o schedlib.o utils.o api.o server.o bulletproof.o users.o memorylib.o 
	gcc -ggdb3 alerts.o calendar.o config.o jobslib.o schedlib.o utils.o api.o server.o bulletproof.o users.o memorylib.o -o ${BINDIR}/jobsched_daemon

jobsched_cmd: calendar_utils.o bulletproof.o utils.o api.o memorylib.o jobsched_cmd.o 
	gcc -ggdb3 calendar_utils.o bulletproof.o utils.o api.o memorylib.o jobsched_cmd.o -o ${BINDIR}/jobsched_cmd

# ------------------------------------------------------
# Utility programs
# ------------------------------------------------------
jobsched_take_snapshot: utils.o memorylib.o jobsched_take_snapshot.o server_global_data.h
	gcc -ggdb3 utils.o memorylib.o jobsched_take_snapshot.o -o ${BINDIR}/jobsched_take_snapshot

explain_errno: explain_errno.c
	gcc -ggdb3 -o ${BINDIR}/explain_errno explain_errno.c

jobsched_security_checks: jobsched_security_checks.c server_global_data.h jobslib.h users.h userrecdef.h
	gcc -ggdb3 -o ${BINDIR}/jobsched_security_checks jobsched_security_checks.c 

jobsched_jobutil: jobsched_jobutil.c server_global_data.h
	gcc -ggdb3 -o ${BINDIR}/jobsched_jobutil jobsched_jobutil.c 

# ------------------------------------------------------
# All the modules required for the programs
# ------------------------------------------------------
jobsched_cmd.o: jobsched_cmd.c server_global_data.h api.h
	gcc -ggdb3 -c -D_GNU_SOURCE -Wall -Wreturn-type jobsched_cmd.c

alerts.o: alerts.c alerts.h
	gcc -ggdb3 -c -D_GNU_SOURCE -Wall -Wreturn-type -Wno-unused -Wstringop-truncation alerts.c

calendar.o: calendar.c calendar.h
	gcc -ggdb3 -c -D_GNU_SOURCE -Wall -Wreturn-type -Wstringop-truncation calendar.c

config.o: config.c config.h
	gcc -ggdb3 -c -D_GNU_SOURCE -Wall -Wreturn-type -Wstringop-truncation -Wno-unused config.c

jobslib.o: jobslib.c jobslib.h
	gcc -ggdb3 -c -D_GNU_SOURCE -Wall -Wreturn-type -Wstringop-truncation -Wno-format-truncation jobslib.c

schedlib.o: schedlib.c schedlib.h server_global_data.h
	gcc -ggdb3 -c -D_GNU_SOURCE -Wall -Wreturn-type -Wno-unused -Wstringop-truncation -Wno-format-truncation -Wno-restrict schedlib.c

utils.o: utils.c utils.h
	gcc -ggdb3 -c -D_GNU_SOURCE -Wall -Wreturn-type -Wstringop-truncation utils.c

api.o: api.c api.h
	gcc -ggdb3 -c -D_GNU_SOURCE -Wall -Wreturn-type -Wno-unused -Wstringop-truncation api.c

server.o: server.c
	gcc -ggdb3 -c -D_GNU_SOURCE -Wall -Wreturn-type -Wno-unused -Wstringop-truncation -Wno-format-truncation -Wno-restrict server.c

bulletproof.o: bulletproof.c
	gcc -ggdb3 -c -D_GNU_SOURCE -Wall -Wreturn-type -Wstringop-truncation bulletproof.c

users.o: users.c users.h userrecdef.h
	gcc -ggdb3 -c -D_GNU_SOURCE -Wall -Wreturn-type -Wstringop-truncation users.c

calendar_utils.o: calendar_utils.c calendar_utils.h
	gcc -ggdb3 -c -D_GNU_SOURCE -Wall -Wreturn-type -Wstringop-truncation calendar_utils.c

memorylib.o: memorylib.c memorylib.h
	gcc -ggdb3 -c -D_GNU_SOURCE -Wall -Wreturn-type -Wstringop-truncation memorylib.c

jobsched_take_snapshot.o: jobsched_take_snapshot.c server_global_data.h jobslib.h
	gcc -ggdb3 -c -D_GNU_SOURCE -Wall -Wreturn-type -Wno-format-truncation jobsched_take_snapshot.c

# ------------------------------------------------------
# Clean up temporary files
# ------------------------------------------------------
clean:
	/bin/rm *.o
	/bin/rm -rf ${BINDIR}

