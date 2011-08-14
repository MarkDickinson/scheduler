#!/bin/bash
# ----------------------------------------------------------
# Synopsis: Run daily to backup the scheduler configuration.
#
# Run by:   jobsched_daemon
# Userid:   mark
# 
# Change livedir and bkpfile to suit your requirements.
#
# Input: $1 the rundate for the job. This is passed by the
#        scheduler; job setup as PARM "~~DATE~~".
#
# Details:
# Take a snapshot of the current active configuration. This
# can be used by jobsched_cmd to completely rebuild the
# server configuration and job definition database if
# ever needed.
#
# The backup files will be stored under the jobsched_daemon
# program directory in the folder backups/configs
# ----------------------------------------------------------

# -1- we expect a date parameter
a=$1
if [ "${a}." == "." ];
then
   echo "No date parameter was passed !"
   exit 1
fi

# -2- load environment (we will start in the scheduler directory)
. etc/shell_vars

# -3- set script specific variables
bkpdir="${SCHED_BASE_DIR}/logs/config_backups"
bkpfile="${bkpdir}/configuration_snapshot_${a}"
program="${SCHED_BIN_DIR}/jobsched_take_snapshot"

# -4- Check the snapshot program exists
if [ ! -x ${program} ];
then
   echo "PROGRAM NOT FOUND: ${program}"
   exit 127   # the code for not found
fi

# -5- Check the backup directory exists, create if needed
# Note: we know the log part exists because the server rc3.d
#       script supplied creates it at startup.
if [ ! -d ${bkpdir} ];
then
	mkdir ${bkpdir}
fi
if [ ! -d ${bkpdir} ];
then
	echo "UNABLE TO CREATE DIRECTORY: ${bkpdir}"
	exit 1
fi

# -6- Run the program
${program} ${SCHED_BASE_DIR} > ${bkpfile}

# -7- Check for errors, if any found exit non-zero so the
# job scheduler will place the job running us into
# alert state.
result=$?
datenow=`date`
if [ ${result} != 0 ];
then
   echo "${datenow} Configuration Snapshot for ${a} ***FAILED***"
   exit 1     # scheduler will place job into alert status
fi

# Exit with 0, Job scheduler will mark us as completed
echo "${datenow} Configuration Snapshot for ${a} Ended OK"
exit 0
