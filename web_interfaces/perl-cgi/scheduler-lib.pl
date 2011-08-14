#!/usr/bin/perl
# Perl Routines to Specific to Mark Dickinsons Job Scheduler
#
# Copyright (c) 2002 Mark Dickinson
#

# -------------------- customise for your site ------------------
# The directory the scheduler cgi-bin files are located in
# Must be a legal path based on your httpd.conf cgi-bin path.
sub scheduler_HTTPD_Home {
  return "http://falcon/cgi-bin/system/scheduler";
}

# The default port number to be used to communicate with the schedulers
sub scheduler_port {
  return "9002";
}

# The location of the scheduler command program
sub scheduler_cmdprog {
  my $useport=&scheduler_port();
  return "/opt/dickinson/job_scheduler/bin/jobsched_cmd ${useport}";
}

# The entry added to httpd.conf to idendify where scheduler icons are
sub scheduler_icon_path {
   return "/scheduler-icons/";
}

# The file to store commands generated by the helpers for later review.
sub scheduler_helper_output_file {
   return "/var/tmp/sched_new_cmds";
}

sub scheduler_HtmlMail {
  return "<small><small>\nCopyright &copy; 2002-2011 Mark Dickinson.\n
<a href=\"mailto:mark\@localhost\">\n
<address>\nmark\@localhost\n</address>\n</a>\n
</small></small>\n";
}
# NOTE: Customisation changed for monitoring multiple systems
#       are at the end of the file. You should not alter
#       them if you are running this on a single system.

# ----------- end of customisation changes ----------------

sub scheduler_common_header() {
# Change this at YOUR OWN RISK.
# I have had a lot of trouble matching up the ", ' and \ fields.
# So any changes you may make here are NOT supported. I will just say
# put back in the distributed version !.
   local ($myname) = $_[0];
   local ($myurl) = &scheduler_HTTPD_Home().'/'.${myname};
   local ($refreshtime) = $_[1];
   print "<html><head>\n";
   print '<SCRIPT LANGUAGE="JavaScript">';
   print "\n<!-- Begin\n//\n//Automatic Page Refresh Script\n// parm1 file to jump 2, parm2 time to wait (thousands of secs)\n//\n";
   print "window.setTimeout(";
   print '"location.href=';
   print "'";
   print "${myurl}";
   print "'";
   print '",';
   print "${refreshtime}";
   print ')';
   print "\n// End -->\n</script>\n";
   #   print '<link rel=StyleSheet type="text/css" href="menubar_stylesheet.css">';
   print "<!-- Embedded styleshheet to avoid having to have someone who uses the\n";
   print "software have to copy style sheets from the cgi directory to the html directory -->\n";
   print '<style type="text/css">'; print "\n";
   print ".tbToolbar { \n";
   print "border-top:1px solid black; \n";
   print "border-bottom:1px solid black; \n";
   print "background-color: white;\n";
   print "width:auto%;\n";
   print "height:25px; \n";
   print "padding-top:2px; \n";
   print "padding-bottom:2px; \n";
   print "padding-left:15px; \n";
   print "margin: 0px;\n";
   print "color:#000000;\n";
   print "position:relative;\n";
   print "}\n";
   print ".tbSubMenu {\n";
   print "	border:1px solid black;\n";
   print "	background-color: white;\n";
   print "	color: black;\n";
   print "	padding:2px;\n";
   print "	display: none;\n";
   print "	cursor: hand;\n";
   print "	position:Absolute;\n";
   print "}\n";
   print ".tbItem { \n";
   print "	padding-left:2px;\n";
   print "	padding-right:2px;\n";
   print "	margin:2px;\n";
   print "	text-decoration: none;\n";
   print "	border:1px solid white;\n";
   print "}\n";
   print ".tbItemSelected { \n";
   print "	padding-left:2px;\n";
   print "	padding-right:2px;\n";
   print "	margin:2px;\n";
   print "	text-decoration: none;\n";
   print "	border:1px solid #c0c0c0;\n";
   print "	background-color:#f0f0f0;\n";
   print "	color:black;\n";
   print "	position:relative;\n";
   print "}\n";
   print ".tbItemLink { \n";
   print "	cursor:default; \n";
   print "	padding:0px; \n";
   print "	margin:0px;\n";
   print "}\n";
   print ".tbSubItemLink { \n";
   print "	cursor:select; \n";
   print "	padding:0px; \n";
   print "	margin:0px;\n";
   print "	min-width:80px;\n";
   print "	border:1px solid white;\n";
   print "	white-space: nowrap;\n";
   print "}\n";
   print ".tbSubItemLinkSelected { \n";
   print "	cursor:pointer; \n";
   print "	padding:0px; \n";
   print "	margin:0px;\n";
   print "	margin-right: auto;\n";
   print "	min-width:80px;\n";
   print "	border:1px solid #c0c0c0;\n";
   print "	background-color:#f0f0f0;\n";
   print "	white-space: nowrap;\n";
   print "}\n";
   print "</style>\n";
   print "\n";
   print '<script language="Javascript">'; print "\n";
   print "//\n//Menu Bar Functions\n//\n";
   print "var Toolbar='<DIV ID=";
   print '"Toolbar" class="tbToolbar" NOWRAP><table border=0><tr><!-- TB_ITEMS --></tr></table></DIV><!-- TB_SUBITEMS -->';
   print "'\n";
   print "var currentMenu=null\n";
   print "var currentItem=null\n";
   print "var focusTimer=null\n";
   print "var root='./'\n";
   print "function drawToolbar() { document.write(Toolbar) }\n";
   print "function addMenu(id,caption) {\n";
   print "    var tmpTag='<!-- TB_ITEMS -->'\n";
   print "    Toolbar=Toolbar.replace(tmpTag,'<TD id=";
   print '"IT_';
   print "'+id+'";
   print '" class="tbItem" NOWRAP><p class="tbItemLink" onmouseover="doAction(\\';
   print "''+id+'\\')";
   print '" onmousemove="setFocus(\\';
   print "''+id+'\\')";
   print '"';
   print ">'+caption+'</p></td><td width=";
   print '"5px">&nbsp;</td>';
   print "'+tmpTag)\n";
   print "     var tmpTag='<!-- TB_SUBITEMS -->'\n";
   print "     Toolbar=Toolbar.replace(tmpTag,'";
   print '<DIV id="TBS_';
   print "'+id+'";
   print '" class="tbSubMenu" onmousemove="setFocus(\\';
   print "''+id+'\\')";
   print '" NOWRAP><!-- TBS_';
   print "'+id+' --></div>'+tmpTag)\n";
   print "}\n";
   print "function addMenuItem(p,id,caption,turl,hint) {\n";
   print "     var tmpTag='<!-- TBS_'+p+' -->'\n";
   print "     Toolbar=Toolbar.replace(tmpTag,'<DIV id=";
   print '"';
   print "TBSI_'+id+'";
   print '" class="tbSubItemLink" onmouseover="showSelect(\\';
   print "''+id+'\\',\\''+hint+'\\')";
   print '" onmouseup="location=root+\\';
   print "''+turl+'\\'";
   print '" NOWRAP>';
   print "'+caption+'</div>'+tmpTag)\n";
   print "}";
   print "function addSubMenu(p,id,caption,url) {\n";
   print "     var tmpTag='<!-- TB_SUBITEMS -->'\n";
   print "     Toolbar=Toolbar.replace(tmpTag,'<DIV id=";
   print '"';
   print "TBSUB_'+p+'";
   print '" class="tbSubMenu" onmousemove="setFocus(\\';
   print "''+id+'\\')";
   print '"';
   print " NOWRAP><!-- TBS_'+id+' --></div>'+tmpTag)\n";
   print "}\n";
   print "function setFocus(id) {\n";
   print "    if (focusTimer) clearTimeout(focusTimer)\n";
   print "    focusTimer=setTimeout('doHide(";
   print '"';
   print "'+id+'";
   print '"';
   print ")',2000)\n";
   print "}\n";
   print "function doHide(id) {\n";
   print "    var o=document.getElementById('TBS_'+id)\n";
   print "    if (!o) var o=document.getElementById('TBSUB_'+id)\n";
   print "    o.style.display="; print '"none"'; print "\n";
   print "    var o=document.getElementById('IT_'+id)\n";
   print "    o.className='tbItem'\n";
   print "    if (currentItem) document.getElementById('TBSI_'+currentItem).className=";
   print '"tbSubItemLink"'; print "\n";
   print "}\n";
   print "function showSelect(id,hint) {\n";
   print "    if (currentItem) document.getElementById('TBSI_'+currentItem).className="; print '"tbSubItemLink"'; print "\n";
   print "    document.getElementById('TBSI_'+id).className="; print '"tbSubItemLinkSelected"'; print "\n";
   print "    currentItem=id\n";
   print "    if (hint!='undefined') status=hint\n";
   print "    else status=''\n";
   print "    var o=document.getElementById('Toolbar')\n";
   print "    y=o.offsetTop+o.offsetHeight-4\n";
   print "    // check for SubMenu and display it	\n";
   print "    var o=document.getElementById('TBSUB_'+id)\n";
   print "    if (o) {\n";
   print "        y+=document.getElementById('TBSI_'+id).offsetTop+document.getElementById('TBSI_'+id).offsetHeight-4\n";
   print "        x=document.getElementById('TBSI_'+id).offsetLeft+document.getElementById('TBSI_'+id).offsetParent.offsetLeft+document.getElementById('TBSI_'+id).offsetParent.offsetWidth-15\n";
   print "        o.style.display='block'	\n";
   print "        o.style.left=x\n";
   print "        o.style.top=y\n";
   print "        currentMenu=o.id\n";
   print "    }\n";
   print "}\n";
   print "function doAction(id) {\n";
   print "    var o=document.getElementById('Toolbar')\n";
   print "    y=o.offsetTop+o.offsetHeight-4\n";
   print "    if (currentMenu) {\n";
   print "        document.getElementById('IT_'+currentMenu).className='tbItem'\n";
   print "        document.getElementById('TBS_'+currentMenu).style.display='none'\n";
   print "    }\n";
   print "    var o=document.getElementById('IT_'+id)\n";
   print "    x=o.offsetLeft+o.offsetParent.offsetLeft;\n";
   print "    o.className='tbItemSelected'\n";
   print "    o=document.getElementById('TBS_'+id)\n";
   print "    o.style.display='block'	\n";
   print "    o.style.left=x\n";
   print "    o.style.top=y\n";
   print "    currentMenu=id\n";
   print "}\n";
   print "</script>";
   print "\n";
   print '<script language="Javascript">';
   print "//\n//Define the menu items we are to be using\n//\n";
   print "addMenu('Active Tasks','Active Tasks')\n";
   print "  addMenuItem('Active Tasks','Act1','Show All Jobs on Active Queue','html_job_status.pl','Everything left to run for today')\n";
   print "  addMenuItem('Active Tasks','Act2','Show All Jobs on Active Queue including dependencies','html_job_status_full.pl','')\n";
   print "  addMenuItem('Active Tasks','Act3','Show Non-Repeat Jobs on Active Queue','html_job_status_no_repeatjobs.pl','')\n";
   print "  addMenuItem('Active Tasks','Act4','Show Non-Repeat Jobs on Active Queue including dependencies','html_job_status_full_no_repeatjobs.pl','')\n";
   print "  addMenuItem('Active Tasks','Act6','Display the ALERT Queue','html_alert_status.pl','Display any Alerts awaiting action')\n";
   print "  addMenuItem('Active Tasks','Act7','Display the Dependencies Queue','html_list_depq.pl','Dependency Queue Management')\n";
   print "  addMenuItem('Active Tasks','Act9','Query the Scheduler current status','html_scheduler_status.pl','Display Server Status')\n";
   print "addMenu('Databases','Databases')\n";
   print "  addMenuItem('Databases','dbs1','View all defined job entries','html_jobsdbs_listall.pl','View offline job database')\n";
   print "  addMenuItem('Databases','dbs2','View all defined calendar entries','html_calendar_listall.pl','View offline job database')\n";
   print "addMenu('Help','Help')\n";
   print "  addMenuItem('Help','help1','Functional help for this browser sample','scheduler_help_loader.pl','Help files')\n";
   print "addMenu('Admin Helpers','Admin Helpers')\n";
   print "  addMenuItem('Admin Helpers','assist1','Build job commands','html_assist_jobs.pl','Job assistant')\n";
   print "  addMenuItem('Admin Helpers','assist2','Build user commands','html_assist_user.pl','User assistant')\n";
   print "  addMenuItem('Admin Helpers','assist3','Build calendar commands','html_assist_calendar.pl','Calendar assistant')\n";
   print "  addMenuItem('Admin Helpers','assist4','View stored requests to date','html_assist_view_history.pl','')\n";
   print '</script>';
   print "\n\n";
   print '<script language="Javascript">'; print "\n//\n//Scripts to control the button overlaying\n//\n";
   print 'function MM_swapImgRestore() { //v3.0'; print "\n";
   print '  var i,x,a=document.MM_sr; for(i=0;a&&i<a.length&&(x=a[i])&&x.oSrc;i++) x.src=x.oSrc;'; print "\n";
   print '}'; print "\n";
   print 'function MM_preloadImages() { //v3.0'; print "\n";
   print '  var d=document; if(d.images){ if(!d.MM_p) d.MM_p=new Array();'; print "\n";
   print '    var i,j=d.MM_p.length,a=MM_preloadImages.arguments; for(i=0; i<a.length; i++)'; print "\n";
   print '    if (a[i].indexOf("#")!=0){ d.MM_p[j]=new Image; d.MM_p[j++].src=a[i];}}'; print "\n";
   print '}'; print "\n";
   print 'function MM_findObj(n, d) { //v3.0'; print "\n";
   print '  var p,i,x;  if(!d) d=document; if((p=n.indexOf("?"))>0&&parent.frames.length) {'; print "\n";
   print '    d=parent.frames[n.substring(p+1)].document; n=n.substring(0,p);}'; print "\n";
   print '  if(!(x=d[n])&&d.all) x=d.all[n]; for (i=0;!x&&i<d.forms.length;i++) x=d.forms[i][n];'; print "\n";
   print '  for(i=0;!x&&d.layers&&i<d.layers.length;i++) x=MM_findObj(n,d.layers[i].document); return x;'; print "\n";
   print '}'; print "\n";
   print 'function MM_swapImage() { //v3.0'; print "\n";
   print '  var i,j=0,x,a=MM_swapImage.arguments; document.MM_sr=new Array; for(i=0;i<(a.length-2);i+=3)'; print "\n";
   print '   if ((x=MM_findObj(a[i]))!=null){document.MM_sr[j++]=x; if(!x.oSrc) x.oSrc=x.src; x.src=a[i+2];}'; print "\n";
   print '}'; print "\n</script>\n";
   print "\n\n</head>\n\n";
   print "<!-- Background light blue, links blue (unvisited), navy (visited), red (active) -->\n";
   print '<BODY TEXT="#000000" LINK="#0000FF" VLINK="000080" ALINK="#FF0000" BGCOLOR="#F0F0FF">';
   print "\n";
   print "<script>\ndrawToolbar();\n</script>\n";
}

sub scheduler_add_hot_button() {
  local ($norm) = $_[0];
  local ($hot) = $_[1];
  local ($urlname) = $_[2];
  local ($btnname) = $_[3];
  print '<A HREF="';
  print &scheduler_HTTPD_Home();
  print "/";
  print ${urlname};
  print "\"";
  print '     onMouseOut="MM_swapImgRestore()"';
  print "     onMouseOver=\"MM_swapImage('";
  print ${btnname};
  print "','','";
  print &scheduler_icon_path();
  print ${hot};
  print "',1)\"";
  print ">";
  print "<IMG name=\""; print ${btnname}; print "\" SRC=\"";
  print &scheduler_icon_path();
  print ${norm};
  print "\" border=\"0\" ALIGN=\"CENTER\"></A>";
  # The below inserts an extra line in <pre> blocks.
  # print "\n";
}

# ==============================================
# CUSTOMER MODIFIABLE AREAS FOR MULTIPLE SYSTEMS
# ==============================================
sub scheduler_system_list() {
  local ($request) = $_[0];
  local ($dataidx) = $_[1];
  # The number of systems we know about
  # ALWAYS ON A SINGLE SYSTEM SET TO 0.
  # This ensures the routines that use this
  # procedure work correctly on localhost.
  #
  # local $sysnamecount  = 0;
  #
  # Changing the above value from 0 means you MUST update
  # the system list and list if elsif below to include
  # a check for each system you intend to monitor.
  # The below set are what I tested on. Change as needed.
  #
  local $sysnamecount  = 1;
  #
  # A list of systems, one pair for each sysnamecount
  local $hostname1     = "falcon";
  local $ipaddr1       = "169.254.218.183";
  local $hostname2     = "nic";
  local $ipaddr2       = "169.254.218.185";
  local $hostname3     = "amber";
  local $ipaddr3       = "169.254.218.184";
  # Catchall if not customised correctly, also used for 0 systems (localhost)
  local $localhostip   = "127.0.0.1";
  local $localhostname = "localhost";

  # Were we asked to return the count of systems
  if ($request eq "GETCOUNT") {
     return( $sysnamecount );
  }

  # Or are we being asked for a system name
  # For each name in the entry list above add a check here
  elsif ($request eq "GETNAME") {
     if ($dataidx == 1) {
        return( $hostname1 );
     }
     elsif ($dataidx == 2) {
        return( $hostname2 );
     }
     elsif ($dataidx == 3) {
        return( $hostname3 );
     }
     # default (0)
     else {
        return( $localhostname );
     }
  }

  # Or are we being asked for an ip-address
  # For each name in the entry list above add a check here
  elsif ($request eq "GETIPADDR") {
     if ($dataidx == 1) {
        return( $ipaddr1 );
     }
     elsif ($dataidx == 2) {
        return( $ipaddr2 );
     }
     elsif ($dataidx == 3) {
        return( $ipaddr3 );
     }
     # default (0)
     else {
        return( $localhostip );
     }
  }

  # Else, an error !
  else {
     die( "scheduler-lib.pl: Bad request passed to scheduler_system_list" );
  }
}

# === You must leave the line below ===
1;  # return true, the library has been loaded OK
