#include <jni.h>

#include <android/log.h>
#include <string>
#include "../jvmti.h"

#include <string>
#include <stack>
#include <vector>

#ifndef JVMTI_SAMPLE_MASTER_COMMON_H
#define JVMTI_SAMPLE_MASTER_COMMON_H

#endif //JVMTI_SAMPLE_MASTER_COMMON_H
#define LOG_TAG "jvmti"

#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static jvmtiEnv *localJvmtiEnv;
static GlobalAgentData *gdata;
#pragma once

// 存放公共的头文件，log日志,全局变量