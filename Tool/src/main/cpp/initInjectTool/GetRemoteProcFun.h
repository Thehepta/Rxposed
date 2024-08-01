//
// Created by rzx on 2024/8/1.
//

#pragma once


#include <sys/types.h>
#include <sys/mman.h>
#include <stdio.h>
#include <sys/system_properties.h>
#include <stdlib.h>
#include <string.h>
#include "elf_symbol_resolver.h"

// 系统lib路径
struct process_libs{
    const char *libc_path;
    const char *linker_path;
    const char *libdl_path;
} process_libs = {"","",""};



/**
 * @brief 处理各架构预定义的库文件
 */


__unused __attribute__((constructor(101)))
void handle_libs(){ // __attribute__((constructor))修饰 最先执行

// 系统lib路径
    process_libs.libc_path = "/system/lib64/bootstrap/libc.so";
    process_libs.linker_path = "/system/bin/linker64";
    process_libs.libdl_path = "/system/lib64/bootstrap/libdl.so";

    printf("[+] libc_path is %s\n", process_libs.libc_path);
//    printf("[+] linker_path is %s\n", process_libs.linker_path);
    printf("[+] libdl_path is %s\n", process_libs.libdl_path);
    printf("[+] system libs is OK\n");
}

/**
 * @brief 获取mmap函数在远程进程中的地址
 *
 * @param pid pid表示远程进程的ID
 * @return void* mmap函数在远程进程中的地址
 */

void *get_mmap_address(pid_t pid){

    return ResolveElfInternalSymbol(pid, process_libs.libc_path, "mmap");
//    return get_remote_func_addr(pid, process_libs.libc_path, (void *)mmap);
}


/**
 * @brief 获取dlopen函数在远程进程中的地址
 * @param pid pid表示远程进程的ID
 * @return void* dlopen函数在远程进程中的地址
 */
void *get_dlopen_address(pid_t pid) {
    void *dlopen_addr;
    char sdk_ver[32];
    memset(sdk_ver, 0, sizeof(sdk_ver));
    __system_property_get("ro.build.version.sdk", sdk_ver);

//    printf("[+] linker_path value:%s\n",process_libs.linker_path);

    dlopen_addr = ResolveElfInternalSymbol(pid, process_libs.libdl_path, "dlopen");
//        dlopen_addr = get_remote_func_addr(pid, process_libs.libdl_path, (void *) dlopen);

    printf("[+] dlopen RemoteFuncAddr:0x%lx\n", (uintptr_t) dlopen_addr);
    return dlopen_addr;
}


/**
 * @brief 获取dlsym函数在远程进程中的地址
 * @param pid pid表示远程进程的ID
 * @return void* dlsym函数在远程进程中的地址
 */
void *get_dlsym_address(pid_t pid) {
    void *dlsym_addr;
    char sdk_ver[32];
    memset(sdk_ver, 0, sizeof(sdk_ver));
    __system_property_get("ro.build.version.sdk", sdk_ver);


    dlsym_addr = ResolveElfInternalSymbol(pid, process_libs.libdl_path, "dlsym");
//        dlsym_addr = get_remote_func_addr(pid, process_libs.libdl_path, (void *) dlsym);

    printf("[+] dlsym RemoteFuncAddr:0x%lx\n", (uintptr_t) dlsym_addr);
    return dlsym_addr;
}




/**
 * @brief 获取dlclose函数在远程进程中的地址
 * @param pid pid表示远程进程的ID
 * @return void* dlclose函数在远程进程中的地址
 */
void *get_dlclose_address(pid_t pid) {
    void *dlclose_addr;
    char sdk_ver[32];
    memset(sdk_ver, 0, sizeof(sdk_ver));
    __system_property_get("ro.build.version.sdk", sdk_ver);

    dlclose_addr = ResolveElfInternalSymbol(pid, process_libs.libdl_path,  "dlclose");
//        dlclose_addr = get_remote_func_addr(pid, process_libs.libdl_path, (void *) dlclose);

    printf("[+] dlclose RemoteFuncAddr:0x%lx\n", (uintptr_t) dlclose_addr);
    return dlclose_addr;
}


/**
 * @brief 获取dlerror函数在远程进程中的地址
 * @param pid pid表示远程进程的ID
 * @return void* dlerror函数在远程进程中的地址
 */
void *get_dlerror_address(pid_t pid) {
    void *dlerror_addr;
    char sdk_ver[32];
    memset(sdk_ver, 0, sizeof(sdk_ver));
    __system_property_get("ro.build.version.sdk", sdk_ver);


    dlerror_addr = ResolveElfInternalSymbol(pid, process_libs.libdl_path, "dlerror");
//        dlerror_addr = get_remote_func_addr(pid, process_libs.libdl_path, (void *) dlerror);

    printf("[+] dlerror RemoteFuncAddr:0x%lx\n", (uintptr_t) dlerror_addr);
    return dlerror_addr;
}