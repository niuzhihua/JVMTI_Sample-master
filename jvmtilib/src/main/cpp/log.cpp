#include <string>
#include <iostream>
#include <android/log.h>
#include "log.h"

#define LOG_TAG "mylog->"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
enum LogLevel {
    NO_LOG = 0,
    FATAL = 1,
    ERROR = 2,
    WARN = 3,
    INFO = 4,
    DEBUG = 5
};

static LogLevel LEVEL = ERROR;

void handleOptions(const char *options) {
    auto level = static_cast<int>(LEVEL);
    if (!std::string(options).empty()) {
        try {
            level = std::stoi(options);
        }
        catch (const std::invalid_argument &e) {
            error("Invalid log level argument. Expected value is integer between 0 and 5. \"ERROR\" level will be set");
        }
    }

    if (NO_LOG <= level && level <= DEBUG) {
        LEVEL = static_cast<LogLevel>(level);
    } else {
        warn("Unexpected log level. Expected value between 0 and 5");
    }
}

void fatal(const char *msg) {
    if (LEVEL >= FATAL) {
//        std::cerr << "MEMORY_AGENT::FATAL " << msg << std::endl;
        ALOGI("=fatal=%s", msg);
    }
}

void error(const char *msg) {
    if (LEVEL >= ERROR) {
//        std::cerr << "MEMORY_AGENT::ERROR " << msg << std::endl;
        ALOGI("=error=%s", msg);
    }
}

void warn(const char *msg) {
    if (LEVEL >= WARN) {
        // std::cout << "MEMORY_AGENT::WARN " << msg << std::endl;
        ALOGI("=WARN=%s", msg);
    }
}

void info(const char *msg) {
    if (LEVEL >= INFO) {
        ALOGI("=info=%s", msg);
        // std::cout << "MEMORY_AGENT::INFO " << msg << std::endl;
    }
}

void debug(const char *msg) {
    if (LEVEL >= DEBUG) {
//        std::cout << "MEMORY_AGENT::DEBUG " << msg << std::endl;
        ALOGI("=debug=%s", msg);
    }
}
