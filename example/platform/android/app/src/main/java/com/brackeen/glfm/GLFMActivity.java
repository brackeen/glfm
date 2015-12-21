package com.brackeen.glfm;

import android.app.NativeActivity;

public class GLFMActivity extends NativeActivity {
    static {
        System.loadLibrary("GLFMExample");
    }
}
