# Vendored libfmq

`AidlMessageQueue` is how an AIDL effect moves audio, and it lives in
`system/libfmq` — which, like the interface definitions, does not ship in the
NDK.

Rather than reimplement the ring buffer from the grantor descriptors, the real
implementation is vendored. That layout is not something to reproduce from
guesswork: getting it subtly wrong surfaces as audio corruption rather than a
clean failure, which is far harder to diagnose than a build error.

## What's here

- `include/fmq`, `base/fmq` — libfmq headers, unmodified
- `EventFlag.cpp` — the one source file needed
- `compat/` — small stand-ins for four platform headers libfmq expects but
  which the NDK does not provide

## The compat shims

| Header | Why it's replaceable |
|---|---|
| `utils/Errors.h` | libfmq needs the status type and a few codes, nothing more |
| `utils/Log.h` | routes `ALOG*` to the NDK's liblog |
| `utils/SystemClock.h` | used only for relative timeouts, so the monotonic clock suffices |
| `cutils/ashmem.h` | implemented with **memfd** |

`android-base/unique_fd.h` and `macros.h` are vendored as-is from libbase
rather than shimmed, since ownership semantics are worth having exactly right.

### On the ashmem shim

`/dev/ashmem` is not reachable from an unprivileged process on modern Android,
and libcutils itself moved to `memfd` for the same reason, so this follows the
platform rather than working around it. The descriptor is sealed against
resizing, matching what the platform does, so what the framework maps behaves
identically.
