/* Stand-in for libcutils' ashmem helpers.
 *
 * Implemented with memfd rather than /dev/ashmem: that device node is not
 * reachable from an unprivileged process on modern Android, and libcutils
 * itself moved to memfd for the same reason. Sealing matches what the platform
 * does, so the descriptor the framework maps behaves the same way. */
#pragma once
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/memfd.h>
#include <sys/syscall.h>
#include <stddef.h>

static inline int ashmem_create_region(const char* name, size_t size) {
    int fd = (int)syscall(__NR_memfd_create, name ? name : "rv4a-fmq",
                          MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0) return -1;
    if (ftruncate(fd, (off_t)size) < 0) { close(fd); return -1; }
    fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW);
    return fd;
}
static inline int ashmem_set_prot_region(int, int) { return 0; }
static inline int ashmem_get_size_region(int fd) {
    off_t sz = lseek(fd, 0, SEEK_END);
    return sz < 0 ? -1 : (int)sz;
}
static inline int ashmem_valid(int fd) { return fd >= 0; }
