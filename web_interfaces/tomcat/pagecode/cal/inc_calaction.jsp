<%--
  inc_calaction.jsp

  The page invoked when we need to issue an action or query against
  a calendar in the calendar definition databases. It works against a specific
  server only.

  This needs to use a calender record object to pass the calendar name, type
  and year to the server.
  The calendar record definition as used by the server in the current
  release of the software is below...

#define CALENDAR_NAME_LEN 40
#define CALENDAR_DESC_LEN 40

/* -------------------------------------------------
   CALENDAR record structure.
   ------------------------------------------------- */
   typedef struct {
      char days[31];
   } month_elements;
   typedef struct {
      month_elements month[12];
   } all_months;

   typedef struct {
      /* --- start record key --- */
      char calendar_name[CALENDAR_NAME_LEN+1];
      char calendar_type;       /* 0 = job
                                 * 1 = holidays
                                 * 9 = deleted
                                 */
      char year[5];  /* 4 + pad, to ensure when year rolls over dates are reset/obsolete */
      /* --- end record key --- */
      char observe_holidays;    /* holidays allowed to override dates Y or N */
      all_months yearly_table;
      char holiday_calendar_to_use[CALENDAR_NAME_LEN+1];
      char description[CALENDAR_DESC_LEN+1];
   } calendar_def;
#endif
--%>
<%
   int zz, doCmd;
   String localCalName, calrecLen, localAction, testLine;
   StringBuffer calRecord = new StringBuffer();
   StringBuffer cmdLine = new StringBuffer();
   StringBuffer errorLine = new StringBuffer();

   doCmd = 1; // default is do the command

   // 1. Build the calendar record up, we need it for all legal requests
//   localCalName = userPool.getUserContext(session.getId(), "JOBNAME" ); Field too short and with with ^
   localCalName = request.getParameter("jobname");
   while (localCalName.length() < 40) { localCalName = localCalName + " "; }
   calRecord.append( localCalName ).append("^");
   testLine = request.getParameter("caltype");
   if (testLine == null) { testLine = "ERR"; }
   if (testLine.equals("JOB")) {
      calRecord.append( "0");
   } else if (testLine.equals("HOL")) {
      calRecord.append( "1");
   } else {
       doCmd = 0;
       errorLine.append( "Illegal calendar type provided to the scriptlet (" + testLine + ")<br>" );
   }
   testLine = request.getParameter("calyear");
   if (testLine == null) { testLine = "ERR"; }
   if (testLine.length() == 4) {
      calRecord.append( testLine ).append( "^" );
   } else {
       doCmd = 0;
       errorLine.append( "Illegal year value provided to the scriptlet (" + testLine + ")<br>" );
   }
   // End of record key, just dummy fill the rest here
   calRecord.append( "N" );
   testLine = "....+....1....+....2....+....3.";
   for (int i = 0; i < 12; i ++) { calRecord.append(testLine); }
   testLine = "....+....1....+....2....+....3....+....4";
   calRecord.append( testLine ).append( testLine );

   // what are we doing ?
   localAction = userPool.getUserContext(session.getId(), "ACTION" );
   if (localAction.equals("display")) {
      out.println( "<h3>Calendar entry " + userPool.getUserContext(session.getId(), "JOBNAME" ) +
                   " on server " + userPool.getUserContext(session.getId(), "HOSTNAME" ) + "</h3>" +
                   "<table border=\"1\"><tr><td>" );
      cmdLine.append(API_CMD_CAL_INFO).append( calRecord.toString() );
   }
   else if (localAction.equals("delete")) {
      out.println( "<h3>Results of trying to delete calendar " + userPool.getUserContext(session.getId(), "JOBNAME" ) +
                   " on server " + userPool.getUserContext(session.getId(), "HOSTNAME" ) + "</h3>" +
                   "<table border=\"1\"><tr><td>" );
      cmdLine.append(API_CMD_CAL_DELETE).append( calRecord.toString() );
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

   // Show a button to return to the calendar list.
   out.println( "<center><a href=\"" + request.getScheme() + "://" + request.getServerName() +
                ":8080" + request.getRequestURI() +
                "?pagename=callistall\">[BACK TO CALENDAR LIST]</a></center>" );
%>
