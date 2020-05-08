#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <uv.h>
#include <jansson.h>
#include <string.h>

#include "neat.h"
#include "neat_json_helpers.h"
#include "json_overhead.h"
#include "neat_log.h"

const size_t INITIAL_SIZE = 10;
const size_t MAX_SIZE = 50;

struct properties_constant *prop_const = &(struct properties_constant) {.first_flow = true};
struct jsonloads_struct *jsonloads_prop = &(struct jsonloads_struct) {.first_flow = true};
struct send_result_conn_struct *send_result_conn_prop = &(struct send_result_conn_struct) {.first_flow = true};
struct send_prop_to_pm_struct *send_prop_to_pm_prop = &(struct send_prop_to_pm_struct) {.first_flow = true};
struct jsonpack_two_keys_struct *jsonpack_two_keys_prop = &(struct jsonpack_two_keys_struct) {.first_flow = true};
struct jsondumps_struct *jsondumps_prop = &(struct jsondumps_struct) {.first_flow = true};
struct jsonpack_char_key_struct *jsonpack_char_key_prop = &(struct jsonpack_char_key_struct) {.first_flow = true};


#define init_array(a) { \
		a->array = malloc(INITIAL_SIZE * sizeof(*a->array)); \
		a->used = 1; \
		a->size = INITIAL_SIZE; \
		a->index = 0; \
	}

#define check_if_full(a) { \
		if (a->size == a->used) { \
			a->size *= 2; \
			a->array = realloc(a->array, a->size * sizeof(*a->array)); \
		} \
		a->index++; \
		a->used++; \
	}

/*#define check_if_full(a) { \
		if (a->size == a->used) { \
			if (a->size*2 < MAX_SIZE) { \
				a->size *= 2; \
				a->array = realloc(a->array, a->size * sizeof(*a->array)); \
			} else { \
				a->index = 0; \
				a->full_array = true; \
				return; \
		} \
		a->index++; \
		a->used++; \
	}

#define what_index(a) { \
			if (a->index < a->size) { \
				a->index++; \
			} else { \
				a->index = 0; \
			} \
		} \

// this is how it would look in functions (also change names to INITIAL_SIZE and MAX_SIZE)
if (!a->full_array) {
	check_if_full(a);
} else {
	what_index(a);
	json_decref(a->array[a->index].___);
}*/

// for json_loads 
json_t *cache_jsonloads(const char *compare_value, struct jsonloads_struct *a) {
	json_t *props_temp;
    json_error_t error;

	for (unsigned i = 0; i < a->used; ++i) {	
		if (strcmp(compare_value, a->array[i].properties_before) == 0) {	 
			return a->array[i].props_before;												
		}	
	}
	if (!a->first_flow) {
		check_if_full(a);
	} else {
		init_array(a);
		a->first_flow = false;
	}
	props_temp = json_loads(compare_value, 0, &error);
	a->array[a->index].props_before = props_temp;
	strcpy(a->array[a->index].properties_before, compare_value);
	return props_temp;
}

// for json_dumps
char *cache_jsondumps(json_t *json, size_t flag, struct jsondumps_struct *a) {
	char *output_buffer_temp;

	for (unsigned i = 0; i < a->used; ++i) {
		if (json_equal(a->array[i].key, json)) {
			return a->array[i].output_buffer_before;
		}
	}
	if (!a->first_flow) {
		check_if_full(a);
	} else {
		init_array(a);
		a->first_flow = false;
	}
	output_buffer_temp = json_dumps(json, flag);
	//json_incref(json);	// only really necessary for the one... "jsondumps2"
	a->array[a->index].key = json;
	strcpy(a->array[a->index].output_buffer_before, output_buffer_temp);
	free(output_buffer_temp);
	return a->array[a->index].output_buffer_before;
}

// for send_properties_to_pm  
json_t *cache_jsonpack_two_values(char *compare_value, char *compare_value2, struct jsonpack_two_keys_struct *a) {
	json_t *endpoint_temp;

	for (unsigned i = 0; i < a->used; ++i) {
		if (strcmp(a->array[i].namebuf_before, compare_value) == 0 && strcmp(a->array[i].ifa_name_before, compare_value2) == 0) { 
			return a->array[i].endpoint_before;
		}
	}
	if (!a->first_flow) {
		check_if_full(a);
	} else {
		init_array(a);
		a->first_flow = false;
	}
	endpoint_temp = json_pack("{ss++si}", "value", compare_value, "@", compare_value2, "precedence", 2);	
	strcpy(a->array[a->index].namebuf_before, compare_value);	
	strcpy(a->array[a->index].ifa_name_before, compare_value2);
	a->array[a->index].endpoint_before = endpoint_temp;	
	return endpoint_temp;
}

json_t *cache_jsonpack_uint_key(uint16_t compare_value, struct send_prop_to_pm_struct *a) {
	json_t *value_temp;

	for (unsigned i = 0; i < a->used; ++i) {
		if (a->array[i].flow_port_before == compare_value) { 
			return a->array[i].port_before;
		}
	}
	if (!a->first_flow) {
		check_if_full(a);
	} else {
		init_array(a);
		//a->first_flow = false;
	}
	value_temp = json_pack("{sisi}", "value", compare_value, "precedence", 2);	
	a->array[a->index].flow_port_before = compare_value;
	a->array[a->index].port_before = value_temp;	
	return value_temp;	// change struct to jsonpack_uint_key_struct
}

json_t *cache_jsonpack_simple(struct properties_constant *a) {	
	json_t *req_type_temp;

	if (!a->first_flow) {
		req_type_temp = a->req_type_before;
	} else {
		a->first_flow = false;
		req_type_temp = json_pack("{s:s}", "value", "pre-resolve");
		a->req_type_before = req_type_temp;
	}
	return req_type_temp;
}

json_t *cache_jsonpack_char_key(const char *compare_value, struct jsonpack_char_key_struct *a) {
	json_t *address_temp;

	for (unsigned i = 0; i < a->used; ++i) {
		if (strcmp(a->array[i].key, compare_value) == 0) { 
			return a->array[i].address_before;
		}
	}
	if (!a->first_flow) {
		check_if_full(a);
	} else {
		init_array(a);
		a->first_flow = false;
	}
	address_temp = json_pack("{sssi}", "value", compare_value, "precedence", 2);	
	strcpy(a->array[a->index].key, compare_value);	
	a->array[a->index].address_before = address_temp;
	return address_temp;
}

char *cache_jsondumps2(json_t *json, size_t flag, struct send_prop_to_pm_struct *a) {
	char *output_buffer_temp;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->used; ++i) {
			if (json_equal(a->array[i].json_value, json)) {	// json_equal(a->array[i].json_value, json) --> get invalid read size (probably since reading empty parameter) 	a->array[i].json_value == json
				return a->array[i].output_buffer_before;
			}
		}
	} else {
		a->first_flow = false;
	}
	output_buffer_temp = json_dumps(json, flag);
	json_incref(json);
	a->array[a->index].json_value = json;
	strcpy(a->array[a->index].output_buffer_before, output_buffer_temp);
	free(output_buffer_temp);
	return a->array[a->index].output_buffer_before;
}

// for send_result_connection
json_t *cache_jsonpack_four_values(char *compare_ip, unsigned int compare_port, int compare_transport, _Bool compare_result, struct send_result_conn_struct *a) {
	json_t *prop_obj_temp;

	for (unsigned i = 0; i < a->used; ++i) {	
		if (a->array[i].transport_before == compare_transport && a->array[i].port_before == compare_port && strcmp(compare_ip, a->array[i].ip_before) == 0 && a->array[i].result_before == compare_result) {	 
			return a->array[i].prop_obj_before;												
		}	
	}
	if (!a->first_flow) {
		check_if_full(a);
	} else {
		init_array(a);
		//a->first_flow = false;
	}
	prop_obj_temp = json_pack("{s:{s:s},s:{s:s},s:{s:i},s:{s:b,s:i},s:{s:b}}",
        "transport", "value", stack_to_string(compare_transport ),
        "remote_ip", "value", compare_ip,
        "port", "value", compare_port,
        "__he_candidate_success", "value", compare_result, "score", compare_result?5:-5,
        "__cached", "value", 1);
	a->array[a->index].prop_obj_before = prop_obj_temp;
	a->array[a->index].transport_before = compare_transport;
	a->array[a->index].port_before = compare_port;
	strcpy(a->array[a->index].ip_before, compare_ip);
	a->array[a->index].result_before = compare_result;
	return prop_obj_temp;
}

json_t *cache_jsonpack(char *compare_value, struct send_result_conn_struct *a) {
	json_t *result_obj_temp;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->used; ++i) {	
			if (strcmp(compare_value, a->array[i].interface_before) == 0) {	 
				result_obj_temp = a->array[i].result_obj_before;	// don't need to assign, can just return	
				a->same_value = true;
				a->index_of_same_value = i;					
				return result_obj_temp;												
			}	
		}
	} else {
		a->first_flow = false;
	}
	a->same_value = false;
	result_obj_temp = json_pack("{s:[{s:{ss}}],s:b}",
    "match", "interface", "value", compare_value, "link", true);
	a->array[a->index].result_obj_before = result_obj_temp;
	strcpy(a->array[a->index].interface_before, compare_value);
	return result_obj_temp;
}

char *cache_jsondumps_dependent(json_t *json, size_t flag, struct send_result_conn_struct *a) {	// should change its name to cache_jsondumps_dependent() or something 
	char *output_buffer_temp;

	if (a->same_value) {	
		a->same_value = false;
		return a->array[a->index_of_same_value].output_buffer_before;
	}
	output_buffer_temp = json_dumps(json, flag);
	strcpy(a->array[a->index].output_buffer_before, output_buffer_temp);
	free(output_buffer_temp);
	return a->array[a->index].output_buffer_before;
}