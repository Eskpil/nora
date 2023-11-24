#include <stdlib.h>
#include <wayland-util.h>

#include "desktop.h"
#include "output.h"
#include "server.h"
#include "view.h"

struct nora_desktop *nora_desktop_create(struct nora_server *server) {
  struct nora_desktop *desktop = calloc(1, sizeof(struct nora_desktop));

  desktop->focused_view = NULL;
  desktop->server = server;

  wl_list_init(&desktop->views);
  wl_list_init(&desktop->layers);

  return desktop;
}

// traditional xmonad like master-slave tiling.
void nora_desktop_retile(struct nora_desktop *desktop) {
  wlr_log(WLR_INFO, "retiling");
  int num_views = wl_list_length(&desktop->views);

  // No point in retiling 0 views
  if (!num_views && !desktop->focused_view) {
    return;
  }

  struct nora_output *output = nora_get_current_output(desktop->server);
  struct wlr_box *area = calloc(1, sizeof(*area));

  wlr_output_layout_get_box(desktop->server->desktop.output_layout,
                            output->wlr_output, area);

  int x = area->x;
  int y = area->y;

  int border_width = 12;

  wlr_log(WLR_INFO, "area at (%d, %d) with (%dx%d)", x, y, area->width,
          area->height);

  // step 1. configure width, height, x and y for the focused view.
  {
    int view_x = x + border_width;
    int view_y = y + border_width;

    wlr_scene_node_set_position(&desktop->focused_view->xdg.scene_tree->node,
                                view_x, view_y);

    int view_width = (num_views == 0) ? area->width - (2 * border_width)
                                      : (area->width / 2) - border_width;
    int view_height = area->height - (2 * border_width);

    wlr_xdg_toplevel_set_size(desktop->focused_view->xdg.xdg_toplevel,
                              view_width, view_height);
  }

  // step 2. configure width, height, x and y for all the slaves.
  if (num_views >= 1) {
    struct nora_view *view = NULL;
    int i = 0;
    wl_list_for_each(view, &desktop->views, link) {
      int view_x = x + (area->width / 2) + border_width;
      int view_y = (area->height / num_views) * i + border_width;

      int view_width = (area->width / 2) - border_width * 2;
      int view_height = (area->height / num_views) - border_width * 2;

      wlr_log(WLR_INFO, "tiling window (%s) at (%d, %d) with (%dx%d)",
              view->xdg.xdg_toplevel->title, view_x, view_y, view_width,
              view_height);
      wlr_scene_node_set_position(&view->xdg.scene_tree->node, view_x, view_y);
      wlr_xdg_toplevel_set_size(view->xdg.xdg_toplevel, view_width,
                                view_height);

      i += 1;
    }
  }

  free(area);
}

void nora_desktop_insert_view(struct nora_desktop *desktop,
                              struct nora_view *view) {
  if (view->kind != NORA_VIEW_KIND_XDG) {
    wl_list_insert(&desktop->layers, &view->link);
    return;
  }

  wlr_log(WLR_INFO, "inserted view (%s)", view->xdg.xdg_toplevel->title);
  if (desktop->focused_view == NULL) {
    wlr_log(WLR_INFO, " > electing new master");
    desktop->focused_view = view;
  } else {
    wlr_log(WLR_INFO, " > enslaving");
    wl_list_insert(&desktop->views, &view->link);
  }

  nora_desktop_retile(desktop);
}

void nora_desktop_remove_view(struct nora_desktop *desktop,
                              struct nora_view *view) {
  if (view->kind != NORA_VIEW_KIND_XDG) {
    wl_list_remove(&view->link);
    return;
  }

  if (desktop->focused_view == view) {
    // TODO: Elect new view.
    desktop->focused_view = NULL;
    if (wl_list_length(&desktop->views)) {
      struct nora_view *first_view =
          wl_container_of(&desktop->views.next, first_view, link);
      desktop->focused_view = first_view;
      wl_list_remove(&first_view->link);
    }
    nora_desktop_retile(desktop);
    return;
  }

  wl_list_remove(&view->link);
  nora_desktop_retile(desktop);
}
