#include <string>
#include "types.h"
#include "gc_roots.h"
#include <android/log.h>
#include <slicer/dex_ir_builder.h>
#include <slicer/code_ir.h>
#include <slicer/reader.h>
#include <slicer/writer.h>
#include <sstream>
#include <algorithm>
#include <stdio.h>
#include <stddef.h>
// 通用配置 ：日志 头文件
#include "leak_find/common.h"

// jvmti 时间回调处理函数
#include "leak_find/demo.h"

// 根据 jvmtiHeapReferenceInfo 中的索引计算出Field
#include "leak_find/obj_find.h"

// 动态注册java 方法 的头文件。 要使用jvmti 上下文，那么就需要动态注册java方法。
// 动态注册需要两步：
// 1： 在JNI_OnLoad 函数中 调用 env->RegisterNatives 注册
// 2： 在这个回调中 再次注册 callbacks.NativeMethodBind = &My_Native_Method_Bind_Listener; 见My_Native_Method_Bind_Listener函数。
#include "leak_find/java_method_regist.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"
#pragma GCC diagnostic ignored "-Wmissing-declarations"

using namespace dex;
using namespace lir;


typedef struct {
    char *signature;
    int count;
    int space;
} ClassDetails;


/**
 * 当前对象  约= 被引用对象
 * @param reference_kind  当前对象的类型。
 * @param reference_info 引用的详细信息
 * @param class_tag  当前对象所属class的标签值, 0 表示未打标签。（如果当前对象是一个运行时的class，则class_tag的值为Java.lang.Class的标签值（如果java.lang.Class未打标签，则为0））。
 * @param referrer_class_tag  引用当前对象的对象所属class的标签值，如果是0，则未打标签或者引用当前对象的是heap root。（如果引用当前对象的对象是一个运行时的class，则 referrer_class_tag 的值为Java.lang.Class的标签值（如果java.lang.Class未打标签，则为0））
 * @param size   当前对象的大小（单位bytes）
 * @param tag_ptr   当前对象的标签值.  如果标签值为0，说明当前对象未打标签。 在回调函数中，你可以为这个值赋值。这个操作类似调用了 SetTag。
 * @param referrer_tag_ptr 引用当前对象的对象标签值.  0表示未打标签， NULL表示 引用当前对象的对象是来自heap root。
 *                      在此回调函数中，你可以为这个值赋值。这个操作类似调用了 SetTag。 如果回调函数的入参 referrer_tag_ptr == tag_ptr，
 *                      则说明他们是一个对象内的递归引用。比如A内部的成员变量仍旧是A。
 * @param length  如果这个对象是一个数组，则这个入参表示数组的长度。如果不是的话，它的值为-1.
 * @param user_data (调用followReference函数时 )外部传递给回调函数的数据
 * @return
 */
jint my_heap_reference_callback(jvmtiHeapReferenceKind reference_kind,
                                const jvmtiHeapReferenceInfo *reference_info, jlong class_tag,
                                jlong referrer_class_tag, jlong size, jlong *tag_ptr,
                                jlong *referrer_tag_ptr, jint length, void *user_data) {


//    ALOGI("====my_heap_reference_callback:====");

    if (reference_kind == 2) {
        // field
        ALOGI("====引用类型field:====%d-index:%d", reference_kind, reference_info->field.index);
        if (referrer_class_tag == NULL) {

            ALOGI("====referrer_tag_ptr: gc root");
        } else {
            ALOGI("====referrer_tag_ptr: %d", referrer_class_tag);
        }

    } else if (reference_kind == 8) {
        // static field
        ALOGI("====引用类型 static field:====%d-index:%d", reference_kind, reference_info->field.index);
        if (referrer_class_tag == NULL) {
            ALOGI("====referrer_tag_ptr: gc root");
        } else {
            ALOGI("====referrer_tag_ptr: %d", referrer_class_tag);
        }
    }


    return JVMTI_VISIT_OBJECTS;
}

/**
 *
 * @param reference_kind  引用类型
 * @param reference_info  应用详细信息 （ Set集合）
 * @param class_tag
 * @param referrer_class_tag
 * @param size  被引用对象的大小
 * @param tag_ptr
 * @param referrer_tag_ptr
 * @param length  如果此对象是数组，那么length 就是数组长度，否则就是-1.
 * @param user_data
 * @return
 */
jint objectReferenceCallback(jvmtiHeapReferenceKind reference_kind,
                             const jvmtiHeapReferenceInfo *reference_info, jlong class_tag,
                             jlong referrer_class_tag, jlong size, jlong *tag_ptr,
                             jlong *referrer_tag_ptr, jint length, void *user_data) {
    ALOGI("=====引用类型:");

}

// Add a label before instructionAfter


static void
addLabel(CodeIr &c,
         lir::Instruction *instructionAfter,
         Label *returnTrueLabel) {
    c.instructions.InsertBefore(instructionAfter, returnTrueLabel);
}


//  使用dexer 修改dex 文件 部分忽略
// Add a byte code before instructionAfter
static void
addInstr(CodeIr &c,
         lir::Instruction *instructionAfter,
         Opcode opcode,
         const std::list<Operand *> &operands) {
    auto instruction = c.Alloc<Bytecode>();

    instruction->opcode = opcode;

    for (auto it = operands.begin(); it != operands.end(); it++) {
        instruction->operands.push_back(*it);
    }

    c.instructions.InsertBefore(instructionAfter, instruction);
}


static void addCall(ir::Builder &b,
                    CodeIr &c,
                    lir::Instruction *instructionAfter,
                    Opcode opcode,
                    ir::Type *type,
                    const char *methodName,
                    ir::Type *returnType,
                    const std::vector<ir::Type *> &types,
                    const std::list<int> &regs) {
    auto proto = b.GetProto(returnType, b.GetTypeList(types));
    auto method = b.GetMethodDecl(b.GetAsciiString(methodName), proto, type);

    VRegList *param_regs = c.Alloc<VRegList>();
    for (auto it = regs.begin(); it != regs.end(); it++) {
        param_regs->registers.push_back(*it);
    }

    addInstr(c, instructionAfter, opcode, {param_regs, c.Alloc<Method>(method,
                                                                       method->orig_index)});
}

static std::string ClassNameToDescriptor(const char *class_name) {
    std::stringstream ss;
    ss << "L";
    for (auto p = class_name; *p != '\0'; ++p) {
        ss << (*p == '.' ? '/' : *p);
    }
    ss << ";";
    return ss.str();
}


static size_t getNumParams(ir::EncodedMethod *method) {
    if (method->decl->prototype->param_types == NULL) {
        return 0;
    }

    return method->decl->prototype->param_types->types.size();
}

/**
 *
 * @param jvmti_env
 * @param env
 * @param classBeingRedefined
 * @param loader
 * @param name
 * @param protectionDomain
 * @param classDataLen  类字节码数据长度
 * @param classData  类字节码数据
 * @param newClassDataLen
 * @param newClassData
 */
static void My_ClassFile_Load_Hook(jvmtiEnv *jvmti_env,
                                   JNIEnv *env,
                                   jclass classBeingRedefined,
                                   jobject loader,
                                   const char *name,
                                   jobject protectionDomain,
                                   jint classDataLen,
                                   const unsigned char *classData,
                                   jint *newClassDataLen,
                                   unsigned char **newClassData) {

    // ALOGI("==========My_ClassFile_Load_Hook %s=======", name);

    if (!strcmp(name, "android/app/Activity")) {        // 过滤Activity类
        if (loader == nullptr) {
            ALOGI("==========bootclassloader=============");
        }
        ALOGI("==========ClassTransform %s=======", name);

        struct Allocator : public Writer::Allocator {
            virtual void *Allocate(size_t size) { return ::malloc(size); }

            virtual void Free(void *ptr) { ::free(ptr); }
        };

        Reader reader(classData, classDataLen);     // 定义一个reader对象，构造传入数据和数据长度
        reader.CreateClassIr(0);
        std::shared_ptr<ir::DexFile> dex_ir = reader.GetIr();

        ir::Builder b(dex_ir);                      // 定义一个ir::Builder对象b。

        for (auto &method : dex_ir->encoded_methods) {      // 遍历 类的方法

            std::string type = method->decl->parent->Decl();
            ir::String *methodName = method->decl->name;
            std::string prototype = method->decl->prototype->Signature();

            // ALOGI("==========ClassTransform method %s=======", methodName->c_str());

            // 过滤 Activity 的onCreate 方法
            if (!strcmp("onCreate", methodName->c_str()) &&
                !strcmp(prototype.c_str(), "(Landroid/os/Bundle;)V")) {
                ALOGI("==========Method modify %s %s=======", methodName->c_str(),
                      prototype.c_str());

                CodeIr c(method.get(), dex_ir);
                int originalNumRegisters = method->code->registers -
                                           method->code->ins_count;//v=寄存器p+v数量减去入参p,此例子中v=3 p=2
                int numAdditionalRegs = std::max(0, 1 - originalNumRegisters);
                int thisReg = numAdditionalRegs + method->code->registers
                              - method->code->ins_count;
                ALOGI("origin:%d  addreg:%d", originalNumRegisters, numAdditionalRegs);
                if (numAdditionalRegs > 0) {
                    c.ir_method->code->registers += numAdditionalRegs;
                }

                ir::Type *stringT = b.GetType("Ljava/lang/String;");
                ir::Type *activityT = b.GetType("Landroid/app/Activity;");
                ir::Type *jvmtiHelperT = b.GetType("Lcom/dodola/jvmtilib/JVMTIHelper;");
                ir::Type *voidT = b.GetType("V");
                std::stringstream ss;
                ss << method->decl->parent->Decl() << "#" << method->decl->name->c_str() << "(";
                bool first = true;
                if (method->decl->prototype->param_types != NULL) {
                    for (const auto &type : method->decl->prototype->param_types->types) {
                        if (first) {
                            first = false;
                        } else {
                            ss << ",";
                        }

                        ss << type->Decl().c_str();
                    }
                }
                ss << ")";
                std::string methodDescStr = ss.str();
                ir::String *methodDesc = b.GetAsciiString(methodDescStr.c_str());

                //该例子中不影响原寄存器数量
                lir::Instruction *fi = *(c.instructions.begin());
                VReg *v0 = c.Alloc<VReg>(0);
                VReg *v1 = c.Alloc<VReg>(1);
                VReg *v2 = c.Alloc<VReg>(2);
                VReg *thiz = c.Alloc<VReg>(thisReg);

                addInstr(c, fi, OP_MOVE_OBJECT, {v0, thiz});
                addInstr(c, fi, OP_CONST_STRING,
                         {v1, c.Alloc<String>(methodDesc, methodDesc->orig_index)});//对于插入到前面的指令来说
                addCall(b, c, fi, OP_INVOKE_STATIC, jvmtiHelperT, "printEnter", voidT,
                        {activityT, stringT},
                        {0, 1});
                c.Assemble();

                Allocator allocator;
                Writer writer2(dex_ir);
                jbyte *transformed(
                        (jbyte *) writer2.CreateImage(&allocator,
                                                      reinterpret_cast<size_t *>(newClassDataLen)));

                jvmti_env->Allocate(*newClassDataLen, newClassData);
                std::memcpy(*newClassData, transformed, *newClassDataLen);
                break;
            }
        }

    }

}


void SetAllCapabilities(jvmtiEnv *jvmti) {
    jvmtiCapabilities caps;
    jvmtiError error;
    error = jvmti->GetPotentialCapabilities(&caps);
    error = jvmti->AddCapabilities(&caps);
}

jvmtiEnv *CreateJvmtiEnv(JavaVM *vm) {
    jvmtiEnv *jvmti_env;
    jint result = vm->GetEnv((void **) &jvmti_env, JVMTI_VERSION_1_2);
    if (result != JNI_OK) {
        return nullptr;
    }

    return jvmti_env;

}


void SetEventNotification(jvmtiEnv *jvmti, jvmtiEventMode mode,
                          jvmtiEvent event_type) {
    jvmtiError err = jvmti->SetEventNotificationMode(mode, event_type, nullptr);
    if (err != JVMTI_ERROR_NONE) {
        ALOGI("=====Failed to enable event====");
    }
}

//
extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *vm, char *options,
                                               void *reserved) {
    ALOGI("===========Agent_OnLoad===============");
}
extern "C" JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm) {
    ALOGI("===========Agent_OnUnload===============");
}

// 第一步 ：重写 Agent_OnAttach 函数。
extern "C" JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM *vm, char *options,
                                                 void *reserved) {

    // 第二步：初始化 jvmti 环境。也就是初始化jvmtiEnv *。
    jvmtiEnv *jvmti_env = CreateJvmtiEnv(vm);

    localJvmtiEnv = jvmti_env;

    if (jvmti_env == NULL) {
        ALOGI("===========error init jvmti===============");
    }
    // 第三步骤： 设置jvmti 的状态 ： 包括4项
    // 1：能力范围状态： 设置jvmti 的 作用范围
    SetAllCapabilities(jvmti_env);


    // 2：事件回调状态：当jvm执行某一个操作时，回掉我们定义的函数。例如垃圾开始回收时。
    jvmtiEventCallbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    // 情况1：vm加载class文件之前 回调
    // 情况2：调用了 RetransformClasses函数或RedefineClasses 修改类class 文件时，回调 Ok
    callbacks.ClassFileLoadHook = &My_ClassFile_Load_Hook; // 类文件加载监听 OK
    callbacks.NativeMethodBind = &My_Native_Method_Bind_Listener;  // 第一次动态注册native方法 或者第一次调用native方法 OK

    callbacks.VMObjectAlloc = &ObjectAllocCallback; // 对象分配内存时回调 ObjectAllocCallback 函数  OK

    callbacks.GarbageCollectionStart = &GCStartCallback;   // 开始垃圾 回收 监听 OK
    callbacks.ObjectFree = &My_Object_Free_Listener;  // Events are only sent for tagged objects  OK
    callbacks.GarbageCollectionFinish = &GCFinishCallback;  // 垃圾回收结束 监听 OK


    callbacks.ThreadStart = &My_Thread_Start_Listener;   // 线程开始 OK
    callbacks.ThreadEnd = &My_Thread_End_Listener;    // 线程结束 OK


    callbacks.MethodEntry = &My_Method_Entry_Listener;  // 方法进入 监听  OK
    callbacks.MethodExit = &My_Method_Exit_Listener; // 方法退出 监听     OK
    callbacks.Exception = &My_Exception_Listener;   // java 代码发生异常 监听 OK
    callbacks.ExceptionCatch = &My_Expcetion_Catch;  // java 代码发生异常时catch 住的 监听 OK
    callbacks.Breakpoint = &My_break_point_listener; // 设置断点 OK （记得清除断点）
    callbacks.SingleStep = &my_single_stemp_listener;  // OK-
    callbacks.FieldAccess = &My_Field_Access_Listener;  // 成员属性访问监听   // OK  需要动态注册方法，并绑定到jvmti
    callbacks.FieldModification = &My_Field_Modificatin_Listener;// 成员属性修改监听  // OK 需要动态注册方法，并绑定到jvmti
    callbacks.FramePop = &My_FramePop_Listener;         // ok  需要调用   localJvmtiEnv->NotifyFramePop(thread, 0);


    int error = jvmti_env->SetEventCallbacks(&callbacks, (jint) sizeof(callbacks));

    // 3：事件通知是否可用状态：
//    SetEventNotification(jvmti_env, JVMTI_ENABLE,
//                         JVMTI_EVENT_OBJECT_FREE);  // 开启 回收对象监听 OK
//    SetEventNotification(jvmti_env, JVMTI_ENABLE,
//                         JVMTI_EVENT_VM_OBJECT_ALLOC); // 开启 jvm 中给对象分配内存 的监听 OK
    SetEventNotification(jvmti_env, JVMTI_ENABLE,
                         JVMTI_EVENT_NATIVE_METHOD_BIND); // 第一次绑定或者调用 native方法  OK

//    SetEventNotification(jvmti_env, JVMTI_ENABLE,
//                         JVMTI_EVENT_GARBAGE_COLLECTION_START); // 垃圾回收开始 OK
//    SetEventNotification(jvmti_env, JVMTI_ENABLE,
//                         JVMTI_EVENT_GARBAGE_COLLECTION_FINISH); // 垃圾回收结束 OK

//
    SetEventNotification(jvmti_env, JVMTI_ENABLE,
                         JVMTI_EVENT_CLASS_FILE_LOAD_HOOK); // java字节码文件 加载时。OK

//    SetEventNotification(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_EXCEPTION);  // 开启 java 代码发生异常 的监听  OK
//    SetEventNotification(jvmti_env, JVMTI_ENABLE,
//                         JVMTI_EVENT_EXCEPTION_CATCH);// 开启 java 代码发生异常时catch住 的监听  OK
//
//    SetEventNotification(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_METHOD_ENTRY); // 方法进入  OK
//    SetEventNotification(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_METHOD_EXIT); // 方法退出  OK
//    SetEventNotification(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_BREAKPOINT);  // SetBreakPoints OK
//
//    SetEventNotification(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_FIELD_ACCESS);   // 字段 访问监听 OK 需要动态绑定
//    SetEventNotification(jvmti_env, JVMTI_ENABLE,
//                         JVMTI_EVENT_FIELD_MODIFICATION); // 字段 修改监听 OK 需要动态绑定
//
//    SetEventNotification(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_THREAD_START);  // 线程开始运行监听 OK
//    SetEventNotification(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_THREAD_END);   // 线程结束运行监听 OK
//
//
//    SetEventNotification(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_FRAME_POP);  // 监听方法退出
//    SetEventNotification(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_SINGLE_STEP);// OK-


    // 获取 stacktrace
    //https://github.com/odnoklassniki/jvmti-tools/blob/master/heapsampler/heapsampler.cpp
    ALOGI("==========Agent_OnAttach=======");

    // 参考 当前时间计算的代码
    // https://github.com/odnoklassniki/jvmti-tools/blob/master/vmtrace/vmtrace.cpp

    // 打印空指针堆栈：
    // https://github.com/murkaje/npe-blame-agent

    //success  SetBreakPoint 事件(callbacks.Breakpoint) demo
    // https://github.com/lofei117/PlayJVMTI/tree/master/agent

    // 修改dex文件：需要学习 操作dex文件的库 dexer .
    //https://github.com/johnlee175/PCallDemo/blob/master/pcall/src/main/cpp/pcall.cpp

    // 获取 stacktrace
    // https://github.com/nezihyigitbasi/jvmti-tools/blob/master/src/print_stack.c

    // 引用
    // https://github.com/JetBrains/debugger-memory-agent/blob/b23838cce7cb966bbf32e49c6c08260b8c73c24e/src/gc_roots.cpp


    gdata = new GlobalAgentData();
    gdata->jvmti = jvmti_env;
    ALOGI("==========initializing done=======");
    return JNI_OK;

}



///*********动态注册 占位方法*************//////////////////
extern "C" JNIEXPORT void JNICALL tempRetransformClasses(JNIEnv *env,
                                                         jclass clazz,
                                                         jobjectArray classes) {
}
extern "C" JNIEXPORT jint JNICALL tempCountInstances(JNIEnv *env,
                                                     jclass clazz,
                                                     jclass classes) {
}
extern "C" JNIEXPORT void JNICALL tempSetFieldAccessWatch(JNIEnv *env,
                                                          jclass clazz,
                                                          jstring classes) {
}
extern "C" JNIEXPORT void JNICALL tempSetFieldModifyWatch(JNIEnv *env,
                                                          jclass clazz,
                                                          jstring classes) {
}
extern "C" JNIEXPORT void JNICALL temptestSignalHandler(JNIEnv *env,
                                                        jclass clazz) {
    ALOGI("==========test SignalHandler=======");

}
extern "C" JNIEXPORT void JNICALL temp_crawl(JNIEnv *env,
                                             jclass clazz, jobject obj) {
    ALOGI("==========temp_crawl=======");
}
extern "C" JNIEXPORT jobjectArray JNICALL temp_getRoots(JNIEnv *env,
                                                        jclass clazz, jobject obj) {
    ALOGI("==========temp_getRoots=======");
    return NULL;
}
extern "C" JNIEXPORT void JNICALL temp_setTag(JNIEnv *env,
                                              jclass clazz, jobject obj) {
    ALOGI("==========temp_setTag=======");
}
extern "C" JNIEXPORT jint JNICALL temp_getObjHashcode(JNIEnv *env,
                                                      jclass clazz, jobject obj) {
    ALOGI("==========temp_setTag=======");
}
extern "C" JNIEXPORT void JNICALL tempSetBreakPoints(JNIEnv *env,
                                                     jclass clazz) {
    ALOGI("==========tempSetBreakPoints=======");
}

void docatch(int signal, siginfo *info, void *reserved) {
    ALOGI("==========native 异常捕获了=======");

}

// https://github.com/FreeFlash/NativeCrashCatch
int handledSignals[] = {
        SIGSEGV//11 C 无效的内存引用
        , SIGABRT//6 C 由abort(3)发出的退出指令
        , SIGFPE//8 C 浮点异常
        , SIGILL//4 C 非法指令
        , SIGBUS//10,7,10 C 总线错误(错误的内存访问)
        , SIGKILL //9 AEF Kill信号
        , SIGSTOP //17,19,23 DEF 终止进程
        , SIGTERM //15 A 终止信号
        , SIGTRAP //5 C 跟踪/断点捕获
};
const int handledSignalsNum = sizeof(handledSignals) / sizeof(handledSignals[0]);

//原信号handler
struct sigaction mold_handlers[handledSignalsNum];
extern "C" JNIEXPORT void JNICALL tempregSignalHandler(JNIEnv *env,
                                                       jclass clazz) {
    ALOGI("==========注册信号处理函数...=======");
    struct sigaction handler;
    memset(&handler, 0, sizeof(handler));
    handler.sa_sigaction = docatch;
    handler.sa_flags = SA_NOMASK;
    for (int i = 0; i < handledSignalsNum; ++i) {
        sigaction(handledSignals[i], &handler, &mold_handlers[i]);
    }
    ALOGI("==========注册信号处理函数... ok=======");

}


// Java 方法名  ，java方法名的签名 ，对应的native方法名。
static JNINativeMethod methods[] = {

        {"retransformClasses",  "([Ljava/lang/Class;)V",                  reinterpret_cast<void *>(tempRetransformClasses)},
        {"countInstances",      "(Ljava/lang/Class;)I",                   reinterpret_cast<void *>(tempCountInstances)},
        {"setFieldAccessWatch", "(Ljava/lang/Class;Ljava/lang/String;)V", reinterpret_cast<void *>(tempSetFieldAccessWatch)},
        {"setFieldModifyWatch", "(Ljava/lang/Class;Ljava/lang/String;)V", reinterpret_cast<void *>(tempSetFieldModifyWatch)},
        {"regSignalHandler",    "()V",                                    reinterpret_cast<void *>(tempregSignalHandler)},
        {"testSignalHandler",   "()V",                                    reinterpret_cast<void *>(temptestSignalHandler)},
        {"_crawl",              "(Ljava/lang/Object;)V",                  reinterpret_cast<void *>(temp_crawl)},
        {"_getRoots",           "(Ljava/lang/Object;)Ljava/lang/Object;", reinterpret_cast<void *>(temp_getRoots)},
        {"setTag",              "(Ljava/lang/Object;)V",                  reinterpret_cast<void *>(temp_setTag)},
        {"getObjHashcode",      "(Ljava/lang/Object;)I",                  reinterpret_cast<void *>(temp_getObjHashcode)},
        {"setBreakPoints",      "()V",                                    reinterpret_cast<void *>(tempSetBreakPoints)}
};


JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    ALOGI("==============library load====================");
    jclass clazz = env->FindClass("com/dodola/jvmtilib/JVMTIHelper");
    env->RegisterNatives(clazz, methods, sizeof(methods) / sizeof(JNINativeMethod));

    return JNI_VERSION_1_6;
}


extern "C"
JNIEXPORT void JNICALL
Java_com_dodola_jvmtilib_JVMTIHelper_countInstances(JNIEnv *env, jobject instance,
                                                    jclass clazz) {
    ALOGI("====1====");
    JavaVM *vm;
    jint err = env->GetJavaVM(&vm);
    jvmtiEnv *localti;
    jint result = vm->GetEnv((void **) &localti, JVMTI_VERSION_1_2);
    if (localti == nullptr) {
        ALOGI("====localti==null====");
        return;
    }
    int count = 0;
    jvmtiHeapCallbacks callbacks;
    ALOGI("====2====");
    (void) memset(&callbacks, 0, sizeof(callbacks));
    ALOGI("====3====");
    callbacks.heap_iteration_callback = &objectCountingCallbac;
    ALOGI("====3.1====");
    ClassDetails *details;
    jint loadedCount = 0;
    jclass *loadedClasses;
    localti->GetLoadedClasses(&loadedCount, &loadedClasses);

    details = (ClassDetails *) calloc(sizeof(ClassDetails), loadedCount);
    ALOGI("====3.2====%d", loadedCount);
    for (int i = 0; i < loadedCount; i++) {
        char *sig;

        /* Get and save the class signature */
        err = localti->GetClassSignature(loadedClasses[i], &sig, NULL);
        if (sig == NULL) {
            ALOGI("====ERROR: No class signature found====");
        }
        details[i].signature = strdup(sig);

        localti->Deallocate((unsigned char *) sig);

        /* Tag this jclass */
        err = localti->SetTag(loadedClasses[i],
                              (jlong) (ptrdiff_t) (void *) (&details[i]));
    }

    // 解决 myJvmTi 为空的问题 就好了。？
    jvmtiError ret = localti->IterateThroughHeap(JVMTI_HEAP_FILTER_CLASS_UNTAGGED, NULL,
                                                 &callbacks,
                                                 (const void *) &count);
    ALOGI("====4====");
    ALOGI("====%d====", count);

}



