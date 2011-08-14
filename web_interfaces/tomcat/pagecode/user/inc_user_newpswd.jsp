<%--
  inc_user_newpswd.jsp

  Called for a user password change request.
  If we are called from the user info display screen we already
  have a username to use, so we use that.
  If we are called from the menu we will have no user name
  so must provide an input box for it.

  If we are called with the "doit=Change user password" we have been called by
  ourself and have all the needed parameters, so just doit.

  The user name and new password must be passed to the job
  scheduler servers in a user record structure, as defined
  in the job scheduler userrecdef.h, as below...

#define USER_MAX_NAMELEN 39
#define USER_MAX_PASSLEN 39
#define USER_LIB_VERSION "01"
#define USERS_MAX_USERLIST 10

typedef struct {
   char userid[USER_MAX_NAMELEN+1];  /* a unix userid */
} USER_unix_userid_element;

typedef struct {
   char userrec_version[3];                 /* version of userrec to allow later on-the-fly upgrades 01 */
   char user_name[USER_MAX_NAMELEN+1];      /* The user name                      */
   char user_password[USER_MAX_PASSLEN+1];  /* users password                     */
   char record_state_flag;                  /* A=active, D=deleted                */
   char user_auth_level;                    /* O=operator, A=admin (all access), 0=none/browse,
                                               J=job auth (browse+add/delete jobs),
                                               S=security user...user maint only */
   char local_auto_auth;                    /* Y=allowed to autologin from jobsched_cmd effective uid locally, N=no */
   char allowed_to_add_users;               /* Y=allowed to add/delete auth to add jobs for users in their auth list
                                               N=not at all,
                                               S=can create users with auth to any unix id */
   USER_unix_userid_element user_list[USERS_MAX_USERLIST];  /* users this ID can define jobs for  */
   char user_details[50];                   /* free form text (ie: real name)     */
   char last_logged_on[18];                 /* DATE_TIME_LEN(scheduler.h)+1 */
   char last_password_change[18];           /* DATE_TIME_LEN(scheduler.h)+1 */
} USER_record_def;
--%>
<%
   int doPrompt = 1; // default is we want input
   String testDoIt = request.getParameter("doit");
   if (testDoIt == null) { testDoIt = "No"; }
   if (testDoIt.equals( "Change user password" )) { doPrompt = 0; }

   // see if we have the username, if its missing prompt for it.
   // In this case even if 'doit' is set we have to prompt as
   // we cannot delete without a username now can we.
   String schedUserName = request.getParameter("username");
   if (schedUserName == null) {
      schedUserName = ""; 
      doPrompt = 1; // force it back on if turned off above
   }
   String localHostName = request.getParameter("server");
   if (localHostName == null) {
      localHostName = ""; 
      doPrompt = 1; // force it back on if turned off above
   }
   String passWord1 = request.getParameter("password1");
   String passWord2 = request.getParameter("password2");
   if ( (passWord1 == null) || (passWord2 == null) ) {
      passWord1 = ""; 
      passWord2 = ""; 
      doPrompt = 1; // force it back on if turned off above
   }
   if ( !(passWord1.equals(passWord2)) ) {
      out.println( "<br><br><b>Passwords did not match, try again</b><br><br>" );
      passWord1 = ""; 
      passWord2 = ""; 
      doPrompt = 1; // force it back on if turned off above
   }

   //
   // If we are required to prompt for user input do so, then call
   // ourself back when we have it.
   //
   if (doPrompt == 1) {
      out.println("<form action=\"" + request.getScheme() + "://" + request.getServerName() + ":8080" +
                  request.getRequestURI() + "\" method=\"post\">");
      if (schedUserName.length() < 1) {
         out.println( "<h2>Changing a users password, which one ?</h2>" );
         out.println("<INPUT TYPE=HIDDEN NAME=\"pagename\" VALUE=\"usernewpswd\"></INPUT>");
         out.println("Enter <b>Userid</b> to change password for : <INPUT TYPE=TEXT WIDTH=39 NAME=\"username\"></INPUT><br><br>");
      } else {
         out.println( "<h2>Changing password for user " + schedUserName + "</h2>" );
         out.println("<INPUT TYPE=HIDDEN NAME=\"pagename\" VALUE=\"usernewpswd\">");
         out.println("<INPUT TYPE=HIDDEN NAME=\"username\" VALUE=\"" + schedUserName + "\">");
      }
      // Build up a list of servers we know about, the user can select one (or
      // all) to issue the job add command to.
      out.println("<b>Select the server to change the users password on</b>: <SELECT NAME=\"server\"><OPTION>All Servers</OPTION>");
      for (int i = 0; i < socketPool.getHostMax(); i++) {
         out.println("<OPTION>" + socketPool.getHostName(i) + "</OPTION>");
      }
      out.println("</SELECT><BR>You must have security authority for user management");
%>
<br><br><pre>
 Enter the new password : <INPUT TYPE=PASSWORD NAME="password1"></INPUT><br>
Repeat the new password : <INPUT TYPE=PASSWORD NAME="password2"></INPUT><br>
</pre>
<%
      out.println("<input type=submit name=doit value=\"Change user password\"> </form><br><br>" );
   } // if doPrompt == 1
   //
   // else we have all the input, we need to delete the user record
   //
   else {
     int numHosts, i, zz;
	 String dataLine;

     // Need to create the user record buffer.
	 String userRecord = "01^" + schedUserName;
	 while (userRecord.length() < 42) { userRecord = userRecord + " "; } // 39 + 2 byte version + ^
	 userRecord = userRecord + "^" + passWord2;
	 while (userRecord.length() < 81) { userRecord = userRecord + " "; } // 42 so far + 39 for pswd
	 userRecord = userRecord + "^A0NN"; // dummy fill
	 // Can leave the rest of the record, it's not used for the change password
	 // request and is not sanity checked YET.

     // We need the command buffer to send
     StringBuffer cmdLine = new StringBuffer();
	 cmdLine.append( API_CMD_USER_NEWPSWD ).append( userRecord );

     // Show the header banner now in case there are errors to report
     out.println( "<H1>Responses to user delete request</H1>" );

     // how many servers are we sending this to ?, send it
     if (localHostName.equals("All Servers")) {
        numHosts = socketPool.getHostMax();
        for (i = 0; i < numHosts; i++ ) {
            zz = socketPool.setCommand( socketPool.getHostName(i), session.getId(), cmdLine,
                                       userPool.getUserName(session.getId()),
                                       userPool.getUserPassword(session.getId()));
            if (zz != 0) { // error
               out.println( "<span style=\"background-color: #FF0000\">" + socketPool.getHostName(i) + ":**Error** " +
                            getCmdrespcodeAsString(zz) + "</span><br>");
               out.println( "<br>");
            }
        } // for Numhosts
     } else { // a single server whos name we know
         zz = socketPool.setCommand( localHostName, session.getId(), cmdLine,
                                    userPool.getUserName(session.getId()),
                                    userPool.getUserPassword(session.getId()));
         if (zz != 0) { // error
            out.println( "<span style=\"background-color: #FF0000\">" + localHostName + ":**Error** " +
                         getCmdrespcodeAsString(zz) + "</span><br>");
            out.println( "<br>");
         }
     } // a single server

     // Now read all the responses we expect
     if (localHostName.equals("All Servers")) {
        numHosts = socketPool.getHostMax();
        for (i = 0; i < numHosts; i++ ) {
           out.println( "<h3>" + socketPool.getHostName(i) + "</h3>" );
           dataLine = socketPool.getResponseCode( socketPool.getHostName(i), session.getId() );
           if ( (dataLine.equals("100")) || (dataLine.equals("+++")) ) {     // response display text to display or non API data
              dataLine = socketPool.getResponseLine( socketPool.getHostName(i), session.getId() );
              while (dataLine != "") {
                 out.println( dataLine );
                 dataLine = socketPool.getResponseLine( socketPool.getHostName(i), session.getId());
              }
           } else {
              out.println( "<span style=\"background-color: #FF0000\">:**Error** " );
              if (dataLine.equals("101")) {     // raw data returned (binary, not display data)
                 out.println( "Respcode=101, raw data returned from server.  This is unexpected. Fix your code" );
              }
              else if (dataLine.equals("102")) {     // error text to display
                 dataLine = socketPool.getResponseLine( socketPool.getHostName(i), session.getId() );
                 while (dataLine != "") {
                    out.println( dataLine );
                    dataLine = socketPool.getResponseLine( socketPool.getHostName(i), session.getId());
                 }
              }
              else if (dataLine.equals("---")) { // no matching buffer
                 out.println( "There is no longer a data buffer associated with your session" );
              } else {
                 out.println( "Unrecognised response code (" + dataLine + ") contact support staff" );
              }
              out.println( "</span><br>");
           }
        } // for Numhosts
     } else { // a single server whos name we know
           out.println( "<h3>" + localHostName + "</h3>" );
           dataLine = socketPool.getResponseCode( localHostName, session.getId() );
           if ( (dataLine.equals("100")) || (dataLine.equals("+++")) ) {     // response display text to display or non API data
              dataLine = socketPool.getResponseLine( localHostName, session.getId() );
              while (dataLine != "") {
                 out.println( dataLine );
                 dataLine = socketPool.getResponseLine( localHostName, session.getId());
              }
           } else {
              out.println( "<span style=\"background-color: #FF0000\">:**Error** " );
              if (dataLine.equals("101")) {     // raw data returned (binary, not display data)
                 out.println( "Respcode=101, raw data returned from server.  This is unexpected. Fix your code" );
              }
              else if (dataLine.equals("102")) {     // error text to display
                 dataLine = socketPool.getResponseLine( localHostName, session.getId() );
                 while (dataLine != "") {
                    out.println( dataLine );
                    dataLine = socketPool.getResponseLine( localHostName, session.getId());
                 }
              }
              else if (dataLine.equals("---")) { // no matching buffer
                 out.println( "There is no longer a data buffer associated with your session" );
              } else {
                 out.println( "Unrecognised response code (" + dataLine + ") contact support staff" );
              }
              out.println( "</span><br>");
           }
     } // a single server
   } // else doPromt was != 1
%>
