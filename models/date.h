#ifndef _DATE_HEADER_
#define _DATE_HEADER_

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

typedef enum{
	date_part_year = 0,
	date_part_month,
	date_part_day
}date_part_t;

// checks for valid YYYY-MM-DD. True if valid
bool date_check(char *date){
	if(date == NULL) return false;
	int size = strlen(date);

	char *anchor = date;
	date_part_t part = 0;
	for(char *cursor = date; cursor <= (date+size); cursor++){
		if(
			((*cursor < '0') ||
			(*cursor > '9')) &&
			(*cursor != '-') &&
			(*cursor != '\0')
		){
			return false;
		} 	

		if(*cursor == '-' || *cursor == '\0'){
			*cursor = '\0';
			int value = strtod(anchor, NULL);
			
			switch(part){
				case date_part_year:
					break;

				case date_part_month:
					if(value < 1 || value > 12) return false;
					break;

				case date_part_day:
					if(value < 1 || value > 31) return false;
					break;
					
				default:
					break;
			}

			part++;
			anchor = cursor + 1;
			continue;
		}
	}

	return true;
}

#endif