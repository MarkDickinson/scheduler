#!/usr/bin/perl
require "./cgi-lib.pl";
require "./scheduler-lib.pl";

$showwaitdetails = 0;   # Change to be checkbox controlled, if 1 lists dependencies

# Used to issue commands were we don't want a response.
sub issue_job_command() {
   $task = $_[0];
   $jobname = $_[1];
   $ipaddr = $_[2];
   select((select(STDOUT), $| = 1)[0]);
   print "";
   $pid = open(PIPE, "-|");
   if ($pid == 0) {
       $progname = &scheduler_cmdprog()." ".$ipaddr;
       open(CMDPGM, "| $progname") || die("Could not start command line interface.");
       print CMDPGM "AUTOLOGIN\n";
       print CMDPGM "SCHED ${task} ${jobname}\n";
       print CMDPGM "exit\n";
       close CMDPGM;
       exit;
   }
   @result = <PIPE>;
   print "<pre>";
   for (@result) {
       next if /command:/;
       next if /GPL V2 Release - this program is not warranted in any way, you use this/;
       next if /                 application at your own risk. Refer to the GPL V2 license./;
# Only used for debugging
#       $dataline = substr( $_, 0, length($_) );
#       print $dataline;
   }
}  # End of sub issue_job_command


sub JOB_status_display() {
    $ipaddr = $_[0];
    $schedhostname = $_[1];
    select((select(STDOUT), $| = 1)[0]);
    print "";
    $pid = open(PIPE, "-|");
    # in the child process the pid is 0
    # if we are the child sent the query and exit.
    if ($pid == 0) {
        $progname = &scheduler_cmdprog()." ".$ipaddr;
        open(CMDPGM, "| $progname") || die("Could not start command line interface.");
        print CMDPGM "SCHED LISTALL\n";
        print CMDPGM "exit\n";
        close CMDPGM;
        exit;
    }
    #     if we are the parent we wish to read the results of the query.
    while (length($schedhostname) < 11) { $schedhostname = $schedhostname." "; }
    if (length($schedhostname) > 11) { $schedhostname = substr($schedhostname, 0, 11); }
    $jobcount = 0;
    @result = <PIPE>;
    print "<pre>";
    for (@result) {
        next if /command:/;
        next if /GPL V2 Release - this program is not warranted in any way, you use this/;
        next if /                 application at your own risk. Refer to the GPL V2 license./;
        $dataline = substr( $_, 0, (length($_) - 1)); # -1 to omit the newline
        $jobname = substr( $dataline, 0, 30 );
        $status = substr( $dataline, 31, 7 );
        # Use fixed display line len to line the buttons up.
        while (length($dataline) < 62) { $dataline = $dataline." "; }
        $dataline = $schedhostname." ".$dataline;
        $failed = 0;
        if ($status eq "DEPENDE") {
           $jobcount = $jobcount + 1;
           print "<a style=\"cursor: hand\" title=\"job is waiting on dependencies\"><span style=\"background-color: #CCFFCC\">";
           print $dataline;
           print "</span></a>";
        }
        elsif ($status eq "SCHEDUL") {
           $jobcount = $jobcount + 1;
	   $testnull = substr( $jobname, 0, 4 );
	   if ($testnull eq "NULL") {
              print '<a style="cursor: hand" title="job will need to run with exclusive access when it begins running"><span style="background-color: #CCFFFF">';
	   }
	   else {
              print '<a style="cursor: hand" title="job is waiting for its time to run"><span style="background-color: #CCFFCC">';
           }
           print $dataline;
           print "</span></a>";
        }
        elsif ($status eq "COMPLET") {
           # We won't show completed jobs in the web page, it just clutters up the screen.
           $failed = 2;   # Not 1, use 2 for no display
        }
        elsif ($status eq "EXECUTI") {
           $jobcount = $jobcount + 1;
           print '<a style="cursor: hand" title="job was running at the time the screen was refreshed"><span style="background-color: #FFFFCC">';
           print $dataline;
           print "</span></a>";
           $failed = 2;   # no buttons, can't change it now
           print "\n";
        }
        elsif ($status eq "FAILED:") {
           $jobcount = $jobcount + 1;
           $alertcount = $alertcount + 1;
           print '<a style="cursor: hand" title="job failed, see alerts screen for details"><span style="background-color: #FF0000">';
           print $dataline;
           print "</span></a>";
           $failed = 1;
        }
        elsif ($status eq "CORRUPT") {
           $jobcount = $jobcount + 1;
           $alertcount = $alertcount + 1;
           print '<a style="cursor: hand" title="job has a corrupt active queue entry, page support"><span style="background-color: #FF0000">';
           print $dataline;
           print "</span></a>";
           $failed = 2;
           print "\n";
        }
        elsif ($status eq "HOLD IS") {
           $jobcount = $jobcount + 1;
           print '<a style="cursor: hand" title="job has been manually held"><span style="background-color: #00FFFF">';
           print $dataline;
           print "</span></a>";
	}
        elsif ($status eq "PENDING") {
           $jobcount = $jobcount + 1;
           print '<a style="cursor: hand" title="job is ready to run but has been deferred due to scheduler activity"><span style="background-color: #CCFFFF">';
           print $dataline;
           print "</span></a>";
	}
        else {
           if ($showwaitdetails != 0) {
              $status2 =  substr($status,0,4);
	      if ("${status2}." ne "DELE.") {
		   print "${dataline}\n";
	      }
           }
           # Ignore any other info, as deleted jobs can throw up dependency
           # wait info if they were deleted before dependency was removed.
           # If a user wants to see the dependencies they can view the info.
	   $failed = 2; 
        }
        $btnname = 'btninfo'.${jobcount};
        if ($failed == 1) {
           $urlname = 'html_alert_info.pl?'.${jobname}."=".${ipaddr};
           print ' ';
           &scheduler_add_hot_button( 'info_button_orange.jpg', 'info_button_red.jpg', ${urlname}, ${btnname} );
	   $failed = 2;  # stop the actions button being displayed
	   print "\n";   # and need this
        }
        elsif ($failed == 0) {
           $urlname = 'html_jobdbs_job_info.pl?'.${jobname}."=".${ipaddr};
           print ' ';
           &scheduler_add_hot_button( 'info_button_blue.jpg', 'info_button_red.jpg', ${urlname}, ${btnname} );
        }

        $test = substr( $jobname, 0, 9 );
        if ($test ne "SCHEDULER") {
           if ($failed != 2) {
              $btnname = 'btnjob'.${jobcount};
              $urlname = 'html_job_sched_action.pl?html_job_status.pl='.${jobname}."=".${ipaddr}."=".${schedhostname};
              print ' ';
              &scheduler_add_hot_button( 'Button_Actions_Blue.jpg', 'Button_Actions_Red.jpg', ${urlname}, ${btnname} );
              print "\n";
           }
        }
        else {
           if ($failed == 0) {
              print " (system task)\n";
           }
        }
    }
    print "</pre>";

    $testfail = substr( $dataline, 12, 18 );
    if ($testfail eq 'Connection refused') {
       print '<span style="background-color: #FF0000"> '.$schedhostname.' **** JOB SCHEDULER IS NOT AVAILABLE **** Check with support staff</span><br>';
    }
    elsif ($jobcount == 0) {
       print "<span style=\"background-color: #FF0000\">${dataline}</span><br>\n";
    }
} # End of sub JOB_status_display()

# =========================================================================
# Mainline starts...
# =========================================================================

# Use a two minute (120 second) auto-refresh of this page.
print &PrintHeader;
&scheduler_common_header( "html_job_status.pl", 120000 );

# Do we have any special processing to do first ?
$passed_get_commands = $ENV{'QUERY_STRING'};  # We pass the name in the query string
if (${passed_get_commands} ne "") {
  ($task,$jobname,$ipaddr) = split(/=/,$passed_get_commands);
  &issue_job_command( $task, $jobname, $ipaddr );
}

# Now show the job status display
#      Show the job status display
print "\n<h2>Jobs Currently Scheduled To Run</h2>\n";
# Show when the screen was last refreshed.
print "\nScreen was last refreshed at ";
$a = time();
$a = localtime($a);
print $a;
print ", autorefresh is aproximately 120 seconds\n";
print "\n<hr><pre>\nHost Name   JOB NAME                       JOB STATUS";

# Find out how many systems we are looking at, and look at them.
$xx = &scheduler_system_list( "GETCOUNT" );
if ($xx == 0) {
   &JOB_status_display( &scheduler_system_list( "GETIPADDR", 0 ),
                        &scheduler_system_list( "GETNAME", 0 ) );
}
else {
   for ($i = 1; $i <= $xx; $i = $i + 1 ) {
      &JOB_status_display( &scheduler_system_list( "GETIPADDR", $i ),
                           &scheduler_system_list( "GETNAME", $i ) );
   }
}

print "<hr>\n";
print &HtmlBot;

exit;
__END__
