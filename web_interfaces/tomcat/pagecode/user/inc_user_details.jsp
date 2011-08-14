<%--
  inc_user_details.jsp

  This will retrieve the details for a user from every server we
  know about, and display the details returned.
  The user name must be passed to the job scheduler servers
  in a user record structure, as defined below...

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
<%
   int zz, numHosts;
   String hostname;
   String dataLine;
   String userRecord;
   StringBuffer cmdLine = new StringBuffer();
   String schedUserName = request.getParameter("username");

   if (schedUserName == null) {
%>
<h2>Input selection error</h2>
<p>The URL that linked to this page was missing key user data,
there is a problem with the page the request was issued from.
The source code will need correction.</p>
<%
   }
   else {
      out.println("<H2>User details for " + schedUserName + " for all servers</H2>");
      // Build the user record
      userRecord = "01^" + schedUserName + "^"; // leave the rest and hope it's not sanity checked

      // Send the query to all hosts
      cmdLine.append( API_CMD_USER_INFO );
      cmdLine.append( userRecord );
      numHosts = socketPool.getHostMax();
      for (int i = 0; i < numHosts; i++ ) {
         hostname = socketPool.getHostName(i);
         zz = socketPool.setCommand( hostname, session.getId(), cmdLine,
                                     userPool.getUserName(session.getId()),
                                     userPool.getUserPassword(session.getId()) );
         if (zz != 0) {     // error
            out.println( "<span style=\"background-color: #FF0000\">" + hostname + ":**Error** " + getCmdrespcodeAsString(zz) + "</span><br>" );
            out.println( "<br>");
         }
      } // for numhosts

      // Now process the reponses from all servers
      out.println( "<table border=\"1\" cellpadding=\"5\"><tr><td><pre>" );
      for (int i = 0; i < numHosts; i++ ) {
         hostname = socketPool.getHostName(i);
         out.println( "<H3>Details for " + hostname + "</H3>" );
         // Check the response code
         dataLine = socketPool.getResponseCode( hostname, session.getId() );
         if ( (dataLine.equals("100")) || (dataLine.equals("+++")) ) {     // response display text to display or non API data
            dataLine = socketPool.getResponseLine( hostname, session.getId() );
            while (dataLine != "") {
               out.println( dataLine );
               dataLine = socketPool.getResponseLine( hostname, session.getId() );
            } // while dataline != ""
         } // if response code = 100
         else {
            out.println( "<span style=\"background-color: #FF0000\">**Error** " );
            if (dataLine.equals("101")) {     // raw data returned (binary, not display data)
               out.println( "Respcode=101, raw data returned from server. This is unexpected. Fix your code" );
            }
            else if (dataLine.equals("102")) {     // error text to display
               dataLine = socketPool.getResponseLine( hostname, session.getId() );
               while (dataLine != "") {
                  out.println( dataLine );
                  dataLine = socketPool.getResponseLine( hostname, session.getId() );
               }
            }
            else if (dataLine.equals("---")) {                                   // no matching buffer
               out.println( hostname + ":There is no longer a data buffer associated with your session" );
            }
            else {
               out.println( hostname + ":Unrecognised response code (" + dataLine + ") contact support staff" );
            }
            out.println( "</span><br>");
         } // not display data
      } // for numHosts
      out.println( "</pre></td></tr></table>" );
      out.println( "<center><a href=\"" + 
                   request.getScheme() + "://" + request.getServerName() + ":8080" +
                   request.getRequestURI() + "?pagename=usernewpswd&username=" + schedUserName +
                   "\">[ CHANGE USER PASSWORD ]</a>" +
                   "   <a href=\"" + 
                   request.getScheme() + "://" + request.getServerName() + ":8080" +
                   request.getRequestURI() + "?pagename=userdel&username=" + schedUserName +
                   "\">[ DELETE USER ]</a></center>");
   } // end of else a username was provided

   out.println( "End of user information display" );
%>
