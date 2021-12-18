#include "Utils.h"

namespace transport::grpc {

void loadTargetBinaryDescription(
    const __tgt_bin_desc *Desc,
    transport::messages::TargetBinaryDescription &Request) {
  Request.set_bin_ptr((uint64_t)Desc);

  // Copy Global Offload Entries
  for (auto *CurEntry = Desc->HostEntriesBegin;
       CurEntry != Desc->HostEntriesEnd; CurEntry++) {
    auto *NewEntry = Request.add_entries();
    copyOffloadEntry(CurEntry, NewEntry);
  }

  // Copy Device Images and Device Offload Entries
  __tgt_device_image *CurImage = Desc->DeviceImages;
  for (auto I = 0; I < Desc->NumDeviceImages; I++, CurImage++) {
    auto *Image = Request.add_images();
    auto Size = (char *)CurImage->ImageEnd - (char *)CurImage->ImageStart;
    Image->set_binary(CurImage->ImageStart, Size);
    Image->set_img_ptr((uint64_t)CurImage);

    // Copy Device Offload Entries
    for (auto *CurEntry = CurImage->EntriesBegin;
         CurEntry != CurImage->EntriesEnd; CurEntry++) {
      auto *NewEntry = Image->add_entries();
      copyOffloadEntry(CurEntry, NewEntry);
    }
  }
}

void unloadTargetBinaryDescription(
    const transport::messages::TargetBinaryDescription *Request,
    __tgt_bin_desc *Desc,
    std::unordered_map<const void *, __tgt_device_image *>
        &HostToRemoteDeviceImage) {
  Desc->NumDeviceImages = Request->images_size();
  Desc->DeviceImages = new __tgt_device_image[Desc->NumDeviceImages];
  Desc->HostEntriesBegin = new __tgt_offload_entry[Request->entries_size()];

  // Copy Global Offload Entries
  __tgt_offload_entry *CurEntry = Desc->HostEntriesBegin;
  for (const auto &Entry : Request->entries()) {
    copyOffloadEntry(Entry, CurEntry);
    CurEntry++;
  }
  Desc->HostEntriesEnd = CurEntry;

  // Copy Device Images and Device Offload Entries
  __tgt_device_image *CurImage = Desc->DeviceImages;
  for (const auto &Image : Request->images()) {
    HostToRemoteDeviceImage[(void *)Image.img_ptr()] = CurImage;
    CurImage->EntriesBegin = new __tgt_offload_entry[Image.entries_size()];
    CurEntry = CurImage->EntriesBegin;

    for (const auto &Entry : Image.entries()) {
      copyOffloadEntry(Entry, CurEntry);
      CurEntry++;
    }
    CurImage->EntriesEnd = CurEntry;

    // Copy Device Image
    CurImage->ImageStart = malloc(sizeof(char) * Image.binary().size());
    memcpy(CurImage->ImageStart,
           static_cast<const void *>(Image.binary().data()),
           Image.binary().size());
    CurImage->ImageEnd =
        (void *)((char *)CurImage->ImageStart + Image.binary().size());

    CurImage++;
  }
}

void freeTargetBinaryDescription(__tgt_bin_desc *Desc) {
  __tgt_device_image *CurImage = Desc->DeviceImages;
  for (auto I = 0; I < Desc->NumDeviceImages; I++, CurImage++)
    delete[](uint64_t *) CurImage->ImageStart;

  delete[] Desc->DeviceImages;

  for (auto *Entry = Desc->HostEntriesBegin; Entry != Desc->HostEntriesEnd;
       Entry++) {
    free(Entry->name);
    free(Entry->addr);
  }

  delete[] Desc->HostEntriesBegin;
}


void loadTargetTable(const __tgt_target_table *Table,
                     transport::messages::TargetTable &TableResponse) {
  for (__tgt_offload_entry *CurEntry = Table->EntriesBegin;
       CurEntry != Table->EntriesEnd; CurEntry++) {
    // TODO: This can probably be trimmed substantially.
    auto *NewEntry = TableResponse.add_entries();
    NewEntry->set_name(CurEntry->name);
    NewEntry->set_addr((uint64_t)CurEntry->addr);
    NewEntry->set_flags(CurEntry->flags);
    NewEntry->set_reserved(CurEntry->reserved);
    NewEntry->set_size(CurEntry->size);
    TableResponse.add_entry_ptrs((int64_t)CurEntry);
  }
}

void unloadTargetTable(
    const transport::messages::TargetTable &TableResponse,
    __tgt_target_table *Table,
    std::unordered_map<void *, void *> &HostToRemoteTargetTableMap) {
  Table->EntriesBegin = new __tgt_offload_entry[TableResponse.entries_size()];

  auto *CurEntry = Table->EntriesBegin;
  for (int I = 0; I < TableResponse.entries_size(); I++) {
    copyOffloadEntry(TableResponse.entries()[I], CurEntry);
    HostToRemoteTargetTableMap[CurEntry->addr] =
        (void *)TableResponse.entry_ptrs()[I];
    CurEntry++;
  }
  Table->EntriesEnd = CurEntry;
}

void copyOffloadEntry(
    const transport::messages::TargetOffloadEntry &EntryResponse,
    __tgt_offload_entry *Entry) {
  Entry->name = strdup(EntryResponse.name().c_str());
  Entry->reserved = EntryResponse.reserved();
  Entry->flags = EntryResponse.flags();
  Entry->addr = strdup(EntryResponse.data().c_str());
  Entry->size = EntryResponse.data().size();
}

void copyOffloadEntry(
    const transport::messages::DeviceOffloadEntry &EntryResponse,
    __tgt_offload_entry *Entry) {
  Entry->name = strdup(EntryResponse.name().c_str());
  Entry->reserved = EntryResponse.reserved();
  Entry->flags = EntryResponse.flags();
  Entry->addr = (void *)EntryResponse.addr();
  Entry->size = EntryResponse.size();
}

/// We shallow copy with just the name because it is a convenient
/// identifier, we do actually just match off of the address.
void shallowCopyOffloadEntry(
    const __tgt_offload_entry *Entry,
    transport::messages::TargetOffloadEntry *EntryResponse) {
  EntryResponse->set_name(Entry->name);
}

void copyOffloadEntry(const __tgt_offload_entry *Entry,
                      transport::messages::TargetOffloadEntry *EntryResponse) {
  shallowCopyOffloadEntry(Entry, EntryResponse);
  EntryResponse->set_reserved(Entry->reserved);
  EntryResponse->set_flags(Entry->flags);
  EntryResponse->set_data(Entry->addr, Entry->size);
}

}