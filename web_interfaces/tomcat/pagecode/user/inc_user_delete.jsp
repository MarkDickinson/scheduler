<%--
  inc_user_delete.jsp

  Called for a user delete request.
  If we are called from the user info display screen we already
  have a username to use, so we use that.
  If we are called from the menu we will have no user name
  so must provide an input box for it.

  If we are called with the "doit=Delete the user" we have been called by
  ourself and have all the needed parameters, so just doit.
--%>
<%
   int doPrompt = 1; // default is we want input
   String testDoIt = request.getParameter("doit");
   if (testDoIt == null) { testDoIt = "No"; }
   if (testDoIt.equals( "Delete the user" )) { doPrompt = 0; }

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

   //
   // If we are required to prompt for user input do so, then call
   // ourself back when we have it.
   //
   if (doPrompt == 1) {
      out.println("<form action=\"" + request.getScheme() + "://" + request.getServerName() + ":8080" +
                  request.getRequestURI() + "\" method=\"post\">");
      if (schedUserName.length() < 1) {
         out.println( "<h2>Deleting a user, which one ?</h2>" );
         out.println("<INPUT TYPE=HIDDEN NAME=\"pagename\" VALUE=\"userdel\"></INPUT>");
         out.println("Enter user to be deleted : <INPUT TYPE=TEXT WIDTH=39 NAME=\"username\"></INPUT><br><br>");
      } else {
         out.println( "<h2>Deleting user " + schedUserName + "</h2>" );
         out.println("<INPUT TYPE=HIDDEN NAME=\"pagename\" VALUE=\"userdel\">");
         out.println("<INPUT TYPE=HIDDEN NAME=\"username\" VALUE=\"" + schedUserName + "\">");
      }
      // Build up a list of servers we know about, the user can select one (or
      // all) to issue the job add command to.
      out.println("<b>Select the server to delete the user from</b>: <SELECT NAME=\"server\"><OPTION>All Servers</OPTION>");
      for (int i = 0; i < socketPool.getHostMax(); i++) {
         out.println("<OPTION>" + socketPool.getHostName(i) + "</OPTION>");
      }
      out.println("</SELECT><BR>You must have security authority for user management");
      out.println("<br><br><input type=submit name=doit value=\"Delete the user\"> </form><br><br>" );
   }
   //
   // else we have all the input, we need to delete the user record
   //
   else {
     int numHosts, i, zz;
	 String dataLine;
     // Need to create the user record buffer. Keep this seperate as
	 // later on if the server ever gets sanity checking for this specific
	 // command we will need to create the entire record structure.
	 String userRecord = "01^" + schedUserName + "^"; // leave the rest, it's not sanity checked YET

     // We need the command buffer to send
     StringBuffer cmdLine = new StringBuffer();
	 cmdLine.append( API_CMD_USER_DELETE ).append( userRecord );

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
   }
%>
