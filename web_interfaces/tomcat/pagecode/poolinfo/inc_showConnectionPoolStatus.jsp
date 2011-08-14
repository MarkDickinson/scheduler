<%--
   inc_showConnectionPoolStatus.jsp

   Show the status of all connection pools we have belonging to this
   application. This is an admin/support screen.
--%>
<%
   int zz, hostCount;
   String dataLine;
   String testLine;
   String hostname;
   StringBuffer yy = new StringBuffer();

   out.println("<H2>Socket connection pool status</H2>");
   out.println( "<b><pre>   Hostname                      Port   ConState        Threadstate            InUse  Session-Id</pre></b>" );
   out.println( "<table border=\"1\" cellpadding=\"5\"><tr><td><pre>" );
   hostCount = socketPool.getHostMax();
   for (int j = 0; j < hostCount; j++ ) {
         hostname = socketPool.getHostName(j);
         zz = socketPool.getSessionMax(hostname);
         for (int i = 0; i < zz; i++) {
            dataLine = socketPool.getSocketInfo( hostname, i );
            if (dataLine.length() >= 47) {
               testLine = dataLine.substring( 38, 47 );
            } else {
               testLine = "short buffer"; // will trigger error/red text
            }
            if (testLine.equals("Connected")) {
               if (dataLine.length() >= 81) { // if we have enough data, try to obtain the userid also
                  testLine = dataLine.substring( 77, 81 );
                  if (!(testLine.equals("spar"))) {
                     workUserName = userPool.getUserName( session.getId() );
                     if (workUserName.length() > 2) {
                        out.println( " " + dataLine + " (" + workUserName + ")" ); // extra byte to line up with colours, show user
                     } else {
                        out.println( " " + dataLine );   // extra byte to line up with colours, no logged on user
                     }
                  } // if not spare
                  else { // is spare, still need to print it out
                     out.println( " " + dataLine );   // extra byte to line up with colours, no logged on user
                  }
               } // if linelen >= 83
               else {  // must be a spare one, still need to display the details
                  out.println( " " + dataLine );   // extra byte to line up with colours, no logged on user
               }
            }
            else {
               out.println( "<span style=\"background-color: #FF0000\">" + dataLine + "</span>" ); // red
            }
         } // for i
   } // for j
   out.println( "</pre></td></tr></table><br>" );
   out.println( "<br>End of socket pool status display" );
%>
