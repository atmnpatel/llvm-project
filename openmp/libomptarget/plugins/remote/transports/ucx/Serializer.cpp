#include "Serializer.h"

#include <utility>

std::string CustomSerializerTy::I32(int32_t Value) {
  transport::ucx::custom::I32 Message(Value);
  return Message.Message;
}

int32_t CustomSerializerTy::I32(std::string Message) {
  transport::ucx::custom::I32 Response(Message);
  return Response.Value;
}

std::string CustomSerializerTy::I64(int64_t Value) {
  transport::ucx::custom::I64 Message(Value);
  return Message.Message;
}

int64_t CustomSerializerTy::I64(std::string Message) {
  transport::ucx::custom::I64 Response(Message);
  return Response.Value;
}

std::string CustomSerializerTy::TargetBinaryDescription(__tgt_bin_desc *TBD) {
  transport::ucx::custom::TargetBinaryDescription Response(TBD);
  return Response.Message;
}

__tgt_bin_desc *CustomSerializerTy::TargetBinaryDescription(
    std::string Message,
    std::unordered_map<const void *, __tgt_device_image *> &DeviceImages) {
  auto *TBD = new __tgt_bin_desc();
  transport::ucx::custom::TargetBinaryDescription Response(Message, TBD,
                                                           DeviceImages);
  return TBD;
}

std::string CustomSerializerTy::Pointer(uintptr_t Pointer) {
  transport::ucx::custom::Pointer Message(Pointer);
  return Message.Message;
}

void *CustomSerializerTy::Pointer(std::string Message) {
  transport::ucx::custom::Pointer Response(Message);
  return Response.Value;
}

std::string CustomSerializerTy::Binary(int32_t DeviceId,
                                       __tgt_device_image *Image) {
  transport::ucx::custom::Binary Response(DeviceId, Image);
  return Response.Message;
}

std::pair<int32_t, __tgt_device_image *>
CustomSerializerTy::Binary(std::string Message) {
  transport::ucx::custom::Binary Response(std::move(Message));
  return {Response.DeviceId, (__tgt_device_image *)Response.Image};
}

std::string ProtobufSerializerTy::I32(int32_t Value) {
  openmp::libomptarget::I32 Message;
  Message.set_number(Value);
  return Message.SerializeAsString();
}

int32_t ProtobufSerializerTy::I32(std::string Message) {
  openmp::libomptarget::I32 Response;
  Response.ParseFromString(Message);
  return Response.number();
}

std::string ProtobufSerializerTy::I64(int64_t Value) {
  openmp::libomptarget::I64 Message;
  Message.set_number(Value);
  return Message.SerializeAsString();
}

int64_t ProtobufSerializerTy::I64(std::string Message) {
  openmp::libomptarget::I64 Response;
  Response.ParseFromString(Message);
  return Response.number();
}

std::string ProtobufSerializerTy::TargetBinaryDescription(__tgt_bin_desc *TBD) {
  openmp::libomptarget::TargetBinaryDescription Response;
  loadTargetBinaryDescription(TBD, Response);
  return Response.SerializeAsString();
}

__tgt_bin_desc *ProtobufSerializerTy::TargetBinaryDescription(
    std::string Message,
    std::unordered_map<const void *, __tgt_device_image *> &DeviceImages) {
  openmp::libomptarget::TargetBinaryDescription Response;
  auto *TBD = new __tgt_bin_desc();
  Response.ParseFromString(Message);
  unloadTargetBinaryDescription(&Response, TBD, DeviceImages);
  return TBD;
}

std::string ProtobufSerializerTy::Pointer(uintptr_t Pointer) {
  openmp::libomptarget::Pointer Message;
  Message.set_number(Pointer);
  return Message.SerializeAsString();
}

void *ProtobufSerializerTy::Pointer(std::string Message) {
  openmp::libomptarget::Pointer Response;
  Response.ParseFromString(Message);
  return (void *)Response.number();
}

std::string ProtobufSerializerTy::Binary(int32_t DeviceId,
                                         __tgt_device_image *Image) {
  openmp::libomptarget::Binary Response;
  Response.set_device_id(DeviceId);
  Response.set_image_ptr((uintptr_t)Image);
  return Response.SerializeAsString();
}

std::pair<int32_t, __tgt_device_image *>
ProtobufSerializerTy::Binary(std::string Message) {
  openmp::libomptarget::Binary Response;
  Response.ParseFromString(Message);
  return {Response.device_id(), (__tgt_device_image *)Response.image_ptr()};
}
