#ifndef NORA_VIEW_H_
#define NORA_VIEW_H_

#include "server.h"

// listener
void nora_new_xdg_surface(struct wl_listener *listener, void *data);
void nora_new_layer_surface(struct wl_listener *listener, void *data);

struct nora_view *nora_view_at(struct nora_server *server, double lx, double ly,
                               struct wlr_surface **surface, double *sx,
                               double *sy);

void nora_focus_view(struct nora_view *view, struct wlr_surface *surface);

#endif // NORA_VIEW_H_
