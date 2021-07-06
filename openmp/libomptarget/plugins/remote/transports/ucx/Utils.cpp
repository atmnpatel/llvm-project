#include "Utils.h"

namespace transport {
namespace ucx {

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

void print_result(std::string Message, bool Sent) {
  printf("\n\n-----------------------------------------\n\n");
  printf("%s Message: %s.\nLength: %ld\n", (Sent ? "Sent " : "Recieved "),
         (Message.size() != 0) ? Message.c_str() : "<none>", Message.size());
  printf("\n-----------------------------------------\n\n");
}

void dump(int Offset, char *Begin, char *End) {
  printf("(dec) %d:  ", Offset);
  for (char *Itr = Begin; Itr != End; Itr++) {
    printf(" %d", *Itr);
  }
  printf("\n");

  printf("(hex) %d:  ", Offset);
  for (char *Itr = Begin; Itr != End; Itr++) {
    printf(" %x", *Itr);
  }
  printf("\n");

  printf("(asc) %d:  ", Offset);
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

void dump(char *Begin, char *End, const std::string &Title) {
  if (DUMP) {
    printf("======================= %s =======================\n",
           Title.c_str());
    for (size_t offset = 0; offset < End - Begin; offset += 16)
      dump(offset, Begin + offset, std::min(Begin + offset + 16, End));
  }
}

} // namespace ucx
} // namespace transport
