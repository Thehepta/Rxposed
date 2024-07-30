//
// Created by chic on 2023/11/27.
//
#include <dlfcn.h>
#include "linker_soinfo.h"
#include "linker_debug.h"
#include "linker_phdr.h"
#include "linker_utils.h"
#include "linker_version.h"
#include "linker_relocate.h"
#include "linker_gnu_hash.h"
#include "linker.h"
#include "elf_symbol_resolver.h"
#include "linker_debug.h"
#include "soinfo_11_transform.h"
#include "soinfo_12_transform.h"
#include "soinfo_12L_transform.h"
#include "sys/ifunc.h"
#include <sys/auxv.h>

int g_argc = 0;
char** g_argv = nullptr;
char** g_envp = nullptr;


ElfW(Addr) __bionic_call_ifunc_resolver(ElfW(Addr) resolver_addr) {
#if defined(__aarch64__)
    typedef ElfW(Addr) (*ifunc_resolver_t)(uint64_t, __ifunc_arg_t*);
    static __ifunc_arg_t arg;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        arg._size = sizeof(__ifunc_arg_t);
        arg._hwcap = getauxval(AT_HWCAP);
        arg._hwcap2 = getauxval(AT_HWCAP2);
    }
    return reinterpret_cast<ifunc_resolver_t>(resolver_addr)(arg._hwcap | _IFUNC_ARG_HWCAP, &arg);
#elif defined(__arm__)
    typedef ElfW(Addr) (*ifunc_resolver_t)(unsigned long);
  static unsigned long hwcap = getauxval(AT_HWCAP);
  return reinterpret_cast<ifunc_resolver_t>(resolver_addr)(hwcap);
#elif defined(__riscv)
  // The third argument is currently unused, but reserved for future
  // expansion. If we pass nullptr from the beginning, it'll be easier
  // to recognize if/when we pass actual data (and matches glibc).
  typedef ElfW(Addr) (*ifunc_resolver_t)(uint64_t, __riscv_hwprobe_t, void*);
  static uint64_t hwcap = getauxval(AT_HWCAP);
  return reinterpret_cast<ifunc_resolver_t>(resolver_addr)(hwcap, __riscv_hwprobe, nullptr);
#else
  typedef ElfW(Addr) (*ifunc_resolver_t)(void);
  return reinterpret_cast<ifunc_resolver_t>(resolver_addr)();
#endif
}


ElfW(Addr) call_ifunc_resolver(ElfW(Addr) resolver_addr) {

    ElfW(Addr) ifunc_addr = __bionic_call_ifunc_resolver(resolver_addr);
    LOGE("Called ifunc_resolver@%p. The result is %p",reinterpret_cast<void *>(resolver_addr), reinterpret_cast<void*>(ifunc_addr));

    return ifunc_addr;
}

#if defined(__LP64__)
static constexpr const char* kLibPath = "lib64";
#else
static constexpr const char* kLibPath = "lib";
#endif


bool soinfo::prelink_image() {
    if (flags_ & FLAG_PRELINKED) return true;
    /* Extract dynamic section */
    ElfW(Word) dynamic_flags = 0;
    phdr_table_get_dynamic_section(phdr, phnum, load_bias, &dynamic, &dynamic_flags);

    /* We can't log anything until the linker is relocated */
    bool relocating_linker = (flags_ & FLAG_LINKER) != 0;
    if (!relocating_linker) {
        INFO("[ Linking \"%s\" ]", get_realpath());
        INFO("si->base = %p si->flags = 0x%08x", reinterpret_cast<void *>(base), flags_);
    }

    if (dynamic == nullptr) {
        if (!relocating_linker) {
            DL_ERR("missing PT_DYNAMIC in \"%s\"", get_realpath());
        }
        return false;
    } else {
        if (!relocating_linker) {
            INFO("dynamic = %p", dynamic);
        }
    }

#if defined(__arm__)
    (void) phdr_table_get_arm_exidx(phdr, phnum, load_bias,
                                  &ARM_exidx, &ARM_exidx_count);
#endif

//    TlsSegment tls_segment;
//    if (__bionic_get_tls_segment(phdr, phnum, load_bias, &tls_segment)) {
//        if (!__bionic_check_tls_alignment(&tls_segment.alignment)) {
//            if (!relocating_linker) {
//                DL_ERR("TLS segment alignment in \"%s\" is not a power of 2: %zu",
//                       get_realpath(), tls_segment.alignment);
//            }
//            return false;
//        }
//        tls_ = std::make_unique<soinfo_tls>();
//        tls_->segment = tls_segment;
//    }

    // Extract useful information from dynamic section.
    // Note that: "Except for the DT_NULL element at the end of the array,
    // and the relative order of DT_NEEDED elements, entries may appear in any order."
    //
    // source: http://www.sco.com/developers/gabi/1998-04-29/ch5.dynamic.html
    uint32_t needed_count = 0;
    for (ElfW(Dyn) *d = dynamic; d->d_tag != DT_NULL; ++d) {
        DEBUG("d = %p, d[0](tag) = %p d[1](val) = %p",
              d, reinterpret_cast<void *>(d->d_tag), reinterpret_cast<void *>(d->d_un.d_val));
        switch (d->d_tag) {
            case DT_SONAME:
                // this is parsed after we have strtab initialized (see below).
                break;

            case DT_HASH:
                nbucket_ = reinterpret_cast<uint32_t *>(load_bias + d->d_un.d_ptr)[0];
                nchain_ = reinterpret_cast<uint32_t *>(load_bias + d->d_un.d_ptr)[1];
                bucket_ = reinterpret_cast<uint32_t *>(load_bias + d->d_un.d_ptr + 8);
                chain_ = reinterpret_cast<uint32_t *>(load_bias + d->d_un.d_ptr + 8 + nbucket_ * 4);
                break;

            case DT_GNU_HASH:
                gnu_nbucket_ = reinterpret_cast<uint32_t *>(load_bias + d->d_un.d_ptr)[0];
                // skip symndx
                gnu_maskwords_ = reinterpret_cast<uint32_t *>(load_bias + d->d_un.d_ptr)[2];
                gnu_shift2_ = reinterpret_cast<uint32_t *>(load_bias + d->d_un.d_ptr)[3];

                gnu_bloom_filter_ = reinterpret_cast<ElfW(Addr) *>(load_bias + d->d_un.d_ptr + 16);
                gnu_bucket_ = reinterpret_cast<uint32_t *>(gnu_bloom_filter_ + gnu_maskwords_);
                // amend chain for symndx = header[1]
                gnu_chain_ = gnu_bucket_ + gnu_nbucket_ -
                             reinterpret_cast<uint32_t *>(load_bias + d->d_un.d_ptr)[1];

                if (!powerof2(gnu_maskwords_)) {
                    DL_ERR("invalid maskwords for gnu_hash = 0x%x, in \"%s\" expecting power to two",
                           gnu_maskwords_, get_realpath());
                    return false;
                }
                --gnu_maskwords_;

                flags_ |= FLAG_GNU_HASH;
                break;

            case DT_STRTAB:
                strtab_ = reinterpret_cast<const char *>(load_bias + d->d_un.d_ptr);
                break;

            case DT_STRSZ:
                strtab_size_ = d->d_un.d_val;
                break;

            case DT_SYMTAB:
                symtab_ = reinterpret_cast<ElfW(Sym) *>(load_bias + d->d_un.d_ptr);
                break;

            case DT_SYMENT:
                if (d->d_un.d_val != sizeof(ElfW(Sym))) {
                    DL_ERR("invalid DT_SYMENT: %zd in \"%s\"",
                           static_cast<size_t>(d->d_un.d_val), get_realpath());
                    return false;
                }
                break;

            case DT_PLTREL:
#if defined(USE_RELA)
                if (d->d_un.d_val != DT_RELA) {
                    DL_ERR("unsupported DT_PLTREL in \"%s\"; expected DT_RELA", get_realpath());
                    return false;
                }
#else
                if (d->d_un.d_val != DT_REL) {
          DL_ERR("unsupported DT_PLTREL in \"%s\"; expected DT_REL", get_realpath());
          return false;
        }
#endif
                break;

            case DT_JMPREL:
#if defined(USE_RELA)
                plt_rela_ = reinterpret_cast<ElfW(Rela) *>(load_bias + d->d_un.d_ptr);
#else
                plt_rel_ = reinterpret_cast<ElfW(Rel)*>(load_bias + d->d_un.d_ptr);
#endif
                break;

            case DT_PLTRELSZ:
#if defined(USE_RELA)
                plt_rela_count_ = d->d_un.d_val / sizeof(ElfW(Rela));
#else
                plt_rel_count_ = d->d_un.d_val / sizeof(ElfW(Rel));
#endif
                break;

            case DT_PLTGOT:
                // Ignored (because RTLD_LAZY is not supported).
                break;

            case DT_DEBUG:
                // Set the DT_DEBUG entry to the address of _r_debug for GDB
                // if the dynamic table is writable
//                if ((dynamic_flags & PF_W) != 0) {
//                    d->d_un.d_val = reinterpret_cast<uintptr_t>(&_r_debug);
//                }
                break;
#if defined(USE_RELA)
            case DT_RELA:
                rela_ = reinterpret_cast<ElfW(Rela) *>(load_bias + d->d_un.d_ptr);
                break;

            case DT_RELASZ:
                rela_count_ = d->d_un.d_val / sizeof(ElfW(Rela));
                break;

            case DT_ANDROID_RELA:
                android_relocs_ = reinterpret_cast<uint8_t *>(load_bias + d->d_un.d_ptr);
                break;

            case DT_ANDROID_RELASZ:
                android_relocs_size_ = d->d_un.d_val;
                break;

            case DT_ANDROID_REL:
                DL_ERR("unsupported DT_ANDROID_REL in \"%s\"", get_realpath());
                return false;

            case DT_ANDROID_RELSZ:
                DL_ERR("unsupported DT_ANDROID_RELSZ in \"%s\"", get_realpath());
                return false;

            case DT_RELAENT:
                if (d->d_un.d_val != sizeof(ElfW(Rela))) {
                    DL_ERR("invalid DT_RELAENT: %zd", static_cast<size_t>(d->d_un.d_val));
                    return false;
                }
                break;

                // Ignored (see DT_RELCOUNT comments for details).
            case DT_RELACOUNT:
                break;

            case DT_REL:
                DL_ERR("unsupported DT_REL in \"%s\"", get_realpath());
                return false;

            case DT_RELSZ:
                DL_ERR("unsupported DT_RELSZ in \"%s\"", get_realpath());
                return false;

#else
                case DT_REL:
        rel_ = reinterpret_cast<ElfW(Rel)*>(load_bias + d->d_un.d_ptr);
        break;

      case DT_RELSZ:
        rel_count_ = d->d_un.d_val / sizeof(ElfW(Rel));
        break;

      case DT_RELENT:
        if (d->d_un.d_val != sizeof(ElfW(Rel))) {
          DL_ERR("invalid DT_RELENT: %zd", static_cast<size_t>(d->d_un.d_val));
          return false;
        }
        break;

      case DT_ANDROID_REL:
        android_relocs_ = reinterpret_cast<uint8_t*>(load_bias + d->d_un.d_ptr);
        break;

      case DT_ANDROID_RELSZ:
        android_relocs_size_ = d->d_un.d_val;
        break;

      case DT_ANDROID_RELA:
        DL_ERR("unsupported DT_ANDROID_RELA in \"%s\"", get_realpath());
        return false;

      case DT_ANDROID_RELASZ:
        DL_ERR("unsupported DT_ANDROID_RELASZ in \"%s\"", get_realpath());
        return false;

      // "Indicates that all RELATIVE relocations have been concatenated together,
      // and specifies the RELATIVE relocation count."
      //
      // TODO: Spec also mentions that this can be used to optimize relocation process;
      // Not currently used by bionic linker - ignored.
      case DT_RELCOUNT:
        break;

      case DT_RELA:
        DL_ERR("unsupported DT_RELA in \"%s\"", get_realpath());
        return false;

      case DT_RELASZ:
        DL_ERR("unsupported DT_RELASZ in \"%s\"", get_realpath());
        return false;

#endif
            case DT_RELR:
            case DT_ANDROID_RELR:
                relr_ = reinterpret_cast<ElfW(Relr) *>(load_bias + d->d_un.d_ptr);
                break;

            case DT_RELRSZ:
            case DT_ANDROID_RELRSZ:
                relr_count_ = d->d_un.d_val / sizeof(ElfW(Relr));
                break;

            case DT_RELRENT:
            case DT_ANDROID_RELRENT:
                if (d->d_un.d_val != sizeof(ElfW(Relr))) {
                    DL_ERR("invalid DT_RELRENT: %zd", static_cast<size_t>(d->d_un.d_val));
                    return false;
                }
                break;

                // Ignored (see DT_RELCOUNT comments for details).
                // There is no DT_RELRCOUNT specifically because it would only be ignored.
            case DT_ANDROID_RELRCOUNT:
                break;

            case DT_INIT:
                init_func_ = reinterpret_cast<linker_ctor_function_t>(load_bias + d->d_un.d_ptr);
                DEBUG("%s constructors (DT_INIT) found at %p", get_realpath(), init_func_);
                break;

            case DT_FINI:
                fini_func_ = reinterpret_cast<linker_dtor_function_t>(load_bias + d->d_un.d_ptr);
                DEBUG("%s destructors (DT_FINI) found at %p", get_realpath(), fini_func_);
                break;

            case DT_INIT_ARRAY:
                init_array_ = reinterpret_cast<linker_ctor_function_t *>(load_bias + d->d_un.d_ptr);
                DEBUG("%s constructors (DT_INIT_ARRAY) found at %p", get_realpath(), init_array_);
                break;

            case DT_INIT_ARRAYSZ:
                init_array_count_ = static_cast<uint32_t>(d->d_un.d_val) / sizeof(ElfW(Addr));
                break;

            case DT_FINI_ARRAY:
                fini_array_ = reinterpret_cast<linker_dtor_function_t *>(load_bias + d->d_un.d_ptr);
                DEBUG("%s destructors (DT_FINI_ARRAY) found at %p", get_realpath(), fini_array_);
                break;

            case DT_FINI_ARRAYSZ:
                fini_array_count_ = static_cast<uint32_t>(d->d_un.d_val) / sizeof(ElfW(Addr));
                break;

            case DT_PREINIT_ARRAY:
                preinit_array_ = reinterpret_cast<linker_ctor_function_t *>(load_bias +
                                                                            d->d_un.d_ptr);
                DEBUG("%s constructors (DT_PREINIT_ARRAY) found at %p", get_realpath(),
                      preinit_array_);
                break;

            case DT_PREINIT_ARRAYSZ:
                preinit_array_count_ = static_cast<uint32_t>(d->d_un.d_val) / sizeof(ElfW(Addr));
                break;

            case DT_TEXTREL:
#if defined(__LP64__)
                DL_ERR("\"%s\" has text relocations", get_realpath());
                return false;
#else
                has_text_relocations = true;
        break;
#endif

            case DT_SYMBOLIC:
                has_DT_SYMBOLIC = true;
                break;

            case DT_NEEDED:
                ++needed_count;
                break;

            case DT_FLAGS:
                if (d->d_un.d_val & DF_TEXTREL) {
#if defined(__LP64__)
                    DL_ERR("\"%s\" has text relocations", get_realpath());
                    return false;
#else
                    has_text_relocations = true;
#endif
                }
                if (d->d_un.d_val & DF_SYMBOLIC) {
                    has_DT_SYMBOLIC = true;
                }
                break;

            case DT_FLAGS_1:
                set_dt_flags_1(d->d_un.d_val);

                if ((d->d_un.d_val & ~SUPPORTED_DT_FLAGS_1) != 0) {
                    DL_WARN("Warning: \"%s\" has unsupported flags DT_FLAGS_1=%p "
                            "(ignoring unsupported flags)",
                            get_realpath(), reinterpret_cast<void *>(d->d_un.d_val));
                }
                break;

                // Ignored: "Its use has been superseded by the DF_BIND_NOW flag"
            case DT_BIND_NOW:
                break;

            case DT_VERSYM:
                versym_ = reinterpret_cast<ElfW(Versym) *>(load_bias + d->d_un.d_ptr);
                break;

            case DT_VERDEF:
                verdef_ptr_ = load_bias + d->d_un.d_ptr;
                break;
            case DT_VERDEFNUM:
                verdef_cnt_ = d->d_un.d_val;
                break;

            case DT_VERNEED:
                verneed_ptr_ = load_bias + d->d_un.d_ptr;
                break;

            case DT_VERNEEDNUM:
                verneed_cnt_ = d->d_un.d_val;
                break;

            case DT_RUNPATH:
                // this is parsed after we have strtab initialized (see below).
                break;

            case DT_TLSDESC_GOT:
            case DT_TLSDESC_PLT:
                // These DT entries are used for lazy TLSDESC relocations. Bionic
                // resolves everything eagerly, so these can be ignored.
                break;

            default:
                if (!relocating_linker) {
                    const char *tag_name;
                    if (d->d_tag == DT_RPATH) {
                        tag_name = "DT_RPATH";
                    } else if (d->d_tag == DT_ENCODING) {
                        tag_name = "DT_ENCODING";
                    } else if (d->d_tag >= DT_LOOS && d->d_tag <= DT_HIOS) {
                        tag_name = "unknown OS-specific";
                    } else if (d->d_tag >= DT_LOPROC && d->d_tag <= DT_HIPROC) {
                        tag_name = "unknown processor-specific";
                    } else {
                        tag_name = "unknown";
                    }
                    DL_WARN("Warning: \"%s\" unused DT entry: %s (type %p arg %p) (ignoring)",
                            get_realpath(),
                            tag_name,
                            reinterpret_cast<void *>(d->d_tag),
                            reinterpret_cast<void *>(d->d_un.d_val));
                }
                break;
        }
    }

    DEBUG("si->base = %p, si->strtab = %p, si->symtab = %p",
          reinterpret_cast<void*>(base), strtab_, symtab_);

    // Sanity checks.
    if (relocating_linker && needed_count != 0) {
        DL_ERR("linker cannot have DT_NEEDED dependencies on other libraries");
        return false;
    }
    if (nbucket_ == 0 && gnu_nbucket_ == 0) {
        DL_ERR("empty/missing DT_HASH/DT_GNU_HASH in \"%s\" "
               "(new hash type from the future?)", get_realpath());
        return false;
    }
    if (strtab_ == nullptr) {
        DL_ERR("empty/missing DT_STRTAB in \"%s\"", get_realpath());
        return false;
    }
    if (symtab_ == nullptr) {
        DL_ERR("empty/missing DT_SYMTAB in \"%s\"", get_realpath());
        return false;
    }

    // second pass - parse entries relying on strtab
    for (ElfW(Dyn)* d = dynamic; d->d_tag != DT_NULL; ++d) {
        switch (d->d_tag) {
            case DT_SONAME:
                set_soname(get_string(d->d_un.d_val));
                break;
            case DT_RUNPATH:
                set_dt_runpath(get_string(d->d_un.d_val));
                break;
        }
    }

    // Before M release linker was using basename in place of soname.
    // In the case when dt_soname is absent some apps stop working
    // because they can't find dt_needed library by soname.
    // This workaround should keep them working. (Applies only
    // for apps targeting sdk version < M.) Make an exception for
    // the main executable and linker; they do not need to have dt_soname.
    // TODO: >= O the linker doesn't need this workaround.
//    if (soname_ == nullptr &&
//        this != solist_get_somain() &&
//        (flags_ & FLAG_LINKER) == 0 &&
//        android_get_application_target_sdk_version() < 23) {
//        soname_ = basename(realpath_.c_str());
//        DL_WARN_documented_change(23,
//                                  "missing-soname-enforced-for-api-level-23",
//                                  "\"%s\" has no DT_SONAME (will use %s instead)",
//                                  get_realpath(), soname_);
//
//        // Don't call add_dlwarning because a missing DT_SONAME isn't important enough to show in the UI
//    }

    // Validate each library's verdef section once, so we don't have to validate
    // it each time we look up a symbol with a version.
//    if (!validate_verdef_section(this)) return false;

    flags_ |= FLAG_PRELINKED;
    return true;
}









static void call_function(const char* function_name __unused,
                          linker_ctor_function_t function,
                          const char* realpath __unused) {
    if (function == nullptr || reinterpret_cast<uintptr_t>(function) == static_cast<uintptr_t>(-1)) {
        return;
    }

    TRACE("[ Calling c-tor %s @ %p for '%s' ]", function_name, function, realpath);
    function(g_argc, g_argv, g_envp);
    TRACE("[ Done calling c-tor %s @ %p for '%s' ]", function_name, function, realpath);
}



template <typename F>
static void call_array(const char* array_name __unused,
                       F* functions,
                       size_t count,
                       bool reverse,
                       const char* realpath) {
    if (functions == nullptr) {
        return;
    }

    TRACE("[ Calling %s (size %zd) @ %p for '%s' ]", array_name, count, functions, realpath);

    int begin = reverse ? (count - 1) : 0;
    int end = reverse ? -1 : count;
    int step = reverse ? -1 : 1;

    for (int i = begin; i != end; i += step) {
        TRACE("[ %s[%d] == %p ]", array_name, i, functions[i]);
        call_function("function", functions[i], realpath);
    }

    TRACE("[ Done calling %s for '%s' ]", array_name, realpath);
}



void soinfo::call_constructors() {
    if (constructors_called ) {
        return;
    }

    constructors_called = true;

    if ( preinit_array_ != nullptr) {
        // The GNU dynamic linker silently ignores these, but we warn the developer.
        PRINT("\"%s\": ignoring DT_PREINIT_ARRAY in shared library!", get_realpath());
    }

//    get_children().for_each([] (soinfo* si) {
//        si->call_constructors();
//    });
//
//    if (!is_linker()) {
//        bionic_trace_begin((std::string("calling constructors: ") + get_realpath()).c_str());
//    }

    // DT_INIT should be called before DT_INIT_ARRAY if both are present.
    call_function("DT_INIT", init_func_, get_realpath());
    call_array("DT_INIT_ARRAY", init_array_, init_array_count_, false, get_realpath());

//    if (!is_linker()) {
//        bionic_trace_end();
//    }
}

void soinfo::set_mapped_by_caller(bool mapped_by_caller) {
    if (mapped_by_caller) {
        flags_ |= FLAG_MAPPED_BY_CALLER;
    } else {
        flags_ &= ~FLAG_MAPPED_BY_CALLER;
    }
}

const char *soinfo::get_realpath() const {
    return realpath_.c_str();
}

void soinfo::set_dt_flags_1(uint32_t dt_flags_1) {
    if (has_min_version(1)) {
        if ((dt_flags_1 & DF_1_GLOBAL) != 0) {
            rtld_flags_ |= RTLD_GLOBAL;
        }

        if ((dt_flags_1 & DF_1_NODELETE) != 0) {
            rtld_flags_ |= RTLD_NODELETE;
        }

        dt_flags_1_ = dt_flags_1;
    }
}

uint32_t soinfo::get_dt_flags_1() const {
    if (has_min_version(1)) {
        return dt_flags_1_;
    }

    return 0;
}

android_namespace_t *soinfo::get_primary_namespace() {
    if (has_min_version(3)) {
        return primary_namespace_;
    }

    return nullptr;
}

const char *soinfo::get_soname() const {
#if defined(__work_around_b_24465209__)
    if (has_min_version(2)) {
    return soname_;
  } else {
    return old_name_;
  }
#else
    return soname_.c_str();
#endif

}

void soinfo::set_dt_runpath(const char* path) {
    if (!has_min_version(3)) {
        return;
    }

    std::vector<std::string> runpaths;

    split_path(path, ":", &runpaths);

    std::string origin = dirname(get_realpath());
    // FIXME: add $PLATFORM.
    std::vector<std::pair<std::string, std::string>> params = {
            {"ORIGIN", origin},
            {"LIB", kLibPath},
    };
    for (auto&& s : runpaths) {
        format_string(&s, params);
    }

    resolve_paths(runpaths, &dt_runpath_);
}

void soinfo::set_soname(const char* soname) {
#if defined(__work_around_b_24465209__)
    if (has_min_version(2)) {
    soname_ = soname;
  }
  strlcpy(old_name_, soname_, sizeof(old_name_));
#else
    soname_ = soname;
#endif
}

const char* soinfo::get_string(ElfW(Word) index) const {
    if (has_min_version(1) && (index >= strtab_size_)) {
        LOGE("%s: strtab out of bounds error; STRSZ=%zd, name=%d",
                         get_realpath(), strtab_size_, index);
    }

    return strtab_ + index;
}

bool soinfo::link_image(SymbolLookupList& lookup_list) {

    local_group_root_ = this;

    if ((flags_ & FLAG_LINKER) == 0 && local_group_root_ == this) {
        target_sdk_version_ = android_get_application_target_sdk_version();
    }

#if !defined(__LP64__)
    if (has_text_relocations) {
    // Fail if app is targeting M or above.
    int app_target_api_level = android_get_application_target_sdk_version();
    if (app_target_api_level >= 23) {
      DL_ERR_AND_LOG("\"%s\" has text relocations (https://android.googlesource.com/platform/"
                     "bionic/+/master/android-changes-for-ndk-developers.md#Text-Relocations-"
                     "Enforced-for-API-level-23)", get_realpath());
      return false;
    }
    if (phdr_table_unprotect_segments(phdr, phnum, load_bias) < 0) {
      DL_ERR("can't unprotect loadable segments for \"%s\": %s", get_realpath(), strerror(errno));
      return false;
    }
  }
#endif


    if (!relocate( lookup_list)) {
        return false;
    }
    DEBUG("[ finished linking %s ]", get_realpath());

#if !defined(__LP64__)
    if (has_text_relocations) {
        // All relocations are done, we can protect our segments back to read-only.
        if (phdr_table_protect_segments(phdr, phnum, load_bias) < 0) {
            DL_ERR("can't protect segments for \"%s\": %s",
                   get_realpath(), strerror(errno));
            return false;
        }
    }
#endif



    return true;

}

void soinfo::set_linked() {
    flags_ |= FLAG_LINKED;
}


bool soinfo::is_linked() const {
    return (flags_ & FLAG_LINKED) != 0;
}

const ElfW(Versym)* soinfo::get_versym(size_t n) const {
    auto table = get_versym_table();
    return table ? table + n : nullptr;
}

bool soinfo::is_gnu_hash() const {
    return (flags_ & FLAG_GNU_HASH) != 0;
}


static soinfo_list_t g_empty_list;

soinfo_list_t& soinfo::get_children() {
    if (has_min_version(0)) {
        return children_;
    }

    return g_empty_list;
}

const soinfo_list_t& soinfo::get_children() const {
    if (has_min_version(0)) {
        return children_;
    }

    return g_empty_list;
}

ElfW(Addr) soinfo::get_verneed_ptr() const {
    if (has_min_version(2)) {
        return verneed_ptr_;
    }

    return 0;
}

size_t soinfo::get_verneed_cnt() const {
    if (has_min_version(2)) {
        return verneed_cnt_;
    }

    return 0;
}

ElfW(Addr) soinfo::get_verdef_ptr() const {
    if (has_min_version(2)) {
        return verdef_ptr_;
    }

    return 0;
}

size_t soinfo::get_verdef_cnt() const {
    if (has_min_version(2)) {
        return verdef_cnt_;
    }

    return 0;
}

SymbolLookupLib soinfo::get_lookup_lib(bool system_sonifo) {
    SymbolLookupLib result {};
    result.si_ = this;

    // For libs that only have SysV hashes, leave the gnu_bloom_filter_ field NULL to signal that
    // the fallback code path is needed.
    if (!is_gnu_hash()) {
        return result;
    }

    result.gnu_maskwords_ = gnu_maskwords_;
    result.gnu_shift2_ = gnu_shift2_;
    result.gnu_bloom_filter_ = gnu_bloom_filter_;

    result.strtab_ = strtab_;
    result.strtab_size_ = strtab_size_;
    result.symtab_ = symtab_;
    result.versym_ = get_versym_table();

    result.gnu_chain_ = gnu_chain_;
    result.gnu_nbucket_ = gnu_nbucket_;
    result.gnu_bucket_ = gnu_bucket_;
    result.system_sonifo = system_sonifo;

    return result;
}

bool soinfo::lookup_version_info(const VersionTracker& version_tracker, ElfW(Word) sym,
                                 const char* sym_name, const version_info** vi) {
    const ElfW(Versym)* sym_ver_ptr = get_versym(sym);
    ElfW(Versym) sym_ver = sym_ver_ptr == nullptr ? 0 : *sym_ver_ptr;

    if (sym_ver != VER_NDX_LOCAL && sym_ver != VER_NDX_GLOBAL) {
        *vi = static_cast<const version_info *>(version_tracker.get_version_info(sym_ver));

        if (*vi == nullptr) {
            DL_ERR("cannot find verneed/verdef for version index=%d "
                   "referenced by symbol \"%s\" at \"%s\"", sym_ver, sym_name, get_realpath());
            return false;
        }
    } else {
        // there is no version info
        *vi = nullptr;
    }

    return true;
}

void soinfo::apply_relr_reloc(ElfW(Addr) offset) {
    ElfW(Addr) address = offset + load_bias;
    *reinterpret_cast<ElfW(Addr)*>(address) += load_bias;
}

// Process relocations in SHT_RELR section (experimental).
// Details of the encoding are described in this post:
//   https://groups.google.com/d/msg/generic-abi/bX460iggiKg/Pi9aSwwABgAJ
bool soinfo::relocate_relr() {
    ElfW(Relr)* begin = relr_;
    ElfW(Relr)* end = relr_ + relr_count_;
    constexpr size_t wordsize = sizeof(ElfW(Addr));

    ElfW(Addr) base = 0;
    for (ElfW(Relr)* current = begin; current < end; ++current) {
        ElfW(Relr) entry = *current;
        ElfW(Addr) offset;

        if ((entry&1) == 0) {
            // Even entry: encodes the offset for next relocation.
            offset = static_cast<ElfW(Addr)>(entry);
            apply_relr_reloc(offset);
            // Set base offset for subsequent bitmap entries.
            base = offset + wordsize;
            continue;
        }

        // Odd entry: encodes bitmap for relocations starting at base.
        offset = base;
        while (entry != 0) {
            entry >>= 1;
            if ((entry&1) != 0) {
                apply_relr_reloc(offset);
            }
            offset += wordsize;
        }

        // Advance base offset by 63 words for 64-bit platforms,
        // or 31 words for 32-bit platforms.
        base += (8*wordsize - 1) * wordsize;
    }
    return true;
}

const ElfW(Sym) *
soinfo::find_symbol_by_name(SymbolName &symbol_name, const version_info *vi) const {
    return is_gnu_hash() ? gnu_lookup(symbol_name, vi) : elf_lookup(symbol_name, vi);
}



static inline bool check_symbol_version(const ElfW(Versym)* ver_table, uint32_t sym_idx,
                                        const ElfW(Versym) verneed) {
    if (ver_table == nullptr) return true;
    const uint32_t verdef = ver_table[sym_idx];
    return (verneed == kVersymNotNeeded) ?
           !(verdef & kVersymHiddenBit) :
           verneed == (verdef & ~kVersymHiddenBit);
}

inline bool is_symbol_global_and_defined(const soinfo* si, const ElfW(Sym)* s) {
    if (__predict_true(ELF_ST_BIND(s->st_info) == STB_GLOBAL ||
                       ELF_ST_BIND(s->st_info) == STB_WEAK)) {
        return s->st_shndx != SHN_UNDEF;
    } else if (__predict_false(ELF_ST_BIND(s->st_info) != STB_LOCAL)) {
        DL_WARN("Warning: unexpected ST_BIND value: %d for \"%s\" in \"%s\" (ignoring)",
                ELF_ST_BIND(s->st_info), si->get_string(s->st_name), si->get_realpath());
    }
    return false;
}


const ElfW(Sym)* soinfo::gnu_lookup(SymbolName& symbol_name, const version_info* vi) const {
    const uint32_t hash = symbol_name.gnu_hash();

    constexpr uint32_t kBloomMaskBits = sizeof(ElfW(Addr)) * 8;
    const uint32_t word_num = (hash / kBloomMaskBits) & gnu_maskwords_;
    const ElfW(Addr) bloom_word = gnu_bloom_filter_[word_num];
    const uint32_t h1 = hash % kBloomMaskBits;
    const uint32_t h2 = (hash >> gnu_shift2_) % kBloomMaskBits;

    LOGE( "SEARCH %s in %s@%p (gnu)",
               symbol_name.get_name(), get_realpath(), reinterpret_cast<void*>(base));

    // test against bloom filter
    if ((1 & (bloom_word >> h1) & (bloom_word >> h2)) == 0) {
        LOGE( "NOT FOUND %s in %s@%p",
                   symbol_name.get_name(), get_realpath(), reinterpret_cast<void*>(base));

        return nullptr;
    }

    // bloom test says "probably yes"...
    uint32_t n = gnu_bucket_[hash % gnu_nbucket_];

    if (n == 0) {
        LOGE( "NOT FOUND %s in %s@%p",
                   symbol_name.get_name(), get_realpath(), reinterpret_cast<void*>(base));

        return nullptr;
    }

    const ElfW(Versym) verneed = find_verdef_version_index(this, vi);
    const ElfW(Versym)* versym = get_versym_table();

    do {
        ElfW(Sym)* s = symtab_ + n;
        if (((gnu_chain_[n] ^ hash) >> 1) == 0 &&
            check_symbol_version(versym, n, verneed) &&
            strcmp(get_string(s->st_name), symbol_name.get_name()) == 0 &&
            is_symbol_global_and_defined(this, s)) {
            LOGE( "FOUND %s in %s (%p) %zd",
                       symbol_name.get_name(), get_realpath(), reinterpret_cast<void*>(s->st_value),
                       static_cast<size_t>(s->st_size));
            return symtab_ + n;
        }
    } while ((gnu_chain_[n++] & 1) == 0);

    LOGE( "NOT FOUND %s in %s@%p",
               symbol_name.get_name(), get_realpath(), reinterpret_cast<void*>(base));

    return nullptr;
}

const ElfW(Sym)* soinfo::elf_lookup(SymbolName& symbol_name, const version_info* vi) const {
    uint32_t hash = symbol_name.elf_hash();

    LOGE( "SEARCH %s in %s@%p h=%x(elf) %zd",
               symbol_name.get_name(), get_realpath(),
               reinterpret_cast<void*>(base), hash, hash % nbucket_);

    const ElfW(Versym) verneed = find_verdef_version_index(this, vi);
    const ElfW(Versym)* versym = get_versym_table();

    for (uint32_t n = bucket_[hash % nbucket_]; n != 0; n = chain_[n]) {
        ElfW(Sym)* s = symtab_ + n;

        if (check_symbol_version(versym, n, verneed) &&
            strcmp(get_string(s->st_name), symbol_name.get_name()) == 0 &&
            is_symbol_global_and_defined(this, s)) {
            LOGE("FOUND %s in %s (%p) %zd",
                       symbol_name.get_name(), get_realpath(),
                       reinterpret_cast<void*>(s->st_value),
                       static_cast<size_t>(s->st_size));
            return symtab_ + n;
        }
    }

    LOGE( "NOT FOUND %s in %s@%p %x %zd",
               symbol_name.get_name(), get_realpath(),
               reinterpret_cast<void*>(base), hash, hash % nbucket_);

    return nullptr;
}

void soinfo::add_child(soinfo *child) {
    if (has_min_version(0)) {
//        child->parents_.push_back(this);
//        this->children_.push_back(child);
    }
}

soinfo::soinfo(android_namespace_t *ns, const char *name, const struct stat *file_stat,
               off64_t file_offset, int rtld_flags) {

    memset(this, 0, sizeof(*this));
    flags_ = FLAG_NEW_SOINFO;
    version_ = SOINFO_VERSION;

    if (file_stat != nullptr) {
        this->st_dev_ = file_stat->st_dev;
        this->st_ino_ = file_stat->st_ino;
        this->file_offset_ = file_offset;
    }

    this->rtld_flags_ = rtld_flags;

}

void soinfo::transform(soinfo * si) {

//第一种方式通过soinfo的字段的前几个位置不变的属性，计算出soinfo结构体后面的随着版本导致结构体变动的属性
//相比第二种有性能损耗，但是更优雅，兼容性更强，理论上兼容所有的版本
    this->base = si->base;
    this->size = si->size;
    this->flags_ = 0;
    this->load_bias = si->load_bias;
    this->phnum = si->phnum;
    this->phdr = si->phdr;
    this->prelink_image();


    //寻找对应android版本soinfo的结构体，然后将soinfo强转成对应版本的，然后将属性赋值给当前使用的这个soinfo
    //每兼容一个版本就要多写一个结构体，但是速度快
//    if(android_get_device_api_level() == 33){              //android 13
//
//    } else if (android_get_device_api_level() == 32){      //android 12l
//        android_12l_soinfo_transform(this, reinterpret_cast<soinfo_12l_transform *>(si));
//    } else if (android_get_device_api_level() == 31){      //android 12
//        android_12_soinfo_transform(this, reinterpret_cast<soinfo_12_transform *>(si));
//    } else if (android_get_device_api_level() == 30){      //android 11
//        android_11_soinfo_transform(this, reinterpret_cast<soinfo_11_transform *>(si));
//    }


}

ElfW(Addr) soinfo::resolve_symbol_address(const ElfW(Sym)* s) const  {
    if (ELF_ST_TYPE(s->st_info) == STT_GNU_IFUNC) {
        return call_ifunc_resolver(s->st_value + load_bias);
    }

    return static_cast<ElfW(Addr)>(s->st_value + load_bias);
}


uint32_t calculate_elf_hash(const char* name) {
    const uint8_t* name_bytes = reinterpret_cast<const uint8_t*>(name);
    uint32_t h = 0, g;

    while (*name_bytes) {
        h = (h << 4) + *name_bytes++;
        g = h & 0xf0000000;
        h ^= g;
        h ^= g >> 24;
    }

    return h;
}


uint32_t SymbolName::elf_hash() {
    if (!has_elf_hash_) {
        elf_hash_ = calculate_elf_hash(name_);
        has_elf_hash_ = true;
    }

    return elf_hash_;
}

uint32_t SymbolName::gnu_hash() {
    if (!has_gnu_hash_) {
        gnu_hash_ = calculate_gnu_hash(name_).first;
        has_gnu_hash_ = true;
    }

    return gnu_hash_;
}

