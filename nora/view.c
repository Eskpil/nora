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

#include "nora/desktop/manager.h"
#include "output.h"
#include "server.h"
#include "view.h"

static void nora_arrange_layers(struct nora_output *output) {
  // TODO Implement layer shell arrangement
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

  struct nora_view *view = wl_container_of(listener, view, map);
  assert(view);

  wlr_log(WLR_INFO, "View: (%s) requested to be mapped",
          view->layer.surface->namespace);
}

static void on_layer_unmap(struct wl_listener *listener, void *data) {
  (void)data;

  struct nora_view *view = wl_container_of(listener, view, unmap);
  assert(view != NULL);

  wlr_log(WLR_INFO, "View: (%s) requested to be unmapped",
          view->layer.surface->namespace);
}

static void on_layer_destroy(struct wl_listener *listener, void *data) {
  (void)data;

  struct nora_view *view = wl_container_of(listener, view, destroy);
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
  view->layer.scene_tree = wlr_scene_layer_surface_v1_create(
      &server->tree_root->scene->tree, surface);
  view->layer.scene_tree->tree->node.data = view;

  view->layer.commit.notify = on_layer_commit;
  wl_signal_add(&surface->surface->events.commit, &view->layer.commit);

  view->map.notify = on_layer_map;
  wl_signal_add(&surface->surface->events.map, &view->map);

  view->unmap.notify = on_layer_unmap;
  wl_signal_add(&surface->surface->events.unmap, &view->unmap);

  view->destroy.notify = on_layer_destroy;
  wl_signal_add(&surface->events.destroy, &view->destroy);

  view->layer.new_popup.notify = on_layer_new_popup;
  wl_signal_add(&surface->events.new_popup, &view->layer.new_popup);

  wlr_log(WLR_INFO, "New layered surface with namespace: (%s)",
          surface->namespace);

  struct nora_output *output =
      nora_output_of_wlr_output(server, surface->output);
  view->output = output;

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
  struct nora_view *view = wl_container_of(listener, view, map);
}

static void on_xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
  /* Called when the surface is unmapped, and should no longer be shown. */
  struct nora_view *view = wl_container_of(listener, view, unmap);
}

static void on_xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
  /* Called when the surface is destroyed and should never be shown again. */
  struct nora_view *view = wl_container_of(listener, view, destroy);

  nora_desktop_view_handle_unstable_v1_destroy(view->view_handle);

  wl_list_remove(&view->map.link);
  wl_list_remove(&view->unmap.link);
  wl_list_remove(&view->destroy.link);

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

static void on_xdg_popup_map(struct wl_listener *listener, void *data) {}

static void on_xdg_popup_unmap(struct wl_listener *listener, void *data) {
  struct nora_view *view = wl_container_of(listener, view, unmap);
}

static void on_xdg_popup_reposition(struct wl_listener *listener, void *data) {
  struct nora_view *view =
      wl_container_of(listener, view, xdg_popup.reposition);
}

static void on_xdg_popup_destroy(struct wl_listener *listener, void *data) {
  struct nora_view *view = wl_container_of(listener, view, destroy);
}

void nora_new_xdg_toplevel(struct wl_listener *listener, void *data) {
  struct nora_server *server =
      wl_container_of(listener, server, desktop.new_xdg_toplevel);
  struct wlr_xdg_toplevel *toplevel = data;

  struct nora_view *view = calloc(1, sizeof(*view));
  view->server = server;

  struct nora_output *output = nora_get_current_output(server);
  view->output = output;

  view->kind = NORA_VIEW_KIND_XDG_TOPLEVEL;
  view->xdg_toplevel.xdg_toplevel = toplevel;

  struct nora_tree_workspace *workspace =
      nora_tree_root_current_workspace(server->tree_root);
  view->xdg_toplevel.scene_tree =
      wlr_scene_xdg_surface_create(workspace->scene_tree, toplevel->base);
  view->xdg_toplevel.scene_tree->node.data = view;

  view->view_handle =
      nora_desktop_view_unstable_v1_create(server->desktop.manager);

  view->map.notify = on_xdg_toplevel_map;
  wl_signal_add(&toplevel->base->surface->events.map, &view->map);
  view->unmap.notify = on_xdg_toplevel_unmap;
  wl_signal_add(&toplevel->base->surface->events.unmap, &view->unmap);
  view->destroy.notify = on_xdg_toplevel_destroy;
  wl_signal_add(&toplevel->base->events.destroy, &view->destroy);

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

  struct nora_tree_container *container = nora_tree_container_create();

  container->ownable = true;
  container->view = view;
  container->surface = toplevel->base->surface;

  view->container = container;

  nora_tree_workspace_insert_container(workspace, container);

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

  view->kind = NORA_VIEW_KIND_XDG_POPUP;

  struct nora_tree_workspace *workspace =
      nora_tree_root_current_workspace(server->tree_root);

  struct wlr_scene_tree *parent_scene_tree = workspace->scene_tree;
  if (popup->parent != NULL) {
    struct nora_tree_container *parent_container =
        nora_tree_root_find_container_by_surface(server->tree_root,
                                                 popup->parent);
    assert(parent_container != NULL);
    struct nora_view *parent = parent_container->view;

    if (parent->kind == NORA_VIEW_KIND_XDG_POPUP)
      parent_scene_tree = parent->xdg_popup.scene_tree;
    else
      parent_scene_tree = parent->xdg_toplevel.scene_tree;
  }

  view->xdg_popup.scene_tree =
      wlr_scene_xdg_surface_create(parent_scene_tree, popup->base);
  view->xdg_popup.scene_tree->node.data = view;

  view->xdg_popup.xdg_popup = popup;

  view->map.notify = on_xdg_popup_map;
  wl_signal_add(&popup->base->surface->events.map, &view->map);
  view->unmap.notify = on_xdg_popup_unmap;
  wl_signal_add(&popup->base->surface->events.unmap, &view->unmap);
  view->destroy.notify = on_xdg_popup_destroy;
  wl_signal_add(&popup->base->events.destroy, &view->destroy);

  view->xdg_popup.reposition.notify = on_xdg_popup_reposition;
  wl_signal_add(&popup->events.reposition, &view->xdg_popup.reposition);

  struct nora_tree_container *container = nora_tree_container_create();
  struct nora_tree_container *parent_container =
      nora_tree_root_find_container_by_surface(view->server->tree_root,
                                               popup->parent);
  assert(parent_container != NULL);

  container->ownable = true;
  container->view = view;
  container->surface = popup->base->surface;

  view->container = container;

  nora_tree_container_insert_child(parent_container, container);
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
    struct nora_tree_container *previous_container =
        nora_tree_root_find_container_by_surface(
            view->server->tree_root, seat->keyboard_state.focused_surface);
    assert(previous_container != NULL);
    struct nora_view *previous = previous_container->view;

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

  assert(view->xdg_toplevel.scene_tree != NULL);

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

struct nora_view *nora_view_at(struct nora_server *server, double lx, double ly,
                               struct wlr_surface **surface, double *sx,
                               double *sy) {

  struct nora_tree_container *container = nora_tree_root_find_container_at(
      server->tree_root, surface, lx, ly, sx, sy);
  if (!container)
    return NULL;

  return container->view;
}
