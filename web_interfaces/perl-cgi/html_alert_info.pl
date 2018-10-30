#!/usr/bin/perl
require "./cgi-lib.pl";
require "./scheduler-lib.pl";

print &PrintHeader;
&scheduler_common_header( "html_alert_status.pl", 120000 );

$jobname = $ENV{'QUERY_STRING'};  # We pass the name in the query string
($jobname,$ipaddr) = split(/=/,$jobname);
my $junk = index($jobname,"%20",0);
$jobname = substr($jobname, 0, $junk);
if ($jobname eq "") {
   printf( "*ERROR* NO JOBNAME WAS PASSED TO THE SCRIPT\n" );
   print &HtmlBot;
   exit;
}

$btncount = 0;
print "<h2>Alert details for ${jobname}</h2>";
print "<pre>\n";

select((select(STDOUT), $| = 1)[0]);
print "";
$pid = open(PIPE, "-|");
# in the child process the pid is 0
# if we are the child sent the query and exit.
if ($pid == 0) {
    $progname = &scheduler_cmdprog()." ".${ipaddr};
    open(CMDPGM, "| $progname") || die("Could not start command line interface.");
    print CMDPGM "ALERT INFO ${jobname}\n";
    print CMDPGM "exit\n";
    close CMDPGM;
    exit;
}
# if we are the parent we wish to read the results of the query.
@result = <PIPE>;
for (@result) {
    next if /command:/;
    next if /GPL V2 Release - this program is not warranted in any way, you use this/;
    next if /                 application at your own risk. Refer to the GPL V2 license./;
    $dataline = substr( $_, 0, length($_) );
    $test=substr($dataline,0,8);
    if ($test eq "MSG TEXT") {
       print "<b>"; print ${dataline}; print "</b>";
    }
    else {
      print ${dataline};
    }
}
print "</pre><br><center><a href=\"".&scheduler_HTTPD_Home()."/html_alert_status.pl\"><img src=\"".&scheduler_icon_path()."alerts_green.jpg\"></a>";
print &HtmlBot;
exit;
__END__
