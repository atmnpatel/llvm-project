#pragma once

#include "Utils.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include "llvm/ADT/AllocatorList.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormatVariadic.h"

#include "llvm/Support/Allocator.h"

#include <forward_list>
#include <functional>
#include <list>

#include "omptarget.h"
#include "ucx.pb.h"

namespace transport {
namespace ucx {

using namespace openmp::libomptarget::ucx;

enum MessageTy : char {
  RegisterLib,
  UnregisterLib,
  IsValidBinary,
  GetNumberOfDevices,
  InitDevice,
  InitRequires,
  LoadBinary,
  IsDataExchangeable,
  ExchangeData,
  DataAlloc,
  DataDelete,
  DataSubmit,
  DataRetrieve,
  RunTargetRegion,
  RunTargetTeamRegion,
  Count
};


class CoderTy {
protected:
  enum TokenTy : int8_t { TT_VALUE, TT_INDIRECTION };

  struct Header {
    MessageTy Type;
    size_t NumSlabs;
  };

  struct SlabHeader {
    size_t SlabNum;
    size_t Size;
  };

public:
  AllocatorTy *Allocator;

  const int32_t MaxMemoryCopySize = 64 - sizeof(TokenTy);
  const int32_t HeaderSize = sizeof(MessageTy) + sizeof(size_t);
  const int32_t SlabHeaderSize = sizeof(size_t) + sizeof(size_t);

  SlabListTy MessageSlabs;
  MessageTy Type;

  CoderTy(AllocatorTy *Allocator);
  CoderTy(AllocatorTy *Allocator, MessageTy Type);

  /* Allocate slab of Size at the end of Slabs */
  SlabTy &allocateSlab(SlabListTy &Slabs, size_t Size = SlabSize);

  /* Dump slabs */
  void dumpSlabs() const;
};


template <typename T>
inline void copyValueToBuffer(char *&Buffer, const T &Value,
                              bool IncrementBuffer = true) {
  if (IncrementBuffer)
    std::memcpy((Buffer += sizeof(Value)) - sizeof(Value), &Value,
                sizeof(Value));
  else
    std::memcpy(Buffer, &Value, sizeof(Value));
}

template <typename T>
inline void copyValueFromBuffer(char *&Buffer, T &Value,
                                bool IncrementBuffer = true) {
  if (IncrementBuffer)
    std::memcpy(&Value, (Buffer += sizeof(Value)) - sizeof(Value),
                sizeof(Value));
  else
    std::memcpy(&Value, Buffer, sizeof(Value));
}

template <typename T>
inline void copyMemoryToBuffer(char *&Buffer, const T &Value, size_t Size,
                               bool IncrementBuffer = true) {
  if (IncrementBuffer)
    std::memcpy((Buffer += Size) - Size, Value, Size);
  else
    std::memcpy(Buffer, Value, Size);
}

template <typename T>
inline void copyMemoryFromBuffer(char *&Buffer, T &Value, size_t Size,
                                 bool IncrementBuffer = true) {
  if (IncrementBuffer)
    std::memcpy(Value, (Buffer += Size) - Size, Size);
  else
    std::memcpy(Value, Buffer, Size);
}

void deleteTargetBinaryDescription(__tgt_bin_desc *Desc);

/// Loads a target binary description into protobuf.
void loadTargetBinaryDescription(
    const __tgt_bin_desc *Desc,
    openmp::libomptarget::ucx::TargetBinaryDescription &Request);

/// Unload a target binary description from protobuf. The map is used to keep
/// track of already copied device images.
void unloadTargetBinaryDescription(
    const openmp::libomptarget::ucx::TargetBinaryDescription *Request,
    __tgt_bin_desc *Desc,
    std::unordered_map<const void *, __tgt_device_image *>
        &HostToRemoteDeviceImage);

/// Frees argument as constructed by loadTargetBinaryDescription
void freeTargetBinaryDescription(__tgt_bin_desc *Desc);

/// Copies from TargetOffloadEntry protobuf to a tgt_bin_desc during unloading.
void copyOffloadEntry(
    const openmp::libomptarget::ucx::TargetOffloadEntry &EntryResponse,
    __tgt_offload_entry *Entry);

/// Copies from tgt_bin_desc into TargetOffloadEntry protobuf during loading.
void copyOffloadEntry(
    const __tgt_offload_entry *Entry,
    openmp::libomptarget::ucx::TargetOffloadEntry *EntryResponse);

/// Shallow copy of offload entry from tgt_bin_desc to TargetOffloadEntry
/// during loading.
void shallowCopyOffloadEntry(
    const __tgt_offload_entry *Entry,
    openmp::libomptarget::ucx::TargetOffloadEntry *EntryResponse);

/// Copies DeviceOffloadEntries into table during unloading.
void copyOffloadEntry(const transport::ucx::DeviceOffloadEntry &EntryResponse,
                      __tgt_offload_entry *Entry);

/// Loads tgt_target_table into a TargetTable protobuf message.
void loadTargetTable(__tgt_target_table *Table, transport::ucx::TargetTable &TableResponse,
                     __tgt_device_image *Image);

/// Unloads from a target_table from protobuf.
void unloadTargetTable(
    openmp::libomptarget::ucx::TargetTable &TableResponse,
    __tgt_target_table *Table,
    std::unordered_map<void *, void *> &HostToRemoteTargetTableMap);

/// Frees argument as constructed by unloadTargetTable
void freeTargetTable(__tgt_target_table *Table);
class EncoderTy : public CoderTy {
  /* Store large memory buffers to serialize at the end */
  llvm::DenseMap<transport::ucx::MemoryTy, size_t> IndirectMemoryMap;

  /* Allocate header after serializing all args */
  void allocateHeader(MessageTy Type);

  /* Serialize slab header after serializing slab contents */
  void serializeSlabHeader(SlabTy &SlabItr);

  /* Serialize header after serializing slabs*/
  void serializeHeader(MessageTy Type, size_t NumSlabs);

  /* Allocate buffer of Size */
  char *allocate(SlabListTy &Slabs, int32_t Size);

  /* Serialize an indirect value */
  void serializeIndirect(int32_t Index, int32_t Size);

  uint64_t Size = 0;

public:
  /* Serialize all Indirect Memory Slabs after small PODs */
  void initializeIndirectMemorySlabs();

  /* Finalize slabs after serializing data */
  void finalize();

  EncoderTy(AllocatorTy *Allocator, MessageTy Type,
            bool InitializeHeader = true)
      : CoderTy(Allocator, Type) {
    if (InitializeHeader)
      allocateHeader(Type);
    else {
      allocateSlab(MessageSlabs);
    }
  }

  template <typename... Targs>
  EncoderTy(AllocatorTy *Allocator, MessageTy Type, Targs const &...Args)
      : CoderTy(Allocator, Type) {
    allocateSlab(MessageSlabs);
    serialize(*this, Args...);
    initializeIndirectMemorySlabs();
    finalize();
  }

  void serializeData(const void *Ptr, size_t Size);
  template <typename Ty> void serializeValue(const Ty &V) {
    TokenTy TT = TT_VALUE;
    char *Buffer = allocate(MessageSlabs, sizeof(TT) + sizeof(V));
    copyValueToBuffer(Buffer, TT);
    copyValueToBuffer(Buffer, V);
  }

  void serializeMemory(void *Ptr, size_t Size);

  template <typename Ty>
  void serializeList(Ty *Begin, Ty *End, bool Pointers = false) {
    int32_t NumElements = End - Begin;
    serializeValue(NumElements);
    if (Pointers)
      for (int32_t I = 0; I < NumElements; ++I)
        serializeValue((uintptr_t)Begin + I);
    for (int32_t I = 0; I < NumElements; ++I)
      serialize<Ty>(*this, Begin[I]);
  }
};
struct Decoder : public CoderTy {
  Decoder(AllocatorTy *Allocator);

  /* Helper struct for Indirect Memory Chunks */
  struct MemoryTy {
    void *Address;
    int32_t Index;
    int32_t Size;
  };

  std::vector<MemoryTy> Indirect;

  /* Parse header slab */
  size_t parseHeader(char *Buffer);

  /* Parse slab header from the buffer */
  std::pair<SlabTy, uint32_t> parseSlabHeader(char *&Buffer);

  /* Insert slab into Slab list */
  void insert(char *Buffer, char *CurBuffer, size_t Size);

  void initializeIndirect();

  template <typename T> void deserializeValue(T &Arg) {
    auto &Slab = MessageSlabs.front();
    TokenTy TT;

    copyValueFromBuffer(Slab.Cur, TT);
    assert(TT == TT_VALUE && "Can not serialize an indirection as a value");
    copyValueFromBuffer(Slab.Cur, Arg);
  }

  std::pair<char *, char *> deserializeMemory();

  template <typename T>
  std::vector<void *> deserializeList(T &Begin, T &End, bool Pointers = false) {
    int32_t NumElements;
    deserializeValue(NumElements);
    // printf("Num Elements: %d\n", NumElements);
    Begin = (T)Allocator->Allocate(sizeof(*Begin) * NumElements, 4);
    std::vector<void *> Ptrs;
    if (Pointers) {
      for (int32_t I = 0; I < NumElements; ++I) {
        uintptr_t Ptr;
        deserializeValue<uintptr_t>(Ptr);
        Ptrs.push_back((void *)Ptr);
        // printf("%p\n", (void *)Ptr);
      }
    }
    auto *CurPtr = Begin;
    for (int32_t I = 0; I < NumElements; ++I, CurPtr++)
      deserialize<T>(*this, CurPtr);
    End = CurPtr;
    return Ptrs;
  }

  template <typename T>
  std::vector<void *> deserializeList(T &Begin, int32_t &NumElements,
                                      bool Pointers = false) {
    deserializeValue(NumElements);
    Begin = (T)Allocator->Allocate(sizeof(*Begin) * NumElements, 4);
    std::vector<void *> Ptrs;
    if (Pointers) {
      for (int32_t I = 0; I < NumElements; ++I) {
        uintptr_t Ptr;
        deserializeValue<uintptr_t>(Ptr);
        Ptrs.push_back((void *)Ptr);
      }
    }
    auto CurPtr = Begin;
    for (int32_t I = 0; I < NumElements; ++I, CurPtr++)
      deserialize<T>(*this, CurPtr);
    return Ptrs;
  }
};

template <typename T> void deserialize(Decoder &D, T &Arg) {
  if (std::is_integral<T>::value)
    D.deserializeValue(Arg);
  else
    deserialize<T>(D, Arg);
}

template <typename TFirst, typename TSecond, typename... Targs>
void deserialize(Decoder &E, TFirst &FirstArg, TSecond &SecondArg,
                 Targs &...Args) {
  deserialize<TFirst>(E, FirstArg);
  deserialize(E, SecondArg, Args...);
}

template <>
inline void deserialize<__tgt_offload_entry *>(Decoder &D,
                                               __tgt_offload_entry *&TOE) {
  D.deserializeValue<int32_t>(TOE->reserved);
  D.deserializeValue<int32_t>(TOE->flags);
  D.deserializeValue<size_t>(TOE->size);
  TOE->name = (char *)D.deserializeMemory().first;
  if (TOE->size)
    TOE->addr = (char *)D.deserializeMemory().first;
  else
    D.deserializeValue(TOE->addr);
}

template <>
inline void deserialize<__tgt_device_image *>(Decoder &D,
                                              __tgt_device_image *&TDI) {
  D.deserializeList(TDI->EntriesBegin, TDI->EntriesEnd);
  std::tie(TDI->ImageStart, TDI->ImageEnd) = D.deserializeMemory();
}

template <>
inline void deserialize<DataSubmitTy *>(Decoder &D,
                                        DataSubmitTy *&DSA) {
  D.deserializeValue(DSA->DeviceId);
  D.deserializeValue(DSA->TgtPtr);
  void *EndPtr;
  std::tie(DSA->HstPtr, EndPtr) = D.deserializeMemory();
  DSA->Size = (char *)EndPtr - (char *)DSA->HstPtr;
}

template <typename T> void serialize(EncoderTy &E, T const &Arg) {
  if (std::is_integral<T>::value)
    E.serializeValue(Arg);
  else
    serialize<T>(E, Arg);
}

template <typename TFirst, typename TSecond, typename... Targs>
void serialize(EncoderTy &E, TFirst const &FirstArg, TSecond const &SecondArg,
               Targs const &...Args) {
  serialize<TFirst>(E, FirstArg);
  serialize(E, SecondArg, Args...);
}

template <>
inline void serialize<__tgt_offload_entry>(EncoderTy &E,
                                           const __tgt_offload_entry &TOE) {
  E.serializeValue(TOE.reserved);
  E.serializeValue(TOE.flags);
  E.serializeValue(TOE.size);
  E.serializeMemory(TOE.name, llvm::StringRef(TOE.name).size() + 1);
  if (TOE.size)
    E.serializeMemory(TOE.addr, TOE.size);
  else
    E.serializeValue((uintptr_t)TOE.addr);
}

template <>
inline void serialize<__tgt_device_image>(EncoderTy &E,
                                          const __tgt_device_image &TDI) {
  E.serializeList(TDI.EntriesBegin, TDI.EntriesEnd);
  E.serializeMemory(TDI.ImageStart,
                    ((char *)(TDI.ImageEnd) - (char *)TDI.ImageStart));
}

template <>
inline void serialize<__tgt_bin_desc>(EncoderTy &E, const __tgt_bin_desc &TBD) {
  E.serializeList(TBD.HostEntriesBegin, TBD.HostEntriesEnd);
  E.serializeValue(TBD.NumDeviceImages);
  E.serializeList(TBD.DeviceImages, TBD.DeviceImages + TBD.NumDeviceImages,
                  true);
}

inline void serializeTT(EncoderTy &E, __tgt_target_table *TT) {
  E.serializeList(TT->EntriesBegin, TT->EntriesEnd, false);
}

template <>
inline void serialize<DataSubmitTy>(EncoderTy &E,
                                    const DataSubmitTy &DA) {
  E.serializeValue(DA.DeviceId);
  E.serializeValue(DA.TgtPtr);
  E.serializeMemory(DA.HstPtr, DA.Size);
}

template <>
inline void
serialize<RunTargetTeamRegionTy>(EncoderTy &E,
                                 const RunTargetTeamRegionTy &TTRA) {
  E.serializeValue(TTRA.DeviceId);
  E.serializeValue((uintptr_t)TTRA.TgtEntryPtr);
  E.serializeValue(TTRA.ArgNum);
  for (auto I = 0; I < TTRA.ArgNum; I++)
    E.serializeValue((uintptr_t) * (TTRA.TgtArgs + I));
  for (auto I = 0; I < TTRA.ArgNum; I++)
    E.serializeValue((uintptr_t)(TTRA.TgtOffsets[I]));
  E.serializeValue(TTRA.TeamNum);
  E.serializeValue(TTRA.ThreadLimit);
  E.serializeValue(TTRA.LoopTripCount);
}

template <>
inline void
serialize<RunTargetRegionTy>(EncoderTy &E,
                             const RunTargetRegionTy &TRA) {

  E.serializeValue(TRA.DeviceId);
  E.serializeValue((uintptr_t)TRA.TgtEntryPtr);
  E.serializeValue(TRA.ArgNum);
  for (auto I = 0; I < TRA.ArgNum; I++)
    E.serializeValue((uintptr_t) * (TRA.TgtArgs + I));
  for (auto I = 0; I < TRA.ArgNum; I++)
    E.serializeValue((uintptr_t)(TRA.TgtOffsets[I]));
}

} // namespace ucx
} // namespace transport
