package hepta.rxposed.manager.util;


import hepta.rxposed.manager.BuildConfig;

public class InjectConfig {
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
        public static String soName;
        public final static String HostProviderName = BuildConfig.APPLICATION_ID+".Provider";
        public final static String assets_policy_tool = "assets/magiskpolicy";
        public final static String assets_policy_te = "assets/rxposed.te";
        public final static String assets_arm64_InjectTool = "assets/arm64_InjectTool";
        public final static String assets_arm32_InjectTool = "assets/armv7_InjectTool";
}