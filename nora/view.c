#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include <wayland-server-core.h>

#include <wayland-util.h>
#include <wlr/util/log.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>

#include "desktop.h"
#include "nora/desktop/manager.h"
#include "output.h"
#include "server.h"
#include "view.h"

static void nora_arrange_layers(struct nora_output *output) {
  uint32_t *margin_left = &output->excluded_margin.left;
  uint32_t *margin_right = &output->excluded_margin.right;
  uint32_t *margin_top = &output->excluded_margin.top;
  uint32_t *margin_bottom = &output->excluded_margin.bottom;

  *margin_left = 0;
  *margin_right = 0;
  *margin_top = 0;
  *margin_bottom = 0;

  uint32_t output_width = output->wlr_output->width;
  uint32_t output_height = output->wlr_output->height;

  // TODO: This layout function is inefficient and relies on some guesses about
  // how the layer shell is supposed to work, it needs reassessment later
  struct nora_view *view;
  wl_list_for_each_reverse(view, &output->server->desktop.layout->layers,
                           link) {
    if (view->kind == NORA_VIEW_KIND_LAYER) {
      struct wlr_layer_surface_v1_state state = view->layer.surface->current;
      bool anchor_left = state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
      bool anchor_right = state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
      bool anchor_top = state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
      bool anchor_bottom = state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;

      bool anchor_horiz = anchor_left && anchor_right;
      bool anchor_vert = anchor_bottom && anchor_top;

      uint32_t desired_width = state.desired_width;
      if (desired_width == 0) {
        desired_width = output_width;
      }
      uint32_t desired_height = state.desired_height;
      if (desired_height == 0) {
        desired_height = output_height;
      }

      uint32_t anchor_sum =
          anchor_left + anchor_right + anchor_top + anchor_bottom;
      switch (anchor_sum) {
      case 0:
        // Not anchored to any edge => display in centre with suggested size
        wlr_layer_surface_v1_configure(view->layer.surface, desired_width,
                                       desired_height);
        view->x = output_width / 2 - desired_width / 2;
        view->y = output_height / 2 - desired_height / 2;
        break;
      case 1:
        wlr_log(WLR_ERROR, "One anchor");
        // Anchored to one edge => use suggested size
        wlr_layer_surface_v1_configure(view->layer.surface, desired_width,
                                       desired_height);
        if (anchor_left || anchor_right) {
          view->y = output_height / 2 - desired_height / 2;
          if (anchor_left) {
            view->x = 0;
          } else {
            view->x = output_width - desired_width;
          }
        } else {
          view->x = output_width / 2 - desired_width / 2;
          if (anchor_top) {
            view->y = 0;
          } else {
            view->y = output_height - desired_height;
          }
        }
        wlr_log(WLR_ERROR, "Set layer surface x %d, y %d, width %d, height %d",
                view->x, view->y, desired_width, desired_height);
        break;
      case 2:
        // Anchored to two edges => use suggested size

        if (anchor_horiz) {
          wlr_layer_surface_v1_configure(view->layer.surface, desired_width,
                                         desired_height);
          view->x = 0;
          view->y = output_height / 2 - desired_height / 2;
        } else if (anchor_vert) {
          wlr_layer_surface_v1_configure(view->layer.surface, desired_width,
                                         desired_height);
          view->x = output_width / 2 - desired_width / 2;
          view->y = 0;
        } else if (anchor_top && anchor_left) {
          wlr_layer_surface_v1_configure(view->layer.surface, desired_width,
                                         desired_height);
          view->x = 0;
          view->y = 0;
        } else if (anchor_top && anchor_right) {
          wlr_layer_surface_v1_configure(view->layer.surface, desired_width,
                                         desired_height);
          view->x = output_width - desired_width;
          view->y = 0;
        } else if (anchor_bottom && anchor_right) {
          wlr_layer_surface_v1_configure(view->layer.surface, desired_width,
                                         desired_height);
          view->x = output_width - desired_width;
          view->y = output_height - desired_height;
        } else if (anchor_bottom && anchor_left) {
          wlr_layer_surface_v1_configure(view->layer.surface, desired_width,
                                         desired_height);
          view->x = 0;
          view->y = output_height - desired_height;
        }
        break;
      case 3:
        // Anchored to three edges => use suggested size on free axis only
        if (anchor_horiz) {
          wlr_layer_surface_v1_configure(view->layer.surface, output_width,
                                         desired_height);
          view->x = 0;
          if (anchor_top) {
            view->y = *margin_top;
            if (state.exclusive_zone) {
              *margin_top += desired_height;
            }
          } else {
            view->y = output_height - desired_height - *margin_bottom;
            if (state.exclusive_zone) {
              *margin_bottom += desired_height;
            }
          }
        } else {
          wlr_layer_surface_v1_configure(view->layer.surface, desired_width,
                                         output_height);
          view->y = 0;
          if (anchor_left) {
            view->x = *margin_left;
            if (state.exclusive_zone) {
              *margin_left += desired_width;
            }
          } else {
            view->x = output_width - desired_width - *margin_right;
            if (state.exclusive_zone) {
              *margin_right += desired_width;
            }
          }
        }
        break;
      case 4:
        // Fill the output
        wlr_layer_surface_v1_configure(view->layer.surface, output_width,
                                       output_height);
        view->x = 0;
        view->y = 0;
        break;
      default:
        UNREACHABLE()
      }
    }
  }
}

static void nora_view_begin_interactive(struct nora_view *view,
                                        enum nora_cursor_mode mode,
                                        uint32_t edges) {
  /* This function sets up an interactive move or resize operation, where the
   * compositor stops propegating pointer events to clients and instead
   * consumes them itself, to move or resize windows. */
  struct nora_server *server = view->server;
  struct wlr_surface *focused_surface =
      server->input.seat->pointer_state.focused_surface;
  if (view->xdg.xdg_toplevel->base->surface !=
      wlr_surface_get_root_surface(focused_surface)) {
    /* Deny move/resize requests from unfocused clients. */
    return;
  }

  server->input.grabbed_view = view;
  server->input.cursor_mode = mode;

  if (mode == NORA_CURSOR_MOVE) {
    server->input.grab_x =
        server->input.cursor->x - view->xdg.scene_tree->node.x;
    server->input.grab_y =
        server->input.cursor->y - view->xdg.scene_tree->node.y;
  } else {
    struct wlr_box geo_box;
    wlr_xdg_surface_get_geometry(view->xdg.xdg_toplevel->base, &geo_box);

    double border_x = (view->xdg.scene_tree->node.x + geo_box.x) +
                      ((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
    double border_y = (view->xdg.scene_tree->node.y + geo_box.y) +
                      ((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
    server->input.grab_x = server->input.cursor->x - border_x;
    server->input.grab_y = server->input.cursor->y - border_y;

    server->input.grab_geobox = geo_box;
    server->input.grab_geobox.x += view->xdg.scene_tree->node.x;
    server->input.grab_geobox.y += view->xdg.scene_tree->node.y;

    server->input.resize_edges = edges;
  }
}

static void on_layer_commit(struct wl_listener *listener, void *data) {
  (void)data;

  struct nora_view *view = wl_container_of(listener, view, layer.commit);
  assert(view != NULL);

  wlr_log(WLR_INFO, "View: (%s) requested to be committed",
          view->layer.surface->namespace);
}

static void on_layer_map(struct wl_listener *listener, void *data) {
  (void)data;

  struct nora_view *view = wl_container_of(listener, view, layer.map);
  assert(view);

  wlr_log(WLR_INFO, "View: (%s) requested to be mapped",
          view->layer.surface->namespace);
}

static void on_layer_unmap(struct wl_listener *listener, void *data) {
  (void)data;

  struct nora_view *view = wl_container_of(listener, view, layer.unmap);
  assert(view != NULL);

  wlr_log(WLR_INFO, "View: (%s) requested to be unmapped",
          view->layer.surface->namespace);

  nora_desktop_remove_view(view->server->desktop.layout, view);
}

static void on_layer_destroy(struct wl_listener *listener, void *data) {
  (void)data;

  struct nora_view *view = wl_container_of(listener, view, layer.destroy);
  assert(view != NULL);

  wlr_log(WLR_INFO, "View: (%s) requested to be destroyed",
          view->layer.surface->namespace);
}

static void on_layer_new_popup(struct wl_listener *listener, void *data) {
  (void)data;

  struct nora_view *view = wl_container_of(listener, view, layer.new_popup);
  assert(view != NULL);

  wlr_log(WLR_INFO, "View: (%s) requested a new popup",
          view->layer.surface->namespace);
}

static void on_xdg_toplevel_map(struct wl_listener *listener, void *data) {
  /* Called when the surface is mapped, or ready to display on-screen. */
  struct nora_view *view = wl_container_of(listener, view, xdg.map);
  nora_desktop_insert_view(view->server->desktop.layout, view);
}

static void on_xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
  /* Called when the surface is unmapped, and should no longer be shown. */
  struct nora_view *view = wl_container_of(listener, view, xdg.unmap);
  nora_desktop_remove_view(view->server->desktop.layout, view);
}

static void on_xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
  /* Called when the surface is destroyed and should never be shown again. */
  struct nora_view *view = wl_container_of(listener, view, xdg.destroy);

  nora_desktop_view_handle_unstable_v1_destroy(view->view_handle);

  wl_list_remove(&view->xdg.map.link);
  wl_list_remove(&view->xdg.unmap.link);
  wl_list_remove(&view->xdg.destroy.link);

  wl_list_remove(&view->xdg.request_fullscreen.link);
  wl_list_remove(&view->xdg.request_maximize.link);
  wl_list_remove(&view->xdg.request_resize.link);
  wl_list_remove(&view->xdg.request_move.link);

  wl_list_remove(&view->xdg.set_app_id.link);
  wl_list_remove(&view->xdg.set_title.link);

  free(view);
}

static void on_xdg_toplevel_request_move(struct wl_listener *listener,
                                         void *data) {
  // TODO: Check the current tiling mode in the surface's workspace.

  struct nora_view *view =
      wl_container_of(listener, view, xdg.request_move);
  nora_view_begin_interactive(view, NORA_CURSOR_MOVE, 0);
}

static void on_xdg_toplevel_request_resize(struct wl_listener *listener,
                                           void *data) {
  // TODO: Check the current tiling mode in the surface's workspace.

  struct wlr_xdg_toplevel_resize_event *event = data;

  struct nora_view *view =
      wl_container_of(listener, view, xdg.request_resize);

  nora_view_begin_interactive(view, NORA_CURSOR_RESIZE, event->edges);
}

// TODO: Implement maximize and fullscreen events.
static void on_xdg_toplevel_request_maximize(struct wl_listener *listener,
                                             void *data) {
  struct nora_view *view =
      wl_container_of(listener, view, xdg.request_maximize);
  wlr_xdg_surface_schedule_configure(view->xdg.xdg_toplevel->base);
}

static void on_xdg_toplevel_request_fullscreen(struct wl_listener *listener,
                                               void *data) {
  struct nora_view *view =
      wl_container_of(listener, view, xdg.request_fullscreen);
  wlr_xdg_surface_schedule_configure(view->xdg.xdg_toplevel->base);
}

static void on_xdg_toplevel_title(struct wl_listener *listener, void *data) {
  struct nora_view *view = wl_container_of(listener, view, xdg.set_title);
  nora_desktop_view_handle_unstable_v1_set_title(view->view_handle,
                                                 view->xdg.xdg_toplevel->title);
}

static void on_xdg_toplevel_app_id(struct wl_listener *listener, void *data) {
  struct nora_view *view = wl_container_of(listener, view, xdg.set_app_id);
  nora_desktop_view_handle_unstable_v1_set_app_id(
      view->view_handle, view->xdg.xdg_toplevel->app_id);
}

void nora_new_layer_surface(struct wl_listener *listener, void *data) {
  struct nora_server *server =
      wl_container_of(listener, server, desktop.new_layer_surface);
  assert(server != NULL);

  struct wlr_layer_surface_v1 *surface = data;

  struct nora_view *view = calloc(1, sizeof(*view));
  view->kind = NORA_VIEW_KIND_LAYER;
  view->server = server;

  view->layer.surface = surface;
  view->layer.scene_tree =
      wlr_scene_layer_surface_v1_create(&server->scene->tree, surface);
  view->layer.scene_tree->tree->node.data = view;

  view->layer.commit.notify = on_layer_commit;
  wl_signal_add(&surface->surface->events.commit, &view->layer.commit);

  view->layer.map.notify = on_layer_map;
  wl_signal_add(&surface->surface->events.map, &view->layer.map);

  view->layer.unmap.notify = on_layer_unmap;
  wl_signal_add(&surface->surface->events.unmap, &view->layer.unmap);

  view->layer.destroy.notify = on_layer_destroy;
  wl_signal_add(&surface->events.destroy, &view->layer.destroy);

  view->layer.new_popup.notify = on_layer_new_popup;
  wl_signal_add(&surface->events.new_popup, &view->layer.new_popup);

  wlr_log(WLR_INFO, "New layered surface with namespace: (%s)",
          surface->namespace);

  struct nora_output *output =
      nora_output_of_wlr_output(server, surface->output);
  view->output = output;

  wl_list_insert(&server->desktop.layout->layers, &view->link);

  nora_arrange_layers(output);
}

void nora_new_xdg_surface(struct wl_listener *listener, void *data) {
  struct nora_server *server =
      wl_container_of(listener, server, desktop.new_xdg_surface);
  struct wlr_xdg_surface *surface = data;

  assert(surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);

  struct nora_view *view = calloc(1, sizeof(*view));
  view->kind = NORA_VIEW_KIND_XDG;
  view->server = server;

  view->xdg.xdg_toplevel = surface->toplevel;
  view->xdg.scene_tree =
      wlr_scene_xdg_surface_create(&server->scene->tree, surface);


  struct nora_output *output = nora_get_current_output(server);
  view->output = output;
  view->xdg.scene_tree->node.data = view;

  view->view_handle =
      nora_desktop_view_unstable_v1_create(server->desktop.manager);

  view->xdg.map.notify = on_xdg_toplevel_map;
  wl_signal_add(&surface->surface->events.map, &view->xdg.map);
  view->xdg.unmap.notify = on_xdg_toplevel_unmap;
  wl_signal_add(&surface->surface->events.unmap, &view->xdg.unmap);
  view->xdg.destroy.notify = on_xdg_toplevel_destroy;
  wl_signal_add(&surface->events.destroy, &view->xdg.destroy);

  struct wlr_xdg_toplevel *toplevel = surface->toplevel;

  // move event
  view->xdg.request_move.notify = on_xdg_toplevel_request_move;
  wl_signal_add(&toplevel->events.request_move, &view->xdg.request_move);

  // minimize event
  view->xdg.request_resize.notify = on_xdg_toplevel_request_resize;
  wl_signal_add(&toplevel->events.request_resize, &view->xdg.request_resize);

  // maximize event
  view->xdg.request_maximize.notify = on_xdg_toplevel_request_maximize;
  wl_signal_add(&toplevel->events.request_maximize,
                &view->xdg.request_maximize);

  // fullscreen event
  view->xdg.request_fullscreen.notify = on_xdg_toplevel_request_fullscreen;
  wl_signal_add(&toplevel->events.request_fullscreen,
                &view->xdg.request_fullscreen);

  // title event
  view->xdg.set_title.notify = on_xdg_toplevel_title;
  wl_signal_add(&toplevel->events.set_title, &view->xdg.set_title);

  // app id event
  view->xdg.set_app_id.notify = on_xdg_toplevel_app_id;
  wl_signal_add(&toplevel->events.set_app_id, &view->xdg.set_app_id);

  wlr_log(WLR_INFO, "New xdg surface with title (%s)",
          surface->toplevel->title);
}

void nora_focus_view(struct nora_view *view, struct wlr_surface *surface) {
  /* Note: this function only deals with keyboard focus. */
  if (view == NULL) {
    return;
  }

  struct nora_server *server = view->server;
  struct wlr_seat *seat = server->input.seat;
  struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
  if (prev_surface == surface) {
    /* Don't re-focus an already focused surface. */
    return;
  }
  if (prev_surface) {
    /*
     * Deactivate the previously focused surface. This lets the client know
     * it no longer has focus and the client will repaint accordingly, e.g.
     * stop displaying a caret.
     */
    struct wlr_xdg_surface *previous = wlr_xdg_surface_try_from_wlr_surface(
        seat->keyboard_state.focused_surface);
    assert(previous != NULL && previous->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);
    if (previous->toplevel != NULL) {
      wlr_xdg_toplevel_set_activated(previous->toplevel, false);
    }
  }
  struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
  /* Move the view to the front */
  wlr_scene_node_raise_to_top(&view->xdg.scene_tree->node);

  /* Activate the new surface */
  wlr_xdg_toplevel_set_activated(view->xdg.xdg_toplevel, true);
  /*
   * Tell the seat to have the keyboard enter this surface. wlroots will keep
   * track of this and automatically send key events to the appropriate
   * clients without additional work on your part.
   */
  if (keyboard != NULL) {
    wlr_seat_keyboard_notify_enter(seat, view->xdg.xdg_toplevel->base->surface,
                                   keyboard->keycodes, keyboard->num_keycodes,
                                   &keyboard->modifiers);
  }
}

struct nora_view *nora_view_at(struct nora_server *server, double lx, double ly,
                               struct wlr_surface **surface, double *sx,
                               double *sy) {
  struct wlr_scene_node *node =
      wlr_scene_node_at(&server->scene->tree.node, lx, ly, sx, sy);
  if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
    return NULL;
  }

  struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
  struct wlr_scene_surface *scene_surface =
      wlr_scene_surface_try_from_buffer(scene_buffer);
  if (!scene_surface) {
    return NULL;
  }

  *surface = scene_surface->surface;
  struct wlr_scene_tree *tree = node->parent;
  while (tree != NULL && tree->node.data == NULL) {
    tree = tree->node.parent;
  }
  return tree->node.data;
}
