#!/usr/bin/perl
require "./cgi-lib.pl";
require "./scheduler-lib.pl";

$submit_required = 0; # Nothing to write by default

# Used to write the line to the file.
sub scheduler_helper_record_line() {
   $dataline = $_[0];
   $source = $ENV{"REMOTE_ADDR"};
   $remotename = $ENV{"REMOTE_HOST"};
   if ($remotename ne "") {
      $source = $source." (".$remotename.")";
   }
   $filename = &scheduler_helper_output_file();
   open( CMDOUT, ">>${filename}" );
   print CMDOUT "#\n# Stored from ${source}\n${dataline}\n";
   close CMDOUT;
   print "<br>Last request stored by ${source} was...<br>${dataline}<br>";
}  # End of sub scheduler_helper_record_line


# =========================================================================
# Mainline starts...
# =========================================================================
print &PrintHeader;
&scheduler_common_header( "html_job_status.pl", 1200000 ); #Jump in 20 minutes
print "<center><h1>Assist Helper - Defining a Job</h1></center><br>";

# -------------------------------------------------------------------------
# Do we have any special processing to do first ?
# If we do store the data from the command the caller has already
# requested.
# -------------------------------------------------------------------------
if (&MethPost) {
   if (!(&ReadParse(*input))) {
      printf( "*ERROR* Unable to read data stream" );
      print &HtmlBot;
   }

   $button = $input{ 'submitcmd' };
   if ($button eq "Store Command") {
      $jobname = $input{ 'jobname' };
      $jobuser = $input{ 'userid' };
      $jobtime = $input{ 'starttime' };
      $jobcmd  = $input{ 'jobcmd' };
      $jobparm = $input{ 'jobparms' };
      $jobdesc = $input{ 'jobdesc' };
      $passed_put_commands = "JOB ADD ${jobname},OWNER ${jobuser},TIME ${jobtime},CMD ${jobcmd}";
      if ($jobparm ne "") {
         $passed_put_commands = $passed_put_commands.",PARM \"${jobparm}\"";
      }
      if ($jobdesc ne "") {
         $passed_put_commands = $passed_put_commands.",DESC \"${jobdesc}\"";
      }
      $test    = $input{ 'jobhasdep' };
      if ($test eq "yes") {
         $jobdep = "";
         $test    = $input{ 'jobdep1' };
	 if ($test ne "") {
            $test2   = substr($test,0,1);
            if ($test2 eq "/") { $jobdep = "FILE ${test}"; }
            else { $jobdep = "JOB ${test}"; }
         }
         $test    = $input{ 'jobdep2' };
	 if ($test ne "") {
            $test2   = substr($test,0,1);
            if ($test2 eq "/") { $jobdep = $jobdep.",FILE ${test}"; }
            else { $jobdep = $jobdep.",JOB ${test}"; }
         }
         $test    = $input{ 'jobdep3' };
	 if ($test ne "") {
            $test2   = substr($test,0,1);
            if ($test2 eq "/") { $jobdep = $jobdep.",FILE ${test}"; }
            else { $jobdep = $jobdep.",JOB ${test}"; }
         }
         $test    = $input{ 'jobdep4' };
	 if ($test ne "") {
            $test2   = substr($test,0,1);
            if ($test2 eq "/") { $jobdep = $jobdep.",FILE ${test}"; }
            else { $jobdep = $jobdep.",JOB ${test}"; }
         }
         $test    = $input{ 'jobdep5' };
	 if ($test ne "") {
            $test2   = substr($test,0,1);
            if ($test2 eq "/") { $jobdep = $jobdep.",FILE ${test}"; }
            else { $jobdep = $jobdep.",JOB ${test}"; }
         }
         if ($jobdep ne "") {
            $passed_put_commands = $passed_put_commands.",DEP \"${jobdep}\"";
         }
      }

      $test = $input{ 'repeat' };
      if ($test eq "yes") {
         $test = $input{ 'repeatmins' };
         if ($repeatmins ne "") {
            $passed_put_commands = $passed_put_commands.",REPEATEVERY \"${test}\"";
         }
      }

      $test = $input{ 'daylist' };
      if ($test eq "yes") {
         $test = $input{ 'dayname' };
         $daylist = "";
         while ($test ne "") {
            $test2 = substr(${test},0,3); 
            $test = substr(${test},4); # 4 rather than three, checkbox fields null seperated
            $daylist = $daylist.",".$test2;
         }
         if (length($daylist) > 2) {
            $daylist = substr($daylist,1,length($daylist));
            $passed_put_commands = $passed_put_commands.",DAYS ${daylist}";
         }
      }
      $test = $input{ 'catchup' };
      if ($test eq "no") {
         $passed_put_commands = $passed_put_commands.",CATCHUP NO";
      }
      $test = $input{ 'usecal' };
      if ($test eq "yes") {
         $test = $input{ 'calname' };
         if ($test ne "") {
            $passed_put_commands = $passed_put_commands.",CALENDAR ${test}";
         }
      }
      # Now record it.
      &scheduler_helper_record_line( "${passed_put_commands}" );
   }
}
else {
   # First time in the form, show the intro
   print "<p>The job assist helper makes it easier to define jobs than having to remember\n";
   print "the entire command syntax yourself. For <em>security reasons</em>, as the web server\n";
   print "should not have access to define jobs; the generated command output is written to\n";
   print "a file on the server. This file will be reviewed by administration personel\n";
   print "on a regular basis, it is they who decide whether your job will actually be created\n";
   print "or not.</p>\n";
}

# -------------------------------------------------------------------------
# Print the normal data entry page now
# -------------------------------------------------------------------------

print "<br><hr><center><h2>Assist Helper - Define a new job here</h2></center><br>";
print "<form action=\"".&scheduler_HTTPD_Home."/html_assist_jobs.pl\" method=\"post\"><br><pre>\n";
print "<H3>Required Fields</H3>\n";
print "Job Name : <input type=text size=31 name=jobname> (required, no spaces)\n";
print "Runs as unix userid : <input type=text size=15 name=userid>    (required, an existing unix userid)\n";
print "Initial time to run : <input type=text size=14 name=starttime>     (required, YYYYMMDD HH:MM)\n\n";
print "    Command to run : <input type=text size=60 name=jobcmd> (provide the full path)\n";
print "Parameters to pass : <input type=text size=60 name=jobparms> (note: Keyword ~~DATE~~ can be used here)\n";
print "Description of job : <input type=text size=60 name=jobdesc>  (a meaningfull description)\n\n";
print "\n\n<H3>Optional Additional Configuration</H3>\n";
print "Does the job have prior dependencies ? : <input type=radio checked name=jobhasdep value=\"no\">No";
print " <input type=radio name=jobhasdep value=\"yes\">Yes\n";
print "      If the job waits on dependencies enter them in the list here\n";
print "      File dependencies must include the full path\n";
print "      Any entry not beginning with / is treated as a job dependency.\n";
print "                   <input type=text size=60 name=jobdep1>\n";
print "                   <input type=text size=60 name=jobdep2>\n";
print "                   <input type=text size=60 name=jobdep3>\n";
print "                   <input type=text size=60 name=jobdep4>\n";
print "                   <input type=text size=60 name=jobdep5>\n\n";
print "Repeating job ? : <input type=radio checked name=repeat value=\"no\">No";
print " <input type=radio name=repeat value=\"yes\">Yes\n";
print "                          Every <input type=text size=5 name=repeatmins> minutes (ie:30 for half hourly)\n\n";
print "Run every day ? : <input type=radio checked name=daylist value=\"no\">Every Day";
print " <input type=radio name=daylist value=\"yes\">Selected Days Only\n";
print "                                 If Selected Days : <input type=checkbox name=dayname value=\"SUN\">Sun ";
print "<input type=checkbox name=dayname value=\"MON\">Mon ";
print "<input type=checkbox name=dayname value=\"TUE\">Tue ";
print "<input type=checkbox name=dayname value=\"WED\">Wed ";
print "<input type=checkbox name=dayname value=\"THU\">Thu ";
print "<input type=checkbox name=dayname value=\"FRI\">Fri ";
print "<input type=checkbox name=dayname value=\"SAT\">Sat\n\n";
print "Catch up missed run days ? : <input type=radio name=catchup value=\"no\">No";
print " <input type=radio name=catchup checked value=\"yes\">Yes\n\n";
print "Use a calendar ? : <input type=radio checked name=usecal value=\"no\">No";
print " <input type=radio name=usecal value=\"yes\">Yes";
print " Calendar Name <input type=text size=40 name=calname> (Calendar must already exist)\n\n";
print "</pre><center><input type=submit name=submitcmd value=\"Store Command\">&nbsp&nbsp&nbsp&nbsp<input type=reset>";
print "</center></form>";

print "<hr>\n";
print &HtmlBot;

exit;
__END__
