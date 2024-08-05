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


    static {
        getConfigSave();
        Context context = RxposedApp.getRxposedContext();
        unziplib(context.getApplicationInfo().sourceDir,context.getFilesDir().getAbsolutePath()+ File.separator);
        String AppFilePath = context.getFilesDir().getAbsolutePath()+ File.separator;
        int App_Uid = RxposedApp.getRxposedContext().getApplicationInfo().uid;
        InjectConfig.policy_path = AppFilePath+InjectConfig.assets_policy_tool;
        InjectConfig.policy_te = AppFilePath+InjectConfig.assets_policy_te;
        InjectConfig.InjectArg = App_Uid+":"+BuildConfig.APPLICATION_ID+":"+ InjectConfig.HostProviderName;;

        InjectConfig.arm64_InjectTool = AppFilePath+InjectConfig.assets_arm64_InjectTool;
        InjectConfig.arm32_InjectTool = AppFilePath+InjectConfig.assets_arm32_InjectTool;
        String soName = "lib"+BuildConfig.Rxposed_Inject_So+".so";
        InjectConfig.soName = soName;
        InjectConfig.appfiles_arm64_InjectSo = AppFilePath+"lib/arm64-v8a/"+soName;
        InjectConfig.arm64_InjectSo = InjectConfig.appfiles_arm64_InjectSo;
        InjectConfig.appfiles_arm32_InjectSo = AppFilePath+"lib/armeabi-v7a/"+soName;
        InjectConfig.arm32_InjectSo = InjectConfig.appfiles_arm32_InjectSo;

        try {
            Runtime.getRuntime().exec("chmod +x "+InjectConfig.arm64_InjectTool);
            Runtime.getRuntime().exec("chmod +x "+InjectConfig.arm32_InjectTool);
            Runtime.getRuntime().exec("chmod +x "+InjectConfig.policy_path);

        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private static void getConfigSave() {
        InjectConfig.su_path = MmkvManager.INSTANCE.getString("supath","su");
        InjectConfig.mountWorkDir = MmkvManager.INSTANCE.getString("mountWorkDir","/apex/com.android.i18nrxp");
        InjectConfig.hidemaps = MmkvManager.INSTANCE.getBoolean("hidemaps",true);
        InjectConfig.injectInit = MmkvManager.INSTANCE.getBoolean("hidemaps",false);
    }

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
            mount_libdir();
        }
        String InjectArg = InjectConfig.InjectArg;
        String cmd_arm64 = InjectConfig.arm64_InjectTool+" -n zygote64 "+ hidemaps + " -so " + InjectConfig.arm64_InjectSo+" -symbols _Z14Ptrace_ZygotesPKc "+InjectArg;

        LogFileHelper.writeLog(cmd_arm64);
        String ret_cmd_64 = rootRun(cmd_arm64);
        LogFileHelper.writeLog(ret_cmd_64);

        String cmd_arm32 = InjectConfig.arm32_InjectTool +" -n zygote   "+ hidemaps + " -so " + InjectConfig.arm32_InjectSo +" -symbols _Z14Ptrace_ZygotesPKc "+InjectArg;

        LogFileHelper.writeLog(cmd_arm32);
        String ret_cmd_32 = rootRun(cmd_arm32);
        LogFileHelper.writeLog(ret_cmd_32);

    }

    private static void mount_libdir() {

        InjectConfig.arm32_InjectSo = "/apex/com.android.i18nrxp/lib/"+InjectConfig.soName;
        InjectConfig.arm64_InjectSo = "/apex/com.android.i18nrxp/lib64/"+InjectConfig.soName;
        rootRun("mkdir -p /apex/com.android.i18nrxp");
        rootRun("mount -t tmpfs tmpfs /apex/com.android.i18nrxp");
        rootRun("chown -R system:system /apex/com.android.i18nrxp");
        rootRun("chcon -R u:object_r:system_file:s0 /apex/com.android.i18nrxp");
        rootRun("cp /data/data/hepta.rxposed.manager/files/lib/arm64-v8a /apex/com.android.i18nrxp/lib64  -R");
        rootRun("cp /data/data/hepta.rxposed.manager/files/lib/armeabi-v7a /apex/com.android.i18nrxp/lib  -R");
        rootRun("chcon -R u:object_r:system_lib_file:s0 /apex/com.android.i18nrxp/lib");
        rootRun("chcon -R u:object_r:system_lib_file:s0 /apex/com.android.i18nrxp/lib64");
        rootRun("chmod 0644 /apex/com.android.i18nrxp/lib64/*");
        rootRun("chmod 0644 /apex/com.android.i18nrxp/lib/*");


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


    //解压lib 和assets 目录到 filee 目录下
    public static void unziplib(String zippath,String resourcepath){
        //判断生成目录是否生成，如果没有就创建
        File pathFile=new File(resourcepath);
        if(!pathFile.exists()){
            pathFile.mkdirs();
        }
        ZipFile zp=null;
        try{
            //指定编码，否则压缩包里面不能有中文目录
            zp=new ZipFile(zippath, Charset.forName("gbk"));
            //遍历里面的文件及文件夹
            Enumeration entries=zp.entries();
            while(entries.hasMoreElements()){
                ZipEntry entry= (ZipEntry) entries.nextElement();
                String zipEntryName=entry.getName();
                if(zipEntryName.contains("lib/") || zipEntryName.contains("assets/")){
                    InputStream in=zp.getInputStream(entry);
                    String outpath=(resourcepath+zipEntryName).replace("/",File.separator);
                    //判断路径是否存在，不存在则创建文件路径
                    File file = new  File(outpath.substring(0,outpath.lastIndexOf(File.separator)));
                    if(!file.exists()){
                        file.mkdirs();
                    }
                    //判断文件全路径是否为文件夹,如果是,不需要解压
                    if(new File(outpath).isDirectory())
                        continue;
                    OutputStream out=new FileOutputStream(outpath);
                    byte[] bf=new byte[2048];
                    int len;
                    while ((len=in.read(bf))>0){
                        out.write(bf,0,len);
                    }
                    in.close();
                    out.close();
                }

            }
            zp.close();
        }catch ( Exception e){
            e.printStackTrace();
        }
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
        String Result  ="";
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
            br = new BufferedReader(new InputStreamReader(is, "UTF-8"));
            while ((is_line = br.readLine()) != null) {
                Log.d(TAG, "cmd > "+is_line);
                Result = Result+is_line+"\n";
            }

            br = new BufferedReader(new InputStreamReader(es, "UTF-8"));
            while ((es_line = br.readLine()) != null) {
//                Log.d(TAG, "cmd > "+es_line);
//                Result += es_line;
            }
        } catch (Throwable t) {
            t.printStackTrace();
        }
        return Result;
    }



}