//
// Created by chic on 2023/11/25.
//

#pragma once

#include <dlfcn.h>
#include "miniz.h"
#include <inttypes.h>
#include <link.h>
#include <sys/stat.h>
#include <unistd.h>
#include "linker_phdr.h"
#include "linker_soinfo.h"
#include<unordered_map>
#include <jni.h>


#if defined(__LP64__)
//#define ELFW(what) ELF64_ ## what
#define APK_NATIVE_LIB "lib/arm64-v8a"
#else
//#define ELFW(what) ELF32_ ## what
#define APK_NATIVE_LIB "lib/armeabi-v7a"
#endif

struct ApkNativeInfo {
    mz_zip_archive_file_stat file_stat;
    uint8_t * somem_addr;
    std::string libname;
    int fd;
};


soinfo* find_all_library_byname(const char* soname) ;
soinfo* find_system_library_byname(const char* soname);





#define SUPPORTED_DT_FLAGS_1 (DF_1_NOW | DF_1_GLOBAL | DF_1_NODELETE | DF_1_PIE)





#define PAGE_START(x) ((x) & PAGE_MASK)

// Returns the offset of address 'x' in its page.
#define PAGE_OFFSET(x) ((x) & ~PAGE_MASK)

// Returns the address of the next page after address 'x', unless 'x' is
// itself at the start of a page.
#define PAGE_END(x) PAGE_START((x) + (PAGE_SIZE-1))


static constexpr uintptr_t align_up(uintptr_t p, size_t align) {
      return (p + align - 1) & ~(align - 1);
}

template <typename T>
static inline T* align_up(T* p, size_t align) {
      return reinterpret_cast<T*>(align_up(reinterpret_cast<uintptr_t>(p), align));
}

static constexpr uintptr_t align_down(uintptr_t p, size_t align) {
      return p & ~(align - 1);
}

template <typename T>
static inline T* align_down(T* p, size_t align) {
      return reinterpret_cast<T*>(align_down(reinterpret_cast<uintptr_t>(p), align));
}


constexpr unsigned kLibraryAlignmentBits = 18;
constexpr size_t kLibraryAlignment = 1UL << kLibraryAlignmentBits;





class ElfReader;


class LoadTask {
public:



    LoadTask(const char* name,
             soinfo* needed_by,
             android_namespace_t* start_from,
             std::unordered_map<const soinfo*, ElfReader>* readers_map)
            : name_(name), needed_by_(needed_by), si_(nullptr),
              fd_(-1), close_fd_(false), file_offset_(0), elf_readers_map_(readers_map),
              is_dt_needed_(false), start_from_(start_from) {}

    // returns the namespace from where we need to start loading this.
    const android_namespace_t* get_start_from() const {
        return start_from_;
    }

    std::unordered_map<const soinfo*, ElfReader>* get_readers_map() {
        return elf_readers_map_;
    }

    soinfo* get_soinfo() const {
        return si_;
    }

    void set_file_offset(off64_t offset) {
        file_offset_ = offset;
    }

    off64_t get_file_offset() const {
        return file_offset_;
    }

    off64_t get_file_size() const {
        return file_size_;
    }
    void set_file_size(off64_t offset) {
        file_size_ = offset;
    }

    void set_soinfo(soinfo* si) {
        si_ = si;
    }

    bool read() {
        //    task->add_elf_readers_map()

        ElfReader &elf_reader = get_elf_reader();
        return elf_reader.Read("", fd_, file_offset_, file_size_);

    }


    bool load(address_space_params* address_space) {
        ElfReader& elf_reader = get_elf_reader();
        if (!elf_reader.Load(address_space)) {
            return false;
        }
        si_->base = elf_reader.load_start();
        si_->size = elf_reader.load_size();
        si_->set_mapped_by_caller(elf_reader.is_mapped_by_caller());
        si_->load_bias = elf_reader.load_bias();
        si_->phnum = elf_reader.phdr_count();
        si_->phdr = elf_reader.loaded_phdr();
        return true;
    }


    void set_fd(int fd, bool assume_ownership) {
        if (fd_ != -1 && close_fd_) {
            close(fd_);
        }
        fd_ = fd;
        close_fd_ = assume_ownership;
    }

    void add_elf_readers_map(const soinfo* si, ElfReader elfReader){
        elf_readers_map_->emplace(si,elfReader);
    }

    ElfReader& get_elf_reader() {
        return (*elf_readers_map_)[si_];
    }


    ~LoadTask() {
        if (fd_ != -1 && close_fd_) {
            close(fd_);
        }
    }


    soinfo* get_needed_by() const {
        return needed_by_;
    }

    void soload();

    void hack();

private:

    std::string name_;
    soinfo* needed_by_;
    soinfo* si_;
    const android_dlextinfo* extinfo_;
    int fd_;
    bool close_fd_;
    off64_t file_offset_;
    off64_t file_size_;
    std::unordered_map<const soinfo*, ElfReader>* elf_readers_map_;
    // TODO(dimitry): needed by workaround for http://b/26394120 (the grey-list)
    bool is_dt_needed_;

    // END OF WORKAROUND
    const android_namespace_t* const start_from_;

//    DISALLOW_IMPLICIT_CONSTRUCTORS(LoadTask);

};

//struct linker_JNIEnv:public _JNIEnv{
//
//    linker_JNIEnv(JNIEnv* env) {
//        functions = env->functions;
//    }
//
//    jclass FindClass(const char* name)
//    { return functions->FindClass(this, name); }
//    const char* GetStringUTFChars(jstring string, jboolean* isCopy)
//    {
//        LOGE("linker_JavaVM GetStringUTFChars");
//        return functions->GetStringUTFChars(this, string, isCopy);
//    }
//};


struct linker_JavaVM:public _JavaVM{

    linker_JavaVM(JavaVM *pVm) {
        functions =pVm->functions;
    }

    jint GetEnv(void ** env, jint version){

        int re =functions->GetEnv(this, env, version);
//        linker_JNIEnv *linkerJniEnv = new linker_JNIEnv((JNIEnv* )*env);
//        *env = linkerJniEnv;
//        LOGE("linker_JavaVM GetEnv");
        return re;
    }

//    { return functions->GetEnv(this, env, version); }

};

void call_JNI_OnLoad(soinfo *si, JNIEnv *pEnv, jobject g_currentDexLoad);




































