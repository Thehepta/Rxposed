//
// Created by chic on 2024/3/22.
//
#pragma once
#include "linker_soinfo.h"

struct soinfo_12l_transform;
void android_12l_soinfo_transform(soinfo*, soinfo_12l_transform *);


template<typename T, typename Allocator>
class LinkedList_12l {
public:
private:
    LinkedListEntry<T>* head_;
    LinkedListEntry<T>* tail_;
};


typedef LinkedList_12l<soinfo, SoinfoListAllocator> soinfo_list_t_12l;
typedef LinkedList_12l<android_namespace_t, NamespaceListAllocator> android_namespace_list_t_12l;



struct soinfo_12l_transform{

#if defined(__work_around_b_24465209__)
    private:
  char old_name_[SOINFO_NAME_LEN];
#endif
public:
    const ElfW(Phdr)* phdr;
    size_t phnum;
#if defined(__work_around_b_24465209__)
    ElfW(Addr) unused0; // DO NOT USE, maintained for compatibility.
#endif
    ElfW(Addr) base;
    size_t size;

#if defined(__work_around_b_24465209__)
    uint32_t unused1;  // DO NOT USE, maintained for compatibility.
#endif

    ElfW(Dyn)* dynamic;

#if defined(__work_around_b_24465209__)
    uint32_t unused2; // DO NOT USE, maintained for compatibility
  uint32_t unused3; // DO NOT USE, maintained for compatibility
#endif

    soinfo* next;

    uint32_t flags_;

    const char* strtab_;
    ElfW(Sym)* symtab_;

    size_t nbucket_;
    size_t nchain_;
    uint32_t* bucket_;
    uint32_t* chain_;

#if !defined(__LP64__)
    ElfW(Addr)** unused4; // DO NOT USE, maintained for compatibility
#endif

#if defined(USE_RELA)
    ElfW(Rela)* plt_rela_;
    size_t plt_rela_count_;

    ElfW(Rela)* rela_;
    size_t rela_count_;
#else
    ElfW(Rel)* plt_rel_;
  size_t plt_rel_count_;

  ElfW(Rel)* rel_;
  size_t rel_count_;
#endif

    linker_ctor_function_t* preinit_array_;
    size_t preinit_array_count_;

    linker_ctor_function_t* init_array_;
    size_t init_array_count_;
    linker_dtor_function_t* fini_array_;
    size_t fini_array_count_;

    linker_ctor_function_t init_func_;
    linker_dtor_function_t fini_func_;

#if defined(__arm__)
    public:
  // ARM EABI section used for stack unwinding.
  uint32_t* ARM_exidx;
  size_t ARM_exidx_count;
 private:
#endif
    size_t ref_count_;
public:
    link_map link_map_head;

    bool constructors_called;

    // When you read a virtual address from the ELF file, add this
    // value to get the corresponding address in the process' address space.
    ElfW(Addr) load_bias;

#if !defined(__LP64__)
    bool has_text_relocations;
#endif
    bool has_DT_SYMBOLIC;


    // This part of the structure is only available
    // when FLAG_NEW_SOINFO is set in this->flags.
    uint32_t version_;

    // version >= 0
    dev_t st_dev_;
    ino_t st_ino_;

    // dependency graph
    soinfo_list_t_12l children_;
    soinfo_list_t_12l parents_;

    // version >= 1
    off64_t file_offset_;
    uint32_t rtld_flags_;
    uint32_t dt_flags_1_;
    size_t strtab_size_;

    // version >= 2

    size_t gnu_nbucket_;
    uint32_t* gnu_bucket_;
    uint32_t* gnu_chain_;
    uint32_t gnu_maskwords_;
    uint32_t gnu_shift2_;
    ElfW(Addr)* gnu_bloom_filter_;

    soinfo* local_group_root_;

    uint8_t* android_relocs_;
    size_t android_relocs_size_;

    std::string soname_;
    std::string realpath_;

    const ElfW(Versym)* versym_;

    ElfW(Addr) verdef_ptr_;
    size_t verdef_cnt_;

    ElfW(Addr) verneed_ptr_;
    size_t verneed_cnt_;

    int target_sdk_version_;

    // version >= 3
    std::vector<std::string> dt_runpath_;
    void* primary_namespace_;
    android_namespace_list_t_12l secondary_namespaces_;
    uintptr_t handle_;


    // version >= 4
    ElfW(Relr)* relr_;
    size_t relr_count_;

//    // version >= 5
//    std::unique_ptr<soinfo_tls> tls_;
//    std::vector<TlsDynamicResolverArg> tlsdesc_args_;
//
//    // version >= 6
//    ElfW(Addr) gap_start_;
//    size_t gap_size_;


};








