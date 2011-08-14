<%
   int i, numHosts, zz;
   String hostname, respCode, dataLine;
   String hostname2 = "";
   StringBuffer yy = new StringBuffer();
   yy.append( API_CMD_SHOWSESSIONS );

   // how many servers do we send the command to ?
   numHosts = socketPool.getHostMax();

   // send the command to all servers
   for (i = 0; i < numHosts; i++ ) {
      hostname = socketPool.getHostName(i);
      zz = socketPool.setCommand( hostname, session.getId(), yy,
                                  userPool.getUserName(session.getId()),
                                  userPool.getUserPassword(session.getId()) );
      if (zz != 0) {     // error
         out.println( "<span style=\"background-color: #FF0000\">" + hostname2 + ":**Error** " + getCmdrespcodeAsString(zz) + "</span><br>" );
         out.println( "<br>");
      }
   } // for numHosts

   // Now read the responses, assume null reponses were handled by the error check above
   out.println("<H2>Current user sessions to the Job Scheduler</H2>");
   out.println( "<table border=\"1\" cellpadding=\"5\"><tr><td><pre>" );
   for (i = 0; i < numHosts; i++ ) {
      hostname = socketPool.getHostName(i);
	  hostname2 = hostname;
	  while ( hostname2.length() < 20 ) { hostname2 = hostname2 + " "; }
      dataLine = socketPool.getResponseLine( hostname, session.getId() );
      while (dataLine != "") {
         out.println( hostname2 + ": " + dataLine );
         dataLine = socketPool.getResponseLine( hostname, session.getId() );
      }
      out.println("<br><br>");
   }
   out.println( "</pre></td></tr></table>" );
   out.println( "<br><br>End of scheduler status displays" );
%>
