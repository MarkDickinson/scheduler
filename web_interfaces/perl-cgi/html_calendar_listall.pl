#!/usr/bin/perl
require "cgi-lib.pl";
require "scheduler-lib.pl";

sub CAL_dbslist_display() {
    $ipaddr = $_[0];
    print "\n<pre>\nYEAR TYPE    CALENDAR NAME                           DESCRIPTION\n<hr>\n";
    select((select(STDOUT), $| = 1)[0]);
    print "";
    $pid = open(PIPE, "-|");
    # in the child process the pid is 0
    # if we are the child sent the query and exit.
    if ($pid == 0) {
        $progname = &scheduler_cmdprog()." ".$ipaddr;
        open(CMDPGM, "| $progname") || die("Could not start command line interface.");
        print CMDPGM "CALENDAR LISTALL\n";
        print CMDPGM "exit\n";
        close CMDPGM;
        exit;
    }
    #     if we are the parent we wish to read the results of the query.
    $calcount = 0;
    @result = <PIPE>;
    print "<br><pre>";
    for (@result) {
        next if /command:/;
        next if /GPL Release - this program is not warranted in any way, you use this/;
        next if /              application at your own risk. Refer to the GPL license./;
        $dataline = substr( $_, 0, (length($_) - 1)); # -1 to omit the newline
	$test=substr( $dataline, 0, 1 );
	if (length($dataline) < 1) { $dataline = " "; }
	if ($dataline ne " ") {
           $calyear = substr( $dataline, 0, 4 );
           $caltype = substr( $dataline, 5, 7 );
           $calname = substr( $dataline, 13, 40 );
           # Strip trailing spaces from the calendar name
           my $junk = index($calname," ",0);
           if ($junk > 0) {
              $calname = substr( $calname, 0, $junk );
           }
           my $junk = index($caltype," ",0);
           if ($junk > 0) {
              $caltype = substr( $caltype, 0, $junk );
           }
           # Use fixed display line len to line the buttons up.
           while (length($dataline) < 84) { $dataline = $dataline." "; }
	   if (length($dataline) > 84) { $dataline = substr($dataline,0,84); }
           print $dataline;
	   $calcount = $calcount + 1;
           $btnname = 'btninfo'.${calcount};
           $urlname = 'html_calendar_info.pl?'.${calname}.','.${caltype}.','.${calyear}.'='.${ipaddr};
           print ' ';
           &scheduler_add_hot_button( 'info_button_blue.jpg', 'info_button_red.jpg', ${urlname}, ${btnname} );
	   print "\n";
       }
    }

    $testfail = substr( $dataline, 0, 18 );
    if ($testfail eq 'Connection refused') {
       print '<span style="background-color: #FF0000"> **** JOB SCHEDULER IS NOT AVAILABLE **** Check with support staff</span><br><br><br>';
    }
    elsif ($calcount == 0) {
       print "<br><br>THERE ARE NO CALENDARS DEFINED IN THE CALENDAR DATABASE<br><br>\n";
    }
} # End of sub CAL_dbslist_display()

# =========================================================================
# Mainline starts...
# =========================================================================

# Show page banner
# Use a two minute (120 second) auto-refresh of this page.
print &PrintHeader;
&scheduler_common_header( "html_job_status.pl", 120000 );

local $envstring = $ENV{'QUERY_STRING'}; # was an ip address passed to us ?
($junk,$ipaddr) = split(/=/,$envstring);
if ($ipaddr eq "") {            # if blank, then it was not
   $xx = &scheduler_system_list( "GETCOUNT" );
   if ($xx == 0) {
      $ipaddr = &scheduler_system_list( "GETIPADDR", 0 );
      $schedhostname = &scheduler_system_list( "GETNAME", 0 );
      # and fall through to carry on
   }
   # else we are monitoring more than one system so we
   # need to make the user select a system to view.
   else {
      print "<br>Currently multiple systems are being monitored.<br>\n";
      print "Select the server you wish to view the job definitions for.<br><br><big><big>\n";
      print "<form method=\"get\">\n<select name=ipaddr>\n";
      for ($i = 1; $i <= $xx; $i = $i + 1 ) {
         print "  <option value=\"";
         print &scheduler_system_list( "GETIPADDR", $i );
         print "\">";
         print &scheduler_system_list( "GETNAME", $i );
         print "</option>\n";
      }
      print "</select>\n<input value=\"Query selected server\" type=submit>\n";
      print "</form></big></big>\n";
      print &HtmlBot;
      exit;
   }
}

# Now show the job status display
&CAL_dbslist_display($ipaddr);

print "<hr>\n";
print &HtmlBot;

exit;
__END__
