#!/usr/bin/perl
require "./cgi-lib.pl";
require "./scheduler-lib.pl";

# If no input in two minutes return to job status display.
print &PrintHeader;
&scheduler_common_header( "html_job_status.pl", 120000 );

# We must have been provided a jobname, die if not.
$passed_get_commands = $ENV{'QUERY_STRING'};  # We pass the name in the query string
if (${passed_get_commands} ne "") {
  ($execurl,$jobname,$ipaddr,$hostname) = split(/=/,$passed_get_commands);
}
else { die( "No jobname passed to $0"); }
my $junk = index($jobname,"%20",0);
$jobname = substr($jobname, 0, $junk );

print "<h2>Altering Job ${jobname} for ${hostname}</h2>\n";

$urlname = "${execurl}?HOLD-ON=".${jobname}."=".${ipaddr};
&scheduler_add_hot_button( 'Button_Holdon_Blue.jpg', 'Button_Holdon_Red.jpg', ${urlname}, 'Btn1' );
print " Hold the job (prevent it from running)<br>\n";

$urlname = "${execurl}?HOLD-OFF=".${jobname}."=".${ipaddr};
&scheduler_add_hot_button( 'Button_Holdoff_Blue.jpg', 'Button_Holdoff_Red.jpg', ${urlname}, 'Btn2' );
print " Release the job, allow it to be scheduled on when ready to run<br>\n";

$urlname = "${execurl}?RUNNOW=".${jobname}."=".${ipaddr};
&scheduler_add_hot_button( 'Button_Runnow_Blue.jpg', 'Button_Runnow_Red.jpg', ${urlname}, 'Btn3' );
print " Run the job now. Only effective if the job is in a time-wait state.<br>\n";

print "<br><br>\n";
$urlname = "${execurl}?DELETE=".${jobname}."=".${ipaddr};
&scheduler_add_hot_button( 'Button_Delete_Blue.jpg', 'Button_Delete_Red.jpg', ${urlname}, 'Btn4' );
print " Delete the job from the scheduler active queue. <em>See considerations below</em>.<br>\n";
print "<br><em>Considerations for deleting a job</em><br>It is important to note that you should only ever\n";
print "delete a job on the request of your systems administrator. Deleteing a job will remove\n";
print "it from the active queue <b>but will not update the job last/next runtime flags</b> as\n";
print "the job has not actually run and the audit information needs to reflect that.<br>\n";
print "The effect of this is that the job must be manually rescheduled on again by the systems administrator\n";
print "at some point <b>before</b> the scheduler newday task runs.<br>As a rule-of-thumb only delete a job\n";
print "from the active scheduler queue if it is the system administrators intent to also delete the\n";
print "job permanently/forever from the job definition database also.\n";

print "<hr>\n";
print &HtmlBot;

exit;
__END__
