#include "an-cli.h"

int main(int argc, char **argv) {
  AutoNetworkingClient cli;
  cli.config(ANCLI_CONFIG_PORT, 37800);
  cli.config(ANCLI_CONFIG_BROADCAST_INTERVAL, 1000);
  cli.config(ANCLI_CONFIG_DEVICE_ID, "12345678");
  cli.config(ANCLI_CONFIG_DEVICE_NAME, "foo");
  cli.config(ANCLI_CONFIG_DEVICE_TYPE, 0);
  cli.networking();
  return 0;
}
