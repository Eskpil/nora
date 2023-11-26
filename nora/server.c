#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_layer_shell_v1.h>

#include "input.h"
#include "nora/desktop/manager.h"
#include "output.h"
#include "server.h"
#include "view.h"

#include "tree.h"

struct nora_server *nora_server_create(struct nora_server_config config) {
  struct nora_server *server = calloc(1, sizeof(struct nora_server));

  wlr_log_init(WLR_DEBUG, NULL);

  server->wl_display = wl_display_create();
  /* The backend is a wlroots feature which abstracts the underlying input and
   * output hardware. The autocreate option will choose the most suitable
   * backend based on the current environment, such as opening an X11 window
   * if an X11 server is running. */
  server->backend = wlr_backend_autocreate(server->wl_display, NULL);
  if (server->backend == NULL) {
    wlr_log(WLR_ERROR, "failed to create wlr_backend");
    return NULL;
  }

  /* Autocreates a renderer, either Pixman, GLES2 or Vulkan for us. The user
   * can also specify a renderer using the WLR_RENDERER env var.
   * The renderer is responsible for defining the various pixel formats it
   * supports for shared memory, this configures that for clients. */
  server->renderer = wlr_renderer_autocreate(server->backend);
  if (server->renderer == NULL) {
    wlr_log(WLR_ERROR, "failed to create wlr_renderer");
    return NULL;
  }

  wlr_renderer_init_wl_display(server->renderer, server->wl_display);

  /* Autocreates an allocator for us.
   * The allocator is the bridge between the renderer and the backend. It
   * handles the buffer creation, allowing wlroots to render onto the
   * screen */
  server->allocator =
      wlr_allocator_autocreate(server->backend, server->renderer);
  if (server->allocator == NULL) {
    wlr_log(WLR_ERROR, "failed to create wlr_allocator");
    return NULL;
  }

  /* This creates some hands-off wlroots interfaces. The compositor is
   * necessary for clients to allocate surfaces, the subcompositor allows to
   * assign the role of subsurfaces to surfaces and the data device manager
   * handles the clipboard. Each of these wlroots interfaces has room for you
   * to dig your fingers in and play with their behavior if you want. Note that
   * the clients cannot set the selection directly without compositor approval,
   * see the handling of the request_set_selection event below.*/
  wlr_compositor_create(server->wl_display, 5, server->renderer);
  wlr_subcompositor_create(server->wl_display);
  wlr_data_device_manager_create(server->wl_display);

  /* Creates an output layout, which a wlroots utility for working with an
   * arrangement of screens in a physical layout. */
  server->desktop.output_layout = wlr_output_layout_create(server->wl_display);

  /* Configure a listener to be notified when new outputs are available on the
   * backend. */
  wl_list_init(&server->desktop.outputs);
  server->desktop.new_output.notify = nora_new_output;
  wl_signal_add(&server->backend->events.new_output,
                &server->desktop.new_output);

  server->desktop.output_manager =
      wlr_output_manager_v1_create(server->wl_display);

  server->presentation =
      wlr_presentation_create(server->wl_display, server->backend);

  server->tree_root = nora_tree_root_create(server);

  server->desktop.xdg_shell = wlr_xdg_shell_create(server->wl_display, 6);

  server->desktop.new_xdg_toplevel.notify = nora_new_xdg_toplevel;
  wl_signal_add(&server->desktop.xdg_shell->events.new_toplevel,
                &server->desktop.new_xdg_toplevel);

  server->desktop.new_xdg_popup.notify = nora_new_xdg_popup;
  wl_signal_add(&server->desktop.xdg_shell->events.new_popup,
                &server->desktop.new_xdg_popup);

  server->desktop.layer_shell =
      wlr_layer_shell_v1_create(server->wl_display, 1);
  server->desktop.new_layer_surface.notify = nora_new_layer_surface;
  wl_signal_add(&server->desktop.layer_shell->events.new_surface,
                &server->desktop.new_layer_surface);

  server->desktop.manager =
      nora_desktop_manager_unstable_v1_create(server->wl_display);

  /* Input
   */
  server->input.seat = wlr_seat_create(server->wl_display, "seat0");

  server->input.cursor = wlr_cursor_create();
  wlr_cursor_attach_output_layout(server->input.cursor,
                                  server->desktop.output_layout);

  server->input.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

  server->input.cursor_mode = NORA_CURSOR_PASSTHROUGH;

  server->input.cursor_motion.notify = nora_input_cursor_motion;
  wl_signal_add(&server->input.cursor->events.motion,
                &server->input.cursor_motion);
  server->input.cursor_motion_absolute.notify =
      nora_input_cursor_motion_absolute;
  wl_signal_add(&server->input.cursor->events.motion_absolute,
                &server->input.cursor_motion_absolute);
  server->input.cursor_button.notify = nora_input_cursor_button;
  wl_signal_add(&server->input.cursor->events.button,
                &server->input.cursor_button);
  server->input.cursor_axis.notify = nora_input_cursor_axis;
  wl_signal_add(&server->input.cursor->events.axis, &server->input.cursor_axis);
  server->input.cursor_frame.notify = nora_input_cursor_frame;
  wl_signal_add(&server->input.cursor->events.frame,
                &server->input.cursor_frame);

  wl_list_init(&server->input.keyboards);
  server->input.new_input.notify = nora_new_input;

  wl_signal_add(&server->backend->events.new_input, &server->input.new_input);
  server->input.request_cursor.notify = nora_input_seat_request_cursor;
  wl_signal_add(&server->input.seat->events.request_set_cursor,
                &server->input.request_cursor);
  server->input.request_set_selection.notify =
      nora_input_seat_request_set_selection;
  wl_signal_add(&server->input.seat->events.request_set_selection,
                &server->input.request_set_selection);

  return server;
}

int nora_server_run(struct nora_server *server) {
  /* Add a Unix socket to the Wayland display. */
  const char *socket = wl_display_add_socket_auto(server->wl_display);
  if (!socket) {
    wlr_backend_destroy(server->backend);
    return 1;
  }

  /* Start the backend. This will enumerate outputs and inputs, become the DRM
   * master, etc */
  if (!wlr_backend_start(server->backend)) {
    wlr_backend_destroy(server->backend);
    wl_display_destroy(server->wl_display);
    return 1;
  }

  /* Set the WAYLAND_DISPLAY environment variable to our socket and run the
   * startup command if requested. */
  setenv("WAYLAND_DISPLAY", socket, true);

  wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s", socket);
  wl_display_run(server->wl_display);

  return 0;
}

int nora_server_destroy(struct nora_server *server) {
  wl_display_destroy_clients(server->wl_display);
  wlr_xcursor_manager_destroy(server->input.cursor_mgr);
  wlr_output_layout_destroy(server->desktop.output_layout);
  wl_display_destroy(server->wl_display);

  free(server);
  return 0;
}
