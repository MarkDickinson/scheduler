#!/usr/bin/perl
require "cgi-lib.pl";
require "scheduler-lib.pl";

# =========================================================================
# Mainline starts...
# =========================================================================

# Show page banner
# Use a five second auto-refresh of this page.
print &PrintHeader;
&scheduler_common_header( "html_job_status.pl", 10000 );

# show the not yet information

print "<h1>Not Implemented in FREE Release</h1>\n";
print "<p>The requested function or help page has not been implemented for the FREE release\n";
print "of this application.</p>\n";
print "<p>System administrators may use the jobsched_cmd program to perform the full range\n";
print "of server functions and requests<br>(refer to the MANUAL_jobsched_cmd file).\n";
print "<br><br><b>This page will redirect to the job status page in 10 seconds</b>\n";

print &HtmlBot;

exit;
__END__
