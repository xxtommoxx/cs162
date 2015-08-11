#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>

int main() {
    struct rlimit lim;
    getrlimit(RLIMIT_STACK, &lim);
    printf("stack size: %lu\n", (unsigned long) lim.rlim_max);
    getrlimit(RLIMIT_NPROC, &lim);
    printf("process limit: %lu\n", (unsigned long) lim.rlim_max);
    getrlimit(RLIMIT_NOFILE, &lim);
    printf("max file descriptors: %lu\n", (unsigned long) lim.rlim_max);
}
