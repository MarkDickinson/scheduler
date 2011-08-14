# ------------------------------------------------------------
#
# lazy_install.sh
#
# ====== ONLY USE FOR FRESH INSTALLS ======
# I have removed dbs conversion functionality as it is assumed
# anybody getting this from a git repository has only just
# discovered it; no dbs changes since this went to github.
#
# Used to install marks_job_scheduler, for those that don't
# want to read the manuals, or read the manual quick start
# instructions.
#
# Basically if you wish to have no idea of what is going
# on you can run this script as root and pray I don't
# have a misplaced rm -rf.
#
# Will create the directories needed, perform the compiles,
# customise control files and startup script, install the
# startup init.d script, create an uninstaller; and lastly
# if the user doing the install choses to start the server
# will add a non-root admin user.
#
# Syntax: ./lazy_install.sh /destination/directory
#     ie: ./lazy_install.sh /opt/dickinson/scheduler
#
# The main reason you may want to use this is that if you are 
# too lazy to read the manual install steps in order to know 
# whats being changed on your system you will probably have
# trouble customising for install, and also uninstalling it
# (should you wish to uninstall). This script will create
# an uninstaller script for you as well as doing the install.
#
# IF YOU USE THE UNINSTALL SCRIPT RUN IT AS THE USER YOU GAVE
# OWNERSHIP OF THE INSTALL TO, NOT AS ROOT, THE UNINSTALL
# USES rm -rf SO JUST DON'T RUN IT AS ROOT
# ------------------------------------------------------------
#

# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#          Create some default values.
# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
sourcedir=`pwd`
targetdir="/opt/dickinson/scheduler"
myuserid=`whoami`
targetuid=${myuserid}
schedname="marks_job_scheduler"
testvar="x"
installed_rc_script="N"
workfile="/tmp/jobsched_install_work"
portnum="9002"
server_started="FALSE"
saved_config=""
chkconfig_used="N"

# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# intro():
#        Show the install intoduction banner.
# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
intro() {
   echo "*************************************************"
   echo "*  Installation script for marks job scheduler  *"
   echo "*************************************************"
   echo "Before beginning you need to know..."
   echo "  The directory you are installing to"
   echo "  You should have picked a name for the init.d script"
   echo "  Decided on  TcpIp port number to use (default is 9002)"
   echo "  And chosen a admin user to own the files, preferably not root"
   echo " "
   read -p "Depress ENTER to proceed" testvar
   clear
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# show_parms():
#     Used by get_parms, display the current values that
#     the install will use so the user knows what we
#     are about to use
# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
show_parms() {
   echo " "
   echo "Enter the number to change a parameter value,"
   echo "     or 'i' to begin the installation"
   echo "          [use ctrl-c to abort]"
   echo " "
   echo "If you change the default port number refer to the html"
   echo "manuals to see how to connect to the port as it is compiled"
   echo "into all the tools to use 9002 by default."
   echo " "
   echo "If a test userid is provided you will be given the oportunity to"
   echo "install and run the supplied sample test job suite. The user must exist."
   echo " "
   echo "1) Target directory : ${targetdir}"
   echo "2) Files are to be owned by : ${targetuid}"
   echo "3) The init.d scriptnames will be : ${schedname}"
   echo "4) TcpIp port number to use : ${portnum}"
   echo " "
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# get_parms():
#   Allow the user to change the default installation
#   values we have set.
#   The parm value to be changed is from the 1-4 list
#   displayed by show_parms above.
# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
get_parms() {
   testvar="x"
   while [ "${testvar}." != "i." ];
   do
      show_parms
	  read -p "Change which option (or 'i' to install with above): " testvar
	  if [ "${testvar}." = "1." ];
	  then
			  read -r -p "New target directory ? " targetdir
			  if [ "${targetdir}." = "." ];
			  then
					  targetdir="/opt/job_scheduler"
					  echo "You must provide one."
			  fi
	  fi
	  if [ "${testvar}." = "2." ];
	  then
			  read -p "Which user will own the files ? " targetuid
			  if [ "${targetuid}." = "." ];
			  then
					  targetuid=${myuserid}
					  echo "You must provide one."
			  fi
	  fi
	  if [ "${testvar}." = "3." ];
	  then
			  read -p "What will the scriptname be (no spaces) ?" schedname
			  if [ "${schedname}." = "." ];
			  then
					  schedname="marks_job_scheduler"
					  echo "You must provide one."
			  fi
	  fi
	  if [ "${testvar}." = "4." ];
	  then
			  read -p "Select port number to use (5000+), recomended is 9002 ?" portnum
			  if [ "${portnum}." = "." ];
			  then
					  portnum=""
					  echo "You must provide one."
			  fi
	  fi
   done
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# install_appl():
#   Create the target directory, untar the shipped tar
#   file into it, create required subdirectories, set the
#   file and directory permissions as needed.
# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
install_appl() {
  if [ ! -d ${targetdir} ];
  then
     mkdir ${targetdir}
  fi
  if [ ! -d ${targetdir} ];
  then
     echo "Unable to create directory ${targetdir}, aborting !"
     exit 1
  fi
  mkdir ${targetdir}/bin
  mkdir ${targetdir}/etc
  mkdir ${targetdir}/logs
  mkdir ${targetdir}/joblogs
  cat << EOF > ${targetdir}/etc/shell_vars
SCHED_BASE_DIR=${targetdir}
SCHED_BIN_DIR=${targetdir}/bin
SCHED_JOBLOG_DIR=${targetdir}/joblogs
EOF
  chmod -R 755 ${targetdir}
  chown -R ${targetuid} ${targetdir}
  chmod 644 ${targetdir}/etc/shell_vars
  # compile from source, clean up temporary .o files
  cd src
  ./compile_all_linux ${targetdir}
  if [ $? -ne 0 ];
  then
     echo "Oops, compile problem. Resolve before rerunning this script."
     find ./ -type f -name "*.o" -exec /bin/rm {} \;  # clean anyway, don't leave files to confuse git
     exit 1
  fi
  find ./ -type f -name "*.o" -exec /bin/rm {} \;
  cd ..
  # secure correctly
  chown -R ${targetuid} ${targetdir}/*
  chmod 500 ${targetdir}/bin/jobsched_daemon
  chmod 511 ${targetdir}/bin/jobsched_cmd
  chmod 511 ${targetdir}/bin/jobsched_take_snapshot
  chmod 511 ${targetdir}/bin/jobsched_security_checks
  chmod 511 ${targetdir}/bin/jobsched_jobutil
  chmod 777 ${targetdir}/logs
  chmod 777 ${targetdir}/joblogs
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# testvar_to_yn():
#   Basically make sure we have a y or n value
# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
testvar_to_yn() {
   testvar2="x"
   while [ "${testvar2}." != "z." ];
   do
      read -p "Reply y or n ? " testvar
	  if [ "${testvar}." = "y." ];
	  then
			  testvar2="z"
	  fi
	  if [ "${testvar}." = "n." ];
	  then
			  testvar2="z"
	  fi
	  echo "Got ${testvar}"
   done
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# customise_initd_script():
#   Customise the init.d script for this installation
#   based upon the directory and port number the user
#   provided for the install target
# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
customise_initd_script() {
   cd samples/initd
   echo "Customising startup scripts for directory ${targetdir}..."
   if [ ! -f server.rc3_d ];
   then
      echo "Unable to find file server.rc3_d, aborting !"
      exit 1
   fi
   echo "s'/opt/dickinson/scheduler'${targetdir}'g" > ${workfile}
   echo "s'9002'${portnum}'g" >> ${workfile}
   cat server.rc3_d | sed -f ${workfile} > server.rc3_d.customised
   /bin/rm -f ${workfile}
   if [ ! -f server.rc3_d.customised ];
   then
      echo "Unable to customise the rc.d script, aborting !"
      exit 1
   fi
   cd ../..
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# install_rc_scripts():
#   Create the required startup and shutdown scripts for
#   the job scheduler. We want it to start in runlevel
#   3 when the system is multiuser. We need the K links
#   as well to ensure the scheduler is shutdown cleanly
#   when the system is shutdown.
#   use chkconfig is available.
# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
install_rc_scripts() {
   customise_initd_script
   echo " "
   echo "Do you wish to have the startup and shutdown files placed"
   echo "in the /etc/rc.d directories so the server starts and stops"
   echo "automatically ?"
   echo "This is strongly recomended as this will allow the scheduler"
   echo "to shutdown cleanly when the system is shutdown."
   testvar="x"
   testvar_to_yn
   if [ "${testvar}." = "y." ];
   then
      echo "Copying rc script to /etc/rc.d/init.d/${schedname}"
      cp -pi samples/initd/server.rc3_d.customised /etc/rc.d/init.d/${schedname}
      chmod 700 /etc/rc.d/init.d/${schedname}
      chown root /etc/rc.d/init.d/${schedname}
      echo "Creating symbolic links in /etc/rc.d directories"
      testauto=`which chkconfig 2> zzz`
      if [ -f zzz ];
      then
         /bin/rm zzz
      fi
      if [ "${testauto}." = "." ];
      then
         chkconfig_used="N"
         echo "*** No chkconfig command found, manually creating links ***"
   	     echo "   /etc/rc.d/rc0.d - K10_${schedname}"
         cd /etc/rc.d/rc0.d
         ln -s ../init.d/${schedname} K10_${schedname}
	     echo "   /etc/rc.d/rc1.d - K10_${schedname}"
         cd /etc/rc.d/rc1.d
         ln -s ../init.d/${schedname} K10_${schedname}
	     echo "   /etc/rc.d/rc6.d - K10_${schedname}"
         cd /etc/rc.d/rc6.d
         ln -s ../init.d/${schedname} K10_${schedname}
	     echo "   /etc/rc.d/rc3.d - S99_${schedname}"
         cd /etc/rc.d/rc3.d
         ln -s ../init.d/${schedname} S99_${schedname}
      else
         chkconfig_used="Y"
         echo "Found chkconfig at ${testauto}, using chkconfig"
         chkconfig --add ${schedname}
         chkconfig ${schedname} on
      fi
      installed_rc_script="Y"
   else
      # so copy the custom one to the targetdir and secure correctly
      cp -p samples/initd/server.rc3_d.customised ${targetdir}/bin/server_control
      chmod 744 ${targetdir}/bin/server_control
      chown root ${targetdir}/bin/server_control
      # and warn the user and provide manual stop/start command
      echo "Not installed to the rc.d directories."
      echo "To start the server use the command..."
      echo "   ${targetdir}/bin/server_control start"
      echo "ALWAYS shut the server down manually before shutting down"
      echo "the system. This will not be done automatically as you have"
      echo "chosen not to install the shutdown scripts into the rc.d"
      echo "directories. Failure to do so may leave the job scheduler"
      echo "in an un-useable state."
   fi
   /bin/rm samples/initd/server.rc3_d.customised
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# start_server():
#   Start the server. Where we start it from depends on
#   whether the rc.d files were created or not.
#   Note: only start if the user requests the server to be
#         started at this time.
# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
start_server() {
  echo " "
  echo "Do you wish to start the server now ?"
  testvar="x"
  testvar_to_yn
  if [ "${testvar}." = "y." ];
  then
    if [ "${installed_rc_script}" = "Y" ];
    then
       if [ ! -x /etc/rc.d/init.d/${schedname} ];
       then
          echo "The installation to /etc/rc.d/init.d appears to have failed !"
          echo "Can't start scheduler, /etc/rc.d/init.d/${schedname} not executable"
        else
           echo "Issuing command: /etc/rc.d/init.d/${schedname} start"
           /etc/rc.d/init.d/${schedname} start
        fi
     else
        echo "Issuing command: ${targetdir}/bin/server_control start"
        ${targetdir}/bin/server_control start
    fi
    # Check if server has started, if it has we will add the admin user
    listening=`netstat -an | grep ${portnum} | grep LISTEN | wc -l`
    if [ ${listening} -gt 0 ];
    then
       server_started="TRUE";
    else
       server_started="FALSE";
       echo "The scheduler appears to have failed to start."
       echo "Please manually check the configurations created."
    fi
  fi
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# show_start_command():
#   When we have finished, show the appropriate start
#   command for the server based on whether the rc script
#   was installed or not.
# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
show_start_command() {
    if [ "${installed_rc_script}" = "Y" ];
    then
       echo "The job scheduler will be started at each reboot..."
       echo "    '/etc/rc.d/init.d/${schedname} start' may be used for manual starts"
    else
       echo "The job scheduler needs to be manually restarted when needed..."
       echo "  use '${targetdir}/bin/server_control start'"
       echo " "
       echo "If you wish to start the server from a system reboot .rc script"
       echo "copy the supplied one to /etc/init.d and use chkconfig to enable it"
    fi
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# create_uninstaller():
#   Create a script to blow the entire thing away if the
#   user doesn't want to keep it.
# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
create_uninstaller() {
   uninstall_file="${targetdir}/bin/uninstall.sh"
   echo "WAIT !, creating uninstall script for you."
   echo "#!/bin/bash" > ${uninstall_file}
   echo "# -------------------------------------------------" >> ${uninstall_file}
   echo "# Generated script." >> ${uninstall_file}
   echo "# This can be used to uninstall the scheduler." >> ${uninstall_file}
   echo "# -------------------------------------------------" >> ${uninstall_file}
   echo "myuserid=\`whoami\`" >> ${uninstall_file}
   echo "if [ ${myuserid} != \"root\" ];" >> ${uninstall_file}
   echo "then" >> ${uninstall_file}
   echo "  echo \"You must be the root user to run this script\" " >> ${uninstall_file}
   echo "  exit 1" >> ${uninstall_file}
   echo "fi" >> ${uninstall_file}
   echo "#" >> ${uninstall_file}
   echo "# Ensure the scheduler is shutdown !" >> ${uninstall_file}
   echo 'a=`ps -eo args | grep "jobsched_daemon" | grep -v "grep"`' >> ${uninstall_file}
   echo "if [ \".\${a}\" != \".\" ];"  >> ${uninstall_file}
   echo "then" >> ${uninstall_file}
   echo "  echo \"The job scheduler server is still running !. Shut it down first.\" " >> ${uninstall_file}
   echo "  exit 1" >> ${uninstall_file}
   echo "fi" >> ${uninstall_file}
   echo "#" >> ${uninstall_file}
   echo "# Confirm the removal first" >> ${uninstall_file}
   echo "echo \"This will remove the application marks_job_scheduler from your system\" " >> ${uninstall_file}
   echo "read -p \"Is this what you wish to do (y/n) ?\" testvar" >> ${uninstall_file}
   echo "if [ \"\${testvar}.\" != \"y.\" ];" >> ${uninstall_file}
   echo "then" >> ${uninstall_file}
   echo "   echo \"Cancelled !\" " >> ${uninstall_file}
   echo "   exit 1" >> ${uninstall_file}
   echo "fi" >> ${uninstall_file}
   echo "#" >> ${uninstall_file}
   if [ ${installed_rc_script} = "Y" ];
   then
      echo "# Remove symbolic links the install created." >> ${uninstall_file}
      if [ "${chkconfig_used}." = "Y." ];
      then
         echo "   chkconfig ${schedname} off" >> ${uninstall_file}
         echo "   chkconfig --del ${schedname}" >> ${uninstall_file}
         echo "   /bin/rm -f /etc/rc.d/init.d/${schedname}" >> ${uninstall_file}
      else
         echo "if [ -h /etc/rc.d/rc0.d/K10_${schedname} ];" >> ${uninstall_file}
         echo "then"  >> ${uninstall_file}
         echo "   rm -f /etc/rc.d/rc0.d/K10_${schedname}"  >> ${uninstall_file}
         echo "   echo \"deleted /etc/rc.d/rc0.d/K10_${schedname}\" "  >> ${uninstall_file}
         echo "fi"  >> ${uninstall_file}
         echo "if [ -h /etc/rc.d/rc1.d/K10_${schedname} ];"  >> ${uninstall_file}
         echo "then"  >> ${uninstall_file}
         echo "   rm -f /etc/rc.d/rc1.d/K10_${schedname}"  >> ${uninstall_file}
         echo "   echo \"deleted /etc/rc.d/rc1.d/K10_${schedname}\" "  >> ${uninstall_file}
         echo "fi"  >> ${uninstall_file}
         echo "if [ -h /etc/rc.d/rc6.d/K10_${schedname} ];"  >> ${uninstall_file}
         echo "then"  >> ${uninstall_file}
         echo "   rm -f /etc/rc.d/rc6.d/K10_${schedname}"  >> ${uninstall_file}
         echo "   echo \"deleted /etc/rc.d/rc6.d/K10_${schedname}\" "  >> ${uninstall_file}
         echo "fi"  >> ${uninstall_file}
         echo "if [ -h /etc/rc.d/rc3.d/S99_${schedname} ];"  >> ${uninstall_file}
         echo "then"  >> ${uninstall_file}
         echo "   rm -f /etc/rc.d/rc3.d/S99_${schedname}"  >> ${uninstall_file}
         echo "   echo \"deleted /etc/rc.d/rc3.d/S99_${schedname}\" "  >> ${uninstall_file}
         echo "fi"  >> ${uninstall_file}
         echo "if [ -f /etc/rc.d/init.d/${schedname} ];"  >> ${uninstall_file}
         echo "then"  >> ${uninstall_file}
         echo "   rm -f /etc/rc.d/init.d/${schedname}"  >> ${uninstall_file}
         echo "   echo \"deleted /etc/rc.d/init.d/${schedname}\" "  >> ${uninstall_file}
         echo "fi"  >> ${uninstall_file}
      fi
   else
      echo "# No /etc/rc.d scripts were created at install, none removed"  >> ${uninstall_file}
   fi
   echo "#"  >> ${uninstall_file}
   echo "# We assume the user installed this application into a unique"  >> ${uninstall_file}
   echo "# directory, but we get confirmation to be sure..."  >> ${uninstall_file}
   echo "echo \" \" " >> ${uninstall_file}
   echo "echo \"**WARNING** All files under ${targetdir} will be deleted\" " >> ${uninstall_file}
   echo "read -p \"Is that what you intend (y/n) ?\" testvar" >> ${uninstall_file}
   echo "if [ \"\${testvar}.\" = \"y.\" ];" >> ${uninstall_file}
   echo "then" >> ${uninstall_file}
   echo "  /bin/rm -fr ${targetdir}" >> ${uninstall_file}
   echo "fi" >> ${uninstall_file}
   echo "# " >> ${uninstall_file}
   echo "echo \"If you have manually installed the browser component elsewhere\" " >> ${uninstall_file}
   echo "echo \"you will have to delete it manually also.\" " >> ${uninstall_file}
   echo "# " >> ${uninstall_file}
   echo "echo \"Uninstall completed. Remove any components you may have\" " >> ${uninstall_file}
   echo "echo \"customised elsewhere as appropriate for your installation.\" " >> ${uninstall_file}
   echo "echo \"**Please 'cd /' now, the application directory you are in has been deleted \" " >> ${uninstall_file}
   echo "exit 0" >> ${uninstall_file}
   echo " "
   chmod 700 ${uninstall_file}
   echo "*****************************************************************"
   echo "An uninstall script based on your configuration has been saved as"
   echo "${uninstall_file}"
   echo "*****************************************************************"
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# add_admin_user():
#   If the server was started add an admin user.
#   TODO: just show how to for now
# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
add_admin_user() {
#   if [ "${server_started}." = "TRUE." ];
#   then
#     need to prompt for a userid, check it exists in /etc/passwd,
#     prompt for a password, and add the user.
#   else
      cat << EOF
------------------------------------------------------------------------
It is recommended you add a non-root userid as and administrator user
as soon as possible after installation; when you start the scheduler
application anyway.

You should run
    ${targetdir}/bin/jobsched_cmd
as soon as possible as root and enter the commands
(replacing mark with your unix userid, and abcdefgh with a unique password)
(and Mark Dickinson with your name of course)

  autologin
  user add mark,password abcdefgh,auth admin,autoauth yes,subsetauth admin,desc "Mark Dickinson"
  login mark abcdefgh

If the login as OK your new id can be used for admin functions instead
of needing be be logged in as root. And the manuals in doc/PDF explain
all this, read them !.
------------------------------------------------------------------------
EOF
#   fi
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# MAINLINE: Call the procedures so carefully defined
#           above to actually do this installation.
# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# - - - - - - - - - - - - - - - - - - - - - - - - - - - -
if [ "${myuserid}." = "root." ];
then
   intro
   get_parms
   install_appl
   install_rc_scripts
   create_uninstaller
   show_start_command
   start_server
   add_admin_user
else
  echo "You must be root to run $0"
fi

# Finished
exit 0
