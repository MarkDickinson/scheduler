<%--
  inc_jobaction.jsp

  The page invoked when we need to issue an action or query against
  a job in the job definition databases. It works against a specific
  server only.
  Notes: no longer does a job details display, this has been moved to
  the inc_job_display.jsp script which is now called from the joblistall
  action menu instead of this, it does the job display always and then
  provides the two action buttons that will link to this.
--%>
<%
   int zz, doCmd;
   String localJobName, jobnameLen, localAction;
   StringBuffer cmdLine = new StringBuffer();

   doCmd = 1; // default is do the command

   // cmd data buffer length is nine digits
   localJobName = userPool.getUserContext(session.getId(), "JOBNAME" );
   jobnameLen = "" + localJobName.length();
   while (jobnameLen.length() < 9) { jobnameLen = "0" + jobnameLen; }
   // what are we doing ?
   localAction = userPool.getUserContext(session.getId(), "ACTION" );
   if (localAction.equals("submit")) {
      out.println( "<h3>Results of scheduling on " + userPool.getUserContext(session.getId(), "JOBNAME" ) +
                   " on server " + userPool.getUserContext(session.getId(), "HOSTNAME" ) + "</h3>" +
                   "<table border=\"1\"><tr><td>" );
      cmdLine.append(API_CMD_JOB_SUBMIT);
   }
   else if (localAction.equals("delete")) {
      out.println( "<h3>Results of trying to delete " + userPool.getUserContext(session.getId(), "JOBNAME" ) +
                   " on server " + userPool.getUserContext(session.getId(), "HOSTNAME" ) + "</h3>" +
                   "<table border=\"1\"><tr><td>" );
      cmdLine.append(API_CMD_JOB_DELETE);
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

   // Show a button to return to the job list.
   out.println( "<center><a href=\"" + request.getScheme() + "://" + request.getServerName() +
                ":8080" + request.getRequestURI() +
                "?pagename=joblistall\">[BACK TO JOB LIST]</a></center>" );
%>
