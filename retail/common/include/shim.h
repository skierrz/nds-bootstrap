#ifndef SHIM_H
#define SHIM_H

#include <stdint.h>
#include "dscm_shim.h"

#define SHIM_RX_PER_POLL 2u

void shim_attach(shim_shared_t* shared);
uint32_t shim_poll(void);

#endif
