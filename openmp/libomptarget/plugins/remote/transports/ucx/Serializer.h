#pragma once

#include "Serialization.h"

struct SerializerTy {
  static std::string EmptyMessage() { return "0"; }

  virtual ~SerializerTy() = default;

  virtual std::string I32(int32_t Value) = 0;
  virtual int32_t I32(std::string Message) = 0;

  virtual std::string I64(int64_t Value) = 0;
  virtual int64_t I64(std::string Message) = 0;

  virtual std::string TargetBinaryDescription(__tgt_bin_desc *TBD) = 0;
  virtual __tgt_bin_desc *TargetBinaryDescription(
      std::string Message,
      std::unordered_map<const void *, __tgt_device_image *> &DeviceImages) = 0;

  virtual std::string Pointer(uintptr_t Pointer) = 0;
  virtual void *Pointer(std::string Message) = 0;

  virtual std::string Binary(int32_t DeviceId, __tgt_device_image *Image) = 0;
  virtual std::pair<int32_t, __tgt_device_image *>
  Binary(std::string Message) = 0;
};

class ProtobufSerializerTy : public SerializerTy {
  std::string I32(int32_t Value) override;
  int32_t I32(std::string Message) override;

  std::string I64(int64_t Value) override;
  int64_t I64(std::string Message) override;

  std::string TargetBinaryDescription(__tgt_bin_desc *TBD) override;
  __tgt_bin_desc *
  TargetBinaryDescription(std::string Message,
                          std::unordered_map<const void *, __tgt_device_image *>
                              &DeviceImages) override;

  std::string Pointer(uintptr_t Pointer) override;
  void *Pointer(std::string Message) override;

  std::string Binary(int32_t DeviceId, __tgt_device_image *Image) override;
  std::pair<int32_t, __tgt_device_image *> Binary(std::string Message) override;
};

class CustomSerializerTy : public SerializerTy {
  std::string I32(int32_t Value) override;
  int32_t I32(std::string Message) override;

  std::string I64(int64_t Value) override;
  int64_t I64(std::string Message) override;

  std::string TargetBinaryDescription(__tgt_bin_desc *TBD) override;
  __tgt_bin_desc *
  TargetBinaryDescription(std::string Message,
                          std::unordered_map<const void *, __tgt_device_image *>
                              &DeviceImages) override;

  std::string Pointer(uintptr_t Pointer) override;
  void *Pointer(std::string Message) override;

  std::string Binary(int32_t DeviceId, __tgt_device_image *Image) override;
  std::pair<int32_t, __tgt_device_image *> Binary(std::string Message) override;
};
