#include "output.h"
#include "server.h"
#include "wlr/util/log.h"
#include <math.h>
#include <stdlib.h>
#include <wayland-util.h>

static void output_frame(struct wl_listener *listener, void *data) {
  /* This function is called every time an output is ready to display a frame,
   * generally at the output's refresh rate (e.g. 60Hz). */
  struct nora_output *output = wl_container_of(listener, output, frame);
  struct wlr_scene *scene =
      nora_tree_root_present_scene(output->server->tree_root);

  struct wlr_scene_output *scene_output =
      wlr_scene_get_scene_output(scene, output->wlr_output);

  /* Render the scene if needed and commit the output */
  wlr_scene_output_commit(scene_output, NULL);

  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  wlr_scene_output_send_frame_done(scene_output, &now);
}

static void output_request_state(struct wl_listener *listener, void *data) {
  /* This function is called when the backend requests a new state for
   * the output. For example, Wayland and X11 backends request a new mode
   * when the output window is resized. */
  struct nora_output *output = wl_container_of(listener, output, request_state);
  const struct wlr_output_event_request_state *event = data;
  wlr_output_commit_state(output->wlr_output, event->state);
}

static void output_destroy(struct wl_listener *listener, void *data) {
  struct nora_output *output = wl_container_of(listener, output, destroy);

  wl_list_remove(&output->frame.link);
  wl_list_remove(&output->request_state.link);
  wl_list_remove(&output->destroy.link);
  wl_list_remove(&output->link);
  free(output);
}

void nora_new_output(struct wl_listener *listener, void *data) {
  /* This event is raised by the backend when a new output (aka a display or
   * monitor) becomes available. */
  struct nora_server *server =
      wl_container_of(listener, server, desktop.new_output);
  struct wlr_output *wlr_output = data;

  /* Configures the output created by the backend to use our allocator
   * and our renderer. Must be done once, before commiting the output */
  wlr_output_init_render(wlr_output, server->allocator, server->renderer);

  /* The output may be disabled, switch it on. */
  struct wlr_output_state state;
  wlr_output_state_init(&state);
  wlr_output_state_set_enabled(&state, true);

  /* Some backends don't have modes. DRM+KMS does, and we need to set a mode
   * before we can use the output. The mode is a tuple of (width, height,
   * refresh rate), and each monitor supports only a specific set of modes. We
   * just pick the monitor's preferred mode, a more sophisticated compositor
   * would let the user configure it. */
  struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
  if (mode != NULL) {
    wlr_output_state_set_mode(&state, mode);
  }

  struct wlr_output_configuration_v1 *output_configuration =
      wlr_output_configuration_v1_create();
  struct wlr_output_configuration_head_v1 *output_head =
      wlr_output_configuration_head_v1_create(output_configuration, wlr_output);
  wlr_output_manager_v1_set_configuration(server->desktop.output_manager,
                                          output_configuration);

  /* Atomically applies the new output state. */
  wlr_output_commit_state(wlr_output, &state);
  wlr_output_state_finish(&state);

  /* Allocates and configures our state for this output */
  struct nora_output *output = calloc(1, sizeof(struct nora_output));
  output->wlr_output = wlr_output;
  output->server = server;

  /* Sets up a listener for the frame event. */
  output->frame.notify = output_frame;
  wl_signal_add(&wlr_output->events.frame, &output->frame);

  /* Sets up a listener for the state request event. */
  output->request_state.notify = output_request_state;
  wl_signal_add(&wlr_output->events.request_state, &output->request_state);

  /* Sets up a listener for the destroy event. */
  output->destroy.notify = output_destroy;
  wl_signal_add(&wlr_output->events.destroy, &output->destroy);

  wl_list_insert(&server->desktop.outputs, &output->link);

  /* Adds this to the output layout. The add_auto function arranges outputs
   * from left-to-right in the order they appear. A more sophisticated
   * compositor would let the user configure the arrangement of outputs in the
   * layout.
   *
   * The output layout utility automatically adds a wl_output global to the
   * display, which Wayland clients can see to find out information about the
   * output (such as DPI, scale factor, manufacturer, etc).
   */
  struct wlr_output_layout_output *l_output =
      wlr_output_layout_add_auto(server->desktop.output_layout, wlr_output);
  struct wlr_scene_output *scene_output =
      wlr_scene_output_create(server->tree_root->scene, wlr_output);

  wlr_scene_output_layout_add_output(server->tree_root->scene_output_layout,
                                     l_output, scene_output);

  nora_tree_root_attach_output(output->server->tree_root, output);
}

// TODO: Use cursor to determine current output.
struct nora_output *nora_get_current_output(struct nora_server *server) {
  struct nora_output *output = NULL;

  double lx = server->input.seat->pointer_state.sx;
  double ly = server->input.seat->pointer_state.sy;

  // if we are unable to get pointer coordinates for some reason just return the
  // first output.
  if (isnan(lx) || isnan(ly)) {
    wl_list_for_each(output, &server->desktop.outputs, link) { return output; }
  }

  struct wlr_output *wlr_output =
      wlr_output_layout_output_at(server->desktop.output_layout, lx, ly);
  if (!wlr_output) {
    wlr_log(WLR_ERROR, "No output at (%lf, %lf)", lx, ly);
    exit(EXIT_FAILURE);
  }

  wl_list_for_each(output, &server->desktop.outputs, link) {
    if (output->wlr_output == wlr_output) {
      return output;
    }
  }

  return NULL;
}

struct nora_output *nora_output_of_wlr_output(struct nora_server *server,
                                              struct wlr_output *wlr_output) {
  struct nora_output *output = NULL;
  wl_list_for_each(output, &server->desktop.outputs, link) {
    if (output->wlr_output == wlr_output) {
      return output;
    }
  }

  return output;
}
