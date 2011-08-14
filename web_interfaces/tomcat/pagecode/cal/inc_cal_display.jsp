<%--
  inc_cal_display.jsp

  This is called from the calendar listall page to display details
  on a calendar entry. It is in its own file as the server does
  not implement a calendar display text response but only returns
  the raw calendar record. We have to parse the contents ourselves.

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
   StringBuffer cmdLine = new StringBuffer();
   String testLine, localHostName, dataLine;
   int zz = 0;
   int i, j, showHolNeeded, showDescNeeded;

   cmdLine.append( API_CMD_CAL_READ_RAW );

   // Get the calendar name, type and year. No checking done as we are called
   // from another JSP page so are confident all parameters are present.
   testLine = userPool.getUserContext(session.getId(), "JOBNAME" );
   localHostName = userPool.getUserContext(session.getId(), "HOSTNAME" );
   // should have been saved in the user jobname context, get explicitly if not available
   if (testLine.length() == 0) { testLine = request.getParameter("jobname"); }
   while (testLine.length() < 40) { testLine = testLine + " "; }
   cmdLine.append( testLine ).append("^");
   testLine = request.getParameter("caltype");
   if (testLine.equals("JOB")) { cmdLine.append("0"); } else { cmdLine.append("1"); }
   testLine = request.getParameter("calyear");
   cmdLine.append( testLine ).append("^");
   // junk fill the rest of the record, it's not used by the server for this
   // lookup request, it will be filled in with valid data on the server.

   // Calendar record completed (if valid) here.
   zz = socketPool.setCommand( localHostName, session.getId(), cmdLine,
                                    userPool.getUserName(session.getId()),
                                    userPool.getUserPassword(session.getId()));
   if (zz != 0) { // error
      out.println( "<span style=\"background-color: #FF0000\">" + localHostName + ":**Error** " +
                   getCmdrespcodeAsString(zz) + "</span><br>");
      out.println( "<br>");
   }

   // Now read the entire response buffer, we will have either raw data or an error message.
   out.println( "<h2>Calendar details for " + userPool.getUserContext(session.getId(), "JOBNAME" ) +
                " for host " + userPool.getUserContext(session.getId(), "HOSTNAME" )  + "</h2>" );

   showHolNeeded = showDescNeeded = 0;
   dataLine = socketPool.getResponseCode( localHostName, session.getId() );
   if (dataLine.equals("101")) {     // raw data to be processed
       // need to parse out all the raw fields into something readable now
       // Get the dataline, and remove the API header (85 bytes + pad)
       dataLine = socketPool.getResponseBuffer( localHostName, session.getId() );
       testLine = dataLine.substring(0,4);
       if (testLine.equals("API*")) { dataLine = dataLine.substring( 86, dataLine.length()); }

       // Calendar name
       testLine = dataLine.substring( 0, 40 );
       dataLine = dataLine.substring( 41, dataLine.length() );
       out.println( "Calendar name : " + testLine + "<br>" );
       // Calendar type
       testLine = dataLine.substring( 0, 1 );
       if (testLine.equals("0")) {               // job
          out.println( "Calendar type : JOB Calendar<br>" );
       } else if (testLine.equals("1")) {        // holiday
          out.println( "Calendar type : HOLIDAY Calendar<br>" );
       } else if (testLine.equals("9")) {        // deleted
          out.println( "Calendar type : **Deleted**<br>" );
       } else {
          out.println( "Calendar type : **ERROR** Type not known to JSP application, type=" + testLine + "<br>" );
       }
       // Calendar year
       testLine = dataLine.substring( 1, 5 );
       dataLine = dataLine.substring( 6, dataLine.length() );
       out.println( "Calendar year : " + testLine + "<br>" );
       // Observe holidays, try and get holiday calendar name early if so
       testLine = dataLine.substring( 0, 1 );
       dataLine = dataLine.substring( 1, dataLine.length() );
       if (testLine.equals("Y")) {
          // extract out the holiday calendar from the middle of the buffer
          if (dataLine.length() > 410) {
             testLine = dataLine.substring( 372, 411 );
             out.println( "Rundates overridden by holiday calendar " + testLine + "<br>" );
          } else {
             out.println( "This calendar is overridden by a holiday calendar<br>" );
             showHolNeeded = 1;
          }
       } else if (testLine.equals("N")) {
          out.println( "No dates are overridden by a holiday calendar<br>" );
       } else {
          out.println( "*warning* invalid holiday override flag" );
       }
       // try and get the description early
       if (dataLine.length() > 414) {
          testLine = dataLine.substring( 413, (dataLine.length() - 1) );
          i = testLine.indexOf("^");  // strip trailing ^^^ fields
          testLine = testLine.substring( 0, i );
          out.println( "Description: " + testLine + "<br>" );
       } else {
          showDescNeeded = 1;
       }
       // Now the day list
       out.println( "<br><br>The dates the job will run on (unless overridden by a holiday calendar) are..." );
       out.println("<pre>");
       out.print("    ");
       for (i = 0; i < 31; i++) {
         testLine = "" + (i + 1);
         if (testLine.length() < 2) { testLine = " " + testLine; }
         out.print( testLine + " " );
       }
       out.print( "<br>" );
       for (j = 0; j < 12; j++) {
          switch( j ) {
            case 0: { out.print( "Jan" ); break; }
            case 1: { out.print( "Feb" ); break; }
            case 2: { out.print( "Mar" ); break; }
            case 3: { out.print( "Apr" ); break; }
            case 4: { out.print( "May" ); break; }
            case 5: { out.print( "Jun" ); break; }
            case 6: { out.print( "Jul" ); break; }
            case 7: { out.print( "Aug" ); break; }
            case 8: { out.print( "Sep" ); break; }
            case 9: { out.print( "Oct" ); break; }
            case 10: { out.print( "Nov" ); break; }
            case 11: { out.print( "Dec" ); break; }
          }
          for (i = 0; i < 31; i++) {
             testLine = dataLine.substring( i, (i+1) );
             out.print( "  " + testLine );
          }
          out.print( "<br>" );
          dataLine = dataLine.substring(31, dataLine.length());
       }
       out.println("</pre>");
       // Holiday calendar name (40 bytes)
       if (showHolNeeded == 1) {  // only if not already displayed before daylist
          testLine = dataLine.substring( 0, 40 );
          out.println( "The dates are overridden by holiday calendar " + testLine );
       }
       dataLine = dataLine.substring(41, dataLine.length());
       // Calendar description (40 bytes, all the rest of the buffer)
       if (showDescNeeded == 1) {  // only if not already displayed before daylist
          dataLine = dataLine.substring(0, (dataLine.length() - 1));
          out.println( "Calendar description: " + dataLine );
       }
   } else {                          // something unexpected
       out.println( "<span style=\"background-color: #FF0000\">:**Error** " );
       if ( (dataLine.equals("102")) || (dataLine.equals("100")) ) {     // error text to display, or info text
          dataLine = socketPool.getResponseLine( localHostName, session.getId() );
          while (dataLine != "") {
              out.println( dataLine );
              dataLine = socketPool.getResponseLine( localHostName, session.getId());
          }
       } else {
          out.println( "The response from the server is not in a form that can be processed, contact support staff" );
       }
       out.println( "</span><br>");
   }
%>
