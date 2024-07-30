//
// Created by rzx on 2024/6/27.
//
#include <sys/mman.h>
#include <asm-generic/unistd.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include "entry.h"
#include "linker.h"
#include "linker_debug.h"



int memfd_create(const char* name, unsigned int flags) {
    // Check kernel version supports memfd_create(). Some older kernels segfault executing
    // memfd_create() rather than returning ENOSYS (b/116769556).
    static constexpr int kRequiredMajor = 3;
    static constexpr int kRequiredMinor = 17;
    struct utsname uts;
    int major, minor;
    if (uname(&uts) != 0 ||
        strcmp(uts.sysname, "Linux") != 0 ||
        sscanf(uts.release, "%d.%d", &major, &minor) != 2 ||
        (major < kRequiredMajor || (major == kRequiredMajor && minor < kRequiredMinor))) {
        errno = ENOSYS;
        return -1;
    }
    return syscall(__NR_memfd_create, name, flags);
}




uint8_t * Creatememfd(int *fd, int size){

    *fd = memfd_create("test", MFD_CLOEXEC);
    if (*fd < 0) {
        perror("Creatememfd Filede: open /dev/ashmem failed");
        exit(EXIT_FAILURE);
    }

    ftruncate(*fd, size);

    uint8_t *ptr = static_cast<uint8_t *>(mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *fd,0));
    if (ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    return ptr;
}


jobject hideLoadApkModule(JNIEnv *env, mz_zip_archive& zip_archive){

    jobject currentDexLoad = nullptr;
    auto classloader = env->FindClass("java/lang/ClassLoader");
    auto getsyscl_mid = env->GetStaticMethodID(classloader, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    auto sys_classloader = env->CallStaticObjectMethod(classloader, getsyscl_mid);

    if (!sys_classloader){
        LOGE("getSystemClassLoader failed!!!");
        return nullptr;
    }
    auto in_memory_classloader = env->FindClass("dalvik/system/InMemoryDexClassLoader");
    auto initMid = env->GetMethodID(in_memory_classloader, "<init>", "([Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
    auto byte_buffer_class = env->FindClass("java/nio/ByteBuffer");


    std::unordered_map<const soinfo*, ElfReader> readers_map;
    std::vector<LoadTask*> load_tasks;
    std::vector<jobject> load_dexs;

    std::vector<ApkNativeInfo> vec_apkNativeInfo;
    int file_count = (int)mz_zip_reader_get_num_files(&zip_archive);
    for (int i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
            printf("Could not retrieve file info.\n");
            mz_zip_reader_end(&zip_archive);
            return nullptr;
        }
        if(strstr(file_stat.m_filename,APK_NATIVE_LIB)!= NULL) {
            int fd;
            uint8_t * somem_addr = Creatememfd(&fd, file_stat.m_uncomp_size);

            if (!somem_addr) {
                printf("Failed to allocate memory.\n");
                mz_zip_reader_end(&zip_archive);
                return nullptr;
            }
            if (!mz_zip_reader_extract_to_mem(&zip_archive, i, somem_addr, file_stat.m_uncomp_size, 0)) {
                printf("Failed to extract file.\n");
                mz_zip_reader_end(&zip_archive);
                return nullptr;
            }

            LoadTask* task =  new LoadTask(file_stat.m_filename, nullptr, nullptr, &readers_map);
            task->set_fd(fd, false);
            task->set_file_offset(0);
            task->set_file_size(file_stat.m_uncomp_size);
            load_tasks.push_back(task);
//            printf("Filename: \"%s\", Comment: \"%s\", Uncompressed size: %llu\n",file_stat.m_filename, file_stat.m_comment ? file_stat.m_comment : "",(mz_uint64) file_stat.m_uncomp_size);
        }
        if(strstr(file_stat.m_filename, ".dex") != NULL){
            uint8_t *file_data = (unsigned char *)malloc(file_stat.m_uncomp_size);
            if (!file_data) {
                printf("Memory allocation failed\n");
                mz_zip_reader_end(&zip_archive);
                return nullptr;
            }
            if (!mz_zip_reader_extract_to_mem(&zip_archive, i, file_data, file_stat.m_uncomp_size, 0)) {
                printf("Failed to extract file\n");
                free(file_data);
                mz_zip_reader_end(&zip_archive);
                return nullptr;
            }
            auto dex_buffer = env->NewDirectByteBuffer(file_data, file_stat.m_uncomp_size);
            load_dexs.push_back(dex_buffer);
        }
    }
    mz_zip_reader_end(&zip_archive);

    jobjectArray byteBuffers = env->NewObjectArray(load_dexs.size(), byte_buffer_class, NULL);

    for (size_t i = 0; i<load_dexs.size(); ++i) {
        jobject load_dex = load_dexs[i];
        env->SetObjectArrayElement(byteBuffers, i, load_dex);
    }
    currentDexLoad = env->NewObject(in_memory_classloader, initMid, byteBuffers, sys_classloader);
    if (currentDexLoad != nullptr ) {

        jobject g_currentDexLoad =  env->NewGlobalRef(currentDexLoad);
//        linker_protect();
        for (size_t i = 0; i<load_tasks.size(); ++i) {

            LoadTask* task = load_tasks[i];
//            soinfo* si = soinf_alloc_fun(g_default_namespace, ""/*real path*/, nullptr, 0, RTLD_GLOBAL);
            soinfo* si = new soinfo(nullptr, ""/*real path*/, nullptr, 0, RTLD_GLOBAL);
            if (si == nullptr) {
                return nullptr;
            }
            task->set_soinfo(si);

            if (!task->read()) {
//            soinfo_free(si);
                task->set_soinfo(nullptr);
                return nullptr;
            }
            const ElfReader& elf_reader = task->get_elf_reader();
            for (const ElfW(Dyn)* d = elf_reader.dynamic(); d->d_tag != DT_NULL; ++d) {
                if (d->d_tag == DT_RUNPATH) {
                    si->set_dt_runpath(elf_reader.get_string(d->d_un.d_val));
                }
                if (d->d_tag == DT_SONAME) {
                    si->set_soname(elf_reader.get_string(d->d_un.d_val));
                }
            }
        }


        for (auto&& task : load_tasks) {
            task->soload();
            call_JNI_OnLoad(task->get_soinfo(),env, g_currentDexLoad);
            task->hack();
            delete task;
        }

//        linker_unprotect();
        env->DeleteGlobalRef(g_currentDexLoad);
        return currentDexLoad;
    } else{
        return nullptr;
    }
}

uint64_t getSoInApkOffset(char * apkSource,char* libname){
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    mz_bool status = mz_zip_reader_init_file(&zip_archive, apkSource, 0);
    if (!status) {
        printf("Could not initialize zip reader.\n");
        return -1;
    }
    int file_count = (int)mz_zip_reader_get_num_files(&zip_archive);
    for (int i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
            printf("Could not retrieve file info.\n");
            mz_zip_reader_end(&zip_archive);
            return -1;
        }
        if(strstr(file_stat.m_filename,APK_NATIVE_LIB)!= NULL) {
            if(strstr(file_stat.m_filename,libname)!= NULL) {
                return file_stat.m_local_header_ofs;
            }
        }
    }
    mz_zip_reader_end(&zip_archive);
    return -1;
}


jobject FilehideLoadApkModule(JNIEnv *env, char * apkSource){


    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    mz_bool status = mz_zip_reader_init_file(&zip_archive, apkSource, 0);
    if (!status) {
        printf("Could not initialize zip reader.\n");
        return nullptr;
    }
    return hideLoadApkModule(env,zip_archive);
}


jobject memhideLoadApkModule(JNIEnv *env, unsigned char *zip_data, size_t zip_size) {


    // 使用 miniz 库解压缩内存中的 zip 文件
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
    mz_bool status = mz_zip_reader_init_mem(&zip_archive, zip_data, zip_size, 0);
    if (!status) {
        printf("Could not initialize zip reader.\n");
        return nullptr;
    }
    jobject resutl_classloader = hideLoadApkModule(env,zip_archive);
    free(zip_data); // 释放内存
    return resutl_classloader;

}

void *load_so_by_fd(int fd){
    std::unordered_map<const soinfo*, ElfReader> readers_map;
    struct stat st;
    if(fstat(fd,&st) == -1){
        perror("custom_dlopen: open failed ");
        close(fd);
        return nullptr;
    }

    LoadTask* task =  new LoadTask("file_data", nullptr, nullptr, &readers_map);
    task->set_fd(fd, false);
    task->set_file_offset(0);
    task->set_file_size(st.st_size);
    soinfo* si = new soinfo(nullptr, ""/*real path*/, nullptr, 0, RTLD_GLOBAL);
    task->set_soinfo(si);

    if (!task->read()) {
        task->set_soinfo(nullptr);
        return nullptr;
    }
    const ElfReader& elf_reader = task->get_elf_reader();
    for (const ElfW(Dyn)* d = elf_reader.dynamic(); d->d_tag != DT_NULL; ++d) {
        if (d->d_tag == DT_RUNPATH) {
            si->set_dt_runpath(elf_reader.get_string(d->d_un.d_val));
        }
        if (d->d_tag == DT_SONAME) {
            si->set_soname(elf_reader.get_string(d->d_un.d_val));
        }
    }
    task->soload();
    task->hack();
    soinfo *ret_si = task->get_soinfo();
    delete task;
    return ret_si;
}

void *custom_dlopen(const char *file_data){
    std::unordered_map<const soinfo*, ElfReader> readers_map;
    int fd = open(file_data,O_RDWR);
    struct stat st;
    if(fstat(fd,&st) == -1){
        perror("custom_dlopen: open failed ");
        close(fd);
        return nullptr;
    }

    LoadTask* task =  new LoadTask(file_data, nullptr, nullptr, &readers_map);
    task->set_fd(fd, false);
    task->set_file_offset(0);
    task->set_file_size(st.st_size);
    soinfo* si = new soinfo(nullptr, ""/*real path*/, nullptr, 0, RTLD_GLOBAL);
    task->set_soinfo(si);

    if (!task->read()) {
        task->set_soinfo(nullptr);
        return nullptr;
    }
    const ElfReader& elf_reader = task->get_elf_reader();
    for (const ElfW(Dyn)* d = elf_reader.dynamic(); d->d_tag != DT_NULL; ++d) {
        if (d->d_tag == DT_RUNPATH) {
            si->set_dt_runpath(elf_reader.get_string(d->d_un.d_val));
        }
        if (d->d_tag == DT_SONAME) {
            si->set_soname(elf_reader.get_string(d->d_un.d_val));
        }
    }
    task->soload();
    task->hack();
    soinfo *ret_si = task->get_soinfo();
    close(fd);
    delete task;
    return ret_si;
}

void *custom_dlsym(void *si,char* syn_name) {
    SymbolName symbol_name(syn_name);


    const ElfW(Sym)* sym = ((soinfo*)si)->find_symbol_by_name(symbol_name, nullptr);;
    if(sym== nullptr){
        return nullptr;
    }
    void *sym_func_addr = reinterpret_cast<void *>(sym->st_value + ((soinfo*)si)->load_bias);
    return sym_func_addr;
}