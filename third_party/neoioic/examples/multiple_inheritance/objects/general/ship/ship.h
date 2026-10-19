

#ifndef NEOIOIC_SHIP_H
#define NEOIOIC_SHIP_H

#include "../general.h"

/* Extended Member */

struct em_ship {
	float max_load;	    /* 最大负载量，以吨为单位。 */
	float max_velocity; /* 最大速度，以千米/小时为单位。 */
};

#endif /* #ifndef NEOIOIC_SHIP_H */
