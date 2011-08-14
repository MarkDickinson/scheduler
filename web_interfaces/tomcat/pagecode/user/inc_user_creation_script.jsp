<%--
  inc_user_creation_script.jsp

  Check the parameters provided for the new user. If all are OK
  then sumbit the add user request to the selected, or to all servers
  as appropriate; and display the reponse to the user.

  This may look a little messy, it needs to build up a C user record
  format. The record format in C (as copied from the server header 
  code, minus comments) is below.

  Imporant notes: the server interface (cunningly designed) expects
  all null characters to be set to "^" at the transmitting end, it 
  will parse them back to nulls when it gets the command buffer. This
  allows us to use "^" to terminate the end of field values here as well
  as the null at max field length without upsetting any string handling
  by requiring the use of real null characters.

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
  String testLine, testChar, localHostName, userListLine, dataLine;
  StringBuffer cmdLine = new StringBuffer();
  StringBuffer userRecord = new StringBuffer();
  StringBuffer errorList = new StringBuffer();
  int zz, doCmd, i, numHosts;

  doCmd = 1; // default is all OK so far
 
  // check we have all the required parameters, buiding the command as we go.
  // 1. The user name is required
  testLine = request.getParameter("username");
  if ((testLine == null) || (testLine.length() == 0)) {
     doCmd = 0;
     errorList.append( "Missing required field: User Name<br>" );
  } else {
     while (testLine.length() < 39) { testLine = testLine + " "; }
     if (testLine.length() > 39) { testLine = testLine.substring(0,39); }
     userRecord.append( "01^" ).append( testLine ).append( "^" );
  }
  // 2. The user initial password, and active flag
  testLine = request.getParameter("password1");
  if ((testLine == null) || (testLine.length() == 0)) {
     doCmd = 0;
     errorList.append( "Missing required field: Users initial password<br>" );
  } else {
     while (testLine.length() < 39) { testLine = testLine + " "; }
     if (testLine.length() > 39) { testLine = testLine.substring(0,39); }
     userRecord.append( testLine ).append( "^A" );
  }
  // 3. The user authority class
  testChar = "0"; // default is read-only access if we have a logic check error
                  // we have to init to avoid var not initialised errors, so use 0
  testLine = request.getParameter("authlevel");
  if ((testLine == null) || (testLine.length() == 0)) {
     doCmd = 0;
     errorList.append( "Missing required field: User authority class<br>" );
  } else {
     testChar = testLine.substring(0,1);
	 if ( (testChar.equals("0")) || (testChar.equals("O")) || (testChar.equals("J")) ||
          (testChar.equals("S")) || (testChar.equals("A"))
        ) {
        userRecord.append( testChar );
     } else {
        doCmd = 0;
        errorList.append( "Invalid value for field User authority class, vale was " ).append( testLine ).append("<br>");
     }
  }
  // 4. Is the user allowed local auto-auth authority
  testLine = request.getParameter("autoauth");
  if ((testLine == null) || (testLine.length() == 0)) {
     doCmd = 0;
     errorList.append( "Missing required field: User auto login authority<br>" );
  } else {
     if (testLine.equals("yes")) {
        userRecord.append("Y");
     } else {
        userRecord.append("N");
     }
  }
  // 5. Allowed to add users ?
  testLine = request.getParameter("useraddauth");
  if ((testLine == null) || (testLine.length() == 0)) {
     doCmd = 0;
     errorList.append( "Missing required field: Users user add authority setting<br>" );
  } else {
     if (testChar.equals("S")) { testLine = "security"; } // security auth users must have security level user management
     if (testLine.equals("yes")) {
        userRecord.append("Y");
     } else if (testLine.equals("security")) {
        userRecord.append("S");
     } else {
        userRecord.append("N");
     }
  }
  // 6. The user list users can add jobs to, there can be up to 10 entries;
  //    having no entries is also legal.
  userListLine = "";
  testLine = request.getParameter("unixuser1");
  if ((testLine != null) && (testLine.length() > 0)) {
     testLine = testLine + "^"; // terminate it, required by server
     while (testLine.length() < 39) { testLine = testLine + " "; }
     if (testLine.length() > 39) { testLine = testLine.substring(0,39); }
	 userListLine = userListLine + testLine + "^";
  }
  testLine = request.getParameter("unixuser2");
  if ((testLine != null) && (testLine.length() > 0)) {
     testLine = testLine + "^"; // terminate it, required by server
     while (testLine.length() < 39) { testLine = testLine + " "; }
     if (testLine.length() > 39) { testLine = testLine.substring(0,39); }
	 userListLine = userListLine + testLine + "^";
  }
  testLine = request.getParameter("unixuser3");
  if ((testLine != null) && (testLine.length() > 0)) {
     testLine = testLine + "^"; // terminate it, required by server
     while (testLine.length() < 39) { testLine = testLine + " "; }
     if (testLine.length() > 39) { testLine = testLine.substring(0,39); }
	 userListLine = userListLine + testLine + "^";
  }
  testLine = request.getParameter("unixuser4");
  if ((testLine != null) && (testLine.length() > 0)) {
     testLine = testLine + "^"; // terminate it, required by server
     while (testLine.length() < 39) { testLine = testLine + " "; }
     if (testLine.length() > 39) { testLine = testLine.substring(0,39); }
	 userListLine = userListLine + testLine + "^";
  }
  testLine = request.getParameter("unixuser5");
  if ((testLine != null) && (testLine.length() > 0)) {
     testLine = testLine + "^"; // terminate it, required by server
     while (testLine.length() < 39) { testLine = testLine + " "; }
     if (testLine.length() > 39) { testLine = testLine.substring(0,39); }
	 userListLine = userListLine + testLine + "^";
  }
  testLine = request.getParameter("unixuser6");
  if ((testLine != null) && (testLine.length() > 0)) {
     testLine = testLine + "^"; // terminate it, required by server
     while (testLine.length() < 39) { testLine = testLine + " "; }
     if (testLine.length() > 39) { testLine = testLine.substring(0,39); }
	 userListLine = userListLine + testLine + "^";
  }
  testLine = request.getParameter("unixuser7");
  if ((testLine != null) && (testLine.length() > 0)) {
     testLine = testLine + "^"; // terminate it, required by server
     while (testLine.length() < 39) { testLine = testLine + " "; }
     if (testLine.length() > 39) { testLine = testLine.substring(0,39); }
	 userListLine = userListLine + testLine + "^";
  }
  testLine = request.getParameter("unixuser8");
  if ((testLine != null) && (testLine.length() > 0)) {
     testLine = testLine + "^"; // terminate it, required by server
     while (testLine.length() < 39) { testLine = testLine + " "; }
     if (testLine.length() > 39) { testLine = testLine.substring(0,39); }
	 userListLine = userListLine + testLine + "^";
  }
  testLine = request.getParameter("unixuser9");
  if ((testLine != null) && (testLine.length() > 0)) {
     testLine = testLine + "^"; // terminate it, required by server
     while (testLine.length() < 39) { testLine = testLine + " "; }
     if (testLine.length() > 39) { testLine = testLine.substring(0,39); }
	 userListLine = userListLine + testLine + "^";
  }
  testLine = request.getParameter("unixuser10");
  if ((testLine != null) && (testLine.length() > 0)) {
     testLine = testLine + "^"; // terminate it, required by server
     while (testLine.length() < 39) { testLine = testLine + " "; }
     if (testLine.length() > 39) { testLine = testLine.substring(0,39); }
	 userListLine = userListLine + testLine + "^";
  }
  // need to pad out if we didn't get ten entries
  for (i = 0; i < 10; i++) {
     zz = (i * 40) + 39;
     if (userListLine.length() < zz) {
       userListLine = userListLine + "^";  // null field in first byte of all empty fields required by server
       while (userListLine.length() < zz) { userListLine = userListLine + " "; }
       userListLine = userListLine + "^";
     }
  }
  userRecord.append( userListLine );
  // 7. The user details/description field, 49 bytes plus pad
  testLine = request.getParameter("userdetails");
  if ((testLine == null) || (testLine.length() == 0)) {
     doCmd = 0;
     errorList.append( "Missing required field: User details/description<br>" );
  } else {
     while (testLine.length() < 49) { testLine = testLine + " "; }
     if (testLine.length() > 49) { testLine = testLine.substring(0,49); }
     userRecord.append( testLine ).append( "^" );
  }
  // 8. Dummy fill the last loggedon and last password change fields
  userRecord.append( "00000000 00:00:00^00000000 00:00:00^" );

  // OK, if no errors we now have a useable user record
  localHostName = request.getParameter("server");
  if ((localHostName == null) || (localHostName.length() < 1)) {
     doCmd = 0;
     errorList.append( "Missing parameter     : No hostname was selected from the list<br>" );
  }

  // NOW, DID WE FIND ANY ERRORS ?
  //  If yes, just show the errors, otherwise we need to determine how many
  //  servers to send the command to, and send it.
  if (doCmd == 0) {
     out.println( "<h3>Errors prevented the user being added</h3>" );
     out.println( "<table border=\"1\" BGCOLOR=\"#FF0000\"><tr><td>" );
     out.println( errorList.toString() );
     out.println( "</td></tr></table>" );
  }
  
 // ELSE NO ERRORS, send the command to the server or servers
  else {
	 cmdLine.append( API_CMD_USER_ADD ).append( userRecord.toString() );
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
     out.println( "<H1>Responses to user add request</H1>" );
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

} // else doCmd permitted
%>
