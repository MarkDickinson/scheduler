#!/usr/bin/perl
require "cgi-lib.pl";
require "scheduler-lib.pl";

#
# Display details for a calendar, we expect to be passed
# <calname>,type <caltype>,year <year>=<ipaddress of server>
#

print &PrintHeader;
&scheduler_common_header( "html_job_status.pl", 120000 );

$dataline = $ENV{'QUERY_STRING'};  # We pass the name in the query string
($dataline,$ipaddr) = split(/=/,$dataline);
if ($dataline eq "") {
   printf( "*ERROR* NO CALENDAR NAME WAS PASSED TO THE SCRIPT\n" );
   print &HtmlBot;
   exit;
}
($calname,$caltype,$calyear) = split(/,/,$dataline);

print "<h2>Calndar details for ${calname}, type ${caltype}, year ${calyear}</h2>";
print "<br><pre>\n";

select((select(STDOUT), $| = 1)[0]);
print "";
$pid = open(PIPE, "-|");
# in the child process the pid is 0
# if we are the child sent the query and exit.
if ($pid == 0) {
    $progname = &scheduler_cmdprog()." ".${ipaddr};
    open(CMDPGM, "| $progname") || die("Could not start command line interface.");
    print CMDPGM "CALENDAR INFO ${calname},TYPE ${caltype},YEAR ${calyear}\n";
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
