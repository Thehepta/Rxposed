package hepta.rxposed.manager.util;

import android.content.AttributionSource;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Process;
import android.util.Log;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.ArrayList;

import hepta.rxposed.manager.RxposedApp;
import hepta.rxposed.manager.fragment.check.ItemBean;

public class CheckTool {


    static {

    }

    public  native boolean chekcPreGetenv();
    public  native void  jni_hook();
    public  native void   jni_unhook();
    public  native boolean jni_hook_test();


    public CheckTool(){
        System.loadLibrary("check");
    }


    public static boolean get_rxposed_status(){
        boolean status = false;
        try {
            int zygote_host_uid = Process.getUidForName(InjectConfig.InjectArg);
            if(zygote_host_uid!=-1){
                status = true;
            }
            return status;
        }catch (Exception e){
            e.printStackTrace();
            return status;
        }
    }

    public  boolean check_jni_hook() {
        boolean no_hook_ret =  jni_hook_test();
        jni_hook();
        boolean hook_ret =  jni_hook_test();
        if(hook_ret == no_hook_ret){
            return false;
        }else {
            jni_unhook();
            boolean restore_hook_ret =  jni_hook_test();
            if(no_hook_ret == restore_hook_ret){
                return true;

            }
            return false;
        }
    }


    public boolean isNativeFunction(Class<?> clazz, String methodName,Class<?>[] cmp_parameter) {
        try {
            Method method = clazz.getDeclaredMethod(methodName,cmp_parameter);
            int modifiers = method.getModifiers();
            return Modifier.isNative(modifiers);
        } catch (NoSuchMethodException e) {
            e.printStackTrace();
            return false;
        }
    }

    public ItemBean Java_CreateApplicationContext() {
        //获取 ActivityThread 类
        Class<?> mActivityThreadClass = null;
        String currentAppName = RxposedApp.getRxposedContext().getPackageName();
        ItemBean itemBean = new ItemBean("Java_CreateApplicationContext",true);
        try {
            mActivityThreadClass = Class.forName("android.app.ActivityThread");

            //获取 ActivityThread 类
            Class<?> mLoadedApkClass = Class.forName("android.app.LoadedApk");
            //获取 ActivityThread 的 currentActivityThread() 方法
            Method currentActivityThread = mActivityThreadClass.getDeclaredMethod("currentActivityThread");
            currentActivityThread.setAccessible(true);
            //获取 ActivityThread 实例
            Object mActivityThread = currentActivityThread.invoke(null);

            Class<?> mCompatibilityInfoClass = Class.forName("android.content.res.CompatibilityInfo");
            Method getLoadedApkMethod = mActivityThreadClass.getDeclaredMethod("getPackageInfoNoCheck", ApplicationInfo.class, mCompatibilityInfoClass);

        /*
             public static final CompatibilityInfo DEFAULT_COMPATIBILITY_INFO = new CompatibilityInfo() {};
         */
            //以上注释是获取默认的 CompatibilityInfo 实例
            Field mCompatibilityInfoDefaultField = mCompatibilityInfoClass.getDeclaredField("DEFAULT_COMPATIBILITY_INFO");
            Object mCompatibilityInfo = mCompatibilityInfoDefaultField.get(null);

            //获取一个 ApplicationInfo实例
            ApplicationInfo applicationInfo = getSystemContext().getPackageManager().getApplicationInfo(currentAppName,0);
            //执行此方法，获取一个 LoadedApk
            Object mLoadedApk = getLoadedApkMethod.invoke(mActivityThread, applicationInfo, mCompatibilityInfo);
            Class<?> mContextImplClass = Class.forName("android.app.ContextImpl");
            Method createAppContext = mContextImplClass.getDeclaredMethod("createAppContext",mActivityThreadClass,mLoadedApkClass);
            createAppContext.setAccessible(true);
            Object context =  createAppContext.invoke(null,mActivityThread,mLoadedApk);

            return itemBean;

        } catch (PackageManager.NameNotFoundException e) {
            Log.e("check","getApplicationInfoAsUser NOT FOUND,return getSystemContext");
            itemBean.setMsg(e.getMessage());
            itemBean.setStatus(false);
            return itemBean;
        }catch (Exception e){
            itemBean.setMsg(e.getMessage());
            itemBean.setStatus(false);
            return itemBean;
        }
    }


    private Context getSystemContext() {
        Context context = null;
        try {
            Method method = Class.forName("android.app.ActivityThread").getMethod("currentActivityThread");
            method.setAccessible(true);
            Object activityThread = method.invoke(null);
            context = (Context) activityThread.getClass().getMethod("getSystemContext").invoke(activityThread);

        } catch (final Exception e) {
            e.printStackTrace();
            Log.e("check", "getSystemContext:"+e.toString());
        }
        return context;
    }

    public ItemBean Found_javaMethod(String className, String Method, Class<?>[] cmp_parameter){
        ItemBean itemBean = new ItemBean(className+":"+Method,true);
        try {

            Class<?> cls_zygote = Class.forName(className);
            cls_zygote.getDeclaredMethod(Method,cmp_parameter);

        } catch (ClassNotFoundException e) {
            itemBean.setMsg(className+":ClassNotFoundException");
            itemBean.setStatus(false);
            return itemBean;
        } catch (NoSuchMethodException e) {
            itemBean.setMsg(className+":"+className+":NoSuchMethodException");
            itemBean.setStatus(false);
            return itemBean;
        }

        return itemBean;
    }

    public ItemBean Found_getConstructorsMethod(String className, String Method, Class<?>[] cmp_parameter){


        ItemBean itemBean = new ItemBean(className+":"+Method,true);
        try {

            Class<?> cls_zygote = Class.forName(className);
            cls_zygote.getConstructor(cmp_parameter);

        } catch (ClassNotFoundException e) {
            itemBean.setMsg(className+":ClassNotFoundException");
            itemBean.setStatus(false);
            return itemBean;
        } catch (NoSuchMethodException e) {
            itemBean.setMsg(className+":"+className+":NoSuchMethodException");
            itemBean.setStatus(false);
            return itemBean;
        }

        return itemBean;
    }

    public native boolean chekc_GetArtmethodNative_init();


    public native boolean chekc_android_os_Process_getUidForName();

    public native boolean  ELFresolveSymbol();

    public void addCheckItem(ArrayList<ItemBean> itemBeans) {


    }

    public native String check_Process_setArgV0();

    public void chekc_java_method(ArrayList<ItemBean> itemBeans){


    }


}
