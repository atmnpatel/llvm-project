#include "Serialization.h"
#include "Utils.h"
#include <cstddef>

namespace transport {
namespace ucx {

CoderTy::CoderTy(AllocatorTy *Allocator)
    : Allocator(Allocator), Type(MessageTy::Count) {}
CoderTy::CoderTy(AllocatorTy *Allocator, MessageTy T)
    : Allocator(Allocator), Type(T) {}

void CoderTy::dumpSlabs() const {
  if (DUMP) {
    size_t I = 0;
    for (auto &Slab : MessageSlabs) {
      printf("Slab: %p, %p, %ld\n", Slab.Begin, Slab.Cur, Slab.Size);
      dump(Slab.Begin, Slab.Begin + Slab.Size, "Message " + std::to_string(I));
      I++;
    }
  }
}

SlabTy &CoderTy::allocateSlab(SlabListTy &Slabs, size_t Size) {
  auto *Begin = (char *)Allocator->Allocate(Size, 4);
  size_t Index = Slabs.size() + 1;
  Slabs.emplace_back(Begin, Begin);

  auto &SlabItr = Slabs.back();

  copyValueToBuffer(SlabItr.Cur, Index);
  SlabItr.Size += sizeof(size_t);

  // Skip slab size fields
  SlabItr.Cur += sizeof(size_t);
  SlabItr.Size += sizeof(size_t);

  return Slabs.back();
}

void loadTargetBinaryDescription(const __tgt_bin_desc *Desc,
                                 TargetBinaryDescription &Request) {
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
    const TargetBinaryDescription *Request, __tgt_bin_desc *Desc,
    std::unordered_map<const void *, __tgt_device_image *>
    &HostToRemoteDeviceImage) {
  Desc->NumDeviceImages = Request->images_size();
  Desc->DeviceImages = new __tgt_device_image[Desc->NumDeviceImages];
  Desc->HostEntriesBegin = new __tgt_offload_entry[Request->entries_size()];

  // Copy Global Offload Entries
  __tgt_offload_entry *CurEntry = Desc->HostEntriesBegin;
  for (auto Entry : Request->entries()) {
    copyOffloadEntry(Entry, CurEntry);
    CurEntry++;
  }
  Desc->HostEntriesEnd = CurEntry;

  // Copy Device Images and Device Offload Entries
  __tgt_device_image *CurImage = Desc->DeviceImages;
  for (auto Image : Request->images()) {
    HostToRemoteDeviceImage[(void *)Image.img_ptr()] = CurImage;

    CurImage->EntriesBegin = new __tgt_offload_entry[Image.entries_size()];
    CurEntry = CurImage->EntriesBegin;

    for (auto Entry : Image.entries()) {
      copyOffloadEntry(Entry, CurEntry);
      CurEntry++;
    }
    CurImage->EntriesEnd = CurEntry;

    // Copy Device Image
    CurImage->ImageStart = new char[Image.binary().size()];
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

void freeTargetTable(__tgt_target_table *Table) {
  for (auto *Entry = Table->EntriesBegin; Entry != Table->EntriesEnd; Entry++)
    free(Entry->name);

  delete[] Table->EntriesBegin;
}

void loadTargetTable(__tgt_target_table *Table, TargetTable &TableResponse,
                     __tgt_device_image *Image) {
  auto *ImageEntry = Image->EntriesBegin;
  for (__tgt_offload_entry *CurEntry = Table->EntriesBegin;
       CurEntry != Table->EntriesEnd; CurEntry++, ImageEntry++) {
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
    TargetTable &TableResponse, __tgt_target_table *Table,
    std::unordered_map<void *, void *> &HostToRemoteTargetTableMap) {
  Table->EntriesBegin = new __tgt_offload_entry[TableResponse.entries_size()];

  auto *CurEntry = Table->EntriesBegin;
  for (int i = 0; i < TableResponse.entries_size(); i++) {
    copyOffloadEntry(TableResponse.entries()[i], CurEntry);
    HostToRemoteTargetTableMap[CurEntry->addr] =
        (void *)TableResponse.entry_ptrs()[i];
    CurEntry++;
  }
  Table->EntriesEnd = CurEntry;
}

void copyOffloadEntry(const TargetOffloadEntry &EntryResponse,
                      __tgt_offload_entry *Entry) {
  Entry->name = strdup(EntryResponse.name().c_str());
  Entry->reserved = EntryResponse.reserved();
  Entry->flags = EntryResponse.flags();
  Entry->addr = strdup(EntryResponse.data().c_str());
  Entry->size = EntryResponse.data().size();
}

void copyOffloadEntry(const DeviceOffloadEntry &EntryResponse,
                      __tgt_offload_entry *Entry) {
  Entry->name = strdup(EntryResponse.name().c_str());
  Entry->reserved = EntryResponse.reserved();
  Entry->flags = EntryResponse.flags();
  Entry->addr = (void *)EntryResponse.addr();
  Entry->size = EntryResponse.size();
}

/// We shallow copy with just the name because it is a convenient
/// identifier, we do actually just match off of the address.
void shallowCopyOffloadEntry(const __tgt_offload_entry *Entry,
                             TargetOffloadEntry *EntryResponse) {
  EntryResponse->set_name(Entry->name);
}

void copyOffloadEntry(const __tgt_offload_entry *Entry,
                      TargetOffloadEntry *EntryResponse) {
  shallowCopyOffloadEntry(Entry, EntryResponse);
  EntryResponse->set_reserved(Entry->reserved);
  EntryResponse->set_flags(Entry->flags);
  EntryResponse->set_data(Entry->addr, Entry->size);
}
Decoder::Decoder(AllocatorTy *Allocator) : CoderTy(Allocator)  {}

void Decoder::initializeIndirect() {
  sort(
      Indirect.begin(), Indirect.end(),
      [](const MemoryTy &A, const MemoryTy &B) { return A.Index > B.Index; });

  for (auto &Entry : Indirect) {
    size_t SizeLeft = Entry.Size;
    auto *CurBuffer = (char *)Entry.Address;
    while (SizeLeft > 0) {
      auto &Slab = MessageSlabs.front();
      size_t BufferSpaceLeft =
          Slab.Size - ((char *)Slab.Cur - (char *)Slab.Begin);

      if (BufferSpaceLeft == 0) {
        MessageSlabs.pop_front();
      }

      size_t SizeToCopy = std::min(SizeLeft, BufferSpaceLeft);
      copyMemoryFromBuffer(Slab.Cur, CurBuffer, SizeToCopy);
      CurBuffer += SizeToCopy;
      SizeLeft -= SizeToCopy;
    }
  }
}

std::pair<char *, char *> Decoder::deserializeMemory() {
  auto &Slab = MessageSlabs.front();
  size_t Size;
  TokenTy TT;

  std::memcpy(&TT, (Slab.Cur += sizeof(TT)) - sizeof(TT), sizeof(TT));

  if (TT == TT_VALUE) {
    std::memcpy(&Size, (Slab.Cur += sizeof(Size)) - sizeof(Size),
                sizeof(Size));

    auto *Buffer = (char *)Allocator->Allocate(Size, 4);
    std::memcpy(Buffer, (Slab.Cur += Size) - Size, Size);
    return {Buffer, Buffer + Size};
  }

  if (TT == TT_INDIRECTION) {
    int32_t Index;
    int32_t Size;

    copyValueFromBuffer(Slab.Cur, Index);
    copyValueFromBuffer(Slab.Cur, Size);

    auto *Buffer = (char *)Allocator->Allocate(Size, 4);

    //printf("Found Indirect: %p, %d, %d\n", Buffer, Index, Size);
    Indirect.push_back({Buffer, Index, Size});
    return {Buffer, Buffer + Size};
  }

  return {nullptr, nullptr};
}

size_t Decoder::parseHeader(char *Buffer) {
  size_t NumSlabs;

  copyValueFromBuffer(Buffer, Type);
  copyValueFromBuffer(Buffer, NumSlabs);

  return NumSlabs;
}

std::pair<SlabTy, uint32_t> Decoder::parseSlabHeader(char *&Buffer) {
  size_t SlabNum, Size;
  char *CurBuffer = Buffer;

  copyValueFromBuffer(CurBuffer, SlabNum);
  copyValueFromBuffer(CurBuffer, Size);

  return {{Buffer, CurBuffer, Size}, SlabNum};
}

void Decoder::insert(char *Buffer, char *CurBuffer, size_t Size) {
  MessageSlabs.emplace_back(Buffer, CurBuffer, Size);
}
void EncoderTy::serializeData(const void *Ptr, size_t Size) {
  TokenTy TT = TT_VALUE;
  char *Buffer = allocate(MessageSlabs, sizeof(TT) + sizeof(Size) + Size);
  copyValueToBuffer(Buffer, TT);
  copyValueToBuffer(Buffer, Size);
  copyMemoryToBuffer(Buffer, Ptr, Size);
}

void EncoderTy::serializeMemory(void *Ptr, size_t Size) {
  if (Size < MaxMemoryCopySize) {
    serializeData(Ptr, Size);
    return;
  }
  auto Index = IndirectMemoryMap.size();
  //printf("Indirect: %u, %ld\n", Index, Size);
  Index = IndirectMemoryMap.insert({MemoryTy{Ptr, Size}, Index}).first->second;
  serializeIndirect(Index, Size);
}

void EncoderTy::finalize() {
  for (auto &Slab : MessageSlabs) {
    serializeSlabHeader(Slab);
    Size += Slab.Size;
  }
  allocateHeader(Type);
}

void EncoderTy::serializeSlabHeader(SlabTy &SlabItr) {
  if (SlabItr.Size == 0)
    return;
  std::memcpy(SlabItr.Begin + sizeof(size_t), &SlabItr.Size, sizeof(SlabItr.Size));
}

void EncoderTy::serializeHeader(MessageTy Type, size_t NumSlabs) {
  auto *Begin = (char *)Allocator->Allocate(HeaderSize, 4);
  MessageSlabs.emplace_front(Begin, Begin);

  auto &Slab = MessageSlabs.front();
  copyValueToBuffer(Slab.Cur, Type);
  copyValueToBuffer(Slab.Cur, NumSlabs);

  Slab.Size = HeaderSize;
}

void EncoderTy::allocateHeader(MessageTy Type) {
  auto NumSlabs = MessageSlabs.size() + 1;
  serializeHeader(Type, NumSlabs);
}

char *EncoderTy::allocate(SlabListTy &Slabs, int32_t Size) {
  SlabTy &Slab = Slabs.back();
  if ((Slab.Cur - Slab.Begin) + Size > SlabSize) {
    Slab = allocateSlab(Slabs);
  }
  Slab.Cur += Size;
  Slab.Size += Size;
  return Slab.Cur - Size;
}

void EncoderTy::serializeIndirect(int32_t Index, int32_t Size) {
  TokenTy TT = TT_INDIRECTION;
  auto *Buffer =
      allocate(MessageSlabs, sizeof(Index) + sizeof(TokenTy) + sizeof(Size));
  copyValueToBuffer(Buffer, TT);
  copyValueToBuffer(Buffer, Index);
  copyValueToBuffer(Buffer, Size);
}

void EncoderTy::initializeIndirectMemorySlabs() {
  for (auto &It : IndirectMemoryMap) {
    auto SizeLeft = It.first.Size;
    auto *CurBuffer = (char *)It.first.Addr;
    while (SizeLeft > 0) {
      auto &Slab = MessageSlabs.back();
      size_t BufferSpaceLeft =
          SlabSize - ((char *)Slab.Cur - (char *)Slab.Begin);

      if (BufferSpaceLeft == 0) {
        auto *Buffer = (char *)Allocator->Allocate(SlabSize, 4);
        MessageSlabs.emplace_back(Buffer, Buffer, 0);
        size_t Index = MessageSlabs.size();
        auto &Slab = MessageSlabs.back();
        std::memcpy(Slab.Cur, &Index, sizeof(Index));
        Slab.Cur += SlabHeaderSize;
        Slab.Size += SlabHeaderSize;
        continue;
      }

      auto SizeToCopy = std::min(SizeLeft, BufferSpaceLeft);
      copyMemoryToBuffer(Slab.Cur, CurBuffer, SizeToCopy);
      Slab.Size += SizeToCopy;
      CurBuffer += SizeToCopy;
      SizeLeft -= SizeToCopy;
    }
  }
}

} // namespace ucx
} // namespace transport
