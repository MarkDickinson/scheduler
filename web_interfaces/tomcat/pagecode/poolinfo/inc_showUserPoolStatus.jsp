<%--
   inc_showUserPoolStatus.jsp

   Show the status of the user pool belonging to this application.
   This is an admin/support screen.
--%>
<%
   String dataLine;
   int zz = userPool.getUserCount();
   out.println( "<p>There are " + zz + " user sessions created out of a maximum of " +
                userPool.getUserMax() + " possible sessions.</p>" );
   out.println("<H2>User pool status</H2>");
   out.println( "<b><pre>   #   Session Id                      User Name</pre></b>" );
   out.println( "<table border=\"1\" cellpadding=\"5\"><tr><td><pre>" );
   for (int j = 0; j < zz; j++ ) {
         dataLine = " " + j;
         while (dataLine.length() < 4) { dataLine = dataLine + " "; }
         dataLine = dataLine + " " + userPool.userDetailsArray[j].clientSessionId + " " + userPool.userDetailsArray[j].userName + "     ";
         out.println( dataLine );
   } // for j
   out.println( "</pre></td></tr></table><br>End of user pool status display" );
%>
