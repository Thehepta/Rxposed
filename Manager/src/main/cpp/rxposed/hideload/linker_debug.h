//
// Created by chic on 2023/11/25.
//

#pragma once

#include <android/log.h>


#define LOG_TAG "hideLinker"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

#define TRACECODE(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,"[file %s],[line %d],[function:%s]",__FILE__, __LINE__,__func__);

#define DEBUG(x...)          LOGE(x)

#define INFO(x...)           LOGE(x)

#define PRINT(x...)          LOGE( x)
#define TRACE(x...)          LOGE( x)

#define DL_WARN(x...)          LOGE( x)

#define DL_ERR(x...)          LOGE( x)

#define DL_ERR_AND_LOG(x...)          LOGE( x)

#define DL_WARN_documented_change(level, x...)          LOGE(x)
