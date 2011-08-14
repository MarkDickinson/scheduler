<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Final//EN">
<%-- -----------------------------------------------------------
  This sample page demonstrates the variables that are 
  available to a page.
  --------------------------------------------------------------
--%>
<%@ page import="java.util.Enumeration" %>
<%@ page import="java.util.Locale" %>
 
<%
   // The outPut StringBuffer is used extensively, create it now.
   StringBuffer outPut = new StringBuffer();
%>
<HTML>
<HEAD>
</HEAD>
<BODY>
<!--
  ====================================================================
  This sample page demonstrates the variables that are 
  available to a page.

  * The pageContext object allows you to reference the request and
    response objects.
    ie: this url is <jsp:expression>request.getRequestURI()</jsp:expression>
  * The session object is used to track information about a specific
    client while using stateless protocols such as HTTP
  * The application object has the same methods as the java ServletContext
    object. It lives from the time the page is loaded or recompiled 
    until the next server restart, page recomile ot jspDestroy is called
    for the page. Information stored in this object is available to
    any other object on the page. IT CAN ALSO communicate back to the
    server without using requests, usefull for sending log information
    back to the servers log for example.
  * The config object has the same methods and interfaces as the java
    ServletConfig object. Allows access to the jsp engine or servlet
    initialisation values, usefull for finding paths and file 
    locations for example.
  * The page (this) object is a reference to the entire page WITHIN
    THE CONTEXT OF THE PAGE. At times diring the JSP lifecycle it may
    not reference the page, but should always do so when the page is 
    being processed.
  * The exception object, is a wrapper containing an uncaught exception
    thrown from the previous page and the previous pages page errorPage
    tag was used. NOTE: to catch an exception within the current page
    refer to the datetime.jsp example that catches a divide by zero
    in a try block.
  ====================================================================
-->

<!-- 
Server information is obtainable through the application object
-->
<h1>
<%
request.getParameter("pagename");
%>
</h1>
<p>
This request has not yet been implemented. The following data is intended for
developers working on the code, you should ignore it.
</p>
<p>
All you need to know is, that at this time the requiest you attempted
is not supported by the application. Try again in a few <b>months</b> time.
</p>
<hr>
The JSP engine serving the page describes itself as <b>
<%= application.getServerInfo() %>
</b><br>
The filename of the URL 
<%= request.getRequestURI() %>
is 
<%= application.getRealPath(request.getRequestURI()) %>
<hr>


<!--
The browser specification expects a locale to be provided as part
of the http header stream. This can be usefull if you support
multiple languages on your site and wish to show different
pages as needed.
-->
<h1>Header information</h1>
<%--
The Locale header stuff just fails under Tomcat
If you want to use it use the hetHeader("Accept-Language")

<h2>Locale Header</h2>
Locale: <CODE><%= request.getLocale() %></CODE><BR>

<h2>Full locale list</h2>
Some browsers provide more than one locale, this is the full list.<BR>
<%
   Enumeration localeNames = request.getLocales();
   while (localeNames.hasMoreElements()) {
      String   Name   = (String)localeNames.nextElement();
      out.print(Name); out.print("<BR>");
   }
%>
--%>

<h2>Header Parameter names and data</h2>
<CODE>
<TABLE BORDER="0">
<%
   Enumeration headerNames = request.getHeaderNames(); // list of header names
   while (headerNames.hasMoreElements()) {
      String   Name   = (String)headerNames.nextElement();
      Enumeration headValues = request.getHeaders(Name);      // list of values for name
      while (headValues.hasMoreElements()) {
         String   headName   = (String)headValues.nextElement();
         outPut.append("<TR>")
               .append("<TD>").append(Name).append("</TD>")
               .append("<TD> = </TD>")
               .append("<TD>").append(headName).append("</TD>")
               .append("</TR>\n");
         Name = "&nbsp;";  // if more than one value for name blank it for table
      } // end while header values
   }    // end while header names
   out.print(outPut.toString());
   outPut.setLength(0);
%>
</TABLE>
</CODE>
<hr>



<!-- 
  The request object values are normally retrieved as parameter
  name and value pairs.
  NOTE: if there is more than one value (more than one occurence)
  of a parameter name what is returned is JSP-implementation
  dependant; the getParameter() is not guaranteed to work across
  all vendor platforms. To work around this you can try
  getParameterValues, which should return them all for all
  platforms that follow the JSP standard.
  The example below show how to get all the request information
  from a request.
  INFORMATION: If you know exactly what parameter name you want
  a value for you would probably just request the value for the
  specific name with request.getParameterValue(name) rather than
  enumarating through the entire list as we do in this example.
  IMPORTANT: using a String an + to concatenate data really
  slows down a java prgram as a new StringBuffer has to be created
  for every + operation to append the data. To avoid this try
  to always start off with a StringBuffer in the first place
  and use the append operation.
-->
<h1>Request Information</h1>

<h2>Request MetaData</h2>
Request Method: <CODE><%= request.getMethod() %></CODE><BR>
Query String: <CODE><%= request.getQueryString() %></CODE>

<h2>Request Parameter names and data</h2>
<CODE>
<TABLE BORDER="0">
<%
   Enumeration requestNames = request.getParameterNames();
   while (requestNames.hasMoreElements()) {
      String   Name   = (String)requestNames.nextElement();
      String[] Values = request.getParameterValues(Name);
      for (int count = 0; count < Values.length; count++ ) {
         outPut.append("<TR>")
               .append("<TD>").append(Name).append("</TD>")
               .append("<TD> = </TD>")
               .append("<TD>").append(Values[count]).append("</TD>")
               .append("</TR>\n");
         Name = "&nbsp;";  // if more than one value for name blank it for table
      } // end for
   }    // end while
   out.print(outPut.toString());
   outPut.setLength(0);
%>
</TABLE>
</CODE>
<hr>


<%--
  Commented out, tomcat spits at the getCookie()
<!--
To make retrieving cookies easier an array of cookie elements
can be returned with the getCookie request methos as below.
-->
<h1>Cookies</h1>
<%
   Cookie[] cookieNames = request.getCookie();
%>
<hr>
--%>

<!--
A list of attributes can be derived from the request object and meta-data,
for example the SSL certificate used by the client (if the client is
connecting via SSl and IF your JSP engine supports retrieving the
attribute, some vendors don't support all attributes).
-->
<h1>Attributes</h1>
These are the attributes derived from the request and meta-data
<b>that the JSP engine supports</b>. There may have been others
that could be derives but different vendors JSP engines don't
necessarily support the full set available.<br>
<%
   Enumeration attributeNames = request.getAttributeNames();
   // Only list them if they are found.
   if (attributeNames.hasMoreElements()) {
      outPut.append("<TABLE>");
      while (attributeNames.hasMoreElements()) {
         String   Name   = (String)attributeNames.nextElement();
         String   Value = (String)request.getAttribute(Name);
         outPut.append("<TR><TD>").append(Name).append("</TD><TD>").append(Value).append("</TD></TR>");
      }
      outPut.append("</TABLE>");
      out.print(outPut.toString());
      outPut.setLength(0);
   } else {
      out.print("<BR><B>No attributes were available for this page</B><BR>");
   }
%>
<hr>
<H1>Client Session Information</H1>
<%
   outPut.append("Session key: ").append(session.getId()).append("<BR>");
   String[] sessionNames = session.getValueNames();
   if (sessionNames != null) {
      if (sessionNames.length > 0) {
         outPut.append("Session data for this page...");
         outPut.append("<TABLE>");
         for (int count = 0; count < sessionNames.length; count++ ) {
             outPut.append("<TR>")
                   .append("<TD>").append((String)sessionNames[count]).append("</TD>")
                   .append("<TD> = </TD>")
                   .append("<TD>").append(session.getValue((String)sessionNames[count])).append("</TD>")
                   .append("</TR>\n");
         }
         outPut.append("</TABLE>");
      } else {
         outPut.append("There are no additional session data entries for this page.");
      }
   } else { 
     outPut.append("There is no session data available.");
   }
   out.print(outPut.toString());
   outPut.setLength(0);
%>

<hr>
End of page.
</BODY>
</HTML>
