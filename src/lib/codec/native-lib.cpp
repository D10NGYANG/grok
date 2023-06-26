#include "jni.h"
#include "grok_codec.h"

extern "C" JNIEXPORT jint JNICALL
Java_com_hailiao_util_imageio_ImageCompress_encode(
        JNIEnv* env,
        jobject /* this */,
        jstring inputPath,
        jstring outputPath,
        jstring rate) {
    const char *input = env->GetStringUTFChars(inputPath, NULL);
    const char *output = env->GetStringUTFChars(outputPath, NULL);
    const char *ratio = env->GetStringUTFChars(rate, NULL);

    const char *argvs0 = "grk_compress";
    const char *argvs1 = "-i";
    const char *argvs3 = "-o";
    const char *argvs5 = "-r";

    char **argvs = new char*[8];
    argvs[0] = const_cast<char*>(argvs0);
    argvs[1] = const_cast<char*>(argvs1);
    argvs[2] = const_cast<char*>(input);
    argvs[3] = const_cast<char*>(argvs3);
    argvs[4] = const_cast<char*>(output);
    argvs[5] = const_cast<char*>(argvs5);
    argvs[6] = const_cast<char*>(ratio);
    argvs[7] = nullptr;

    int result = grk_codec_compress(8, argvs, nullptr, nullptr);
    // 返回result
    env->ReleaseStringUTFChars(inputPath, input);
    env->ReleaseStringUTFChars(outputPath, output);
    env->ReleaseStringUTFChars(rate, ratio);
    delete[] argvs;
    return result;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_hailiao_util_imageio_ImageCompress_decode(
        JNIEnv* env,
        jobject /* this */,
        jstring inputPath,
        jstring outputPath) {
    const char *input = env->GetStringUTFChars(inputPath, 0);
    const char *output = env->GetStringUTFChars(outputPath, 0);

    const char *argvs0 = "grk_decompress";
    const char *argvs1 = "-i";
    const char *argvs3 = "-o";

    char **argvs = new char*[6];
    argvs[0] = const_cast<char*>(argvs0);
    argvs[1] = const_cast<char*>(argvs1);
    argvs[2] = const_cast<char*>(input);
    argvs[3] = const_cast<char*>(argvs3);
    argvs[4] = const_cast<char*>(output);
    argvs[5] = nullptr;
    int result = grk_codec_decompress(6, argvs);
    // 返回result
    env->ReleaseStringUTFChars(inputPath, input);
    env->ReleaseStringUTFChars(outputPath, output);
    delete[] argvs;
    return result;
}