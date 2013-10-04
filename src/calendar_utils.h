#ifndef CALENDAR_UTILS
#define CALENDAR_UTILS

#include "scheduler.h"

void DEBUG_CALENDAR_dump_to_stdout( calendar_def * calbuffer );
int  CALENDAR_UTILS_populate_calendar_by_day( char * daylist, calendar_def * calbuffer );

#endif
