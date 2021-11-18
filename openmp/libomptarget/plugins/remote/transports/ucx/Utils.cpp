#include "Utils.h"

namespace transport::ucx {

std::vector<std::string> MessageKindToString = {
    "RegisterLib",         "UnregisterLib", "IsValidBinary",
    "GetNumberOfDevices",  "InitDevice",    "InitRequires",
    "LoadBinary",          "DataAlloc",     "DataDelete",
    "DataSubmit",          "DataRetrieve",  "RunTargetRegion",
    "RunTargetTeamRegion", "Count"};

ConnectionConfigTy::ConnectionConfigTy(std::string Addr) {
  const std::string Delimiter = ":";
  size_t Pos;
  if ((Pos = Addr.find(Delimiter)) != std::string::npos) {
    Address = Addr.substr(0, Pos);
    Port = std::stoi(Addr.substr(Pos + 1, Addr.length() - Pos));
  }
}

void ConnectionConfigTy::dump() const {
  printf("  Connection: %s:%d\n", Address.c_str(), Port);
}

ManagerConfigTy::ManagerConfigTy() {
  if (const char *Env = std::getenv("LIBOMPTARGET_RPC_ADDRESS")) {
    std::string AddressString = Env;
    const std::string Delimiter = ",";

    do {
      auto Pos = (AddressString.find(Delimiter) != std::string::npos)
                     ? AddressString.find(Delimiter)
                     : AddressString.length();
      auto Token = AddressString.substr(0, Pos);
      ConnectionConfigs.emplace_back(Token);
      AddressString.erase(0, Pos + Delimiter.length());
    } while (!AddressString.empty());
  } else
    ConnectionConfigs.emplace_back("0.0.0.0:13337");

  if (const char *Env = std::getenv("LIBOMPTARGET_RPC_BLOCK_SIZE"))
    BufferSize = std::stoi(Env);
  else
    BufferSize = 1 << 20;
}

void ManagerConfigTy::dump() const {
  printf("Manager Config Dump:\n");
  printf("  Buffer Size: %lu\n", BufferSize);
  for (const auto &ConnectionConfig : ConnectionConfigs)
    ConnectionConfig.dump();
}

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
