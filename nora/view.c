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

static void nora_view_begin_interactive(struct nora_view *view,
                                        enum nora_cursor_mode mode,
                                        uint32_t edges) {
  /* This function sets up an interactive move or resize operation, where the
   * compositor stops propagating pointer events to clients and instead
   * consumes them itself, to move or resize windows. */
  struct nora_server *server = view->server;
  struct wlr_surface *focused_surface =
      server->input.seat->pointer_state.focused_surface;
  if (view->xdg_toplevel.xdg_toplevel->base->surface !=
      wlr_surface_get_root_surface(focused_surface)) {
    /* Deny move/resize requests from unfocused clients. */
    return;
  }

  server->input.grabbed_view = view;
  server->input.cursor_mode = mode;

  if (mode == NORA_CURSOR_MOVE) {
    server->input.grab_x =
        server->input.cursor->x - view->xdg_toplevel.scene_tree->node.x;
    server->input.grab_y =
        server->input.cursor->y - view->xdg_toplevel.scene_tree->node.y;
  } else {
    struct wlr_box geo_box;
    wlr_xdg_surface_get_geometry(view->xdg_toplevel.xdg_toplevel->base,
                                 &geo_box);

    double border_x = (view->xdg_toplevel.scene_tree->node.x + geo_box.x) +
                      ((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
    double border_y = (view->xdg_toplevel.scene_tree->node.y + geo_box.y) +
                      ((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
    server->input.grab_x = server->input.cursor->x - border_x;
    server->input.grab_y = server->input.cursor->y - border_y;

    server->input.grab_geobox = geo_box;
    server->input.grab_geobox.x += view->xdg_toplevel.scene_tree->node.x;
    server->input.grab_geobox.y += view->xdg_toplevel.scene_tree->node.y;

    server->input.resize_edges = edges;
  }
}

static void on_xdg_toplevel_map(struct wl_listener *listener, void *data) {
  /* Called when the surface is mapped, or ready to display on-screen. */
  struct nora_view *view = wl_container_of(listener, view, xdg_toplevel.map);
  nora_desktop_insert_view(view->server->desktop.layout, view);
}

static void on_xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
  /* Called when the surface is unmapped, and should no longer be shown. */
  struct nora_view *view = wl_container_of(listener, view, xdg_toplevel.unmap);
  nora_desktop_remove_view(view->server->desktop.layout, view);
}

static void on_xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
  /* Called when the surface is destroyed and should never be shown again. */
  struct nora_view *view =
      wl_container_of(listener, view, xdg_toplevel.destroy);

  nora_desktop_view_handle_unstable_v1_destroy(view->view_handle);

  wl_list_remove(&view->xdg_toplevel.map.link);
  wl_list_remove(&view->xdg_toplevel.unmap.link);
  wl_list_remove(&view->xdg_toplevel.destroy.link);

  wl_list_remove(&view->xdg_toplevel.request_fullscreen.link);
  wl_list_remove(&view->xdg_toplevel.request_maximize.link);
  wl_list_remove(&view->xdg_toplevel.request_resize.link);
  wl_list_remove(&view->xdg_toplevel.request_move.link);

  wl_list_remove(&view->xdg_toplevel.set_app_id.link);
  wl_list_remove(&view->xdg_toplevel.set_title.link);

  free(view);
}

static void on_xdg_toplevel_request_move(struct wl_listener *listener,
                                         void *data) {
  // TODO: Check the current tiling mode in the surface's workspace.

  struct nora_view *view =
      wl_container_of(listener, view, xdg_toplevel.request_move);
  nora_view_begin_interactive(view, NORA_CURSOR_MOVE, 0);
}

static void on_xdg_toplevel_request_resize(struct wl_listener *listener,
                                           void *data) {
  // TODO: Check the current tiling mode in the surface's workspace.

  struct wlr_xdg_toplevel_resize_event *event = data;

  struct nora_view *view =
      wl_container_of(listener, view, xdg_toplevel.request_resize);

  nora_view_begin_interactive(view, NORA_CURSOR_RESIZE, event->edges);
}

// TODO: Implement maximize and fullscreen events.
static void on_xdg_toplevel_request_maximize(struct wl_listener *listener,
                                             void *data) {
  struct nora_view *view =
      wl_container_of(listener, view, xdg_toplevel.request_maximize);
  wlr_xdg_surface_schedule_configure(view->xdg_toplevel.xdg_toplevel->base);
}

static void on_xdg_toplevel_request_fullscreen(struct wl_listener *listener,
                                               void *data) {
  struct nora_view *view =
      wl_container_of(listener, view, xdg_toplevel.request_fullscreen);
  wlr_xdg_surface_schedule_configure(view->xdg_toplevel.xdg_toplevel->base);
}

static void on_xdg_toplevel_title(struct wl_listener *listener, void *data) {
  struct nora_view *view =
      wl_container_of(listener, view, xdg_toplevel.set_title);
  nora_desktop_view_handle_unstable_v1_set_title(
      view->view_handle, view->xdg_toplevel.xdg_toplevel->title);
}

static void on_xdg_toplevel_app_id(struct wl_listener *listener, void *data) {
  struct nora_view *view =
      wl_container_of(listener, view, xdg_toplevel.set_app_id);
  nora_desktop_view_handle_unstable_v1_set_app_id(
      view->view_handle, view->xdg_toplevel.xdg_toplevel->app_id);
}

static void on_xdg_popup_map(struct wl_listener *listener, void *data) {
  struct nora_view *view = wl_container_of(listener, view, xdg_popup.map);
  nora_desktop_insert_view(view->server->desktop.layout, view);
}

static void on_xdg_popup_unmap(struct wl_listener *listener, void *data) {
  struct nora_view *view = wl_container_of(listener, view, xdg_popup.unmap);
}

static void on_xdg_popup_reposition(struct wl_listener *listener, void *data) {
  struct nora_view *view =
      wl_container_of(listener, view, xdg_popup.reposition);
}

static void on_xdg_popup_destroy(struct wl_listener *listener, void *data) {
  struct nora_view *view = wl_container_of(listener, view, xdg_popup.destroy);
}

void nora_new_xdg_toplevel(struct wl_listener *listener, void *data) {
  struct nora_server *server =
      wl_container_of(listener, server, desktop.new_xdg_toplevel);
  struct wlr_xdg_toplevel *toplevel = data;

  struct nora_view *view = calloc(1, sizeof(*view));
  view->server = server;

  struct nora_output *output = nora_get_current_output(server);
  view->output = output;

  view->surface = toplevel->base->surface;

  view->xdg_toplevel.scene_tree =
      wlr_scene_xdg_surface_create(&server->scene->tree, toplevel->base);

  view->kind = NORA_VIEW_KIND_XDG_TOPLEVEL;
  view->xdg_toplevel.scene_tree->node.data = view;

  view->xdg_toplevel.xdg_toplevel = toplevel;

  view->view_handle =
      nora_desktop_view_unstable_v1_create(server->desktop.manager);

  view->xdg_toplevel.map.notify = on_xdg_toplevel_map;
  wl_signal_add(&toplevel->base->surface->events.map, &view->xdg_toplevel.map);
  view->xdg_toplevel.unmap.notify = on_xdg_toplevel_unmap;
  wl_signal_add(&toplevel->base->surface->events.unmap,
                &view->xdg_toplevel.unmap);
  view->xdg_toplevel.destroy.notify = on_xdg_toplevel_destroy;
  wl_signal_add(&toplevel->base->events.destroy, &view->xdg_toplevel.destroy);

  // move event
  view->xdg_toplevel.request_move.notify = on_xdg_toplevel_request_move;
  wl_signal_add(&toplevel->events.request_move,
                &view->xdg_toplevel.request_move);

  // minimize event
  view->xdg_toplevel.request_resize.notify = on_xdg_toplevel_request_resize;
  wl_signal_add(&toplevel->events.request_resize,
                &view->xdg_toplevel.request_resize);

  // maximize event
  view->xdg_toplevel.request_maximize.notify = on_xdg_toplevel_request_maximize;
  wl_signal_add(&toplevel->events.request_maximize,
                &view->xdg_toplevel.request_maximize);

  // fullscreen event
  view->xdg_toplevel.request_fullscreen.notify =
      on_xdg_toplevel_request_fullscreen;
  wl_signal_add(&toplevel->events.request_fullscreen,
                &view->xdg_toplevel.request_fullscreen);

  // title event
  view->xdg_toplevel.set_title.notify = on_xdg_toplevel_title;
  wl_signal_add(&toplevel->events.set_title, &view->xdg_toplevel.set_title);

  // app id event
  view->xdg_toplevel.set_app_id.notify = on_xdg_toplevel_app_id;
  wl_signal_add(&toplevel->events.set_app_id, &view->xdg_toplevel.set_app_id);

  wlr_log(WLR_INFO, "New xdg surface with title (%s)", toplevel->title);
}

void nora_new_xdg_popup(struct wl_listener *listener, void *data) {
  struct nora_server *server =
      wl_container_of(listener, server, desktop.new_xdg_popup);
  struct wlr_xdg_popup *popup = data;

  struct nora_view *view = calloc(1, sizeof(*view));
  view->server = server;

  struct nora_output *output = nora_get_current_output(server);
  view->output = output;

  view->surface = popup->base->surface;

  view->kind = NORA_VIEW_KIND_XDG_POPUP;

  struct wlr_scene_tree *parent_scene_tree = &server->scene->tree;
  if (popup->parent != NULL) {
    struct nora_view *parent =
        nora_view_try_from_wlr_surface(server, popup->parent);
    assert(parent != NULL);
    if (parent->kind == NORA_VIEW_KIND_XDG_POPUP)
      parent_scene_tree = parent->xdg_popup.scene_tree;
    else
      parent_scene_tree = parent->xdg_toplevel.scene_tree;
  }

  view->xdg_popup.scene_tree =
      wlr_scene_xdg_surface_create(parent_scene_tree, popup->base);
  view->xdg_popup.scene_tree->node.data = view;

  view->xdg_popup.xdg_popup = popup;

  view->xdg_popup.map.notify = on_xdg_popup_map;
  wl_signal_add(&popup->base->surface->events.map, &view->xdg_popup.map);
  view->xdg_popup.unmap.notify = on_xdg_popup_unmap;
  wl_signal_add(&popup->base->surface->events.unmap, &view->xdg_popup.unmap);
  view->xdg_popup.destroy.notify = on_xdg_popup_destroy;
  wl_signal_add(&popup->base->events.destroy, &view->xdg_popup.destroy);

  view->xdg_popup.reposition.notify = on_xdg_popup_reposition;
  wl_signal_add(&popup->events.reposition, &view->xdg_popup.reposition);
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

  struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
  if (prev_surface) {
    /*
     * Deactivate the previously focused surface. This lets the client know
     * it no longer has focus and the client will repaint accordingly, e.g.
     * stop displaying a caret.
     */

    struct nora_view *previous = nora_view_try_from_wlr_surface(
        view->server, seat->keyboard_state.focused_surface);
    assert(previous != NULL);

    if (previous->kind == NORA_VIEW_KIND_XDG_TOPLEVEL &&
        previous->xdg_toplevel.xdg_toplevel != NULL)
      wlr_xdg_toplevel_set_activated(previous->xdg_toplevel.xdg_toplevel,
                                     false);
  }

  if (view->kind == NORA_VIEW_KIND_XDG_POPUP) {
    wlr_scene_node_raise_to_top(&view->xdg_popup.scene_tree->node);
    if (keyboard != NULL) {
      wlr_seat_keyboard_notify_enter(
          seat, view->xdg_popup.xdg_popup->base->surface, keyboard->keycodes,
          keyboard->num_keycodes, &keyboard->modifiers);
    }
    return;
  }

  if (view->kind == NORA_VIEW_KIND_LAYER) {
    wlr_log(WLR_ERROR, "implement focusing of layer surfaces");
    return;
  }

  /* Move the view to the front */
  wlr_scene_node_raise_to_top(&view->xdg_toplevel.scene_tree->node);

  /* Activate the new surface */
  wlr_xdg_toplevel_set_activated(view->xdg_toplevel.xdg_toplevel, true);
  /*
   * Tell the seat to have the keyboard enter this surface. wlroots will keep
   * track of this and automatically send key events to the appropriate
   * clients without additional work on your part.
   */
  if (keyboard != NULL) {
    wlr_seat_keyboard_notify_enter(
        seat, view->xdg_toplevel.xdg_toplevel->base->surface,
        keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
  }
}

struct nora_view *nora_view_try_from_wlr_surface(struct nora_server *server,
                                                 struct wlr_surface *surface) {
  struct nora_desktop *desktop = server->desktop.layout;

  struct nora_view *view, *tmp;
  wl_list_for_each_safe(view, tmp, &desktop->views, link) {
    if (view->surface == surface) {
      wlr_log(WLR_INFO, "candidates (>%p) (%p)", view->surface, surface);
      wlr_log(WLR_INFO, "matched");
      return view;
    }
  }

  wlr_log(WLR_INFO, "no match");

  return NULL;
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
