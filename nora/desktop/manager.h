#ifndef NORA_DESKTOP_MANAGER_H_
#define NORA_DESKTOP_MANAGER_H_

#include <wayland-server-core.h>
#include <wayland-util.h>

struct nora_desktop_manager_unstable_v1 {
  struct wl_global *global;
  struct wl_list resources;

  struct wl_list workspaces;
  struct wl_list views;

  struct wl_listener display_destroy;

  struct {
  } events;

  void *data;
};

struct nora_desktop_workspace_handle_unstable_v1 {
  struct wl_list link; // nora_desktop_manager_handle.workspaces
  struct wl_list resources;

  char *id;

  struct {
  } events;

  void *data;
};

struct nora_desktop_view_handle_unstable_v1 {
  struct wl_list link; // nora_desktop_manager_handle.views
  struct wl_list resources;

  struct nora_desktop_workspace_handle_unstable_v1 *workspace;
  struct nora_desktop_manager_unstable_v1 *manager;

  char *app_id;
  char *title;
  bool hidden;

  struct {
  } events;

  void *data;
};

struct nora_desktop_manager_unstable_v1 *
nora_desktop_manager_unstable_v1_create(struct wl_display *display);

struct nora_desktop_view_handle_unstable_v1 *
nora_desktop_view_unstable_v1_create(
    struct nora_desktop_manager_unstable_v1 *manager);

void nora_desktop_view_handle_unstable_v1_set_title(
    struct nora_desktop_view_handle_unstable_v1 *view_handle, char *title);

void nora_desktop_view_handle_unstable_v1_set_app_id(
    struct nora_desktop_view_handle_unstable_v1 *view_handle, char *app_id);

void nora_desktop_view_handle_unstable_v1_destroy(
    struct nora_desktop_view_handle_unstable_v1 *view_handle);

#endif // NORA_DESKTOP_MANAGER_H_