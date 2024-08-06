package hepta.rxposed.manager.util;


import android.content.Context;
import android.content.Intent;
import android.util.Log;

import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.Enumeration;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import hepta.rxposed.manager.BuildConfig;
import hepta.rxposed.manager.RxposedApp;

public  class InjectTool {

    // Used to load the 'injecttool' library on application startup.

    /**
     * A native method that is implemented by the 'injecttool' native library,
     * which is packaged with this application.
     */

    public static String TAG = "InjectTool";

    public static void zygote_reboot() throws IOException {
        rootRun("killall zygote");
    }

    public static void Application_reboot() throws IOException {
        Context context = RxposedApp.getRxposedContext();
        Intent intent = context.getPackageManager().getLaunchIntentForPackage(context.getPackageName());
        assert intent != null;
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP); //与正常页面跳转一样可传递序列化数据,在Launch页面内获得
        intent.putExtra("REBOOT", "reboot");
        context.startActivity(intent);
        android.os.Process.killProcess(android.os.Process.myPid());
    }

    // /data/user/0/hepta.rxposed.manager/files/arm64_InjectTool  -n zygote64 -hidemaps 1 -so /data/user/0/hepta.rxposed.manager/files/arm64_libnativeloader.so -symbols _Z14Ptrace_ZygotesPKc hepta.rxposed.manager.Provider;com.hep>                                                     <

    public static boolean StartInject()  {
        //修改selinux 规则
        set_selinux_context();
        //ptrace zygote
        zygote_ptrace();
        return  true;
    }

    private static void set_selinux_context() {
//
        String selinux_policy = InjectConfig.policy_path +" --apply "+ InjectConfig.policy_te + " --live ";
        rootRun(selinux_policy);
        String selinux_domain = getAppDomain();
        // magiskpolicy  --live "allow zygote unstrsfe binder {  call  transfer }"
        String selinux_policy_domain = InjectConfig.policy_path +  " --live \"allow zygote "+selinux_domain +" binder { call transfer }\"";
        rootRun(selinux_policy_domain);


    }

    private static String getAppDomain() {
        String selinux = shell("id -Z");
        assert selinux != null;
        return selinux.split(":")[2];
    }



    ///data/user/0/hepta.rxposed.manager/files/assets/armv7_InjectTool -n zygote -hidemaps 1 -so /data/user/0/hepta.rxposed.manager/files/lib/armeabi-v7a/librxposed.so -symbols _Z14Ptrace_ZygotesPKc 10288:hepta.rxposed.manager:hepta.rxposed.manager.Provider
    public static  void zygote_ptrace()  {
        //zygote 附加
        String hidemaps = "";
        if (InjectConfig.hidemaps){ //添加maps hide功能，目前可能被放弃
            hidemaps = "-hidemaps";
        }else {
            InjectConfig.mount_libdir(InjectConfig.mountWorkDir,"");
        }
//        String InjectArg = InjectConfig.InjectArg;
//        String cmd_arm64 = InjectConfig.arm64_InjectTool+" -n zygote64 "+ hidemaps + " -so " + InjectConfig.arm64_InjectSo+" -symbols _Z14Ptrace_ZygotesPKc "+InjectArg;
//
//        LogFileHelper.writeLog(cmd_arm64);
//        String ret_cmd_64 = rootRun(cmd_arm64);
//        LogFileHelper.writeLog(ret_cmd_64);
//
//        String cmd_arm32 = InjectConfig.arm32_InjectTool +" -n zygote   "+ hidemaps + " -so " + InjectConfig.arm32_InjectSo +" -symbols _Z14Ptrace_ZygotesPKc "+InjectArg;
//
//        LogFileHelper.writeLog(cmd_arm32);
//        String ret_cmd_32 = rootRun(cmd_arm32);
//        LogFileHelper.writeLog(ret_cmd_32);

    }

    public static void inject_text(){
        String cmd_arm64 = InjectConfig.arm64_InjectTool+" -n zygote64 -hidemaps  -so "+ InjectConfig.arm64_InjectSo+" -symbols _Z14Ptrace_ZygotesPKc "+InjectConfig.InjectArg;
        String cmd_armv7 = InjectConfig.arm64_InjectTool+" -n zygote -hidemaps  -so "  + InjectConfig.arm32_InjectSo +" -symbols _Z14Ptrace_ZygotesPKc "+InjectConfig.InjectArg;


        LogFileHelper.writeLog(cmd_arm64);
        String ret_cmd_64 = rootRun(cmd_arm64);
        LogFileHelper.writeLog(ret_cmd_64);

        LogFileHelper.writeLog(cmd_armv7);
        String ret_cmd_32 = rootRun(cmd_armv7);
        LogFileHelper.writeLog(ret_cmd_32);
    }

        //用于指定进程注入，目前用的不多
    // /data/user/0/hepta.rxposed.manager/files/arm64_InjectTool -p 4903  -so /data/user/0/hepta.rxposed.manager/files/arm64_libnativeloader.so -symbols _Z9Inject_ProcessPKc hepta.rxposed.manager.Provider;com.hep>                                                     <
    public static void Inject_Process(int Pid,String package_list)  {

        String Inject_Arg = InjectConfig.HostProviderName+":"+package_list;
        int arch = GetProcessArchByPid(Pid);
        if(arch == 64){

            String cmd_arm64 = InjectConfig.arm64_InjectTool+" -p "+Pid+" -so "+ InjectConfig.arm64_InjectSo+" -symbols _Z14Inject_PorcessPKc "+Inject_Arg;;
            Log.e(TAG,"arm64:"+cmd_arm64);
            rootRun(cmd_arm64);
        }else {

            String cmd_armv7 = InjectConfig.arm32_InjectTool +" -p "+Pid+" -so "+ InjectConfig.arm32_InjectSo +" -symbols _Z14Inject_PorcessPKc "+Inject_Arg;
            Log.e(TAG,"arm32:"+cmd_armv7);
            rootRun(cmd_armv7);
        }

    }

    private static int GetProcessArchByPid(int pid) {
        String exe = rootRun("file -L /proc/"+pid+"/exe");
        if(exe.contains("linker64")){
            return 64;
        }else {
            return 32;
        }
    }


    public static boolean copyAssetToDst(Context context, String fileName,String dstFilePath){
        try {
            File outFile =new File(dstFilePath);
            if (!outFile.exists()){
                boolean res=outFile.createNewFile();
                if (!res){
                    return false;
                }
            }else {
                if (outFile.length()>10){//表示已经写入一次
                    outFile.delete();
                }
            }

            InputStream is=context.getAssets().open(fileName);
            FileOutputStream fos = new FileOutputStream(outFile);
            byte[] buffer = new byte[1024];
            int byteCount;
            while ((byteCount = is.read(buffer)) != -1) {
                fos.write(buffer, 0, byteCount);
            }
            fos.flush();
            is.close();
            fos.close();
            return true;
        } catch (IOException e) {
            e.printStackTrace();
        }

        return false;
    }


        public static String shell(String cmd){
        try {
            Process process = Runtime.getRuntime().exec(cmd);
            BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()));
            String line = "";
            while (true) {
                if ((line = reader.readLine()) == null) break;
                return line;
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }

        return null;
    }


    public static String rootRun(String cmd)
    {
        Log.e(TAG,cmd);
        StringBuilder Result  = new StringBuilder();
        try {
            // 申请获取root权限
            Process process = Runtime.getRuntime().exec(InjectConfig.su_path); //"/system/xbin/su"
            // 获取输出流
            OutputStream outputStream = process.getOutputStream();
            InputStream is = process.getInputStream();
            InputStream es = process.getErrorStream();
            DataOutputStream dataOutputStream = new DataOutputStream(outputStream);
            dataOutputStream.writeBytes(cmd);
            dataOutputStream.flush();
            dataOutputStream.close();
            outputStream.close();
            int code = process.waitFor();
            String is_line = null;
            String es_line = null;
//            Log.d(TAG, "Run:\"" + cmd +"\", "+"process.waitFor() = " + code);
            BufferedReader br;
            br = new BufferedReader(new InputStreamReader(is, StandardCharsets.UTF_8));
            while ((is_line = br.readLine()) != null) {
                Log.d(TAG, "cmd > "+is_line);
                Result.append(is_line).append("\n");
            }

            br = new BufferedReader(new InputStreamReader(es, StandardCharsets.UTF_8));
            while ((es_line = br.readLine()) != null) {
                Log.e(TAG, "errcmd > "+es_line);
                Result.append(es_line);
            }
        } catch (Throwable t) {
            t.printStackTrace();
        }
        return Result.toString();
    }



}