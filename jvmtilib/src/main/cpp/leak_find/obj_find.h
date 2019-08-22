#include <jni.h>
#include <string>
#include "../jvmti.h"
#include "../types.h"
#include "common.h"

#ifndef JVMTI_SAMPLE_MASTER_OBJ_FIND_H
#define JVMTI_SAMPLE_MASTER_OBJ_FIND_H
#endif


// 根据 followReferences 函数  中 jvmtiHeapReferenceInfo 入参的索引计算出Field .


/****/
typedef struct Clazz Clazz;
typedef struct Field Field;
typedef struct ClassList ClassList;
typedef struct FieldList FieldList;
typedef struct PointedToList PointedToList;
typedef struct Tag Tag;

typedef struct Clazz {
    char *name;
    jclass clazz;
    Field **fields;
    int nFields;
    bool fieldsCalculated;
    Clazz *super;
    Clazz **intfcs;
    int nIntfc;
    int fieldOffsetFromParent;
    int fieldOffsetFromInterfaces;
    bool enqueued;
};
typedef struct Field {
    Clazz *clazz;
    char *name;
    jfieldID fieldID;
};
typedef struct ClassList {
    Clazz *car;
    struct ClassList *cdr;
};

typedef struct FieldList {
    Field *car;
    struct FieldList *cdr;
};
typedef struct Tag {
    FieldList *directFieldList;
    PointedToList *pointedTo;
    Clazz *clazz;
    bool visited;
};
typedef struct PointedToList {
    Tag *car;
    struct PointedToList *cdr;
};


static Clazz **classCache;
static int sizeOfClassCache;
static Clazz *cc1; // LeakActivity
static Clazz *cc2; // MainActivity

static void check_jvmti_error(jvmtiEnv *jvmti, jvmtiError errnum,
                              const char *str) {
    if (errnum != JVMTI_ERROR_NONE) {
        char *errnum_str;

        errnum_str = NULL;
        (void) jvmti->GetErrorName(errnum, &errnum_str);

//        printf("ERROR: JVMTI: %d(%s): %s\n", errnum,
//               (errnum_str == NULL ? "Unknown" : errnum_str),
//               (str == NULL ? "" : str));
        ALOGI("==>%s", str);
    }
}

static int computeFieldOffsetsFromParent(Clazz *c) {
    if (!c->super) {
        if (c->fieldOffsetFromParent < 0)
            c->fieldOffsetFromParent = 0;
        return c->fieldOffsetFromParent;
    }
    if (c->fieldOffsetFromParent < 0) {
        c->fieldOffsetFromParent = computeFieldOffsetsFromParent(c->super);
        return c->fieldOffsetFromParent + c->nFields;
    }
    return c->fieldOffsetFromParent + c->nFields;
}

static void collectAllInterfaces(Clazz *c, ClassList *lst) {
    int i;
    for (i = 0; i < c->nIntfc; i++) {
        if (c->intfcs[i] && !c->intfcs[i]->enqueued) {
            c->intfcs[i]->enqueued = true;
            lst->cdr = new ClassList();
            lst->cdr->car = c->intfcs[i];
            lst->cdr->cdr = NULL;
            collectAllInterfaces(c->intfcs[i], lst);
        }
    }
    if (c->super)
        collectAllInterfaces(c->super, lst);
}

static void computeFieldOffsetsFromInterfaces(Clazz *c) {
    if (c->fieldOffsetFromInterfaces < 0) {
        //Collect all interfaces
        ClassList lst;
        lst.cdr = NULL;
        collectAllInterfaces(c, &lst);
        ClassList *p;
        p = lst.cdr;
        c->fieldOffsetFromInterfaces = 0;
        while (p && p->car) {
            c->fieldOffsetFromInterfaces += p->car->nFields;
            p->car->enqueued = false;
            p = p->cdr;
        }
    }
}

static void updateClassCache(JNIEnv *env) {
    jvmtiError err;
//    jclass c1 = env->FindClass("com/dodola/jvmti/LeakActivity");
//    jclass c2 = env->FindClass("com/dodola/jvmti/MainActivity");

    if (gdata->jvmti != NULL) {
        ALOGI("====gdata->jvmti=ok===");
    }
    jint nClasses;
    jclass *classes;

//    jclass *classes = (jclass *) malloc(sizeof(jclass) * nClasses);
//    classes[0] = c1;
//    classes[1] = c2;

    err = gdata->jvmti->GetLoadedClasses(&nClasses, &classes);

    check_jvmti_error(gdata->jvmti, err, "Cannot get classes");
    if (classCache != NULL) {
        free(classCache);
    }
    classCache = new Clazz *[nClasses];
    memset(classCache, 0, sizeof(Clazz *) * nClasses);
    sizeOfClassCache = nClasses;

    Clazz *c;
    int i;
    jlong tag;
    jint status;
    Tag *t;
    for (i = 0; i < nClasses; i++) {

        err = gdata->jvmti->GetTag(classes[i], &tag);

        check_jvmti_error(gdata->jvmti, err, "Unable to get class tag");
        if (tag) //We already built this class info object - and the class info object is in the tag of the class object
        {
            classCache[i] = ((Tag *) tag)->clazz;
            continue;
        }
        err = gdata->jvmti->GetClassStatus(classes[i], &status);
        check_jvmti_error(gdata->jvmti, err, "Cannot get class status");
        if ((status & JVMTI_CLASS_STATUS_PREPARED) == 0) {
            classCache[i] = NULL;
            continue;
        }
        c = new Clazz();
        t = new Tag();
        t->visited = false;
        t->clazz = c;
        t->directFieldList = NULL;
        t->pointedTo = NULL;
        c->fieldOffsetFromParent = -1;
        c->fieldOffsetFromInterfaces = -1;
        c->fieldsCalculated = false;
        c->clazz = (jclass) env->NewGlobalRef(classes[i]);
        gdata->jvmti->GetClassSignature(classes[i], &c->name, NULL);

        char *classSign;
        gdata->jvmti->GetClassSignature(classes[i], &classSign, NULL);

        // 保存 LeakActivity 和 MainActivity 类的Class 实例 ：用来测试。
        if (strcmp("Lcom/dodola/jvmti/LeakActivity;", classSign) == 0) {
            cc1 = c;
            ALOGI("=cc1===ok");
        }
        if (strcmp("Lcom/dodola/jvmti/MainActivity;", classSign) == 0) {
            cc2 = c;
            ALOGI("=cc2===ok");
        }


        classCache[i] = c;
        err = gdata->jvmti->SetTag(classes[i], (ptrdiff_t) (void *) t);
        check_jvmti_error(gdata->jvmti, err, "Cannot set class tag");
    }
    jclass *intfcs;
    jfieldID *fields;
    int j;
    jclass super;

    //Now that we've built info on each class, we will make sure that for each one,
    //we also have pointers to its super class, interfaces, etc.
    for (i = 0; i < nClasses; i++) {
        if (classCache[i] && !classCache[i]->fieldsCalculated) {
            c = classCache[i];

            super = env->GetSuperclass(classes[i]);
            if (super) {
                err = gdata->jvmti->GetTag(super, &tag);
                check_jvmti_error(gdata->jvmti, err, "Cannot get super class");
                if (tag) {
                    c->super = ((Tag *) (ptrdiff_t) tag)->clazz;
                }
            }

            //Get the fields
            err = gdata->jvmti->GetClassFields(c->clazz, &(c->nFields),
                                               &fields);
            check_jvmti_error(gdata->jvmti, err, "Cannot get class fields");
            c->fields = new Field *[c->nFields];
            for (j = 0; j < c->nFields; j++) {
                c->fields[j] = new Field();
                c->fields[j]->clazz = c;
                c->fields[j]->fieldID = fields[j];
                err = gdata->jvmti->GetFieldName(c->clazz, fields[j],
                                                 &(c->fields[j]->name), NULL, NULL);
                check_jvmti_error(gdata->jvmti, err, "Can't get field name");

            }
            gdata->jvmti->Deallocate((unsigned char *) (void *) fields);

            //Get the interfaces
            err = gdata->jvmti->GetImplementedInterfaces(classes[i],
                                                         &(c->nIntfc), &intfcs);
            check_jvmti_error(gdata->jvmti, err, "Cannot get interface info");
            c->intfcs = new Clazz *[c->nIntfc];
            for (j = 0; j < c->nIntfc; j++) {
                err = gdata->jvmti->GetTag(intfcs[j], &tag);
                check_jvmti_error(gdata->jvmti, err,
                                  "Cannot get interface info");
                if (tag) {
                    c->intfcs[j] = ((Tag *) tag)->clazz;
                } else
                    c->intfcs[j] = NULL;
            }
            gdata->jvmti->Deallocate((unsigned char *) (void *) intfcs);
            classCache[i]->fieldsCalculated = 1;
        }
    }
}

static Field *getField(Clazz *c, int rawIdx) {
    if (c->fieldOffsetFromInterfaces < 0)
        computeFieldOffsetsFromParent(c);

    if (c->fieldOffsetFromInterfaces < 0)
        computeFieldOffsetsFromInterfaces(c);
    int idx = rawIdx - c->fieldOffsetFromInterfaces;
    if (idx >= c->fieldOffsetFromParent)
        idx = idx - c->fieldOffsetFromParent;
    else
        return getField(c->super, rawIdx);
    if (idx < 0 || idx >= c->nFields)
        return NULL;
    return c->fields[idx];
}

// 更加成员变量索引找到 成员名。
void testGetField(JNIEnv *env) {
    updateClassCache(env);

    if (sizeOfClassCache > 0) {

        // 例如在LeakActivity 中 instance 成员的的索引是239
        Field *f = getField(cc1, 239); // 239 : instance--ok  240:t
//        Field *f2 = getField(cc2, 5);
        if (f != NULL) {
            ALOGI("=====f not null====%s-%s", cc1->name, f->name);
        } else {
            ALOGI("=====f   null====");
        }
    } else {
        ALOGI("=====init classCache faild====");
    }
}
/****/