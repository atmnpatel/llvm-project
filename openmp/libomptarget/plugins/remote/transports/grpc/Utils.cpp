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

/// Dumps the memory region from Start to End in order to debug memory transfer
/// errors within the plugin
void dump(const void *Start, const void *End) {
  unsigned char Line[17];
  const unsigned char *PrintCharacter = (const unsigned char *)Start;

  unsigned int I = 0;
  for (; I < ((const int *)End - (const int *)Start); I++) {
    if ((I % 16) == 0) {
      if (I != 0)
        printf("  %s\n", Line);

      printf("  %04x ", I);
    }

    printf(" %02x", PrintCharacter[I]);

    if ((PrintCharacter[I] < 0x20) || (PrintCharacter[I] > 0x7e))
      Line[I % 16] = '.';
    else
      Line[I % 16] = PrintCharacter[I];

    Line[(I % 16) + 1] = '\0';
  }

  while ((I % 16) != 0) {
    printf("   ");
    I++;
  }

  printf("  %s\n", Line);
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
  dump(Image->ImageStart, Image->ImageEnd);
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