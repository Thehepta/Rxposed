package hepta.rxposed.manager.util;

import hepta.rxposed.manager.RxposedApp;

public class Consts {

    public static final int START_FRAGMENT_MODULE = 1 ;
    public static final int START_FRAGMENT_FRAMEWORK = 2 ;

    public static final int MODULE_TYPE_XPOSED = 1 ;
    public static final int MODULE_TYPE_RXPOSED = 2 ;
    public static final int MODULE_TYPE_DEPEND = 3;
    public static final int MODULE_TYPE_APP = 4;

    public static final int MOUNT_TMP = 1;
    public static final int HIDE_MAPS = 2;


    public static final String INJECT_FRAGMENT_ARGE = "injectProcess";
    public static final String INJECTLIST_FRAGMENT_ARGE = "injectList";

    public static final String LOG_FILE_NAME = RxposedApp.getInstance().getFilesDir()+"/inject_log.txt";


}
