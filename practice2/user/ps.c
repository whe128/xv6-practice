// A1 --

#include "types.h"
#include "user.h"
#include "procinfo.h"

#define MAX_PROC 64

// is leap year
int is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// transfer the format to "yyyy-mm-dd hh:mm:ss"
void epoch_to_datetime(long epoch, char *buf)
{
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    long days = epoch / 86400;
    long rem = epoch % 86400;
    int hour = rem / 3600;
    rem %= 3600;
    int minute = rem / 60;
    int second = rem % 60;

    // year calculation
    int year = 1970;
    while (1)
    {
        int year_days = is_leap_year(year) ? 366 : 365;

        if (days >= year_days)
        {
            days -= year_days;
            year++;
        }
        else
        {
            break;
        }
    }

    // leap year adjustment
    if (is_leap_year(year))
        days_in_month[1] = 29;

    // month
    int month = 1;
    while (month <= 12)
    {
        int mdays = days_in_month[month - 1];

        if (days >= mdays)
        {
            days -= mdays;
            month++;
        }
        else
        {
            break;
        }
    }

    int day = days + 1;

    // standard output: yyyy-mm-dd hh:mm:ss
    char *p = buf;

    // Year (4 digits)
    p[0] = '0' + (year / 1000);
    p[1] = '0' + ((year / 100) % 10);
    p[2] = '0' + ((year / 10) % 10);
    p[3] = '0' + (year % 10);
    p[4] = '-';

    // Month (2 digits)
    p[5] = '0' + (month / 10);
    p[6] = '0' + (month % 10);
    p[7] = '-';

    // Day (2 digits)
    p[8] = '0' + (day / 10);
    p[9] = '0' + (day % 10);
    p[10] = ' ';

    // Hour (2 digits)
    p[11] = '0' + (hour / 10);
    p[12] = '0' + (hour % 10);
    p[13] = ':';

    // Minute (2 digits)
    p[14] = '0' + (minute / 10);
    p[15] = '0' + (minute % 10);
    p[16] = ':';

    // Second (2 digits)
    p[17] = '0' + (second / 10);
    p[18] = '0' + (second % 10);
    p[19] = '\0';
}

// format elapsed time
// if < 24hr: "hh:mm:ss"
// if >= 24hr: "d-hh:mm:ss"
void format_elapsed(long elapsed, char *buf)
{
    long days = elapsed / 86400;
    long rem = elapsed % 86400;
    int hour = rem / 3600;
    rem %= 3600;
    int minute = rem / 60;
    int second = rem % 60;

    char *p = buf;
    int idx = 0;

    // if days > 0, add "d-"
    if (days > 0)
    {
        // transfer days to string
        char digits[20];
        int i = 0;
        long temp = days;

        do
        {
            digits[i++] = '0' + (temp % 10);
            temp /= 10;
        } while (temp > 0);

        // reverse output
        for (int j = 0; j < i; j++)
        {
            buf[idx++] = digits[i - 1 - j];
        }
        buf[idx++] = '-';
        p = buf + idx;
    }

    // output hh:mm:ss
    p[0] = '0' + (hour / 10);
    p[1] = '0' + (hour % 10);
    p[2] = ':';
    p[3] = '0' + (minute / 10);
    p[4] = '0' + (minute % 10);
    p[5] = ':';
    p[6] = '0' + (second / 10);
    p[7] = '0' + (second % 10);
    p[8] = '\0';
}

void format_name(char *name, int bufLen)
{
    int len = strlen(name);

    if (len < bufLen - 1)
    {
        for (int i = len; i < bufLen - 1; i++)
        {
            name[i] = ' ';
        }
        name[bufLen - 1] = '\0';
    }
}

int main(int argc, char *argv[])
{
    struct procinfo_t procs[MAX_PROC];
    int num_procs;

    num_procs = procinfo(MAX_PROC, procs);
    if (num_procs < 0)
    {
        // error output
        printf(2, "ps: procinfo failed\n");
        exit();
    }

    printf(1, "PID\tState\tName\t\tStarted\t\t\t\tElapsed\n");

    for (int i = 0; i < num_procs; i++)
    {
        char date_time[32];
        char elapsed[32];
        format_name(procs[i].name, sizeof(procs[i].name));

        epoch_to_datetime(procs[i].start_time, date_time);
        format_elapsed(procs[i].elapsed_time, elapsed);

        printf(1, "%d\t%s\t%s\t%s\t\t%s\n",
               procs[i].pid,
               procs[i].st,
               procs[i].name,
               date_time,
               elapsed);
    }

    exit();
}

// -- A1
