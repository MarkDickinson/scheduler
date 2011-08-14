#!/usr/bin/perl
require "cgi-lib.pl";
require "scheduler-lib.pl";

$submit_required = 0; # Nothing to write by default

# Used to write the line to the file.
sub scheduler_helper_record_line() {
   my $dataline = $_[0];
   my $source = $ENV{"REMOTE_ADDR"};
   my $remotename = $ENV{"REMOTE_HOST"};
   if ($remotename ne "") {
      $source = $source." (".$remotename.")";
   }
   my $filename = &scheduler_helper_output_file();

   open( CMDOUT, ">>${filename}" );
   print CMDOUT "#\n# Stored from ${source}\n${dataline}\n";
   close CMDOUT;

   print "From ${source}, stored: ${dataline}<br>";
}  # End of sub scheduler_helper_record_line


sub print_month() {
   my $monthname = $_[0];
   my $i = 0;
   print "${monthname} ";
   for ($i = 1; $i <=31; $i++) {  # Days 1 to 31
      print " <input type=checkbox name=${monthname}days value=\"${i}\">";
   }
   print "\n";
}  # End of sub print_month

sub print_all_months() {
   print "Select the days to run : (it's up to you to enure they are legal for any given month)\n";
   print "      01 02 03  04 05  06  07 08 09  10 11  12 13 14  15 16  17 18  19 20  21 22  23 24  25 26  27 28  29 30  31\n";
   &print_month( "Jan" );
   &print_month( "Feb" );
   &print_month( "Mar" );
   &print_month( "Apr" );
   &print_month( "May" );
   &print_month( "Jun" );
   &print_month( "Jul" );
   &print_month( "Aug" );
   &print_month( "Sep" );
   &print_month( "Oct" );
   &print_month( "Nov" );
   &print_month( "Dec" );
}  # End of sub print_all_months

sub process_a_month() {
   my $monthname = $_[0];
   my $added = $_[1];
   my $calname = $_[2];
   my $caltype = $_[3];
   my $calyear = $_[4];
   my $caldesc = $_[5];
   my $datastr = $_[6];
   if (length($datastr) > 0) { 
      # convert any nulls into ,
      $datastr =~ s/\0/,/g;
      if ($added == 0) {
         $command_line = "CALENDAR ADD ${calname},TYPE ${caltype},YEAR ${calyear},DESC \"${caldesc}\"";
         $added = 1; # added, must merge from now on
      }
      else {
         $command_line = "CALENDAR MERGE ${calname},TYPE ${caltype},YEAR ${calyear}";
      }
      $command_line = $command_line.",FORMAT DAYS ${monthname} \"".${datastr}."\"";
      &scheduler_helper_record_line( "${command_line}" );
   }
   return $added;
}  # End of sub process_a_month

# =========================================================================
# Mainline starts...
# =========================================================================
print &PrintHeader;
&scheduler_common_header( "html_job_status.pl", 1200000 ); #Jump in 20 minutes
print "<center><h1>Assist Helper - Defining a Calendar Entry</h1></center><br>";

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
      $added = 0; # nothing added yet
      $calname = $input{ 'calname' };
      $caltype = $input{ 'caltype' };
      $caldesc = $input{ 'caldesc' };
      $calyear = $input{ 'calyear' };

      # Work out what the command is
      $command_line = "CALENDAR ADD ${calname},TYPE ${caltype},YEAR ${calyear},DESC \"${caldesc}\"";

      # Let testparm hold what we are checking
      $testparm = $input{ 'dateseltype' };

      # Check if specific days have been selected (tested OK)
      $test = index( $testparm, 'daylist' );
      if ($test >= 0) {
         $daylist = "";
         $test = $input{ 'daylistname' };
         while ($test ne "") {
            $test2 = substr(${test},0,3);
            $test = substr(${test},4); # 4 rather than three, checkbox fields null seperated
            $daylist = $daylist.",".$test2;
         }
         if (length($daylist) > 2) {
            $daylist = substr($daylist,1,length($daylist));
            $command_line = $command_line.",DAYNAMES \"${daylist}\"";
         }
         &scheduler_helper_record_line( "${command_line}" );
         $added = 1; # added, must merge from now on
      }

      # super hard, get all the dates now, probably need to loop for each month and
      # write a merge for all after january.
      # Actually, be lazy and one for each month, this is a sample helper I won't use
      # anyway, we'll move this function to a GUI at some point anyway.
      $test = index( $testparm, 'bydates', 0 );
      if ($test >= 0) {
         $added = &process_a_month( "JAN", $added, $calname, $caltype, $calyear, "${caldesc}", "$input{'Jandays'}" );
         $added = &process_a_month( "FEB", $added, $calname, $caltype, $calyear, "${caldesc}", "$input{'Febdays'}" );
         $added = &process_a_month( "MAR", $added, $calname, $caltype, $calyear, "${caldesc}", "$input{'Mardays'}" );
         $added = &process_a_month( "APR", $added, $calname, $caltype, $calyear, "${caldesc}", "$input{'Aprdays'}" );
         $added = &process_a_month( "MAY", $added, $calname, $caltype, $calyear, "${caldesc}", "$input{'Maydays'}" );
         $added = &process_a_month( "JUN", $added, $calname, $caltype, $calyear, "${caldesc}", "$input{'Jundays'}" );
         $added = &process_a_month( "JUL", $added, $calname, $caltype, $calyear, "${caldesc}", "$input{'Juldays'}" );
         $added = &process_a_month( "AUG", $added, $calname, $caltype, $calyear, "${caldesc}", "$input{'Augdays'}" );
         $added = &process_a_month( "SEP", $added, $calname, $caltype, $calyear, "${caldesc}", "$input{'Sepdays'}" );
         $added = &process_a_month( "OCT", $added, $calname, $caltype, $calyear, "${caldesc}", "$input{'Octdays'}" );
         $added = &process_a_month( "NOV", $added, $calname, $caltype, $calyear, "${caldesc}", "$input{'Novdays'}" );
         $added = &process_a_month( "DEC", $added, $calname, $caltype, $calyear, "${caldesc}", "$input{'Decdays'}" );
         # End of month checks
      }
   }
}
else {
   # First time in the form, show the intro
   print "<p>The calendar assist helper makes it easier to create calendars.\n";
   print "You <b>must</b> enter all the required fields (warning:no checking is done).\n";
   print "The commands will be stored for review by the scheduler administrators who\n";
   print "will make the final decision on whether to accept the command for processing\n";
   print "or not.</p>";
}

# -------------------------------------------------------------------------
# Print the normal data entry page now
# -------------------------------------------------------------------------

print "<br><hr><center><h2>Assist Helper - Define a new calendar entry here</h2></center><br>";
print "<form action=\"".&scheduler_HTTPD_Home."/html_assist_calendar.pl\" method=\"post\"><br><pre>\n";
print "<H3>Required Fields</H3>\n";
print "Calendar Name : <input type=text size=15 name=calname> (required, no spaces)\n";
print "Calendar Type : <select name=caltype><option value=\"JOB\">JOB</option>";
print "<option value=\"HOLIDAY\">HOLIDAY</option></select>     (required, JOB or HOLIDAY))\n";
print "Calendar Year : <select name=calyear>";
($sec,$min,$hour,$mday,$mon,$year,$wday,$ydays) = localtime(time());
$year = $year + 1900;
for ($i = 0; $i < 6; $i++) {
   print "<option value=\"$year\">$year</option>";
   $year = $year + 1;
}
print "</select>         (required, 4 digit year))\n";
print "Description   : <input type=text size=60 name=caldesc> (describe it)\n\n";

print "<H3>You must select at least one of, or a combination of, the below to provide rundates</H3>\n";
print "Ensure you check the checkbox next to each selection type if using it !\n\n";

print "<input type=checkbox name=dateseltype value=\"daylist\"><B>You may select by specific weekdays</B>\n";
print "       Sun Mon Tue Wed Thu Fri Sat\n";
print "        <input type=checkbox name=daylistname value=\"sun\">";
print "  <input type=checkbox name=daylistname value=\"mon\">";
print " <input type=checkbox name=daylistname value=\"tue\">";
print "  <input type=checkbox name=daylistname value=\"wed\">";
print " <input type=checkbox name=daylistname value=\"thu\">";
print "  <input type=checkbox name=daylistname value=\"fri\">";
print " <input type=checkbox name=daylistname value=\"sat\">";
print "\n\n";

print "<input type=checkbox name=dateseltype value=\"bydates\"><B>You may select by specific dates</B>\n";
&print_all_months();

print "\n\n<H3>Optional Additional Configuration</H3>\n";
print "Holiday calendar used to override this calendar : <input type=text size=40 name=holidaycal>\n";

print "</pre><center><input type=submit name=submitcmd value=\"Store Command\">&nbsp&nbsp&nbsp&nbsp<input type=reset>";
print "</center></form>";

print "<hr>\n";
print &HtmlBot;

exit;
__END__
