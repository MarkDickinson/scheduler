<%--
  inc_job_display.jsp

  Display the details for a job, and offer the user the option of
  submitting or deleting the job.
--%>
<%
   int zz;
   String actionType, baseURI;
   String localjobName, localjobNameLen;
   StringBuffer cmdLine = new StringBuffer();

   out.println( "<h3>Job details for " +
                 userPool.getUserContext(session.getId(), "JOBNAME" ) + " for server " +
                 userPool.getUserContext(session.getId(), "HOSTNAME" ) + "</h3><br>" );

   localjobName = userPool.getUserContext(session.getId(), "JOBNAME" );
   if ((localjobName == null) || (localjobName.length() < 2)) {
       out.println( "<span style=\"background-color: #FF0000\">**Error** no legal Jobname provided to script</span><br>" );
   } else {
      cmdLine.append( API_CMD_JOB_INFO );
      localjobNameLen = "" + localjobName.length();
      while (localjobNameLen.length() < 9) { localjobNameLen = "0" + localjobNameLen; }
      cmdLine = cmdLine.append( localjobNameLen ).append( "^" ).append( userPool.getUserContext(session.getId(),"JOBNAME") );
      zz = socketPool.setCommand( userPool.getUserContext(session.getId(), "HOSTNAME" ), session.getId(), cmdLine,
                                  userPool.getUserName(session.getId()),
                                  userPool.getUserPassword(session.getId()) );
      if (zz != 0) {     // error, no attempt to contact server or logon failure
         out.println( "<span style=\"background-color: #FF0000\">" + pageHostName + ":**Error** " + getCmdrespcodeAsString(zz) + "</span><br>" );
         out.println( "<br>Code = " + zz + ", hostname = " + userPool.getUserContext(session.getId(), "HOSTNAME" ) + "<br>");
      }
      else {  // a response from the server
%>
<%@ include file="../../common/standard_response_display.jsp" %>
<%
         // Show  the possible action buttons
         out.println(  "<center><a href=\"" + request.getScheme() + "://" +
                      request.getServerName() + ":8080" + request.getRequestURI() +
                      "?pagename=jobaction&jobname=" + localjobName + "&server=" +
                      userPool.getUserContext(session.getId(), "HOSTNAME" ) +
                      "&action=submit\"><img src=\"buttons/submit_to_activeQ.jpg\"></a> " +
                      "<a href=\"" + request.getScheme() + "://" +
                      request.getServerName() + ":8080" + request.getRequestURI() + "?pagename=alertaction&jobname=" +
                      localjobName + "&server=" +
                      userPool.getUserContext(session.getId(), "HOSTNAME" ) +
                      "&action=delete\"><img src=\"buttons/delete_orange.jpg\"></a></center>");
      }  // if a response from the server
   } // else good jobname
%>
