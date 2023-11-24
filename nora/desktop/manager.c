#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>

#include "manager.h"
#include "nora-desktop-management-unstable-v1-protocol.h"
#include "wlr/util/log.h"

static void
nora_desktop_manager_resource_destroy(struct wl_resource *resource) {
  wl_list_remove(wl_resource_get_link(resource));
}

static void nora_desktop_manager_bind(struct wl_client *client, void *data,
                                      uint32_t version, uint32_t id) {
  struct nora_desktop_manager_unstable_v1 *manager = data;

  struct wl_resource *resource = wl_resource_create(
      client, &nora_desktop_manager_v1_interface, version, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wlr_log(WLR_INFO, "Got client!");

  wl_resource_set_implementation(resource, NULL, manager,
                                 nora_desktop_manager_resource_destroy);

  wl_list_insert(&manager->resources, wl_resource_get_link(resource));

  // TODO: Report all know toplevels and workspaces here.

  return;
}

static void on_view_hide(struct wl_client *client, struct wl_resource *resource) {
  wlr_log(WLR_INFO, "received hide request");
}

static const struct nora_desktop_view_v1_interface view_interface = {
  .hide = on_view_hide,
};

static void
nora_desktop_view_handle_resource_destroy(struct wl_resource *resource) {
  wl_list_remove(wl_resource_get_link(resource));
}

static struct wl_resource *create_view_handle_resource_for_resource(
    struct nora_desktop_view_handle_unstable_v1 *view_handle,
    struct wl_resource *manager_resource) {
  struct wl_client *client = wl_resource_get_client(manager_resource);
  struct wl_resource *resource =
      wl_resource_create(client, &nora_desktop_view_v1_interface,
                         wl_resource_get_version(manager_resource), 0);
  if (!resource) {
    wl_client_post_no_memory(client);
    return NULL;
  }

  wl_resource_set_implementation(resource, &view_interface, view_handle,
                                 nora_desktop_view_handle_resource_destroy);

  wl_list_insert(&view_handle->resources, wl_resource_get_link(resource));
  nora_desktop_manager_v1_send_view(manager_resource, resource);

  return resource;
}

struct nora_desktop_manager_unstable_v1 *
nora_desktop_manager_unstable_v1_create(struct wl_display *display) {
  struct nora_desktop_manager_unstable_v1 *manager =
      calloc(1, sizeof(*manager));

  wl_list_init(&manager->resources);
  wl_list_init(&manager->views);
  wl_list_init(&manager->workspaces);

  manager->global =
      wl_global_create(display, &nora_desktop_manager_v1_interface, 1, manager,
                       nora_desktop_manager_bind);

  return manager;
}

struct nora_desktop_view_handle_unstable_v1 *
nora_desktop_view_unstable_v1_create(
    struct nora_desktop_manager_unstable_v1 *manager) {
  struct nora_desktop_view_handle_unstable_v1 *view_handle =
      calloc(1, sizeof(*view_handle));

  wl_list_init(&view_handle->resources);

  wl_list_insert(&manager->views, &view_handle->link);
  view_handle->manager = manager;

  struct wl_resource *manager_resource, *tmp;
  wl_resource_for_each_safe(manager_resource, tmp, &manager->resources) {
    create_view_handle_resource_for_resource(view_handle, manager_resource);
  }

  return view_handle;
}

void nora_desktop_view_handle_unstable_v1_set_title(
    struct nora_desktop_view_handle_unstable_v1 *view_handle, char *title) {
  if (view_handle->title) {
    free(view_handle->title);
  }

  view_handle->title = strdup(title);

  struct wl_resource *resource, *tmp;
  wl_list_for_each_safe(resource, tmp, &view_handle->resources, link) {
    nora_desktop_view_v1_send_title(resource, view_handle->title);
  };
}

void nora_desktop_view_handle_unstable_v1_set_app_id(
    struct nora_desktop_view_handle_unstable_v1 *view_handle, char *app_id) {
  if (view_handle->app_id) {
    free(view_handle->app_id);
  }

  view_handle->app_id = strdup(app_id);

  struct wl_resource *resource, *tmp;
  wl_list_for_each_safe(resource, tmp, &view_handle->resources, link) {
    nora_desktop_view_v1_send_app_id(resource, view_handle->app_id);
  };
}

void nora_desktop_view_handle_unstable_v1_destroy(
    struct nora_desktop_view_handle_unstable_v1 *view_handle) {
  // TODO: Perform proper destroy

  wl_list_remove(&view_handle->link);

  struct wl_resource *resource, *tmp;
  wl_list_for_each_safe(resource, tmp, &view_handle->resources, link) {
    nora_desktop_view_v1_send_destroy(resource);
  };

  free(view_handle);
}
