#include "port_types.h"

#include <errno.h>
#include <stdint.h>

int hubble_timer_init(struct hubble_timer *timer,
		      void (*cb)(struct hubble_timer *timer, void *user_data),
		      const void *user_data)
{
	return -ENOSYS;
}

int hubble_timer_start(struct hubble_timer *timer, uint64_t expiration_ms)
{
	return -ENOSYS;
}

int hubble_timer_stop(struct hubble_timer *timer)
{
	return -ENOSYS;
}
