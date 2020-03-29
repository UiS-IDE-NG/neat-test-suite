#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <uv.h>
#include <jansson.h>

#include "neat.h"

// for json_loads: try to store more than one value?? (last 10 (?) in an array)
int first_flow = 1;	// change to boolean?
json_t *props_before;
char properties_before[1000];

// for send_result_connection_attempt_to_pm and nt_json_send_once_no_reply
// create an array of prop_obj_before??? or something too
json_t *prop_obj_before;
json_t *result_obj_before;
_Bool first = true;
_Bool first2 = true;
_Bool same_value = false;
_Bool same_value_jsondumps = false;
char output_buffer_before[1000];
// for json_object_set
json_t *result_obj_after_set;

struct properies_for_jsonpack *properties_jsonpack;
//properties_jsonpack array_of_jsonpacks[10]; // store ten instances


// for neat_set_property (after json_loads)
_Bool first_f = true;
int what_index_to_replace = 0;
struct properties *properties_set;
//struct properties_to_set array_properties[10]; // tried different things, not really working, but since it is not in use at the moment --> look at it later


// for send_properties_to_pm and nt_json_send_once
// many values are repeated --> can create a struct and create several extern types of them (depending on how many that are needed)
int what_index_to_replace2 = 0;
_Bool first3 = true;
_Bool first4 = true;
_Bool same_value_jsondumps2 = false;
char output_buffer_before2[1000];
//struct send_properties_to_pm_values send_prop_array[10];



// _Bool uses 1 byte and int uses 2-4 bytes --> so to save memory use _Bool for boolean operations

// for json_dumps --> make structs instead of arrays??? --> can't really iterate through it though --> have to use something called macros-X
//json_t *ip_before[100];	// malloc(1000 * sizeof(json_t))
//json_t *ipvalues[100]; 	// don't know how large it should be

