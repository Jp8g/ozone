#include <unistd.h>

long oz_get_thread_count() {
	return sysconf(_SC_NPROCESSORS_ONLN);
}