package hepta.rxposed.manager.util;


import android.content.Context;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.Charset;
import java.util.Enumeration;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import hepta.rxposed.manager.BuildConfig;
import hepta.rxposed.manager.RxposedApp;

public class InjectConfig {
        public  static String config_name;
        public  static String su_path;
        public  static String arm64_InjectSo ;
        public  static String arm32_InjectSo;
        public static String mountWorkDir;
        //注入参数
        public static String InjectArg;
        //修改selinux策略工具路径
        public static  String policy_path;
        //要修改的selinux策略
        public static  String policy_te;
        //是否隐藏注入到zygote中的so的 maps（隐藏是不需要本地路径的）
        public static  boolean hidemaps;
        //注入1号进程init，开启server注入功能，开发中
        public static  boolean injectInit;
        //注入工具的路径，app目录下，没必要移动到别的路径中
        public  static String arm64_InjectTool ;
        public  static String arm32_InjectTool;


        //原始so路径，不会删除，用作初始化和修改后备份 (app files目录下)
        public  static String appfiles_arm64_InjectSo ;
        public  static String appfiles_arm32_InjectSo;
        public final static String soName = "lib"+BuildConfig.Rxposed_Inject_So+".so";
        public final static String HostProviderName = BuildConfig.APPLICATION_ID+".Provider";
        public final static String assets_policy_tool = "assets/magiskpolicy";
        public final static String assets_policy_te = "assets/rxposed.te";
        public final static String assets_arm64_InjectTool = "assets/arm64_InjectTool";
        public final static String assets_arm32_InjectTool = "assets/armv7_InjectTool";


        static public  void Init(){
                getConfigSave();

                Context context = RxposedApp.getRxposedContext();
                int App_Uid = context.getApplicationInfo().uid;
                InjectConfig.InjectArg = App_Uid+":"+BuildConfig.APPLICATION_ID+":"+ InjectConfig.HostProviderName;;

                String AppFilePath = context.getFilesDir().getAbsolutePath()+ File.separator;
                unziplib(context.getApplicationInfo().sourceDir,AppFilePath);
                InjectConfig.policy_path = AppFilePath+InjectConfig.assets_policy_tool;
                InjectConfig.policy_te = AppFilePath+InjectConfig.assets_policy_te;

                InjectConfig.arm64_InjectTool = AppFilePath+InjectConfig.assets_arm64_InjectTool;
                InjectConfig.arm32_InjectTool = AppFilePath+InjectConfig.assets_arm32_InjectTool;

                InjectConfig.appfiles_arm64_InjectSo = AppFilePath+"lib/arm64-v8a/"+InjectConfig.soName;
                InjectConfig.appfiles_arm32_InjectSo = AppFilePath+"lib/armeabi-v7a/"+InjectConfig.soName;

                try {
                        Runtime.getRuntime().exec("chmod +x "+InjectConfig.arm64_InjectTool);
                        Runtime.getRuntime().exec("chmod +x "+InjectConfig.arm32_InjectTool);
                        Runtime.getRuntime().exec("chmod +x "+InjectConfig.policy_path);

                } catch (IOException e) {
                        throw new RuntimeException(e);
                }
        }

        public static void mount_libdir(String mountWorkDir,String srcLibDir) {

                InjectTool.rootRun("mkdir -p "+mountWorkDir);
                InjectTool.rootRun("mount -t tmpfs tmpfs "+mountWorkDir);
                InjectTool.rootRun("chown -R system:system "+mountWorkDir);
                InjectTool.rootRun("chcon -R u:object_r:system_file:s0 "+mountWorkDir);
                InjectTool.rootRun("cp /data/data/hepta.rxposed.manager/files/lib/arm64-v8a "+mountWorkDir+"/lib64  -R");
                InjectTool.rootRun("cp /data/data/hepta.rxposed.manager/files/lib/armeabi-v7a "+mountWorkDir+"/lib  -R");
                InjectTool.rootRun("chcon -R u:object_r:system_lib_file:s0 "+mountWorkDir+"/lib");
                InjectTool.rootRun("chcon -R u:object_r:system_lib_file:s0 "+mountWorkDir+"/lib64");
                InjectTool.rootRun("chmod 0644 "+mountWorkDir+"/lib64/*");
                InjectTool.rootRun("chmod 0644 "+mountWorkDir+"/lib/*");

        }

        private static void getConfigSave() {

                InjectConfig.config_name = MmkvManager.INSTANCE.getString("config_name","default");
                InjectConfig.su_path = MmkvManager.INSTANCE.getString("supath","su");
                InjectConfig.mountWorkDir = MmkvManager.INSTANCE.getString("mountWorkDir","/apex/com.android.i18nrxp");
//                InjectConfig.hidemaps = MmkvManager.INSTANCE.getBoolean("hidemaps",true);
                InjectConfig.hidemaps = MmkvManager.INSTANCE.getBoolean("hidemaps",false);
                InjectConfig.injectInit = MmkvManager.INSTANCE.getBoolean("injectInit",false);

                InjectConfig.arm32_InjectSo = mountWorkDir+"/lib/"+InjectConfig.soName;
                InjectConfig.arm64_InjectSo = mountWorkDir+"/lib64/"+InjectConfig.soName;
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

}