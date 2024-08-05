//
// Created by thehepta on 2024/2/19.
//

#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#define LOG_TAG "Rxposed_Test"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

//_Z11Ptrace_InitPKc
void Ptrace_Init(const char* arg){
    LOGE("Rxposed_Test inject successful");
    // 获取并打印环境变量
    char *LD_PRELOAD = getenv("LD_PRELOAD");
    if (LD_PRELOAD != NULL) {
        LOGE("LD_PRELOAD = %s\n", LD_PRELOAD);
    } else {
        LOGE("LD_PRELOAD not found\n");
        // 设置新的环境变量
        if (setenv("LD_PRELOAD", arg, 1) != 0) {
            LOGE("LD_PRELOAD set failed\n");
            return;
        }

    }
}
