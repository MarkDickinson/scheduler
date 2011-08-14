<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Final//EN">
<%-- 
  This is a sample error page.

  You can direct all JSP scripts to use a common error
  page by including the erropage directive at the start
  of each jsp page, and example to trigger this one is
     <%@ page errorPage = "error_page_sample.jsp" %>

  A key thing to note about this method is that the
  error page has access to the environment of the page
  that triggered the error condition, for example the
  URI and filename returned from request. calls will be
  that of the original page that had the error. This
  allows you to display a common error page that can
  contain truely meaningfull information on where it
  was triggered.

  IMPORTANT: you must have the page statement
           isErrorPage="true"
  if this is to be treated as the error page.

  NOTE: we write the error to the server log file from
  this error page. Any customised error page you write
  should do the same thing, or you could have a hard
  time resolving the error.
--%>
<%@ page isErrorPage="true" %>
<%@ page import="java.io.*" %>
<%@ page import="java.util.Enumeration" %>
 
<%-- A procedure to produce a formatted error message that we
     will be writing to the JSP server log file.
--%>
<%!
   String showError (HttpServletRequest request, Throwable ex) throws IOException {
      StringBuffer err = new StringBuffer();
      err.append("Uncaught runtime error --\n");
      err.append("    URI: ");
      err.append(request.getRequestURI()).append("\n");
      err.append("Extra Path: ");
      err.append(request.getPathInfo()).append("\n");
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      PrintWriter pw = new PrintWriter (baos, true);
      ex.printStackTrace(pw);
      err.append("    Stack: ");
      err.append(baos.toString()).append("\n");
      baos.close();
      // save the method and query string
      err.append("Request Method:");
      err.append(request.getMethod());
      err.append("\n  Query String:");
      err.append(request.getQueryString());
      // report the parameters we got also
      Enumeration requestNames = request.getParameterNames();
      while (requestNames.hasMoreElements()) {
         String   Name   = (String)requestNames.nextElement();
         String[] Values = request.getParameterValues(Name);
         for (int count = 0; count < Values.length; count++ ) {
            err.append(Name)
               .append(" = ")
               .append(Values[count]).append("\n");
         } // end for
      }    // end while
      return(err.toString());
   }
%>

<%-- We need to set the response code to indicate there was
     a server error.
     I'm not sure, but I think if we just send a page as normal
     then because we have trapped the error and sent a valid page
     it could response with the normal 200 OK response; which would
     be jolly good except for those applications that do health
     check polling. We should return the true error code.
--%>
<%
   // Set the server response code to show a server processing error has occurred
   response.setStatus( response.SC_INTERNAL_SERVER_ERROR );
%>

<HTML>
<STYLE>
.errorblock {
  position: relative; border: 1px; margin 1px;
  background-color: #eeeeee; text-align: left;
  font-family: Arial, Helvetica, sans-serif;
  font-size: 9pt; color: #000000;
}
</STYLE>
<BODY>
<%
   application.log("*********** Fatal code error ************\n");
   // This code calls the error formatter defined above to
   // format the error message, and write it to the server
   // log file so a history of the error is kept.
   String error = showError( request, exception );
   application.log( error );
   // Log the environment variables as well

   // end the error message block
   application.log("******** End code error dump block *********\n");
%>

<!--
   Well all the JSP code to log and report the error has
   been done, now we need to build the response the user
   will see.
   We will throw in some meaningfull variables from the
   JSP environments as well.
-->
<p>Your request could not be processed due to an error on the server.</p>
<DIV CLASS="errorblock">
<b>Error Details:</b><BR>
Error in: <%= request.getRequestURI() %><BR>
Reason  : <%= exception.getMessage() %>
</DIV>
<p>
The error event has been recorded in the system logs
for later analysis by support staff.
</p>
<p>
If the problem persists please contact the help desk
on 000-0000 and raise a fault with the following details
<ul>
<li>describe what you were trying to do</li>
<li>the time of the last occurence</li>
<li>the two error detail fields listed above</li>.
</ul>
</p>
<p>
Please wait a few minutes before retrying the request
(<i>time to grab a coffee</i>)<BR>
We apologise for any inconvenience caused by this issue.
</p>
</BODY>
</HTML>
