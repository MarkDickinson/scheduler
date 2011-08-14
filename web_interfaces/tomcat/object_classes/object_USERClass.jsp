<%-- 
  Whomever includes us must have the below in their page
  (uncommented of course).
<%@ page import="java.util.Timer" %>
<%@ page import="java.util.TimerTask" %>
<%@ page import="java.util.Date" %>

  And then onto the real working stuff.

  object_USERClass.jsp

  This class manages a table of user entries. It records the sesisonid,
  username and userpassword for each session. This needs to be kept available
  to the application due to the socket expiry/re-use features we use as if the
  users socket connection is stolen we want to be able to log them back on
  again automatically.

  We expire user records after 30 minutes of inactivity, this will force them
  to logon again, as all pages check for a valid user record in this class
  (searches against sesisonid) on will only proceed when there is one.

  We start off with MAX_TABLE_INCREMENT users, and as needed grow the user
  table by this increment until we reach MAX_USERS_ALLOWED at which point we
  will refuse further logins to the application.
  NOTE: we never shrink the table, may revist this but not needed as the
  expired user slots are reused rather than increasing the table if we are able
  to do so.
--%>
<%!
/* =================================================================================
  USERClass.java

  User management class the job scheduler. Tracks and expires user sessions.

  Version History
  0.001-BETA 08Feb2004 MID Created initial java class.

  ==================================================================================
*/

/* *********************************************************************************
   ********************************************************************************* */
public class USERClass implements Serializable {

   public static final int MAX_USERS_ALLOWED = 100;
   public static final int MAX_TABLE_INCREMENT = 5;

   // -----------------------------------------------------------------------------
   // Globals we require
   // -----------------------------------------------------------------------------
   private   userDetails[] userDetailsArray;
   private   TimerTask expire_loop;
   private   int userElementCount = 0;  // How many entries are in the table

   /** -----------------------------------------------------------------------------
   // USERClass():
   //
   // Contructor: Create the initial user table with the minimum increment.
   // Input: N/A
   // Output: The minimum increment of user slots will be available.
   // Globals: userElementCount set to the number of user slots that exist
   // Threads: A user expiry time will be created.
   // -----------------------------------------------------------------------------
   */
   public USERClass () {
      int i, useElements;
      this.userDetailsArray = new userDetails[MAX_USERS_ALLOWED]; // should not use too much space, we don't
                                                                  // create the objects until we need them
      for (i = 0; i < MAX_TABLE_INCREMENT; i++ ) {
         this.userDetailsArray[i] = new userDetails();
         if ( (userDetailsArray == null) || (userDetailsArray[i] == null) ) {
            System.out.println( "Scheduler: *Error* unable to create user sesison object number " + i );
         }
      }
      System.out.println( "Scheduler: Setting USER session pool entry count to " + MAX_TABLE_INCREMENT + " in constructor");
      userElementCount = MAX_TABLE_INCREMENT;
      // A timer to clear user sessions that havn;t done activity in a while.
      this.expire_loop = new TimerTask() {
         public void run() {
            // to avoid mucking about with -ve number checks we use all the
            // minutes up to the current hour as part of the check value test,
            // and if the minute now < last access minute add 60 minutes to the
            // current time to get a full hours offset rather than -60 from
            // last access, as its only the difference we want anyway.
            int j, minutesTest;
            int minutesForHour, testlastaccesstime, testcurrenttime;
            Date timeNow = new Date();
			int minutenow = timeNow.getMinutes();  // minutes now
			int hournow = timeNow.getHours();      // hours now
            minutesTest = 0;
            minutesForHour = 0;
            // Work out the number of minutes to the current hour.
            for (j = 0; j < hournow; j++ ) { minutesForHour = minutesForHour + 60; }
            // And add the minutes we are testing for to the value worked out.
            minutesTest = minutesForHour + minutenow;
            for (j = 0; j < userElementCount; j++) {
               // expire the session if no activity in the last 30 minutes
               if (userDetailsArray[j].userName.length() > 0) {
                   testlastaccesstime = minutesForHour + userDetailsArray[j].lastActivityTimestamp; // last accessed minute
                   testlastaccesstime = testlastaccesstime + 30;  // check for over 30 mins inactivity
                   if (minutenow < userDetailsArray[j].lastActivityTimestamp) {
                      testcurrenttime = minutesTest + 60; 
                   } else {
                      testcurrenttime = minutesTest; 
                   }
                  if (testlastaccesstime < testcurrenttime) {  // over 30 minutes, expire entry
                     System.out.println( "Scheduler: expiring session for "
                                         + userDetailsArray[j].userName + ", over 30 mins inactivity (last="
                                         + userDetailsArray[j].lastActivityTimestamp + ", now = " + minutenow +")" );
                     expireUserEntry(j);
                  }  // expire stuff
               }  // 
            }  // if user allocated to session
         } // run
      }; // TimerTask
      Timer timer = new Timer();
      timer.scheduleAtFixedRate( expire_loop, 180000, 180000 ); // After 3 min, repeat delay is 3 min
   }  // USERClass constructor

   /** ----------------------------------------------------------------------------
   // USERClassDESTROY:
   //
   // Destructor, hook this in somewhere
   //
   // Input: N/A
   // Output: N/A
   // Globals: All connections terminated
   // Threads: All connectionObject threads terminated
   // -----------------------------------------------------------------------------
   */
   public void USERClassDESTROY() {
      System.out.println( "Scheduler: stopping user session expiry thread" );
      try {
         if (this.expire_loop != null) { expire_loop.cancel(); } // end the timer task
      } catch (Exception e) {
         System.out.println( "Scheduler: unable to cancel user expiry timer task, continuing shutdown" );
      }
      for (int i = 0; i < userElementCount; i++) { this.userDetailsArray[i] = null; }
   } // destructor

   /** ----------------------------------------------------------------------------
   // locateUserEntry: PRIVATE
   //
   // Try to locate any session entry the user already has.
   // Return the entry number if found, 0 if not.
   // Input: the clients tomcat/jsp session id
   // Output:
   //     userElementCount+1 - no matching userids exist
   //     n - the user entry number
   // Globals: N/A
   // Threads: N/A
   // -----------------------------------------------------------------------------
   */
   private int locateUserEntry( String clientSessionId ) {
      for (int i = 0; i < userElementCount; i++ ) {
         if (userDetailsArray[i].clientSessionId != null) {  // if null then not in use yet
            if (userDetailsArray[i].clientSessionId.equals( clientSessionId )) { return i; }
         }
      }
      return (userElementCount + 1);
   } // locateUserEntry

   /** ----------------------------------------------------------------------------
   // allocateUserEntry: PRIVATE
   // Find a free entry in the user pool for the user session to be tracked by.
   // If we have no free entries see if we can expand the user table.
   // Input: the tomcat/JSP sessionid, the user name and the user password
   // Output:
   //     0-(userElementCount-1) = the entry that was allocated
   //     (userElementCount+1)   = no free entries at this time
   // Globals: userElementCount adjusted if we need to increase the pool size.
   // Threads: N/A
   // -----------------------------------------------------------------------------
   */
   private int allocateUserEntry( String clientSessionId, String userName, String userPassword ) {
      int i, freeSessionId;
	  Date workDate = new Date();

      // Set out check above the maximum allowed.
      freeSessionId = userElementCount + 1;
	  // First !!!, see if the user already has a session, may be a second
	  // logon request for the same session.
      i = 0;
      while (( i < userElementCount) && (freeSessionId > userElementCount)) {
         if (userDetailsArray[i].clientSessionId.equals(clientSessionId)) {
             freeSessionId = i;  // reuse this one !
             synchronized(userDetailsArray[i]) {
               userDetailsArray[i].userName = userName;
               userDetailsArray[i].userPassword = userPassword;
               userDetailsArray[i].lastActivityTimestamp = workDate.getMinutes();
             } // syncronised
         } // if
         i++;   // lets not forget to increment i
      } // while

      // First, try to get a session that is not inUse.
      i = 0;
      while (( i < userElementCount) && (freeSessionId > userElementCount)) {
         if (userDetailsArray[i].clientSessionId.length() == 0) {
            // We will grab this one
            synchronized(userDetailsArray[i]) {
               userDetailsArray[i].clientSessionId = clientSessionId;
               userDetailsArray[i].userName = userName;
               userDetailsArray[i].userPassword = userPassword;
               userDetailsArray[i].lastActivityTimestamp = workDate.getMinutes();
            } // syncronised
            freeSessionId = i;
            userDetailsArray[i].currentHostName = "";
            userDetailsArray[i].currentJobName = "";
            userDetailsArray[i].currentActionName = "";
         } // if
         i++;   // lets not forget to increment i
      } // while

      // If we were unable to get a session that is not in use we will try to
	  // create some more user table array entries, and call ourself to go
	  // through all the logic again.
      if (freeSessionId > userElementCount) {
         if ((userElementCount + MAX_TABLE_INCREMENT) <= MAX_USERS_ALLOWED) {
            System.out.println("Scheduler: increasing user pool from " + userElementCount +
                               " to " + (userElementCount + MAX_TABLE_INCREMENT) + " due to workload" );
            for (i = userElementCount; i < (userElementCount + MAX_TABLE_INCREMENT); i++ ) {
            }
            userElementCount = userElementCount + MAX_TABLE_INCREMENT; // warning, cannot synchronise an int, trust to luck
			return ( allocateUserEntry( clientSessionId, userName, userPassword ) );  // recurse
		 } else {
             // at max sessions allowed
             return( (userElementCount + 1) );
         }
      }  // if we trying to create more table entries
      else {
         return( freeSessionId );
      }
   } // alocateUserEntry

   /** ---------------------------------------------------------------------------
   // expireUserEntry: PRIVATE
   //
   // Called by a timer when the inactivity session timer has found a session
   // that needs to be dropped.
   // Input:   The entry number in the user table
   // Output:  N/A
   // Globals: Entry in userDetailArray flushed of user details
   // Threads: N/A
   // ----------------------------------------------------------------------------
   */
   private void expireUserEntry( int entryNum ) {
      userDetailsArray[entryNum].clientSessionId = "";
      userDetailsArray[entryNum].userName = "";
      userDetailsArray[entryNum].userPassword = "";
      userDetailsArray[entryNum].currentHostName = "";
      userDetailsArray[entryNum].currentJobName = "";
//    just leave timestamp as-is as we locate spare sessions by searching for a username anyway.
//      userDetailsArray[entryNum].lastActivityTimestamp = 0;
   } // expireUserEntry

   /** ---------------------------------------------------------------------------
   // setUserLogoff: PRIVATE
   //
   // Called manuall when a user chooses to logoff explicitly.
   // Input:   The tomcat/JSP client session id.
   // Output:  N/A
   // Globals: Entry in userDetailArray flushed of user details
   // Threads: N/A
   // ----------------------------------------------------------------------------
   */
   public void setUserLogoff( String clientSessionId ) {
      int entryNum;
      entryNum = locateUserEntry( clientSessionId );
      if (entryNum < userElementCount) {
         System.out.println( "Scheduler: user " + userDetailsArray[entryNum].userName + " logged off" );
         expireUserEntry( entryNum );
      }
   } // setUserLogoff

   /** ----------------------------------------------------------------------------
   // getUserCount:
   //
   // return the number of user array entries allocated
   // Input: N/A
   // Output: The number of elements allocated to the user array
   // Globals: N/A
   // Threads: N/A
   // -----------------------------------------------------------------------------
   */
   public int getUserCount() {
      return userElementCount;
   } // getUserCount

   /** ----------------------------------------------------------------------------
   // getUserMax:
   //
   // return the maximum number of user array entries allocated
   // Input: N/A
   // Output: The maximum number of elements that can ever be allocated to the user array
   // Globals: N/A
   // Threads: N/A
   // -----------------------------------------------------------------------------
   */
   public int getUserMax() {
      return MAX_USERS_ALLOWED;
   } // getUserMax

   /** ----------------------------------------------------------------------------
   // getUserName( String clientSessionId ):
   //
   // Return the user name for the requested JSP sessionid
   // Input: the tomcat/JSP sessionid
   // Output: A string containing the user name
   // Globals: N/A
   // Threads: N/A
   // -----------------------------------------------------------------------------
   */
   private String getUserName( String clientSessionId ) {
      int entryNum;
	  String workLine = "";
      entryNum = locateUserEntry( clientSessionId );
      if (entryNum < userElementCount) {
         return userDetailsArray[entryNum].userName;
      } else {
         return "";
      }
   } // getUserName

   /** ----------------------------------------------------------------------------
   // getUserPassword( String clientSessionId ):
   //
   // Return the user password for the requested JSP sessionid. Insecure you
   // say ?, well the entire point of the user class it to allow for the user
   // to be transparently logged back onto the job scheduler servers if there
   // is any session stealing going on due to excessive workload, so the
   // password must be retrievable by the main code in order to provide logon
   // details on behalf of the user. As the JSP remains on the server this is
   // still fairly secure, none of this is passed back to a browser.
   // Input: the tomcat/JSP sessionid
   // Output: A string containing the user password.
   // Globals: N/A
   // Threads: N/A
   // -----------------------------------------------------------------------------
   */
   private String getUserPassword( String clientSessionId ) {
      int entryNum;
	  String workLine = "";
      entryNum = locateUserEntry( clientSessionId );
      if (entryNum < userElementCount) {
         return userDetailsArray[entryNum].userPassword;
      } else {
         return "";
      }
   } // getUserPassword

   /** ----------------------------------------------------------------------------
   // setUserActivityTime( String clientSessionId ):
   //
   // Called whenever a user does any activity, to update their last activity
   // timestamp.
   //
   // Input: the tomcat/JSP sessionid
   // Output: A string containing the user name
   // Globals: N/A
   // Threads: N/A
   // -----------------------------------------------------------------------------
   */
   public void setUserActivityTime( String clientSessionId ) {
      int entryNum, minutesNow;
	  Date workTime = new Date();
      entryNum = locateUserEntry( clientSessionId );
      if (entryNum < userElementCount) {
         minutesNow = workTime.getMinutes();
         userDetailsArray[entryNum].lastActivityTimestamp = minutesNow;
      }
   } // setUserActivityTime

   /** ----------------------------------------------------------------------------
   // setUserContext( String clientSessionId, String hostname, String jobname, String actionname ):
   //
   // Save any key context variable for a user.
   //
   // Input: the tomcat/JSP sessionid, a host name and job name as appropriate
   // Output: N/A
   // Globals: User session variable updated with key context informarion.
   // Threads: N/A
   // -----------------------------------------------------------------------------
   */
   public void setUserContext( String clientSessionId, String hostname, String jobname, String actionname ) {
      int entryNum;
      entryNum = locateUserEntry( clientSessionId );
      if (entryNum < userElementCount) {
         userDetailsArray[entryNum].currentHostName = hostname;
         userDetailsArray[entryNum].currentJobName = jobname;
         userDetailsArray[entryNum].currentActionName = actionname;
      }
   } // setUserContext

   /** ----------------------------------------------------------------------------
   // getUserContext( String clientSessionId, String contextKey ):
   //
   // Retrieve a context value for a user.
   //
   // Input: the tomcat/JSP sessionid, the context type required
   // Output: Context value requested returned.
   // Globals: N/A
   // Threads: N/A
   // -----------------------------------------------------------------------------
   */
   public String getUserContext( String clientSessionId, String contextKey) {
      int entryNum;
      String resultString;
      entryNum = locateUserEntry( clientSessionId );
      if (entryNum < userElementCount) {
         if ( contextKey == null ) { resultString = ""; }
         else if ( contextKey.equals("HOSTNAME") ) { resultString = userDetailsArray[entryNum].currentHostName; }
         else if ( contextKey.equals("JOBNAME") ) { resultString = userDetailsArray[entryNum].currentJobName; }
         else if ( contextKey.equals("ACTION") ) { resultString = userDetailsArray[entryNum].currentActionName; }
         else { resultString = ""; }
      }
      else { resultString = ""; }
      return resultString;
   } // getUserContext


/* *********************************************************************************
   userDetails: Internal class:
   This class describes a user. We don't have any methods, our main class will
   update the variables directly as required.
   ********************************************************************************* */
   // Describe what we have in our connection pool objects
   private class userDetails {
        String clientSessionId = "";   // client session id using the connection
        String userName = "";          // The user name logged onto the sessionid
        String userPassword = "";      // The user password, we need to retrieve it
        int lastActivityTimestamp;     // last time this user did anything
        // Context information needed. As the pages multithread we want to save
        // any key variables for a user request in their user context. These
        // may or may not be empty.
        String currentHostName;        // current servername commands are issued for
        String currentJobName;         // current Job commands are issued for
        String currentActionName;      // current request pending issuence for job

        // -----------------------------------------------------------------------
        // Constructor
		//
        // Input:  N/A
        // Output: N/A
        // Globals: N/A
        // Threads: each connectionPoolObject created starts a reader thread
        //          for the socket.
        // -----------------------------------------------------------------------
        public userDetails() {
          Date workDate = new Date();
          this.clientSessionId = "";
          this.userName = "";
          this.userPassword = "";
          this.lastActivityTimestamp = workDate.getMinutes();
        } // contructor

   } // userDetails

} // SCHEDPoolBean
%>
