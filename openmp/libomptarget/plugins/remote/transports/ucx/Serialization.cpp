#include "Serialization.h"
#include "Utils.h"
#include "omptarget.h"
#include <cstddef>
#include <cstdint>

namespace transport::ucx {

namespace custom {

MessageTy::MessageTy(size_t Size)
    : MessageSize(Size) {
  Message = new char[Size];
  CurBuffer = Message;
}

MessageTy::MessageTy(std::string_view M)
    : CurBuffer(const_cast<char*>(M.data())) {
  }

  void MessageTy::serialize(uintptr_t Value) {
  std::memcpy((void *)((CurBuffer += sizeof(Value)) - sizeof(Value)), &Value,
              sizeof(Value));
}

void MessageTy::serialize(void *BufferStart, void *BufferEnd) {
  size_t BufferSize = ((uintptr_t)BufferEnd - (uintptr_t)BufferStart);
  std::memcpy((void *)((CurBuffer += sizeof(BufferSize)) - sizeof(BufferSize)),
              &BufferSize, sizeof(BufferSize));
  std::memcpy((void *)((CurBuffer += BufferSize) - BufferSize), BufferStart,
              BufferSize);
}

void MessageTy::serialize(char *String) {
  serialize(String, String + strlen(String));
}

void MessageTy::serialize(__tgt_offload_entry *Entry) {
  serialize(Entry->name);
  serialize((uintptr_t)Entry->addr);
  serialize(Entry->size);
  if (Entry->size)
    serialize(Entry->addr, (void *)((uintptr_t)Entry->addr + Entry->size));
  serialize(Entry->flags);
  serialize(Entry->reserved);
}

void MessageTy::serialize(__tgt_device_image *Image) {
  auto NumEntries = 0;
  for (auto *CurEntry = Image->EntriesBegin; CurEntry != Image->EntriesEnd;
       CurEntry++, NumEntries++)
    ;
  serialize(NumEntries);

  for (auto *CurEntry = Image->EntriesBegin; CurEntry != Image->EntriesEnd;
       CurEntry++)
    serialize(CurEntry);

  serialize(Image->ImageStart, Image->ImageEnd);
}

void *MessageTy::deserializePointer() {
  void *Pointer = nullptr;
  std::memcpy(&Pointer, (CurBuffer += sizeof(Pointer)) - sizeof(Pointer),
              sizeof(Pointer));
  return Pointer;
}

void MessageTy::deserialize(void *&BufferStart, void *&BufferEnd) {
  size_t StrSize = 0;
  deserialize(StrSize);
  BufferStart = new char[StrSize];
  std::memcpy(BufferStart, (CurBuffer += StrSize) - StrSize, StrSize);
  BufferEnd = (void *)((uintptr_t)BufferStart + StrSize);
}

void MessageTy::deserialize(char *&String) {
  void *BufferStart = (void *)String;
  void *BufferEnd = nullptr;
  size_t StrSize = 0;
  deserialize(StrSize);
  BufferStart = new char[StrSize + 1];
  std::memcpy(BufferStart, (CurBuffer += StrSize) - StrSize, StrSize);
  ((char *)BufferStart)[StrSize] = '\0';
  BufferEnd = (void *)((uintptr_t)BufferStart + StrSize + 1);
  String = (char *)BufferStart;
}

void MessageTy::deserialize(__tgt_offload_entry *&Entry) {
  deserialize(Entry->name);
  Entry->addr = deserializePointer();
  deserialize(Entry->size);
  if (Entry->size) {
    void *End = nullptr;
    deserialize(Entry->addr, End);
  }
  deserialize(Entry->flags);
  deserialize(Entry->reserved);
}

void MessageTy::deserialize(__tgt_device_image *&Image) {
  int32_t NumEntries = 0;
  deserialize(NumEntries);

  Image->EntriesBegin = new __tgt_offload_entry[NumEntries];
  Image->EntriesEnd = &Image->EntriesBegin[NumEntries];
  for (auto *CurEntry = Image->EntriesBegin; CurEntry != Image->EntriesEnd;
       CurEntry++) {
    deserialize(CurEntry);
  }

  deserialize(Image->ImageStart, Image->ImageEnd);
}

I32::I32(int32_t Value) : MessageTy(sizeof(Value)), Value(Value) {
  serialize(Value);
}

I32::I32(std::string_view MessageBuffer) : MessageTy(MessageBuffer) {
  Value = *((int32_t *)CurBuffer);
}

I64::I64(int64_t Value) : MessageTy(sizeof(Value)) { serialize(Value); }

I64::I64(std::string_view MessageBuffer) : MessageTy(MessageBuffer) {
  std::memcpy(&Value, CurBuffer, sizeof(int64_t));
}

Pointer::Pointer(uintptr_t Value) : MessageTy(sizeof(Value)) {
  serialize(Value);
}

Pointer::Pointer(std::string_view MessageBuffer) : MessageTy(MessageBuffer) {
  std::memcpy(&Value, CurBuffer, sizeof(uintptr_t));
}

TargetBinaryDescription::TargetBinaryDescription(__tgt_bin_desc *Description) {
  int32_t NumEntries = 0;
  MessageSize += sizeof(NumEntries);

  // Compute size of __tgt_offload_entries
  for (auto *CurEntry = Description->HostEntriesBegin;
       CurEntry != Description->HostEntriesEnd; CurEntry++) {
    MessageSize += sizeof(size_t) + strlen(CurEntry->name) +
                   sizeof(CurEntry->addr) + sizeof(CurEntry->size) +
                   (CurEntry->size ? CurEntry->size : 0) +
                   sizeof(CurEntry->flags) + sizeof(CurEntry->reserved);
  }

  // Compute size of __tgt_device_images
  auto *CurImage = Description->DeviceImages;
  MessageSize += sizeof(Description->NumDeviceImages);
  for (auto I = 0; I < Description->NumDeviceImages; I++, CurImage++) {
    MessageSize += sizeof(uintptr_t);

    MessageSize +=
        (uintptr_t)CurImage->ImageEnd - (uintptr_t)CurImage->ImageStart;
    MessageSize += sizeof(size_t);

    for (auto *CurEntry = CurImage->EntriesBegin;
         CurEntry != CurImage->EntriesEnd; CurEntry++)
      MessageSize += sizeof(size_t) + strlen(CurEntry->name) +
                     sizeof(CurEntry->addr) + sizeof(CurEntry->size) +
                     (CurEntry->size ? CurEntry->size : 0) +
                     sizeof(CurEntry->flags) + sizeof(CurEntry->reserved);
    MessageSize += sizeof(NumEntries);
  }

  Message = new char[MessageSize];
  CurBuffer = Message;

  // Find number of entries
  NumEntries = 0;
  for (auto *CurEntry = Description->HostEntriesBegin;
       CurEntry != Description->HostEntriesEnd; CurEntry++)
    NumEntries++;

  // Serialize host entries
  serialize(NumEntries);
  for (auto *CurEntry = Description->HostEntriesBegin;
       CurEntry != Description->HostEntriesEnd; CurEntry++)
    serialize(CurEntry);

  // Serialize device images
  serialize(Description->NumDeviceImages);
  CurImage = Description->DeviceImages;
  for (auto I = 0; I < Description->NumDeviceImages; I++, CurImage++) {
    serialize((uintptr_t)CurImage);
    serialize(CurImage);
  }
}

TargetBinaryDescription::TargetBinaryDescription(
    std::string_view MessageBuffer, __tgt_bin_desc *Description,
    std::unordered_map<const void *, __tgt_device_image *>
        &HostToRemoteDeviceImage)
    : MessageTy(MessageBuffer) {
  int32_t NumEntries;
  deserialize(NumEntries);

  Description->HostEntriesBegin = new __tgt_offload_entry[NumEntries];
  Description->HostEntriesEnd = &Description->HostEntriesBegin[NumEntries];

  for (auto *CurEntry = Description->HostEntriesBegin;
       CurEntry != Description->HostEntriesEnd; CurEntry++)
    deserialize(CurEntry);

  deserialize(Description->NumDeviceImages);

  Description->DeviceImages =
      new __tgt_device_image[Description->NumDeviceImages];

  std::vector<void *> ImagePtrs;
  auto *Image = Description->DeviceImages;
  for (auto I = 0; I < Description->NumDeviceImages; I++, Image++) {
    void *Ptr = deserializePointer();
    ImagePtrs.push_back(Ptr);
    deserialize(Image);
    HostToRemoteDeviceImage[Ptr] = Image;
  }
}

Binary::Binary(int32_t DeviceId, __tgt_device_image *Image)
    : MessageTy(sizeof(DeviceId) + sizeof(Image)) {
  serialize(DeviceId);
  serialize((uintptr_t)Image);
}

Binary::Binary(std::string_view MessageBuffer) : MessageTy(MessageBuffer) {
  deserialize(DeviceId);
  Image = deserializePointer();
}

TargetTable::TargetTable(__tgt_target_table *Table) {
  int32_t NumEntries = 0;
  MessageSize += sizeof(NumEntries);

  // Compute size of __tgt_offload_entries
  for (auto *CurEntry = Table->EntriesBegin; CurEntry != Table->EntriesEnd;
       CurEntry++, NumEntries++) {
    MessageSize += sizeof(size_t) + strlen(CurEntry->name) +
                   sizeof(CurEntry->addr) + sizeof(CurEntry->size) +
                   (CurEntry->size ? CurEntry->size : 0) +
                   sizeof(CurEntry->flags) + sizeof(CurEntry->reserved);
  }

  Message = new char[MessageSize];
  CurBuffer = Message;

  serialize(NumEntries);
  for (auto *CurEntry = Table->EntriesBegin; CurEntry != Table->EntriesEnd;
       CurEntry++)
    serialize(CurEntry);
}

TargetTable::TargetTable(std::string_view MessageBuffer)
    : MessageTy(MessageBuffer) {
  Table = new __tgt_target_table;

  int NumEntries;
  deserialize(NumEntries);

  Table->EntriesBegin = new __tgt_offload_entry[NumEntries];
  Table->EntriesEnd = Table->EntriesBegin + NumEntries;

  for (auto *CurEntry = Table->EntriesBegin; CurEntry != Table->EntriesEnd;
       CurEntry++)
    deserialize(CurEntry);
}

DataAlloc::DataAlloc(int32_t DeviceId, int64_t AllocSize, void *HstPtr) {
  MessageSize = sizeof(DeviceId) + sizeof(AllocSize) + sizeof(HstPtr);
  Message = new char[MessageSize];
  CurBuffer = Message;
  serialize(DeviceId);
  serialize(AllocSize);
  serialize((uintptr_t)HstPtr);
}

DataAlloc::DataAlloc(std::string_view MessageBuffer)
    : MessageTy(MessageBuffer) {
  deserialize(DeviceId);
  deserialize(AllocSize);
  HstPtr = deserializePointer();
}

DataDelete::DataDelete(int32_t DeviceId, void *TgtPtr)
    : MessageTy(sizeof(DeviceId) + sizeof(TgtPtr)) {
  serialize(DeviceId);
  serialize((uintptr_t)TgtPtr);
}

DataDelete::DataDelete(std::string_view MessageBuffer)
    : MessageTy(MessageBuffer) {
  deserialize(DeviceId);
  TgtPtr = deserializePointer();
}

DataSubmit::DataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                       int64_t DataSize)
    : MessageTy(sizeof(DeviceId) + sizeof(uintptr_t) + sizeof(DataSize) +
                DataSize) {
  serialize(DeviceId);
  serialize((uintptr_t)TgtPtr);
  serialize((void *)HstPtr, (void *)((char *)HstPtr + DataSize));
}

DataSubmit::DataSubmit(std::string_view MessageBuffer)
    : MessageTy(MessageBuffer) {
  deserialize(DeviceId);
  TgtPtr = deserializePointer();
  HstPtr = nullptr;
  void *EndPtr = nullptr;
  deserialize(HstPtr, EndPtr);
  DataSize = (uintptr_t)EndPtr - (uintptr_t)HstPtr;
}

DataRetrieve::DataRetrieve(int32_t DeviceId, void *TgtPtr,
                           int64_t DataSize)
    : MessageTy(sizeof(DeviceId) + sizeof(TgtPtr) +
                sizeof(DataSize)) {
  serialize(DeviceId);
  serialize((uintptr_t)TgtPtr);
  serialize(DataSize);
}

DataRetrieve::DataRetrieve(std::string_view MessageBuffer)
    : MessageTy(MessageBuffer) {
  deserialize(DeviceId);
  TgtPtr = deserializePointer();
  deserialize(DataSize);
}

Data::Data(int32_t Value, char *Buffer, size_t DataSize)
    : MessageTy(sizeof(DataSize) + DataSize + sizeof(Value)) {
  serialize(Value);
  serialize(Buffer, Buffer + DataSize);
}

Data::Data(std::string_view MessageBuffer) : MessageTy(MessageBuffer) {
  deserialize(Value);
  DataBuffer = nullptr;
  void *BufferEnd = nullptr;
  deserialize(DataBuffer, BufferEnd);
  DataSize = (uintptr_t)BufferEnd - (uintptr_t)DataBuffer;
}

TargetRegion::TargetRegion(int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs,
                           ptrdiff_t *TgtOffsets, int32_t ArgNum)
    : MessageTy(sizeof(DeviceId) + sizeof(TgtEntryPtr) + sizeof(ArgNum) +
                sizeof(*TgtArgs) * ArgNum + sizeof(ptrdiff_t) * ArgNum) {
  serialize(DeviceId);
  serialize((uintptr_t)TgtEntryPtr);
  serialize(ArgNum);
  for (auto I = 0; I < ArgNum; I++)
    serialize((uintptr_t)TgtArgs[I]);
  for (auto I = 0; I < ArgNum; I++)
    serialize(TgtOffsets[I]);
}

TargetRegion::TargetRegion(std::string_view MessageBuffer)
    : MessageTy(MessageBuffer) {
  deserialize(DeviceId);
  TgtEntryPtr = deserializePointer();
  deserialize(ArgNum);
  TgtArgs = new void *[ArgNum];
  TgtOffsets = new ptrdiff_t[ArgNum];
  for (auto I = 0; I < ArgNum; I++)
    TgtArgs[I] = deserializePointer();
  for (auto I = 0; I < ArgNum; I++)
    deserialize(TgtOffsets[I]);
}

TargetTeamRegion::TargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr,
                                   void **TgtArgs, ptrdiff_t *TgtOffsets,
                                   int32_t ArgNum, int32_t TeamNum,
                                   int32_t ThreadLimit, uint64_t LoopTripCount)
    : MessageTy(sizeof(DeviceId) + sizeof(TgtEntryPtr) + sizeof(ArgNum) +
                sizeof(*TgtArgs) * ArgNum + sizeof(ptrdiff_t) * ArgNum +
                sizeof(TeamNum) + sizeof(ThreadLimit) + sizeof(LoopTripCount)) {
  serialize(DeviceId);
  serialize((uintptr_t)TgtEntryPtr);
  serialize(ArgNum);
  for (auto I = 0; I < ArgNum; I++)
    serialize((uintptr_t)TgtArgs[I]);
  for (auto I = 0; I < ArgNum; I++)
    serialize(TgtOffsets[I]);
  serialize(TeamNum);
  serialize(ThreadLimit);
  serialize(LoopTripCount);
}

TargetTeamRegion::TargetTeamRegion(std::string_view MessageBuffer)
    : MessageTy(MessageBuffer) {
  deserialize(DeviceId);
  TgtEntryPtr = deserializePointer();
  deserialize(ArgNum);
  TgtArgs = new void *[ArgNum];
  TgtOffsets = new ptrdiff_t[ArgNum];
  for (auto I = 0; I < ArgNum; I++)
    TgtArgs[I] = deserializePointer();
  for (auto I = 0; I < ArgNum; I++)
    deserialize(TgtOffsets[I]);
  deserialize(TeamNum);
  deserialize(ThreadLimit);
  deserialize(LoopTripCount);
}
} // namespace custom

} // namespace transport::ucx
