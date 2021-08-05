#include "Utils.h"

namespace transport::ucx {

std::vector<std::string> MessageKindToString = {
    "RegisterLib",         "UnregisterLib", "IsValidBinary",
    "GetNumberOfDevices",  "InitDevice",    "InitRequires",
    "LoadBinary",          "DataAlloc",     "DataDelete",
    "DataSubmit",          "DataRetrieve",  "RunTargetRegion",
    "RunTargetTeamRegion", "Count"};

std::string getIP(const sockaddr_storage *SocketAddress) {
  char IP[IPStringLength];

  switch (SocketAddress->ss_family) {
  case AF_INET: {
    sockaddr_in Addr;
    memcpy(&Addr, SocketAddress, sizeof(struct sockaddr_in));
    inet_ntop(AF_INET, &Addr.sin_addr, IP, IPStringLength);
    break;
  }
  case AF_INET6: {
    sockaddr_in6 Addr;
    memcpy(&Addr, SocketAddress, sizeof(struct sockaddr_in6));
    inet_ntop(AF_INET6, &Addr.sin6_addr, IP, IPStringLength);
    break;
  }
  default:
    return "Invalid address family";
  }
  return std::string(IP);
}

std::string getPort(const sockaddr_storage *SocketAddress) {
  char Port[PortStringLength];

  switch (SocketAddress->ss_family) {
  case AF_INET: {
    sockaddr_in Addr;
    memcpy(&Addr, SocketAddress, sizeof(struct sockaddr_in));
    snprintf(Port, PortStringLength, "%d", ntohs(Addr.sin_port));
    break;
  }
  case AF_INET6: {
    sockaddr_in6 Addr;
    memcpy(&Addr, SocketAddress, sizeof(struct sockaddr_in6));
    snprintf(Port, PortStringLength, "%d", ntohs(Addr.sin6_port));
    break;
  }
  default:
    return "Invalid address family";
  }
  return std::string(Port);
}

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
  printf("  Entry (%s): %p [%ld], %d, %d\n", Entry->name, Entry->addr, Entry->size, Entry->flags, Entry->reserved);
}

void dump(__tgt_bin_desc *Desc) {
  printf("Global Entries:\n");
  for (auto *Entry = Desc->HostEntriesBegin; Entry != Desc->HostEntriesEnd; Entry++) {
    dump(Entry);
  }

  printf("Images: %d\n", Desc->NumDeviceImages);
  auto *Image = Desc->DeviceImages;
  for (auto Idx = 0; Idx < Desc->NumDeviceImages; Idx++, Image++) {
    printf("Image %d: [%ld]\n", Idx, (char *) Image->ImageEnd - (char *) Image->ImageStart);
    for (auto *Entry = Image->EntriesBegin; Entry != Image->EntriesEnd; Entry++) {
      dump(Entry);
    }
  }
}

} // namespace transport::ucx
