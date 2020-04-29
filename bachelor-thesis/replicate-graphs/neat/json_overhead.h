#include <stdlib.h>
#include <stdbool.h>
#include <uv.h>
#include <jansson.h>

#include "neat.h"
//#include "uthash.h"
//#include <glib.h>
//#include "khash.h" 

extern const size_t array_initial_size;

// for neat_open
extern _Bool first_flow2;
extern int index_to_replace7;
struct properties_neat_open {
	json_t *flow_properties_before;
	unsigned int flow_ismultihoming;
	unsigned int flow_preserveMessageBoundaries;
	unsigned int flow_security_needed;
	json_t *flow_user_ips;
};
extern struct properties_neat_open neat_open_prop_array[10];

// for json_loads
struct properties_jsonloads {
	char properties_before[1000];
	json_t *props_before;
};
extern struct properties_jsonloads *jsonloads_a;

// less to repeat when creating structures that are very alike --> this means that init_array and check if full will not really work as the argument is not always the same
// use #define on them too XD????
#define CREATE_ARRAY_STRUCTURE(X, Y, Z) \
	struct X { \
		struct Y *array; \
		size_t used; \
		size_t size; \
		int index; \
		int index_of_same_value; \
		_Bool first_flow; \
		_Bool same_value; \
	}; \
	extern struct X *Z;

CREATE_ARRAY_STRUCTURE(neat_open_struct, properties_neat_open, neat_open_prop)
CREATE_ARRAY_STRUCTURE(jsonloads_struct, properties_jsonloads, jsonloads_prop)
CREATE_ARRAY_STRUCTURE(neat_set_struct, properties_neat_set, neat_set_prop)
CREATE_ARRAY_STRUCTURE(send_result_conn_struct, properties_send_result_conn, send_result_conn_prop)
CREATE_ARRAY_STRUCTURE(send_prop_to_pm_struct, properties_send_prop_to_pm, send_prop_to_pm_prop)
CREATE_ARRAY_STRUCTURE(send_prop_to_pm_struct2, properties_send_prop_to_pm2, send_prop_to_pm_prop2)
CREATE_ARRAY_STRUCTURE(open_resolve_struct, properties_open_resolve, open_resolve_prop)


typedef enum {
	NEAT_OPEN,
	JSON_LOADS,
	NEAT_SET,
	SEND_RESULT_CONN,
	SEND_PROP_TO_PM,
	SEND_PROP_TO_PM2
} WHAT_STRUCTURE;

struct array_prop {
	//array_parameters *array;	// doesn't really work; can't access the values in the structs --> will probably only have this struct without array_parameters - will still probably need to create 6 of them 
	struct properties_for_jsonloads *array;
	size_t used;
	size_t size;
	int index;
	/*int index_increase;
	int index_of_same_value;
	_Bool same_value;
	_Bool first_time;*/
	_Bool first_flow;
};

// if struct (union: array_parameters) removed , not sure if they have to be pointers
//extern struct array_prop *neat_open_prop, *jsonloads_prop, *neat_set_array_prop, *send_result_conn_prop, *send_prop_to_pm_prop, *send_prop_to_pm_prop2; 

extern _Bool first_flow; 
extern int index_to_replace4;
extern _Bool same;
//extern json_t *props;

//extern struct properties_for_jsonloads properties_jsonloads[10];

// for neat_set_property (after json_loads)
extern _Bool first_f;
extern int index_to_replace;
extern int index_of_same_value;
extern _Bool same_value2;
struct properties_neat_set {
	json_t *prop_before;
    json_t *val_before;
    char key_before[1000];
    json_t *flow_properties_before;
};
extern struct properties_neat_set array_properties[10];


// for send_result_connection_attempt_to_pm and nt_json_send_once_no_reply
// maybe make a struct of all the values?
extern _Bool first;
extern _Bool first2;
extern _Bool same_value;
extern _Bool same_value_jsondumps;
extern char output_buffer_before[1000];
extern json_t *prop_obj_before;
extern json_t *result_obj_before;
extern json_t *result_obj_after_set;
struct properies_for_jsonpack {
	char interface[1000]; 
	char ip[1000];	// --> test out lower values than 1000 --> see how much is necessary
    unsigned int port;
    int transport;
    _Bool result;	
};
/*struct properies_for_jsonpack {
	char interface_before[1000]; 
	char ip_before[1000];	// --> test out lower values than 1000 --> see how much is necessary
    unsigned int port_before;
    int transport_before;
    _Bool result_before;	
};*/
extern struct properies_for_jsonpack properties_jsonpack;	// send_result_conn_a; (or _array)

// for send_properties_to_pm and nt_json_send_once
extern int index_to_replace2;
extern int index_to_replace3;
extern int index_of_same_value2;
extern _Bool first3;
extern _Bool first4;
extern _Bool first5;
extern _Bool same_value_jsondumps2;
extern _Bool same_value_jsondumps3;
extern char output_buffer_before2[1000];
struct properties_send_prop_to_pm {
	int index_before;
	json_t *ipvalue_before;
	json_t *addr_before;
	char ip_before[1000];
	json_t *endpoint_before;
	char namebuf_before[1000];
	char ifa_name_before[1000];
};
// outside of one of the loops
struct properties_constant {
	json_t *req_type_before;
	json_t *properties_before;
	_Bool first_flow;
}
extern struct properties_constant *prop_const;
struct properties_send_prop_to_pm2 {
	json_t *endpoints_before;
	json_t *properties_after_set;
	json_t *port_before;
	json_t *properties_after_set2;
	uint16_t flow_port_before;
	json_t *domains_before;
	json_t *address;
	json_t *address_before;
	json_t *address_before2;
	char flow_name_before[1000];
	char address_name_before[1000];
	json_t *properties_before;
	json_t *properties_before2;
	char output_buffer_before[1000];
};
extern struct properties_send_prop_to_pm send_prop_array[10];
extern struct properties_send_prop_to_pm2 send_prop_array2[10];

// open_resolve
struct properties_open_resolve {
	json_t *ip_before;
	json_t addr_before;
	json_t *ipvalue_before;
}


//json_t *cache_value(const char *compare_value, struct properties_for_jsonloads values_from_array[], int index);
//json_t *check_if_cached(const char *compare_value, struct properties_for_jsonloads values_from_array[]);

//json_t *cache_jsonloads(const char *compare_value, WHAT_STRUCTURE s);
//json_t *cache_jsonloads(const char *compare_value, struct jsonloads_struct *a);
//void init_array(struct jsonloads_struct *a);
//void check_if_full(struct jsonloads_struct *a);

//json_t *cache_jsonobjectget(json_t* compare_value, struct neat_set_prop *a);

json_t *cache_jsonloads(const char *compare_value, struct jsonloads_struct *a);
void init_array(struct jsonloads_struct *a);
void check_if_full(struct jsonloads_struct *a);
//json_t *cache_jsonloads(const char *compare_value, struct array_prop *a, struct properties_jsonloads *b);
//void init_array(struct array_prop *a);
//_Bool check_if_full(struct array_prop *a);

json_t *cache_jsonobjectget(json_t* compare_value, struct neat_set_struct *a);
json_t *cache_jsonobjectset(json_t *compare_value, const char *key, json_t *flow_properties, struct neat_set_struct *a); 

json_t *cache_jsonobjectget2(json_t* compare_value, struct send_prop_to_pm_struct *a);
char *cache_jsondumps(json_t *json, size_t flag, struct send_prop_to_pm_struct *a);
json_t *cache_jsonpack_two_values(char *compare_value, char *compare_value2, struct send_prop_to_pm_struct *a);

json_t *cache_jsonobjectset2(json_t *compare_value, const char *key, json_t *flow_properties, struct send_prop_to_pm_struct2 *a);
json_t *cache_jsonpack2(uint16_t *compare_value, struct jsonpack_struct *a);

// at the moment variable arguments - can also try 
/*_Bool cache_first_flow(json_t* json, int index_to_replace, ...);	
_Bool check_if_cached(json_t* json_cached, ...);

char cache_json_dumps_value(char *json, _Bool condition);

_Bool cache_value(json_t *value_to_check, json_t *array_to_check_in);*/
