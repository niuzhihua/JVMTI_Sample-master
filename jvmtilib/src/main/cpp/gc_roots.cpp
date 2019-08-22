// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <unordered_map>
#include <set>
#include <iostream>
#include <vector>
#include <cstring>
#include <queue>
#include "gc_roots.h"
#include "types.h"
#include "utils.h"
#include "log.h"
#include "jvmti.h"
#include <android/log.h>

using namespace std;
#define LOG_TAG "gc_roots.cpp->"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

class referenceInfo {
public:
    referenceInfo(jlong tag, jvmtiHeapReferenceKind kind) : tag_(tag), kind_(kind) {}

    virtual ~referenceInfo() = default;

    virtual jobject getReferenceInfo(JNIEnv *env, jvmtiEnv *jvmti) {
        return nullptr;
    }

    jlong tag() {
        return tag_;
    }

    jvmtiHeapReferenceKind kind() {
        return kind_;
    }

private:
    jlong tag_;
    jvmtiHeapReferenceKind kind_;
};

class infoWithIndex : public referenceInfo {
public:
    infoWithIndex(jlong tag, jvmtiHeapReferenceKind kind, jint index) : referenceInfo(tag, kind),
                                                                        index_(index) {}

    jobject getReferenceInfo(JNIEnv *env, jvmtiEnv *jvmti) override {
        return toJavaArray(env, index_);
    }

private:
    jint index_;
};

static jlongArray buildStackInfo(JNIEnv *env, jlong threadId, jint depth, jint slot) {
    std::vector<jlong> vector = {threadId, depth, slot};
    return toJavaArray(env, vector);
}

static jobjectArray buildMethodInfo(JNIEnv *env, jvmtiEnv *jvmti, jmethodID id) {
    if (id == nullptr) {
        return nullptr;
    }
    char *name, *signature, *genericSignature;
    jvmtiError err = jvmti->GetMethodName(id, &name, &signature, &genericSignature);
    handleError(jvmti, err, "Could not receive method info");
    if (err != JVMTI_ERROR_NONE)
        return nullptr;
    jobjectArray result = env->NewObjectArray(3, env->FindClass("java/lang/String"), nullptr);
    env->SetObjectArrayElement(result, 0, env->NewStringUTF(name));
    env->SetObjectArrayElement(result, 1, env->NewStringUTF(signature));
    env->SetObjectArrayElement(result, 2, env->NewStringUTF(genericSignature));
    jvmti->Deallocate(reinterpret_cast<unsigned char *>(name));
    jvmti->Deallocate(reinterpret_cast<unsigned char *>(signature));
    jvmti->Deallocate(reinterpret_cast<unsigned char *>(genericSignature));
    return result;
}

class stackInfo : public referenceInfo {
public:
    stackInfo(jlong tag, jvmtiHeapReferenceKind kind, jlong threadId, jint slot, jint depth,
              jmethodID methodId)
            : referenceInfo(tag, kind), threadId_(threadId), slot_(slot), depth_(depth),
              methodId_(methodId) {}

public:
    jobject getReferenceInfo(JNIEnv *env, jvmtiEnv *jvmti) override {
        return wrapWithArray(env,
                             buildStackInfo(env, threadId_, depth_, slot_),
                             buildMethodInfo(env, jvmti, methodId_)
        );
    }

private:
    jlong threadId_;
    jint slot_;
    jint depth_;
    jmethodID methodId_;
};

typedef struct PathNodeTag {
    std::vector<referenceInfo *> backRefs;

    ~PathNodeTag() {
        for (auto info : backRefs) {
            delete info;
        }
    }
} GcTag;

GcTag *createGcTag() {
    auto *tag = new GcTag();
    return tag;
}

GcTag *pointerToGcTag(jlong tagPtr) {
    if (tagPtr == 0) {
        return new GcTag();
    }
    return (GcTag *) (ptrdiff_t) (void *) tagPtr;
}

void cleanTag(jlong tag) {
    delete pointerToGcTag(tag);
}

referenceInfo *
createReferenceInfo(jlong tag, jvmtiHeapReferenceKind kind, const jvmtiHeapReferenceInfo *info) {
    switch (kind) {
        case JVMTI_HEAP_REFERENCE_STATIC_FIELD:
        case JVMTI_HEAP_REFERENCE_FIELD:
            return new infoWithIndex(tag, kind, info->field.index);
        case JVMTI_HEAP_REFERENCE_ARRAY_ELEMENT:
            return new infoWithIndex(tag, kind, info->array.index);
        case JVMTI_HEAP_REFERENCE_CONSTANT_POOL:
            return new infoWithIndex(tag, kind, info->constant_pool.index);
        case JVMTI_HEAP_REFERENCE_STACK_LOCAL: {
            jvmtiHeapReferenceInfoStackLocal const &stackLocal = info->stack_local;
            return new stackInfo(tag, kind, stackLocal.thread_id, stackLocal.slot, stackLocal.depth,
                                 stackLocal.method);
        }
        case JVMTI_HEAP_REFERENCE_JNI_LOCAL: {
            jvmtiHeapReferenceInfoJniLocal const &jniLocal = info->jni_local;
            return new stackInfo(tag, kind, jniLocal.thread_id, -1, jniLocal.depth,
                                 jniLocal.method);
        }
            // no information provided
        case JVMTI_HEAP_REFERENCE_CLASS:
        case JVMTI_HEAP_REFERENCE_CLASS_LOADER:
        case JVMTI_HEAP_REFERENCE_SIGNERS:
        case JVMTI_HEAP_REFERENCE_PROTECTION_DOMAIN:
        case JVMTI_HEAP_REFERENCE_INTERFACE:
        case JVMTI_HEAP_REFERENCE_SUPERCLASS:
        case JVMTI_HEAP_REFERENCE_JNI_GLOBAL:
        case JVMTI_HEAP_REFERENCE_SYSTEM_CLASS:
        case JVMTI_HEAP_REFERENCE_MONITOR:
        case JVMTI_HEAP_REFERENCE_THREAD:
        case JVMTI_HEAP_REFERENCE_OTHER:
            break;
    }

    return new referenceInfo(tag, kind);
}

extern "C"
JNIEXPORT jint cbGcPaths(jvmtiHeapReferenceKind referenceKind,
                         const jvmtiHeapReferenceInfo *referenceInfo, jlong classTag,
                         jlong referrerClassTag, jlong size, jlong *tagPtr,
                         jlong *referrerTagPtr, jint length, void *userData) {

    if (*tagPtr == 0) {        // tagPtr: 当前对象的标签值， 命中表示: 当前对象未打标签。
        *tagPtr = pointerToTag(createGcTag());  // 给当前对象设置tag
    }

    PathNodeTag *tag = pointerToGcTag(*tagPtr);

    // 为null 表示引用当前对象的对象是来自heap root， 及引用当前对象的对象是 heap root
    // referrer_tag_ptr == tag_ptr  则说明他们是一个对象内的递归引用
    if (referrerTagPtr != nullptr) {

        if (*referrerTagPtr == 0) { //referrerTagPtr: 引用当前对象的对象标签值     命中表示：(如果为0)说明未打标签。
            *referrerTagPtr = pointerToTag(createGcTag());      // 给引用当前对象的对象  设置tag
        }

        tag->backRefs.push_back(createReferenceInfo(*referrerTagPtr, referenceKind, referenceInfo));

    } else {
        // gc root found
        tag->backRefs.push_back(createReferenceInfo(-1, referenceKind, referenceInfo));
    }
    return JVMTI_VISIT_OBJECTS;
}

/**
 *
 * @param start  是一个GcTag，表示从这个Tag开始 扫描。
 * @param visited 保存访问过的 tag
 * @param limit   扫描深度
 */
static void walk(jlong start, set<jlong> &visited, jint limit) {
    std::queue<jlong> queue;
    queue.push(start);
    jlong tag;
    while (!queue.empty() && visited.size() <= limit) {
        tag = queue.front();
        queue.pop();
        visited.insert(tag);
        for (referenceInfo *info: pointerToGcTag(tag)->backRefs) {
            jlong parentTag = info->tag();
            if (parentTag != -1 && visited.find(parentTag) == visited.end()) {
                queue.push(parentTag);
            }
        }
    }
}

/**
 *
 * @param env
 * @param jvmti
 * @param tag  对象的tag 值
 * @param tagToIndex  map<tag值，索引> ，索引从0开始，是有序的。
 * @return
 */
static jobjectArray
createLinksInfos(JNIEnv *env, jvmtiEnv *jvmti, jlong tag, unordered_map<jlong, jint> tagToIndex) {

    // 将对象的tag 转化为 GcTag,  如果tag=0,则创建新的GcTag
    GcTag *gcTag = pointerToGcTag(tag);
    std::vector<jint> prevIndices;
    std::vector<jint> refKinds;
    std::vector<jobject> refInfos;

    jint exceedLimitCount = 0;

    for (referenceInfo *info : gcTag->backRefs) { // 遍历GcTag 的成员 (引用链)：vector<referenceInfo *>

        jlong prevTag = info->tag();        // 拿到 链中的tag

        //tagToIndex.find(prevTag) == tagToIndex.end() : 表示没找到
        if (prevTag != -1 && tagToIndex.find(prevTag) == tagToIndex.end()) {
            ++exceedLimitCount;
            continue;
        }

        prevIndices.push_back(prevTag == -1 ? -1 : tagToIndex[prevTag]); // 保存链中的tag 到Vector
        refKinds.push_back(static_cast<jint>(info->kind()));            // 保存链中的 引用类型 到Vector

        // info->getReferenceInfo(env, jvmti) 返回的是 jobject 。
        // （根据类型 可能）最终在子类中 将int 类型的 index 放入数组 jIntArray 中, 也就是int数组。
        // 这里为什么要在存放 为数组呢？
        refInfos.push_back(info->getReferenceInfo(env, jvmti));         // 保存链中的 index  到Vector
    }

    // 创建一个 长度为 4 的数组
    jobjectArray result = env->NewObjectArray(4, env->FindClass("java/lang/Object"), nullptr);
    env->SetObjectArrayElement(result, 0, toJavaArray(env, prevIndices));
    env->SetObjectArrayElement(result, 1, toJavaArray(env, refKinds));
    env->SetObjectArrayElement(result, 2, toJavaArray(env, refInfos));
    env->SetObjectArrayElement(result, 3,
                               toJavaArray(env, exceedLimitCount)); // 看见没，这里也把int 放入数组中了。
    return result;
}

static jobjectArray createResultObject(JNIEnv *env, jvmtiEnv *jvmti, std::vector<jlong> &tags) {
    jclass langObject = env->FindClass("java/lang/Object");
    jobjectArray result = env->NewObjectArray(2, langObject,
                                              nullptr);// 创建一个长度为2的object数组   new Object[2]
    jvmtiError err;
    ALOGI("====tags====%d", tags.size());
    std::vector<std::pair<jobject, jlong>> objectToTag;
    err = cleanHeapAndGetObjectsByTags(jvmti, tags, objectToTag,
                                       cleanTag); // 根据Tags 获取 objects , 保存到vector<pair> 中。
    handleError(jvmti, err, "Could not receive objects by their tags");
    // TODO: Assert objectsCount == tags.size()
    jint objectsCount = static_cast<jint>(objectToTag.size());      // 获取的 vector<pair> objects 的长度

    // 创建一个 数组 resultObjects
    jobjectArray resultObjects = env->NewObjectArray(objectsCount, langObject, nullptr);
    // 创建一个 数组 links
    jobjectArray links = env->NewObjectArray(objectsCount, langObject, nullptr);

    unordered_map<jlong, jint> tagToIndex;  // 创建一个 map
    for (jsize i = 0; i < objectsCount; ++i) {  // 将 vector<pair> 中的数据 放入map 中。

        // objectToTag[i] 就是pair<jobject, jlong> 类型。objectToTag[i].second 就是jlong，jlong是对象的tag值。

        //数组形式插入 数据到 map ,相当于Java 中的put
        tagToIndex[objectToTag[i].second] = i;

        // 相当于： tagToIndex.put( objectToTag[i].second] , i);

    }

    for (jsize i = 0; i < objectsCount; ++i) {
        // 给 resultObjects 数组赋值 ， 值为： objectToTag[i].first ，也就是查找的对象jobject
        env->SetObjectArrayElement(resultObjects, i, objectToTag[i].first);

        // 构建引用链
        auto infos = createLinksInfos(env, jvmti, objectToTag[i].second, tagToIndex);

        // 给 links 数组赋值 , 值为：infos数组 （计算出来的引用链 ，用数组表示)
        env->SetObjectArrayElement(links, i, infos);
    }

    // 将 resultObjects数组 和 links 数组 放入 result数组 中。
    env->SetObjectArrayElement(result, 0, resultObjects);  // 放入0位置
    env->SetObjectArrayElement(result, 1, links);       // 放入1位置, 也就是数组的元素还是数组
    ALOGI("===return result==== ");
    return result;
}

jobjectArray
findGcRoots(JNIEnv *jni, jvmtiEnv *jvmti, jclass thisClass, jobject object, jint limit) {
    jvmtiError err;
    jvmtiHeapCallbacks cb;
    info("Looking for paths to gc roots started");   // 寻找指向gc roots 的路径

    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&cbGcPaths);

    GcTag *tag = createGcTag();                             // 创建一个GcTag 结构体
    err = jvmti->SetTag(object, pointerToTag(tag));         // 给当前对象object  设置Tag
    handleError(jvmti, err, "Could not set tag for target object");
    info("start following through references");

    err = jvmti->FollowReferences(JVMTI_HEAP_OBJECT_EITHER, nullptr, nullptr, &cb,
                                  nullptr);  // 执行扫描堆

    handleError(jvmti, err, "FollowReference call failed");

    info("heap tagged");

    set<jlong> uniqueTags;
    info("start walking through collected tags");

    // pointerToTag(tag) 就是给当前对象 设置的tag,
    walk(pointerToTag(tag), uniqueTags, limit);         // 从当前对象 开始扫描 ,

    info("create resulting java objects");
    unordered_map<jlong, jint> tag_to_index;

    vector<jlong> tags(uniqueTags.begin(), uniqueTags.end());
    jobjectArray result = createResultObject(jni, jvmti, tags);

    info("remove all tags from objects in heap");
    err = removeAllTagsFromHeap(jvmti, cleanTag);
    handleError(jvmti, err, "Count not remove all tags");
    return result;
}