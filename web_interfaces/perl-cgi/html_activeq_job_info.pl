#!/usr/bin/perl
require "cgi-lib.pl";
require "scheduler-lib.pl";

print &PrintHeader;
&scheduler_common_header( "html_job_status.pl", 120000 );

$jobname = $ENV{'QUERY_STRING'};  # We pass the name in the query string
if ($jobname eq "") {
   printf( "*ERROR* NO JOBNAME WAS PASSED TO THE SCRIPT\n" );
   print &HtmlBot;
   exit;
}

$test = substr($jobname,0,9);
if ($test eq "SCHEDULER") {
        print "<h2>Job details for queued job ${jobname}</h2>";
	print "<p>This is an internally managed job schedler server maintenance task.</p>\n";
	print "<p>These job details are maintained by the job server and are not viewable\n";
	print "or maintainable by users.</p>\n";
	if ($jobname eq "SCHEDULER-NEWDAY") {
		print "<p>The SCHEDULER-NEWDAY job is responsible for cleaning up and\n";
		print "compressing databases used by the previous days batch run, and\n";
		print "for scheduling on the next days batch jobs.</p>\n";
	}
	print $HtmlBot;
	exit;
}

print "<h2>Job details for queued job ${jobname}</h2>";
print "<em>Note:</em> the job details are as the job was defined. Any dependencies listed\n";
print "may already be satisfied.<br>The real-world outstanding dependencies for all jobs are\n";
print "available on the dependency queue screen.<br>\n";
print "<pre>\n";

select((select(STDOUT), $| = 1)[0]);
print "";
$pid = open(PIPE, "-|");
# in the child process the pid is 0
# if we are the child sent the query and exit.
if ($pid == 0) {
    $progname = &scheduler_cmdprog();
    open(CMDPGM, "| $progname") || die("Could not start command line interface.");
    print CMDPGM "JOB INFO ${jobname}\n";
    print CMDPGM "exit\n";
    close CMDPGM;
    exit;
}
# if we are the parent we wish to read the results of the query.
@result = <PIPE>;
for (@result) {
    next if /command:/;
    next if /GPL Release - this program is not warranted in any way, you use this/;
    next if /              application at your own risk. Refer to the GPL license./;
    $dataline = substr( $_, 0, length($_) );
    $test=substr($dataline,0,8);
    print ${dataline};
}
print &HtmlBot;
exit;
__END__
