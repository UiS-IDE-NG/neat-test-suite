#include <stdlib.h>
#include <stdio.h>
#include <uv.h>
#include <jansson.h>

#include "neat.h"

// for json_loads
int first_flow = 1; 
json_t *props_before;
char properties_before[1000];

// for json_dumps
//json_t *ip_before[100];	// malloc(1000 * sizeof(json_t))
//json_t *ipvalues[100]; 	// don't know how large it should be