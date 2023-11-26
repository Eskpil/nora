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
#include <wlr/types/wlr_linux_dmabuf_v1.h>
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

#include "tree.h"

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

  struct wlr_presentation *presentation;
  struct wlr_drm *drm;
  struct wlr_linux_dmabuf_v1 *linux_dmabuf_v1;

  struct nora_tree_root *tree_root;

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

struct nora_keyboard {
  struct wl_list link;
  struct nora_server *server;
  struct wlr_keyboard *wlr_keyboard;

  struct wl_listener modifiers;
  struct wl_listener key;
  struct wl_listener destroy;
};

struct nora_server *nora_server_create(struct nora_server_config config);

int nora_server_run(struct nora_server *server);

// after this function it is no longer safe to perform any actions on a
// nora_server instance.
int nora_server_destroy(struct nora_server *server);

#endif // NORA_SERVER
