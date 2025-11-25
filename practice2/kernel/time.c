#include "types.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"

// A1 --

extern long xtime; // current time; defined in proc.c

static int days_in_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

int isleapyear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

long rtcdate_to_epoch(struct rtcdate *r)
{
    long days = 0;

    // years
    for (int y = 1970; y < r->year; y++)
    {
        days += isleapyear(y) ? 366 : 365;
    }

    // months
    for (int m = 1; m < r->month; m++)
    {
        days += days_in_month[m - 1];
        if (m == 2 && isleapyear(r->year))
        {
            days += 1;
        }
    }

    // days
    days += r->day - 1;

    // to seconds
    return days * 24 * 3600 + r->hour * 3600 + r->minute * 60 + r->second;
}

// TODO: complete this function for assignment 1
void time_init(void)
{
    struct rtcdate r;

    cmostime(&r);
    xtime = rtcdate_to_epoch(&r);
}

int sys_time(void)
{
    long *t;
    if (argptr(0, (char **)&t, sizeof(long)) < 0)
        return -1;

    if (t != 0)
        *t = xtime;

    return xtime;
}

int sys_stime(void)
{
    long *t;
    if (argptr(0, (char **)&t, sizeof(long)) < 0)
        return -1;

    xtime = *t;
    return 0;
}

// -- A1
