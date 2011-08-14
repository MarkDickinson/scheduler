<%--
  standard_response_display.jsp

  This is pulled in by any procedure that is able to display its responses from
  the server scheduler, without the need for any special pre-processing or
  formatting. It will write the reponse as-is within a table border.
  If the response code is an error code the table will be red, that is the only
  concession to formatting made here.

  This should ideally be a called procedure rather than an include file,
  however the out variable is not defined if it is put into it's own class (and
  the session id needs to be passed. BASICALLY if in its own class it doesn't
  know about the jsp environment. I will try to get that working.
--%>
<%
	  String dataLine = "";
      // Check the response code
      String cmdRespCode = socketPool.getResponseCode( userPool.getUserContext(session.getId(), "HOSTNAME" ), session.getId() );
      if ( (!(cmdRespCode.equals("100"))) && (!(cmdRespCode.equals("+++"))) ) {   // not normal OK display text
         out.println( "<table border=\"1\" BGCOLOR=\"#FF0000\"><tr><td><pre>" +
                      userPool.getUserContext(session.getId(), "HOSTNAME" ) + ":**Error** " );
      } else {
         out.println( "<table border=\"1\"><tr><td><pre>" );
      }
      if ( (cmdRespCode.equals("100")) || (cmdRespCode.equals("+++")) || (cmdRespCode.equals("102")) ) {  
         dataLine = socketPool.getResponseLine( userPool.getUserContext(session.getId(), "HOSTNAME" ), session.getId() );
         while (dataLine != "") {
            out.println( dataLine );
            dataLine = socketPool.getResponseLine( userPool.getUserContext(session.getId(), "HOSTNAME" ), session.getId() );
         }
      }
      else if (cmdRespCode.equals("101")) {
         out.println( "Respcode=101, raw data returned from server. This is unexpected. Fix your code" );
      } else if (cmdRespCode.equals("---")) {
         out.println( userPool.getUserContext(session.getId(), "HOSTNAME" ) +
                      ":There is no longer a data buffer associated with your session for host '" +
                      userPool.getUserContext(session.getId(), "HOSTNAME" ) + "'" );
      }
      else {
         out.println( userPool.getUserContext(session.getId(), "HOSTNAME" ) +
                     ":Unrecognised response code (" + dataLine + ") contact support staff" );
      }
      out.println( "</pre></td></tr></table><br>");
%>
