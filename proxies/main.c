#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client-core.h>

#include "desktop-management.h"
#include "output-management.h"
#include "proxy.h"

int main(void) {
  setenv("WAYLAND_DISPLAY", "wayland-0", 1);
  printf("connecting to: (%s)\n", getenv("WAYLAND_DISPLAY"));

  struct nora_proxy *proxy = nora_proxy_create();

  struct nora_proxy_output_management_state *output_management_state =
      nora_proxy_output_management_create(proxy);
  struct nora_proxy_desktop_management_state *desktop_management_state =
      nora_proxy_desktop_management_create(proxy);

  nora_proxy_flush(proxy);

  nora_proxy_output_management_configure(output_management_state);
  nora_proxy_desktop_management_configure(desktop_management_state);

  for (;;) {
    int ret = wl_display_roundtrip(proxy->display);
    if (ret < 0) {
      fprintf(stderr, "could not perform roundtrip: %s\n", strerror(-ret));
      return ret;
    }

    ret = sd_bus_process(proxy->bus, NULL);
    if (ret < 0) {
      fprintf(stderr, "Failed to process bus: %s\n", strerror(-ret));
      return ret;
    }

    /* ... handle other events if needed ... */
  }

  nora_proxy_destroy(proxy);
}
