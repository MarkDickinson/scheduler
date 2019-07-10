# marks-job-scheduler-httpd.spec
#
# This spec file creates a package to instal the perl cgi scripts needed
# for the httpd based operator interface.
#
# It also attempts to add the apach user to the job scheduler database
# if the job scheduler is running at the time the rpm is installed.
#
# It also provides and loads the selinux rules needed for this interface
# to work correctly.
#

%define _topdir        /home/mark/ownCloud/git/falcon/scheduler/packaging 
%define name            marks-job-scheduler-http
%define release        1%{?dist}
%define version     1.18
%define buildroot %{_topdir}/%{name}?%{version}?root

BuildRoot:    %{buildroot}
Summary:         Marks Job Scheduler HTTP web interface
License:         GPL V2
Name:             %{name}
Version:         %{version}
Release:         %{release}
Prefix:         /opt/dickinson
Group:          Dickinson
Packager:       Mark Dickinson
URL:            https://mdickinson.dyndns.org
Requires:       marks-job-scheduler
Requires:       httpd
Requires:       mod_perl
Requires:       perl

%description
Marks Job Scheduler.
Perl cgi-bin scripts to provide an interface to monitor multiple instances of marks job scheduler.

%prep
mkdir -p $RPM_BUILD_ROOT/etc/httpd/conf.d
mkdir -p $RPM_BUILD_ROOT/var/www/cgi-bin/scheduler
cp -r /home/mark/ownCloud/git/falcon/scheduler/web_interfaces/perl-cgi/* $RPM_BUILD_ROOT/var/www/cgi-bin/scheduler
echo "Alias /scheduler-icons/ /var/www/cgi-bin/scheduler/icons/" > $RPM_BUILD_ROOT/etc/httpd/conf.d/marks_job_scheduler.conf
exit

%files
%attr(0775, apache, apache) /var/www/cgi-bin/scheduler/*
%attr(0644, apache, apache) /etc/httpd/conf.d/marks_job_scheduler.conf

%post
isup=`ps -ef | grep jobsched_daemon | grep -v grep`
if [ "${isup}." != "." ];
then
   # Add the job scheduler apache userid needed by the cgi-bin interface
   echo "autologin" > /var/tmp/jobsched_adduser.txt
   echo 'user add apache,password apache,autoauth yes,auth operator,desc "Web Operator id"' >> /var/tmp/jobsched_adduser.txt
   cat /var/tmp/jobsched_adduser.txt | /opt/dickinson/scheduler/bin/jobsched_cmd
   /bin/rm /var/tmp/jobsched_adduser.txt
   echo "Added apache userid to job scheduler for web monitoring use"
   # Advise default usage and customisation hints
   echo "Web interface is available at http://localhost/cgi-bin/scheduler/html_job_status.pl"
   echo "Edit /var/www/cgi-bin/scheduler/scheduler-lib.pl to add multiple servers to monitor"
else
   echo "************************* Warning *************************"
   echo "Job Scheduler is not running, manually run the commands below as root after starting the job scheduler"
   echo "/opt/dickinson/scheduler/bin/jobsched_cmd"
   echo "autologin"
   echo 'user add apache,password apache,autoauth yes,auth operator,desc "Web Operator id"'
   echo "exit"
   echo "***********************************************************"
   # Advise default usage and customisation hints
   echo "After adding the apache user to the job scheduler the"
   echo "Web interface is available at http://localhost/cgi-bin/scheduler/html_job_status.pl"
   echo "Edit /var/www/cgi-bin/scheduler/scheduler-lib.pl to add multiple servers to monitor"
fi
echo ""
# Selinux rules are required
echo "Applying selinux policy"
semodule -i /var/www/cgi-bin/scheduler/selinux/marks-job-scheduler-httpd.pp
# Advise what is needed
echo ""
cat << EOF
If you have issues with accessing the interface remember you need to also have performed
the following steps to make your httpd service available.
systemctl enable httpd
systemctl start httpd
firewall-cmd --add-service=httpd
firewall-cmd --add-service=httpd-permanent
EOF

%postun
# Remove the custom selinux rules
semodule -r marks-job-scheduler-httpd
/bin/rm -rf /var/www/cgi-bin/scheduler

%clean
/bin/rm -rf $RPM_BUILD_ROOT/etc
/bin/rm -rf $RPM_BUILD_ROOT/var

%changelog
* Mon Jul 8 2019 Mark Dickinson
  - Packaged perl cgi interface into a F30 RPM release

