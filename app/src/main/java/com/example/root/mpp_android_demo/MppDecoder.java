package com.example.root.mpp_android_demo;

import java.util.Objects;

/**
 * Created by root on 17-11-30.
 */

public class MppDecoder {
    static {
        System.loadLibrary("native-lib");
        System.loadLibrary("mpp");
        System.loadLibrary("vpu");
    }

    public static native int MppDecoderRegister(Object surface);
}
