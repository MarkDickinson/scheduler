<%--
  constants.jsp

  These are all the API command templates used by the application, conveniently
  all kept in one place so if the API format changes I only need to modify this
  one file.

  These are templates only. For example the short appearing ones require the
  code pages to put the nine byte command data buffer length in as they compute
  it.

  The hostname field that would normally be filled in has no meaning from
  within the JSP library so I have filled that with JSP_Interface_Modules so
  the job scheduler servers at least have some idea of where commands are being
  issued from by the users.
--%>
<%!
   // Used by all
   public static final int API_HEADER_DATA_LEN = 85;

   // Used by the sched subsystem code pages
   public static final String API_CMD_SCHED_RUNNOW =
            "API*^SCHED    ^000100001^O00000305^000^JSP_Interface_Modules                      ^";
   public static final String API_CMD_SCHED_DELETE =
            "API*^SCHED    ^000100001^O00000002^000^JSP_Interface_Modules                      ^";
   public static final String API_CMD_SCHED_HOLD =
            "API*^SCHED    ^000100001^O00000306^000^JSP_Interface_Modules                      ^";
   public static final String API_CMD_SCHED_RELEASE =
            "API*^SCHED    ^000100001^O00000307^000^JSP_Interface_Modules                      ^";
   public static final String API_CMD_SCHED_LISTALL =
            "API*^SCHED    ^000100001^000000009^000^JSP_Interface_Modules              ^000000000^";
   public static final String API_CMD_SCHED_STATUS_SHORT =
            "API*^SCHED    ^000100001^000000304^000^JSP_Interface_Modules              ^000000000^";
   public static final String API_CMD_SHOWSESSIONS =
            "API*^SCHED    ^000100001^000000303^000^JSP_Interface_Modules              ^000000000^";

   // Used by the alert subsystem code pages
   public static final String API_CMD_ALERT_LISTALL =
            "API*^ALERT    ^000100001^000000009^000^JSP_Interface_Modules              ^000000000^";
   // below moved +8 from logical to get it working
   public static final String API_CMD_ALERT_RESTART =
            "API*^ALERT    ^000100001^O00000006^000^JSP_Interface_Modules              XXXXXXXX^";
   public static final String API_CMD_ALERT_FORCEOK =
            "API*^ALERT    ^000100001^O00000202^000^JSP_Interface_Modules              XXXXXXXX^";
   public static final String API_CMD_ALERT_INFO =
            "API*^ALERT    ^000100001^000000004^000^JSP_Interface_Modules              XXXXXXXX^";

   // Used by the job subsystem code pages
   public static final String API_CMD_JOB_LISTALL =
            "API*^JOB      ^000100001^000000009^000^JSP_Interface_Modules              ^000000000^";
   // below moved +8 from logical to get it working
   public static final String API_CMD_JOB_INFO =
            "API*^JOB      ^000100001^000000004^000^JSP_Interface_Modules              XXXXXXXX^";
   public static final String API_CMD_JOB_SUBMIT =
            "API*^JOB      ^000100001^O00000006^000^JSP_Interface_Modules              XXXXXXXX^";
   public static final String API_CMD_JOB_DELETE =
            "API*^JOB      ^000100001^J00000032^000^JSP_Interface_Modules              XXXXXXXX^";
   public static final String API_CMD_JOB_ADD =
            "API*^JOB      ^000100001^J00000031^000^JSP_Interface_Modules              XXXXXXXX^";

   // Used by the dependency subsystem
   public static final String API_CMD_DEP_LISTALL =
            "API*^DEP      ^000100001^000000009^000^JSP_Interface_Modules              ^000000000^";
   public static final String API_CMD_DEP_CLEARALL_DEP =
            "API*^DEP      ^000100001^O00000102^000^JSP_Interface_Modules              XXXXXXXX^";
   public static final String API_CMD_DEP_CLEARALL_JOB =
            "API*^DEP      ^000100001^O00000103^000^JSP_Interface_Modules              XXXXXXXX^";

   // Used by the calendar subsystem
   public static final String API_CMD_CAL_LISTALL =
            "API*^CAL      ^000100001^000000009^000^JSP_Interface_Modules               ^000000000^";
   public static final String API_CMD_CAL_INFO =
            "API*^CAL      ^000100001^000000004^000^JSP_Interface_Modules              X^000000502^";
   public static final String API_CMD_CAL_READ_RAW =
            "API*^CAL      ^000100001^000000011^000^JSP_Interface_Modules              X^000000000^";
   public static final String API_CMD_CAL_ADD =
            "API*^CAL      ^000100001^J00000601^000^JSP_Interface_Modules              X^000000502^";
   public static final String API_CMD_CAL_DELETE =
            "API*^CAL      ^000100001^J00000604^000^JSP_Interface_Modules              X^000000502^";

   // used by the user subsystem
   public static final String API_CMD_USER_LISTALL =
            "API*^USER     ^000100001^000000009^000^JSP_Interface_Modules               ^000000000^";
   public static final String API_CMD_USER_ADD =
            "API*^USER     ^000100001^S00000503^000^JSP_Interface_Modules              X^000000573^";
   public static final String API_CMD_USER_NEWPSWD =
            "API*^USER     ^000100001^S00000505^000^JSP_Interface_Modules              X^000000573^";
   public static final String API_CMD_USER_DELETE =
            "API*^USER     ^000100001^S00000504^000^JSP_Interface_Modules              X^000000573^";
   public static final String API_CMD_USER_INFO =
            "API*^USER     ^000100001^000000004^000^JSP_Interface_Modules              X^000000573^";
%>
