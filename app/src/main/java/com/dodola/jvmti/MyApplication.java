package com.dodola.jvmti;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;
import android.util.Log;

import com.dodola.jvmtilib.JVMTIHelper;

import java.lang.reflect.Field;
import java.util.Arrays;

/**
 * Created by 31414 on 2019/6/21.
 */

public class MyApplication extends Application {

    public LeakActivity testLeak;
    static MyApplication application;

    @Override
    public void onCreate() {
        super.onCreate();
        application = this;

    }

}
