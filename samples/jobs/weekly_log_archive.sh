#!/bin/bash
# ----------------------------------------------------------
# Synopsis: Run weekly to backup the scheduler log files.
#
# Run by:   jobsched_daemon
# Userid:   mark
# 
# Details:
#   Backs up the log files of job output on a weekly basis.
#   Will delete any log files over three weeks old.
# Notes:
#   Will delete all log files in the logs directory if
#   possible, including our own. Must run as a NULL
#   jobname to ensure no other jobs are writing to
#   the logs directory.
# ----------------------------------------------------------
a=`date +%y%m%d`

# -1- we start in the directory the scheduler was in,
#     so we can load out environment variables.
. etc/shell_vars

# -2- do our work from the job log directory
cd ${SCHED_JOBLOG_DIR}

# -3- we need an archive directory to archive into
if [ ! -d log_archives ];
then
   mkdir log_archives
fi
if [ ! -d log_archives ];
then
   echo "Unable to create directory log_archives !, exit code 1"
   echo "** LOG ARCHIVE JOB FAILED **"
   exit 1
fi

# -4- Backup files in the joblogs directory into an archive file
echo "---- Backup up log files, date ${a} ----"
c="logs_week_ending_${a}.tar"
b="log_archives/${c}"
# Note: Get rid of any existing files with todays datestamp
# or tar + gzip will prompt, to which the scheduler will
# reply with EOF. As that won't delete the archive and return
# an error result we clean it up ourselves first.
if [ -f ${b} ];
then
   rm -f ${b}
fi
if [ -f ${b}.gz ];
then
   rm -f ${b}.gz
fi
tar -cvf ${b} *.log        # tar up the log files to the archive
result=$?
if [ "${result}." != "0." ];
then
   echo "*ERROR* TAR FAILED WITH RC=${result}"
   exit ${result}
fi
rm -f *.log                # remove the files, they are tarred up now

# -5- ALL REMAINING ACTIONS IN THE ARCHIVE DIR
cd log_archives
# -5a- gzip what we have archived this time
gzip ${c}
# -5b- Now we need to cleanup some old log files
find ./ -type f -name "log_week*" -mtime +15 -print | xargs --no-run-if-empty ls -la

# And all done
exit 0
