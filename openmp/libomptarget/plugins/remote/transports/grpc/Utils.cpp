//===---------------- Utils.cpp - Utilities for Remote RTL ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Utilities for data movement and debugging.
//
//===----------------------------------------------------------------------===//

#include "Utils.h"
#include "omptarget.h"

namespace transport::grpc {

void dump(size_t Offset, char *Begin, const char *End) {
  printf("(dec) %lu:  ", Offset);
  for (char *Itr = Begin; Itr != End; Itr++) {
    printf(" %d", *Itr);
  }
  printf("\n");

  printf("(hex) %lu:  ", Offset);
  for (char *Itr = Begin; Itr != End; Itr++) {
    printf(" %x", *Itr);
  }
  printf("\n");

  printf("(asc) %lu:  ", Offset);
  for (char *Itr = Begin; Itr != End; Itr++) {
    if (std::isgraph(*Itr)) {
      printf(" %c", *Itr);
    } else {
      printf(" %o", *Itr);
    }
  }
  printf("\n");
}

void dump(char *Begin, int32_t Size, const std::string &Title) {
  return dump(Begin, Begin + Size, Title);
}

void dump(const char *Begin, const char *End, const std::string &Title) {
  printf("======================= %s =======================\n", Title.c_str());
  for (size_t offset = 0; offset < End - Begin; offset += 16)
    dump(offset, (char *)Begin + offset, std::min(Begin + offset + 16, End));
}

void dump(__tgt_offload_entry *Entry) {
  fprintf(stderr, "Entry (%p):\n", (void *)Entry);
  fprintf(stderr, "  Name: %s (%p)\n", Entry->name, (void *)&Entry->name);
  fprintf(stderr, "  Reserved: %d (%p)\n", Entry->reserved,
          (void *)&Entry->reserved);
  fprintf(stderr, "  Flags: %d (%p)\n", Entry->flags, (void *)&Entry->flags);
  fprintf(stderr, "  Addr: %p\n", Entry->addr);
  fprintf(stderr, "  Size: %lu\n", Entry->size);
}

void dump(__tgt_target_table *Table) {
  for (auto *CurEntry = Table->EntriesBegin; CurEntry != Table->EntriesEnd;
       CurEntry++)
    dump(CurEntry);
}

void dump(TargetOffloadEntry Entry) {
  fprintf(stderr, "Entry: ");
  fprintf(stderr, "  Name: %s\n", Entry.name().c_str());
  fprintf(stderr, "  Reserved: %d\n", Entry.reserved());
  fprintf(stderr, "  Flags: %d\n", Entry.flags());
  fprintf(stderr, "  Size:  %ld\n", Entry.data().size());
  dump(static_cast<const void *>(Entry.data().data()),
       static_cast<const void *>((Entry.data().c_str() + Entry.data().size())));
}

void dump(__tgt_device_image *Image) {
  dump((char *) Image->ImageStart, (char *) Image->ImageEnd);
  __tgt_offload_entry *EntryItr = Image->EntriesBegin;
  for (; EntryItr != Image->EntriesEnd; EntryItr++)
    dump(EntryItr);
}

void dump(std::unordered_map<void *, __tgt_offload_entry *> &Map) {
  fprintf(stderr, "Host to Remote Entry Map:\n");
  for (auto Entry : Map)
    fprintf(stderr, "  Host (%p) -> Tgt (%p): Addr((%p))\n", Entry.first,
            (void *)Entry.second, (void *)Entry.second->addr);
}
} // namespace transport::grpc
