package com.dodola.jvmtilib;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.os.Debug;
import android.util.Log;
import android.widget.Toast;

import java.io.File;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;

/**
 * Created by dodola on 2018/12/16.
 */
public class JVMTIHelper {
    private static String packageCodePath = "";

    public static void init(Context context) {
        try {

//            9.0	28	Pie (Android P)
//            8.1	27	Oreo(Android O)（奥利奥）
//            8.0	26	Oreo(Android O)（奥利奥）
            // Android 8.0 以上才可以使用 jvmti 机制
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                packageCodePath = context.getPackageCodePath();
                ClassLoader classLoader = context.getClassLoader();
                Method findLibrary = ClassLoader.class.getDeclaredMethod("findLibrary", String.class);
                String jvmtiAgentLibPath = (String) findLibrary.invoke(classLoader, "jvmti_agent");
                //copy lib to /data/data/com.dodola.jvmti/files/jvmti
                Log.d("jvmtiagentlibpath", "jvmtiagentlibpath " + jvmtiAgentLibPath);
                File filesDir = context.getFilesDir();
                File jvmtiLibDir = new File(filesDir, "jvmti");
                if (!jvmtiLibDir.exists()) {
                    jvmtiLibDir.mkdirs();

                }
                File agentLibSo = new File(jvmtiLibDir, "agent.so");
                if (agentLibSo.exists()) {
                    agentLibSo.delete();
                }
                Files.copy(Paths.get(new File(jvmtiAgentLibPath).getAbsolutePath()), Paths.get((agentLibSo).getAbsolutePath()));

                Log.d("Jvmti", agentLibSo.getAbsolutePath() + "," + context.getPackageCodePath());

                // 在9.0 中提供了 开启 agent 的 API
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                    Debug.attachJvmtiAgent(agentLibSo.getAbsolutePath(), null, classLoader);

                } else {
                    // 8.0 采用反射形式开启 Agent
                    try {
                        Class vmDebugClazz = Class.forName("dalvik.system.VMDebug");
                        Method attachAgentMethod = vmDebugClazz.getMethod("attachAgent", String.class);
                        attachAgentMethod.setAccessible(true);
                        attachAgentMethod.invoke(null, agentLibSo.getAbsolutePath());
                    } catch (Exception ex) {
                        ex.printStackTrace();
                    }
                }
                System.loadLibrary("jvmti_agent");
            }
        } catch (Exception ex) {
            ex.printStackTrace();
        }
    }

    // 修改类文件
    public static native void retransformClasses(Class[] classes);

    //查询一个类的对象个数
    public static native int countInstances(Class klass);


    public static native void setBreakPoints();

    public static native void setTag(Object obj);

    // 获取对象hashcode
    public static native int getObjHashcode(Object obj);

    public static native void setFieldAccessWatch(Class clazz, String field);

    public static native void setFieldModifyWatch(Class clazz, String field);


    // 获取对象引用
    private static native Object _getRoots(Object obj);


    public static native void _crawl(Object obj);

    public static Object getRoots(Object obj) {
        if (obj == null)
            throw new IllegalArgumentException("被检测对象为null");
        return _getRoots(obj);
    }

    private static Field[] filter(Field[] in) {
        ArrayList<Field> ret = new ArrayList<Field>();
        for (Field sf : in) {
            if (sf.getDeclaringClass() == ClassLoader.class
                    || sf.getDeclaringClass() == Proxy.class)
                continue;
            ret.add(sf);
        }
        return ret.toArray(new Field[ret.size()]);
    }

    public static void printEnter(String log) {
        Log.d("JVMTIHelper", "_____________________" + log);
    }

    public static void printEnter(Activity context, String log) {
        Toast.makeText(context, "======" + log, Toast.LENGTH_SHORT).show();
        Log.d("JVMTIHelper", "_____________________" + log);
    }


    public static native void regSignalHandler();

    public static native void testSignalHandler();
}
