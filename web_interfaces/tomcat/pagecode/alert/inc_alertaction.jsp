<%--
  inc_alertrestart.jsp

  The page invoked when we need to issue an request against an alert. It
  will restart, force-ok or display details for the alert based upon the
  request provided.
--%>
<%
   int zz, doCmd;
   String localJobName, jobnameLen, localAction;
   StringBuffer cmdLine = new StringBuffer();

   doCmd = 1; // default is do the command
   zz = 0;    // must initialise it or get may not have been initialised errors.

   // cmd data buffer length is nine digits
   localJobName = userPool.getUserContext(session.getId(), "JOBNAME" );
   jobnameLen = "" + localJobName.length();
   while (jobnameLen.length() < 9) { jobnameLen = "0" + jobnameLen; }
   // what are we doing ?
   localAction = userPool.getUserContext(session.getId(), "ACTION" );
   if (localAction.equals("restart")) {
      out.println( "<h3>Results of restarting " + userPool.getUserContext(session.getId(), "JOBNAME" ) +
                   " on server " + userPool.getUserContext(session.getId(), "HOSTNAME" ) + "</h3>" );
      cmdLine.append(API_CMD_ALERT_RESTART);
   }
   else if (localAction.equals("forceok")) {
      out.println( "<h3>Results of Forcing OK " + userPool.getUserContext(session.getId(), "JOBNAME" ) +
                   " on server " + userPool.getUserContext(session.getId(), "HOSTNAME" ) + "</h3>" );
      cmdLine.append(API_CMD_ALERT_FORCEOK);
   }
   else if (localAction.equals("display")) {
      out.println( "<h3>Details for alert " + userPool.getUserContext(session.getId(), "JOBNAME" ) +
                   " on server " + userPool.getUserContext(session.getId(), "HOSTNAME" ) + "</h3>" );
      cmdLine.append(API_CMD_ALERT_INFO);
   }
   else {
      doCmd = 0;
%>
<h3>Error: request not actioned</h3>
<p>The action command passed to the scriptlet is not a valid command, please
contact support.</p>
<%
      out.println( "Command requested was '" + userPool.getUserContext(session.getId(), "ACTION" ) + "'" );
   }

   if (doCmd == 1) {
      cmdLine.append(jobnameLen).append("^").append(userPool.getUserContext(session.getId(), "JOBNAME"));
      zz = socketPool.setCommand( userPool.getUserContext(session.getId(), "HOSTNAME" ), session.getId(), cmdLine,
                                  userPool.getUserName(session.getId()),
                                  userPool.getUserPassword(session.getId()) );
      if (zz != 0) {     // error, no attempt to contact server or logon failure
         out.println( "<span style=\"background-color: #FF0000\">" + pageHostName + ":**Error** " + getCmdrespcodeAsString(zz) + "</span><br>" );
         out.println( "<br>");
      }
      else {  // a response from the server
%>
<%@ include file="../../common/standard_response_display.jsp" %>
<%
      } // else response
   } // if doCmd

   // Show a button to return to the alert list.
   out.println( "<center>" );
   if ( (localAction.equals("display")) && (doCmd == 1) && (zz == 0) ) { // a display and all was OK
      out.println( "<a href=\"" + request.getScheme() +
                   "://" + request.getServerName() + ":8080" + request.getRequestURI() +
                   "?pagename=alertaction&jobname=" + localJobName +
                   "&server=" + userPool.getUserContext(session.getId(), "HOSTNAME" ) +
                   "&action=restart\"><img src=\"buttons/restart_blue.jpg\"></a>" +
                   "<a href=\"" + request.getScheme() +
                   "://" + request.getServerName() + ":8080" + request.getRequestURI() +
                   "?pagename=alertaction&jobname=" + localJobName +
                   "&server=" + userPool.getUserContext(session.getId(), "HOSTNAME" ) +
                   "&action=forceok\"><img src=\"buttons/forceok_blue.jpg\"></a>" );
   }
   out.println( "<a href=\"" + request.getScheme() + "://" + request.getServerName() +
                ":8080" + request.getRequestURI() +
                "?pagename=alertlistall\"><img src=\"buttons/alerts_orange.jpg\"></a></center>" );
%>
