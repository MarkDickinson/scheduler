<%--
  page_refresh_code.jsp

  There are a few pages that have a dropdown box to select an automatic
  page refresh time. This code is shared amongst them to create the
  dropdown box and ensure the correct entry is selected.
  It also created the javascript code to repost the page when the
  value of the dropdown box changes, this is required for the
  server app to read the new value and set it in the page refresh
  header when the page is resent to the client.
--%>
<SCRIPT LANGUAGE="JavaScript">
function refreshtime_change() {
   newtimeindex = this.refreshtime.selectedIndex;
   if (newtimeindex == 1) { newtimetext = "2 minutes"; }
   else if (newtimeindex == 2) { newtimetext = "5 minutes"; }
   else if (newtimeindex == 3) { newtimetext = "10 minutes"; }
   else { newtimetext = "Never automatically"; }
<%   
   String newBaseURL = request.getScheme() + "://" + request.getServerName() + ":8080" +
                       request.getRequestURI() + "?pagename=" + pageRequestType + "&refreshtime=";
   out.println( "newurl = \"" + newBaseURL  + "\" + newtimetext;" );
%>
  location.href=newurl;
}
</SCRIPT>

Last Refreshed at
<%= new java.util.Date() %>
<br>
<%
   out.println( "<table align=\"right\" cellpadding=\"0\" cellborder=\"0\" border=\"0\"><tr><td>" );
   out.println( "Automatic refresh interval : <select name=\"refreshtime\" onChange=\"refreshtime_change()\">" );
   String refreshIntvl = request.getParameter("refreshtime");
   if (refreshIntvl == null) {
      out.println( "<option value=\"Never automatically\">Never automatically</option>" );
      out.println( "<option value=\"2 minutes\">2 minutes</option>" );
      out.println( "<option value=\"5 minutes\">5 minutes</option>" );
      out.println( "<option value=\"10 minutes\">10 minutes</option>" );
   } else {
      if (refreshIntvl.equals("Never automatically")) {
         out.println( "<option selected value=\"Never automatically\">Never automatically</option>" );
      } else {
         out.println( "<option value=\"Never automatically\">Never automatically</option>" );
      }
      if (refreshIntvl.equals("2 minutes")) {
         out.println( "<option selected value=\"2 minutes\">2 minutes</option>" );
      } else {
         out.println( "<option value=\"2 minutes\">2 minutes</option>" );
      }
      if (refreshIntvl.equals("5 minutes")) {
         out.println( "<option selected value=\"5 minutes\">5 minutes</option>" );
      } else {
         out.println( "<option value=\"5 minutes\">5 minutes</option>" );
      }
      if (refreshIntvl.equals("10 minutes")) {
         out.println( "<option selected value=\"10 minutes\">10 minutes</option>" );
      } else {
         out.println( "<option value=\"10 minutes\">10 minutes</option>" );
      }
   }
   out.println( "</select>" );
   out.println( "</td></tr></table><br><br>" );
%>
