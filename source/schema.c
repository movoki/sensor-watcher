#include <esp_log.h>

#include "adc.h"
#include "application.h"
#include "ble.h"
#include "board.h"
#include "backends.h"
#include "devices.h"
#include "i2c.h"
#include "logs.h"
#include "measurements.h"
#include "nodes.h"
#include "onewire.h"
#include "schema.h"
#include "wifi.h"

// [ [ [ path ] SCHEMA_GET_RESPONSE | SCHEMA_PUT_REQUEST [ schema ] ]
//   [ [ path ] SCHEMA_POST_REQUEST [ schema ]  ] ]

bool root_schema_handler(bp_pack_t *writer)
{
    bool ok = true;

    // GET
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_create_container(writer, BP_LIST);           // Path
        ok = ok && bp_finish_container(writer);
        ok = ok && bp_put_integer(writer, SCHEMA_GET_RESPONSE);    // Methods
        ok = ok && bp_create_container(writer, BP_LIST);           // Schema
	        ok = ok && bp_put_integer(writer, SCHEMA_LIST | SCHEMA_INDEX);
	        ok = ok && bp_create_container(writer, BP_LIST);
	        	ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_IDENTIFIER);
    		ok = ok && bp_finish_container(writer);
    	ok = ok && bp_finish_container(writer);
    ok = ok && bp_finish_container(writer);

    return ok;
}

uint32_t schema_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer)
{
    bool ok = true;

    if(method == PM_GET) {
        ok = ok && bp_create_container(writer, BP_LIST);
        	ok = ok && root_schema_handler(writer);
        	ok = ok && adc_schema_handler("adc", writer);
        	ok = ok && application_schema_handler("application", writer);
        	ok = ok && ble_schema_handler("ble", writer);
        	ok = ok && board_schema_handler("board", writer);
        	ok = ok && backends_schema_handler("backends", writer);
        	ok = ok && devices_schema_handler("devices", writer);
        	ok = ok && i2c_schema_handler("i2c", writer);
        	ok = ok && logs_schema_handler("logs", writer);
        	ok = ok && measurements_schema_handler("measurements", writer);
        	ok = ok && nodes_schema_handler("nodes", writer);
        	ok = ok && onewire_schema_handler("onewire", writer);
        	ok = ok && wifi_schema_handler("wifi", writer);
        ok = ok && bp_finish_container(writer);
        return ok ? PM_205_Content : PM_413_Request_Entity_Too_Large;
    }
    else
        return PM_405_Method_Not_Allowed;
}
