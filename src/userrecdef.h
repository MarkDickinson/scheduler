#ifndef USERREC_DEFS
#define USERREC_DEFS

#define USER_MAX_NAMELEN 39
#define USER_MAX_PASSLEN 39
#define USER_LIB_VERSION "01"
#define USERS_MAX_USERLIST 10

typedef struct {
   char userid[USER_MAX_NAMELEN+1];  /* a unix userid */
} USER_unix_userid_element;

typedef struct {
   char userrec_version[3];                 /* version of userrec to allow later on-the-fly upgrades 01 */
   char user_name[USER_MAX_NAMELEN+1];      /* The user name                      */
   char user_password[USER_MAX_PASSLEN+1];  /* users password                     */
   char record_state_flag;                  /* A=active, D=deleted                */
   char user_auth_level;                    /* O=operator, A=admin (all access), 0=none/browse,
                                               J=job auth (browse+add/delete jobs),
                                               S=security user...user maint only */
   char local_auto_auth;                    /* Y=allowed to autologin from jobsched_cmd effective uid locally, N=no */
   char allowed_to_add_users;               /* Y=allowed to add/delete auth to add jobs for users in their auth list
                                               N=not at all,
                                               S=can create users with auth to any unix id */
   USER_unix_userid_element user_list[USERS_MAX_USERLIST];  /* users this ID can define jobs for  */
   char user_details[50];                   /* free form text (ie: real name)     */
   char last_logged_on[18];                 /* DATE_TIME_LEN(scheduler.h)+1 */
   char last_password_change[18];           /* DATE_TIME_LEN(scheduler.h)+1 */
} USER_record_def;

#endif
