#!/usr/bin/perl
require "cgi-lib.pl";
require "scheduler-lib.pl";

$submit_required = 0; # Nothing to write by default

# Used to write the line to the file.
sub scheduler_helper_record_line() {
   $dataline = $_[0];
   $source = $ENV{"REMOTE_ADDR"};
   $remotename = $ENV{"REMOTE_HOST"};
   if ($remotename ne "") {
      $source = $source." (".$remotename.")";
   }
   $filename = &scheduler_helper_output_file();
   open( CMDOUT, ">>${filename}" );
   print CMDOUT "#\n# Stored from ${source}\n${dataline}\n";
   close CMDOUT;
   print "<br>Last request stored by ${source} was...<br>${dataline}<br>";
}  # End of sub scheduler_helper_record_line


# =========================================================================
# Mainline starts...
# =========================================================================
print &PrintHeader;
&scheduler_common_header( "html_job_status.pl", 1200000 ); #Jump in 20 minutes
print "<center><h1>Assist Helper - Defining a Scheduler User</h1></center><br>";

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
      $username = $input{ 'username' };
      $userpass = $input{ 'userpass' };
      $userdesc = $input{ 'userdesc' };
      $userauth = $input{ 'userauth' };
      $autoauth = $input{ 'autoauth' };
      $subauth  = $input{ 'subauth' };
      $userauthlist = "";

      # User list only required if user has JOB authority
      if ($userauth eq "job") {
         $test    = $input{ 'usersub1' };
	 if ($test ne "") { $userauthlist = $test; }
         $test    = $input{ 'usersub2' };
	 if ($test ne "") { $userauthlist = $userauthlist.",".$test; }
         $test    = $input{ 'usersub3' };
	 if ($test ne "") { $userauthlist = $userauthlist.",".$test; }
         $test    = $input{ 'usersub4' };
	 if ($test ne "") { $userauthlist = $userauthlist.",".$test; }
         $test    = $input{ 'usersub5' };
	 if ($test ne "") { $userauthlist = $userauthlist.",".$test; }
         $test    = $input{ 'usersub6' };
	 if ($test ne "") { $userauthlist = $userauthlist.",".$test; }
         $test    = $input{ 'usersub7' };
	 if ($test ne "") { $userauthlist = $userauthlist.",".$test; }
         $test    = $input{ 'usersub8' };
	 if ($test ne "") { $userauthlist = $userauthlist.",".$test; }
         $test    = $input{ 'usersub9' };
	 if ($test ne "") { $userauthlist = $userauthlist.",".$test; }
         $test    = $input{ 'usersub10' };
	 if ($test ne "") { $userauthlist = $userauthlist.",".$test; }
      }

      # Work out what the command is
      $command_line = "USER ADD ${username},PASSWORD ${userpass},AUTH ${userauth},AUTOAUTH ${autoauth}";
      $command_line = $command_line.",SUBSETAUTH ${subauth}";
      if ($userauthlist ne "") {
         $command_line = $command_line.",USERLIST \"".$userauthlist."\"";
      }
      $command_line = $command_line.",DESC \"".$userdesc."\"";

      # Now record it.
      &scheduler_helper_record_line( "${command_line}" );
   }
}
else {
   # First time in the form, show the intro
   print "<p>The user assist helper makes it easier to define users than having to remember\n";
   print "the entire command syntax yourself. For <em>security reasons</em>, as the web server\n";
   print "should not have access to define users; the generated command output is written to\n";
   print "a file on the server. This file will be reviewed by administration personel\n";
   print "on a regular basis, it is they who decide whether your job will actually be created\n";
   print "or not.</p>\n";
   print "<p>It is important to note that the users being defines here are users that are\n";
   print "permitted to access the scheduler functions only; you are not defining native\n";
   print "unix users.</p>";
}

# -------------------------------------------------------------------------
# Print the normal data entry page now
# -------------------------------------------------------------------------

print "<br><hr><center><h2>Assist Helper - Define a new scheduler user here</h2></center><br>";
print "<form action=\"".&scheduler_HTTPD_Home."/html_assist_user.pl\" method=\"post\"><br><pre>\n";
print "<H3>Required Fields</H3>\n";
print "User Name   : <input type=text size=15 name=username> (required, if user has a unix account use that id)\n";
print "Password    : <input type=text size=15 name=userpass> (required, users scheduler pswd (seperate from unix pswd))\n";
print "Description : <input type=text size=60 name=userdesc> (who is this user)\n\n";
print "Authority level to be granted : \n";
print "  <input type=radio name=userauth value=\"admin\">Total Control, can perform any function (Admin rights)\n";
print "  <input type=radio name=userauth checked value=\"operator\">Operator Authority (Active Job and Alert control, no database update)\n";
print "  <input type=radio name=userauth value=\"job\">Job definition (Create/Delete jobs and calendars, no execution control)\n";
print "  <input type=radio name=userauth value=\"security\">Security (Create/Delete users, only browse access to other operations)\n";
print "\nUser list : this is a list of unix userids that this user can create jobs for\n";
print "            only add the minimum required for this users function, ie: if the\n";
print "            user is an oracle dbs, only add the oracle user.\n";
print "            <b>These are only requires for a user with \"JOB\" authority</b>\n";
print "               <input type=text size=15 name=usersub1>";
print "  <input type=text size=15 name=usersub2>";
print "  <input type=text size=15 name=usersub3>\n";
print "               <input type=text size=15 name=usersub4>";
print "  <input type=text size=15 name=usersub5>";
print "  <input type=text size=15 name=usersub6>\n";
print "               <input type=text size=15 name=usersub7>";
print "  <input type=text size=15 name=usersub8>";
print "  <input type=text size=15 name=usersub9>\n";
print "               <input type=text size=15 name=usersub10>\n";


print "\n\n<H3>Optional Additional Configuration</H3>\n";
print "Is the user permitted to use the autologin command. Read the manual !\n";
print "It is unsafe to guess yes/no, consult the manual if you don't fully\n";
print "understand this function, or your systems administrators will not\n";
print "process the command for you.\n";
print "Autoauth allowed : <input type=radio name=autoauth checked value=\"no\">No";
print "  <input type=radio name=autoauth value=\"yes\">Yes\n\n";

print "Is the user permitted subset authority. This is defined as\n";
print "  No    - the user may not create any other users\n";
print "  Yes   - allowed to create other users that can create jobs to run\n";
print "          with the same userids as in this users list.\n";
print "  Admin - Grants the user <b>full security rights</b> even if they\n";
print "          are not setup with a security auth level.\n";
print "User Subset Authority allowed : <input type=radio name=subauth checked value=\"no\">No";
print "  <input type=radio name=subauth value=\"yes\">Yes";
print "  <input type=radio name=subauth value=\"admin\">Admin\n\n";
print "</pre><center><input type=submit name=submitcmd value=\"Store Command\">&nbsp&nbsp&nbsp&nbsp<input type=reset>";
print "</center></form>";

print "<hr>\n";
print &HtmlBot;

exit;
__END__
