#include <stdlib.h>
#include <stdbool.h>
#include <uv.h>
#include <jansson.h>

#include "neat.h"

// for json_loads
extern int first_flow; 
extern json_t *props_before;
extern char properties_before[1000];
extern char props_b[1000];

// for send_result_connection_attempt_to_pm and nt_json_send_once_no_reply
// maybe make a struct of all the values?
extern _Bool first;
extern _Bool first2;
extern _Bool same_value;
extern _Bool same_value_jsondumps;
extern char output_buffer_before[1000];
extern json_t *prop_obj_before;
extern json_t *result_obj_before;
// probably have to change the variables so that are not references --> char interface[1000]
// and use strcpy(..., ...)
struct properies_for_jsonpack
{
	char interface[1000]; // needed for a later jsonpack function 
	char ip[1000];	// --> test out lower values than 1000 --> see how much is necessary
    unsigned int port;
    int transport;
    _Bool result;	// not in the originally structure
};
extern struct properies_for_jsonpack properties_jsonpack;
// for json_object_set
extern json_t *result_obj_after_set;

// for neat_set_property (after json_loads)
extern _Bool first_f;
extern int what_index_to_replace;
struct properties_to_set
{
	json_t *prop_before;
    json_t *val_before;
    const char *key_before;
};

extern struct properties_to_set *properties_set;

// json_t *array = NULL, *endpoints = NULL, *properties = NULL, *domains = NULL, *address, *port, *req_type;
// for send_properties_to_pm and nt_json_send_once
extern int what_index_to_replace2;
extern _Bool first3;
extern _Bool first4;
extern _Bool same_value_jsondumps2;
extern char output_buffer_before2[1000];
struct send_properties_to_pm_values
{
	int index_before;
	json_t *ipvalue_before;
	json_t *ip_before;
	json_t *endpoint_before;
	json_t *endpoints_beofre;
	json_t *properties_after_set;
	json_t *port_beofre;
	json_t *flow_port_before;
	json_t *domains_before;
	json_t *address;
	json_t *properties_before;
};
//extern struct send_properties_to_pm_values send_prop_array[10];


// for json_dumps
//extern json_t *ip_before[100];
//extern json_t *ipvalues[100]; // don't know how large it should be
