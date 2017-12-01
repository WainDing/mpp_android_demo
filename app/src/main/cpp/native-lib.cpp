#include <jni.h>
#include <string>
#include <fstream>
#include "MppDecoder.h"

extern "C"
JNIEXPORT jstring JNICALL
Java_com_example_root_mpp_1android_1demo_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C"
JNIEXPORT int JNICALL
        Java_com_example_root_mpp_1android_1demo_MppDecoder_MppDecoderRegister(
             JNIEnv *env,
             jclass clazz,
             jobject surface) {
    int ret = 0;
    int len = 0;
    std::string file_name = "/sdcard/Movies/test.h264";
    MppDecoder *mpp_dec = new MppDecoder();
    char *read_buf = new char[0xfff];

    LOGE("start to exec mpp decode program.\n");
    std::ifstream fin(file_name, std::ios::binary);
    if (!fin) {
        LOGE("failed to open input file %s.\n", file_name.c_str());
        return -1;
    }

    /*
     * set decode width & height
     * set display width & height
     */
    ret = mpp_dec->init(MPP_CTX_DEC, MPP_VIDEO_CodingAVC, 1920, 1080, 2560, 1440);
    if (ret) {
        goto RET;
    }

    while (!fin.eof()) {
        fin.read(read_buf, 0xfff);
        len = fin.gcount();
        LOGE("read file length %d.\n", len);

        if (len == 0xfff) {
            ret = mpp_dec->decode(read_buf, 0xfff, 0);
            if (ret) {
                goto RET;
            }
        } else {
            ret = mpp_dec->decode(read_buf, len, 1);
            if (ret) {
                goto RET;
            }
        }
    }

    if (read_buf) {
        delete(read_buf);
        read_buf = NULL;
    }

    mpp_dec->deinit();
    delete mpp_dec;

    LOGE("end mpp process,exit.\n");
RET:
    return ret;
}