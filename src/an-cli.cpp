#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include "rlog.h"
#include "an-cli.h"
#include "caps.h"

#define TAG "an-cli"
#define SBUFSIZE 4096

using namespace std;

void AutoNetworkingClient::config(uint32_t opt, ...) {
  va_list ap;
  va_start(ap, opt);
  switch (opt) {
    case ANCLI_CONFIG_PORT:
      options.port = va_arg(ap, int32_t);
      break;
    case ANCLI_CONFIG_BROADCAST_INTERVAL:
      options.brinter = va_arg(ap, uint32_t);
      break;
    case ANCLI_CONFIG_DEVICE_ID:
      options.device_id = va_arg(ap, char *);
      break;
    case ANCLI_CONFIG_DEVICE_NAME:
      options.device_name = va_arg(ap, char *);
      break;
    case ANCLI_CONFIG_DEVICE_TYPE:
      options.device_type = va_arg(ap, uint32_t);
      break;
  }
  va_end(ap);
}

bool AutoNetworkingClient::networking() {
  if (!init(options.port, options.brinter)) {
    return false;
  }

  ssize_t c;
  uint32_t sdsz;
  void *sdata = get_send_data(sdsz);
  char buf[4096];
  bool r = false;
  while (true) {
    c = sendto(sockfd, sdata, sdsz, 0, (struct sockaddr *)&braddr, sizeof(braddr));
    if (c < 0) {
      KLOGE(TAG, "msg broadcast failed: %s", strerror(errno));
      break;
    }
    KLOGI(TAG, "msg broadcast success");

    c = recvfrom(sockfd, buf, sizeof(buf), MSG_WAITALL, nullptr, nullptr);
    if (c < 0) {
      if (errno != EAGAIN) {
        KLOGE(TAG, "socket receive failed: %s", strerror(errno));
        break;
      }
    }

    KLOGI(TAG, "msg received %d bytes", c);
    if (c > 0 && handle_msg(buf, c)) {
      r = true;
      break;
    }
  }

  destroy();
  return r;
}

void *AutoNetworkingClient::get_send_data(uint32_t &sz) {
  if (send_buffer) {
    sz = sbufsize;
    return send_buffer;
  }
  send_buffer = new char[SBUFSIZE];
  shared_ptr<Caps> data = Caps::new_instance();
  data->write(options.device_id);
  data->write(options.device_type);
  data->write(options.device_name);
  int32_t r = data->serialize(send_buffer, SBUFSIZE);
  if (r < 0) {
    delete[] send_buffer;
    send_buffer = nullptr;
    return nullptr;
  }
  sbufsize = sz = r;
  return send_buffer;
}

void AutoNetworkingClient::destroy() {
  ::close(sockfd);
  sockfd = -1;
  delete[] send_buffer;
  send_buffer = nullptr;
}

bool AutoNetworkingClient::handle_msg(char *data, ssize_t size) {
  shared_ptr<Caps> msg;
  if (Caps::parse(data, size, msg, false) != CAPS_SUCCESS) {
    KLOGE(TAG, "received data invalid: not caps format");
    return false;
  }
  string uri;
  if (msg->read(uri) != CAPS_SUCCESS) {
    KLOGE(TAG, "received data invalid: no uri");
    return false;
  }
  KLOGI(TAG, "flora service uri = %s", uri.c_str());

  // connect flora
  uri.append("#");
  uri.append(options.device_id);
  flora_agent.config(FLORA_AGENT_CONFIG_URI, uri.c_str());
  flora_agent.start();
  return true;
}

bool AutoNetworkingClient::init(int32_t port, uint32_t inter) {
  int fd = socket(PF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
    return false;
  int opt = -1;
  if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (char*)&opt, sizeof(opt)) < 0) {
    ::close(fd);
    KLOGE(TAG, "set socket broadcast failed: %s", strerror(errno));
    return false;
  }
  struct timeval tv;
  tv.tv_sec = inter / 1000;
  tv.tv_usec = (inter % 1000) * 1000;
  if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    ::close(fd);
    KLOGE(TAG, "set socket recv timeout failed: %s", strerror(errno));
    return false;
  }
  memset(&braddr, 0, sizeof(braddr));
  braddr.sin_family = AF_INET;
  braddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  braddr.sin_port = htons(port);
  sockfd = fd;
  return true;
}
