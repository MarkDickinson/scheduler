#!/bin/bash
# --------------------------------------------------------------------
#
# end_alert.sh
#
# a helper script for batch jobs that need to cancel alerts.
#
# Function: generate a message indicating an alert is over
#
# Input parameters are 
#  $1 : The job name as known by the scheduler
#  $2 : Any message text provided by the job scheduler
# 
# The supplied standalone version will just write the alert text to
# the syslog daemon (/var/log/messages).
# It is recomended that you instead obtain my centralised alert
# collection tool, which will allow you to run a script which will
# forward all alerts to a remote (or local) collection point where
# alerts from all servers may be monitored from a single interface.
# If used the effect of ending an alert would be to delete it from
# the central server (and operations monitoring screens).
#
# If you have installed it get your job_scheduler administrator to
# change the alert forwarding programs with the jobsched_cmd 
# commands
# SCHED ALERT FORWARDING EXECRAISECMD /opt/dickinson/alert_management/scripts/raise_alert.sh
# SCHED ALERT FORWARDING EXECCANCELCMD /opt/dickinson/alert_management/scripts/end_alert.sh
#
# If you don't have it then this script should be customised for the alert
# forwarding tool used in your site (Tivoli postemsg?) to ensure they are
# forwarded correctly.
# --------------------------------------------------------------------

mypath=`dirname $0`
# Always log what we are provided with a datestamp for auditing.
a=`date`
echo "${a} $*" >> ${mypath}/alerts.log
# Write to syslog, Pipe any errors to the same audit file
logger $*  2>&1 >> ${mypath}/alerts.log
exit 0
