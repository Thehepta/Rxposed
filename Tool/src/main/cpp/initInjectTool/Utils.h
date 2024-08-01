// system lib
#pragma once
#include <asm/ptrace.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <dirent.h>
#include <elf.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <asm/unistd.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/system_properties.h>


//不能用文件写入的方式，经过测试会有延迟问题，设置了以后代码会立即执行，这个时候，selinux策略还没有生效导致注入失败
int get_selinux_status(){
    FILE *fp;
    char buffer[1024];
    // 执行shell命令并获取输出结果
    fp = popen("getenforce", "r");
    if (fp == NULL) {
        printf("Failed to run command\n");
        return 1;
    }

    // 读取输出结果并打印
    fgets(buffer, sizeof(buffer), fp);
    pclose(fp);
    if(strncmp(buffer,"Enforcing", strlen("Enforcing")) == 0){
        return 1;
    } else{
        return 0;
    }
}

/**
 *
 * @param value SELinux的状态值 0为宽容模式Permissive 1为严格模式Enforcing
 * @return
 */
bool set_selinux_state(int value) {
    bool succ = true;
    char cmd[20];
    snprintf(cmd, 20, "setenforce %d", value);
    system(cmd);
    printf("[+] setlinux cmd :%s\n",cmd);
    return succ;
}

/**
 * @brief Get the pid by pkg_name
 * 成功返回 true
 * 失败返回 false
 *
 * @param pid
 * @param task_name
 * @return true
 * @return false
 */
bool get_pid_by_name(pid_t *pid, char *task_name){
    DIR *dir;
    struct dirent *ptr;
    FILE *fp;
    char filepath[150];
    char cur_task_name[1024];
    char buf[1024];

    dir = opendir("/proc");
    if (NULL != dir){
        while ((ptr = readdir(dir)) != NULL){ //循环读取/proc下的每一个文件/文件夹
            //如果读取到的是"."或者".."则跳过，读取到的不是文件夹名字也跳过
            if ((strcmp(ptr->d_name, ".") == 0) || (strcmp(ptr->d_name, "..") == 0))
                continue;
            if (DT_DIR != ptr->d_type)
                continue;

            sprintf(filepath, "/proc/%s/cmdline", ptr->d_name); //生成要读取的文件的路径
            fp = fopen(filepath, "r");
            if (NULL != fp){
                if (fgets(buf, 1024 - 1, fp) == NULL){
                    fclose(fp);
                    continue;
                }
                sscanf(buf, "%s", cur_task_name);
                //如果文件内容满足要求则打印路径的名字（即进程的PID）
                if (strstr(task_name, cur_task_name)){
                    *pid = atoi(ptr->d_name);
                    return true;
                }
                fclose(fp);
            }
        }
        closedir(dir);
    }
    return false;
}

/**
 * @brief 在指定进程中搜索对应模块的基址
 *
 * @param pid pid表示远程进程的ID 若为-1表示自身进程
 * @param ModuleName ModuleName表示要搜索的模块的名称
 * @return void* 返回0表示获取模块基址失败，返回非0为要搜索的模块基址
 */
void *get_module_base_addr(pid_t pid, const char *ModuleName){
    FILE *fp = NULL;
    long ModuleBaseAddr = 0;
    char szFileName[50] = {0};
    char szMapFileLine[1024] = {0};

    // 读取"/proc/pid/maps"可以获得该进程加载的模块
    if (pid < 0){
        //  枚举自身进程模块
        snprintf(szFileName, sizeof(szFileName), "/proc/self/maps");
    } else {
        snprintf(szFileName, sizeof(szFileName), "/proc/%d/maps", pid);
    }

    fp = fopen(szFileName, "r");

    if (fp != NULL){
        while (fgets(szMapFileLine, sizeof(szMapFileLine), fp)){
            if (strstr(szMapFileLine, ModuleName)){
                char *Addr = strtok(szMapFileLine, "-");
                ModuleBaseAddr = strtoul(Addr, NULL, 16);

                if (ModuleBaseAddr == 0x8000)
                    ModuleBaseAddr = 0;

                break;
            }
        }
        fclose(fp);
    }

    return (void *)ModuleBaseAddr;
}

/**
 * @brief 获取远程进程与本进程都加载的模块中函数的地址
 *
 * @param pid pid表示远程进程的ID
 * @param ModuleName ModuleName表示模块名称
 * @param LocalFuncAddr LocalFuncAddr表示本地进程中该函数的地址
 * @return void* 返回远程进程中对应函数的地址
 */
void *get_remote_func_addr(pid_t pid, const char *ModuleName, void *LocalFuncAddr){
    void *LocalModuleAddr, *RemoteModuleAddr, *RemoteFuncAddr;
    //获取本地某个模块的起始地址
    LocalModuleAddr = get_module_base_addr(-1, ModuleName);
    //获取远程pid的某个模块的起始地址
    RemoteModuleAddr = get_module_base_addr(pid, ModuleName);
    // local_addr - local_handle的值为指定函数(如mmap)在该模块中的偏移量，然后再加上remote_handle，结果就为指定函数在目标进程的虚拟地址
    RemoteFuncAddr = (void *)((uintptr_t)LocalFuncAddr - (uintptr_t)LocalModuleAddr + (uintptr_t)RemoteModuleAddr);

    printf("[+][get_remote_func_addr] lmod=0x%lX, rmod=0x%lX, lfunc=0x%lX, rfunc=0x%lX\n", LocalModuleAddr, RemoteModuleAddr, LocalFuncAddr, RemoteFuncAddr);
    return RemoteFuncAddr;
}

