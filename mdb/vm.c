#include <stdio.h>
#include <stdlib.h>

#include "mdb.h"

int
mdb_get_port (int pid, mach_port_t *port)
{
	return task_for_pid(mach_task_self(), (pid_t) pid, port);
}

int
mdb_get_region (
	mach_port_t port,
	uint64_t *address,
	uint64_t *size,
	vm_region_basic_info_data_64_t *info)
{
	kern_return_t kr;
	mach_port_t object;
	mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;

	kr = vm_region_64(port, (vm_address_t *) address, (vm_size_t *) size,
					  VM_REGION_BASIC_INFO, (vm_region_info_t) info,
					  &info_count, &object);

	return kr;
}

int
mdb_read_memory (
	mach_port_t port,
	uint64_t address,
	uint64_t *size,
	unsigned char **buf)
{
	kern_return_t kr;

	*buf = (unsigned char *) malloc(*size);

	kr = vm_read_overwrite(port, (vm_address_t) address, (vm_size_t) *size,
						   (vm_address_t) *buf, (vm_size_t *) size);

	return kr;
}

int
mdb_get_info (mach_port_t port, task_basic_info_data_t *info)
{
	kern_return_t kr;
	mach_msg_type_number_t info_count = TASK_BASIC_INFO_COUNT;

	kr = task_info(port, TASK_BASIC_INFO_64, (task_info_t) info, &info_count);

	return kr;
}
