<%--
   job_creation.jsp

   Show a data entry form the user can use to add a job entry.
--%>

<br><center><h1>Creating a new Job Definition</h1>
<%
    // Start the form, and
    // define some hidden fields so we can pass what would normally be in the
	// query string to the server with this post request.
    out.println("<form action=\"" + request.getScheme() + "://" + request.getServerName() + ":8080" +
                request.getRequestURI() + "\" method=\"post\">");
    out.println("<INPUT TYPE=HIDDEN NAME=\"pagename\" VALUE=\"jobaddscript\">");
	// Build up a list of servers we know about, the user can select one (or
	// all) to issue the job add command to.
	out.println("<b>Select the server to add the job to</b>: <SELECT NAME=\"server\"><OPTION>All Servers</OPTION>");
	for (int i = 0; i < socketPool.getHostMax(); i++) {
       out.println("<OPTION>" + socketPool.getHostName(i) + "</OPTION>");
	}
	out.println("</SELECT><BR>You must have job creation authority for the unix userid to be used");
%>
</center><pre>
<H2>Required Fields</H2>
           <b>Job Name</b> : <input type=text size=31 name=jobname> (required, no spaces)
Runs as unix userid : <input type=text size=14 name=userid>    (required, an existing unix userid)
Initial time to run : <input type=text size=14 name=starttime>     (required, YYYYMMDD HH:MM)
     Command to run : <input type=text size=60 name=jobcmd> (provide the full path)
 Parameters to pass : <input type=text size=60 name=jobparms> (note: Keyword ~~DATE~~ can be used here)
 Description of job : <input type=text size=60 name=jobdesc> (a meaningfull description)
</pre>
<H2>Optional Additional Configuration</H2>
<h3>Job Dependency Rules</h3>
<pre>
Does the job have prior job or file dependencies ? : <input type=radio checked name=jobhasdep value="no">No <input type=radio name=jobhasdep value="yes">Yes<br>
      If the job waits on dependencies enter them in the list here
      File dependencies must include the full path, Any entry not beginning with / is treated as a job dependency.
                   <input type=text size=60 name=jobdep1>
                   <input type=text size=60 name=jobdep2>
                   <input type=text size=60 name=jobdep3>
                   <input type=text size=60 name=jobdep4>
                   <input type=text size=60 name=jobdep5>

Catch up missed run days ? : <input type=radio name=catchup value="no">No <input type=radio name=catchup checked value="yes">Yes<br><br>
</pre>
<h3>Job Scheduling Customisation</h3>
<p>
You can only <b>select one of the options</b> below, repeating interval, selected
days, or a calendar; <b>or you may leave them all unselected</b> in which case the
job will be scheduled to run on a daily basis.<br>
</p>
<pre>
Repeating job ? : <input type=radio checked name=repeat value="no">No <input type=radio name=repeat value="yes">Yes
If repeating, rerun at <input type=text size=5 name=repeatmins> minute intervals (ie:30 for half hourly)<br><br>
Run every day ? : <input type=radio checked name=daylist value="no">Every Day <input type=radio name=daylist value="yes">Selected Days Only
If selected days only, which ones <input type=checkbox name=dayname0 value="SUN">Sun <input type=checkbox name=dayname1 value="MON">Mon <input type=checkbox name=dayname2 value="TUE">Tue <input type=checkbox name=dayname3 value="WED">Wed <input type=checkbox name=dayname4 value="THU">Thu <input type=checkbox name=dayname5 value="FRI">Fri <input type=checkbox name=dayname6 value="SAT">Sat
<br><br>
Use a calendar ? : <input type=radio checked name=usecal value="no">No <input type=radio name=usecal value="yes">Yes
If using a job calendar what is the job calendar name <input type=text size=40 name=calname> (Calendar must already exist)<br><br>
</pre><hr>
<center><b>Ready to submit the job, checked everything ?</b><br><input type=submit name=submitcmd value="Create Job">&nbsp&nbsp&nbsp&nbsp<input type=reset>
</center></form>
<hr><br>
