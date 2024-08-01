//
// Created by chic on 2023/6/2.
//

#pragma once


#include <sys/types.h>

void * ResolveElfInternalSymbol(pid_t pid,const char *library_name, const char *symbol_name);