## introduce
这是一个android 平台的ptrace注入工具，添加了注入的so maphide的功能，隐藏是一方面，另外是可以直接绕过文件selinux权限有问题的检查



## example
```
/data/user/0/hepta.rxposed.manager/files/assets/arm64_InjectTool -p 4903 -hidemaps 1 -so /data/user/0/hepta.rxposed.manager/files/arm64_libnativeloader.so -symbols _Z9Inject_ProcessPKc hepta.rxposed.manager.Provider;com.hep>                                                     <
/data/user/0/hepta.rxposed.manager/files/assets/arm64_InjectTool -p 1 -hidemaps 1 -so /data/user/0/hepta.rxposed.manager/files/lib/arm64-v8a/libtest.so                                    <


```