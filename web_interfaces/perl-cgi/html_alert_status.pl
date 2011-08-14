#!/usr/bin/perl
require "cgi-lib.pl";
require "scheduler-lib.pl";

# GLOBAL alertcount
$ALERTCOUNT = 0;

sub issue_alert_command() {
   $task = $_[0];
   $jobname = $_[1];
   $ipaddr = $_[2];
   my $junk = index($jobname,"%20",0);
   $jobname = substr($jobname, 0, $junk );

   select((select(STDOUT), $| = 1)[0]);
   print "";
   $pid = open(PIPE, "-|");
   if ($pid == 0) {
       $progname = &scheduler_cmdprog()." ".${ipaddr};
       open(CMDPGM, "| $progname") || die("Could not start command line interface.");
       print CMDPGM "AUTOLOGIN\n";
       print CMDPGM "ALERT ${task} ${jobname}\n";
       print CMDPGM "exit\n";
       close CMDPGM;
       exit;
   }
   @result = <PIPE>;
   for (@result) {
       next if /command:/;
       next if /GPL Release - this program is not warranted in any way, you use this/;
       next if /              application at your own risk. Refer to the GPL license./;
       # Don't bother showing command results here.
#          $dataline = substr( $_, 0, length($_) ); 
#          if (length($dataline) > 3) {
#             print ${dataline};
#             print "HOST=${ipaddr}\n";
#          }
   }
}

sub SHOW_ALERTS() {
   $ipaddr = $_[0];
   $schedhostname = $_[1];
   select((select(STDOUT), $| = 1)[0]);
   print "";
   $pid = open(PIPE, "-|");
   # in the child process the pid is 0
   # if we are the child sent the query and exit.
   if ($pid == 0) {
       $progname = &scheduler_cmdprog()." ".${ipaddr};
       open(CMDPGM, "| $progname") || die("Could not start command line interface.");
       print CMDPGM "ALERT LISTALL\n";
       print CMDPGM "exit\n";
       close CMDPGM;
       exit;
   }
   # if we are the parent we wish to read the results of the query.
   while (length($schedhostname) < 11) { $schedhostname = $schedhostname." "; }
   if (length($schedhostname) > 11) { $schedhostname = substr($schedhostname, 0, 11); }
   @result = <PIPE>;
   $btncount = 0;
   for (@result) {
       next if /command:/;
       next if /GPL Release - this program is not warranted in any way, you use this/;
       next if /              application at your own risk. Refer to the GPL license./;
       $dataline = substr( $_, 0, (length($_) - 1) ); # -1 to drop lf byte
       $test = substr( $dataline, 1, 5 );
       $testfail = substr( $dataline, 0, 18 );
       if ( ($testfail eq 'Connection refused') || ($testfail eq 'No route to host: ') ) {
          print "<a style=\"cursor: hand\" title=\"The job scheduling server on ${schedhostname} is not responding\">";
          print "<span style=\"background-color: #FF0000\">";
          print "${schedhostname} ${dataline}<br></span></a>";
       }
       elsif ($test ne "") {
          # And we have an alert counter to update
          $ALERTCOUNT = $ALERTCOUNT + 1;
          # Then the original stuff
          $ackchar = substr( $dataline, (length($dataline) - 1), 1 );
          $btncount = $btncount + 1;
          $jobname = substr( $dataline, 18, 30 );
          print " <a style=\"cursor: hand\" title=\"Job ${jobname} has failed and needs intervention\"><span style=\"background-color:";
          if (${ackchar} eq 'Y') {
             print " #FF00FF\"> ";
          }
          else {
             print " #FF0000\"> ";
          }
          print "$schedhostname $dataline";
          print "</span></a> ";
          $urlname = 'html_alert_info.pl?'.${jobname}."=".${ipaddr};
          $btnname = 'alertinf'.${btncount};
          &scheduler_add_hot_button( 'info_button_blue.jpg', 'info_button_red.jpg', ${urlname}, ${btnname} );
          $urlname = 'html_alert_status.pl?RESTART='.${jobname}."=".${ipaddr};
          $btnname = 'alertres'.${btncount};
          &scheduler_add_hot_button( 'restart_blue.jpg', 'restart_red.jpg', ${urlname}, ${btnname} );

          if (${ackchar} ne 'Y') {
             $urlname = 'html_alert_status.pl?ACK='.${jobname}."=".${ipaddr};
             $btnname = 'alertack'.${btncount};
             &scheduler_add_hot_button( 'ack_alert_blue.jpg', 'ack_alert_red.jpg', ${urlname}, ${btnname} );
          }

          print "\n";
       }
   }
   return;
}

# Use a two minute (120 second) auto-refresh of this page.
print &PrintHeader;
&scheduler_common_header( "html_alert_status.pl", 120000 );

# Do we have any special processing to do first ?
$passed_get_commands = $ENV{'QUERY_STRING'};  # We pass the name in the query string
if (${passed_get_commands} ne "") {
  ($task,$jobname,$ipaddr) = split(/=/,$passed_get_commands);
  &issue_alert_command( $task, $jobname, $ipaddr );
}

# Show the title stuff
print "<h2>Outstanding Alerts</h2>\n";
# Show when the screen was last refreshed.
print "Screen was last refreshed at ";
$a = time();
$a = localtime($a);
print $a;
print "\n";

# Show any outstanding alerts
print "<br><pre>";
$xx = &scheduler_system_list( "GETCOUNT" );
if ($xx == 0) {
   &SHOW_ALERTS( &scheduler_system_list( "GETIPADDR", 0 ),
                 &scheduler_system_list( "GETNAME", 0 ) );
}
else {
   for ( $i = 1; $i <= $xx; $i = $i + 1 ) {
      &SHOW_ALERTS( &scheduler_system_list( "GETIPADDR", $i ),
                    &scheduler_system_list( "GETNAME", $i ) );
   }
}
print "</pre>\n"; 
# Any alerts ?
if ($ALERTCOUNT == 0) {
   print "There are no current alerts to display for any monitored system\n"
}
print &HtmlBot;

exit;
__END__
