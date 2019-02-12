#include <sys/socket.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include "an-svc.h"
#include "rlog.h"
#include "caps.h"
#include "thr-pool.h"

#define TAG "an-svc"

using namespace std;

void AutoNetworkingService::config(uint32_t opt, ...) {
  va_list ap;
  va_start(ap, opt);
  switch(opt) {
    case ANSVC_CONFIG_PORT:
      options.port = va_arg(ap, int32_t);
      break;
    case ANSVC_CONFIG_FLORA_URI:
      options.flora_uri = va_arg(ap, char *);
      break;
    case ANSVC_CONFIG_MAX_THREADS:
      options.max_threads = va_arg(ap, uint32_t);
      break;
    case ANSVC_CONFIG_HANDSHAKE_TIMEOUT:
      options.handshake_timeout = va_arg(ap, uint32_t);
      break;
  }
  va_end(ap);
}

bool AutoNetworkingService::start(bool blocking) {
  sockfd = init_socket(options.port);
  if (sockfd < 0)
    return false;
  bufsize = 4096;
  buffer = new char[bufsize];

  shared_ptr<Caps> msg = Caps::new_instance();
  msg->write(options.flora_uri);
  int32_t c = msg->serialize(nullptr, 0);
  if (c < 0) {
    ::close(sockfd);
    return false;
  }
  handshake2_data.resize(c);
  msg->serialize(handshake2_data.data(), handshake2_data.size());

  if (blocking) {
    ThreadPool thr_pool(options.max_threads);
    run(&thr_pool);
  } else {
    thread thr([this]() {
        ThreadPool thr_pool(this->options.max_threads);
        this->run(&thr_pool);
        });
    thr.detach();
  }
  return true;
}

void AutoNetworkingService::run(void *arg) {
  ssize_t c;
  struct sockaddr_in caddr;
  socklen_t addrlen;

  while (true) {
    addrlen = sizeof(caddr);
    c = recvfrom(sockfd, buffer, bufsize, MSG_WAITALL, (struct sockaddr *)&caddr, &addrlen);
    if (c < 0) {
      KLOGE(TAG, "auto-networking service recvfrom failed: %s", strerror(errno));
      destroy();
      // TODO: socket error, restart auto-networking service
      break;
    }
    KLOGI(TAG, "recv msg %d bytes, from %s:%d", c, inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));
    handle_msg(buffer, c, caddr, arg);
  }
}

int AutoNetworkingService::init_socket(int32_t port) {
  int fd = socket(PF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
    return fd;
  struct sockaddr_in saddr;
  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = INADDR_ANY;
  saddr.sin_port = htons(port);
  if (::bind(fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
    ::close(fd);
    KLOGE(TAG, "auto-networking service bind failed: %s", strerror(errno));
    return -1;
  }
  return fd;
}

void AutoNetworkingService::handle_msg(char *data, uint32_t size, sockaddr_in &caddr, void *arg) {
  shared_ptr<Caps> msg;
  if (Caps::parse(data, size, msg, false) != CAPS_SUCCESS) {
    KLOGW(TAG, "received data invalid: not caps");
    return;
  }
  string id;
  if (msg->read(id) != CAPS_SUCCESS) {
    KLOGW(TAG, "received data invalid: no device id");
    return;
  }
  ANDeviceInfo devinfo;
  if (msg->read(devinfo.type) != CAPS_SUCCESS) {
    KLOGW(TAG, "received data invalid: no device type");
    return;
  }
  if (msg->read(devinfo.name) != CAPS_SUCCESS) {
    KLOGW(TAG, "received data invalid: no device name");
    return;
  }
  KLOGI(TAG, "device id %s, type %d, name %s", id.c_str(), devinfo.type, devinfo.name.c_str());

  unique_lock<mutex> locker(map_mutex);
  if (connected_devices.find(id) != connected_devices.end()) {
    KLOGI(TAG, "device [%s]%s already connected", id.c_str(), devinfo.name.c_str());
    return;
  }
  if (connecting_devices.find(id) != connecting_devices.end()) {
    KLOGI(TAG, "device [%s]%s is connecting", id.c_str(), devinfo.name.c_str());
    return;
  }
  connecting_devices.insert(make_pair(id, devinfo));
  locker.unlock();

  reinterpret_cast<ThreadPool *>(arg)->push([this, caddr, id]() {
      this->handshake2(caddr, id);
      });
}

void AutoNetworkingService::handshake2(const struct sockaddr_in &caddr, const string &devid) {
  uint32_t elapsed = 0;

  while (true) {
    if (elapsed >= options.handshake_timeout) {
      map_mutex.lock();
      connecting_devices.erase(devid);
      map_mutex.unlock();
      KLOGI(TAG, "handshake timeout for dev %s", devid.c_str());
      break;
    }
    map_mutex.lock();
    if (connecting_devices.find(devid) == connecting_devices.end()) {
      map_mutex.unlock();
      break;
    }
    map_mutex.unlock();
    sendto(sockfd, handshake2_data.data(), handshake2_data.size(),
        0, (struct sockaddr *)&caddr, sizeof(caddr));
    KLOGI(TAG, "send handshake2 %u bytes to %s:%d, time %u/%u",
        handshake2_data.size(), inet_ntoa(caddr.sin_addr),
        ntohs(caddr.sin_port), elapsed, options.handshake_timeout);
    sleep(1);
    elapsed += 1000;
  }
}

void AutoNetworkingService::networking_complete(const char *devid) {
  ANDeviceMap::iterator it;
  string id = devid;
  lock_guard<mutex> locker(map_mutex);
  it = connecting_devices.find(id);
  if (it == connecting_devices.end())
    return;
  connected_devices.insert(*it);
  connecting_devices.erase(it);
  KLOGI(TAG, "device %s connected", devid);
}

void AutoNetworkingService::disconnect(const char *devid) {
  string id = devid;
  lock_guard<mutex> locker(map_mutex);
  connected_devices.erase(id);
  KLOGI(TAG, "erase device %s", devid);
}

void AutoNetworkingService::get_devices(ANDeviceArray &devs) {
  ANDeviceMap::iterator it;
  lock_guard<mutex> locker(map_mutex);
  devs.reserve(connected_devices.size());
  for (it = connected_devices.begin(); it != connected_devices.end(); ++it) {
    devs.push_back(*it);
  }
}

bool AutoNetworkingService::is_device_connecting(const char *devid) {
  string id = devid;
  lock_guard<mutex> locker(map_mutex);
  return connecting_devices.find(id) != connecting_devices.end();
}

void AutoNetworkingService::destroy() {
  ::close(sockfd);
  sockfd = -1;
  delete[] buffer;
  buffer = nullptr;
  bufsize = 0;
}
