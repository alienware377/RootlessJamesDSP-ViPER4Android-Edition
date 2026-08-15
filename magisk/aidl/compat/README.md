# Platform binder headers

`AServiceManager_addService` and `ABinderProcess_joinThreadPool` are **system**
APIs. The NDK ships the app-facing half of libbinder_ndk but not these, since
an ordinary app has no business registering a system service — which is exactly
what a HAL does.

The headers are vendored from AOSP unmodified. The symbols themselves are
present in `libbinder_ndk.so` on device, so only the declarations are missing;
nothing here is reimplemented.

This is the same situation as the AIDL interfaces and libfmq: the code is
platform-side by nature, and the NDK's omissions are about intended audience
rather than availability.
