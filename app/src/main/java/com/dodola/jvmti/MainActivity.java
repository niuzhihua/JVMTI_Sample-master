package com.dodola.jvmti;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

import com.dodola.jvmtilib.JVMTIHelper;
import com.dodola.jvmtilib.TestClass;

import java.util.ArrayList;

public class MainActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        JVMTIHelper.init(this);

        findViewById(R.id.set_field).setOnClickListener(v -> {
            JVMTIHelper.setFieldAccessWatch(TestClass.class, "str");  // 设置 field 被访问的监听
            JVMTIHelper.setFieldModifyWatch(TestClass.class, "str");  // 设置 field 被修改的监听
        });
        findViewById(R.id.field_Access).setOnClickListener(v -> {
            String b = TestClass.getStr();  // 访问field
            String c = TestClass.getStr();
        });

        findViewById(R.id.field_Modifi).setOnClickListener(v -> {

            TestClass.setStr("aaa"); // 修改field
        });


        findViewById(R.id.button_gc).setOnClickListener(v -> {

            TestClass t = new TestClass();
            t = null;
            System.gc();
            System.runFinalization();
        });

        findViewById(R.id.thread).setOnClickListener(v -> {
            // 监听线程启动和结束  OK
            testThreadStartEnd();
        });
        findViewById(R.id.set_tag).setOnClickListener(v -> {
            // 在Main2Activity 里面给Main2Activity对象设置tag
            startActivity(new Intent(MainActivity.this, Main2Activity.class));
        });


        findViewById(R.id.button_modify_class).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // 1: 使用jvmti 设置动态注册监听。在监听函数中 把retransformClasses 的native 函数添加到bootclassloader
                // 2: 动态注册 retransformClasses 方法，当调用retransformClasses 方法时，就可以使用jvmti 了。
                JVMTIHelper.retransformClasses(new Class[]{Activity.class});  // OK
            }
        });


        findViewById(R.id.exception_catch).setOnClickListener(v -> {
            testExceptionListener(); // java层异常捕获  OK
        });
        findViewById(R.id.count_instance).setOnClickListener(v -> {
            int count = JVMTIHelper.countInstances(Button.class); // 查询一个类的对象数量 OK
            Toast.makeText(MainActivity.this, "count:" + count, Toast.LENGTH_LONG).show();
        });

        findViewById(R.id.get_obj_hashcode).setOnClickListener(v -> {
            testObjectHashCode();// 查询一个对象的 hashcode OK
        });

    }


    public void methodEntryExit() {

//        testCrash();

        testFramePop();

    }

    private int testFramePop() {
        // 1:调用 NotifyFramePop(native) 函数
        JVMTIHelper.countInstances(LeakActivity.class);
        // 测试 本方法异常退出 ， 只要本方法不抛出异常 就是正常结束。

        String a = null;
        boolean b = a.equals("a");

        return 0;
    }


    private void testRawMonitor() {
        System.out.println("--start---");
        try {
            Thread.sleep(5000);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        System.out.println("--end---");
    }


    //设置breakpoints
    public void setBreakPoints(View view) {  // OK
        methodTest("6");
        JVMTIHelper.setBreakPoints();
        methodTest("a");
    }

    // 检测 breakpoints 是否触发了
    public void checkBreakPoints(View view) {  // OK
        testBreakPoint();
    }

    private void testResourceExhausted() {
        ArrayList<byte[]> list = new ArrayList<>();
        while (true) {
            list.add(new byte[1024 * 1024]);
        }
    }


    private int framePop2() {
        try {
            int a = 3;
            int b = 0;
            return a / b;
        } catch (Exception e) {
            e.printStackTrace();
        }
        return -1;
    }

    private int framePop1() {
        int a = 10;
        int b = 10;
        return a / b;
    }


    private void testMethodEntryExit() {


        methodTest("abc");
        niuzhihua2("abc");
        System.out.println("-java-testMethodEntryExit--");
    }

    private void testCrash() {
        int a = 0;
        int b = 3;
        int c = b / a;
    }

    private void testThreadStartEnd() {
        Thread t = new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    Thread.sleep(3000);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        });
        t.start();
    }

    Bitmap icon;
    Object str;

    private void testObjectHashCode() {
        str = new Object();
        icon = BitmapFactory.decodeResource(getResources(), R.mipmap.ic_launcher);
        int nativeHashcode = JVMTIHelper.getObjHashcode(this);
        String msg = "java-hashcode:" + this.hashCode() + "nativeHashcode:" + nativeHashcode;
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show();
        System.out.println(msg);
    }

    private void testExceptionListener() {
        int a = 0;
        int b = 1;
        String c = "2";
        String c2 = "3";
        int[] arr = null;  // 本方法第四行
        int bv = arr.length; // 发生异常位置  5
    }


    private void methodTest(String s) {
        s = "aga";

        int a = 2;
        System.out.println("--java method: methodTest---");
        int b = a + 2;
    }

    private void niuzhihua2(String s) {
        s = "aga";
        try {
            Thread.sleep(100);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    public static LeakActivity in;

    public void open_leakactivity(View view) {
        startActivity(new Intent(this, LeakActivity.class));
    }

    public void registSigHanlder(View view) {

        JVMTIHelper.regSignalHandler();
    }

    public void testSigHanlder(View view) {
        JVMTIHelper.testSignalHandler();
    }


    private void testBreakPoint() {
        int a = 2;
        int ab = 3;
    }


}

