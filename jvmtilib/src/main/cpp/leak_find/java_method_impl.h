#include "common.h"

#ifndef JVMTI_SAMPLE_MASTER_JAVA_METHOD_IMPL_H
#define JVMTI_SAMPLE_MASTER_JAVA_METHOD_IMPL_H

#endif //JVMTI_SAMPLE_MASTER_JAVA_METHOD_IMPL_H

// 这个文件存放这java native 方法的实现

// 调用此函数后触发class load hook 监听。
extern "C" JNIEXPORT void JNICALL retransformClasses(JNIEnv *env,
                                                     jclass clazz,
                                                     jobjectArray classes) {
// 获取 Class[] 数组长度
    jsize numTransformedClasses = env->GetArrayLength(classes);
// 申请内存
    jclass *transformedClasses = (jclass *) malloc(numTransformedClasses * sizeof(jclass));

// 遍历数组
    for (int i = 0; i < numTransformedClasses; i++) {
        transformedClasses[i] = (jclass) env->NewGlobalRef(env->GetObjectArrayElement(classes, i));
    }
    ALOGI("==============jvmti:retransformClasses ===============");

// 通过 RetransformClasses 或 RedefineClasses 函数 修改 class， 调用此函数后触发class load hook 监听。
    jvmtiError error = localJvmtiEnv->RetransformClasses(numTransformedClasses,
                                                         transformedClasses);

// 释放内存
    for (int i = 0; i < numTransformedClasses; i++) {
        env->DeleteGlobalRef(transformedClasses[i]);
    }
    free(transformedClasses);
}


// 堆 迭代回调 函数
jint
objectCountingCallbac(jlong class_tag, jlong size, jlong *tag_ptr, jint length, void *user_data) {
    int *count = (int *) user_data;
    (*count)++;
    return JVMTI_VISIT_OBJECTS;
}


extern "C" JNIEXPORT jint JNICALL countInstances(JNIEnv *env,
                                                 jclass clazz,
                                                 jclass clazz_param) {

/*
    jthread thread;
    localJvmtiEnv->GetCurrentThread(&thread);
    jint depth = 1;
    localJvmtiEnv->NotifyFramePop(thread, depth);// 0<depth<10 就会触发FramePop监听

    ALOGI("=====countInstances====%d", depth);*/

/*    jint fieldsCount = 0;
    jfieldID *jfieldID1 = 0;
    localJvmtiEnv->GetClassFields(clazz_param, &fieldsCount, &jfieldID1);


    ALOGI("=====fieldsCount====%d", fieldsCount);
    if (fieldsCount > 0) {
        for (int i = 0; i < fieldsCount; i++) {
            char *name;
            char *sign;
            char *gen;
            localJvmtiEnv->GetFieldName(clazz_param, jfieldID1[i], &name, &sign, &gen);
            ALOGI("=====name====%s", name);
        }
    }*/


    int count = 0;
    jvmtiHeapCallbacks callbacks;
    (void) memset(&callbacks, 0, sizeof(callbacks));
    callbacks.heap_iteration_callback = &objectCountingCallbac; // 堆 遍历回调函数

//     使用下面两个标记，需要打tag.
//     JVMTI_HEAP_FILTER_UNTAGGED        过滤掉 没有打tag 的对象。也就是只查询打tag的对象，使用时需要打tag.
//     JVMTI_HEAP_FILTER_CLASS_UNTAGGED  过滤掉 没有打tag 的类，也就是只查询打tag的类。使用时需要打tag.
//     下面两个使用时 ，打tag 非必选。
//     JVMTI_HEAP_FILTER_TAGGED          过滤掉 打tag的对象，也就是只查询 没有打tag 的对象。打tag 非必选。
//     JVMTI_HEAP_FILTER_CLASS_TAGGED    过滤掉 打tag的类.也就是只查询 没有打tag 的类的对象。打tag 非必选。

//     查找一个类的对象数量  OK
    jvmtiError ret = localJvmtiEnv->IterateThroughHeap(JVMTI_HEAP_FILTER_CLASS_TAGGED, clazz_param,
                                                       &callbacks,
                                                       (const void *) &count);
    ALOGI("====对象数量:%d====", count);

    return count;

}





extern "C" JNIEXPORT void JNICALL testSetFieldAccessWatch(JNIEnv *env,
                                                          jclass clazz,
                                                          jclass set_clazz,
                                                          jstring set_field) {

    jboolean b = false;
    const char *fieldName = env->GetStringUTFChars(set_field, &b);

    jfieldID jfieldID1 = env->GetStaticFieldID(set_clazz, fieldName, "Ljava/lang/String;");
    localJvmtiEnv->SetFieldAccessWatch(set_clazz, jfieldID1);

//    env->ReleaseStringUTFChars(set_field,fieldName);

    ALOGI("====testSetFieldAccessWatch==ok==");
}
extern "C" JNIEXPORT void JNICALL testSetFieldModifWatch(JNIEnv *env,
                                                         jclass clazz,
                                                         jclass set_clazz,
                                                         jstring set_field) {

    jboolean b = false;
    const char *fieldName = env->GetStringUTFChars(set_field, &b);
    jfieldID jfieldID1 = env->GetStaticFieldID(set_clazz, fieldName, "Ljava/lang/String;");
    localJvmtiEnv->SetFieldModificationWatch(set_clazz, jfieldID1);

//    env->ReleaseStringUTFChars(set_field,fieldName);

    ALOGI("====testSetFieldModif==ok==");
}


extern "C" JNIEXPORT void JNICALL testcrawl(JNIEnv *env,
                                            jclass clazz, jobject obj) {
    ALOGI("====testcrawl...====");
    jint hashcode = 0;
    localJvmtiEnv->GetObjectHashCode(obj, &hashcode);
    ALOGI("====hashcode====%d", hashcode);
}

extern "C" JNIEXPORT jobjectArray JNICALL test_get_Roots(JNIEnv *env,
                                                         jclass clazz, jobject obj) {
    ALOGI("====test_get_Roots...====");
    if (gdata != NULL && gdata->jvmti != NULL) {
        ALOGI("====before findGcRoots====");
        jobjectArray array = findGcRoots(env, gdata->jvmti, clazz, obj, 5000);
        jsize s = env->GetArrayLength(array);
        ALOGI("====after findGcRoots====%d", s);
        if (array != NULL) {
            ALOGI("====ok====");
            return array;
        }
    } else {
        ALOGI("====gdata-jvmti-null====");
    }

    return NULL;
}


extern "C" JNIEXPORT jint JNICALL test_getObjHashcode(JNIEnv *jni_env,
                                                      jclass clazz, jobject obj) {
    jint hashcode = 0;
    localJvmtiEnv->GetObjectHashCode(obj, &hashcode);
    return hashcode;
}


extern "C" JNIEXPORT void JNICALL test_setTag(JNIEnv *jni_env,
// 给对象 obj 设置tag
                                              jclass clazz, jobject obj) {

    jint hashcode;
    localJvmtiEnv->GetObjectHashCode(obj, &hashcode);
    localJvmtiEnv->SetTag(obj, hashcode);

    jclass cls = jni_env->FindClass("java/lang/Object");
    jmethodID mid_getName = jni_env->GetMethodID(cls, "toString", "()Ljava/lang/String;");
    jstring name = static_cast<jstring>(jni_env->CallObjectMethod(obj, mid_getName));
    const char *toString = jni_env->GetStringUTFChars(name, JNI_FALSE);

    ALOGI("===obj:%s set tag %d ok...", toString, hashcode);

}


// 给方法设置一个 breakpoint 监听
extern "C" JNIEXPORT void JNICALL testSetBreakPoint(JNIEnv *env,
                                                    jclass clazz) {


    jclass clz = env->FindClass("com/dodola/jvmti/MainActivity");
    jmethodID jmethodID1 = env->GetMethodID(clz, "testBreakPoint", "()V");

    // 设置 point
    jvmtiCapabilities caps;
    memset(&caps, 0, sizeof(caps));
    jvmtiError error = localJvmtiEnv->GetCapabilities(&caps);
    if (error == JVMTI_ERROR_NONE) {  // No error has occurred
        if (caps.can_generate_breakpoint_events) {
            error = localJvmtiEnv->SetBreakpoint(jmethodID1, NULL);
            if (error == JVMTI_ERROR_NONE) {
                ALOGI("==========method testBreakPoint SetBreakpoint succeed...");
                return;
            }
        }
    }

    ALOGI("==========method testBreakPoint SetBreakpoint failed...");

}


