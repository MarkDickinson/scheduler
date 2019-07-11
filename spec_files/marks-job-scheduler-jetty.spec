# marks-job-scheduler-jetty.spec
#
# This spec file creates a RPM to install the J2EE/JSP files into
# the Jetty webapps directory.
# While the app has also been tested under Tomcat I only use the lighter
# weight Jetty app now.
#
# It also loads selinux rules specific to jetty to allow the interface
# to actually work on a system in enforcing mode.
#

%define _topdir         /home/mark/ownCloud/git/packaging
%define name            marks-job-scheduler-jetty
%define release        1%{?dist}
%define version     1.18
%define buildroot %{_topdir}/%{name}?%{version}?root

BuildRoot:    %{buildroot}
Summary:         Marks Job Scheduler J2EE Jetty interface
License:         GPLv2
Name:             %{name}
Version:         %{version}
Release:         %{release}
Source:         %{name}?%{version}.tar.gz
Prefix:         /opt/dickinson
Group:          Dickinson
Packager:       Mark Dickinson
URL:            https://mdickinson.dyndns.org/linux/doc/Job_Scheduler_about.php
Requires:       marks-job-scheduler
Requires:       httpd
Requires:       jetty

%description
Marks Job Scheduler J2EE/JSP interface.
A J2EE interface to the scheduler, packaged specifically to run under Jetty.

%prep
mkdir -p $RPM_BUILD_ROOT/var/lib/jetty/webapps/scheduler
cp -r /home/mark/ownCloud/git/falcon/scheduler/web_interfaces/tomcat/* $RPM_BUILD_ROOT/var/lib/jetty/webapps/scheduler
exit

%files
%attr(0755, root, root) /var/lib/jetty/webapps/scheduler/*

%post
echo "Loading selinux rules"
semodule -i /var/lib/jetty/webapps/scheduler/selinux/marks-job-scheduler-jetty.pp
echo "Interface available at http://localhost:8080/scheduler"

cat << EOF
If you have issues accessing the interface check you have done the following Jetty configurations
systemctl enable jetty
systemctl start jetty
firewall-cmd --add-port=8080/tcp
firewall-cmd --add-port=8080/tcp --permanent
EOF

%postun
/bin/rm -rf /var/lib/jetty/webapps/scheduler
semodule -r marks-job-scheduler-jetty

%clean
/bin/rm -rf $RPM_BUILD_ROOT/var

%changelog
* Mon Jul 8 2019 Mark Dickinson
  - Packaged into a binary F30 RPM release

