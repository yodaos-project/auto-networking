#pragma once

#include <netinet/in.h>
#include <string>
#include <map>
#include <vector>
#include <mutex>

typedef struct {
  uint32_t type;
  std::string name;
} ANDeviceInfo;
typedef std::pair<std::string, ANDeviceInfo> ANDevicePair;
typedef std::map<std::string, ANDeviceInfo> ANDeviceMap;
typedef std::vector<ANDevicePair> ANDeviceArray;

#define ANSVC_CONFIG_PORT 0
#define ANSVC_CONFIG_FLORA_URI 1
#define ANSVC_CONFIG_MAX_THREADS 2
#define ANSVC_CONFIG_HANDSHAKE_TIMEOUT 3
class AutoNetworkingService {
public:
  void config(uint32_t opt, ...);

  bool start(bool blocking = false);

  void networking_complete(const char *devid);

  void disconnect(const char *devid);

  void get_devices(ANDeviceArray &devs);

  bool is_device_connecting(const char *devid);

private:
  void run(void *arg);

  int init_socket(int32_t port);

  void destroy();

  void handle_msg(char *data, uint32_t size, struct sockaddr_in &caddr, void *arg);

  void handshake2(const struct sockaddr_in &caddr, const std::string &devid);

  class Options {
  public:
    int32_t port = 0;
    uint32_t max_threads = 4;
    uint32_t handshake_timeout = 20000;
    std::string flora_uri;
  };

private:
  int sockfd = -1;
  char *buffer = nullptr;
  uint32_t bufsize = 0;
  Options options;
  ANDeviceMap connected_devices;
  ANDeviceMap connecting_devices;
  std::mutex map_mutex;
  std::vector<char> handshake2_data;
};
