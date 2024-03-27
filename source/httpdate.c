#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static bool is_leap_year(int year)
{
	return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

bool httpdate_parse(const char *date, time_t *timestamp)
{
	char s[30];
	int day, month, year, hours, minutes, seconds;

	if(strlen(date) != 29)
		return false;

	strncpy(s, date, sizeof(s));

	if(strlen(s) != 29 ||
	   s[3]  != ',' ||
	   s[4]  != ' ' ||
	   s[7]  != ' ' ||
	   s[11] != ' ' ||
	   s[16] != ' ' ||
	   s[19] != ':' ||
	   s[22] != ':' ||
	   s[25] != ' ' ||
	   s[26] != 'G' ||
	   s[27] != 'M' ||
	   s[28] != 'T')
	    return false;

	s[7] = s[16] = s[19] = s[22] = s[25] = 0;

	day = atoi(s + 5);
	year = atoi(s + 12);
	hours = atoi(s + 17);
	minutes = atoi(s + 20);
	seconds = atoi(s + 23);

	switch(s[8]) {
		case 'J': month = s[9] == 'a' ? 1 : ( s[10] == 'n' ? 6 : 7); break;
		case 'F': month = 2; break;
		case 'M': month = s[10] == 'r' ? 3 : 5; break;
		case 'A': month = s[9] == 'p' ? 4 : 8; break;
		case 'S': month = 9; break;
		case 'O': month = 10; break;
		case 'N': month = 11; break;
		case 'D': month = 12; break;
		default: return false;
	}

	int year_days_by_month[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
	int leap_years = ((year - 1) - 1968) / 4 - ((year - 1) - 1900) / 100 + ((year - 1) - 1600) / 400;
	int year_days = year_days_by_month[month - 1] + day - 1 + (is_leap_year(year) && month > 2 ? 1 : 0);
	int days = (year - 1970) * 365 + leap_years + year_days;
	*timestamp = (time_t) seconds + (time_t) minutes * 60 + (time_t) hours * 3600 + (time_t) days * 86400;

	return true;
}
