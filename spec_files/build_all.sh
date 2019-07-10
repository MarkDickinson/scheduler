#
# build_all.sh
#
# Purpose, create all the rpm files for this application.
#

# Create a binary x86_64 release based on my current git source files.
# Source files are compiled to binaries and packaged as part of the rpmbuild
# process so the binaries are only suitable for my compile environment
# (currently x86_64 fc30).
rpmbuild --target x86_64 -bb SPECS/marks-job-scheduler.spec

# Create a build from source release
# It creates all the directories and configurations, but will perform
# the compile to build the binaries as part of the rpm install, so as
# the compile is done on the users machine the binaries should work
# on the users machine reguardless of architecture.
rpmbuild --target noarch -bb SPECS/marks-job-scheduler-buildfromsrc.spec

# This rpm will install the perl-cgi interface into the default
# /var/www/cgi-bin location. It required httpd, mod_perl and perl.
rpmbuild --target noarch -bb SPECS/marks-job-scheduler-httpd.spec

# This rpm will install the J2EE/JSP interface into the jetty 
# webapps folder, it reguires jetty.
rpmbuild --target noarch -bb SPECS/marks-job-scheduler-jetty.spec

# End of build_all.sh
