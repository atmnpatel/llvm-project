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


} // namespace transport::ucx
