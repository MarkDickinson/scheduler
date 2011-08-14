<%--
  inc_schedjobaction.jsp

  The page invoked when we need to issue an command against a job on the active
  scheduler queue.
  We use the user context variables to decide what we are doing as this could
  be a destructive operation for a scheduled job so we need to make absolutely
  sure we don't accidentally pickup the command any other userin the main
  thread may be issuing.

  On entry the user context variables are expected to contain the hostname the
  command is for, the jobname the command is for, and the command name which
  may be one of menu/holdon/holdoff/runnow/delete.
--%>
<%
   int zz, issueCommand;
   String actionType, baseURI;
   String localjobName, localjobNameLen;
   StringBuffer cmdLine = new StringBuffer();

   issueCommand = 1; // issue by default

   localjobName = userPool.getUserContext(session.getId(), "JOBNAME" );
   if ((localjobName == null) || (localjobName.length() < 2)) {
       issueCommand = 0; // no jobname, nothing to issue, AND an error, should not happen.
   }

   actionType = userPool.getUserContext(session.getId(), "ACTION" );
   if ((actionType == null) || (actionType.length() < 4)) { actionType = "menu"; }
   if (actionType.equals("menu")) {
       issueCommand = 0; // nothing to issue, the user has selected a choice
                         // but we need to go back via the main page to setup the context
                         // environment for the choice.
       out.println( "<h2>Jobname: " + userPool.getUserContext(session.getId(), "JOBNAME" ) + "</h2>" );
       out.println( "<h2>Server Host: " + userPool.getUserContext(session.getId(), "HOSTNAME" ) + "</h2>" );
       out.println("<p>Select the command to be issued for this job. <b>Check the hostname and" +
                   "jobname are correct</b> before issuing the command.</p><ul>" );
       baseURI = request.getScheme() + "://" + request.getServerName() +
                 ":8080" + request.getRequestURI() +
                 "?pagename=schedjobaction&server=" +
                 userPool.getUserContext(session.getId(), "HOSTNAME" ) + 
                 "&jobname=" + userPool.getUserContext(session.getId(), "JOBNAME" ) + 
                 "&action=";
       out.println( "<li><a href=\"" + baseURI + "holdon\">Hold job " +
                    userPool.getUserContext(session.getId(), "JOBNAME" ) + 
                    "</a> will place the job into a hold state requiring manual release</li>" );
       out.println( "<li><a href=\"" + baseURI + "holdoff\">Release job " +
                    userPool.getUserContext(session.getId(), "JOBNAME" ) + 
                    "</a> will release the job if it was previously held</li>" );
       out.println( "<li><a href=\"" + baseURI + "runnow\">Run job " +
                    userPool.getUserContext(session.getId(), "JOBNAME" ) + 
                    "</a> <b>now</b> will run the job immediately <em>as long as there are " +
                    "no outstanding dependencies remaining</em> for the job</li>" );
       out.println( "<li><a href=\"" + baseURI + "display\">Display job " +
                    userPool.getUserContext(session.getId(), "JOBNAME" ) + 
                    "</a> details from the offline job definition file</li>" );
       out.println( "<li><a href=\"" + baseURI + "delete\">Delete job " +
                    userPool.getUserContext(session.getId(), "JOBNAME" ) + 
                    "</a> from the active scheduler queue. <b>See the notes on deleting a job " +
                    "from the active queue below</b>.</li></ul>" );
%>
<h3>Notes on deleting a job</h3>
<p>If you delete the job from the active queue all jobs that have dependencies
upon the job being deleted will have those dependencies cleared. The
dependencies will not be re-added if you resubmit the job to the active
queue.</p>
<p>The job will be deleted from the active job scheduler queue on the server,
it will not be deleted from the job database, and in the job database it will
still be an outstanding job. You will need to wither manually resubmit the job
when you have fixed the problem rewquiring it deleted; or delete it from the
job database which will remove it forever, it will never run again.</p>
<p>If you manually resubmit the job at a later time, and it has dependencies
upon jobs that have already completed running then those depndenciews will
never be automatically satified, you will have to clear them manually.</p>
<%
   } else if (actionType.equals("holdon")) {
       cmdLine.append( API_CMD_SCHED_HOLD );
   } else if (actionType.equals("holdoff")) {
       cmdLine.append( API_CMD_SCHED_RELEASE );
   } else if (actionType.equals("runnow")) {
       cmdLine.append( API_CMD_SCHED_RUNNOW );
   } else if (actionType.equals("delete")) {
       cmdLine.append( API_CMD_SCHED_DELETE );
   } else if (actionType.equals("display")) {
       cmdLine.append( API_CMD_JOB_INFO );
   } else { // unknown
       issueCommand = 0; // nothing to issue
       out.println( "This page was called without required parameters, try using the menu system !" );
       out.println( "Nothing to do ... !" );
   }

   // only issue the command if we need to do so.
   if (issueCommand == 1) {
      localjobNameLen = "" + localjobName.length();
      while (localjobNameLen.length() < 9) { localjobNameLen = "0" + localjobNameLen; }
      cmdLine = cmdLine.append( localjobNameLen ).append( "^" ).append( userPool.getUserContext(session.getId(),"JOBNAME") );
      zz = socketPool.setCommand( userPool.getUserContext(session.getId(), "HOSTNAME" ), session.getId(), cmdLine,
                                  userPool.getUserName(session.getId()),
                                  userPool.getUserPassword(session.getId()) );
      out.println( "<h3>Results of command " + userPool.getUserContext(session.getId(), "ACTION" ) +
                   " " + userPool.getUserContext(session.getId(), "JOBNAME" ) + " for server " +
                   userPool.getUserContext(session.getId(), "HOSTNAME" ) + "</h3><br>" );
      if (zz != 0) {     // error, no attempt to contact server or logon failure
         out.println( "<span style=\"background-color: #FF0000\">" + pageHostName + ":**Error** " + getCmdrespcodeAsString(zz) + "</span><br>" );
         out.println( "<br>Code = " + zz + ", hostname = " + userPool.getUserContext(session.getId(), "HOSTNAME" ) + "<br>");
      }
      else {  // a response from the server
%>
<%@ include file="../../common/standard_response_display.jsp" %>
<%
         // Show a button to return to the job list.
         out.println( "<center><a href=\"" + request.getScheme() + "://" + request.getServerName() +
                      ":8080" + request.getRequestURI() +
                      "?pagename=schedlistall\">[BACK TO SCHEDULER JOB DISPLAY PAGE]</a></center>" );
      }  // if a response from the server
   } // if issueCommand
%>
