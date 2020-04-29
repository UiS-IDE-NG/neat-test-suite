#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <uv.h>
#include <jansson.h>
#include <string.h>

#include "neat.h"
#include "json_overhead.h"
#include "neat_log.h"

const size_t array_initial_size = 10;

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


struct properties_jsonloads {
	char properties_before[1000];
	json_t *props_before;
};

struct properties_jsondumps {	
	json_t *output_buffer_before;	// value
	_Bool same_value_jsondumps;	// "key": initialize to false, or set it false/true depending of what is true (only true is set atm), or it can be set false on initialization, the function (probably this)
};

struct properties_jsonloads {
	json_t *props_before;
	char properties_before[1000];
};

struct properties_jsonobjectget {
	json_t *key;
	json_t *value;
};

struct properties_jsonobjectset {
	json_t *key;
	json_t *value;
}

// will probably seperate them into several structures instead of this (if not, many values will not be used for each insertion of data) - there will be 5 different structs then
struct properties_jsonpack {
	json_t *key;
	char key2[1000];
	int *key3;
	int *key4;
	char key5[1000];
	_Bool key6;
	uint16_t key7;
	json_t *value;
}

struct properties_for_array {
	int index;
	int index_increase;
	int index_of_same_value;
	_Bool same_value;
	_Bool first_flow;
	//_Bool first_time;
}



struct neat_open_struct *neat_open_prop = &(struct neat_open_struct) {.first_flow = true};
struct jsonloads_struct *jsonloads_prop = &(struct jsonloads_struct) {.first_flow = true};
struct neat_set_struct *neat_set_prop = &(struct neat_set_struct) {.first_flow = true};

// if it is done this way, have to consider if index, first_flow etc should be in its own struct and just make many of them (more memory - most likely)

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

// this is the basic structure of all the functions, can use this if they share the same struct (which they don't atm) --> have to split structs (but the other struct need to be inside another struct to assign it globaly too)
// this other struct can always include index and same_value (okey maybe not this one as there is actually another base for this function for those who use if same instead of the for loop)
// check_if_full and init_array will not really work either though --> can always create an enum and use that as the return value and depending on what it gives that is what will be done
json_t *cache_value(struct array_prop *a) {
	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {
			/* some function or "return true to go back since it will be hard to include all the necessary functions in this function" --> can't do this! this is a for loop okey
			--> maybe create a structure or something... */
		}	
	}
	if (!a->first_flow) {
		// json_decref(a->array[a->index].props_before); can't do this here (don't need to either, just need to do it before the assignment of a new value and therefore do this if statement again)
		check_if_full(a);
	} else {
		init_array(a);
		a->first_flow = false;	// only for the last "function"/ instance that will use it
	}
	// some function 
}

// this will be similar too how I did before with this function checking if first flow and then two other functions that checks if same value and that cache the value (there will be several of this depending on the function)
// meaning that there will be more functions than before
if (the_first_flow) {
	... = malloc(array_initial_size * sizeof(...));
	cache_value(..., );
} else {
	if ((... = check_if_cached(..., )) == NULL) {
		if (if_full(...)) {
			... = realloc(a->array, a->size * sizeof(...));
		}
		cache_value(..., );
	}
}

// not sure if the division will really work that well (the array should be in this struct since this are the values that accounts for the whole array, but it can't if i want to set the functions up as they are)
// other values like index, same, index_of_same_value are more to every array item, so they can be included in the struct of arrays, but then i will have a struct with only one item, a struct
struct array_prop {
	size_t size;
	size_t used;
	_Bool first_flow;
}

_Bool the_first_flow(struct array_prop *a) {
	if (!a->first_flow) {
		return false;
	} else {
		// malloc of array is done outside the function if this function returns true 
		a->used = 0;
		a->size = array_initial_size;
		a->index = 0;
		a->first_flow = false;
		return true;
	}
}

_Bool if_full(struct array_prop *a) {
	a->used++; 
	a->index++;
	if (a->size < a->used) { 
			a->size *= 2; 
			return true;
		} 
	return false; 
}

// jsonloads
json_t *cache_value(const char *compare_value, struct jsonloads_struct *a) {
	json_t *props_temp;
    json_error_t error;

    props_temp = json_loads(compare_value, 0, &error);
    a->array[a->index].props_before = props_temp;
    strcpy(a->array[a->index].properties_before, compare_value);
    return props_temp;
}

json_t *check_if_cached(const char *compare_value, struct jsonloads_struct *a, struct array_prop *p) {
	json_t *props_temp = NULL;
	for (int i = 0; i < p->size; ++i) {
		if (strcmp(compare_value, a->array[a->index].properties_before) == 0) {
            props_temp = a->array[a->index].props_before;
            break;
        } 
	}
	return props_temp;
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

char *cache_jsondumps(json_t *json, size_t flag, struct jsondumps_struct *a) {
	char *output_buffer_temp;

	if (a->first_flow) {
		init_array(a);	// only if first in struct
		a->first_flow = false; // this can be inside init_array (maybe, depends on how the structures are set)
	} else {
		if (same_value_jsondumps) {	// may be inside the struct instead (probably call just call it "same" or "same_value")
			same_value_jsondumps = false;
			return a->array[a->index_of_same_value].output_buffer_before; // index_of_same_value may be inside struct
		} else {
			check_if_full(a);
		}
	}
	output_buffer_temp = json_dumps(json, flag);
	strcpy(a->array[a->index].output_buffer_before, output_buffer_temp);
	free(output_buffer_temp);
	return a->array[a->index].output_buffer_before;
}

json_t *cache_jsonobjectget(json_t *compare_value, struct jsonobjectget_struct *a, const char *key) {
	json_t *value_temp;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {
			if (json_equal(a->array[i].key, compare_value)) { // a->array[i].prop_before == compare_value
				value_temp = a->array[i].value;
				a->same_value = true;				// these are general values, 
				a->index_of_same_value = i;	// if needed more, add them to the structure
				return value_temp;
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
	value_temp = json_object_get(compare_value, key);		// possible to make this a function on its own, butt i'm not sure if i can make it general without them being in the same structure
	a->array[a->index].value = value_temp;				// need to put the values in the struct into the structure in that case, which means the all need to store the same type of values
	a->array[a->index].key = compare_value;
	return value_temp;
}

json_t *cache_jsonobjectset(json_t *compare_value, const char *key, json_t *flow_properties, struct jsonobjectset_struct *a) {
	//json_t * value_temp;

	if (!a->first_flow) {
		if (same) {
			//value_temp = a->array[index_of_same_value].flow_properties_before;	// can just return, no ned to assign too
			return a->array[index_of_same_value].flow_properties_before;
		}
	}
	if (!a->first_flow) {
		// this should also only be done once per struct (actually the function should be changed to account for this - the last instance should increase it - maybe two indexes? where one is increased
		// if they are the same and then have some value/flag (boolean value probably) that checks if the other value should be increased too --> basically one of the indexes is used if first flow and
		// same value and the other if there is no matches)
		check_if_full(a);	
	} else {
		// jsondecref?
		init_array(a);
		a->first_flow = false;
	}
	json_object_set(flow_properties, key, compare_value);
	a->array[a->index].flow_properties_before = flow_properties;
	return flow_properties;	// have an if statement that says if not NULL and then assign flow->properties = return value
}

json_t *cache_jsonobjectset2(json_t *compare_value, const char *key, json_t *flow_properties, struct jsonobjectset_struct *a) {
	//json_t * value_temp;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {
			if (json_equal(a->array[i].key, compare_value)) {
				//value_temp = a->array[i].flow_properties_before;	// can just return, no ned to assign too
				return  a->array[i].flow_properties_before;
			}
		}
	}
	if (!a->first_flow) {
		// this should also only be done once per struct (actually the function should be changed to account for this - the last instance should increase it - maybe two indexes? where one is increased
		// if they are the same and then have some value/flag (boolean value probably) that checks if the other value should be increased too --> basically one of the indexes is used if first flow and
		// same value and the other if there is no matches)
		check_if_full(a);	
	} else {
		// jsondecref?
		init_array(a);
		a->first_flow = false;	// don't need to initialize the array as it has already been done
	}
	json_object_set(flow_properties, key, compare_value);
	a->array[a->index].flow_properties_before = flow_properties;
	return flow_properties;	// have an if statement that says if not NULL and then assign flow->properties = return value
}

// actually, this will only work for two and an extra function need to be made
/*
this is the third instance, thought it was for uint16_t
result_obj = json_pack("{s:[{s:{ss}}],s:b}",
    "match", "interface", "value", he_res->interface, "link", true);
can have variabel arguments in the end of char * , will work then (the last value though is either an int or a boolean - use an if statement or something to check how many args and if most add true to the end else 2 (not a real solution))
*/
json_t *cache_jsonpack(char *compare_value, const char *format, struct jsonpack_struct *a) {		// this function works for these specific three functions that are used here (overall hard to make specific functions for jsonpack)
	json_t *value_temp;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {
			if (strcmp(a->array[i].key, compare_value) == 0 && strcmp(a->array[i].format_before, format) == 0) { 
				value_temp = a->array[i].value;
				//a->same_value = true;				 
				//a->index_of_same_value = i;
				return value_temp;
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
	value_temp = json_pack(format, "value", compare_value, "precedence", 2);	
	strcpy(a->array[a->index].key, compare_value);	
	strcpy(a->array[a->index].format_before, format);
	a->array[a->index].value = value_temp;				
	return value_temp;
}

json_t *cache_jsonpack2(char *compare_value, struct jsonpack_struct *a) {		// this function works for these specific three functions that are used here (overall hard to make specific functions for jsonpack)
	json_t *value_temp;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {
			if (strcmp(a->array[i].key, compare_value) == 0) { 
				value_temp = a->array[i].value;
				//a->same_value = true;				 
				//a->index_of_same_value = i;
				return value_temp;
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
	value_temp = json_pack("{s:[{s:{ss}}],s:b}",
    "match", "interface", "value", compare_value, "link", true);	
	strcpy(a->array[a->index].key, compare_value);	
	a->array[a->index].value = value_temp;				
	return value_temp;
}


// can just store it in a global variabel (even though those are not recommended) or in an array with one value or just have it like this to make it general
json_t *cache_jsonpack_simple(struct jsonpack_struct *a) {	// change struct and to make it general add the values that need to be added (maybe in the form of a struct with all the needed alternatives?)
	json_t *value_temp;

	if (a->first_flow) {
		//check_if_full(a);	// not necessary (will actually not really work considering how each of these functions are set up)
		value_temp = a->array[index].key;
	} else {
		init_array(a);
		a->first_flow = false;
		value_temp = json_pack("{s:s}", "value", "pre-resolve");
		a->array[index].key = value_temp;
	}
	return value_temp;

}

json_t *cache_jsonpack_two_values(char *compare_value, char *compare_value2, struct jsonpack_struct *a) {		// this function works for these specific three functions that are used here (overall hard to make specific functions for jsonpack)
	json_t *value_temp;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {
			if (strcmp(a->array[i].key, compare_value) == 0 && strcmp(a->array[i].key2, compare_value2) == 0) { 
				value_temp = a->array[i].value;
				//a->same_value = true;				 
				//a->index_of_same_value = i;
				return value_temp;
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
	value_temp = json_pack("{ss++si}", "value", compare_value, "@", compare_value2, "precedence", 2);	
	strcpy(a->array[a->index].key, compare_value);	
	strcpy(a->array[a->index].key2, compare_value2);
	a->array[a->index].value = value_temp;				
	return value_temp;
}

json_t *cache_jsonpack_uint16(uint16_t *compare_value, struct jsonpack_struct *a) {		// this function works for these specific three functions that are used here (overall hard to make specific functions for jsonpack)
	json_t *value_temp;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {
			if (a->array[i].key == compare_value) { 
				value_temp = a->array[i].value;
				//a->same_value = true;				 
				//a->index_of_same_value = i;
				return value_temp;
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
	value_temp = json_pack("{sisi}", "value", compare_value, "precedence", 2);	
	a->array[a->index].key = compare_value;
	a->array[a->index].value = value_temp;				
	return value_temp;
}

json_t *cache_jsonpack_four_values(char *compare_ip, unsigned int compare_port, int compare_transport, _Bool compare_result, struct jsonpack_struct *a) {
	json_t *props_obj_temp;

	if (!a->first_flow) {
		for (unsigned i = 0; i < a->size; ++i) {	
			if (a->array[i].transport_before == compare_transport && a->array[i].port_before == compare_port && strcmp(compare_ip, a->array[i].ip_before) == 0 && a->array[i].result_before == compare_result) {	 
				props_obj_temp = a->array[i].props_obj_before;	// don't need to assign, can just return						
				return props_obj_temp;												
			}	
		}
	}
	if (!a->first_flow) {
		json_decref(a->array[a->index].props_obj_before);
		check_if_full(a);
	} else {
		init_array(a);
		a->first_flow = false;	// only for the last "function"/ instance that will use it
	}
	props_obj_temp = json_pack("{s:{s:s},s:{s:s},s:{s:i},s:{s:b,s:i},s:{s:b}}",
        "transport", "value", compare_transport,
        "remote_ip", "value", compare_ip,
        "port", "value", compare_port,
        "__he_candidate_success", "value", compare_result, "score", compare_result?5:-5,
        "__cached", "value", 1);
	a->array[a->index].props_obj_before = props_obj_temp;
	a->array[a->index].transport_before = compare_transport;
	a->array[a->index].port_before = compare_port;
	strcpy(a->array[a->index].ip_before, compare_ip);
	a->array[a->index].result_before = compare_result;
	return props_obj_temp;
}