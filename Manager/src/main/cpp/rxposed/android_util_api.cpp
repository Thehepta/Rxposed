//
// Created by chic on 2023/5/15.
//


#include <dlfcn.h>
#include "android_util_api.h"
#include "hideload/entry.h"

using namespace std;


vector<string> string_split(string str,string pattern)
{
    string::size_type pos;
    vector<string> result;
    str += pattern;//扩展字符串以方便操作
    int size = str.size();
    for (int i = 0; i < size; i++)
    {
        pos = str.find(pattern, i);
        if (pos < size)
        {
            string s = str.substr(i, pos - i);
            result.push_back(s);
            i = pos + pattern.size() - 1;
        }
    }
    return result;
}


//清除异常，打印java堆栈好用户输入，继续执行
bool NDK_ExceptionCheck(JNIEnv *env,const char* message){

    if(env->ExceptionCheck()){
        LOGD("%s",message);
        env->ExceptionDescribe();
        env->ExceptionClear();
        return true;
    }
    return false;
}


JNIEnv *Pre_GetEnv() {
    //这个函数有使用限制可能无法在zygote以外的应用进程中使用，主要是因为so命名限制的问题 dlopen 无法打开libandroid_runtime.so
    //如果想要在任何地方使用，需要突破dlopen限制，比如使用dobby的全局符号查找工具
    void *libandroid_runtime = dlopen("libandroid_runtime.so", RTLD_NOW);
    if (libandroid_runtime == nullptr) {
        return nullptr;
    }
    void *getAndroidRuntimeEnv = reinterpret_cast<void *>(dlsym(libandroid_runtime,
                                                                "_ZN7android14AndroidRuntime9getJNIEnvEv"));
    if (getAndroidRuntimeEnv == nullptr) {
        return nullptr;
    }
    dlclose(libandroid_runtime);
    return ((JNIEnv *(*)()) getAndroidRuntimeEnv)();
}



jobject PathClassLoaderLoadAPK(JNIEnv *pEnv,jstring apkSource,jstring nativelib){

    jclass PathClassLoader_cls = pEnv->FindClass("dalvik/system/PathClassLoader");
    jmethodID PathClassLoader_init_mth = pEnv->GetMethodID(PathClassLoader_cls, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    jclass ClassLoader_cls = pEnv->FindClass("java/lang/ClassLoader");
    jmethodID ClassLoader_getSystemClassLoader_cls = pEnv->GetStaticMethodID(ClassLoader_cls, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    jobject SystemClassLoader_obj = pEnv->CallStaticObjectMethod(ClassLoader_cls,ClassLoader_getSystemClassLoader_cls);
    jobject ApkClassLoader = pEnv->NewObject(PathClassLoader_cls,PathClassLoader_init_mth,apkSource,nativelib,SystemClassLoader_obj);
    pEnv->DeleteLocalRef(SystemClassLoader_obj);
    return ApkClassLoader;
}

void load_apk_And_Call_Class_Entry_Method(JNIEnv *pEnv, jobject android_context,string source,string NativelibPath,string Entry_class,string Entry_method,string hide,string argument) {

    LOGE("enbale pkgName:%s ",source.c_str());
    jobject  ApkClassLoader = nullptr;
    jobject entryClass_obj = nullptr;
    jstring apkSource = pEnv->NewStringUTF(source.c_str());
    jstring rxposed_argument = pEnv->NewStringUTF(argument.c_str());
    if(strncmp(hide.c_str(),"true", strlen("true"))==0){
        ApkClassLoader = FilehideLoadApkModule(pEnv, (char*)source.c_str());
    } else{
        jstring nativelib = pEnv->NewStringUTF(NativelibPath.c_str());
        ApkClassLoader = PathClassLoaderLoadAPK(pEnv, apkSource, nativelib);
    }

    jclass ClassLoader_cls = pEnv->FindClass("java/lang/ClassLoader");
    jmethodID Class_loadClass_method = pEnv->GetMethodID(ClassLoader_cls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring class_name = pEnv->NewStringUTF(Entry_class.c_str());

    if(pEnv->ExceptionCheck()){
//        goto out2;
    }
//    Class clazz = dexClassLoader.loadClass(name);
    LOGE("loadclass Entry_class=%s ",Entry_class.c_str());
    entryClass_obj = pEnv->CallObjectMethod(ApkClassLoader, Class_loadClass_method, class_name);
    if(pEnv->ExceptionCheck()){
//        goto out2;
    }
//    Method native_hook = clazz.getMethod("native_hook");
    LOGE("invoke method_name=%s ",Entry_method.c_str());
    jmethodID call_method_mth = pEnv->GetStaticMethodID(static_cast<jclass>(entryClass_obj), Entry_method.c_str(), "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)V");
    DEBUG();
    if(pEnv->ExceptionCheck()){
        goto out2;
    }
    DEBUG();
    pEnv->CallStaticVoidMethod(static_cast<jclass>(entryClass_obj), call_method_mth, android_context, apkSource,rxposed_argument);
    DEBUG();
    if(pEnv->ExceptionCheck()){

    }
    DEBUG();
    out2:
    pEnv->ExceptionDescribe();
    pEnv->ExceptionClear();//清除引发的异常，在Java层不会打印异常堆栈信息，如果不清除，后面的调用ThrowNew抛出的异常堆栈信息会
    pEnv->DeleteLocalRef(ApkClassLoader);
    pEnv->DeleteLocalRef(entryClass_obj);
}


jobject CreateApplicationContext(JNIEnv *env, string pkgName,uid_t currentUid) {

    jobject ApplicationContext = nullptr;
    //获取Activity Thread的实例对象
    jclass mActivityThreadClass = env->FindClass("android/app/ActivityThread");
    jclass mLoadedApkClass = env->FindClass("android/app/LoadedApk");
    jclass mContextImplClass = env->FindClass("android/app/ContextImpl");
    jclass mCompatibilityInfoClass = env->FindClass("android/content/res/CompatibilityInfo");
    jmethodID getLoadedApkMethod = env->GetMethodID(mActivityThreadClass,"getPackageInfoNoCheck","(Landroid/content/pm/ApplicationInfo;Landroid/content/res/CompatibilityInfo;)Landroid/app/LoadedApk;");

    if(NDK_ExceptionCheck(env,"find class android/app/ActivityThread failed")){
        return nullptr;
    }

    jmethodID currentActivityThread = env->GetStaticMethodID(mActivityThreadClass, "currentActivityThread", "()Landroid/app/ActivityThread;");
    jobject at = env->CallStaticObjectMethod(mActivityThreadClass, currentActivityThread);
    if(at == nullptr){
//        LOGE("Failed to call currentActivityThread,at is null");
        return nullptr;
    }

    //ContextImpl，getSystemContext
    jmethodID getApplication = env->GetMethodID(mActivityThreadClass, "getSystemContext", "()Landroid/app/ContextImpl;");
    jobject SystemContext = env->CallObjectMethod(at, getApplication);
    if(SystemContext == nullptr){
        LOGE("Failed to call getRxposedContext,SystemContext is null");
        return nullptr;
    }
    jmethodID getPackageManager_method = env->GetMethodID(mContextImplClass, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jclass PackageManager_cls = env->FindClass("android/content/pm/PackageManager");
    jmethodID pm_getApplicationInfo_method = env->GetMethodID(PackageManager_cls, "getApplicationInfo", "(Ljava/lang/String;I)Landroid/content/pm/ApplicationInfo;");
    jmethodID pm_getPackagesForUid_method = env->GetMethodID(PackageManager_cls, "getPackagesForUid", "(I)[Ljava/lang/String;");
    jobject PackageManager = env->CallObjectMethod(SystemContext, getPackageManager_method);

    jobjectArray packageNameArray = static_cast<jobjectArray>(env->CallObjectMethod(PackageManager,pm_getPackagesForUid_method,(jint) currentUid));
    jstring jstrPackageName;
    if (packageNameArray != nullptr && env->GetArrayLength(packageNameArray) > 0) {
        jstrPackageName = (jstring) env->GetObjectArrayElement(packageNameArray, 0);
    } else{
        return nullptr;
    }
    jobject applicationInfo = env->CallObjectMethod(PackageManager,pm_getApplicationInfo_method,jstrPackageName,0);
    string err_message = " getApplicationInfo not found,return SystemContext,uid = "+currentUid;
    if(NDK_ExceptionCheck(env,(char *)err_message.c_str())){
//        int version = android_get_device_api_level()
        return nullptr;
    }
    jfieldID mCompatibilityInfoDefaultField = env->GetStaticFieldID(mCompatibilityInfoClass,"DEFAULT_COMPATIBILITY_INFO","Landroid/content/res/CompatibilityInfo;");
    jobject mCompatibilityInfo = env->GetStaticObjectField(mCompatibilityInfoClass,mCompatibilityInfoDefaultField);
    jmethodID createAppContext_method = env->GetStaticMethodID(mContextImplClass, "createAppContext", "(Landroid/app/ActivityThread;Landroid/app/LoadedApk;)Landroid/app/ContextImpl;");
    jobject  mLoadedApk = env->CallObjectMethod(at,getLoadedApkMethod,applicationInfo,mCompatibilityInfo);
    ApplicationContext = env->CallStaticObjectMethod(mContextImplClass,createAppContext_method,at,mLoadedApk);

    env->DeleteLocalRef(at);
    return ApplicationContext;
}


void print_java_stack(JNIEnv *env){
    DEBUG("");
    jclass throwableClass = env->FindClass("java/lang/Throwable");
    jmethodID getStackTraceMethod = env->GetMethodID(throwableClass, "getStackTrace", "()[Ljava/lang/StackTraceElement;");
    jthrowable exception = env->ExceptionOccurred();
    if (exception != NULL) {
        env->ExceptionClear();
    }
    jthrowable newException = (jthrowable)env->NewGlobalRef(exception);
    jobjectArray stackTrace = (jobjectArray)env->CallObjectMethod(newException, getStackTraceMethod);

    // 打印Java堆栈信息
    jsize stackTraceLength = env->GetArrayLength(stackTrace);
    for (int i = 0; i < stackTraceLength; i++) {
        DEBUG("");

        jobject stackTraceElement = env->GetObjectArrayElement(stackTrace, i);
        jstring stackTraceElementString = (jstring)env->CallObjectMethod(stackTraceElement, env->GetMethodID(env->FindClass("java/lang/StackTraceElement"), "toString", "()Ljava/lang/String;"));
        const char* stackTraceElementChars = env->GetStringUTFChars(stackTraceElementString, NULL);
        LOGE("Java Stack Trace Element %d: %s", i, stackTraceElementChars);
        env->ReleaseStringUTFChars(stackTraceElementString, stackTraceElementChars);
    }
    DEBUG("");
    // 释放资源
    env->DeleteGlobalRef(newException);

}

const char* get_Process_setArgV0(JNIEnv *env) {
    // TODO: implement check_Process_setArgV0()

    jclass  Process_cls = env->FindClass("android/os/Process");
    jmethodID javamethod = env->GetStaticMethodID(Process_cls,"setArgV0Native", "(Ljava/lang/String;)V");
    if (javamethod == nullptr) {
        if(env->ExceptionCheck()) {
            env->ExceptionClear();
            javamethod = env->GetStaticMethodID(Process_cls, "setArgV0", "(Ljava/lang/String;)V");
            if(javamethod == nullptr){
                if(env->ExceptionCheck()) {
                    env->ExceptionClear();
                    return nullptr;
                }
            } else{
                return "setArgV0";
            }
        }
    } else{
        return "setArgV0Native";
    }
}






