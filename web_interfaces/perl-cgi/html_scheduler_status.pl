#!/usr/bin/perl
require "cgi-lib.pl";
require "scheduler-lib.pl";

sub SCHED_status_display() {
    local $ipaddr = $_[0];
    # Show the scheduler status display
    print "\n<h2>Server Status Information</h2><br>\n";
    # Show we will redirect in 120 seconds.
    print "Redirect to the job status display in aproximately 120 seconds\n\n";
    select((select(STDOUT), $| = 1)[0]);
    print "";
    $pid = open(PIPE, "-|");
    # in the child process the pid is 0
    # if we are the child sent the query and exit.
    if ($pid == 0) {
        $progname = &scheduler_cmdprog()." ".$ipaddr;
        open(CMDPGM, "| $progname") || die("Could not start command line interface.");
        print CMDPGM "SCHED STATUS\n";
        print CMDPGM "exit\n";
        close CMDPGM;
        exit;
    }
    #     if we are the parent we wish to read the results of the query.
    @result = <PIPE>;
    print "<hr><br><pre>";
    for (@result) {
        next if /command:/;
        next if /GPL Release - this program is not warranted in any way, you use this/;
        next if /              application at your own risk. Refer to the GPL license./;
        $dataline = substr( $_, 0, length($_) );
	print "${dataline}";
    }
} # End of sub JOB_status_display()

# =========================================================================
# Mainline starts...
# =========================================================================

print &PrintHeader;
# Use a two minute (120 second) auto-refresh of this page.
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
      print "Select the server you wish a status display from.<br><br><big><big>\n";
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

# show the job status display
&SCHED_status_display( $ipaddr );

print "<hr>\n";
print &HtmlBot;

exit;
__END__
