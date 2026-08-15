/* Minimal stand-in for libutils' Errors.h: libfmq needs the status type and a
   few codes, not the platform's wider error machinery. */
#pragma once
#include <errno.h>
namespace android {
typedef int status_t;
enum {
    OK = 0,
    NO_ERROR = 0,
    UNKNOWN_ERROR = (-2147483647 - 1),
    NO_MEMORY = -ENOMEM,
    INVALID_OPERATION = -ENOSYS,
    BAD_VALUE = -EINVAL,
    TIMED_OUT = -ETIMEDOUT,
};
}  // namespace android
