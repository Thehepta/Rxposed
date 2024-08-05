package hepta.rxposed.manager.util

import android.content.pm.PackageManager
import android.util.Log
import com.google.gson.Gson
import com.tencent.mmkv.MMKV
import hepta.rxposed.manager.RxposedApp


object MmkvManager {

    const val ID_MAIN = "MAIN"
    const val ID_SERVER_CONFIG = "SERVER_CONFIG"
    const val ID_SERVER_RAW = "SERVER_RAW"
    const val ID_SERVER_AFF = "SERVER_AFF"
    const val ID_SETTING = "SETTING"
    const val KEY_SELECTED_SERVER = "SELECTED_SERVER"
    const val KEY_ANG_CONFIGS = "ANG_CONFIGS"
    const val KEY_MOD_CONFIGS = "MOD_CONFIGS"
    const val KEY_PLUG_CONFIGS = "PLUG_CONFIGS"

    const val KEY_APP_ALLWO_NONE = 0
    const val KEY_APP_ADD_ALLOW = 1
    const val KEY_APP_ADD_ALLOW_LIST = "KEY_APP_ADD_ALLOW_LIST"

    const val KEY_APP_ADD_DIS_ALLOW = 2
    const val KEY_APP_ADD_DIS_ALLOW_LIST = "KEY_APP_ADD_DIS_ALLOW_LIST"


    private val settingStorage by lazy { MMKV.mmkvWithID(ID_MAIN, MMKV.MULTI_PROCESS_MODE) }
    private val serverStorage by lazy { MMKV.mmkvWithID(ID_SERVER_CONFIG, MMKV.MULTI_PROCESS_MODE) }
    private val serverAffStorage by lazy { MMKV.mmkvWithID(ID_SERVER_AFF, MMKV.MULTI_PROCESS_MODE) }
    private val appListStorage by lazy { MMKV.mmkvWithID(ID_SETTING, MMKV.MULTI_PROCESS_MODE) }

    fun putInt(key:String,value:Int){
        settingStorage.putInt(key,value)
    }
    fun putString(key:String,value:String){
        settingStorage.putString(key,value)
    }
    fun getString(key:String,defalutValue:String): String? {
        return settingStorage.getString(key,defalutValue)
    }
    fun getBoolean(key:String,value:Boolean):Boolean{
       return settingStorage.getBoolean(key,value)
    }
    fun putBoolean(key:String,value:Boolean){
        settingStorage.putBoolean(key,value)
    }


    fun getPLugList() : MutableMap<String, Boolean> {
        val json = appListStorage?.decodeString(KEY_PLUG_CONFIGS)
        return if (json.isNullOrBlank()) {
            InitPlugList()
        } else {
            val tmp = Gson().fromJson(json, Map::class.java)
            return tmp as MutableMap<String, Boolean>
        }
    }

    fun InitPlugList() :MutableMap<String,Boolean> {
        val list = mutableMapOf<String,Boolean>()
        for (pkg in RxposedApp.getInstance().packageManager.getInstalledPackages(PackageManager.GET_META_DATA)) {
            val app = pkg.applicationInfo
            if (app.metaData != null && app.metaData.containsKey("rxposed_support")) {
                list.put(app.packageName,false)
            }
        }
        return list;
    }
    fun getModuleStatus(pkgName:String):Boolean{
        val tmp = getModuleList()
        return tmp.getOrDefault(pkgName,false)
    }
    fun setModuleStatus(pkgName:String,boolean: Boolean) {
        val tmp = getModuleList()
        tmp.replace(pkgName,boolean)
        setModuleList(tmp)
    }

    fun getModuleList() : MutableMap<String, Boolean> {
        val json = appListStorage?.decodeString(KEY_MOD_CONFIGS)
        return if (json.isNullOrBlank()) {
            InitModuleList()
        } else {
            val tmp = Gson().fromJson(json, Map::class.java)
            return tmp as MutableMap<String, Boolean>
        }
    }

    fun setModuleList(list :MutableMap<String,Boolean>) {
        val json = Gson().toJson(list)
        appListStorage?.encode(KEY_MOD_CONFIGS,json)
    }

    fun updataModuleList() :MutableMap<String, Boolean>{
        val tmp = mutableMapOf<String,Boolean>();
        val old = getModuleList()
        for (pkg in RxposedApp.getInstance().packageManager.getInstalledPackages(PackageManager.GET_META_DATA)) {
            val app = pkg.applicationInfo
            if (app.metaData != null && app.metaData.containsKey("rxmodule")) {
                Log.e("Rzx","rxmodule:"+app.packageName)
                if(old.containsKey(app.packageName)){
                    tmp.put(app.packageName,old.getValue(app.packageName))
                }else{
                    tmp.put(app.packageName,false)
                }
            }
        }
        setModuleList(tmp)
        return tmp
    }

    fun InitModuleList() :MutableMap<String,Boolean> {
        val list = mutableMapOf<String,Boolean>()
        for (pkg in RxposedApp.getInstance().packageManager.getInstalledPackages(PackageManager.GET_META_DATA)) {
            val app = pkg.applicationInfo
            if (app.metaData != null && app.metaData.containsKey("rxmodule")) {
                list.put(app.packageName,false)
            }
        }
        return list;
    }

    fun setAppEnableModuleStatus(appName:String, mooduleName:String, boolean: Boolean){
        var modules = appListStorage.decodeStringSet(appName)
        if( modules== null)
        {
            modules = mutableSetOf<String>()
        }
        if(boolean){
            modules.add(mooduleName)
        }else{
            modules.remove(mooduleName)
        }
        if(modules.size == 0){

        }
        appListStorage.encode(appName,modules)
    }

    // appName = com.android.camera
    fun getAppEnableModuleStatus(appName:String, ModuleName:String):Boolean{
         var modules = appListStorage.decodeStringSet(appName)
        if(modules == null){
            return false
        }
        return modules.contains(ModuleName)
    }

    fun getAppEnableModuleList(appName:String): MutableList<String>? {
        val module_map: Map<String, Boolean> = getModuleList()
        var enableModuleList :MutableList<String> = mutableListOf()
        var modules = appListStorage.decodeStringSet(appName)
        if (modules != null) {
            for(module in modules){
                if(module_map.get(module) == true){
                    enableModuleList.add(module)
                }
            }
        }
        return enableModuleList
    }

}