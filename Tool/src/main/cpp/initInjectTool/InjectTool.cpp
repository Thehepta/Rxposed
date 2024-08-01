// user lib
#include "PtraceInject.h"

// 由于Inject是命令行工具 因此 替换全部的LOG为printf 以方便观察注入流程
int main(int argc, char *argv[]) {

    printf("[+] Start Inject\n");


//    dumpsys package  hepta.rxposed.manager  | grep "userId=" | awk -F'=' '{print $2}'  //返回uid
    // 开始注入
    // /arm64_InjectTool -p 1 -so /data/user/0/hepta.rxposed.manager/files/lib/arm64-v8a/libtest.so                                     <

    /** 以下是Inject命令行工具的参数
     ** 部分参数选填
     * -p 目标进程pid <-- 不传pid就传包名
     * ---- // 由于 -f 参数需要创建中间文件 因此 请务必在Inject工具目录执行该工具
     * ---- // 即 /data/local/tmp/Inject -f -n XXX <-- 错误
     * ---- // 即 cd /data/local/tmp && ./Inject -f -n XXX <-- 正确
     * -so 注入的so路径 <-- 必填 本来就是so注入工具 只能是绝对路径!!
     * -symbols 指定启用so中的某功能 <-- 选填 可以指定so中某功能的symbols 也可以通过__attribute__((constructor))让so注入后自行初始化
     */
    if (init_inject(argc, argv) == 0){
        printf("[+] Finish Inject\n");
    } else {
        printf("[-] Inject Erro\n");
    }
    return 0;
}
