package com.dodola.jvmti;

/**
 * Created by 31414 on 2019/8/16.
 */

public interface ILeak {
    int a = 8;
    int b = 55;
    int c = 66;

    void leak();
}
