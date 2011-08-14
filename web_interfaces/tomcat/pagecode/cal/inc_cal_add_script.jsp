<%--
  inc_cal_add_script.jsp

  This is triggered by the inc_cal_add_page.jsp script to actually get the
  fields the user enetred, and build them into a valid calendar record
  buffer that we can write to the server(s) to add the new calendar
  record.

  The request object values are normally retrieved as parameter
  name and value pairs. HOWEVER if there is more than one value of
  of parameter name (more than one occurence provided, ie: a checkbox
  with two entries checked) what is returned is JSP-implementation
  dependant; the getParameter() is not guaranteed to work across all
  vendor platforms. To work around this we need to use getParameterValues,
  which should return them all for all platforms that follow the JSP standard.

  The dtata we write to the server must be in a calendar record format, that
  format as defined by the server C interface at the current release is as
  follows...

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
--%>

<%
   StringBuffer calendarRecord = new StringBuffer();
   StringBuffer cmdLine = new StringBuffer();
   StringBuffer errorLine = new StringBuffer();
   String testLine = "";
   String holidayCalName;
   int count;
   int doCmd = 1;  // default is to do the command
   // the scheduled day array, define and initialise.
   char[] dayEntries = {  // 31 * 12 entries
      'N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N',
      'N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N',
      'N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N',
      'N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N',
      'N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N',
      'N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N',
      'N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N',
      'N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N',
      'N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N',
      'N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N',
      'N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N',
      'N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N'
   };
   int dayEntryBase = 0; // used to move up in 31 entry increments

   // Start out buffer
   cmdLine.append( API_CMD_CAL_ADD );

   // Get the calendar name
   testLine = userPool.getUserContext(session.getId(), "JOBNAME" );
   // should have been saved in the user jobname context, get explicitly if not available
   if (testLine.length() == 0) { testLine = request.getParameter("jobname"); }
   if ((testLine == null) || (testLine.length() == 0)) {
      doCmd = 0;
      errorLine.append( "No calendar name was provided<br>" );
   } else {
      testLine = testLine + "^";
      while (testLine.length() < 40) { testLine = testLine + " "; }
      cmdLine.append( testLine ).append("^");
   }

   // Get the calendar type and year, these are filled in by select boxes in
   // the add page so will always have values, store as-is.
   testLine = request.getParameter("caltype");
   if ((testLine == null) || (testLine.length() == 0)) {
      doCmd = 0;
      errorLine.append( "No calendar type was provided<br>" );
   } else {
      if (testLine.equals("JOB")) { cmdLine.append("0"); } else { cmdLine.append("1"); }
   }
   testLine = request.getParameter("calyear");
   if ((testLine == null) || (testLine.length() == 0)) {
      doCmd = 0;
      errorLine.append( "No calendar year was provided<br>" );
   } else {
      cmdLine.append( testLine ).append("^");
   }

   // Check for a holiday calendar name, we need to know if we
   // are to set the observe holidays flag
   holidayCalName = request.getParameter("holidaycal");
   if ((holidayCalName == null) || (holidayCalName.equals(""))) {
      cmdLine.append( "N" );
   } else {
      cmdLine.append( "Y" );
   }
   // Now extract the days selected from the input form calendar.
   int foundAnEntry = 0;
   String[] monthTable = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
   String[] dayArrayValues;
   for (int monthCount = 0; monthCount < 12; monthCount++ ) {
      dayArrayValues = request.getParameterValues(monthTable[monthCount]);
      for (count = 0; count < dayArrayValues.length; count++ ) {
          int indexVal = StringToInt(dayArrayValues[count]);
          dayEntries[dayEntryBase + indexVal] = 'Y';
          foundAnEntry++;
      } // for count
      dayEntryBase = dayEntryBase + 31;
   }  // for monthCount
   if (foundAnEntry == 0) {
      doCmd = 0;
      errorLine.append( "No run days were selected for the calendar<br>" );
   } else {
      for (count = 0; count < (31 * 12); count++ ) { cmdLine.append(dayEntries[count]); }
   }

   // If we had a holiday calendar store its name now
   if ((holidayCalName != null) && (holidayCalName.length() > 0)) {
      holidayCalName = holidayCalName + "^";
   } else {
      holidayCalName = "^";
   }
   while (holidayCalName.length() < 40) { holidayCalName = holidayCalName + " "; }
   cmdLine.append( holidayCalName ).append( "^" );

   // Get the calendar description
   testLine = request.getParameter("caldesc");
   if ((testLine == null) || (testLine.equals(""))) {
      doCmd = 0;
      errorLine.append( "No calendar description was provided<br>" );
   } else {
      testLine = testLine + "^";
      while (testLine.length() < 40) { testLine = testLine + " "; }
      cmdLine.append( testLine ).append("^");
   }

   // Calendar record completed (if valid) here.

  // NOW, DID WE FIND ANY ERRORS ?
  //  If yes, just show the errors, otherwise we need to determine how many
  //  servers to send the command to, and send it.
  if (doCmd == 0) {
     out.println( "<h3>Errors prevented the calendar being added</h3>" );
     out.println( "<table border=\"1\" BGCOLOR=\"#FF0000\"><tr><td>" );
     out.println( errorLine.toString() );
     out.println( "</td></tr></table>" );
  }
  
 // ELSE NO ERRORS, send the command to the server or servers
  else {
     // how many servers are we sending this to ?, send it
     int zz, i, numHosts;
	 String dataLine;
     String localHostName = userPool.getUserContext(session.getId(), "HOSTNAME" );
     if (localHostName.equals("All Servers")) {
        numHosts = socketPool.getHostMax();
        for (i = 0; i < numHosts; i++ ) {
            zz = socketPool.setCommand( socketPool.getHostName(i), session.getId(), cmdLine,
                                       userPool.getUserName(session.getId()),
                                       userPool.getUserPassword(session.getId()));
            if (zz != 0) { // error
               out.println( "<span style=\"background-color: #FF0000\">" + socketPool.getHostName(i) + ":**Error** " +
                            getCmdrespcodeAsString(zz) + "</span><br>");
               out.println( "<br>");
            }
        } // for Numhosts
     } else { // a single server whos name we know
         zz = socketPool.setCommand( localHostName, session.getId(), cmdLine,
                                    userPool.getUserName(session.getId()),
                                    userPool.getUserPassword(session.getId()));
         if (zz != 0) { // error
            out.println( "<span style=\"background-color: #FF0000\">" + localHostName + ":**Error** " +
                         getCmdrespcodeAsString(zz) + "</span><br>");
            out.println( "<br>");
         }
     } // a single server

     // Now read all the responses we expect
     out.println( "<H1>Responses to calendar add request</H1>" );
     if (localHostName.equals("All Servers")) {
        numHosts = socketPool.getHostMax();
        for (i = 0; i < numHosts; i++ ) {
           out.println( "<h3>" + socketPool.getHostName(i) + "</h3>" );
           dataLine = socketPool.getResponseCode( socketPool.getHostName(i), session.getId() );
           if ( (dataLine.equals("100")) || (dataLine.equals("+++")) ) {     // response display text to display or non API data
              dataLine = socketPool.getResponseLine( socketPool.getHostName(i), session.getId() );
              while (dataLine != "") {
                 out.println( dataLine );
                 dataLine = socketPool.getResponseLine( socketPool.getHostName(i), session.getId());
              }
           } else {
              out.println( "<span style=\"background-color: #FF0000\">:**Error** " );
              if (dataLine.equals("101")) {     // raw data returned (binary, not display data)
                 out.println( "Respcode=101, raw data returned from server.  This is unexpected. Fix your code" );
              }
              else if (dataLine.equals("102")) {     // error text to display
                 dataLine = socketPool.getResponseLine( socketPool.getHostName(i), session.getId() );
                 while (dataLine != "") {
                    out.println( dataLine );
                    dataLine = socketPool.getResponseLine( socketPool.getHostName(i), session.getId());
                 }
              }
              else if (dataLine.equals("---")) { // no matching buffer
                 out.println( "There is no longer a data buffer associated with your session" );
              } else {
                 out.println( "Unrecognised response code (" + dataLine + ") contact support staff" );
              }
              out.println( "</span><br>");
           }
        } // for Numhosts
        out.println("<hr><p>If you see no response data displayed for a host, it did not respond in time. Check results manually.</p>" );
     } else { // a single server whos name we know
           out.println( "<h3>" + localHostName + "</h3>" );
           dataLine = socketPool.getResponseCode( localHostName, session.getId() );
           if ( (dataLine.equals("100")) || (dataLine.equals("+++")) ) {     // response display text to display or non API data
              dataLine = socketPool.getResponseLine( localHostName, session.getId() );
              while (dataLine != "") {
                 out.println( dataLine );
                 dataLine = socketPool.getResponseLine( localHostName, session.getId());
              }
           } else {
              out.println( "<span style=\"background-color: #FF0000\">:**Error** " );
              if (dataLine.equals("101")) {     // raw data returned (binary, not display data)
                 out.println( "Respcode=101, raw data returned from server.  This is unexpected. Fix your code" );
              }
              else if (dataLine.equals("102")) {     // error text to display
                 dataLine = socketPool.getResponseLine( localHostName, session.getId() );
                 while (dataLine != "") {
                    out.println( dataLine );
                    dataLine = socketPool.getResponseLine( localHostName, session.getId());
                 }
              }
              else if (dataLine.equals("---")) { // no matching buffer
                 out.println( "There is no longer a data buffer associated with your session" );
              } else {
                 out.println( "Unrecognised response code (" + dataLine + ") contact support staff" );
              }
              out.println( "</span><br>");
           }
           out.println("<hr><p>If you see no response data displayed, the host did not respond in time. Check results manually.</p>" );
     } // a single server

} // else doCmd permitted
%>
