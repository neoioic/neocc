

#ifndef NEOIOIC_CAR_H
#define NEOIOIC_CAR_H

#include "../general.h"

/* Extended Member */

struct em_car {
	float max_load;	    /* 最大负载量，以吨为单位。 */
	float max_velocity; /* 最大速度，以千米/小时为单位。 */
};

#endif /* #ifndef NEOIOIC_CAR_H */
