<%--
  inc_job_creation_script.jsp

  Check the parameters provided for the new job. If all are OK
  then sumbit the add job request to the selected, or to all servers
  as appropriate; and display the reponse to the user.

  This may look a little messy, it needs to build up a C job record
  format. The record format in C (as copied from the server header 
  code, minus comments) is below.

  Imporant notes: the server interface (cunningly designed) expects
  all null characters to be set to "^" at the transmitting end, it 
  will parse them back to nulls when it gets the command buffer. This
  allows us to use "^" to terminate the end of field values here as well
  as the null at max field length without upsetting any string handling
  by requiring the use of real null characters.
  OR IS IT NL ?.

#define MAX_DEPENDENCIES 5
#define CALENDAR_NAME_LEN 40
#define JOB_NUMBER_LEN 6
#define JOB_NAME_LEN 30
#define JOB_DATETIME_LEN 17
#define JOB_DEPENDTARGET_LEN 80
#define JOB_OWNER_LEN 30
#define JOB_EXECPROG_LEN 80
#define JOB_PARM_LEN 80
#define JOB_CALNAME_LEN CALENDAR_NAME_LEN
#define JOB_DESC_LEN 40
#define JOB_REPEATLEN 3
#define JOB_REPEAT_MAXVALUE 999

/* -------------------------------------------------
   JOB header record. Common to all records.
   NEVER MOVE jobnumber. It is used by name to address
   the start of the job header buffer for bulk memory
   copies.
   ------------------------------------------------- */
   typedef struct {
      char jobnumber[JOB_NUMBER_LEN+1]; /* jobnumber MUST ALWAYS be first, its used for addressing */
      char jobname[JOB_NAME_LEN+1];
      char info_flag;             /* A=active,D=deleted,S=system */
   } job_header_def;

/* -------------------------------------------------
   Dependency info.
   A standard structure for recoding job dependencies.
   ------------------------------------------------- */
   typedef struct {
      char dependency_type;      /* 0 = no dependency, 1 = waiting for a job to complete, 2 = waiting for a file to appear */
      char dependency_name[JOB_DEPENDTARGET_LEN+1];  /* job or filename we are waiting on */
   } dependency_info_def;

/* -------------------------------------------------
  The JOBS file record.
  This record contains detailed information about a
  job.
  This record is used as a reference point
  for all functions performed by the scheduler.
   ------------------------------------------------- */
   typedef struct {
      job_header_def job_header;
      char last_runtime[JOB_DATETIME_LEN+1];           /* yyyymmdd hh:mm:ss  */
      char next_scheduled_runtime[JOB_DATETIME_LEN+1];
      char job_owner[JOB_OWNER_LEN+1];
      char program_to_execute[JOB_EXECPROG_LEN+1];
      char program_parameters[JOB_PARM_LEN+1];
	  char description[JOB_DESC_LEN+1];   /* Job description field */
      dependency_info_def dependencies[MAX_DEPENDENCIES];
      char use_calendar;        /* 0=no, 1=yes, 2=crontype weekdays without calendar, 3=every nn minutes */
      char calendar_name[CALENDAR_NAME_LEN+1];
	  char crontype_weekdays[7]; /* fields 0-6 (0=off, 1=on) of Sun thru Sat if use_calendar=2 */
	  char requeue_mins[JOB_REPEATLEN+1]; /* number of minutes this job repeats at if use_calendar=3 */
	  char job_catchup_allowed; /* if Y job repeats if scheduler catchup set, if off then
                                   it is not a job that is allowed to be 'caught up'
                                   ie: disk cleanups that only need to run once to catchup anyway */
      char calendar_error;      /* Y=calendar error(expired or missing), N=all is OK with calendar */
	  /* Reserve some fields for further expansion */
	  char reserved_1;
	  char reserved_2;
	  char reserved_3;
	  char reserved_4;
	  char reserved_5;
	  char reserved_6;
   } jobsfile_def;
--%>
<%
  String testLine, testChar, workLine, calendarLine, daylistLine, repeatLine, localHostName, dataLine;
  StringBuffer cmdLine = new StringBuffer();
  StringBuffer jobRecord = new StringBuffer();
  StringBuffer errorList = new StringBuffer();
  int zz, doCmd, i, foundDayList, foundCalendar, foundCheck, foundRepeat, numHosts;

  doCmd = 1; // default is all OK so far
  foundDayList = 0; // no job scheduling overides yet
  foundCalendar = 0; // no Calendar found yet
  foundRepeat = 0;
  daylistLine = "";  // to stop may not have been initialised errors
  calendarLine = ""; // to stop may not have been initialised errors
  repeatLine = "";   // to stop may not have been initialised errors
 
  // check we have all the required parameters, buiding the command as we go.
  // 1. The job header definition
  testLine = request.getParameter("jobname");
  if ((testLine == null) || (testLine.length() == 0)) {
     doCmd = 0;
     errorList.append( "Missing required field: Job Name<br>" );
  } else {
     testLine = testLine + "^";
     while (testLine.length() < 30) { testLine = testLine + " "; }
     jobRecord.append( "000000^" ).append( testLine ).append( "^A" );
  }
  // 2. The last runtime needs to be dummied up
     jobRecord.append( "00000000 00:00:00^" );
  // 3. The next scheduled runtime
  testLine = request.getParameter("starttime");
  if ((testLine == null) || (testLine.length() == 0)) {
     doCmd = 0;
     errorList.append( "Missing required field: Job Start Time<br>" );
  } else if (testLine.length() != 14) {
     doCmd = 0;
     out.println( "<span style=\"background-color: #FF0000\">Field length error    : Job Start Time</span>" );
  } else {
     jobRecord.append( testLine ).append( ":00^" );
  }
  // 4. The job owner
  testLine = request.getParameter("userid");
  if ((testLine == null) || (testLine.length() == 0)) {
     doCmd = 0;
     errorList.append( "Missing required field: Unix userid<br>" );
  } else {
     testLine = testLine + "^";
     while (testLine.length() < 30) { testLine = testLine + " "; }
     if (testLine.length() > 30) {
        doCmd = 0;
        errorList.append( "Field too large       : Unix userid (max 30 bytes)<br>" );
     } else {
        jobRecord.append( testLine ).append( "^" );
     }
  }
  // 5. The program to execute
  testLine = request.getParameter("jobcmd");
  if ((testLine == null) || (testLine.length() == 0)) {
     doCmd = 0;
     errorList.append( "Missing required field: Program to execute<br>" );
  } else {
     testLine = testLine + "^";
     while (testLine.length() < 80) { testLine = testLine + " "; }
     if (testLine.length() > 80) {
        doCmd = 0;
        errorList.append( "Field too large       : Program to execute (max 80 bytes including full path)<br>" );
     } else {
        jobRecord.append( testLine ).append( "^" );
     }
  }
  // 6. Any program parameters, may be blank
  testLine = request.getParameter("jobparms");
  if ((testLine == null) || (testLine.length() == 0)) {
     testLine = "^";
  } else {
     testLine = testLine + "^";
  }
  while (testLine.length() < 80) { testLine = testLine + " "; }
  if (testLine.length() > 80) {
     doCmd = 0;
     errorList.append( "Field too large       : Job parameters (max 80 bytes)<br>" );
  } else {
     jobRecord.append( testLine ).append( "^" );
  }
  // 7. Job Desription
  testLine = request.getParameter("jobdesc");
  if ((testLine == null) || (testLine.length() == 0)) {
     doCmd = 0;
     errorList.append( "Missing required field: Job description<br>" );
  } else {
     testLine = testLine + "^";
     while (testLine.length() < 40) { testLine = testLine + " "; }
     if (testLine.length() > 40) {
        doCmd = 0;
        errorList.append( "Field too large       : Job description (max 40 bytes)<br>" );
     } else {
        jobRecord.append( testLine ).append( "^" );
     }
  }
  // 8. Job dependency info, up to MAX_DEPENDENCIES (5 in this release)
  testLine = request.getParameter("jobhasdep");
  if ((testLine == null) || (testLine.length() == 0)) {
     testLine = "no";  // assume none
  }
  if (testLine.equals("yes")) {   // some dependencies to be stored
     foundCheck = 0;  // use to count dependencies found here
     testLine = request.getParameter("jobdep1");
     if (testLine != null) {
        testLine = testLine + "^";
        while (testLine.length() < 80) { testLine = testLine + " "; }
        testChar = testLine.substring( 0, 1 );
        if (testChar.equals("/")) {               // file wait, type 2
           testLine = "2" + testLine + "^";
        } else {                                  // job wait, type 1
           testLine = "1" + testLine + "^";
        }
        jobRecord.append( testLine );
        foundCheck++;
     }
     testLine = request.getParameter("jobdep2");
     if (testLine != null) {
        testLine = testLine + "^";
        while (testLine.length() < 80) { testLine = testLine + " "; }
        testChar = testLine.substring( 0, 1 );
        if (testChar.equals("/")) {               // file wait, type 2
           testLine = "2" + testLine + "^";
        } else {                                  // job wait, type 1
           testLine = "1" + testLine + "^";
        }
        jobRecord.append( testLine );
        foundCheck++;
     }
     testLine = request.getParameter("jobdep3");
     if (testLine != null) {
        testLine = testLine + "^";
        while (testLine.length() < 80) { testLine = testLine + " "; }
        testChar = testLine.substring( 0, 1 );
        if (testChar.equals("/")) {               // file wait, type 2
           testLine = "2" + testLine + "^";
        } else {                                  // job wait, type 1
           testLine = "1" + testLine + "^";
        }
        jobRecord.append( testLine );
        foundCheck++;
     }
     testLine = request.getParameter("jobdep4");
     if (testLine != null) {
        testLine = testLine + "^";
        while (testLine.length() < 80) { testLine = testLine + " "; }
        testChar = testLine.substring( 0, 1 );
        if (testChar.equals("/")) {               // file wait, type 2
           testLine = "2" + testLine + "^";
        } else {                                  // job wait, type 1
           testLine = "1" + testLine + "^";
        }
        jobRecord.append( testLine );
        foundCheck++;
     }
     testLine = request.getParameter("jobdep5");
     if (testLine != null) {
        testLine = testLine + "^";
        while (testLine.length() < 80) { testLine = testLine + " "; }
        testChar = testLine.substring( 0, 1 );
        if (testChar.equals("/")) {               // file wait, type 2
           testLine = "2" + testLine + "^";
        } else {                                  // job wait, type 1
           testLine = "1" + testLine + "^";
        }
        jobRecord.append( testLine );
        foundCheck++;
     }
     if (foundCheck == 0) {
        doCmd = 0;
        errorList.append( "Missing Dependencies  : Use dependencies was checked, but no dependeny entries were provided<br>" );
     }
  } else {                        // no dependencies, set empty fields
     testLine = "                                                                     "; // save some loop time
     while (testLine.length() < 80) { testLine = testLine + " "; }
     testLine = "0" + testLine + "^";
     for (i = 0; i < 5; i++ ) { jobRecord.append( testLine ); }
  }
  // 9. Are we using scheduling overides
  //   9.1 Are we using a calendar
  testLine = request.getParameter("usecal");
  if ((testLine == null) || (testLine.length() == 0)) {
     testLine = "no";  // assume none
  }
  if (testLine.equals("yes")) {
     testLine = request.getParameter("calname");
     if ((testLine == null) || (testLine.length() == 0)) {
        doCmd = 0;
        errorList.append( "Missing Calendar      : Use calendar was checked, but no calendar name was provided<br>" );
     } else {
        testLine = testLine + "^";
        while (testLine.length() < 40) { testLine = testLine + " "; }
        testLine = testLine + "^";
        calendarLine = testLine;               // save without header, don't append yet
	    foundCalendar = 1;  // we are using a calendar
     }
  } else {
     testLine = "^";
     while (testLine.length() < 40) { testLine = testLine + " "; }
     testLine = testLine + "^";
     calendarLine = testLine;               // save without header, don't append yet
  }

  //   9.2 Are we using selected week days   FRED <- I search on this
  //       These can be used with a calendar !.
  testLine = request.getParameter("daylist");
  if ((testLine == null) || (testLine.length() == 0)) {
     testLine = "no";  // assume none
  }
  if (testLine.equals("yes")) {
     // process dayname0 through 6
     foundCheck = 0;
     for (i = 0; i < 7; i++ ) {
        testLine = request.getParameter("dayname" + i);
        if (testLine != null) {
           if (foundCalendar == 1) {      // calendar can override
              daylistLine = daylistLine + "2";
           } else {
              daylistLine = daylistLine + "1";
           }
           foundCheck++;
        } else {
           daylistLine = daylistLine + "0";
        } // end 0
     } // end i loop
     if (foundCheck == 0) {
        doCmd = 0;
        errorList.append( "Missing Day List      : Use selected days was chosen, but no days were selected<br>" );
     } else {
        foundDayList = 1;
     }
  } else { // else daylist = yes
     daylistLine = "0000000"; // no days
  }

  //   9.3 Are we using a repeat interval
  testLine = request.getParameter("repeat");
  if ((testLine == null) || (testLine.length() == 0)) {
     testLine = "no";  // assume none
  }
  if (testLine.equals("yes")) {
     if ((foundDayList == 1) || (foundCalendar == 1)) {
        doCmd = 0;
        errorList.append( "Conflict detected     : You are using a repeat interval, but daynames or a calendar has been selected<br>" );
     } else {
        testLine = request.getParameter("repeatmins");
        if ((testLine == null) || (testLine.length() == 0)) {
           doCmd = 0;
           errorList.append( "Missing parameter     : You selected a repeat interval, but did not enter a repeat minute interval<br>" );
        } else {
            while (testLine.length() < 3) { testLine = "0" + testLine; }
			repeatLine = testLine;     // save it
            jobRecord.append( testLine );
            foundRepeat = 1;
        }
     } // no overide found yet
  } else { // yes for repeat
     repeatLine = "000";
  }

  // Now we have the calendar, daylist and repeat calue we know what to put in
  // the calendar type field, so we can build the calendar type, calendar name,
  // daylist and repeat interval fields up.
  if (foundDayList != 0) {
     if (foundCalendar == 0) {
         jobRecord.append( "2" + calendarLine ); // 2 is use calendar only, no daylist
     } else {
         jobRecord.append( "1" + calendarLine ); // 1 is use calendar only, no daylist
     }
	 jobRecord.append( daylistLine );            // will be set or zeros
  } else if (foundRepeat == 1) {
     jobRecord.append( "3" + calendarLine );     // 3s a repeating job type
	 jobRecord.append( daylistLine );
  } else {
     jobRecord.append( "0" + calendarLine ).append(daylistLine);  // 0 is no calendar
  }
  jobRecord.append( repeatLine ).append("^");

  // 10. The catchup flag
  testLine = request.getParameter("catchup");
  if ((testLine == null) || (testLine.length() == 0)) {
     testLine = "no";  // assume none
  }
  if (testLine.equals("yes")) {
     jobRecord.append( "Y" );
  } else {
     jobRecord.append( "N" );
  }

  // 11. Fill in the reserved fields
  jobRecord.append( "------" );

  // OK, if no errors we now have a useable job record
  localHostName = request.getParameter("server");
  if ((localHostName == null) || (localHostName.length() < 1)) {
     doCmd = 0;
     errorList.append( "Missing parameter     : No hostname was selected from the list<br>" );
  }

  // NOW, DID WE FIND ANY ERRORS ?
  //  If yes, just show the errors, otherwise we need to determine how many
  //  servers to send the command to, and send it.
  if (doCmd == 0) {
     out.println( "<h3>Errors prevented the job being added</h3>" );
     out.println( "<table border=\"1\" BGCOLOR=\"#FF0000\"><tr><td>" );
     out.println( errorList.toString() );
     out.println( "</td></tr></table>" );
  }
  
 // ELSE NO ERRORS, send the command to the server or servers
  else {
     testLine = "" + jobRecord.length();
	 while (testLine.length() < 9) { testLine = "0" + testLine; }
	 cmdLine.append( API_CMD_JOB_ADD ).append( testLine ).append( jobRecord.toString() );
     // how many servers are we sending this to ?, send it
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
     out.println( "<H1>Responses to job add request</H1>" );
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
     } // a single server

} // else doCmd permitted
%>
