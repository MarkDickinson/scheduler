<%--
  inc_deplistall.jsp

  List all dependencies from the job schedulers. Add command links to
  work on the dependenicies.
--%> 
<%
   int zz, i, numHosts;
   String hostname, hostname2;
   String dataLine;
   String testResponse = "";
   String jobName;
   String outLine;
   StringBuffer yy = new StringBuffer();
   yy.append( API_CMD_DEP_LISTALL );

   hostname2 = "";  // to stop may have not been initialised failures
   out.println("<H2>Jobs with dependencies</H2>");
   out.println( "<table border=\"1\" cellpadding=\"5\"><tr><td><pre>" );

   numHosts = socketPool.getHostMax();
   for (i = 0; i < numHosts; i++ ) {
      hostname = socketPool.getHostName(i);
      zz = socketPool.setCommand( hostname, session.getId(), yy,
                                  userPool.getUserName(session.getId()),
                                  userPool.getUserPassword(session.getId()) );
      if (zz != 0) {     // error
         out.println( "<span style=\"background-color: #FF0000\"> " + hostname + ":**Error** " + getCmdrespcodeAsString(zz) + "</span><br>" );
      }
   }  // end of issuing commands

   //  --- Now process the responses from each server ---
   //  hopefully the first server we wrote to has managed to get its response fully written by now.
   for (i = 0; i < numHosts; i++ ) {
      hostname = socketPool.getHostName(i);
	  hostname2 = hostname;
	  while ( hostname2.length() < 20 ) { hostname2 = hostname2 + " "; }
      // Check the response code
      dataLine = socketPool.getResponseCode( hostname, session.getId() );
      if ( (dataLine.equals("100")) || (dataLine.equals("+++")) ) {     // response display text to display or non API data
         dataLine = socketPool.getResponseLine( hostname, session.getId() );
         while (dataLine != "") {
            testResponse = dataLine.substring( 0, 3 );
            if (testResponse.equals("Job")) {
               jobName = dataLine.substring( 4, 34 );
               outLine = "<span style=\"background-color: #B1B1B1\">" + hostname2 + ": " + jobName;
               while (outLine.length() < 99) { outLine = outLine + " "; }
               if (outLine.length() > 99) { outLine = outLine.substring( 0, 99); }
               outLine = outLine + "<a href=\"" + request.getScheme() + "://" +
                      request.getServerName() + ":8080" + request.getRequestURI() +
                      "?pagename=depaction&jobname=" + jobName +
                      "&server=" + hostname + "&action=clearallforjob\">[CLEAR ALL FOR JOB]</a>             </span>";
             } else {
               testResponse = dataLine.substring( 0, 7 );
               if (testResponse.equals("    FIL")) {    // can be file or job deps
                  jobName = dataLine.substring( 10, dataLine.length() );
               } else {
                  jobName = dataLine.substring( 9, dataLine.length() );
               }
               outLine = "<span style=\"background-color: #E0E0E0\">                      " + jobName;
               while (outLine.length() < 99) { outLine = outLine + " "; }
               if (outLine.length() > 99) { outLine = outLine.substring( 0, 99); }
               outLine = outLine + "<a href=\"" + request.getScheme() + "://" +
                      request.getServerName() + ":8080" + request.getRequestURI() +
                      "?pagename=depaction&jobname=" + jobName +
                      "&server=" + hostname + "&action=clearallfordep\">[CLEAR DEPENDENCY FROM ALL JOBS]</a></span>";
            }
            out.println( outLine );  // print the line, thats a good idea
            // read the next response line in
            dataLine = socketPool.getResponseLine( hostname, session.getId() );
         } // while dataline != ""
      } // if response code was 100
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
   out.println( "<br><br>End of job dependency display" );
%>
