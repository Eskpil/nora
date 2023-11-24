#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>
#include <wayland-client-protocol.h>
#include <wayland-util.h>

#include "output-management.h"
#include "proxy.h"
#include "wlr-output-management-unstable-v1-client-protocol.h"

static void on_head_name(void *data, struct zwlr_output_head_v1 *head,
                         const char *name) {
  struct nora_proxy_output *output = data;

  output->name = calloc(1, strlen(name));
  strcpy(output->name, name);
}

static void on_head_description(void *data, struct zwlr_output_head_v1 *head,
                                const char *description) {
  struct nora_proxy_output *output = data;

  output->description = calloc(1, strlen(description));
  strcpy(output->description, description);
}

static void on_head_physical_size(void *data, struct zwlr_output_head_v1 *head,
                                  int32_t w, int32_t h) {
  struct nora_proxy_output *output = data;
  output->physical_size[0] = w;
  output->physical_size[1] = h;
}

static void on_head_position(void *data, struct zwlr_output_head_v1 *head,
                             int32_t x, int32_t y) {
  struct nora_proxy_output *output = data;
  output->position[0] = x;
  output->position[1] = y;
}

static void on_head_make(void *data, struct zwlr_output_head_v1 *head,
                         const char *make) {
  struct nora_proxy_output *output = data;

  output->make = calloc(1, strlen(make));
  strcpy(output->make, make);
}

static void on_head_model(void *data, struct zwlr_output_head_v1 *head,
                          const char *model) {
  struct nora_proxy_output *output = data;

  output->model = calloc(1, strlen(model));
  strcpy(output->model, model);
}

static void on_head_serial_number(void *data, struct zwlr_output_head_v1 *head,
                                  const char *serial_number) {
  struct nora_proxy_output *output = data;

  output->serial_number = calloc(1, strlen(serial_number));
  strcpy(output->serial_number, serial_number);
}

static void on_head_enabled(void *data, struct zwlr_output_head_v1 *head,
                            int32_t enabled) {
  struct nora_proxy_output *output = data;
  output->enabled = enabled;
}

static void on_head_transform(void *data, struct zwlr_output_head_v1 *head,
                              int32_t transform) {
  struct nora_proxy_output *output = data;
  output->transform = transform;
}

static void on_head_scale(void *data, struct zwlr_output_head_v1 *head,
                          int32_t scale) {
  struct nora_proxy_output *output = data;
  output->scale = scale;
}

// TODO: Implement this lot.
static void on_head_adaptive_sync(void *data, struct zwlr_output_head_v1 *head,
                                  uint32_t state) {}

static void on_head_current_mode(void *data, struct zwlr_output_head_v1 *head,
                                 struct zwlr_output_mode_v1 *mode) {}

static void on_head_mode(void *data, struct zwlr_output_head_v1 *head,
                         struct zwlr_output_mode_v1 *mode) {}

static void on_head_finished(void *data, struct zwlr_output_head_v1 *head) {}

static const struct zwlr_output_head_v1_listener head_listener = {
    .name = on_head_name,
    .description = on_head_description,
    .physical_size = on_head_physical_size,
    .position = on_head_position,
    .make = on_head_make,
    .model = on_head_model,
    .serial_number = on_head_serial_number,
    .adaptive_sync = on_head_adaptive_sync,
    .current_mode = on_head_current_mode,
    .mode = on_head_mode,
    .finished = on_head_finished,
    .enabled = on_head_enabled,
    .transform = on_head_transform,
    .scale = on_head_scale,
};

static void
on_manager_head(void *data,
                struct zwlr_output_manager_v1 *zwlr_output_manager_v1,
                struct zwlr_output_head_v1 *head) {
  struct nora_proxy_output_management_state *state = data;

  struct nora_proxy_output *output = calloc(1, sizeof(*output));

  char *path = calloc(1, 128);
  sprintf(path, "/org/nora/output/_%d", wl_list_length(&state->outputs));

  int ret =
      sd_bus_add_object_vtable(state->proxy->bus, NULL, path, "org.nora.Output",
                               nora_proxy_output_vtable, output);
  if (0 > ret) {
    fprintf(stderr, "could not add output object: %s\n", strerror(-ret));
    exit(EXIT_FAILURE);
  }

  wl_list_insert(&state->outputs, &output->link);
  zwlr_output_head_v1_add_listener(head, &head_listener, output);
}

static void
on_manager_finished(void *data,
                    struct zwlr_output_manager_v1 *zwlr_output_manager_v1) {}

static void on_manager_done(void *data, struct zwlr_output_manager_v1 *manager,
                            uint32_t serial) {}

static const struct zwlr_output_manager_v1_listener output_manager_listener = {
    .head = on_manager_head,
    .done = on_manager_done,
    .finished = on_manager_finished,
};

void *nora_proxy_output_management_create(struct nora_proxy *proxy) {
  struct nora_proxy_output_management_state *state = calloc(1, sizeof(*state));
  state->proxy = proxy;

  wl_list_init(&state->outputs);

  nora_proxy_bind(proxy, zwlr_output_manager_v1_interface, 4);

  return state;
}

void nora_proxy_output_management_configure(
    struct nora_proxy_output_management_state *state) {
  struct zwlr_output_manager_v1 *manager =
      nora_proxy_extract(state->proxy, zwlr_output_manager_v1_interface, 4);

  zwlr_output_manager_v1_add_listener(manager, &output_manager_listener, state);
}
