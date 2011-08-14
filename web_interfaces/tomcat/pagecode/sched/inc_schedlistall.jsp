<%--
  inc_schedlistall.jsp

  Code to display all jobs the the active scheduler queue for all servers.
  It will colour code each job line depending upon what state the job is in.
  It adds an action button against each job, so users can perform actions
  agains a job for a server.
--%>
<%
   int zz, doDisplay, isAlert, numHosts, suppressButton;
   String hostname, hostname2;
   String dataLine;
   String testResponse = "";
   String jobName;
   String spanHeader = "";
   int i;
   StringBuffer yy = new StringBuffer();
   yy.append( API_CMD_SCHED_LISTALL );

   hostname2 = ""; // to stop the variable may not have been initialised failure
   out.println("<H2>Jobs on the scheduler active queue</H2>");

   // Include the page refresh code block
%>
<%@ include file="../../common/page_refresh_code.jsp" %>
<%   
   out.println( "<b><pre> Hostname              Job Name                       Job Status</pre></b>" );
   out.println( "<table border=\"1\" cellpadding=\"5\"><tr><td><pre>" );

   numHosts = socketPool.getHostMax();
   for (i = 0; i < numHosts; i++ ) {
      hostname = socketPool.getHostName(i);
      zz = socketPool.setCommand( hostname, session.getId(), yy,
                                  userPool.getUserName(session.getId()),
                                  userPool.getUserPassword(session.getId()) );
      if (zz != 0) {     // error
         out.println( "<span style=\"background-color: #FF0000\">" + hostname2 + ":**Error** " + getCmdrespcodeAsString(zz) + "</span><br>" );
         out.println( "<br>");
      }
   } // for Numhosts

   // Now read the responses from each server, we assume the first has had time
   // to reply by now.
   for (i = 0; i < numHosts; i++ ) {
      hostname = socketPool.getHostName(i);
	  hostname2 = hostname;
	  while ( hostname2.length() < 20 ) { hostname2 = hostname2 + " "; }
      // Check the response code
      dataLine = socketPool.getResponseCode( hostname, session.getId() );
      if ( (dataLine.equals("100")) || (dataLine.equals("+++")) ) {     // response display text to display or non API data
         dataLine = socketPool.getResponseLine( hostname, session.getId() );
         while ( (dataLine != "") && (dataLine.length() >= 39)) {
            doDisplay = 1;
            suppressButton = 0;
            jobName = dataLine.substring( 0, 30 );
            isAlert = 0;
            // check the job state so we know what span colour to use for the line
            testResponse = dataLine.substring( 31, 38 );
            if (testResponse.equals("DEPENDE")) {
               spanHeader = "<a style=\"cursor: hand\" title=\"job is waiting on dependencies\"><span style=\"background-color: #CCFFCC\">";
            }
            else if (testResponse.equals("SCHEDUL")) {
               // Check for the special NULL job category
               testResponse = dataLine.substring( 0, 4 );
               if (testResponse.equals("NULL")) {
                  spanHeader = "<a style=\"cursor: hand\" title=\"job is waiting for its time to run, will need exclusive access when it runs\"><span style=\"background-color: #CCFFFF\">";
               } else {
                  spanHeader = "<a style=\"cursor: hand\" title=\"job is waiting for its time to run\"><span style=\"background-color: #CCFFCC\">";
               }
            }
            else if (testResponse.equals("COMPLET")) {
               spanHeader = "<a style=\"cursor: hand\" title=\"job completed\"><span style=\"background-color: #CCFFCC\">";
            }
            else if (testResponse.equals("EXECUTI")) {
               spanHeader = "<a style=\"cursor: hand\" title=\"job is currently running\"><span style=\"background-color: #FFFFCC\">";
               suppressButton = 1;
            }
            else if (testResponse.equals("FAILED:")) {
               spanHeader = "<a style=\"cursor: hand\" title=\"job has failed, investigate\"><span style=\"background-color: #FF0000\">";
               isAlert = 1;
            }
            else if (testResponse.equals("CORRUPT")) {
               spanHeader = "<a style=\"cursor: hand\" title=\"job has a corrupt active entry, page support\"><span style=\"background-color: #FF0000\">";
            }
            else if (testResponse.equals("HOLD IS")) {
               spanHeader = "<a style=\"cursor: hand\" title=\"job has been manually held\"><span style=\"background-color: #00FFFF\">";
            }
            else if (testResponse.equals("PENDING")) {
               spanHeader = "<a style=\"cursor: hand\" title=\"job is ready to run but has been defered due to scheduler activity\"><span style=\"background-color: #CCFFFF\">";
            }
            else if (testResponse.equals("DELETED")) {
               doDisplay = 0;
            }
            else {
               // see if a waiting event, if so write as-is
               testResponse = dataLine.substring( 4, 11 );
               if (testResponse.equals("WAITING")) {
                  out.println( dataLine );
               } else {
                  out.println( "Unkown scheduling state, " + testResponse + "<BR>" );
               }
               doDisplay = 0;
            }

            // Make the line a fixed length so all the colours and buttons line up.
            if (doDisplay == 1) {
               while (dataLine.length() < 62) { dataLine = dataLine + " "; }
               // what button(s) do we need
               if (isAlert == 0) {
                  // add normal buttons
                  if (suppressButton == 0) {
                     out.println( spanHeader + hostname2 + ": " + dataLine  + "</span></a> <a href=\"" + request.getScheme() +
                               "://" + request.getServerName() + ":8080" + request.getRequestURI() +
                               "?pagename=schedjobaction&jobname=" + jobName +
                               "&server=" + hostname + "&action=menu\"><img src=\"buttons/Button_Actions_Blue.jpg\"></a>" );
                  } else {
                     out.println( spanHeader + hostname2 + ": " + dataLine  + "</span></a>" );
                  }
               } else {
                  // add the alert button
                  out.println( spanHeader + hostname2 + ": " + dataLine  + "</span></a> <a href=\"" + request.getScheme() +
                               "://" + request.getServerName() + ":8080" + request.getRequestURI() +
                               "?pagename=alertlistall&server=" + hostname + "\"><img src=\"buttons/alerts_orange.jpg\"></a>" );
               }
            }
            dataLine = socketPool.getResponseLine( hostname, session.getId() );
         }
      }
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

   } // for numHosts reading responses
   out.println( "</pre></td></tr></table>" );
   out.println( "<br><br>End of sched listall display" );
%>
