#include <stdlib.h>
#include <stdbool.h>
#include <uv.h>
#include <jansson.h>

#include "neat.h" 

extern const size_t INITIAL_SIZE;
extern const size_t MAX_SIZE;	

#define CREATE_ARRAY_STRUCTURE(X, Y, Z) \
	struct X { \
		struct Y *array; \
		size_t used; \
		size_t size; \
		int index; \
		int index_of_same_value; \
		_Bool first_flow; \
		_Bool same_value; \
		_Bool full_array; \
	}; \
	extern struct X *Z;

CREATE_ARRAY_STRUCTURE(jsonloads_struct, properties_jsonloads, jsonloads_prop)
CREATE_ARRAY_STRUCTURE(send_result_conn_struct, properties_send_result_conn, send_result_conn_prop)
CREATE_ARRAY_STRUCTURE(send_prop_to_pm_struct, properties_send_prop_to_pm, send_prop_to_pm_prop)
CREATE_ARRAY_STRUCTURE(jsonpack_two_keys_struct, properties_jsonpack_two_keys, jsonpack_two_keys_prop)

CREATE_ARRAY_STRUCTURE(test_struct, properties_test, test_prop)
CREATE_ARRAY_STRUCTURE(jsondumps_struct, properties_jsondumps, jsondumps_prop)

CREATE_ARRAY_STRUCTURE(jsonpack_char_key_struct, properties_jsonpack_char_key, jsonpack_char_key_prop)

// for json_loads
struct properties_jsonloads {
	char properties_before[1000];
	json_t *props_before;
};

// for send_result_connection_attempt_to_pm and nt_json_send_once_no_reply
struct properties_send_result_conn {
	char interface_before[1000]; 
	char ip_before[1000];	
    unsigned int port_before;
    int transport_before;
    _Bool result_before;
    json_t *prop_obj_before;
    json_t *result_obj_before;	
    char output_buffer_before[1000];
};

// for send_properties_to_pm and nt_json_send_once
struct properties_jsonpack_two_keys {
	json_t *endpoint_before;
	char namebuf_before[1000];
	char ifa_name_before[1000];
};
struct properties_send_prop_to_pm {
	json_t *port_before;
	uint16_t flow_port_before;
	json_t *address_before;
	char flow_name_before[1000];
	char output_buffer_before[1000];
	json_t *json_value;
};
struct properties_jsonpack_char_key {
	char key[1000];
	json_t *address_before;
};
struct properties_constant {
	json_t *req_type_before;
	json_t *properties_before;
	_Bool first_flow;
};
extern struct properties_constant *prop_const;

// for json_dumps functions
struct properties_jsondumps {
	json_t *key;
	char output_buffer_before[1000];
};


json_t *cache_jsonloads(const char *compare_value, struct jsonloads_struct *a);

char *cache_jsondumps(json_t *json, size_t flag, struct jsondumps_struct *a);

json_t *cache_jsonpack_two_values(char *compare_value, char *compare_value2, struct jsonpack_two_keys_struct *a);

json_t *cache_jsonpack_uint_key(uint16_t compare_value, struct send_prop_to_pm_struct *a);
char *cache_jsondumps2(json_t *json, size_t flag, struct send_prop_to_pm_struct *a);

json_t *cache_jsonpack_char_key(const char *compare_value, struct jsonpack_char_key_struct *a);

json_t *cache_jsonpack_simple(struct properties_constant *a);

json_t *cache_jsonpack_four_values(char *compare_ip, unsigned int compare_port, int compare_transport, _Bool compare_result, struct send_result_conn_struct *a);
json_t *cache_jsonpack(char *compare_value, struct send_result_conn_struct *a);
char *cache_jsondumps_dependent(json_t *json, size_t flag, struct send_result_conn_struct *a);
