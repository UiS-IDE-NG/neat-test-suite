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
//#include "uthash.h"

const size_t array_initial_size = 10;

// for neat_open
_Bool first_flow2 = true;
int index_to_replace7 = 0;
struct properties_neat_open neat_open_prop_array[10];

// for json_loads
_Bool first_flow = true;
int index_to_replace4 = 0;
_Bool same = false;
//json_t *props;
//struct properties_jsonloads properties_jsonloads[10];
struct properties_jsonloads *jsonloads_a;

// for neat_set_property (after json_loads)
_Bool first_f = true;
int index_to_replace = 0;
int index_of_same_value;
_Bool same_value2 = false;
struct properties_neat_set array_properties[10];


// for send_result_connection_attempt_to_pm and nt_json_send_once_no_reply
// create an array of prop_obj_before??? or something too
json_t *prop_obj_before;
json_t *result_obj_before;
json_t *result_obj_after_set;
_Bool first = true;
_Bool first2 = true;
_Bool same_value = false;
_Bool same_value_jsondumps = false;
char output_buffer_before[1000];

//struct properies_send_result_conn properties_jsonpack;
//struct properies_for_jsonpack properties_jsonpack[10];

// for send_properties_to_pm and nt_json_send_once
// many values are repeated --> can create a struct and create several extern types of them (depending on how many that are needed)
int index_to_replace2 = 0;
int index_to_replace3 = 0;
int index_of_same_value2;
_Bool first3 = true;
_Bool first4 = true;
_Bool first5 = true;
_Bool same_value_jsondumps2 = false;
_Bool same_value_jsondumps3 = false;
char output_buffer_before2[1000];
struct properties_send_prop_to_pm send_prop_array[10];
json_t *req_type_before;
json_t *properties_before;
struct properties_send_prop_to_pm2 send_prop_array2[10];

// for json_loads()
/*json_t *cache_value(const char *compare_value, struct properties_for_jsonloads values_from_array[], int index) {
	json_t *props_temp;
    json_error_t error;

	props_temp = json_loads(compare_value, 0, &error);
	values_from_array[index].props_before = props_temp;
    strcpy(values_from_array[index].properties_before, compare_value);
    return props_temp;
}

json_t *check_if_cached(const char *compare_value, struct properties_for_jsonloads values_from_array[]) {
	json_t *props_temp = NULL;
	for (int i = 0; i < 10; ++i) {	// sizeof(properties_jsonloads) / sizeof(properties_jsonloads[0])
		if (strcmp(compare_value, values_from_array[i].properties_before) == 0) {
            props_temp = values_from_array[i].props_before;
            break;
        } 
	}
	return props_temp;
}*/

// if something doesn't work, can try and not initialize first flow (can probably just cut it from the struct then)
//#define INIT_ARRAY_PROP(X) struct array_prop *X = &(struct array_prop) {.first_flow = true};

struct properties_constant *prop_const = &(struct properties_constant) {.first_flow = true};

struct neat_open_struct *neat_open_prop = &(struct neat_open_struct) {.first_flow = true};
struct jsonloads_struct *jsonloads_prop = &(struct jsonloads_struct) {.first_flow = true};
struct neat_set_struct *neat_set_prop = &(struct neat_set_struct) {.first_flow = true};
struct send_result_conn_struct *send_result_conn_prop = &(struct send_result_conn_struct) {.first_flow = true};
struct send_prop_to_pm_struct *send_prop_to_pm_prop = &(struct send_prop_to_pm_struct) {.first_flow = true};
struct send_prop_to_pm_struct2 *send_prop_to_pm_prop2 = &(struct send_prop_to_pm_struct2) {.first_flow = true};
struct open_resolve_struct *open_resolve_prop = &(struct open_resolve_struct) {.first_flow = true};


/*INIT_ARRAY_PROP(neat_open_prop)
INIT_ARRAY_PROP(jsonloads_prop)
INIT_ARRAY_PROP(neat_set_prop)
INIT_ARRAY_PROP(send_result_conn_prop)
INIT_ARRAY_PROP(send_prop_to_pm_prop)
INIT_ARRAY_PROP(send_prop_to_pm_prop2)*/

// can't initialize a pointer i think before malloc has been used... can't really use first flow then --> can try to not use a pointer and just the struct directly though
//struct array_prop *neat_open_prop = &{ ->first_flow = true}, *jsonloads_prop = { ->first_flow = true}, *neat_set_array_prop = { ->first_flow = true}, *send_result_conn_prop = { ->first_flow = true}, *send_prop_to_pm_prop = { ->first_flow = true}, *send_prop_to_pm_prop2 = { ->first_flow = true};	// make them pointers?

/*void init_array(struct array_prop *a) {	// initial size doesn't really need to be there if the initialsize is to be same all the time --> define it somewhere then
	//a->array = malloc(array_initial_size * sizeof(array_parameters));
	//a->array = malloc(array_initial_size * sizeof(struct properties_jsonloads));
	a->used = 0;
	a->size = array_initial_size;
	a->index = 0;
}

_Bool check_if_full(struct array_prop *a) {	// may return true/false instead
	a->used++;
	a->index++;
	if (a->size < a->used) {	// since used is increased before
		a->size *= 2;
		//a->array = realloc(a->array, a->size * sizeof(array_parameters));
		//a->array = realloc(a->array, a->size * sizeof(struct properties_jsonloads));
		return true;
	}
	return false;
}*/

/*void init_array(struct jsonloads_struct *a) {	// initial size doesn't really need to be there if the initialsize is to be same all the time --> define it somewhere then
	//a->array = malloc(array_initial_size * sizeof(array_parameters));
	a->array = malloc(array_initial_size * sizeof(*a->array));
	a->used = 0;
	a->size = array_initial_size;
	a->index = 0;
}

void check_if_full(struct jsonloads_struct *a) {	// may return true/false instead
	if (a->size == a->used) {
		a->size *= 2;
		//a->array = realloc(a->array, a->size * sizeof(array_parameters));
		a->array = realloc(a->array, a->size * sizeof(*a->array));
	}
	a->used++;
	a->index++;
}*/

// can use macro, but it will lead to higher cpu time so...
#define init_array(a) { \
		a->array = malloc(array_initial_size * sizeof(*a->array)); \
		a->used = 0; \
		a->size = array_initial_size; \
		a->index = 0; \
	}

#define check_if_full(a) { \
		if (a->size == a->used) { \
			a->size *= 2; \
			a->array = realloc(a->array, a->size * sizeof(*a->array)); \
		} \
		a->used++; \
		a->index++; \
	}

/*
if (a->first) { \
	if (a->size == a->used) { \
		a->size *= 2; \
		a->array = realloc(a->array, a->size * sizeof(*a->array)); \
		} \
	a->used++; \
	a->index++; \
	a->first = false; \
} \
if (a->last) { \
	a->last = false; \
} \
*/

/*
will not really work (more like not necessary with how it is defined atm), but just remember that it needs to be freed in the end
should create an entire thing (function where all the global variables gets freed or something...)
can always create a macro for this too................... (if there really is nothing better)
void free_array(struct array_prop *a) {
	free(a->array);
	a->array = NULL;
	a->used = a->size = 0;
}
*/

// this function is in nt_free_flow --> test if correct with memory sampling
// cause aborted messages when run in script so atm commented out
// also test "normal" (aka no improvements) with memory sampling to see if there is memory leaks then
// may call it free_memory_of_arrays or free_memory_of_cached_values or just free_cached_values()
void free_memory() {	// may also need to json_decref struff, will see. 
	free(neat_open_prop->array);
	free(jsonloads_prop->array);
	free(neat_set_prop->array);
	free(send_result_conn_prop->array);
	free(send_prop_to_pm_prop->array);
	free(send_prop_to_pm_prop2->array);
	free(open_resolve_prop->array);
}

json_t *cache_jsonloads(const char *compare_value, struct jsonloads_struct *a) {
	json_t *props_temp;
    json_error_t error;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {	
			if (strcmp(compare_value, a->array[i].properties_before) == 0) {	 
				props_temp = a->array[i].props_before;	// don't need to assign, can just return						
				return props_temp;												
			}	
		}
	}
	if (!a->first_flow) {
		json_decref(a->array[a->index].props_before);
		check_if_full(a);
	} else {
		init_array(a);
		a->first_flow = false;	// only for the last "function"/ instance that will use it
	}
	props_temp = json_loads(compare_value, 0, &error);
	a->array[a->index].props_before = props_temp;
	strcpy(a->array[a->index].properties_before, compare_value);
	return props_temp;
}

// also, do the props_temp value need to be json_decref? (no leaks if not at least)
// this does not work since b will only be allocated localy, not globaly even though the parameter/argument is a global variabel
/*json_t *cache_jsonloads(const char *compare_value, struct array_prop *a, struct properties_jsonloads *b) {	// have to add struct properties_for_jsonloads as parameter probably
	json_t *props_temp;
    json_error_t error;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {	
			if (strcmp(compare_value, b[i].properties_before) == 0) {	 
				props_temp = b[i].props_before;	// don't need to assign, can just return						
				return props_temp;												
			}	
		}
	}
	if (!a->first_flow) {
		json_decref(b[a->index].props_before);
		if (check_if_full(a)) {	// better name: if_array_full
			b = realloc(a->array, a->size * sizeof(*b)); 	// struct properties_jsonloads
		}	
	} else {
		b = malloc(array_initial_size * sizeof(*b));
		init_array(a);
		a->first_flow = false;	// only for the last "function"/ instance that will use it
	}
	props_temp = json_loads(compare_value, 0, &error);
	b[a->index].props_before = props_temp;
	strcpy(b[a->index].properties_before, compare_value);
	return props_temp;
}*/

// not sure if a struct is really needed, can just add json_t directly in the other struct (this depends on if that struct is gonna include this, or if i'm just gonna have two not connected structs)
/*struct jsondumps_prop
{	
	json_t *output_buffer_before;	// value
	_Bool same_value_jsondumps;	// "key": initialize to false, or set it false/true depending of what is true (only true is set atm), or it can be set false on initialization, the function (probably this)
};*/

// think this will for most jsondumps functions as long as they use the same struct 
/*char *cache_jsondumps(json_t *json, size_t flag, struct array_prop *a) {
	char *output_buffer_temp;

	if (a->first_flow) {
		init_array(a);	// only if first in struct
		a->first_flow = false; // this can be inside init_array (maybe, depends on how the structures are set)
	} else {
		if (same_value_jsondumps) {	// may be inside the struct instead (probably call just call it "same" or "same_value")
			same_value_jsondumps = false;
			return a->array[index_of_same_value].output_buffer_before; // index_of_same_value may be inside struct
		} else {
			check_if_full(a);
		}
	}
	output_buffer_temp = json_dumps(json, flag);
	strcpy(a->array[a->index].output_buffer_before, output_buffer_temp);
	free(output_buffer_temp);
	return a->array[a->index].output_buffer_before;
}*/


// for neat_set_property
json_t *cache_jsonobjectget(json_t* compare_value, struct neat_set_struct *a) {
	json_t *val_temp;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {
			if (json_equal(a->array[i].prop_before, compare_value)) { // a->array[i].prop_before == compare_value
				val_temp = a->array[i].val_before;
				a->same_value = true;				// these are general values, 
				a->index_of_same_value = i;	// if needed more, add them to the structure
				return val_temp;
			}
		}
	}
	if (!a->first_flow) {
		// is json_decref needed?
		check_if_full(a);
	} else {
		init_array(a);
		a->first_flow = false;	// only for the last "function"/ instance that will use it
	}
	val_temp = json_object_get(compare_value, "value");		// possible to make this a function on its own, butt i'm not sure if i can make it general without them being in the same structure
	a->array[a->index].val_before = val_temp;				// need to put the values in the struct into the structure in that case, which means the all need to store the same type of values
	a->array[a->index].prop_before = compare_value;
	return val_temp;
}

// leaks memory --> think i need to use json_decref on flow->properties somewhere
json_t *cache_jsonobjectset(json_t *compare_value, const char *key, json_t *flow_properties, struct neat_set_struct *a) {
	//json_t *flow_properties_temp = NULL;

	if (!a->first_flow) {
		if (a->same_value) {
			//flow_properties_temp = a->array[index_of_same_value].flow_properties_before;	// can just return, no ned to assign too
			a->same_value = false;
			return a->array[a->index_of_same_value].flow_properties_before;
		}
	}
	if (!a->first_flow) {
		// this should also only be done once per struct (actually the function should be changed to account for this - the last instance should increase it - maybe two indexes? where one is increased
		// if they are the same and then have some value/flag (boolean value probably) that checks if the other value should be increased too --> basically one of the indexes is used if first flow and
		// same value and the other if there is no matches)
		//check_if_full(a);	// should not be here for reasons above
	} else {
		// jsondecref?
		a->first_flow = false;	// don't need to initialize the array as it has already been done
	}
	json_object_set(flow_properties, key, compare_value);
	a->array[a->index].flow_properties_before = flow_properties;
	return flow_properties;
}

// --> to make it easier to use the same functions, name alike values the same? --> they are so very alike!!! only the stuct differentiate them
// possible alike names: key and value, key_temp ?
// try and make the structures more alike this way - but at the same time, there will most likely be more than one key and value pair (and sometimes more than one value per key)
// can always just have each instance in their own structure; can make a structure for each function (probably more depending on how many values each have)
// let's see what works best

// for send_properties_to_pm  
json_t *cache_jsonobjectget2(json_t* compare_value, struct send_prop_to_pm_struct *a) {
	json_t * ipvalue_temp;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {
			if (a->array[i].addr_before == compare_value) {	// json_equal(a->array[i].addr_before, addr)
				ipvalue_temp = a->array[i].ipvalue_before;
				a->same_value = true;				 
				a->index_of_same_value = i;	
				return ipvalue_temp;
			}
		}
	}
	if (!a->first_flow) {
		// is json_decref needed?
		check_if_full(a);
	} else {
		init_array(a);
		//a->first_flow = false;	// only for the last "function"/ instance that will use it
	}
	ipvalue_temp = json_object_get(compare_value, "value");		// possible to make this a function on its own, butt i'm not sure if i can make it general without them being in the same structure
	a->array[a->index].ipvalue_before = ipvalue_temp;				// need to put the values in the struct into the structure in that case, which means the all need to store the same type of values
	a->array[a->index].addr_before = compare_value;
	return ipvalue_temp;
}

char *cache_jsondumps(json_t *json, size_t flag, struct send_prop_to_pm_struct *a) {
	char *output_buffer_temp;

	/*if (a->first_flow) {
		//init_array(a);	// only if first in struct
		a->first_flow = false; // this can be inside init_array (maybe, depends on how the structures are set)
	} else {*/
	if (!a->first_flow) {
		if (a->same_value) {	// may be inside the struct instead (probably call just call it "same" or "same_value")
			a->same_value = false;
			return a->array[a->index_of_same_value].ip_before; // index_of_same_value may be inside struct
		} else {
			check_if_full(a);
		}
	}
	output_buffer_temp = json_dumps(json, flag);
	strcpy(a->array[a->index].ip_before, output_buffer_temp);
	free(output_buffer_temp);
	return a->array[a->index].ip_before;
}

json_t *cache_jsonpack_two_values(char *compare_value, char *compare_value2, struct send_prop_to_pm_struct *a) {		// this function works for these specific three functions that are used here (overall hard to make specific functions for jsonpack)
	json_t *endpoint_temp;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {
			if (strcmp(a->array[i].namebuf_before, compare_value) == 0 && strcmp(a->array[i].ifa_name_before, compare_value2) == 0) { 
				endpoint_temp = a->array[i].endpoint_before;
				return endpoint_temp;
			}
		}
	}
	if (!a->first_flow) {
		// json_decref();
		check_if_full(a);
	} else {
		//init_array(a);
		a->first_flow = false;	// only for the last "function"/ instance that will use it
	}
	endpoint_temp = json_pack("{ss++si}", "value", compare_value, "@", compare_value2, "precedence", 2);	
	strcpy(a->array[a->index].namebuf_before, compare_value);	
	strcpy(a->array[a->index].ifa_name_before, compare_value2);
	a->array[a->index].endpoint_before = endpoint_temp;				
	return endpoint_temp;
}

json_t *cache_jsonobjectset2(json_t *compare_value, const char *key, json_t *flow_properties, struct send_prop_to_pm_struct2 *a) {
	//json_t * value_temp;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {
			if (a->array[i].endpoints_before == compare_value) {  // json_equal(a->array[i].endpoints_before, compare_value)
				//value_temp = a->array[i].properties_after_set;	// can just return, no ned to assign too
				return a->array[i].properties_after_set;
			}
		}
	}
	if (!a->first_flow) {
		// this should also only be done once per struct (actually the function should be changed to account for this - the last instance should increase it - maybe two indexes? where one is increased
		// if they are the same and then have some value/flag (boolean value probably) that checks if the other value should be increased too --> basically one of the indexes is used if first flow and
		// same value and the other if there is no matches)
		// jsondecref?
		check_if_full(a);	
	} else {
		init_array(a);
		//a->first_flow = false;	// don't need to initialize the array as it has already been done
	}
	json_object_set(flow_properties, key, compare_value);
	a->array[a->index].properties_after_set = flow_properties;
	return flow_properties;	// have an if statement that says if not NULL and then assign flow->properties = return value
}

json_t *cache_jsonpack2(uint16_t compare_value, struct send_prop_to_pm_struct2 *a) {
	json_t *value_temp;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {
			if (a->array[i].flow_port_before == compare_value) { 
				value_temp = a->array[i].port_before;
				a->same_value = true;				 
				a->index_of_same_value = i;
				return value_temp;
			}
		}
	}
	if (!a->first_flow) {
		// json_decref()
		check_if_full(a);
	} else {
		init_array(a);
		a->first_flow = false;	// only for the last "function"/ instance that will use it
	}
	value_temp = json_pack("{s:[{s:{ss}}],s:b}", "match", "interface", "value", compare_value, "link", true);	
	a->array[a->index].flow_port_before = compare_value;
	a->array[a->index].port_before = value_temp;				
	return value_temp;
}

json_t *cache_jsonobjectset3(json_t *compare_value, const char *key, json_t *flow_properties, struct send_prop_to_pm_struct2 *a) {
	//json_t *flow_properties_temp = NULL;

	if (!a->first_flow) {
		if (a->same_value) {
			//flow_properties_temp = a->array[index_of_same_value].flow_properties_before;	// can just return, no ned to assign too
			a->same_value = false;
			return a->array[a->index_of_same_value].properties_after_set2;
		}
	}
	if (!a->first_flow) {
		//check_if_full(a);	// should not be here for reasons above
	} else {
		// jsondecref?
		a->first_flow = false;	// don't need to initialize the array as it has already been done
	}
	json_object_set(flow_properties, key, compare_value);
	a->array[a->index].properties_after_set2 = flow_properties;
	return flow_properties;	// have an if statement that says if not NULL and then assign flow->properties = return value
}

json_t *cache_jsonpack_simple(struct properties_constant *a) {	// to make it general add the values that need to be added (maybe in the form of a struct with all the needed alternatives?)
	json_t *req_type_temp;

	if (!a->first_flow) {
		req_type_temp = a->req_type_before;
	} else {
		a->first_flow = false;	// should not be here, but this works (jsonobjectset leaks memory though) --> check why in jsonobject simple
		req_type_temp = json_pack("{s:s}", "value", "pre-resolve");
		a->req_type_before = req_type_temp;
	}
	return req_type_temp;
}

json_t *cache_jsonobjectset_simple(json_t *compare_value, const char *key, json_t *properties, struct properties_constant *a) {
	if (a->first_flow) {
		json_object_set(properties, key, compare_value);
		a->properties_before = properties;
		a->first_flow = false;
	} else {
		properties = a->properties_before;
	}
	return properties;
}

json_t *cache_jsonpack3(const char *compare_value, struct send_prop_to_pm_struct2 *a) {		// can work for two instances
	json_t *address_temp;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {
			if (strcmp(a->array[i].flow_name_before, compare_value) == 0) { 
				address_temp = a->array[i].address_before;
				a->same_value = true;
				a->index_of_same_value = i;
				return address_temp;
			}
		}
	}
	if (!a->first_flow) {
		// json_decref();
		check_if_full(a);
	} else {
		init_array(a);
		//a->first_flow = false;	// only for the last "function"/ instance that will use it
	}
	address_temp = json_pack("{sssi}", "value", compare_value, "precedence", 2);	
	strcpy(a->array[a->index].flow_name_before, compare_value);	
	a->array[a->index].address_before = address_temp;				
	return address_temp;
}

json_t *cache_jsonobjectset4(json_t *compare_value, const char *key, json_t *flow_properties, struct send_prop_to_pm_struct2 *a) {
	if (!a->first_flow) {
		if (a->same_value) {
			//flow_properties_temp = a->array[index_of_same_value].flow_properties_before;	// can just return, no ned to assign too
			//a->same_value = false; don't set it to false since it will be true again for json_dumps
			return a->array[a->index_of_same_value].properties_before;
		}
	}
	if (!a->first_flow) {
		// jsondecref?
		//check_if_full(a);	// should not be here for reasons above -- but need to increase the index (or not really as it should have been done earlier, but at the same time)
	} else {
		a->first_flow = false;	// don't need to initialize the array as it has already been done
	}
	json_object_set(flow_properties, key, compare_value);
	a->array[a->index].properties_before = flow_properties;
	return flow_properties;	// have an if statement that says if not NULL and then assign flow->properties = return value
}

json_t *cache_jsonpack4(char *compare_value, struct send_prop_to_pm_struct2 *a) {		// can work for two instances
	json_t *address_temp;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {
			if (strcmp(a->array[i].address_name_before, compare_value) == 0) { 
				address_temp = a->array[i].address_before2;
				return address_temp;
			}
		}
	}
	if (!a->first_flow) {
		// json_decref();
		check_if_full(a);
	} else {
		init_array(a);
		a->first_flow = false;	// only for the last "function"/ instance that will use it
	}
	address_temp = json_pack("{sssi}", "value", compare_value, "precedence", 2);	
	strcpy(a->array[a->index].address_name_before, compare_value);	
	a->array[a->index].address_before2 = address_temp;				
	return address_temp;
}

json_t *cache_jsonobjectset5(json_t *compare_value, const char *key, json_t *flow_properties, struct send_prop_to_pm_struct2 *a) {
	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {
			if (a->array[i].domains_before == compare_value) {	// json_equal(a->array[i].domains_before, compare_value)
				a->same_value = true;
				a->index_of_same_value = i;
				return  a->array[i].properties_before2;
			}
		}
	}
	if (!a->first_flow) {
		// jsondecref?
		//check_if_full(a);	
	} else {
		//init_array(a);
		a->first_flow = false;	// don't need to initialize the array as it has already been done
	}
	json_object_set(flow_properties, key, compare_value);
	a->array[a->index].properties_before2 = flow_properties;
	a->array[a->index].domains_before = flow_properties;
	return flow_properties;	// have an if statement that says if not NULL and then assign flow->properties = return value
}

char *cache_jsondumps2(json_t *json, size_t flag, struct send_prop_to_pm_struct2 *a) {
	char *output_buffer_temp;

	if (a->first_flow) {
		//init_array(a);	// only if first in struct
		a->first_flow = false; // this can be inside init_array (maybe, depends on how the structures are set)
	} else {
		if (a->same_value) {	// may be inside the struct instead (probably call just call it "same" or "same_value")
			a->same_value = false;
			return a->array[a->index_of_same_value].output_buffer_before; // index_of_same_value may be inside struct
		} else {
			//check_if_full(a);
		}
	}
	output_buffer_temp = json_dumps(json, flag);
	strcpy(a->array[a->index].output_buffer_before, output_buffer_temp);
	free(output_buffer_temp);
	return a->array[a->index].output_buffer_before;
}


// for send_result_connection
json_t *cache_jsonpack_four_values(char *compare_ip, unsigned int compare_port, int compare_transport, _Bool compare_result, struct send_result_conn_struct *a) {
	json_t *prop_obj_temp;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {	
			if (a->array[i].transport_before == compare_transport && a->array[i].port_before == compare_port && strcmp(compare_ip, a->array[i].ip_before) == 0 && a->array[i].result_before == compare_result) {	 
				prop_obj_temp = a->array[i].prop_obj_before;	// don't need to assign, can just return						
				return prop_obj_temp;												
			}	
		}
	}
	if (!a->first_flow) {
		json_decref(a->array[a->index].prop_obj_before);
		check_if_full(a);
	} else {
		init_array(a);
		//a->first_flow = false;	// only for the last "function"/ instance that will use it
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
		for (unsigned i = 0; i < a->size; ++i) {	
			if (strcmp(compare_value, a->array[i].interface_before) == 0) {	 
				result_obj_temp = a->array[i].result_obj_before;	// don't need to assign, can just return	
				a->same_value = true;
				a->index_of_same_value = i;					
				return result_obj_temp;												
			}	
		}
	}
	if (!a->first_flow) {
		//json_decref(a->array[a->index].result_obj_before);
		//check_if_full(a);	
	} else {
		//init_array(a);
		//a->first_flow = false;	// only for the last "function"/ instance that will use it
	}
	result_obj_temp = json_pack("{s:[{s:{ss}}],s:b}",
    "match", "interface", "value", compare_value, "link", true);
	a->array[a->index].result_obj_before = result_obj_temp;
	strcpy(a->array[a->index].interface_before, compare_value);
	return result_obj_temp;
}

json_t *cache_jsonobjectset6(json_t *compare_value, const char *key, json_t *flow_properties, struct send_result_conn_struct *a) {
	json_t * flow_properties_temp = NULL;

	if (!a->first_flow) {
		if (same) {
			//flow_properties_temp = a->array[index_of_same_value].flow_properties_before;	// can just return, no ned to assign too
			return a->array[index_of_same_value].result_obj_after_set;
		}
	}
	if (!a->first_flow) {
		// this should also only be done once per struct (actually the function should be changed to account for this - the last instance should increase it - maybe two indexes? where one is increased
		// if they are the same and then have some value/flag (boolean value probably) that checks if the other value should be increased too --> basically one of the indexes is used if first flow and
		// same value and the other if there is no matches)
		//check_if_full(a);	// really need to update this
	} else {
		// jsondecref?
		//a->first_flow = false;	// don't need to initialize the array as it has already been done
	}
	if (json_object_set(flow_properties, key, compare_value) == -1) {
		return flow_properties_temp; 	// is NULL
	}
	a->array[a->index].result_obj_after_set = flow_properties;
	return flow_properties;	// have an if statement that says if not NULL and then assign flow->properties = return value
}

char *cache_jsondumps3(json_t *json, size_t flag, struct send_result_conn_struct *a) {
	char *output_buffer_temp;

	if (a->first_flow) {
		//init_array(a);	// only if first in struct
		a->first_flow = false; // this can be inside init_array (maybe, depends on how the structures are set)
	} else {
	//if (!a->first_flow) {
		if (a->same_value) {	// may be inside the struct instead (probably call just call it "same" or "same_value")
			a->same_value = false;
			return a->array[a->index_of_same_value].output_buffer_before; // index_of_same_value may be inside struct
		} else {
			//check_if_full(a);
		}
	}
	output_buffer_temp = json_dumps(json, flag);
	strcpy(a->array[a->index].output_buffer_before, output_buffer_temp);
	free(output_buffer_temp);
	return a->array[a->index].output_buffer_before;
}


// open_resolve
json_t *cache_jsonobjectget3(json_t *compare_value, struct open_resolve_struct *a) {
	json_t *val_temp;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {
			if (a->array[i].addr_before == compare_value) { // a->array[i].addr_before == compare_value json_equal(a->array[i].addr_before, compare_value)
				val_temp = a->array[i].ipvalue_before;
				a->same_value = true;				// these are general values, 
				a->index_of_same_value = i;	// if needed more, add them to the structure
				return val_temp;
			}
		}
	}
	if (!a->first_flow) {
		// is json_decref needed?
		check_if_full(a);
	} else {
		init_array(a);
		//a->first_flow = false;	// only for the last "function"/ instance that will use it
	}
	val_temp = json_object_get(compare_value, "value");		// possible to make this a function on its own, butt i'm not sure if i can make it general without them being in the same structure
	a->array[a->index].ipvalue_before = val_temp;				// need to put the values in the struct into the structure in that case, which means the all need to store the same type of values
	a->array[a->index].addr_before = compare_value;
	return val_temp;
}

char *cache_jsondumps4(json_t *json, size_t flag, struct open_resolve_struct *a) {
	char *output_buffer_temp;

	if (a->first_flow) {
		//init_array(a);	// only if first in struct
		a->first_flow = false; // this can be inside init_array (maybe, depends on how the structures are set)
	} else {
	//if (!a->first_flow) {
		if (a->same_value) {	// may be inside the struct instead (probably call just call it "same" or "same_value")
			a->same_value = false;
			return a->array[a->index_of_same_value].ip_before; // index_of_same_value may be inside struct
		} else {
			check_if_full(a);
		}
	}
	output_buffer_temp = json_dumps(json, flag);
	strcpy(a->array[a->index].ip_before, output_buffer_temp);
	free(output_buffer_temp);
	return a->array[a->index].ip_before;
}


// neat_open - doesn't really decrease the cpu usage (more like increase it) --> try without functions (do it directly like before)
_Bool cached_values(json_t *compare_value, struct neat_open_struct *a) {
	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {
			if (json_equal(a->array[i].flow_properties_before, compare_value)) {	// a->array[i].flow_properties_before == compare_value json_equal(a->array[i].flow_properties_before, compare_value)
				return true;
			}
		}
	} else {
		init_array(a);
		//a->first_flow = false;
	}
	return false;
}

void *cache_jsonobjectget_many(json_t *value, unsigned int value2, unsigned int value3, unsigned int value4, json_t *value5, struct neat_open_struct *a) {
	if (!a->first_flow) {
		check_if_full(a);
	} else {
		a->first_flow = false;
	}
	a->array[a->index].flow_properties_before = value;
	a->array[a->index].flow_ismultihoming = value2;
	a->array[a->index].flow_preserveMessageBoundaries = value3;
	a->array[a->index].flow_security_needed = value4;
	a->array[a->index].flow_user_ips = value5;
	return 0;
}



