#include "common.h"
#include "java_method_impl.h"

#ifndef JVMTI_SAMPLE_MASTER_NATIVE_IMPL_H
#define JVMTI_SAMPLE_MASTER_NATIVE_IMPL_H

#endif //JVMTI_SAMPLE_MASTER_NATIVE_IMPL_H

// 这个文件用来动态注册 java native 方法 到bootclassloader ，这样才能用jvmti_env上下文。

// 将packageCodePath 添加到bootclassloader
void JNICALL
My_Native_Method_Bind_Listener(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread,
                               jmethodID method,
                               void *address, void **new_address_ptr) {
    ALOGI("===========My_Native_Method_Bind_Listener===============");

    jclass clazz = jni_env->FindClass("com/dodola/jvmtilib/JVMTIHelper");
    jmethodID methodid = jni_env->GetStaticMethodID(clazz, "retransformClasses",
                                                    "([Ljava/lang/Class;)V");
    if (methodid == method) {
        *new_address_ptr = reinterpret_cast<void *>(&retransformClasses);
    }

    jmethodID jmethodID2 = jni_env->GetStaticMethodID(clazz, "countInstances",
                                                      "(Ljava/lang/Class;)I");
    if (jmethodID2 == method) {
        *new_address_ptr = reinterpret_cast<void *>(&countInstances);
    }

    jmethodID jmethodID3 = jni_env->GetStaticMethodID(clazz, "setFieldAccessWatch",
                                                      "(Ljava/lang/Class;Ljava/lang/String;)V"); // java 方法setFieldAccessWatch
    if (jmethodID3 == method) {
        *new_address_ptr = reinterpret_cast<void *>(&testSetFieldAccessWatch);
    }
    jmethodID jmethodID4 = jni_env->GetStaticMethodID(clazz, "setFieldModifyWatch",
                                                      "(Ljava/lang/Class;Ljava/lang/String;)V"); // java 方法setFieldModifyWatch
    if (jmethodID4 == method) {
        *new_address_ptr = reinterpret_cast<void *>(&testSetFieldModifWatch);
    }

    jmethodID jmethodID5 = jni_env->GetStaticMethodID(clazz, "setBreakPoints",
                                                      "()V"); // java 方法setBreakPoints
    if (jmethodID5 == method) {
        *new_address_ptr = reinterpret_cast<void *>(&testSetBreakPoint);
    }
    jmethodID jmethodID6 = jni_env->GetStaticMethodID(clazz, "_crawl",
                                                      "(Ljava/lang/Object;)V"); // java 方法_crawl
    if (jmethodID6 == method) {
        *new_address_ptr = reinterpret_cast<void *>(&testcrawl);
    }
    jmethodID jmethodID7 = jni_env->GetStaticMethodID(clazz, "_getRoots",
                                                      "(Ljava/lang/Object;)Ljava/lang/Object;"); // java 方法_getRoots
    if (jmethodID7 == method) {
        *new_address_ptr = reinterpret_cast<void *>(&test_get_Roots);
    }
    jmethodID jmethodID8 = jni_env->GetStaticMethodID(clazz, "setTag",
                                                      "(Ljava/lang/Object;)V"); // java 方法_setTag
    if (jmethodID8 == method) {
        *new_address_ptr = reinterpret_cast<void *>(&test_setTag);
    }

    jmethodID jmethodID9 = jni_env->GetStaticMethodID(clazz, "getObjHashcode",
                                                      "(Ljava/lang/Object;)I"); // java 方法_getObjHashcode
    if (jmethodID9 == method) {
        *new_address_ptr = reinterpret_cast<void *>(&test_getObjHashcode);
    }


//绑定 package code 到BootClassLoader 里
    jfieldID packageCodePathId = jni_env->GetStaticFieldID(clazz, "packageCodePath",
                                                           "Ljava/lang/String;");
    jstring packageCodePath = static_cast<jstring>(jni_env->GetStaticObjectField(clazz,
                                                                                 packageCodePathId));
    const char *pathChar = jni_env->GetStringUTFChars(packageCodePath, JNI_FALSE);
    ALOGI("===========add to boot classloader %s===============", pathChar);
    jvmti_env->AddToBootstrapClassLoaderSearch(pathChar);
    jni_env->ReleaseStringUTFChars(packageCodePath, pathChar);

}

