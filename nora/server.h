#ifndef NORA_SERVER_H
#define NORA_SERVER_H

#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#include "desktop/manager.h"

#define UNREACHABLE()                                                          \
  wlr_log(WLR_ERROR, "UNREACHABLE");                                           \
  exit(EXIT_FAILURE);

enum nora_cursor_mode {
  NORA_CURSOR_PASSTHROUGH,
  NORA_CURSOR_MOVE,
  NORA_CURSOR_RESIZE,
};

struct nora_server_config {};

struct nora_server {
  struct wl_display *wl_display;
  struct wlr_backend *backend;
  struct wlr_renderer *renderer;
  struct wlr_allocator *allocator;
  struct wlr_scene *scene;
  struct wlr_scene_output_layout *scene_layout;
  struct wlr_presentation *presentation;
  struct wlr_drm *drm;

  struct {
    struct wlr_seat *seat;

    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_mgr;
    struct wl_listener cursor_motion;
    struct wl_listener cursor_motion_absolute;
    struct wl_listener cursor_button;
    struct wl_listener cursor_axis;
    struct wl_listener cursor_frame;

    struct wl_listener new_input;
    struct wl_listener request_cursor;
    struct wl_listener request_set_selection;

    struct wl_list keyboards;

    /* Grabs (resizing, moving etc)
     */
    enum nora_cursor_mode cursor_mode;
    struct nora_view *grabbed_view;
    double grab_x, grab_y;
    struct wlr_box grab_geobox;
    uint32_t resize_edges;
  } input;

  struct {
    struct wlr_xdg_shell *xdg_shell;
    struct wlr_layer_shell_v1 *layer_shell;
    struct wlr_output_manager_v1 *output_manager;
    struct wlr_output_configuration_v1 *output_configuration;
    struct wlr_output_layout *output_layout;
    struct nora_desktop_manager_unstable_v1 *manager;
    struct nora_desktop *layout;
    struct wl_list outputs;

    struct wl_listener new_xdg_toplevel;
    struct wl_listener new_xdg_popup;

    struct wl_listener new_layer_surface;

    struct wl_listener new_output;
  } desktop;
};

struct nora_output {
  struct wl_list link;

  struct nora_server *server;
  struct wlr_output *wlr_output;
  struct wl_listener frame;
  struct wl_listener request_state;
  struct wl_listener destroy;

  struct {
    uint32_t left;
    uint32_t right;
    uint32_t top;
    uint32_t bottom;
  } excluded_margin;
};

enum nora_view_kind {
  NORA_VIEW_KIND_XDG_TOPLEVEL,
  NORA_VIEW_KIND_XDG_POPUP,
  NORA_VIEW_KIND_LAYER,
};

struct nora_view {
  struct wl_list link;
  struct nora_server *server;
  enum nora_view_kind kind;

  struct nora_output *output;

  struct nora_desktop_view_handle_unstable_v1 *view_handle;

  int x, y;

  struct wlr_surface *surface;

  // TODO: Find an ergonomic way to avoid having to repeat the map, unmap and
  // destroy listeners on xdg based surfaces.
  union {
    struct {
      struct wlr_xdg_toplevel *xdg_toplevel;

      struct wlr_scene_tree *scene_tree;

      struct wl_listener map;
      struct wl_listener unmap;
      struct wl_listener destroy;

      struct wl_listener request_move;
      struct wl_listener request_resize;
      struct wl_listener request_maximize;
      struct wl_listener request_fullscreen;

      struct wl_listener set_title;
      struct wl_listener set_app_id;

      struct wlr_box box;
    } xdg_toplevel;

    struct {
      struct wlr_xdg_popup *xdg_popup;
      struct wlr_scene_tree *scene_tree;

      struct wl_listener map;
      struct wl_listener unmap;
      struct wl_listener destroy;

      struct wl_listener reposition;

      struct nora_view *parent;
    } xdg_popup;

    struct {
      struct wlr_layer_surface_v1 *surface;
      struct wlr_scene_layer_surface_v1 *scene_tree;

      struct wl_listener map;
      struct wl_listener unmap;
      struct wl_listener destroy;
      struct wl_listener new_popup;
      struct wl_listener commit;
    } layer;
  };
};

struct nora_keyboard {
  struct wl_list link;
  struct nora_server *server;
  struct wlr_keyboard *wlr_keyboard;

  struct wl_listener modifiers;
  struct wl_listener key;
  struct wl_listener destroy;
};

struct nora_desktop {
  struct wl_list views;
  struct wl_list layers;

  struct nora_view *focused_view;

  struct nora_server *server;
};

struct nora_server *nora_server_create(struct nora_server_config config);

int nora_server_run(struct nora_server *server);

// after this function it is no longer safe to perform any actions on a
// nora_server instance.
int nora_server_destroy(struct nora_server *server);

#endif // NORA_SERVER
