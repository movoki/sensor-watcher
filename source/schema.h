#ifndef schema_h
#define schema_h

#include "bigpacks.h"

#define SCHEMA_NULL        		  (1UL << 0)		// Flags without argument
#define SCHEMA_BOOLEAN     		  (1UL << 1)
#define SCHEMA_INTEGER     		  (1UL << 2)
#define SCHEMA_FLOAT       		  (1UL << 3)
#define SCHEMA_STRING      		  (1UL << 4)
#define SCHEMA_BINARY      		  (1UL << 5)

#define SCHEMA_REQUIRED    		  (1UL << 8)
#define SCHEMA_UNIQUE      		  (1UL << 9)
#define SCHEMA_INDEX 	    	  (1UL << 10)
#define SCHEMA_IDENTIFIER 	      (1UL << 11)
#define SCHEMA_READ_ONLY   		  (1UL << 12)
#define SCHEMA_WRITE_ONLY  		  (1UL << 13)

#define SCHEMA_LIST        		  (1UL << 16)		// Flags with argument
#define SCHEMA_MAP         		  (1UL << 17)
#define SCHEMA_TUPLE        	  (1UL << 18)

#define SCHEMA_LABEL       		  (1UL << 19)
#define SCHEMA_DESCRIPTION 		  (1UL << 20)
#define SCHEMA_VALUES      		  (1UL << 21)
#define SCHEMA_FORMAT      		  (1UL << 22)
#define SCHEMA_UNIT      		  (1UL << 23)
#define SCHEMA_DEFAULT     		  (1UL << 24)	///

#define SCHEMA_MINIMUM      	  (1UL << 26)
#define SCHEMA_MAXIMUM      	  (1UL << 27)
#define SCHEMA_MINIMUM_BYTES      (1UL << 28)
#define SCHEMA_MAXIMUM_BYTES      (1UL << 29)
#define SCHEMA_MINIMUM_ELEMENTS   (1UL << 30)
#define SCHEMA_MAXIMUM_ELEMENTS   (1UL << 31)


#define SCHEMA_GET_REQUEST        	(1UL << 0)
#define SCHEMA_GET_RESPONSE     	(1UL << 1)
#define SCHEMA_POST_REQUEST     	(1UL << 2)
#define SCHEMA_POST_RESPONSE       	(1UL << 3)
#define SCHEMA_PUT_REQUEST      	(1UL << 4)
#define SCHEMA_PUT_RESPONSE      	(1UL << 5)
#define SCHEMA_DELETE_REQUEST       (1UL << 6)
#define SCHEMA_DELETE_RESPONSE      (1UL << 7)

uint32_t schema_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer);

#endif