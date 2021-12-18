#include "BaseUtils.h"
#include <messages.pb.h>


void freeTargetTable(__tgt_target_table *Table) {
  for (auto *Entry = Table->EntriesBegin; Entry != Table->EntriesEnd; Entry++)
    free(Entry->name);

  delete[] Table->EntriesBegin;
}

void dump(const __tgt_offload_entry *Entry) {
  printf("  Entry (%s): %p [%ld], %d, %d\n", Entry->name, Entry->addr, Entry->size, Entry->flags, Entry->reserved);
}

void dump(const __tgt_bin_desc *Desc) {
  printf("Global Entries:\n");
  for (auto *Entry = Desc->HostEntriesBegin; Entry != Desc->HostEntriesEnd; Entry++) {
    dump(Entry);
  }

  printf("Images: %d\n", Desc->NumDeviceImages);
  auto *Image = Desc->DeviceImages;
  for (auto Idx = 0; Idx < Desc->NumDeviceImages; Idx++, Image++) {
    printf("Image %d\n", Idx);
    for (auto *Entry = Image->EntriesBegin; Entry != Image->EntriesEnd; Entry++) {
      dump(Entry);
    }
  }
}

void dump(size_t Offset, const char *Begin, const char *End) {
  printf("(dec) %lu:  ", Offset);
  for (auto *Itr = Begin; Itr != End; Itr++) {
    printf(" %d", *Itr);
  }
  printf("\n");

  printf("(hex) %lu:  ", Offset);
  for (auto *Itr = Begin; Itr != End; Itr++) {
    printf(" %x", *Itr);
  }
  printf("\n");

  printf("(asc) %lu:  ", Offset);
  for (auto *Itr = Begin; Itr != End; Itr++) {
    if (std::isgraph(*Itr)) {
      printf(" %c", *Itr);
    } else {
      printf(" %o", *Itr);
    }
  }
  printf("\n");
}

void dump(const char *Begin, int32_t Size, const std::string &Title) {
  return dump(Begin, Begin + Size, Title);
}

void dump(std::string Buffer) {
  return dump(Buffer.data(), Buffer.data() + Buffer.size(), "");
}

void dump(const char *Begin, const char *End, const std::string &Title) {
  printf("======================= %s =======================\n", Title.c_str());
  for (size_t offset = 0; offset < End - Begin; offset += 16)
    dump(offset, Begin + offset, std::min(Begin + offset + 16, End));
}
