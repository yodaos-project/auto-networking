#pragma once

#include <netinet/in.h>
#include "flora-agent.h"

#define ANCLI_CONFIG_PORT 0
#define ANCLI_CONFIG_BROADCAST_INTERVAL 1
#define ANCLI_CONFIG_DEVICE_ID 2
#define ANCLI_CONFIG_DEVICE_NAME 3
#define ANCLI_CONFIG_DEVICE_TYPE 4

class AutoNetworkingClient {
public:
  void config(uint32_t opt, ...);

  bool networking();

  inline flora::Agent *get_connection() { return &flora_agent; }

private:
  bool init(int32_t port, uint32_t inter);

  void *get_send_data(uint32_t &sz);

  bool handle_msg(char *data, ssize_t size);

  void destroy();

private:
  class Options {
  public:
    int32_t port = 0;
    uint32_t brinter = 10000;
    std::string device_id;
    std::string device_name;
    uint32_t device_type = 0;
  };

  int sockfd = -1;
  struct sockaddr_in braddr;
  char *send_buffer = nullptr;
  uint32_t sbufsize = 0;
  Options options;
  flora::Agent flora_agent;
};
