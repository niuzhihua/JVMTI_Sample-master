package com.dodola.jvmti;

import android.app.Activity;
import android.os.Debug;
import android.os.Handler;
import android.os.Message;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;

import com.dodola.jvmtilib.JVMTIHelper;
import com.dodola.jvmtilib.TestBase;

import java.lang.reflect.Field;
import java.util.Arrays;

public class LeakActivity extends Activity implements ILeak {


    static LeakActivity instance;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_leak);
        instance = this;
        MainActivity.in = this;
        MyApplication.application.testLeak = this;
    }


    static int t = 0;

    @Override
    protected void onDestroy() {
        super.onDestroy();
        t++;
        if (t == 1) {

            JVMTIHelper._crawl(this);
                Object obj = JVMTIHelper.getRoots(this);
                if (obj != null) {
                System.out.println("------------------");
                TestBase.printGcRoots(obj);
                System.out.println("------------------");

            }
        } else {
            System.out.println("t<4:" + t);
        }


    }

    @Override
    public void leak() {

    }
}
