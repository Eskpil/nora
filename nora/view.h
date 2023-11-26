#ifndef NORA_VIEW_H_
#define NORA_VIEW_H_

#include "server.h"

enum nora_view_kind {
  NORA_VIEW_KIND_XDG_TOPLEVEL,
  NORA_VIEW_KIND_XDG_POPUP,
  NORA_VIEW_KIND_LAYER,
};

struct nora_view {
  struct wl_list link;

  struct nora_server *server;
  struct nora_tree_container *container;
  struct nora_output *output;
  struct nora_desktop_view_handle_unstable_v1 *view_handle;

  enum nora_view_kind kind;

  int x, y;

  struct wl_listener map;
  struct wl_listener unmap;
  struct wl_listener destroy;

  union {
    struct {
      struct wlr_xdg_toplevel *xdg_toplevel;
      struct wlr_scene_tree *scene_tree;

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

      struct wl_listener reposition;
    } xdg_popup;

    struct {
      struct wlr_layer_surface_v1 *surface;
      struct wlr_scene_layer_surface_v1 *scene_tree;

      struct wl_listener new_popup;
      struct wl_listener commit;
    } layer;
  };
};

// listener
void nora_new_layer_surface(struct wl_listener *listener, void *data);

void nora_new_xdg_toplevel(struct wl_listener *listener, void *data);
void nora_new_xdg_popup(struct wl_listener *listener, void *data);

struct nora_view *nora_view_try_from_wlr_surface(struct nora_server *server,
                                                 struct wlr_surface *surface);

struct nora_view *nora_view_at(struct nora_server *server, double lx, double ly,
                               struct wlr_surface **surface, double *sx,
                               double *sy);

void nora_focus_view(struct nora_view *view, struct wlr_surface *surface);

#endif // NORA_VIEW_H_
