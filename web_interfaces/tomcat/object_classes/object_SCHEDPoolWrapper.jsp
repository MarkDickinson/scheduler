<%!
/* =================================================================================
  SCHEDPoolWrapper.java

  This is a wrapper class for the SCHEDPoolBean object. It allows us to manage
  multiple connection pool objects so we may have a connection pool object for
  every machine we are monitoring but still be able to access them through a 
  simple interface.

  It MUST use the same common interface methods as the SCHEDPoolBean class but
  each will require a hostname String to be passed as the first parameter.
  This is to minimise the code changes required from a single server model to
  a multiple server model, the only changes required will be to use the
  SCHEDPoolWrapper constructor and destructor rather than the SCHEDPoolBean one
  and to insert a hostname field in all the function calls.
  Of course there are a few additional steps, this class returns the number of
  array elements and provides a lookup of each hostname for the elements, as
  now we are doing server connection pooling as well as socket pooling this
  will allow callers to loop through each host the wrapper manages.

  I have also decided not to add a setCommand ALL to loop for every server
  as only the sched listall and alert listall will need it so to minimise
  application size this can remain in the page JSP code rather than being
  in every object created.

  Version History
  0.001-BETA 07Feb2004 MID Implemented the wrapper class 

  ==================================================================================
*/

/* *********************************************************************************
   ********************************************************************************* */
public class SCHEDPoolWrapper implements Serializable {

   // -----------------------------------------------------------------------------
   // Globals we require
   // -----------------------------------------------------------------------------
   private   SCHEDPoolBean[] SCHEDPoolBeanArray;
   private   int SCHEDpoolElementCount = 0;  // How many entries are in the table
   // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   // Change the below for your site
   // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   // int countOfHosts = 4;
   // String arrayOfHosts[] = { "vmhost1", "vmhost3", "vosprey2", "phoenix" };
   // int arrayOfPorts[] =    { 9002, 9002, 9002, 9002 };
   // int socketsPerHost = 5;
   int countOfHosts = 1;
   String arrayOfHosts[] = { "localhost" };
   int arrayOfPorts[] =    { 9002 };
   int socketsPerHost = 5;
   // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   // Change the above for your site
   // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

   /** -----------------------------------------------------------------------------
   // SCHEDPoolWrapper():
   //
   // Contructor: creates an array of SCHEDPoolBean connection pool array
   //             objects. What is created is hard coded at present.
   // Input: N/A
   // Output:  an array of connection pool object arrays, all connected to
   //          their appropriate host systems.
   // Globals: N/A while things are hard coded
   // Threads: None directly, but the SCHEDPoolBean objects created will all
   //          create multiple threads.
   // -----------------------------------------------------------------------------
   */
   public SCHEDPoolWrapper() {
	  // Allocate the connection pool wrappers, this also initialises the
	  // connection pool entries within the wrapper objects.
      int i;
      this.SCHEDPoolBeanArray = new SCHEDPoolBean[countOfHosts]; // countOfHosts elements in array
      for (i = 0; i < countOfHosts; i++ ) {
         this.SCHEDPoolBeanArray[i] = new SCHEDPoolBean( socketsPerHost, arrayOfHosts[i], arrayOfPorts[i] );
         if ( (SCHEDPoolBeanArray == null) || (SCHEDPoolBeanArray[i] == null) ) {
            System.out.println( "Scheduler: *Error* unable to create connection wrapper object number " + i );
         }
      }

      System.out.println( "Scheduler: Wrapper pool contains " + countOfHosts + " Host pool connection array objects");
   }  // SCHEDPoolWrapper constructor

   /** ----------------------------------------------------------------------------
   // SCHEDPoolWrapperDESTROY:
   //
   // Destructor
   //
   // Input: N/A
   // Output: N/A
   // Globals: All SCHEDPoolBeanObjects we manage will be disconnected.
   // Threads: All threads started by the SCHEDPoolBean objects will be stopped.
   // -----------------------------------------------------------------------------
   */
   public void SCHEDPoolWrapperDESTROY() {
      System.out.println( "Scheduler: Connection pool wrapper is terminating all managed connections" );
      for (int i = 0; i < countOfHosts; i++) { SCHEDPoolBeanArray[i].SCHEDPoolBeanDESTROY(); }
   } // destructor

   /** ----------------------------------------------------------------------------
   // locateHostEntry: PRIVATE
   //
   // Try to locate the wrapper entry for a connection pool.
   // Return > max entries possible if not found, else return the entry number.
   // Input: the host name
   // Output:
   //     countOfHosts+1 - no matching session exists
   //     n - the session number
   // Globals: N/A
   // Threads: N/A
   // -----------------------------------------------------------------------------
   */
   private int locateHostEntry( String hostname ) {
      for (int i = 0; i < countOfHosts; i++ ) {
         if (arrayOfHosts[i].equals(hostname)) { return i; }
      }
      return (countOfHosts + 1);
   } // locateHostEntry

   /** ----------------------------------------------------------------------------
   // setCommand:
   //
   // Input:
   //  String hostname - the remote host the command is destined for
   //  String SessionID - tomcat client session id
   //  StringBuffer commandStringBuffer - the preformatted command
   //  String userName - user name for login to server if needed
   //  String userPassword - user password for login to server if needed
   // Returns:
   //    from SCHEDPoolBean objects themselves
   //       0 = command sent
   //       1 = no free connections in the pool
   //       2 = logon error
   //       3 = retry, slot was released before we got to the write
   //       4 = the command string passed doesn't seem to be an API header
   //       5 = socket IO error, server application may be down
   //    from this method additionally
   //       6 = host is not known
   // Globals: N/A, no updates to globals
   // Threads: N/A, uses the SCHEDPoolBean threads
   // ----------------------------------------------------------------------------
   */
   public int setCommand( String hostname, String SessionId, StringBuffer commandStringBuffer,
                          String userName, String userPassword ) {
      int foundHostNum, retCode;
      foundHostNum = locateHostEntry( hostname );
      if (foundHostNum > countOfHosts) {
         retCode = 6; // host not known
      }
      else {
         retCode = SCHEDPoolBeanArray[foundHostNum].setCommand( SessionId, commandStringBuffer, userName, userPassword );
      }
      return retCode;
   } // setCommand

   /** ---------------------------------------------------------------------------
   // getResponseLine:
   //
   // Get the next logical response line from the socket entry the user has
   // for the current host.
   // Input: the hostname the data is to be read for and the clients tomcat/jsp
   //        sessionid for the SCHEDPoolBean to use to index into it's socket table
   // Output: one logical line from the buffer is returned
   // Globals:N/A, data modification only done within the SCHEDPoolBean object
   // Threads: N/A
   // ----------------------------------------------------------------------------
   */
   public String getResponseLine( String hostname, String clientSessionId ) {
      int foundHostNum;
      String localCopy;

      foundHostNum = locateHostEntry( hostname );
      if (foundHostNum > countOfHosts) {
         localCopy = ""; // host not known so no data
         System.out.println( "Scheduler: *warning* Host " + hostname + " not found in record read lookup" );
      }
      else {
         localCopy = SCHEDPoolBeanArray[foundHostNum].getResponseLine( clientSessionId );
      }
      return localCopy;
   } // getResponseLine

   /** ----------------------------------------------------------------------------
   // getResponseBuffer:
   //
   // Return the entire reponse buffer block to the client for the data buffer
   // for the clients entry associated with the hostname.
   // Input: the hostname the request is for and the clients tomcat/jsp sessionid
   //        so the SCHEDPoolbean entry can index into it's table
   // Output: the contents of the session data buffer
   // Globals: session data buffer is erased after the request is actioned
   // Threads: N/A
   // ----------------------------------------------------------------------------
   */
   public String getResponseBuffer( String hostname, String clientSessionId ) {
	  int foundHostNum;
      String localCopy;

      foundHostNum = locateHostEntry( hostname );
      if (foundHostNum > countOfHosts) {
         localCopy = ""; // host not known so no data
         System.out.println( "Scheduler: *warning* Host " + hostname + " not found in record read lookup" );
      }
      else {
         localCopy = SCHEDPoolBeanArray[foundHostNum].getResponseBuffer( clientSessionId );
      }
      return localCopy;
   } // getResponseBuffer

   /** ----------------------------------------------------------------------------
   // getResponseCode:
   //
   // return the response code in the current API header for the hostname
   // requested.
   // Input: the hostname for te buffer to be checked and the clients tomcat/jsp
   //        sessionid so the SCHEDPoolBean can index into it's tables
   // Output: see getResponseCode in the SCHEDPoolBean object.
   // Globals: N/A
   // Threads: N/A
   //
   // Notes:
   // If the buffer doesn't actually contain an API header return "+++".
   // If the buffer doesn't exist for the sessionid return "---".
   // Else return the three byte response code
   // -----------------------------------------------------------------------------
   */
   public String getResponseCode( String hostname, String sesid ) {
      int foundHostNum;
      String localCopy;

      foundHostNum = locateHostEntry( hostname );
      if (foundHostNum > countOfHosts) {
         localCopy = "---"; // host not known so no buffer found
         System.out.println( "Scheduler: *warning* Host " + hostname + " not found in record read lookup" );
      }
      else {
         localCopy = SCHEDPoolBeanArray[foundHostNum].getResponseCode( sesid );
      }
      return localCopy;
   } // getResponseCode

   /** ----------------------------------------------------------------------------
   // getSessionMax:
   //
   // return the number of pool entries allocated for the selected host.
   // Input:  The hostname to get the connection pool count for.
   // Output: The number of pool elements allocated to the pool for the
   //         selected host.
   // Globals: N/A
   // Threads: N/A
   // -----------------------------------------------------------------------------
   */
   public int getSessionMax( String hostname ) {
      int foundHostNum;
      foundHostNum = locateHostEntry( hostname );
      if (foundHostNum > countOfHosts) {
         return 0; // host not known so no pool entries
      }
      else {
         return SCHEDPoolBeanArray[foundHostNum].getSessionMax();
      }
   } // getSessionMax

   /** ----------------------------------------------------------------------------
   // allocateSession:
   //
   // Added to allow the login page to do a looping login of all servers.
   // See allocateSessionEntry in the SCHEDPoolBean for return codes.
   // -----------------------------------------------------------------------------
   */
   public int allocateSession( String hostname, String clientSessionId, String userName, String userPassword ) {
      int foundHostNum;
      foundHostNum = locateHostEntry( hostname );
      if (foundHostNum > countOfHosts) {
         return (countOfHosts + 99); // host not known so invalid login
      }
      else {
         return SCHEDPoolBeanArray[foundHostNum].allocateSessionEntry( clientSessionId, userName, userPassword );
      }
   } // allocateSession

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
   private String getSocketInfo( String hostname, int entryNum ) {
      int foundHostNum;
      String localCopy;
      String workLine;

      foundHostNum = locateHostEntry( hostname );
      if (foundHostNum > countOfHosts) {
         workLine = hostname;
         while (workLine.length() < 30) { workLine = workLine + " "; }
		 localCopy = workLine + " hostname is not defined in the connection pool.      ";
      }
      else {
         localCopy = SCHEDPoolBeanArray[foundHostNum].getSocketInfo( entryNum );
      }
      return localCopy;
   } // getSocketInfo

   // *****************************************************************************
   // The following are specific to the wrapper rather than being interfaces to
   // existing SCHEDPoolBean interfaces. They provide enough information for
   // the caller to map through this wrapper library to the SCHEDPoolBean
   // class.
   // *****************************************************************************

   /** ----------------------------------------------------------------------------
   // getHostMax:
   //
   // return the number of hosts we are managing connection pool objects for.
   // Input: N/A
   // Output: The number of hosts we have allocated connection pool objects for.
   // Globals: N/A
   // Threads: N/A
   //
   // Notes: this allows the caller to know how many objects we manage so they
   //        can loop through the connection pool.
   // -----------------------------------------------------------------------------
   */
   public int getHostMax() {
      return countOfHosts;
   } // getHostMax

   /** ----------------------------------------------------------------------------
   // getHostName:
   //
   // return the host name for the entry number requested.
   // Input:  The wrapper array entry number we want the hostname for
   // Output: The hostname if found, or an empty string if not found.
   // Globals: N/A
   // Threads: N/A
   //
   // Notes: this allows the caller to map to the correct connection pool entry
   //        as it provides the hostname required as the key.
   // -----------------------------------------------------------------------------
   */
   public String getHostName( int entryNum ) {
      String localCopy = "";
      if (entryNum < countOfHosts) {
         localCopy = arrayOfHosts[entryNum];
	  }
      return localCopy;   // will still be empty as initialised unless we found a host
   } // getHostName

   /** ----------------------------------------------------------------------------
   // LogoffAll:
   //
   // Logoff sessions for this JSP sessionid in all socket pools we manage.
   // Only called manually by the external application code if a user manually
   // logs off thier session.
   // Input:  The client tomcat/JSP session id
   // Output:  N/A
   // Globals: N/A
   // Threads: N/A
   // -----------------------------------------------------------------------------
   */
   public void LogoffAll( String clientSessionId ) {
      for (int i = 0; i < countOfHosts; i++) {
         SCHEDPoolBeanArray[i].userLoggedOff( clientSessionId );
      }
   } // LogoffAll

} // SCHEDPoolWrapper
%>
