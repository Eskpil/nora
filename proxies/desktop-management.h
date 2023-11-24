#ifndef PROXY_DESKTOP_MANAGEMENT_H_
#define PROXY_DESKTOP_MANAGEMENT_H_

#include "proxy.h"
#include <wayland-client.h>

struct nora_proxy_workspace {};

struct nora_proxy_view {
  struct wl_list link;

  struct nora_desktop_view_v1 *inner;

  struct nora_proxy_desktop_management_state *state;

  char *object_path;

  char *app_id;
  char *title;
};

struct nora_proxy_desktop_management_state {
  struct nora_proxy *proxy;

  struct wl_list workspaces;
  struct wl_list views;
};

void *nora_proxy_desktop_management_create(struct nora_proxy *proxy);
void nora_proxy_desktop_management_configure(
    struct nora_proxy_desktop_management_state *state);

int nora_proxy_view_method_hide(sd_bus_message *m, void *userdata, sd_bus_error *error);

static const sd_bus_vtable nora_proxy_view_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Title", "s", NULL, offsetof(struct nora_proxy_view, title),
                    SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("AppId", "s", NULL,
                    offsetof(struct nora_proxy_view, app_id),
                    SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_METHOD("Hide", "", "", nora_proxy_view_method_hide, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END};

#endif // PROXY_DESKTOP_MANAGEMENT_H_
