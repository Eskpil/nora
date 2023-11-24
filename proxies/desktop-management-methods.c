#include "desktop-management.h"
#include "nora-desktop-management-unstable-v1-client-protocol.h"

int nora_proxy_view_method_hide(sd_bus_message *m, void *userdata, sd_bus_error *error) {
  struct nora_proxy_view *view = userdata;

  printf("forwarding hide\n");

  nora_desktop_view_v1_hide(view->inner);

  return 1;
}
