#!/usr/bin/perl
require "./cgi-lib.pl";
require "./scheduler-lib.pl";

print &PrintHeader;
# Switch back after 5mins inactivity
&scheduler_common_header( "html_job_status.pl", 300000 );

local $filename = &scheduler_helper_output_file;

print "<H2>Commands awaiting review by the scheduler administrators</H2>\n";
print "<p>These are the requests that have been submitted since the last set of\n";
print "commands were processed by your scheduler administrators. The frequency\n";
print "with which the requests are processed depends upon your site policy.</p>\n";
print "<b>Suggest you use the refresh button, old displays stay in cache for an annoyingly long time.</b>\n";
print "<hr>";

# loop through the file printing the contents
# 1. open for input
if (open (myfilehandle, '<'.$filename ) != 1) {
   # unable to open file
   print "Sorry, unable to open change request file ${filename}\n";
   print "The most likely cause is that there have been no changes requested since\n";
   print "the last file was processed by the scheduler administrators.\n";
}
else {
   print "<pre>";
   # 2. read until EOF, writing whats read
   while (!eof(myfilehandle)) {
      if (read(myfilehandle,$dataline,100) > 0) {
         print $dataline;
      }
   }
   print "</pre>";
   # 3. close the file
   close( myfilehandle );
}

print "<hr>";
print &HtmlBot;
exit;
__END__
