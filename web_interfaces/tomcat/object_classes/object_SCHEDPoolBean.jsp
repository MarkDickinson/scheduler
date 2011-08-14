<%-- 
  Whomever includes us must have the below in their page
  (uncommented of course).
<%@ page import="java.net.*" %>
<%@ page import="java.io.*" %>
<%@ page import="java.util.Timer" %>
<%@ page import="java.util.TimerTask" %>

  And then onto the real working stuff.
--%>
<%!
/* =================================================================================
  SCHEDPoolBean.java

  Socket connection pooling JavaBean for the job scheduler. Allows for multiple
  connections to the job scheduler for use by JSP pages (or any other tool) via
  a re-useable pool of connections. Logs on to the scheduler using the real
  users userid/password which is tracked by tomcat sessionid.

  Version History
  0.001-BETA 20Jan2004 MID Created initial java class.
  0.001-BETA 06Feb2004 MID Made a JSP include page.

  ==================================================================================
*/

/* *********************************************************************************
   The SCHEDPoolBean class is used to manage a pool of connections to the job
   scheduler application, and provide an interface to issue commands and
   retrieve the response data.
   It uses the class connectionPoolObject to describe each connection and
   manage the buffers and threads needed by each individual connection.
   ********************************************************************************* */
public class SCHEDPoolBean implements Serializable {

   /* API data interface as defined bythe scheduler API library
   typedef struct {
      char API_EyeCatcher[5];                 Eye Catcher 
      char API_System_Name[10];               JOB, SCHED, CAL etc 
      char API_System_Version[10];            Version of API sybsystem
      char API_Command_Number[API_CMD_LEN+1];  requested function
      char API_Command_Response[4];           returned where appropriate
      char API_hostname[API_HOSTNAME_LEN+1];  what host provided the data
      char API_More_Data_Follows;             0 = no, 1 = yes, 2 = continuation no, 3 = continuation yes
      char API_Data_Len[DATA_LEN_FIELD+1];    data length exluding header
      char API_Data_Buffer[MAX_API_DATA_LEN]; data to pass around
   } API_Header_Def;
   */

   public static final int    API_HEADER_LEN = 85;
   public static final String API_SCHED_HEADER = "API*^SCHED    ^000100001^";
   public static final String API_DUMMY_BLOCK  = "^000^HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHXXXX^0";
                                           //          ....+....1....+....2....+....3....+.
   public static final String API_CMD_LOGOFF =
         "API*^SCHED    ^000100001^000000506^000^HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHXXXX^000000000^";
                                           //    ....+....1....+....2....+....3....+.

   public static final String API_CMD_LOGON = "000000501";
   public static final int MAX_CONNECTIONS_ALLOWED = 10;

   // -----------------------------------------------------------------------------
   // Globals we require
   // -----------------------------------------------------------------------------
   private   connectionPoolObject[] connectionArray;
   private   TimerTask refresh_loop;
   private   int poolElementCount = 0;  // How many entries are in the table

   /** -----------------------------------------------------------------------------
   // SCHEDPoolBean():
   //
   // Contructor: only one connection created. A bad idea. Only connects to
   //             localhost on the known port number. This is primarily for testing.
   // Input: N/A
   // Output: a single reuseable connection thread
   // Globals: poolElementCount set to 1
   // Threads: None directly, but the connectionPoolObject will create a socket
   //          read thread for itself.
   // -----------------------------------------------------------------------------
   */
   public SCHEDPoolBean() {
      int junk;
	  // Allocatethe connection pool of one element
      this.connectionArray = new connectionPoolObject[1]; // 1 element in array
      try {
         this.connectionArray[0] = new connectionPoolObject( "localhost", 9002 );
         if ( (connectionArray == null) || (connectionArray[0] == null) ) {
            System.out.println( "Scheduler: *Error* unable to create new connection pool object" );
            this.poolElementCount = 0;
         }
         else {
            junk = this.connectionArray[0].checkConnection();
            System.out.println( "Scheduler: Setting pool entry count to 1 in constructor" );
            this.poolElementCount = 1;
         }
      } catch (IOException e) {
         System.out.println( "Scheduler: *Error* IOException creating connectionPool element" );
		 return;
	  }
      this.refresh_loop = new TimerTask() {
         public void run() {
            int j, k, minutesNow, hoursNow, baseHour, lastAccessMin, conOK;
            Date workDate;
            Boolean testReconnect;
            // Used for buffer inactivity time checks
            workDate = new Date();
            hoursNow = workDate.getHours();
            baseHour = 0;
            for (k = 0; k < hoursNow; k++) { baseHour = baseHour + 60; }
            // Check each session connection entry
            conOK = 1;
            for (j = 0; j < poolElementCount; j++) {
               if ( (connectionArray[j].sessionDataBuffer.length() == 0) && 
                    (connectionArray[j].inUse == true) )
               {
                  connectionArray[j].inUse = false;
               }
               else if ( (connectionArray[j].sessionDataBuffer.length() == 0) && 
                    (connectionArray[j].inUse == false) )
               {
                  setLogoff( j, "Timer" );
               }
               // Additional checks for inactivity time
               minutesNow = baseHour + workDate.getMinutes();
               lastAccessMin = baseHour + connectionArray[j].lastReferenceTime + 5; // +5 as we expire after 5mins inactivity
               if (connectionArray[j].lastReferenceTime < workDate.getMinutes()) { minutesNow = minutesNow + 60; }
               if ((lastAccessMin < minutesNow) &&   // buffer inactive for 5 minutes
                   (connectionArray[j].sessionDataBuffer.length() > 0)) {   // and still buffer data
                  connectionArray[j].eraseReadBuffer( "Inactivity Timer expired" );    // erase it so the above checks trigger
               }
               // We have disconnect pending flags now for closed sockets
               if (connectionArray[j].disConnectQueued) {
                  connectionArray[j].disConnect();
               } else {
                  // Check for any failed connections and restart if possible
                  if (conOK == 1) {
                     conOK = connectionArray[j].checkConnection();
                  }
               }
            }
         } // run
      }; // TimerTask
      Timer timer = new Timer();
      timer.scheduleAtFixedRate( refresh_loop, 120000, 120000 ); // After 2 min, repeat delay is 2 min
   }  // SCHEDPoolBean constructor

   /** -----------------------------------------------------------------------------
   // SCHEDPoolBean( int numElements ):
   //
   // Constructor: numElements connections created
   // Input: the nober of connections to create for the pool
   // Output: The pool is created
   // Globals: poolElementCount set to number of entries
   // Threads: Timer thread created to expire pool entries. +note: each
   //          connection pool object also creates a socket read thread for the
   //          connection pool object.
   // -----------------------------------------------------------------------------
   */
   public SCHEDPoolBean ( int numElements, String hostname, int portnum ) {
      int i, useElements, junk;
	  if (numElements > MAX_CONNECTIONS_ALLOWED)
	  {
	     useElements = MAX_CONNECTIONS_ALLOWED;
      }
      else {
	     useElements = numElements;
      }
      this.connectionArray = new connectionPoolObject[useElements];
      try {
         for (i = 0; i < useElements; i++ ) {
            this.connectionArray[i] = new connectionPoolObject( hostname, portnum );
            if ( (connectionArray == null) || (connectionArray[i] == null) ) {
               System.out.println( "Scheduler: *Error* unable to create connection pool object number " + i );
            } else {
               junk = this.connectionArray[i].checkConnection();
            }
         }
      } catch (IOException e) {
         System.out.println( "Scheduler: *Error* IOException creating connectionPool elements" );
		 return;
	  }
      System.out.println( "Scheduler: Setting pool entry count to " + useElements + " in constructor");
      poolElementCount = useElements;
      // A timer to clear not in use sessions that may hang around
      // if inuse, but buffer empty set to not inuse (no logoff, give user
      // grace to re-use it).
      // if not inuse, logoff
      // fix: buffers were not being read (read before buffer filled), add buffer inactivity checks
      // note: still have to figure out how to delay a read until the buffer IS complete
      this.refresh_loop = new TimerTask() {
         public void run() {
            int j, minutesNow, conOK;
            Date workDate;
            Boolean testReconnect;
            conOK = 1;
            for (j = 0; j < poolElementCount; j++) {
               if ( (connectionArray[j].sessionDataBuffer.length() == 0) && 
                    (connectionArray[j].inUse == true) )
               {
                  connectionArray[j].inUse = false;
               }
               else if ( (connectionArray[j].sessionDataBuffer.length() == 0) && 
                    (connectionArray[j].inUse == false) )
               {
                  setLogoff( j, "Timer" );
               }
               // Additional checks for inactivity time
               if ( (connectionArray[j].sessionDataBuffer.length() > 0) && (connectionArray[j].lastReferenceTime <= 61) ) {
                  workDate = new Date();
                  minutesNow = (workDate.getMinutes() - 2); // allow for 2 mins buffer inactivity
                  if (minutesNow < 0) { minutesNow = minutesNow + 60; }
                  if ((connectionArray[j].lastReferenceTime < minutesNow) &&   // buffer inactive for 2 minutes
                      (connectionArray[j].sessionDataBuffer.length() > 0)) {   // and still buffer data
                     // erase it so the above checks trigger
                     connectionArray[j].eraseReadBuffer( "Inactivity Timer Expired - clearing readbuffer" );
                  }
               }
               // We have disconnect pending flags now for closed sockets
               if (connectionArray[j].disConnectQueued) {
                  connectionArray[j].disConnect();
               } else {
                  // Check for any failed connections and restart if possible
                  if (conOK == 1) {
                     conOK = connectionArray[j].checkConnection();
                  }
               }
            }
         } // run
      }; // TimerTask
      Timer timer = new Timer();
      timer.scheduleAtFixedRate( refresh_loop, 120000, 120000 ); // After 2 min, repeat delay is 2 min
   }  // SCHEDPoolBean constructor

   /** ----------------------------------------------------------------------------
   // SCHEDPoolBeanDESTROY:
   //
   // Destructor, hook this in somewhere
   //
   // Input: N/A
   // Output: N/A
   // Globals: All connections terminated
   // Threads: All connectionObject threads terminated
   // -----------------------------------------------------------------------------
   */
   public void SCHEDPoolBeanDESTROY() {
      System.out.println( "Scheduler: stopping socket recycling thread" );
      try {
         if (this.refresh_loop != null) { refresh_loop.cancel(); } // end the timer task
      } catch (Exception e) {
         System.out.println( "Scheduler: unable to cancel timer task, continuing shutdown" );
      }
      System.out.println( "Scheduler: Closing all " + poolElementCount + " connection pool entries in SCHEDPoolBeanDestroy" );
      for (int i = 0; i < poolElementCount; i++) { this.connectionArray[i].disConnect(); }
   } // destructor

   /** ----------------------------------------------------------------------------
   // locateSessionEntry: PRIVATE
   //
   // Try to locate any session entry the user already has.
   // Return the entry number if found, 0 if not.
   // Input: the clients tomcat/jsp session id
   // Output:
   //     poolElementCount+1 - no matching session exists
   //     n - the session number
   // Globals: N/A
   // Threads: N/A
   // -----------------------------------------------------------------------------
   */
   private int locateSessionEntry( String clientSessionId ) {
      for (int i = 0; i < poolElementCount; i++ ) {
         if (connectionArray[i].sessionId != null) {  // if null then not in use yet
            if (connectionArray[i].sessionId.equals( clientSessionId )) { return i; }
         }
      }
      return (poolElementCount + 1);
   } // locateSessionEntry

   /** ----------------------------------------------------------------------------
   // allocateSessionEntry:
   // Find a free entry in the connection pool for the request to use.
   // Input:
   // Output:
   //     0-(poolElementCount-1) = the entry that was allocated
   //     (poolElementCount+1)   = no free entries at this time
   //     (poolElementCount+99)  = invalid user logon (+99 to make it clear a user fault not a bean fault)
   // Globals:
   // Threads: If a connection is found the reader thread for the connection
   //          found will be triggered when the reponse the login comes in.
   // Notes: we DO NOT flush the response line that comes in with the logon
   //        response, our caller will probably want to use it, the caller can clean
   //        it up (or it will be flushed on the next write).
   // -----------------------------------------------------------------------------
   */
   public int allocateSessionEntry( String clientSessionId, String userName, String userPassword ) {
      int i, freeSessionId;
	  StringBuffer cmdStringBuffer;
	  String ByteLength;
	  String testResponse;
      // ensure we have good data
      if ((clientSessionId == null) || (userName == null) || (userPassword == null)) {
         return (poolElementCount + 99);
      }

      cmdStringBuffer = new StringBuffer();
      freeSessionId = poolElementCount + 1;

      // As a client can only gave one session see if there is an existing one
      // we can use, the user may be re-logging on to an existing session.
      i = 0;
      while (( i < poolElementCount) && (freeSessionId > poolElementCount)) {
         if (connectionArray[i].sessionId.equals(clientSessionId)) {
             freeSessionId = i;
             connectionArray[i].eraseReadBuffer( "allocateSessionEntry (SCHEDPoolBean)" );  // flush any old data
         }
         i++;
      } // while searching for existing sessions

      // If we failed to reuse, try to get a session that is not inUse.
      i = 0;
      while (( i < poolElementCount) && (freeSessionId > poolElementCount)) {
         if ( (!(connectionArray[i].inUse)) && (connectionArray[i].isConnected) && (!(connectionArray[i].disConnectQueued)) ) {
            // set the inUse flag to stop it being grabbed before we do IO for the client request
            synchronized(connectionArray[i]) {
               if ( !(connectionArray[i].inUse) ) {  // recheck, may have been grabbed before we syncronised
                  connectionArray[i].inUse = true;
                  freeSessionId = i;
               } // if ! in use
            } // syncronised
         } // if
         i++;   // lets not forget to increment i
      } // while

      // If we were unable to get a session that is not in use we
      // must locate one with no data in the buffer and steal it.
      // This is bad as a session with no data is not necessarily free,
      // it may be reading data; but it's the best we can do.
      if (freeSessionId > poolElementCount) {
         System.out.println( "Scheduler: *Warning*: connection pool shortage, trying to steal a session" );
         i = 0;
         while (( i < poolElementCount) && (freeSessionId > poolElementCount)) {
            if ( (connectionArray[i].sessionDataBuffer.length() == 0) && (connectionArray[i].isConnected) &&
                 (!(connectionArray[i].disConnectQueued))
               )
            {
               setLogoff( i, "allocateSessionEntry" );
               synchronized(connectionArray[i]) {
                  if ( !(connectionArray[i].inUse) ) {  // recheck, may have been grabbed before we syncronised
                     connectionArray[i].inUse = true;
                     freeSessionId = i;
                  } // if ! in use
               } // syncronised
            }
            i++; // lets not forget to increment i
         } // while
      }  // if we are stealing a connection

      // Nothing free, a last ditch effort is to see if there are any dead
      // connections we may be able to re-activate.
      if (freeSessionId > poolElementCount) {
         System.out.println( "Scheduler: *Warning*: no free connection pool elements found, checking for dead connections" );
         i = 0;
         while (( i < poolElementCount) && (freeSessionId > poolElementCount) && (!(connectionArray[i].disConnectQueued))) {
            if ( !(connectionArray[i].isConnected) ) {
               if (connectionArray[i].reConnect()) {
                  System.out.println( "Scheduler: Reconnected entry " + i );
                  freeSessionId = i;
               }
               else {
                  System.out.println( "Scheduler: *Warning*: unable to reconnect entry " + i );
               }
            }
            i++; // lets not forget to increment i
         } // while
      } // if we are trying to reactivate any dead connections

      // If we were still unable to allocate a slot then we will
      // have to reject the request with no connections available.
      if (freeSessionId > poolElementCount) {
         System.out.println( "Scheduler: *Error*: no free connection pool elements available (" + poolElementCount + " pool entries)" );
         return (poolElementCount + 1);
      } // request not possible at the moment

      // OK, we have a slot allocated to us now
      // do the logon and check results
      connectionArray[freeSessionId].setSessionOwner( clientSessionId );  // so we can lookup later
	  cmdStringBuffer.setLength(0);
	  cmdStringBuffer.append(API_SCHED_HEADER);
	  cmdStringBuffer.append(API_CMD_LOGON);
	  cmdStringBuffer.append(API_DUMMY_BLOCK);
	  // 9 byte length + null
	  i = (userName.length() + userPassword.length() + 3);
	  ByteLength = "" + i;     // convert i to a string, and pad it out to 9 bytes with leading 0's
      while (ByteLength.length() < 9) { ByteLength = "0" + ByteLength; } // much slower than bufferstring append
	  cmdStringBuffer.append(ByteLength);
	  cmdStringBuffer.append('^');
	  cmdStringBuffer.append(userName);
	  cmdStringBuffer.append(" ");
	  cmdStringBuffer.append(userPassword);
	  cmdStringBuffer.append(" 2");  // need pid field, use 2 = JSP socket

	  // write the logon command so painstakingly formatted
      if (!(connectionArray[freeSessionId].writeDataStream( cmdStringBuffer.toString() ) )) {
         return (poolElementCount + 1);  // write failed, no free useable entries
      }

      // Even though we know the index use getResponseCode as while we are
	  // trying to see if we can do this without threads the connection
	  // data buffer is not automatically updated, getResponseCode will
	  // try to do so.
	  // Note: trying without threads as they use 100% CPU even while yielding.
      testResponse = getResponseCode( clientSessionId );
      if (testResponse.equals("102")) {  // error response code, logon failure in this case
         connectionArray[freeSessionId].inUse = false;
         connectionArray[freeSessionId].eraseReadBuffer( "allocateSession (SCHEDPoolBean)" );
         connectionArray[freeSessionId].setSessionOwner( "" );  // drop ownership
         System.out.println( "Scheduler: logon for " + userName + " rejected by server" );
// abnormal return here, clearly markes to avoid confusing it with the ok return below
         return (poolElementCount + 99); // Logon error, refused by server
     }
      // Other non-error response, so logon was accepted.
      System.out.println( "Scheduler: logon for " + userName + " accepted by server" );

      // Other non-error response, so logon was accepted.
      connectionArray[freeSessionId].sessionId = clientSessionId;
      connectionArray[freeSessionId].eraseReadBuffer( "allocateSession (SCHEDPoolBean)" );
      return freeSessionId;
   } // alocateSessionEntry

   /** ---------------------------------------------------------------------------
   // setLogoff: PRIVATE
   //
   // Called by a timer when a connection no longer has data queued to send a
   // logoff request to the server and mark the session available for reuse.
   // Also called by allocateSessionEntry when we need to steal someone elses
   // connection from them, and so need to log them off.
   // Do it all in here, no external calls.
   // Input:   The entry number in the connectionArray table
   // Output:  N/A
   // Globals: Entry in connectionArray flushed of user details and data
   // Threads: The socket read thread for this connectionObject is triggered
   //          to get the logoff response, we clean that up.
   // ----------------------------------------------------------------------------
   */
   private void setLogoff( int entryNum, String sourcemodule ) {
      int i;

      // If nothing is using it then don't bother logging it off
      // Doing so just generates job server overhead
      if ( connectionArray[entryNum].sessionId.equals("") ) { return; }

      // Send the logoff request
      connectionArray[entryNum].inUse = true;
      try {
         connectionArray[entryNum].out.println( API_CMD_LOGOFF );
      }
      catch (Exception e) {
         System.out.println( "Scheduler: *Error* socket IO error, closing connection" );
         connectionArray[entryNum].disConnect();
         return;
      }

      // Try to wait for a full buffer, up to 8 seconds
      i = 0;
      while ( ( connectionArray[entryNum].sessionDataBuffer.length() < API_HEADER_LEN ) && ( i < 100000 ) ) {
         /* sleep(???); // sleep 2 seconds */
         i++;
      }

	  // set connection Pool entry to no longer in use
      connectionArray[entryNum].inUse = false;
      connectionArray[entryNum].sessionDataBuffer = "";
      connectionArray[entryNum].sessionId = "";
      connectionArray[entryNum].lastReferenceTime = 61;
   } // setLogoff

   /** ---------------------------------------------------------------------------
   // userLoggedOff:
   //
   // Provided so the external code can advise us of when a user has explicitly
   // logged off the application. It will call the private setLogoff on the
   // users behalf.
   // Input:   The users tomcat/JSP client session identifier.
   // Output:  N/A
   // Globals: see setLogoff
   // Threads: see setLogoff
   // ----------------------------------------------------------------------------
   */
   public void userLoggedOff( String clientSessionId ) {
      int mySessionNum;
      mySessionNum = locateSessionEntry( clientSessionId );
      if (mySessionNum < poolElementCount) {
         setLogoff( mySessionNum, "userLoggedOff" );
	  }
      else {
         System.out.println( "Scheduler: logoff for session " + clientSessionId + " when no session exists" );
      }
   } // userLoggedOff

   /** ----------------------------------------------------------------------------
   // setCommand:
   //
   // Locate any existing entry for the client, use that socket if found. If
   // not allocate them a new one.
   // Write the request, the thread reader will store it into the buffer for
   // this session object.
   // Input:
   //  String SessionID - tomcat client session id
   //  StringBuffer commandStringBuffer - the preformatted command
   //  String userName - user name for login to server if needed
   //  String userPassword - user password for login to server if needed
   // Returns:
   //  0 = command sent
   //  1 = no free connections in the pool
   //  2 = logon error
   //  3 = retry, slot was released before we got to the write
   //  4 = the command string passed doesn't seem to be an API header
   //  5 = socket IO error, server application may be down
   // Globals: N/A
   // Threads: note: the response tothe request will trigger the appropriate
   //          socket read thread for this session to store response data.
   // ----------------------------------------------------------------------------
   */
   public int setCommand( String SessionId, StringBuffer commandStringBuffer, String userName, String userPassword ) {
       int mySessionNum;
	   String commandString;
	   String sanityAPICheck;

       // if the user has an existing connection, re-use it
       mySessionNum = locateSessionEntry( SessionId );
       if (mySessionNum > poolElementCount) {
	       // if not allocate a new one and logon with username/password, check response
           mySessionNum = allocateSessionEntry( SessionId, userName, userPassword );
           if (mySessionNum == (poolElementCount + 1)) {  // no free connections
               return 1;
           }
           else if (mySessionNum == (poolElementCount + 99)) { // logon error
               return 2;
           }
           else if (mySessionNum >= poolElementCount) { // shouldn't be possible, but no sessions found
               return 1;
           }
       }

       // Convert it to a String, we want to do checks before the write
       commandString = commandStringBuffer.toString();

       // Minimal sanity checking before we sent it
       if (commandString.length() < API_HEADER_LEN) {
          System.out.println( "Scheduler: *Error* command buffer to small, fix the code" );
          System.out.println( "Scheduler: bufferdump=" + commandString );
          return 4;     // not a legal API header
       }
       sanityAPICheck = commandString.substring(0, 4);    // Check API* header flag
       if ( !(sanityAPICheck.equals("API*")) ) {
          System.out.println( "Scheduler: *Error* illegal API header found (" + sanityAPICheck + ") in setCommand, fix the code" );
          System.out.println( "Scheduler: bufferdump=" + commandString );
          return 4;     // not a legal API header
       }
       sanityAPICheck = commandString.substring(15, 24);   // check api version number
       if ( !(sanityAPICheck.equals("000100001")) ) {
          System.out.println( "Scheduler: *Error* illegal API version (" + sanityAPICheck + ") in setCommand, fix the code" );
          System.out.println( "Scheduler: bufferdump=" + commandString );
          return 4;     // not a legal API header
       }

       // write the command to the server unless someone grabbed our session
       // while we were doing stuff in here.
       if (connectionArray[mySessionNum].sessionId.equals( SessionId )) {
          connectionArray[mySessionNum].inUse = true;
          connectionArray[mySessionNum].eraseReadBuffer( "setCommand (SCHEDPoolBean)" );
          try {
             connectionArray[mySessionNum].writeDataStream( commandString );
          }
          catch (Exception e) {
             System.out.println( "Scheduler: *Error* socket write error in setCommand, closing connection" );
             connectionArray[mySessionNum].disConnect();
             return 5;
          }
          return 0;
       }
       else {
          return 3;   // someone stole our session
       }
   } // setCommand

   /** ---------------------------------------------------------------------------
   // getResponseLine:
   //
   // Return response lines one by one, null when no more. Utilise the fact we
   // know the server writes a ^ at each newline position, we parse for those.
   // Note: Skips over the API header area.
   // Input: the clients tomcat/jsp sessionid to index into the table
   // Output: one logical line from the buffer is returned
   // Globals: session data buffer is rolled down or emptied as lines are read
   // Threads: N/A
   // ----------------------------------------------------------------------------
   */
   public String getResponseLine( String clientSessionId ) {
      String localCopy, ServerData;
	  int tableEntry, i;
	  String testForHeader;
      Date workTime;

	  // find the clients data buffer in the connection pool by searching
	  // for the entry matching their tomcat session id.
      tableEntry = locateSessionEntry( clientSessionId );
	  if (tableEntry > poolElementCount) {        // not found
         localCopy = "";
         System.out.println( "Scheduler: *Warning* data line read request when no matching session for " + clientSessionId );
	  }
	  else {
        // and original code from when the socket read thread was used
        workTime = new Date();
        connectionArray[tableEntry].lastReferenceTime = workTime.getMinutes();
        connectionArray[tableEntry].waitOnData();
        // check if we need to skip the API header block
        if ( (connectionArray[tableEntry].sessionDataBuffer.length()) > API_HEADER_LEN) {
           testForHeader = connectionArray[tableEntry].sessionDataBuffer.substring(0,4);
           if (testForHeader.equals("API*")) {
                 connectionArray[tableEntry].sessionDataBuffer =
                         connectionArray[tableEntry].sessionDataBuffer.substring(
                         (API_HEADER_LEN+1), connectionArray[tableEntry].sessionDataBuffer.length() );
           }
        }
		// We know we are only working with data lines now
        try {
           i = connectionArray[tableEntry].sessionDataBuffer.indexOf('^');
           localCopy = connectionArray[tableEntry].sessionDataBuffer.substring( 0, i );
           // syncronize moveing data down as the reader may still be adding data to the buffer
           synchronized(connectionArray[tableEntry].sessionDataBuffer) {
              // if data left, move data down
              if (connectionArray[tableEntry].sessionDataBuffer.length() > (i+1) ) {
                 connectionArray[tableEntry].sessionDataBuffer =
                         connectionArray[tableEntry].sessionDataBuffer.substring(
                         (i+1), connectionArray[tableEntry].sessionDataBuffer.length()
                 );
              }
              // else if no real data left 0 the buffer
              else {
                 connectionArray[tableEntry].sessionDataBuffer = ""; // read it all, flush out last line read (still in buffer)
              }
           }  // syncronized block
        }
        catch (IndexOutOfBoundsException e2) {
            connectionArray[tableEntry].eraseReadBuffer( "getResponseLine (index out of bounds trap)" );
            localCopy = "";
        }
     } // else

     return localCopy;
   } // getResponseLine

   /** ----------------------------------------------------------------------------
   // getResponseBuffer:
   //
   // Return the entire reponse buffer block to the client. It must parse it
   // in the client code.
   // Note: returns the ENTIRE data buffer including the API header area
   //       if present, that the client needs to parse also.
   // Input: the clients tomcat/jsp sessionid to index into the table
   // Output: the contents of the session data buffer
   // Globals: session data buffer is erased after the request is actioned
   // Threads: N/A
   // ----------------------------------------------------------------------------
   */
   public String getResponseBuffer( String clientSessionId ) {
	  int tableEntry;
      String localCopy, ServerData;
      Date workTime;

	  // find the clients data buffer in the connection pool by searching
	  // for the entry matching their tomcat session id.
      tableEntry = locateSessionEntry( clientSessionId );
	  if (tableEntry > poolElementCount) {        // not found
         localCopy = "";
	  }
	  else {
         // and original code from when the socket read thread was used
         workTime = new Date();
         connectionArray[tableEntry].lastReferenceTime = workTime.getMinutes();
         connectionArray[tableEntry].waitOnData();
	     // save the data locally before we clear out the pool entry
		 localCopy = connectionArray[tableEntry].sessionDataBuffer;
		 connectionArray[tableEntry].sessionDataBuffer = ""; // no eraseReadBuffer, we don't want alert for this
      }
	  return localCopy;
   } // getResponseBuffer

   /** ----------------------------------------------------------------------------
   // getResponseCode:
   //
   // return the response code in the current API header.
   // Input: the clients tomcat/jsp sessionid to index into the table
   // Output: see notes below
   // Globals: N/A
   // Threads: N/A
   //
   // Notes:
   // If the buffer doesn't actually contain an API header return "+++".
   // If the buffer doesn't exist for the sessionid return "---".
   // Else return the three byte response code
   // -----------------------------------------------------------------------------
   */
   public String getResponseCode( String sesid ) {
      int tableEntry;
      String localCopy, ServerData;
      tableEntry = locateSessionEntry( sesid );
      if (tableEntry > poolElementCount) {        // not found
         localCopy = "---";                       // set to OK code, client will have an empty display
         System.out.println( "Scheduler: *Warning* resp code request when no matching session for " + sesid );
      }
      else {
        connectionArray[tableEntry].waitOnData();
        // and original code from when the socket read thread was used
         if (connectionArray[tableEntry].sessionDataBuffer.length() < 39) {
             localCopy = "+++";   // not an API header (too small), return +++
         }
         else {
            localCopy = connectionArray[tableEntry].sessionDataBuffer.substring( 0, 4 );
            if (localCopy.equals("API*")) {   // it is an API header
                localCopy = connectionArray[tableEntry].sessionDataBuffer.substring( 35, 38 );
            }
            else {
                localCopy = "+++";   // not an API header, return 999
            }
         }
      }
      return localCopy;
   } // getResponseCode

   /** ----------------------------------------------------------------------------
   // getSessionMax:
   //
   // return the number of pool entries allocated
   // Input: N/A
   // Output: The number of pool elements allocated to the pool
   // Globals: N/A
   // Threads: N/A
   // -----------------------------------------------------------------------------
   */
   public int getSessionMax() {
      return poolElementCount;
   } // getSessionMax

   /** ----------------------------------------------------------------------------
   // getSocketInfo( int entryNum ):
   //
   // Return one line describing the socket connection status.
   // Input: the socket entry number to check
   // Output: A string containing the info
   // Globals: N/A
   // Threads: N/A
   // -----------------------------------------------------------------------------
   */
   private String getSocketInfo( int entryNum ) {
	  String workLine = "";
      StringBuffer localLine = new StringBuffer();

	  try {
         workLine = connectionArray[entryNum].hostname;
         while (workLine.length() < 30) { workLine = workLine + " "; }
         localLine.append( workLine ).append( " " );
         workLine = "" + connectionArray[entryNum].portnum;
	     while (workLine.length() < 6) { workLine = workLine + " "; }
	     localLine.append( workLine );
         if (connectionArray[entryNum].isConnected) {
            if (connectionArray[entryNum].disConnectQueued) {
               localLine.append( " Disconnecting  " );
            } else if (connectionArray[entryNum].s != null) {
               localLine.append( " Connected      " );
            } else {
               localLine.append( " Connection-err " );
            }
         } else {
            localLine.append( " Disconnected   " );
         } // if isConnected
         if (connectionArray[entryNum].threadtask != null) {
            localLine.append( " Thread-task-defined   " );
         } else {
            localLine.append( " Thread-task-undefined " );
         } // threadTask check
         if (connectionArray[entryNum].inUse) {
            localLine.append( " in-use " ).append( connectionArray[entryNum].sessionId );
         } else {
   	        localLine.append( " spare " );
         } // inUse check
	  } catch ( IndexOutOfBoundsException e ) {
         localLine.append( "Error, the socket entry number requested is out of bounds, fix your code" );
	  }

      return (localLine.toString());
   } // getSocketInfo

   // ----------------------------------------------------------------------------
   // ----------------------------------------------------------------------------

/* *********************************************************************************
   connectionPoolObject: Internal class:
   This class describes our connection objects, the socket connections,
   threads, buffer areas used etc.
   ********************************************************************************* */
   // Describe what we have in our connection pool objects
   private class connectionPoolObject {
       String sessionId = "";         // client session id using the connection
       boolean inUse = false;         // actively doing IO, true or false
       boolean isConnected = false;   // is socket connected, true or false
       Socket s;                      // socket
       PrintWriter out;               // output stream
       BufferedReader in;             // input stream
       String sessionDataBuffer = ""; // input data buffer area
       Thread threadtask;             // input stream reader thread task
       String hostname = "localhost"; // hostname to connect to
       int    portnum = 9002;         // port number to connect to
       int    lastReferenceTime = 61; // minute in which it was last used
       int    replyOutStanding = 0;   // default is no reply being waited on
       private TimerTask SLEEPthreadtask;        // used to do a sleep of 0.333 secs
	   public boolean disConnectQueued; // errors in reader thread passed up the chain

        // -----------------------------------------------------------------------
        // Constructor
        // connectionPoolObject connectionPoolObject():
		//
		// Create a connectionPoolObject connecting to localhost on port 9002.
        // Input: hostname and port number to use, both String variables
        // Output: creates a connectionPoolObject, and connects the socket to
        //         the requested host.
        // Globals: N/A
        // Threads: each connectionPoolObject created starts a reader thread
        //          for the socket.
        // -----------------------------------------------------------------------
        public connectionPoolObject() throws IOException {
          this.hostname = "localhost";
          this.portnum = 9002;
          this.threadtask = null;
          this.replyOutStanding = 0;
          this.SLEEPthreadtask = null;
          this.disConnectQueued = false;
          if (reConnect()) {
               // NOP
          }
          if ((in == null) || (out == null) || (s == null)) {
             System.out.println( "Scheduler: *Error* connectPoolObject constructor failed to initialise socket" );
          }
        } // contructor

        // -----------------------------------------------------------------------
        // Constructor
        // connectionPoolObject connectionPoolObject( String useHost, int usePort ):
        //
        // Create a connectionPoolObject connecting to useHost on usePort.
        // Input: hostname and port number to use, both String variables
        // Output: creates a connectionPoolObject, and connects the socket to
        //         the requested host.
        // Globals: N/A
        // Threads: each connectionPoolObject created starts a reader thread
        //          for the socket.
        // -----------------------------------------------------------------------
        public connectionPoolObject( String useHost, int usePort ) throws IOException {
          this.hostname = useHost;
          this.portnum = usePort;
          this.threadtask = null;
          this.replyOutStanding = 0;
          this.SLEEPthreadtask = null;
          this.disConnectQueued = false;
          if (reConnect()) {
              // NOP
           }
           if ((in == null) || (out == null) || (s == null)) {
              System.out.println( "Scheduler: *Error* connectPoolObject constructor failed to initialise socket" );
           }
        } // constructor

        // -----------------------------------------------------------------------
        // reConnect: INTERNAL USE ONLY
        //
        // Will creata a new socket connection to the objects target host and
        // port, and start a reader thread for the socket.
        // Note: creates the threader thread if needed, else just starts the
        //       existing thread again.
        // Input: N/A 
        // Output: N/A
        // Globals: N/A
        // Threads: Creates a socket read thread if it needs to. Starts the
        //          socket read thread.
        // -----------------------------------------------------------------------
        public boolean reConnect() {
           try {
              System.out.println( "Scheduler: attempting to connect to host '" + hostname + "' on port " + portnum );
              if (s != null) { s = null; }   // try to destroy any previous connection
              s = new Socket(hostname,portnum);
              out = new PrintWriter(s.getOutputStream(),true);
              in = new BufferedReader(new InputStreamReader(s.getInputStream()));
              isConnected = true;
              sessionId = "";
              sessionDataBuffer = "";
              inUse = false;
              disConnectQueued = false;
              if (threadtask == null) {
                threadtask = new Thread() {
                                         public void run() {
                                            String ServerData;
                                            int gotEndOfBuffer;
                                            try {
                                                // TODO, change to stringbuffer so append can  be used
                                                gotEndOfBuffer = 0;
                                                while ( (ServerData = in.readLine()) != null ) {
                                                   synchronized(sessionDataBuffer) {
                                                       // TODO, change to stringbuffer so append can  be used
                                                       sessionDataBuffer = sessionDataBuffer + ServerData;
                                                       gotEndOfBuffer = sessionDataBuffer.indexOf( "\n" );
                                                       if (!(gotEndOfBuffer <= 0)) { replyOutStanding = 0; }
                                                   }
                                                   yield();
                                                }
                                                // make the assumption a null is the remote connection is no more
                                                System.out.println( "Scheduler: *Error* server socket closed by remote host, disconnect queued" );
                                                disConnectQueued = true;
                                                sessionId = "";   // stop existing users using it
                                                yield();
                                            } catch (IOException e) {
                                               System.out.println( "Scheduler: *Error* in socket read thread, stopping connection" );
                                               disConnect();
                                            }
                                         }; // run
                                     }; // new
			  }; // if threadtask = null
              threadtask.start();
              System.out.println( "Scheduler: connected, and reader thread launched" );

              // The server writes out a connection banner, read and discard it
              System.out.println( "Scheduler: connected - " + sessionDataBuffer );
			  eraseReadBuffer( "reConnect (SCHEDPoolBean)" );
              return true;
           }
           catch (IOException e) {
     		  isConnected = false;
              disConnect();
              System.out.println( "Scheduler: *Error* Unable to contact " + hostname + " on port " + portnum );
			  return false;
           }
		} // reConnect

        // -----------------------------------------------------------------------
        // disConnect: INTERNAL USE ONLY
        //
        // Disconnects an established socket connection, stops the reader
        // thread and resets the objecy flags to show the connection is no
        // longer being used.
        // Input: N/A 
        // Output: N/A
        // Globals: N/A
        // Threads: Stops the socket read thread for this object.
        // -----------------------------------------------------------------------
		public void disConnect() {
           try {
               disConnectQueued = false;
			   if (threadtask != null) { threadtask.stop(); } // stop thread task
               if (s != null) { s.close(); s = null; }  // closes in & out copies also
     		   inUse = false;
     		   isConnected = false;
     		   in = null;
     		   out = null;
               eraseReadBuffer( "disConnect" );
               sessionId = "";
           }
           catch (IOException e2) {
              System.out.println( "Scheduler: Errors closing socket, assuming it will die");
           }
		} // disConnect

        // -----------------------------------------------------------------------
        // checkConnection: INTERNAL USE ONLY
        //
        // Checks to ensure a connection has not dropped, will attempt to
        // reconnect it if it has.
        // Note: implemented as JSP just won't seem to run the constructors for
        //       the class, so used to force a connection. Have also changed
        //       all timer code that checks for dead connections to use this.
        // Input: N/A 
        // Output: N/A
        // Globals: N/A
        // Threads: Stops the socket read thread for this object.
        //
        // Notes: changed to return 1 if OK, 0 if not. This allows procs
        //        higher up the chain to not have to bother retrying all
        //        pool entries if it is determined that the first is not
        //        connectable.
        // -----------------------------------------------------------------------
        public int checkConnection() {
           if (!(isConnected)) {
              System.out.println( "Scheduler: *Warning* dead connection found, restarting it");
              if (reConnect()) {
                 System.out.println( "Scheduler: restarted dead connection");
                 return( 1 );
              }
              else {
                 System.out.println( "Scheduler: *Error* Unable to restart dead connection");
                 return( 0 );
              }
           }
           // If already connected no problem
           return( 1 );
        } // checkConnection

        // -----------------------------------------------------------------------
        // writeDataStream: INTERNAL USE ONLY
        //
        // Writes a data string to this objects out socket.
        // Input: a String
        // Output: returns True if OK, false if we failed
        // Globals: N/A
        // Threads: N/A
        // -----------------------------------------------------------------------
        public synchronized boolean writeDataStream( String myDataStream ) {
           Date workTime = new Date();
		   this.lastReferenceTime = workTime.getMinutes();
           if (disConnectQueued) {
              System.out.println("Scheduler: *warning* pre-emptive socket close in writeDataStream, socket queued for disconnect");
              disConnect();
              return false;
           }
           if (out == null) {
              System.out.println("Scheduler: *Error* output socket is null, in writeDataStream, NO RECOVERY");
              disConnect();
              return false;
           }
           eraseReadBuffer( "writeDataStream, flushing old data" );  // a new request, discard any old data
           try {
              out.println( myDataStream );
              this.replyOutStanding = 1;
           }
           catch (Exception e) {
              System.out.println( "Scheduler: *Error* socket write error in writeDataStream, closing connection" );
              disConnect();
              return false;
           }
           return true;
        } // writeDataStream

        // -----------------------------------------------------------------------
        // eraseReadBuffer: INTERNAL USE ONLY
        //
        // Clears a connection objects read buffer.
        // Input: N/A
        // Output: N/A
        // Globals: N/A
        // Threads: N/A
        // -----------------------------------------------------------------------
        public void eraseReadBuffer( String descText ) {
           if (sessionDataBuffer.length() > 0) {
              System.out.println( "Scheduler: *warning* flushing read buffer for " + descText +
                                  " while " + sessionDataBuffer.length() + " bytes are unread" );
              System.out.println( "Scheduler: *warning* buffer contents to flush = " + sessionDataBuffer );
           }
           sessionDataBuffer = "";    // Clear the contents
           this.replyOutStanding = 0;
        } // eraseReadBuffer

        // -----------------------------------------------------------------------
        // setSessionOwner: INTERNAL USE ONLY
        //
        // Set the tomcat sessionid allocated to this sockect connection.
        // Input: a valid tomcat sessionid
        // Output: N/A
        // Globals: N/A
        // Threads: N/A
        // -----------------------------------------------------------------------
        public void setSessionOwner( String sesid ) {
           synchronized( sessionId ) {
              sessionId = sesid;
           }
        } // setSessionOwner

        // -----------------------------------------------------------------------
        // waitOnData: INTERNAL USE ONLY
        //
        // Wait until there is some data in the buffer.
        // I've tried a lot of stuff here, wait/notifies until a newline in the buffer,
        // wait/notify until theres anything in the buffer ets and just could not
        // get it to play the way I wanted.
        // This solution, to invoke a thread that sleeps for 0.333 seconds and wait
        // on in completing is the best working solution so far.
        //
        // We ONLY WAIT is there is no data in the buffer, ie: we don't wait on
        // every socket connection we manage, as if we have to wait a little
        // while for one, it gives the other reader threads time to get stuff
        // into their buffers as well.
        //
        // Input: N/A
        // Output: N/A
        // Globals: N/A
        // Threads: Starts and joins one, thread doesn't exist we we leave.
		//
        // Notes: added the maxWait check as if the remote server goes away
        //        after we posted the write we would hang here forever.
        // -----------------------------------------------------------------------
        public synchronized void waitOnData() {
             int maxWait = 0;
             // Only wait if we know we have a reply outstanding
             if (replyOutStanding == 1) {
                // and only wait if we are connected
                if (isConnected) {
                   maxWait = 0;
                   while ( (sessionDataBuffer.length() <= 1) && (maxWait < 15) ) {
                      try {
                         Thread tempwait = new waitForInterval();  // default of 0.3 seconds wait
                         tempwait.join();     // wait until the thread completes
                         maxWait++;
                      } catch (InterruptedException e) {
                         // ignore
                      }
                   } // while
                }
                replyOutStanding = 0;
             }
        } // waitOnData

   } // connectionPoolObject

   // **************************************************************
   // SUBCLASS USED:
   // This is a bit painfull, but I need a way to make the mainline
   // wait for nn seconds, and java doesn't seem to have a delay 
   // function outside a thread.
   // So where the mainline needs to deleay it will need to create
   // an instance of this and 'join' it.
   //
   // See the above waitOnData function on how this can be used.
   //
   // If called with the default constructor will wait 0.333 secs.
   // Can also be called with a constructor that accepts millisecs to use.
   // **************************************************************
   public class waitForInterval extends Thread {
      private long numMilliSecsToWait;

      waitForInterval() {
         numMilliSecsToWait = 333;  // default is 0.333 secs
         try {
            this.sleep( numMilliSecsToWait );
         } catch (InterruptedException e) {
            // ignore
         }
      } // waitForInterval default constructor

      waitForInterval( int numMilliSecs ) {
         numMilliSecsToWait = numMilliSecs;  // use whats provided
         try {
            this.sleep( numMilliSecsToWait );
         } catch (InterruptedException e) {
            // ignore
         }
      } // waitForInterval overridden constructor

   } // waitForInterval

} // SCHEDPoolBean
%>
