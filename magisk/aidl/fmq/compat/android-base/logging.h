/* Stand-in for libbase's logging.h.
 *
 * Only the streaming LOG(SEVERITY) form is used here, and vendoring libbase for
 * it would drag in sources and dependencies far out of proportion to the need.
 * This provides the same shape, writing to the NDK's liblog. LOG(FATAL) aborts
 * as the real one does, which matters: the service uses it when it cannot
 * register, and continuing half-registered would be worse than dying. */
#pragma once
#include <android/log.h>
#include <sstream>
#include <cstdlib>

#ifndef LOG_TAG
#define LOG_TAG "rv4a"
#endif

namespace rv4a_log {

enum Severity { VERBOSE, DEBUG, INFO, WARNING, ERROR, FATAL };

inline int toAndroid(Severity s) {
    switch (s) {
        case VERBOSE: return ANDROID_LOG_VERBOSE;
        case DEBUG:   return ANDROID_LOG_DEBUG;
        case INFO:    return ANDROID_LOG_INFO;
        case WARNING: return ANDROID_LOG_WARN;
        default:      return ANDROID_LOG_ERROR;
    }
}

class Message {
  public:
    explicit Message(Severity s) : mSeverity(s) {}
    ~Message() {
        __android_log_print(toAndroid(mSeverity), LOG_TAG, "%s", mStream.str().c_str());
        if (mSeverity == FATAL) abort();
    }
    template <typename T>
    Message& operator<<(const T& value) {
        mStream << value;
        return *this;
    }

  private:
    Severity mSeverity;
    std::ostringstream mStream;
};

}  // namespace rv4a_log

#define VERBOSE ::rv4a_log::VERBOSE
#define DEBUG   ::rv4a_log::DEBUG
#define INFO    ::rv4a_log::INFO
#define WARNING ::rv4a_log::WARNING
#define ERROR   ::rv4a_log::ERROR
#define FATAL   ::rv4a_log::FATAL

#define LOG(severity) ::rv4a_log::Message(severity)
