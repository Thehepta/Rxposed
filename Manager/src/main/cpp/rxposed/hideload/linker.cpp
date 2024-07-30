//
// Created by chic on 2023/11/26.
//

#include <cstdio>
#include <locale>
#include <linux/ashmem.h>
#include "linker.h"
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <vector>
#include "elf_symbol_resolver.h"
#include "linker_debug.h"
#include <sys/syscall.h>
#include <sys/utsname.h>
#include "jni_hook.h"


soinfo* (*solist_get_head)() = (soinfo* (*)()) linkerResolveElfInternalSymbol(
        get_android_linker_path(), "__dl__Z15solist_get_headv");

soinfo* (*solist_get_somain)() = (soinfo* (*)()) linkerResolveElfInternalSymbol(
        get_android_linker_path(), "__dl__Z17solist_get_somainv");

char* (*soinfo_get_soname)(soinfo*) = (char* (*)(soinfo*)) linkerResolveElfInternalSymbol(
        get_android_linker_path(), "__dl__ZNK6soinfo10get_sonameEv");




static inline uintptr_t untag_address(uintptr_t p) {
#if defined(__aarch64__)
    return p & ((1ULL << 56) - 1);
#else
    return p;
#endif
}

template <typename T>
static inline T* untag_address(T* p) {
    return reinterpret_cast<T*>(untag_address(reinterpret_cast<uintptr_t>(p)));
}

//通过动态计算结构内部成员的位置，循环找到soinfo 内部的next的位置，然后通过next进行遍历
soinfo* find_all_library_byname(const char* soname){
    std::vector<void *> linker_solist;

    static uintptr_t *solist_head = NULL;
    if (!solist_head)
        solist_head = (uintptr_t *)solist_get_head();


    static uintptr_t somain = 0;

    if (!somain)
        somain = (uintptr_t)solist_get_somain();

    // Generate the name for an offset.
#define PARAM_OFFSET(type_, member_) __##type_##__##member_##__offset_
#define STRUCT_OFFSET PARAM_OFFSET
    int STRUCT_OFFSET(solist, next) = 0;
    for (size_t i = 0; i < 1024 / sizeof(void *); i++) {
        if (*(uintptr_t *)((uintptr_t)solist_head + i * sizeof(void *)) == somain) {
            STRUCT_OFFSET(solist, next) = i * sizeof(void *);
            break;
        }
    }

    linker_solist.push_back(solist_head);

    uintptr_t sonext = 0;
    sonext = *(uintptr_t *)((uintptr_t)solist_head + STRUCT_OFFSET(solist, next));
    while (sonext) {
        linker_solist.push_back((void *)sonext);
        sonext = *(uintptr_t *)((uintptr_t)sonext + STRUCT_OFFSET(solist, next));
        if(sonext == 0){
            continue;
        }
        char* ret_name = soinfo_get_soname(reinterpret_cast<soinfo *>(sonext));
        LOGE("get_soname : %s",ret_name);
        LOGE("get_soname == null so->realpath: %s",((soinfo*)sonext)->get_realpath());

    }

    return nullptr;
}

soinfo* find_system_library_byname(const char* soname) {

    for (soinfo* si = solist_get_head(); si != nullptr; si = si->next) {
        char* ret_name = soinfo_get_soname(si);
        if(ret_name!= nullptr){
            if(0 == strncmp(ret_name,soname, strlen(soname))) {
                return si;
            }
        }

    }
    return nullptr;
}

soinfo* find_containing_library(const void* p) {

//    static soinfo* (*solist_get_head)() = NULL;
//    if (!solist_get_head)
//        solist_get_head = (soinfo* (*)())linkerResolveElfInternalSymbol(get_android_linker_path(), "__dl__Z15solist_get_headv");

//    static soinfo* (*solist_get_somain)() = NULL;
//    if (!solist_get_somain)
//        solist_get_somain = (soinfo* (*)())linkerResolveElfInternalSymbol(get_android_linker_path(), "__dl__Z17solist_get_somainv");

    ElfW(Addr) address = reinterpret_cast<ElfW(Addr)>(untag_address(p));
    for (soinfo* si = solist_get_head(); si != nullptr; si = si->next) {
        if (address < si->base || address - si->base >= si->size) {
            continue;
        }
        ElfW(Addr) vaddr = address - si->load_bias;
        for (size_t i = 0; i != si->phnum; ++i) {
            const ElfW(Phdr)* phdr = &si->phdr[i];
            if (phdr->p_type != PT_LOAD) {
                continue;
            }
            if (vaddr >= phdr->p_vaddr && vaddr < phdr->p_vaddr + phdr->p_memsz) {
                return si;
            }
        }
    }
    return nullptr;
}






//只支持单so，删除互相依赖处理
soinfo* find_library(const char *soname) {

//    LoadTask* find_soinfo = nullptr;
//    for (auto&& task : load_tasks) {
//        if(0 == strncmp(task->get_soinfo()->get_soname(),soname, strlen(soname))){
//            find_soinfo = task;
//        }
//    }
//    if(find_soinfo != nullptr) {
//        if(find_soinfo->get_soinfo()->is_linked()){
//            find_soinfo->soload(load_tasks, nullptr);
//        }
//        return find_soinfo->get_soinfo();
//
//    }else{
    return find_system_library_byname(soname);
//    }

}

const char* fix_dt_needed(const char* dt_needed, const char* sopath __unused) {
#if !defined(__LP64__)
    int app_target_api_level = android_get_application_target_sdk_version();
    if (app_target_api_level < 23) {
        const char* bname = basename(dt_needed);
        if (bname != dt_needed) {
            DL_WARN_documented_change(23,
                                      "invalid-dt_needed-entries-enforced-for-api-level-23",
                                      "library \"%s\" has invalid DT_NEEDED entry \"%s\"",
                                      sopath, dt_needed, app_target_api_level);
//      add_dlwarning(sopath, "invalid DT_NEEDED entry",  dt_needed);
        }

        return bname;
    }
#endif
    return dt_needed;
}


void LoadTask::soload( ) {

    SymbolLookupList lookup_list;

    for (const ElfW(Dyn)* d = get_elf_reader().dynamic(); d->d_tag != DT_NULL; ++d) {
        if (d->d_tag == DT_NEEDED) {
            const char* name = fix_dt_needed(get_elf_reader().get_string(d->d_un.d_val), get_elf_reader().name());
            LOGE("NEED name: %s",name);
            soinfo* system_si = find_library(name);
            soinfo *custom_si  = new soinfo();
            custom_si->set_soname(name);
            custom_si->transform(system_si);
            SymbolLookupLib SyLib = custom_si->get_lookup_lib(true);
            lookup_list.addSymbolLib(SyLib);
        }
    }


    address_space_params  default_params;
    load(&default_params);
    get_soinfo()->prelink_image();
    get_soinfo()->set_dt_flags_1(get_soinfo()->get_dt_flags_1() | DF_1_GLOBAL);
    lookup_list.addSymbolLib(get_soinfo()->get_lookup_lib(false));
    get_soinfo()->link_image(lookup_list);
    get_soinfo()->set_linked();
    get_soinfo()->call_constructors();

    auto it =     lookup_list.getVectorSymLib().begin();
    auto end = lookup_list.getVectorSymLib().end();
    std::vector<SymbolLookupLib>::iterator  lib ;
    while (true) {
        if (it == end) return;
        lib = it++;
//        if(lib->system_sonifo){
            delete lib->si_ ;
//        }
    }
}

void call_JNI_OnLoad(soinfo *si, JNIEnv *pEnv, jobject g_currentDexLoad) {


    SymbolName symbol_JNI_OnLoad("JNI_OnLoad");
    const ElfW(Sym)* sym = si->find_symbol_by_name(symbol_JNI_OnLoad, nullptr);
    if(sym== nullptr){
        return;
    }
    int(*JNI_OnLoadFn)(JavaVM*, void*) = ( int(*)(JavaVM*, void*))(sym->st_value + si->load_bias);

    JavaVM *vm;
    pEnv->GetJavaVM(reinterpret_cast<JavaVM **>(&vm));
    void * linkerJniInvokeInterface = jni_hook_init(vm,g_currentDexLoad);
    JNI_OnLoadFn(static_cast<JavaVM *>(linkerJniInvokeInterface), nullptr);
}

void LoadTask::hack() {
    ElfReader & elfReader = get_elf_reader();
    elfReader.loaded_phdr();
}







uintptr_t ToPathLoadSoGetSymbolAddr(char * sopath,char *topath,char*call_arg){

}

uintptr_t ToMemLoadSoGetSymbolAddr(char * sopath,char*call_arg){

}
















































