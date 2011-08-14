<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Final//EN">
<%-- 
  SCHEDPoolPage.jsp

  JSP web interface to "Mark Dickinsons Job Scheduler", provides a secure
  web interface in that scheduler commands are issued under the userid of the
  user running the browser session.

  Imbed the code from the java connection pool interface, and use this page
  for all requests to the job scheduler server.
  The appropriate function page will be displayed depening on the command
  in the user query string, by including the appropriate html display code
  for the page based on the query string.
--%>
<%-- <%@ page errorPage = "error_page.jsp" %> --%>
<%@ page import="java.net.*" %>
<%@ page import="java.io.*" %>
<%@ page import="java.util.Timer" %>
<%@ page import="java.util.TimerTask" %>
<%@ page import="java.util.Date" %>
<%!
/* =================================================================================
  SCHEDPoolPage.java

  A jsp page that allows full management of the job scheduler application(s).
  Uses a pool of permanently connected sessions to the server being managed
  to avoid the overheads of the previous cgi interfaces that had to start a 
  new interface each request.
  Allows for job scheduler user security as with a permanent pool each session
  can belong to a known user tracked via the tomcat session id which was just
  not possible via the old apache cgi method.

  Version History
  0.001-BETA 20Jan2004 MID Created initial java class.

  ==================================================================================
*/
%>
<%@ include file="object_classes/object_SCHEDPoolBean.jsp" %>
<%@ include file="object_classes/object_SCHEDPoolWrapper.jsp" %>
<%@ include file="object_classes/object_USERClass.jsp" %>

<%@ include file="common/constants.jsp" %>

<%! 
// -------------------------------------------------------------------------
//       Constructors and Destructors for the page objects.
// -------------------------------------------------------------------------
  SCHEDPoolWrapper socketPool;
  USERClass userPool;

  public void jspInit() {
     int i;
	 String hostname;
     System.out.println( "Scheduler: ----------------------- Initialising --------------------" );
     socketPool = new SCHEDPoolWrapper();
	 System.out.println( "Scheduler: jspInit: Connection pool initialised, " + socketPool.getHostMax() + " hosts in pool" );
	 for (i = 0; i < socketPool.getHostMax(); i++ ) {
	    hostname = socketPool.getHostName(i);
	    System.out.println( "Scheduler: jspInit: " + hostname + " connection pool , " + socketPool.getSessionMax(hostname) + " sessions in pool" );
     } // end for loop
	 userPool = new USERClass();
     System.out.println( "Scheduler: ----------------------- Initialised ---------------------" );
  }

  public void jspDestroy() {
     System.out.println( "Scheduler: ------------------------ Terminating --------------------" );
     System.out.println( "Scheduler: jspDestroy: Clearing connection pool in jspDestroy" );
     socketPool.SCHEDPoolWrapperDESTROY();
     userPool.USERClassDESTROY();
     System.out.println( "Scheduler: ------------------------ Terminated ---------------------" );
  }

// -------------------------------------------------------------------------
//                       Some helper routines
// -------------------------------------------------------------------------
  // translate a command send response code into a message string, called from
  // virtually all routines that issue a command to any server.
  public String getCmdrespcodeAsString( int num ) {
     String msgString = "";
     if (num == 0) { msgString = "OK"; }
     else if (num == 1) { msgString = "No free connections in socket pool"; }
     else if (num == 2) { msgString = "Logon details rejected by the server"; }
     else if (num == 3) { msgString = "RETRY, contention issue"; }  // we found a slot, but someone grabbed it first
     else if (num == 4) { msgString = "Command string is in an illegal format"; }
     else if (num == 5) { msgString = "Socket IO error, remote application may be down"; }
     else if (num == 6) { msgString = "Hostname used is not configured to the application"; }
     else { msgString = "Undocumented response code = " + num; }
	 return msgString;
  } // getCmdresponseAsString

  // Java just doesn't have a routine to convert a number held in a string
  // to an integer, we need one so create one here. It will only support
  // the ASCII character set. I can't affort an MVS mainframe to test any EBCDIC
  // handling and don't intend to bother coding it.
  public int StringToInt( String numberInString ) {
     int totalVal = 0;
     char testByte = '0';
     if ((int)testByte == 48) { // ascii character table
        char[] zz = numberInString.toCharArray();
        totalVal = 0;  // just in case it's not initialised to zero
        for (int i = 0; i < numberInString.length(); i++) {
           totalVal = (totalVal * 10) + ((int)zz[i] - 48);
        }
     } else { // must be ebcdic character set, not implemented
        System.out.println( "EBCDIC Character Set is not supported");
        totalVal = 0;
     }
     return totalVal;
  } // StringToInt
%>
<HTML><HEAD><TITLE>Marks Job Scheduler, JSP Interface, &copy Mark Dickinson 2002, 2003, 2004</TITLE>
<META HTTP-EQUIV="Pragma" CONTENT="no-cache">
<%
  // If the page is expected to refresh write the refresh request to the page.
  // May need to retrieve and rewrite all parameters here
  String pageRequestType = request.getParameter("pagename");
  if (pageRequestType == null) { pageRequestType = "logon"; }
  String saveRequest = "";          // in case we need to go through a login
  // The below are used both here and in include files, declare and set globally
  String userName = userPool.getUserName( session.getId() );
  String workUserName = ""; 
  String pageJobName = request.getParameter("jobname");
  String pageHostName = request.getParameter("server");
  String pageActionName = request.getParameter("action");
  // Is the current page one where an automatic refresh interval timer needs
  // to be set, if so, and if it is legal, then set the page to refresh.
  String testrefreshIntvl = request.getParameter("refreshtime");
  if (testrefreshIntvl != null) {
     int refreshIntvl;
     if (testrefreshIntvl.equals("2 minutes")) { refreshIntvl = 120000; }
     else if (testrefreshIntvl.equals("5 minutes")) { refreshIntvl = 300000; }
     else if (testrefreshIntvl.equals("10 minutes")) { refreshIntvl = 600000; }
     else { refreshIntvl = 0; }
     if (refreshIntvl != 0) {
        out.println( "<SCRIPT LANGUAGE=\"JavaScript\">" );
        out.println( "window.setTimeout( \"location.href='" + request.getScheme() + "://" +
                      request.getServerName() + ":8080" + request.getRequestURI() +
                      "?pagename=" + pageRequestType + "&refreshtime=" + testrefreshIntvl +
                      "'\"," + refreshIntvl + ");" );
        out.println( "</SCRIPT>" );
     }
  }
%>
<link rel=StyleSheet type="text/css" href="javascript/toolbar.css">
<script language="Javascript" src="javascript/menubar_lib.js"></script>
<script language="Javascript" src="javascript/menu_options.js"></script>
<script language="Javascript1.2" src="javascript/sticky_menu_code.js"></script>
</HEAD>
<BODY>
<BR>
<TABLE BORDER="0" CELLPADDING="0" CELLSPACING="0" BGCOLOR="#000000" WIDTH="100%"><TR>
<TD ALIGN="LEFT"><SPAN style="font-family: Arial, Helvetica, sans-serif; font-size:7pt; color: #FFFFFF;">Job Scheduler JSP Interface</SPAN></TD>
<TD ALIGN="RIGHT"><SPAN style="font-family: Arial, Helvetica, sans-serif; font-size:7pt; color: #FFFFFF;">Marks Job Scheduler, &copy Mark Dickinson, 2001, 2002, 2003, 2004</SPAN></TD>
</TR></TABLE>
<%--
-------------------------------------------------------------------------------------------------------
                                        M A I N L I N E
What we do here is
  * see if the user has ever logged on, if not get userid and password and check by finding a free session to log them on with.
  * check the request type so we know what html page to load up for the user
  * load the reguired html page
-------------------------------------------------------------------------------------------------------
--%>
<%--
1. Check if the user is logged on, if so we need to build the status bar
   display for the user based on their authority level.
--%>
<%
  // see if the user is logged on, force them to logon if not
  if ( (userName == null) || userName.equals("") ) {
     saveRequest = pageRequestType;
     pageRequestType = "logon";
  }
  else {
     // save the user context as close to the top of the page as possible
     if (pageJobName == null) { pageJobName = ""; }
     if (pageHostName == null) { pageHostName = ""; }
     if (pageActionName == null) { pageActionName = ""; }
     userPool.setUserContext( session.getId(), pageHostName, pageJobName, pageActionName );
     userPool.setUserActivityTime( session.getId() );  // user has done something, rest inactive timer
     // Build the menu bar
     if (  (!(pageRequestType.equals("logon"))) && (!(pageRequestType.equals("logoff")))  ) {
		out.println("<span id=\"staticcombo\" style=\"position:absolute;left:0;top:0;visibility:hidden\">" );
        out.println("<script> drawToolbar(); </script>" );
        out.println("</span><br><br><br>" );
     }
  }
%>
<%--
2. See what the request type was and display the appropriate page.
--%>
<%
  if (pageRequestType.equals("logon")) {
%>
<%@ include file="inc_logonpage.jsp" %>
<%
  }
  else if (pageRequestType.equals("logoff")) {
      userPool.setUserLogoff( session.getId() );  // user has done something, reset inactive timer
      socketPool.LogoffAll( session.getId() );    // logoff all socket entries also
      out.println("<table border=\"1\"><tr><td>All your sessions have been logged off</td></tr></table>");
      session.invalidate();
      out.println( "<br><a href=\"" + request.getScheme() + "://" + request.getServerName() + ":8080" +
                   request.getRequestURI() + "?pagename=logon\">[ LOGON AGAIN ]</a>");
  }
  else if (pageRequestType.equals("defaultblank")) {
%>
<%@ include file="inc_defaultblank.html" %>
<%
  }
  else if (pageRequestType.equals("joblistall")) {
%>
<%@ include file="pagecode/job/inc_joblistall.jsp" %>
<%
  }
  else if (pageRequestType.equals("jobdisplaymenu")) {
%>
<%@ include file="pagecode/job/inc_job_display.jsp" %>
<%
  }
  else if (pageRequestType.equals("jobaction")) {
%>
<%@ include file="pagecode/job/inc_jobaction.jsp" %>
<%
  }
  else if (pageRequestType.equals("jobadd")) {
%>
<%@ include file="pagecode/job/inc_job_creation_form.jsp" %>
<%
  }
  else if (pageRequestType.equals("jobaddscript")) {
%>
<%@ include file="pagecode/job/inc_job_creation_script.jsp" %>
<%
  }
  else if (pageRequestType.equals("schedserverstatus")) {
%>
<%@ include file="pagecode/sched/inc_sched_statusshort.jsp" %>
<%
  }
  else if (pageRequestType.equals("schedlistall")) {
%>
<%@ include file="pagecode/sched/inc_schedlistall.jsp" %>
<%
  }
  else if (pageRequestType.equals("schedshowsessions")) {
%>
<%@ include file="pagecode/sched/inc_showsessions.jsp" %>
<%
  }
  else if (pageRequestType.equals("schedjobaction")) {
    // parms are pagename=schedjobaction, jobname=NAME, server=hostname, action=menu/holdon/holdoff/runnow/delete
%>
<%@ include file="pagecode/sched/inc_schedjobaction.jsp" %>
<%
  }
  else if (pageRequestType.equals("alertlistall")) {
%>
<%@ include file="pagecode/alert/inc_alertlistall.jsp" %>
<%
  }
  else if (pageRequestType.equals("alertaction")) {
%>
<%@ include file="pagecode/alert/inc_alertaction.jsp" %>
<%
  }
  else if (pageRequestType.equals("deplistall")) {
%>
<%@ include file="pagecode/dep/inc_deplistall.jsp" %>
<%
  }
  else if (pageRequestType.equals("depaction")) {
%>
<%@ include file="pagecode/dep/inc_depaction.jsp" %>
<%
  }
  else if (pageRequestType.equals("callistall")) {
%>
<%@ include file="pagecode/cal/inc_callistall.jsp" %>
<%
  }
  else if (pageRequestType.equals("calaction")) {
%>
<%@ include file="pagecode/cal/inc_calaction.jsp" %>
<%
  }
  else if (pageRequestType.equals("calupdate")) {
%>
<%@ include file="inc_notyet.jsp" %>
<%
  }
  else if (pageRequestType.equals("caladd")) {
%>
<%@ include file="pagecode/cal/inc_cal_add_page.jsp" %>
<%
  }
  else if (pageRequestType.equals("caladdscript")) {
%>
<%@ include file="pagecode/cal/inc_cal_add_script.jsp" %>
<%
  }
  else if (pageRequestType.equals("caldisplay")) {
%>
<%@ include file="pagecode/cal/inc_cal_display.jsp" %>
<%
  }
  else if (pageRequestType.equals("userlistall")) {
%>
<%@ include file="pagecode/user/inc_user_listall.jsp" %>
<%
  }
  else if (pageRequestType.equals("userinfo")) {
%>
<%@ include file="pagecode/user/inc_user_details.jsp" %>
<%
  }
  else if (pageRequestType.equals("useradd")) {
%>
<%@ include file="pagecode/user/inc_user_creation_form.jsp" %>
<%
  }
  else if (pageRequestType.equals("useraddscript")) {
%>
<%@ include file="pagecode/user/inc_user_creation_script.jsp" %>
<%
  }
  else if (pageRequestType.equals("usernewpswd")) {
%>
<%@ include file="pagecode/user/inc_user_newpswd.jsp" %>
<%
  }
  else if (pageRequestType.equals("userdel")) {
%>
<%@ include file="pagecode/user/inc_user_delete.jsp" %>
<%
  }
  else if (pageRequestType.equals("showConnectionPool")) {
%>
<%@ include file="pagecode/poolinfo/inc_showConnectionPoolStatus.jsp" %>
<%
  }
  else if (pageRequestType.equals("showUserPool")) {
%>
<%@ include file="pagecode/poolinfo/inc_showUserPoolStatus.jsp" %>
<%
  }
  else {   // unknown, show default page
%>
<%@ include file="inc_notyet.jsp" %>
<%
  }
%>
<BR><BR>
<TABLE BORDER="0" CELLPADDING="0" CELLSPACING="0" BGCOLOR="#000000" WIDTH="100%"><TR>
<TD ALIGN="CENTER"><SPAN style="font-family: Arial, Helvetica, sans-serif; font-size:7pt; color: #FFFFFF;">
This JSP Interface is Version 0.01 BETA</SPAN></TD>
</TR></TABLE>
</BODY>
</HTML>
