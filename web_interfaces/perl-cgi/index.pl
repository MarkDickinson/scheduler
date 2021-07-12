#!/usr/bin/perl
require "./cgi-lib.pl";
require "./scheduler-lib.pl";

# =========================================================================
# This is a dummy index page for the application. For those that have
# not bookmarked one of the pages that actually do anything.
# It does have the full header/menu-items plus will redirect to the
# main status page in 10 seconds if the user doesn't do anything.
# =========================================================================

# Show page banner
# Use a ten second redirect of this page.
print &PrintHeader;
&scheduler_common_header( "html_job_status.pl", 10000 );

# show the not yet information

print "<h1>Marks Job Scheduler default index page</h1>\n";
print "<p>This is the free Job Scheduler application maintained by Mark Dickinson.</p>\n";
print "<p>You will be redirected to the <a href=\"html_job_status.pl\">main status display</a> in 10 seconds.\n";
print "To avoid this page alltogether you should bookmark the main status page rather than this page.</p>\n";

print &HtmlBot;

exit;
__END__
