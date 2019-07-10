# marks-job-scheduler.spec
#
# This specification creates a x86_64 distribution. The binaries to be
# shipped are compiled from source in the build step at the time the RPM
# is created so this RPM can only be used on the same architecture (and OS)
# the RPM is built on.
#
# This must be a x86_64 RPM for {%dist} as the binaries created when this
# RPM is built are those that are installed.
#
# This 'provides' marks-job-scheduler, as does the buildfromsrc install,
# this allows packages dependant upon that to be installed requardless of
# whether this or the buildfromsrc package create the required environment.
#

%define _topdir         /home/mark/ownCloud/git/falcon/scheduler/packaging
%define name            marks-job-scheduler
%define release        1%{?dist}
%define version     1.18
%define buildroot %{_topdir}/%{name}?%{version}?root

BuildRoot:    %{buildroot}
Summary:         Marks Job Scheduler
License:         GPL V2
Name:             %{name}
Version:         %{version}
Release:         %{release}
Source:         %{name}?%{version}.tar.gz
Prefix:         /opt/dickinson
Group:          Dickinson
Packager:       Mark Dickinson
URL:            https://mdickinson.dyndns.org
Provides:       marks-job-scheduler

%description
Marks Job Scheduler. A full featured command line driven job scheduler.
Repeating jobs, calendars, job and file dependencies, user role seperation etc.

%prep
mkdir -p $RPM_BUILD_ROOT/usr/lib/systemd/system
mkdir -p $RPM_BUILD_ROOT/usr/local/share/man/man1
mkdir -p $RPM_BUILD_ROOT/opt/dickinson/scheduler
mkdir -p $RPM_BUILD_ROOT/opt/dickinson/scheduler/jobs
mkdir -p $RPM_BUILD_ROOT/opt/dickinson/scheduler/joblogs
mkdir -p $RPM_BUILD_ROOT/opt/dickinson/scheduler/logs
mkdir -p $RPM_BUILD_ROOT/opt/dickinson/scheduler/etc
mkdir -p $RPM_BUILD_ROOT/opt/dickinson/scheduler/bin
mkdir -p $RPM_BUILD_ROOT/opt/dickinson/scheduler/alert_tools
mkdir -p $RPM_BUILD_ROOT/opt/dickinson/scheduler/src
mkdir -p $RPM_BUILD_ROOT/opt/dickinson/scheduler/manuals
cp -r /home/mark/ownCloud/git/falcon/scheduler/src/* $RPM_BUILD_ROOT/opt/dickinson/scheduler/src
cp /home/mark/ownCloud/git/falcon/scheduler/samples/alert_samples/* $RPM_BUILD_ROOT/opt/dickinson/scheduler/alert_tools
cp /home/mark/ownCloud/git/falcon/scheduler/samples/alert_samples/alerts_custom.txt $RPM_BUILD_ROOT/opt/dickinson/scheduler
cp /home/mark/ownCloud/git/falcon/scheduler/samples/initd/marks_job_scheduler.service $RPM_BUILD_ROOT/usr/lib/systemd/system/marks_job_scheduler.service
cp /home/mark/ownCloud/git/falcon/scheduler/doc/man/man1/* $RPM_BUILD_ROOT/usr/local/share/man/man1
cp /home/mark/ownCloud/git/falcon/scheduler/doc/PDF/* $RPM_BUILD_ROOT/opt/dickinson/scheduler/manuals
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
exit

%build
cd $RPM_BUILD_ROOT/opt/dickinson/scheduler/src
./compile_all_linux $RPM_BUILD_ROOT/opt/dickinson/scheduler
cd ..
/bin/rm -rf $RPM_BUILD_ROOT/opt/dickinson/scheduler/src

%files
%attr(0755, root, root) /opt/dickinson/scheduler/bin/*
%attr(0755, root, root) /opt/dickinson/scheduler/etc/*
%attr(0755, root, root) /opt/dickinson/scheduler/logs/*
%attr(0755, root, root) /opt/dickinson/scheduler/joblogs/*
%attr(0755, root, root) /opt/dickinson/scheduler/jobs/*
%attr(0644, root, root) /opt/dickinson/scheduler/alerts_custom.txt
%attr(0755, root, root) /opt/dickinson/scheduler/alert_tools/*
%attr(0644, root, root) /opt/dickinson/scheduler/manuals/*
%attr(0755, root, root) /usr/lib/systemd/system/marks_job_scheduler.service
%attr(0444,root,root) /usr/local/share/man/man1/jobsched_cmd.1 
%attr(0444,root,root) /usr/local/share/man/man1/jobsched_daemon.1
%attr(0444,root,root) /usr/local/share/man/man1/jobsched_jobutil.1 
%attr(0444,root,root) /usr/local/share/man/man1/jobsched_security_checks.1
%attr(0444,root,root) /usr/local/share/man/man1/jobsched_take_snapshot.1

%post
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
# We have deleted the services file
systemctl daemon-reload

%clean
/bin/rm -rf $RPM_BUILD_ROOT/etc
/bin/rm -rf $RPM_BUILD_ROOT/var
/bin/rm -rf $RPM_BUILD_ROOT/opt

%changelog
* Mon Jul 8 2019 Mark Dickinson
  - Packaged into a binary F30 RPM release

