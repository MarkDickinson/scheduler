#!/usr/bin/perl
require "cgi-lib.pl";
require "scheduler-lib.pl";

print &PrintHeader;
&scheduler_common_header( "html_job_status.pl", 120000 );

print "<h2>Current Dependency Queue Entries</h2>";

# Used to issue commands were we don't want a response.
sub issue_command() {
   $ipaddr = $_[0];
   $command = $_[1];
   $command =~ s/%20/ /g;  # translate the %20 back to spaces in the command

   select((select(STDOUT), $| = 1)[0]);
   print "";
   $pid = open(PIPE, "-|");
   if ($pid == 0) {
      $progname = &scheduler_cmdprog()." ".$ipaddr;
      open(CMDPGM, "| $progname") || die("Could not start command line interface.");
      print CMDPGM "AUTOLOGIN\n";
      print CMDPGM "${command}\n";
      print CMDPGM "exit\n";
      close CMDPGM;
      exit;
   }
   @result = <PIPE>;
   print "<pre>";
   for (@result) {
      next if /command:/;
      next if /GPL Release - this program is not warranted in any way, you use this/;
      next if /              application at your own risk. Refer to the GPL license./;
# only used for debugging
#       $dataline = substr( $_, 0, length($_) );
#       $dataline = substr( $_, 0, length($_) );
#       print $dataline;
   }
}

sub getjobname() {
   my $temp = $_[0];
   my ($a,$b) = split(/ /,$temp);
   return "${b}";
}

sub getdepname() {
   my $temp = $_[0];
   $temp = substr($temp,9,100);
   # remove trailing spaces
   my $junk = index($temp," ",0);
   if ($junk > 0) {
      $temp = substr( $temp, 0, $junk );
   }
   return "${temp}";
}

sub DEPENDENCIES_listall() {
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
       print CMDPGM "DEP LISTALL\n";
       print CMDPGM "exit\n";
       close CMDPGM;
       exit;
   }
   # if we are the parent we wish to read the results of the query.
   while (length($schedhostname) < 11) { $schedhostname = $schedhostname." "; }
   if (length($schedhostname) > 11) { $schedhostname = substr( $schedhostname, 0, 11 ); }
   @result = <PIPE>;
   for (@result) {
       next if /command:/;
       next if /GPL Release - this program is not warranted in any way, you use this/;
       next if /              application at your own risk. Refer to the GPL license./;
       $dataline = substr( $_, 0, (length($_) - 1) ); # -1 to strip lf
       $test1 = substr( $dataline, 0, 3 );
       $test2 = substr( $dataline, 4, 3 );
       if (length($dataline) > 70) {
   	    $dataline = substr($dataline,0,70);
       }
       if ($test1 eq "Job") {
          $jobname=&getjobname($dataline);
          print "<br>${schedhostname} ${dataline}  <A HREF=\"html_list_depq.pl?${ipaddr}=DEP CLEARALL JOB ${jobname}\">Clear all for this job</A>\n";
       }
       elsif ($test2 eq "JOB") {
          $depname=&getdepname($dataline);
          print "            ${dataline}  <A HREF=\"html_list_depq.pl?${ipaddr}=DEP CLEARALL DEP ${depname}\">Clear from ALL jobs</A>\n";
       }
       elsif ($test2 eq "FIL") {
          $depname=&getdepname($dataline);
          print "            ${dataline}  <A HREF=\"html_list_depq.pl?${ipaddr}=DEP CLEARALL DEP ${depname}\">Clear from ALL jobs</A>\n";
       }
       else {
           $testfail = substr( $dataline, 0, 18 );
           if ( ($testfail eq 'Connection refused') || ($testfail eq 'No route to host: ') ) {
               print "<a style=\"cursor: hand\" title=\"The job scheduling server on ${schedhostname} is not responding\">";
               print "<span style=\"background-color: #FF0000\">";
               print "${schedhostname} ${dataline}<br></span></a>";
           }
       }
#       else {
#          print "${schedhostname} ${dataline}\n";
#       }
   }
} # sub DEPENDECIES_listall

# See if we have any commands to issue first
$passed_get_commands = $ENV{'QUERY_STRING'};  # We pass the name in the query string
if (${passed_get_commands} ne "") {
   ($ipaddr,$command) = split(/=/,$passed_get_commands);
   &issue_command( $ipaddr, $command );
}

# Find out how many systems we are looking at and look at them
print "<pre>\n";
$xx = &scheduler_system_list( "GETCOUNT" );
if ($xx == 0) {
   &DEPENDENCIES_listall( &scheduler_system_list( "GETIPADDR", 0 ),
                          &scheduler_system_list( "GETNAME", 0 ) );
}
else {
   for ($i = 1; $i <= $xx; $i = $i + 1 ) {
      &DEPENDENCIES_listall( &scheduler_system_list( "GETIPADDR", $i ),
                             &scheduler_system_list( "GETNAME", $i ) );
   }
}
print "</pre>\n";

exit;
__END__
