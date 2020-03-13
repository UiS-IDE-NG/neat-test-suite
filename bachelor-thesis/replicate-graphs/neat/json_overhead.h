#include <stdlib.h>
#include <uv.h>
#include <jansson.h>

#include "neat.h"

// for json_loads
extern int first_flow; 
extern json_t *props_before;
extern char properties_before[1000];
extern char props_b[1000];

// for json_dumps
//extern json_t *ip_before[100];
//extern json_t *ipvalues[100]; // don't know how large it should be