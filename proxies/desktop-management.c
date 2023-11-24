#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <wayland-util.h>

#include "desktop-management.h"
#include "nora-desktop-management-unstable-v1-client-protocol.h"
#include "nora-desktop-management-unstable-v1-protocol.h"
#include "proxy.h"

static void on_view_app_id(void *data,
                           struct nora_desktop_view_v1 *nora_desktop_view_v1,
                           const char *app_id) {
  struct nora_proxy_view *view = data;

  if (!view->app_id) {
    free(view->app_id);
  }

  view->app_id = strdup(app_id);
}

static void on_view_title(void *data,
                          struct nora_desktop_view_v1 *nora_desktop_view_v1,
                          const char *title) {
  struct nora_proxy_view *view = data;

  if (!view->title) {
    free(view->title);
  }

  view->title = strdup(title);
}

static void on_view_destroy(void *data,
                            struct nora_desktop_view_v1 *nora_desktop_view_v1) {
  struct nora_proxy_view *view = data;

  fprintf(stdout, "got on_view_destroy\n");
}

static void on_view_hidden(void *data,
                           struct nora_desktop_view_v1 *nora_desktop_view_v1,
                           uint32_t hidden) {
  struct nora_proxy_view *view = data;
}

static void on_view_kind(void *data,
                         struct nora_desktop_view_v1 *nora_desktop_view_v1,
                         uint32_t kind) {
  struct nora_proxy_view *view = data;
}

static void on_view_workspace(void *data,
                              struct nora_desktop_view_v1 *nora_desktop_view_v1,
                              struct nora_desktop_workspace_v1 *workspace) {
  struct nora_proxy_view *view = data;
}

static struct nora_desktop_view_v1_listener view_listener = {
    .app_id = on_view_app_id,
    .title = on_view_title,
    .destroy = on_view_destroy,
    .hidden = on_view_hidden,
    .kind = on_view_kind,
    .workspace = on_view_workspace,
};

static void
on_desktop_view(void *data,
                struct nora_desktop_manager_v1 *nora_desktop_manager_v1,
                struct nora_desktop_view_v1 *view_handle) {
  struct nora_proxy_desktop_management_state *state = data;

  struct nora_proxy_view *view = calloc(1, sizeof(*view));
  view->state = state;
  view->inner = view_handle;

  char *path = calloc(1, 256);
  sprintf(path, "/org/nora/view/_%d", wl_list_length(&state->views));

  view->object_path = path;

  int ret =
      sd_bus_add_object_vtable(state->proxy->bus, NULL, path, "org.nora.View",
                               nora_proxy_view_vtable, view);
  if (0 > ret) {
    fprintf(stderr, "could not add output object: %s\n", strerror(-ret));
    exit(EXIT_FAILURE);
  }

  nora_desktop_view_v1_add_listener(view_handle, &view_listener, view);
}

static void
on_desktop_workspace(void *data,
                     struct nora_desktop_manager_v1 *nora_desktop_manager_v1,
                     struct nora_desktop_workspace_v1 *workspace) {
  fprintf(stdout, "got workspace!\n");
}

static const struct nora_desktop_manager_v1_listener manager_listener = {
    .view = on_desktop_view,
    .workspace = on_desktop_workspace,
};

void *nora_proxy_desktop_management_create(struct nora_proxy *proxy) {
  struct nora_proxy_desktop_management_state *state = calloc(1, sizeof(*state));

  state->proxy = proxy;

  wl_list_init(&state->views);
  wl_list_init(&state->workspaces);

  nora_proxy_bind(proxy, nora_desktop_manager_v1_interface, 1);

  return state;
}

void nora_proxy_desktop_management_configure(
    struct nora_proxy_desktop_management_state *state) {
  struct nora_desktop_manager_v1 *manager =
      nora_proxy_extract(state->proxy, nora_desktop_manager_v1_interface, 1);

  nora_desktop_manager_v1_add_listener(manager, &manager_listener, state);
}
