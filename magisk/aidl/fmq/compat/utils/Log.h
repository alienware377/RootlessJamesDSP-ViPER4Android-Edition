/* Routes libfmq's ALOG macros to the NDK's liblog, which we have where
   libutils is not available. */
#pragma once
#include <android/log.h>
#ifndef LOG_TAG
#define LOG_TAG "rv4a-fmq"
#endif
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define ALOGV(...) ((void)0)
#define ALOGE_IF(c, ...) do { if (c) ALOGE(__VA_ARGS__); } while (0)
#define ALOGW_IF(c, ...) do { if (c) ALOGW(__VA_ARGS__); } while (0)
