#pragma once

#include <cstdio>
#include "omptarget.h"

#define CLIENT_DBG(...)                                                        \
  {                                                                            \
    if (DebugLevel > 0) {                                                      \
      fprintf(stderr, "[[Client]] --> ");                                      \
      fprintf(stderr, __VA_ARGS__);                                            \
      fprintf(stderr, "\n");                                                   \
    }                                                                          \
  }

#define SERVER_DBG(...)                                                        \
  {                                                                            \
    if (DebugLevel > 0) {                                                      \
      fprintf(stderr, "[[Server]] --> ");                                      \
      fprintf(stderr, __VA_ARGS__);                                            \
      fprintf(stderr, "\n");                                                   \
    }                                                                          \
  }

/// Frees argument as constructed by unloadTargetTable
void freeTargetTable(__tgt_target_table *Table);

void dump(const __tgt_bin_desc *Desc);
void dump(const char *Begin, int32_t Size, const std::string &Title = "");
void dump(int Offset, const char *Begin, const char *End);
void dump(const char *Begin, const char *End, const std::string &Title = "");
