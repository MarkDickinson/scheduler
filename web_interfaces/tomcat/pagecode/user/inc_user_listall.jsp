<%--
   inc_user_listall.jsp
   List all users from all servers.
--%>
<%
   int zz, numHosts;
   String hostname, hostname2;
   String dataLine;
   String schedUserName;
   StringBuffer cmdLine = new StringBuffer();

   hostname2 = ""; // stop may not be initialised errors
   out.println("<H2>All known users</H2>");
   out.println( "<b><pre> Server                User Id                                Authority Description</pre></b>" );
   out.println( "<table border=\"1\" cellpadding=\"5\"><tr><td><pre>" );

   numHosts = socketPool.getHostMax();
   for (int i = 0; i < numHosts; i++ ) {
      hostname = socketPool.getHostName(i);
	  cmdLine.append( API_CMD_USER_LISTALL );
      zz = socketPool.setCommand( hostname, session.getId(), cmdLine,
                                  userPool.getUserName(session.getId()),
                                  userPool.getUserPassword(session.getId()) );
      if (zz != 0) {     // error
         out.println( "<span style=\"background-color: #FF0000\">" + hostname + ":**Error** " + getCmdrespcodeAsString(zz) + "</span><br>" );
         out.println( "<br>");
      }
   } // for numhosts

   // Now process the reponses from all servers
   for (int i = 0; i < numHosts; i++ ) {
      hostname = socketPool.getHostName(i);
	  hostname2 = hostname;
	  while ( hostname2.length() < 20 ) { hostname2 = hostname2 + " "; }
      // Check the response code
      dataLine = socketPool.getResponseCode( hostname, session.getId() );
      if ( (dataLine.equals("100")) || (dataLine.equals("+++")) ) {     // response display text to display or non API data
         dataLine = socketPool.getResponseLine( hostname, session.getId() );
         while ( (dataLine != "") && (dataLine.length() >= 31)) {
            schedUserName = dataLine.substring( 0, 39 );
            // Make the line a fixed length so all the colours and buttons line up.
            while (dataLine.length() < 86) { dataLine = dataLine + " "; }
            if (dataLine.length() > 86) { dataLine = dataLine.substring(0,86); }
            // add buttons
            out.println( hostname2 + ": " +
                         dataLine  + "<a href=\"" + request.getScheme() +
                         "://" + request.getServerName() + ":8080" + request.getRequestURI() +
                         "?pagename=userinfo&username=" + schedUserName +
                         "\">[ INFO ]</a> " );
            dataLine = socketPool.getResponseLine( hostname, session.getId() );
         } // while dataline != ""
      } // if response code = 100
      else {
         out.println( "<span style=\"background-color: #FF0000\">" + hostname + ":**Error** " );
         if (dataLine.equals("101")) {     // raw data returned (binary, not display data)
            out.println( "Respcode=101, raw data returned from server. This is unexpected. Fix your code" );
         }
         else if (dataLine.equals("102")) {     // error text to display
            dataLine = socketPool.getResponseLine( hostname, session.getId() );
            while (dataLine != "") {
               out.println( dataLine );
               dataLine = socketPool.getResponseLine( hostname, session.getId() );
            }
         }
         else if (dataLine.equals("---")) {                                   // no matching buffer
            out.println( hostname + ":There is no longer a data buffer associated with your session" );
         }
         else {
            out.println( hostname + ":Unrecognised response code (" + dataLine + ") contact support staff" );
         }
         out.println( "</span><br>");
      } // not display data
   } // for numHosts
   out.println( "</pre></td></tr></table>" );
   out.println( "End of user listall display" );
%>
