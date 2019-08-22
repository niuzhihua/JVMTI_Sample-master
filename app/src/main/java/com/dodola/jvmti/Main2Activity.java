package com.dodola.jvmti;

import android.os.Bundle;
import android.app.Activity;
import android.widget.Toast;

import com.dodola.jvmtilib.JVMTIHelper;

public class Main2Activity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main2);


    }

    @Override
    protected void onResume() {
        super.onResume();
        JVMTIHelper.setTag(Main2Activity.this);
        Toast.makeText(this, "给Main2Activity对象设置tag 成功", Toast.LENGTH_LONG).show();
    }
}
