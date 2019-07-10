# marks-job-scheduler-buildfromsrc.spec
#
# This specification creates a RPM file that includes the source, and at the
# time the user installs the RPM the source will be compiled on the users
# system.
#
# This should be a noarch RPM as the intent of having the compile occur on
# the users machine is to allow installation on architecture where the x86_64
# binaries will not run.
#
# This 'provides' marks-job-scheduler, as does the binary install, this allows
# packages dependant upon that to be installed requardless of whether this or
# the binary x86_64 is installed by the user.
#
# As it needs to compile the source this RPM has a dependecy on gcc
#

%define _topdir         /home/mark/ownCloud/git/falcon/scheduler/packaging
%define name            marks-job-scheduler-buildfromsrc
%define release        1%{?dist}
%define version     1.18
%define buildroot %{_topdir}/%{name}?%{version}?root

BuildRoot:    %{buildroot}
Summary:         Marks Job Scheduler
License:         GPL V2
Name:             %{name}
Version:         %{version}
Release:         %{release}
Prefix:         /opt/dickinson
Group:          Dickinson
Packager:       Mark Dickinson
URL:            https://mdickinson.dyndns.org
Requires:       gcc
Provides:       marks-job-scheduler

%description
Marks Job Scheduler install from source.
A full featured command line driven job scheduler.
Repeating jobs, calendars, job and file dependencies, user role seperation etc.

%prep
mkdir -p $RPM_BUILD_ROOT/usr/lib/systemd/system
mkdir -p $RPM_BUILD_ROOT/usr/local/share/man/man1
mkdir -p $RPM_BUILD_ROOT/opt/dickinson/scheduler
mkdir -p $RPM_BUILD_ROOT/opt/dickinson/scheduler/jobs
mkdir -p $RPM_BUILD_ROOT/opt/dickinson/scheduler/joblogs
mkdir -p $RPM_BUILD_ROOT/opt/dickinson/scheduler/logs
mkdir -p $RPM_BUILD_ROOT/opt/dickinson/scheduler/etc
mkdir -p $RPM_BUILD_ROOT/opt/dickinson/scheduler/alert_tools
mkdir -p $RPM_BUILD_ROOT/opt/dickinson/scheduler/src
mkdir -p $RPM_BUILD_ROOT/opt/dickinson/scheduler/bin
mkdir -p $RPM_BUILD_ROOT/opt/dickinson/scheduler/manuals
cp /home/mark/ownCloud/git/falcon/scheduler/samples/alert_samples/alerts_custom.txt $RPM_BUILD_ROOT/opt/dickinson/scheduler
cat << EOF > $RPM_BUILD_ROOT/opt/dickinson/scheduler/etc/shell_vars
# Used by scripts and systemd services
SCHED_BASE_DIR="/opt/dickinson/scheduler"
SCHED_BIN_DIR="/opt/dickinson/scheduler/bin"
SCHED_JOBLOG_DIR="/opt/dickinson/scheduler/joblogs"
SCHED_PORT="9002"
# Default is to listen on all interfaces rather than bind to one ip-address
SCHED_IPADDR=
EOF
cat << EOF > $RPM_BUILD_ROOT/opt/dickinson/scheduler/etc/stop_cmds
autologin
sched shutdown
EOF
cat << EOF > $RPM_BUILD_ROOT/opt/dickinson/scheduler/logs/README
This directory must exist. The Job scheduler writes activity logs here.
EOF
cat << EOF > $RPM_BUILD_ROOT/opt/dickinson/scheduler/joblogs/README
This directory must exist. All jobs that run write their output here.
EOF
cat << EOF > $RPM_BUILD_ROOT/opt/dickinson/scheduler/jobs/README
This directory should exist. This is the prefered location for scripts
that are to be run by the job scheduler.
EOF
cat << EOF > $RPM_BUILD_ROOT/opt/dickinson/scheduler/bin/README
This directory must exist for the install to work.
EOF
cp /home/mark/ownCloud/git/falcon/scheduler/samples/alert_samples/* $RPM_BUILD_ROOT/opt/dickinson/scheduler/alert_tools
cp /home/mark/ownCloud/git/falcon/scheduler/samples/initd/marks_job_scheduler.service $RPM_BUILD_ROOT/usr/lib/systemd/system/marks_job_scheduler.service
cp /home/mark/ownCloud/git/falcon/scheduler/doc/man/man1/* $RPM_BUILD_ROOT/usr/local/share/man/man1
cp /home/mark/ownCloud/git/falcon/scheduler/src/* $RPM_BUILD_ROOT/opt/dickinson/scheduler/src
cp /home/mark/ownCloud/git/falcon/scheduler/doc/PDF/* $RPM_BUILD_ROOT/opt/dickinson/scheduler/manuals
exit

%files
%attr(0755, root, root) /opt/dickinson/scheduler/src/*
%attr(0755, root, root) /opt/dickinson/scheduler/etc/*
%attr(0755, root, root) /opt/dickinson/scheduler/logs/*
%attr(0755, root, root) /opt/dickinson/scheduler/joblogs/*
%attr(0755, root, root) /opt/dickinson/scheduler/jobs/*
%attr(0755, root, root) /opt/dickinson/scheduler/bin/*
%attr(0644, root, root) /opt/dickinson/scheduler/alerts_custom.txt
%attr(0755, root, root) /opt/dickinson/scheduler/alert_tools/*
%attr(0644, root, root) /opt/dickinson/scheduler/manuals/*
%attr(0644, root, root) /usr/lib/systemd/system/marks_job_scheduler.service
%attr(0444,root,root) /usr/local/share/man/man1/jobsched_cmd.1 
%attr(0444,root,root) /usr/local/share/man/man1/jobsched_daemon.1
%attr(0444,root,root) /usr/local/share/man/man1/jobsched_jobutil.1 
%attr(0444,root,root) /usr/local/share/man/man1/jobsched_security_checks.1
%attr(0444,root,root) /usr/local/share/man/man1/jobsched_take_snapshot.1

%post
cd /opt/dickinson/scheduler/src
./compile_all_linux /opt/dickinson/scheduler
# Start the job scheduler
systemctl daemon-reload
systemctl enable marks_job_scheduler
systemctl start marks_job_scheduler

%postun
# Job scheduler may have been running at the time the rpm was
# removed, search and kill it if running.
isupoid=`ps -ef | grep -i jobsched_daemon | grep -v grep | awk {'print $2'}`
if [ "${isupoid}." != "." ];
then
   kill -9 ${isupoid}
fi
# Clean up all the logs and joblogs that will have been created
# by the job scheduler running.
/bin/rm -rf /opt/dickinson/scheduler
# If the job scheduler was the only one of my packages installed
# then clean up the top level directory as well.
datacount=`ls /opt/dickinson | wc -l`
if [ ${datacount} == 0 ];
then
   rmdir /opt/dickinson
fi
# we have deleted the services file
systemctl daemon-reload

%clean
/bin/rm -rf $RPM_BUILD_ROOT/etc
/bin/rm -rf $RPM_BUILD_ROOT/opt

%changelog
* Mon Jul 8 2019 Mark Dickinson
  - Packaged into a source distribution that compiles from source on install

