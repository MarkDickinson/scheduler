#!/usr/bin/perl
require "./cgi-lib.pl";
require "./scheduler-lib.pl";

# A required header
print &PrintHeader;

# Switch back after 3mins inactivity
&scheduler_common_header( "html_job_status.pl", 180000 );

# Write out the help web page file

   local $filename = "html_help_pages.html";

   # loop through the file printing the contents
   # 1. open for input
   if (open (myfilehandle, '<'.$filename ) != 1) {
      # fatal error, unable to open file
      print "Sorry, unable to open help file ${filename}\n";
      print "Contact you system administrator\n";
      die( $filename );
   }
   # 2. read until EOF, writing whats read
   while (!eof(myfilehandle)) {
      if (read(myfilehandle,$dataline,100) > 0) {
         print $dataline;
      }
   }

   # 3. close the file
   close( myfilehandle );

   print &HtmlBot;
   exit;
