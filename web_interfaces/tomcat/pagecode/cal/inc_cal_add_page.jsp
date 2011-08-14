<%--
  inc_cal_add_page.jsp

  This builds the calendar add template page. The only things of note are we
  populate the server list with the servers the application knows about to
  allow the user to select one (or all) and we populate the year list with
  what we know are good values so we don't have to do too much checking in the
  serlvet code block that actually does the add.
--%>
<%
   String workLine = "";
   String currentCalSelected = "";
   Date workDate = new Date();
   int workYear, i, j, currentCalNumber = 0;
   
   // Check/Set any year we may have been preovided with, or default to here;
   // before doing anything else.
   // The year the calendar is for is required
   // See if one was passed to us, use it if it was, otherwise default to
   // current year.
   workYear = workDate.getYear() + 1900;
   currentCalSelected = request.getParameter("calyear");
   if (currentCalSelected == null) {
      currentCalNumber =  workYear;
      currentCalSelected = "" + currentCalNumber;
   } else {
      currentCalNumber = StringToInt( currentCalSelected );
      // Ensure its in an allowable range
      if ( (currentCalNumber < workYear) || (currentCalNumber > (workYear + 5))) {
         currentCalNumber = workYear;
         currentCalSelected = "" + currentCalNumber;
      }
   }

   // Some javascript code we will be calling
   out.println( "<SCRIPT LANGUAGE=\"JavaScript\">" );
   out.println( "// --------------------------------------------------------------" );
   out.println( "// These arrays need to be globally defined, they are used by" );
   out.println( "// all the helper routines." );
   out.println( "// --------------------------------------------------------------" );
   out.println( "monthnames = new Array( \"Jan\", \"Feb\", \"Mar\", \"Apr\", \"May\", \"Jun\", \"Jul\", \"Aug\", \"Sep\", \"Oct\", \"Nov\", \"Dec\" );" );
   out.println( "monthdays = new Array(12);" );
   out.println( "monthdays[0]=31;" );
   out.println( "monthdays[1]=28; // updated to 29 if needed by calendar draw" );
   out.println( "monthdays[2]=31;" );
   out.println( "monthdays[3]=30;" );
   out.println( "monthdays[4]=31;" );
   out.println( "monthdays[5]=30;" );
   out.println( "monthdays[6]=31;" );
   out.println( "monthdays[7]=31;" );
   out.println( "monthdays[8]=30;" );
   out.println( "monthdays[9]=31;" );
   out.println( "monthdays[10]=30;" );
   out.println( "monthdays[11]=31;" );
   out.println( "" );
   out.println( "// --------------------------------------------------------------" );
   out.println( "// Draw the calandar for the selected year to the page." );
   out.println( "// --------------------------------------------------------------" );
   out.println( "function displayMonthTable() {" );
   out.println( "calyear = " + currentCalNumber + ";" );

   out.println( "document.write(\"<table border=2 bgcolor=white \");" );
   out.println( "document.write(\"bordercolor=black><font color=black>\");" );
   out.println( "document.write(\"<tr><td><b>\" + calyear + \"</b><br>Month/Day</td><td>Su</td><td>M</td><td>Tu</td><td>W</td><td>Th</td><td>F</td><td>Sa</td>\");" );
   out.println( "document.write(\"<td>Su</td><td>M</td><td>Tu</td><td>W</td><td>Th</td><td>F</td><td>Sa</td>\");" );
   out.println( "document.write(\"<td>Su</td><td>M</td><td>Tu</td><td>W</td><td>Th</td><td>F</td><td>Sa</td>\");" );
   out.println( "document.write(\"<td>Su</td><td>M</td><td>Tu</td><td>W</td><td>Th</td><td>F</td><td>Sa</td>\");" );
   out.println( "document.write(\"<td>Su</td><td>M</td><td>Tu</td><td>W</td><td>Th</td><td>F</td><td>Sa</td>\");" );
   out.println( "document.write(\"<td>Su</td><td>M</td><td>Tu</td><td>W</td><td>Th</td><td>F</td><td>Sa</td></tr>\");" );
   out.println( "for (i=0;i < 12; i++ ) {" );
   out.println( "todayDate=new Date( calyear, i, 1 ); // 1st of each month" );
   out.println( "thisday=todayDate.getDay();" );
   out.println( "thisyear=calyear;" );
   out.println( "if (i == 1) {" );
   out.println( "thisyear = thisyear % 100;" );
   out.println( "thisyear = ((thisyear < 50) ? (2000 + thisyear) : (1900 + thisyear));" );
   out.println( "if (((thisyear % 4 == 0) && !(thisyear % 100 == 0)) ||(thisyear % 400 == 0)) monthdays[1]++;" );
   out.println( "}" );
   out.println( "" );
   out.println( "document.write( \"<tr><td>\" );" );
   out.println( "document.write( monthnames[i] );" );
   out.println( "document.write( \"</td>\" );" );
   out.println( "for (s = 0; s < thisday; s++) { document.write(\"<td></td>\"); }" );
   out.println( "for (j = 1; j <= monthdays[i]; j++ ) {" );
   out.println( "document.write( \"<TD>\" + j + \"<BR><INPUT TYPE=CHECKBOX NAME=\" );" );
   out.println( "document.write( monthnames[i] + \" VALUE=\" + j + \"></INPUT></TD>\" );" );
   out.println( "}" );
   out.println( "document.write( \"</tr>\" );" );
   out.println( "}" );
   out.println( "document.write(\"</table></p>\");" );
   out.println( "} // display_month_table" );
   out.println( "" );
   out.println( "// --------------------------------------------------------------" );
   out.println( "// Set some of the checkboxes in the calendar based upon what" );
   out.println( "// day name checkbox has been selected/unselected." );
   out.println( "// --------------------------------------------------------------" );
   out.println( "function set_dayentries() {" );
   out.println( "for (j = 0; j < 7; j++) {  // Sun through Sat" );
   out.println( "for (i = 0; i < 12; i++ ) {" );
   out.println( "if (dataform.daylist[j].checked) { boolval = true; } else { boolval = false; };" );
   out.println( "testDate=new Date( calyear, i, 1 ); // 1st of each month" );
   out.println( "firstday=testDate.getDay();" );
   out.println( "startnum = (j + 7) - firstday;" );
   out.println( "while (startnum > 6) { startnum = startnum - 7; }" );
   out.println( "while (startnum < monthdays[i]) {" );
   out.println( "switch (i) {" );
   out.println( "case 0: dataform.Jan[startnum].checked = boolval; break;" );
   out.println( "case 1: dataform.Feb[startnum].checked = boolval; break;" );
   out.println( "case 2: dataform.Mar[startnum].checked = boolval; break;" );
   out.println( "case 3: dataform.Apr[startnum].checked = boolval; break;" );
   out.println( "case 4: dataform.May[startnum].checked = boolval; break;" );
   out.println( "case 5: dataform.Jun[startnum].checked = boolval; break;" );
   out.println( "case 6: dataform.Jul[startnum].checked = boolval; break;" );
   out.println( "case 7: dataform.Aug[startnum].checked = boolval; break;" );
   out.println( "case 8: dataform.Sep[startnum].checked = boolval; break;" );
   out.println( "case 9: dataform.Oct[startnum].checked = boolval; break;" );
   out.println( "case 10: dataform.Nov[startnum].checked = boolval; break;" );
   out.println( "case 11: dataform.Dec[startnum].checked = boolval; break;" );
   out.println( "default: break;" );
   out.println( "} // switch" );
   out.println( "startnum = startnum + 7;" );
   out.println( "}" );
   out.println( "} // for i" );
   out.println( "} // for j" );
   out.println( "} // set_dayentries" );
   out.println( "" );
   out.println( "// --------------------------------------------------------------" );
   out.println( "// Year has changed, must redisplay the form for the new year." );
   out.println( "// --------------------------------------------------------------" );
   out.println( "function calendar_year_change() {" );
   out.println( "   newyear = dataform.calyear.selectedIndex; // the index entry" );
   out.println( "   newyear = newyear + " + workYear + ";             // add the base yearto get real entry year" );
   out.println( "   restofit=\"" + request.getScheme() + "://" + request.getServerName() +
                 ":8080" + request.getRequestURI() + "?pagename=caladd&calyear=\"" );
   out.println( "   location.href=restofit + newyear;" );
   out.println( "} // calendar_year_change" );
   out.println( "</SCRIPT>" );

   // Onto the page itself
   out.println( "<center><h1>Create a new Calendar</h1></center>");

   // -- build the form header and the hidden fields needed --
   out.println("<form name=dataform action=\"" + request.getScheme() + "://" + request.getServerName() + ":8080" +
               request.getRequestURI() + "\" method=\"post\">");
   out.println("<INPUT TYPE=HIDDEN NAME=\"pagename\" VALUE=\"caladdscript\">");

   // Build up a list of servers we know about, the user can select one (or all) to issue the job add command to.
   out.println("<b>Select the server to add the calendar to</b>: <SELECT NAME=\"server\"><OPTION>All Servers</OPTION>");
   for (i = 0; i < socketPool.getHostMax(); i++) {
      out.println("<OPTION>" + socketPool.getHostName(i) + "</OPTION>");
   }
   out.println("</SELECT><BR>You must have calendar creation authority for the unix userid to be used");

   // -- get the required fields --
   out.println( "<HR><H3>Required Fields</H3><pre>");
   out.println( "<p>These fields are required for every calendar, they describe the calendar.</p>" );
   out.println( "Calendar Name : <input type=text size=15 name=jobname> (required, no spaces)");
   out.println( "Calendar Type : <select name=caltype><option value=\"JOB\">JOB</option>");
   out.println( "<option value=\"HOLIDAY\">HOLIDAY</option></select>     (required, JOB or HOLIDAY))");

   // Do some mucking about to provide a list of upcoming years the user can select from
   // Note, we must set the selected value to either what theuser provided or
   // the current year we defaulted to as appropriate.
   out.println( "Calendar Year : <select name=calyear onChange=\"calendar_year_change()\"> (changing the year will clear the form)");
   for (i = 0; i < 6; i++) {
      workLine = "" + (workYear + i);
      if ((workYear + i) == currentCalNumber) {
         out.println( "<option selected value=\"" + workLine + "\">" + workLine + "</option>");
      } else {
         out.println( "<option value=\"" + workLine + "\">" + workLine + "</option>");
      }
   }
   out.println( "</select>         (required, 4 digit year))");

   // The calendar description
   out.println( "Description   : <input type=text size=60 name=caldesc> (describe it)</pre>");

   // -- the dates now --
   out.println( "<HR><H3>Scheduling specifications</H3>");
   out.println( "<p>You must select days for the calendar. You may turn on/off a range of specific days in the");
   out.println( "main calendar by using the dayname toggles, then customise further using the calendar itself.</p><pre>");

   // Provide a checkbox list of days that can be selected
   out.println( "Dayname toggles: " +
               "Sun <INPUT TYPE=CHECKBOX NAME=\"daylist\" onClick=\"set_dayentries()\"></INPUT>" +
               "  Mon <INPUT TYPE=CHECKBOX NAME=\"daylist\" onClick=\"set_dayentries()\"></INPUT>" +
               "  Tue <INPUT TYPE=CHECKBOX NAME=\"daylist\" onClick=\"set_dayentries()\"></INPUT>" +
               "  Wed <INPUT TYPE=CHECKBOX NAME=\"daylist\" onClick=\"set_dayentries()\"></INPUT>" +
               "  Thu <INPUT TYPE=CHECKBOX NAME=\"daylist\" onClick=\"set_dayentries()\"></INPUT>" +
               "  Fri <INPUT TYPE=CHECKBOX NAME=\"daylist\" onClick=\"set_dayentries()\"></INPUT>" +
               "  Sat <INPUT TYPE=CHECKBOX NAME=\"daylist\" onClick=\"set_dayentries()\"></INPUT>");
   out.println( "</pre>");

   // Allow the option of selecting by specific dates, these will be merged with the other selections.
   out.println( "Finish selecting days to run from the calendar here :" );
   out.println( "<SCRIPT>" );
   out.println( "displayMonthTable();" );
   out.println( "</SCRIPT>" );

   // Allow for a holiday calendar to be used to override the dates selected already
   out.println( "<HR><H3>Holiday Calendar Override</H3>");
   out.println( "<p>Only enter a holiday calendar name here if this calendar is a <b>JOB</b> calendar. " +
                "Holiday calendars should not be applied to other holiday calendars. <b>This field is optional</b>.</p>" );
   out.println( "Holiday calendar used to override this calendar : <input type=text size=40 name=holidaycal>");

   // And the form submit buttons, and close the form
   out.println( "<hr><center><input type=submit name=submitcmd value=\"Add New Calendar\">&nbsp&nbsp&nbsp&nbsp<input type=reset>");
   out.println( "</center></form>");
%>
