//
// Created by chic on 2024/3/22.
//

#include "soinfo_12l_transform.h"



void android_12l_soinfo_transform(soinfo* si, soinfo_12l_transform * si_in){

    si->phdr = si_in->phdr;
    si->phnum = si_in->phnum;
    si->base = si_in->base;
    si->size = si_in->size;
    si->dynamic = si_in->dynamic;
    si->flags_ = si_in->flags_;
    si->strtab_ = si_in->strtab_;
    si->symtab_ = si_in->symtab_;
    si->nbucket_ = si_in->nbucket_;
    si->nchain_ = si_in->nchain_;
    si->bucket_ = si_in->bucket_;
    si->chain_ = si_in->chain_;

#if defined(USE_RELA)
    si->plt_rela_ = si_in->plt_rela_;
    si->plt_rela_count_ = si_in->plt_rela_count_;
    si->rela_ = si_in->rela_;
    si->rela_count_ = si_in->rela_count_;
#endif
    si->preinit_array_ = si_in->preinit_array_;
    si->preinit_array_count_ = si_in->preinit_array_count_;
    si->init_array_ = si_in->init_array_;
    si->init_array_count_ = si_in->init_array_count_;
    si->fini_array_ = si_in->fini_array_;
    si->fini_array_count_ = si_in->fini_array_count_;
    si->init_func_ = si_in->init_func_;
    si->fini_func_ = si_in->fini_func_;
    si->constructors_called = si_in->constructors_called;
    si->load_bias = si_in->load_bias;
    si->version_ = si_in->version_;
    si->rtld_flags_ = si_in->rtld_flags_;
    si->dt_flags_1_ = si_in->dt_flags_1_;
    si->strtab_size_ = si_in->strtab_size_;
    si->gnu_nbucket_ = si_in->gnu_nbucket_;
    si->gnu_bucket_ = si_in->gnu_bucket_;
    si->gnu_chain_ = si_in->gnu_chain_;
    si->gnu_maskwords_ = si_in->gnu_maskwords_;
    si->gnu_shift2_ = si_in->gnu_shift2_;
    si->gnu_bloom_filter_ = si_in->gnu_bloom_filter_;
    si->android_relocs_ = si_in->android_relocs_;
    si->android_relocs_size_ = si_in->android_relocs_size_;
    si->soname_ = si_in->soname_;
    si->versym_ = si_in->versym_;
    si->verdef_ptr_ = si_in->verdef_ptr_;
    si->verdef_cnt_ = si_in->verdef_cnt_;
    si->verneed_ptr_ = si_in->verneed_ptr_;
    si->verneed_cnt_ = si_in->verneed_cnt_;
}