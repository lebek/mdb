#include <mach/mach.h>
#include <mach/mach_traps.h>
#include <mach/mach_types.h>

int mdb_get_port (int pid, mach_port_t *port);

int mdb_get_region (
	mach_port_t task,
	uint64_t *address,
	uint64_t *size,
	vm_region_basic_info_data_64_t *info);

int mdb_read_memory (
	mach_port_t task,
	uint64_t address,
	uint64_t *size,
	unsigned char **buf);

int mdb_get_info (mach_port_t port, task_basic_info_data_t *info);

