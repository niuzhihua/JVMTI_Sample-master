#include "common.h"
#include <android/log.h>

#ifndef JVMTI_SAMPLE_MASTER_DEMO_H
#define JVMTI_SAMPLE_MASTER_DEMO_H

#endif //JVMTI_SAMPLE_MASTER_DEMO_H

// 这个文件存放  jvmti监听事件的  处理函数


void ObjectAllocCallback(jvmtiEnv *jvmti, JNIEnv *jni,
                         jthread thread, jobject object,
                         jclass klass, jlong size) {

    /**
    jclass cls = jni->FindClass("java/lang/Class");
    jmethodID mid_getName = jni->GetMethodID(cls, "getName", "()Ljava/lang/String;");

    jstring name = static_cast<jstring>(jni->CallObjectMethod(klass, mid_getName));
    const char *className = jni->GetStringUTFChars(name, JNI_FALSE);


    // 字符比较相等 : 找到activity 的实例对象分配
//    if (strcmp(className, "com.dodola.jvmti.Main2Activity") == 0) {
    if (strstr(className, "Activity") != NULL && strstr(className, "com.dodola.jvmti") != NULL) {
        // 保存 Activity 对象 的hashcode

        // 记录 activity 的tag ,hashcode
        jint hashcode;
        jvmti->GetObjectHashCode(object, &hashcode);
        // activity对象内存分配时  设置标记
        jvmti->SetTag(object, hashcode);
        ALOGI("==========alloc callback=======%ld", time);
        ALOGI("==========alloc callback======= %s {size:%ld}", className, size);
    }

    jni->ReleaseStringUTFChars(name, className);

     **/
}


void My_Object_Free_Listener(jvmtiEnv *jvmti_env, jlong tag) {
    ALOGE("==触发 Object Free Listener===被回收对象tag:%d", tag);
//    delete reinterpret_cast<const char *>(tag);
/*    ALOGE("==========vector=======%d--tag:%ld", v.size(), tag);
    std::vector<jlong>::iterator result;
    result = find(v.begin(), v.end(), tag);
    if (result != v.end()) {
        ALOGE("========== has=======");
        std::vector<jlong>::iterator del = std::find(v.begin(), v.end(), tag);
        v.erase(del);
        ALOGE("========== has del 剩余：=======%d", v.size());
    }
    {
        ALOGE("==========not has=======%d", v.size());
        jlong leakObjTag = v.front();
        ALOGE("==========泄露对象tag:=======%ld", leakObjTag);

    }*/

//    jint tagCount = 0;
//    jlong tags = 0;
//    jint countOfFreedObj = 0;   // 被回收的对象的个数
//    jobject *objectArray = 0;  // 被回收的对象 放在数组中
//    jlong *tag_result_ptr = 0;
//    jvmtiError error = jvmti_env->GetObjectsWithTags(tagCount, &tags, &countOfFreedObj,
//                                                     &objectArray, &tag_result_ptr);
//    ALOGI("==========触发 Object Free=======%d", countOfFreedObj);

}


void GCStartCallback(jvmtiEnv *jvmti) {

    jlong time = 0;
    jvmtiError error = jvmti->GetTime(&time);
    ALOGI("==========触发 GCStart===%u", time);
}

void GCFinishCallback(jvmtiEnv *jvmti) {

    jlong time = 0;
    jvmtiError error = jvmti->GetTime(&time);
    ALOGI("==========触发 GCFinish===%u", time);

}

void My_Method_Entry_Listener(jvmtiEnv *jvmti_env,
                              JNIEnv *jni_env,
                              jthread thread,
                              jmethodID method) {

    char *methodName;
    char *sign;
    char *generic;
    // 获取方法名，签名
    jvmti_env->GetMethodName(method, &methodName, &sign, &generic);

    // 获取方法所在线程
    jvmtiThreadInfo info;
    jvmti_env->GetThreadInfo(thread, &info);
    if (strcmp(methodName, "testMethodEntryExit") == 0) {

        jlong time;
        jvmti_env->GetTime(&time);
        ALOGI("==========方法进入:%s-%s-%s-%lu", methodName, sign, info.name, time);


    }

//    if (strcmp(methodName, "niuzhihua") == 0) {
//        stk.push(methodName);
//    }

}

// 当调用了localJvmtiEnv->NotifyFramePop 函数时 才触发这个监听。
void My_FramePop_Listener(jvmtiEnv *jvmti_env,
                          JNIEnv *jni_env,
                          jthread thread,
                          jmethodID method,
                          jboolean was_popped_by_exception) {
    if (was_popped_by_exception) {
        ALOGI("=====My_FramePop_Listener=by_exception===");
    } else {
        ALOGI("=====My_FramePop_Listener=return===");
    }
}

// 每当有Java方法执行完就触发此监听
void My_Method_Exit_Listener(jvmtiEnv *jvmti_env,
                             JNIEnv *jni_env,
                             jthread thread,
                             jmethodID method,
                             jboolean was_popped_by_exception,
                             jvalue return_value) {

    char *methodName;
    char *sign;
    char *generic;
    // 获取方法名，签名
    jvmti_env->GetMethodName(method, &methodName, &sign, &generic);
    // 获取方法所在线程
    jvmtiThreadInfo info;
    jvmti_env->GetThreadInfo(thread, &info);


    if (strcmp(methodName, "testMethodEntryExit") == 0) {
        jlong time;
        jvmti_env->GetTime(&time);
        ALOGI("==========方法退出:%s-%s-%s-%lu", methodName, sign, info.name, time);
    }

//    if (strcmp(methodName, "niuzhihua") == 0) {

    jlong time;
    jvmti_env->GetTime(&time);

//        int count = 0;
//        jvmtiHeapCallbacks callbacks;
//        (void) memset(&callbacks, 0, sizeof(callbacks));
//        callbacks.heap_iteration_callback = &objectCountingCallbac; // 堆 遍历回调函数
//        callbacks.heap_reference_callback = &objectReferenceCallback; // 在堆中查找对象引用 时的回调函数

    // 获取 已经被加载的类 和 数量 。
/*        jint loadedCount = 0; // 数量
        jclass *loadedClasses; // 类的签名 （数组）
        jvmti_env->GetLoadedClasses(&loadedCount, &loadedClasses);

        ClassDetails *details;
        details = (ClassDetails *) calloc(sizeof(ClassDetails), loadedCount);
        ALOGI("====加载类数量：====%d", loadedCount);
        for (int i = 0; i < loadedCount; i++) {
            char *sig;

            *//* Get and save the class signature *//*
            jvmtiError err = jvmti_env->GetClassSignature(loadedClasses[i], &sig, NULL);
            if (sig == NULL) {
                ALOGI("====ERROR: No class signature found====");
            }
            details[i].signature = strdup(sig);
            if (i < 10) {
                ALOGI("====类signature：====%d", details[i].signature);
            }
            jvmti_env->Deallocate((unsigned char *) sig);

            // Tag this jclass
            jvmtiError err2 = jvmti_env->SetTag(loadedClasses[i],
                                                (jlong) (ptrdiff_t) (void *) (&details[i]));
        }*/
//        jclass clazz;
//        jvmti_env->GetMethodDeclaringClass(method, &clazz);
//        jclass clazz2 = jni_env->FindClass("android/graphics/Bitmap");
//        jclass clazz3 = jni_env->FindClass("android/view/View");
//        jclass clazz4 = jni_env->FindClass("android/widget/Button");
    // 使用下面两个标记，需要打tag.
    // JVMTI_HEAP_FILTER_UNTAGGED       // 过滤掉 没有打tag 的对象。也就是只查询打tag的对象，使用时需要打tag.
    // JVMTI_HEAP_FILTER_CLASS_UNTAGGED // 过滤掉 没有打tag 的类，也就是只查询打tag的类。使用时需要打tag.
    // 下面两个使用时 ，打tag 非必选。
    // JVMTI_HEAP_FILTER_TAGGED         // 过滤掉 打tag的对象，也就是只查询 没有打tag 的对象。打tag 非必选。
    // JVMTI_HEAP_FILTER_CLASS_TAGGED   // 过滤掉 打tag的类.也就是只查询 没有打tag 的类的对象。打tag 非必选。

    // 查找一个类的对象数量
//        jvmtiError ret = jvmti_env->IterateThroughHeap(JVMTI_HEAP_FILTER_CLASS_TAGGED, clazz,
//                                                       &callbacks,
//                                                       (const void *) &count);

//        char *classSign;
//        char *classGen;
//        jvmti_env->GetClassSignature(clazz, &classSign, &classGen);
    //if initial_object is not specified, all objects reachable from the heap roots
    // 如果 initial_object没有指定，
//        jvmti_env->FollowReferences(JVMTI_HEAP_FILTER_CLASS_TAGGED,clazz,NULL,&callbacks,NULL);

//        ALOGI("====对象数量:%d====", count);
//    ALOGI("======方法退出:%s-%s-threadname:%s-%lu", methodName, sign, info.name, time);

//    }
}


void My_Exception_Listener(jvmtiEnv *jvmti_env,
                           JNIEnv *jni_env,
                           jthread thread,
                           jmethodID method,
                           jlocation location,
                           jobject exceptionObj,
                           jmethodID catch_method,
                           jlocation catch_location) {



    /***************************/
    // 打印堆栈信息
//    jvmtiError errorCode;
//    jint maxFrameCount = 60;
//    jvmtiFrameInfo *frames = (jvmtiFrameInfo *) malloc(sizeof(jvmtiFrameInfo) * maxFrameCount);
//    jint count = 0;
//    char *methodName = NULL;
//    char *declaringClassName = NULL;
//    jclass declaringClass;
////    jthread thread;
////    localJvmtiEnv->GetCurrentThread(&thread);
//    errorCode = jvmti_env->GetStackTrace(thread, 0, maxFrameCount, frames, &count);
//    if (errorCode != JVMTI_ERROR_NONE) {
//        jvmtiThreadInfo threadInfo;
//        jvmti_env->GetThreadInfo(thread, &threadInfo);
//        ALOGI("====Error getting the stack trace of thread [%s]====", threadInfo.name);
//    }
//
//    for (int i = 0; i < count; i++) {
//        errorCode = jvmti_env->GetMethodName(frames[i].method, &methodName,
//                                             NULL, NULL);
//        if (errorCode == JVMTI_ERROR_NONE) {
//            errorCode = jvmti_env->GetMethodDeclaringClass(frames[i].method,
//                                                           &declaringClass);
//            errorCode = jvmti_env->GetClassSignature(declaringClass,
//                                                     &declaringClassName, NULL);
//            if (errorCode == JVMTI_ERROR_NONE) {
//
//                ALOGI("%s::%s()--%d-%d-%d", declaringClassName, methodName, frames[i].location,
//                      location, catch_location);
//            }
//        }
//    }
//
//    if (methodName != NULL) {
//        jvmti_env->Deallocate((unsigned char *) methodName);
//    }
//
//    if (declaringClassName != NULL) {
//        jvmti_env->Deallocate((unsigned char *) declaringClassName);
//    }
//
//    free(frames);
    /**********************************/

    // 检测空指针异常 demo
//    jclass cls = jni_env->FindClass("java/lang/Exception");
//    jmethodID mid_getName = jni_env->GetMethodID(cls, "toString", "()Ljava/lang/String;");
//    jstring name = static_cast<jstring>(jni_env->CallObjectMethod(exceptionObj, mid_getName));
//    const char *className = jni_env->GetStringUTFChars(name, JNI_FALSE);
//    ALOGI("===My_Exception_Listener===%s-location:%d", className, location);
//    jni_env->ReleaseStringUTFChars(name, className);
    // 方式二
    // 获得方法对应的类
    jclass clazz;
    jvmti_env->GetMethodDeclaringClass(method, &clazz);
    // 获得类的签名
    char *class_signature;
    jvmti_env->GetClassSignature(clazz, &class_signature, nullptr);
    if (strcmp(class_signature, "Lcom/dodola/jvmti/MainActivity;") == 0) {
        jclass throwable_class = jni_env->FindClass("java/lang/Throwable");
        jmethodID print_method = jni_env->GetMethodID(throwable_class, "printStackTrace", "()V");
        jni_env->CallVoidMethod(exceptionObj, print_method);


    }
}

// breakpoints 监听被触发时 执行此函数
void My_break_point_listener(jvmtiEnv *jvmti_env,
                             JNIEnv *jni_env,
                             jthread thread,
                             jmethodID method,
                             jlocation location) {
    ALOGI("===break_point_listener 触发了===%d", location);
    jvmti_env->ClearBreakpoint(method, location);
    ALOGI("===Breakpoint 被移除了===");
}

void my_single_stemp_listener(jvmtiEnv *jvmti_env,
                              JNIEnv *jni_env,
                              jthread thread,
                              jmethodID method,
                              jlocation location) {
//    ALOGI("=======my_single_stemp_listener======");
}

void My_Expcetion_Catch(jvmtiEnv *jvmti_env,
                        JNIEnv *jni_env,
                        jthread thread,
                        jmethodID methodId,
                        jlocation location,
                        jobject exceptionObj) {


    jclass cls = jni_env->FindClass("java/lang/Object");
    jmethodID mid_getName = jni_env->GetMethodID(cls, "getClass", "()Ljava/lang/Class;");
    jobject classObj = jni_env->CallObjectMethod(exceptionObj, mid_getName);  // 获得Class 对象

    jclass cls2 = jni_env->FindClass("java/lang/Class");
    jmethodID mid_getName2 = jni_env->GetMethodID(cls2, "getName", "()Ljava/lang/String;"); //

    jstring name = static_cast<jstring>(jni_env->CallObjectMethod(classObj, mid_getName2));
    //以上是 xx.getClass().getName()
    const char *className = jni_env->GetStringUTFChars(name, JNI_FALSE);

    ALOGI("==My_Expcetion_Catch====%s-location:%d", className, location);
    jni_env->ReleaseStringUTFChars(name, className);
}

void My_Field_Access_Listener(jvmtiEnv *jvmti_env,
                              JNIEnv *jni_env,
                              jthread thread,
                              jmethodID method,
                              jlocation location,
                              jclass field_klass,
                              jobject object,
                              jfieldID field) {
    ALOGI("====Field_Access_Listener====");

    char *methodName;
    char *methodSign;
    char *methodGen;

    jvmtiThreadInfo threadInfo;

    jvmti_env->GetThreadInfo(thread, &threadInfo);
    jvmti_env->GetMethodName(method, &methodName, &methodSign, &methodGen);
    ALOGI("==访问字段的线程:%s", threadInfo.name);
    ALOGI("==访问字段的方法:%s-%s", methodName, methodSign);
    if (object == NULL) {
        ALOGI("==访问字段的对象:静态");
    } else {
        ALOGI("==访问字段的对象:对象");
    }

    char *fieldName;
    char *fieldSign;
    char *fieldGen;
    jvmti_env->GetFieldName(field_klass, field, &fieldName, &fieldSign, &fieldGen);
    ALOGI("==被访问字段:%s-%s", fieldName, fieldSign);

    char *classSign;
    char *classGen;
    jvmti_env->GetClassSignature(field_klass, &classSign, &classGen);
    ALOGI("==被访问字段所在的类:%s-%s", fieldName, classSign);

}

void My_Field_Modificatin_Listener(jvmtiEnv *jvmti_env,
                                   JNIEnv *jni_env,
                                   jthread thread,
                                   jmethodID method,
                                   jlocation location,
                                   jclass field_klass,
                                   jobject object,
                                   jfieldID field,
                                   char signature_type,
                                   jvalue new_value) {
    ALOGI("==My_Field_Modificatin_Listener====");
    ALOGI("==修改字段新值为：%c====", signature_type);

    if (object == NULL) {

        ALOGI("==访问字段的对象:静态");
        if (signature_type == 'L') {
            ALOGI("===引用类型===");
            jobject jobject1 = jni_env->GetStaticObjectField(field_klass, field);
            jclass cls = jni_env->FindClass("java/lang/Object");
            const char *name = "toString";
            const char *sign = "()Ljava/lang/String;";
            jmethodID methodId = jni_env->GetMethodID(cls, name, sign);
            jstring value = (jstring) jni_env->CallObjectMethod(jobject1, methodId);
            jboolean b = false;
            const char *value2 = jni_env->GetStringUTFChars(value, &b);
            ALOGI("===原来的值:%s", value2);

            jstring newValue = (jstring) jni_env->CallObjectMethod(new_value.l, methodId);
            const char *newValueStr = jni_env->GetStringUTFChars(newValue, &b);
            ALOGI("===修改后的值:%s", newValueStr);

        } else {
            ALOGI("===基本类型===");
        }
    } else {
        ALOGI("==访问字段的对象:对象");
        if (signature_type == 'L') {
            ALOGI("===引用类型===");
        } else {
            ALOGI("===基本类型===");
        }
    }

}

void My_Thread_Start_Listener(jvmtiEnv *jvmti_env,
                              JNIEnv *jni_env,
                              jthread thread) {
    jvmtiThreadInfo info;
    jvmti_env->GetThreadInfo(thread, &info);

    ALOGI("==Thread_Start_Listener==线程名:%s", info.name);
}

void My_Thread_End_Listener(jvmtiEnv *jvmti_env,
                            JNIEnv *jni_env,
                            jthread thread) {

    jvmtiThreadInfo threadInfo;
    jvmti_env->GetThreadInfo(thread, &threadInfo);
    char *threadName = threadInfo.name;
    ALOGI("==My_Thread_End_Listener==线程名:%s", threadName);
}