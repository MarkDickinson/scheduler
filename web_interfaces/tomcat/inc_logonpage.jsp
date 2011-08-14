<%--
  Get the user and password details, then try to logon to each server we know
  about. We will display a logon/fail list of server connections to the user
  so they know what has happened.
--%>
<%
   int zz, loggedOn, maxpoolCount, minAuth, needExplain;
   String hostname = "";
   String dataLine = "";
   String userPswd = request.getParameter("password");
   // userName variable is created in the main page, just set it here.
   userName = request.getParameter("username"); // This is created by the caller SCHEDPoolPage.jsp
   needExplain = 0;
   zz = 1; // stop not initialised errors

   // If no username, prompt for one with the logon request data form
   if ( (userName == null) || userName.equals("")) {
      out.println("<br><br><center><H1>LOGIN REQUIRED</H1>");
      out.println("<FORM NAME=\"loginform\" METHOD=\"post\">");   // ACTION= not reqd, same page does the work
      out.println("<TABLE BGCOLOR=\"#FFFF80\" BORDER=\"0\" CELLPADDING=\"5\" CELLSPACING=\"0\">");
      out.println("<TR>");
      out.println("<TD ALIGN=\"right\">Login Name:</TD>");
      out.println("<TD ALIGN=\"left\"><INPUT TYPE=\"text\" NAME=\"username\" SIZE=\"10\"></TD>");
      out.println("</TR>");
      out.println("<TR>");
      out.println("<TD ALIGN=\"right\">Password:</TD>");
      out.println("<TD ALIGN=\"left\"><INPUT TYPE=\"password\" NAME=\"password\" SIZE=\"10\"></TD>");
      out.println("</TR>");
      out.println("<TR><TD COLSPAN=\"2\" ALIGN=\"CENTER\"><INPUT TYPE=\"submit\" VALUE=\"Login\"></TD></TR>");
      out.println("</TABLE>");
      out.println("<p>Enter your username and password.<br>You will be logged onto all servers you");
      out.println("have login rights for.</p></center>" );
   }

   // else, we are processing a logon request data form
   else {
      if (saveRequest.equals("logon")) {
         saveRequest = "defaultblank";
      }
      // check again, somehow a null username has been getting here (past the check above ?)
      if ((userName == null) || (userPswd == null)) {
         out.println( "<H1>Logon not attempted</H1><p>Either the username or password fields were not provided.</p>" );
         saveRequest = "logon";
      } else {
         zz = userPool.allocateUserEntry( session.getId(), userName, userPswd );
         if (zz >= userPool.getUserCount() ) {
            // no free user entries, deny login
            out.println( "<H1>Logon failed, Maximum sessions already in use</H1><p>The maximum number of users are already logged on, try again later</p>" );
            saveRequest = "logon";
         } else {
            out.println("<H1>Login progress status</H1><table border=\"1\"><tr><td>" );
            zz = socketPool.getHostMax();
            for (int i = 0; i < zz; i++ ) {
               hostname = socketPool.getHostName(i);
               maxpoolCount = socketPool.getSessionMax( hostname );
               loggedOn = socketPool.allocateSession( hostname, session.getId(), userName, userPswd );
               if (loggedOn < maxpoolCount) {
                  // logged on, do some processing with the line still in the read data buffer,
                  // we want to try to work out a minimum auth level so we know what menus to display for the user.
                  dataLine = socketPool.getResponseLine( hostname, session.getId() );
                  // dataLine = getTextField( dataLine, 6 ); // AUTH GRANTED: Access level now <level> other text
                  // STILL TO DO
                  // if (dataLine.equals("BROWSE-ONLY")) { then ; }
                  // if (dataLine.equals("SECURITY")) { then ; }
                  // if (dataLine.equals("JOB")) { then ; }
                  // if (dataLine.equals("OPERATOR")) { then ; }
                  // if (dataLine.equals("ADMIN")) { then ; }
                  // report the logon was OK.
                  out.println( "<span style=\"background-color: #CCFFCC\">OK</span>" + hostname + ": logged on<br>" );
               } else if (loggedOn == (maxpoolCount + 1)) {
                  // no free session pool objects
                  out.println( "<span style=\"background-color: #FF0000\">KO</span>" +
                               hostname + ": NOT logged on, no available connections to server.  I will retry later<br>" );
                  needExplain = 1;
               } else {
                  // logon failure
                  out.println( "<span style=\"background-color: #FF0000\">KO</span>" +
                               hostname + ": NOT logged on, invalid login for the server<br>" );
                  if (zz == 1) { saveRequest = "logon"; } // only one host, back to login page
                  needExplain = 2;
               }
            } // for number of hosts
            out.println( "</td></tr></table><br>End of Login progress display<br>" );
         } // neither name or pswd were null
      }  // else user entry allocated OK
      // If we failed to logon to a server due to a connection pool shortage
      // for the server, then let the user know what it means.
      if (needExplain == 1) {
         out.println( "<big><b>Your logon to a server(s) failed due to contention problems or the remote server(s) being down." );
         out.println( "It will automatically be retried as a connection becomes available.</b></big>" );
      } else if (needExplain == 2) {
          if (zz == 1) {
              out.println( "<big><b>Your logon was invalid.</b></big><br>" );
              out.println( "You will not be able to procees past the login page until you have corrected the login error.<br>" );
          } else {
              out.println( "<big><b>Your logon to one or more servers failed.</b></big><br>" );
              out.println( "You are not permitted to perform any job scheduler update operations against that server.<br>" );
              out.println( "You should contact your security department to get your login for the server corrected.<br>" );
          }
      }
      // add a click link the user can click on to leave the status page to
      // continue to their original page.
      out.println( "<br><a href=\"" + request.getScheme() + "://" + request.getServerName() + ":8080" +
                   request.getRequestURI() + "?pagename=" + saveRequest +
                   "\">[ PROCEED to requested page ]</a>" );
   } // else doing logon processing
%>
