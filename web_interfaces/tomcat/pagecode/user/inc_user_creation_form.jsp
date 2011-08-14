<%--
   inc_user_creation_form.jsp

   Show a data entry form the user can use to add a new user entry.

  The user information will have to be passed to the job
  scheduler servers in a user record structure, as defined
  in the job scheduler userrecdef.h, as below...

#define USER_MAX_NAMELEN 39
#define USER_MAX_PASSLEN 39
#define USER_LIB_VERSION "01"
#define USERS_MAX_USERLIST 10

typedef struct {
   char userid[USER_MAX_NAMELEN+1];  /* a unix userid */
} USER_unix_userid_element;

typedef struct {
   char userrec_version[3];                 /* version of userrec to allow later on-the-fly upgrades 01 */
   char user_name[USER_MAX_NAMELEN+1];      /* The user name                      */
   char user_password[USER_MAX_PASSLEN+1];  /* users password                     */
   char record_state_flag;                  /* A=active, D=deleted                */
   char user_auth_level;                    /* O=operator, A=admin (all access), 0=none/browse,
                                               J=job auth (browse+add/delete jobs),
                                               S=security user...user maint only */
   char local_auto_auth;                    /* Y=allowed to autologin from jobsched_cmd effective uid locally, N=no */
   char allowed_to_add_users;               /* Y=allowed to add/delete auth to add jobs for users in their auth list
                                               N=not at all,
                                               S=can create users with auth to any unix id */
   USER_unix_userid_element user_list[USERS_MAX_USERLIST];  /* users this ID can define jobs for  */
   char user_details[50];                   /* free form text (ie: real name)     */
   char last_logged_on[18];                 /* DATE_TIME_LEN(scheduler.h)+1 */
   char last_password_change[18];           /* DATE_TIME_LEN(scheduler.h)+1 */
} USER_record_def;
--%>
<center><h1>Creating a new User record</h1>
<%
    // Start the form, and
    // define some hidden fields so we can pass what would normally be in the
	// query string to the server with this post request.
    out.println("<form action=\"" + request.getScheme() + "://" + request.getServerName() + ":8080" +
                request.getRequestURI() + "\" method=\"post\">");
    out.println("<INPUT TYPE=HIDDEN NAME=\"pagename\" VALUE=\"useraddscript\">");
	// Build up a list of servers we know about, the user can select one (or
	// all) to issue the job add command to.
	out.println( "<table border=\"1\"><tr><td width=\"75%\">" );
	out.println("You must have security authority for the server or manager subsetauth authority on the server for the unix userids the new user will be allowed to create jobs for.");
	out.println("This is explained in the <a href=\"pagecode/user/security_details.html\">authority descriptions</a> document.");
	out.println("</td><td><b>Select the server to add the User to</b>:<br> <SELECT NAME=\"server\"><OPTION>All Servers</OPTION>");
	for (int i = 0; i < socketPool.getHostMax(); i++) {
       out.println("<OPTION>" + socketPool.getHostName(i) + "</OPTION>");
	}
	out.println("</SELECT></td></td></table>");
%>
</center><pre>
<b>Job scheduler login id</b> : <input type=text size=39 name=username> (no spaces)
          <b>User Details</b> : <input type=text size=50 name=userdetails> (free-form; full name, dept etc)
 <b>User initial password</b> : <input type=PASSWORD size=39 name=password1>
</pre>
<table border="1" width="100%"><tr><td>Refer to the <a href="pagecode/user/security_details.html">authority descriptions</a> if you
are unsure about any of the three fields below.</td></tr></table>
<pre>
User Authority Level : <select name="authlevel">
    <option>0 - Read only</option>
    <option>O - Operator Authority, active job management</option>
    <option>J - Job authority, job and calendar management</option>
    <option>S - Security group, can only add and delete users</option>
    <option>A - Admin, only ever grant to Serber Administrators</option>
    </select>

Command line autologin allowed: No<input type=radio name="autoauth" value="no"></input> Yes<input type=radio name="autoauth" value="yes"></input>

Able to add users: No<input type=radio name="useraddauth" value="no"></input> SubSet Auth<input type=radio name="useraddauth" value="yes"></input> Full Security Auth<input type=radio name="useraddauth" value="security">
</pre>
<table border="1" width="100%"><tr><td><b>Note:</b> If you are defining a user as a Security or Admin user they will
get full security authority regualrdless of what you enter in the 'Able to add
users' selection. Refer to the <a href="pagecode/user/security_details.html">authority descriptions</a>
for information on the above three fields.</td></tr></table>
<h3>Enter the unix userids this user can create jobs for</h3>
<p>These must be valid unix userids that exist on the server(s) you are
adding the user record to. <b>They are only required for <em>Job</em> or
<em>Admin</em> users</b>, the other user authority level groups are
not permitted to create jobs.<br>
This list restricts the user being added to only being able to create jobs to
run as the unix userid(s) they are granted, <b>you should only allow
<em>systems support personel</em> to use <em>root</em></b>.
</p>
<pre>
<input type=text size="15" name="unixuser1"></input> <input type=text size="15" name="unixuser2"></input> <input type=text size="15" name="unixuser3"></input> <input type=text size="15" name="unixuser4"></input> <input type=text size="15" name="unixuser5"></input>
<input type=text size="15" name="unixuser6"></input> <input type=text size="15" name="unixuser7"></input> <input type=text size="15" name="unixuser8"></input> <input type=text size="15" name="unixuser9"></input> <input type=text size="15" name="unixuser10"></input>
</pre>
<hr>
<center><input type=submit name=submitcmd value="Add the User record">&nbsp&nbsp&nbsp&nbsp<input type=reset>
</center></form>
<br>
