#ifndef NORA_OUTPUT_H_
#define NORA_OUTPUT_H_

#include "backend/wayland.h"
#include "server.h"

struct nora_output *nora_get_current_output(struct nora_server *server);
struct nora_output *nora_output_of_wlr_output(struct nora_server *server,
                                              struct wlr_output *wlr_output);

// listener;
void nora_new_output(struct wl_listener *listener, void *data);

#endif // NORA_OUTPUT_H_
