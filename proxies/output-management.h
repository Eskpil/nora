#ifndef PROXY_OUTPUT_MANAGEMENT_H_
#define PROXY_OUTPUT_MANAGEMENT_H_

#include <bits/pthreadtypes.h>

#include <stdint.h>
#include <systemd/sd-bus-vtable.h>
#include <systemd/sd-bus.h>

#include "proxy.h"

struct nora_proxy_output {
  struct wl_list link;

  char *name;
  char *description;

  int32_t position[2], physical_size[2];

  int32_t enabled;
  int32_t transform;
  int32_t scale;

  char *make;
  char *serial_number;
  char *model;
};

struct nora_proxy_output_management_state {
  struct nora_proxy *proxy;
  struct wl_list outputs;
};

void *nora_proxy_output_management_create(struct nora_proxy *proxy);
void nora_proxy_output_management_configure(
    struct nora_proxy_output_management_state *state);

int nora_proxy_output_get_position(sd_bus *bus, const char *path,
                                   const char *interface, const char *property,
                                   sd_bus_message *reply, void *userdata,
                                   sd_bus_error *error);

int nora_proxy_output_get_physical_size(sd_bus *bus, const char *path,
                                        const char *interface,
                                        const char *property,
                                        sd_bus_message *reply, void *userdata,
                                        sd_bus_error *error);

static const sd_bus_vtable nora_proxy_output_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Name", "s", NULL, offsetof(struct nora_proxy_output, name),
                    SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Description", "s", NULL,
                    offsetof(struct nora_proxy_output, description),
                    SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Position", "ai", nora_proxy_output_get_position, 0,
                    SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("PhysicalSize", "ai", nora_proxy_output_get_physical_size,
                    0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Enabled", "i", NULL,
                    offsetof(struct nora_proxy_output, enabled),
                    SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Transform", "i", NULL,
                    offsetof(struct nora_proxy_output, transform),
                    SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Scale", "i", NULL,
                    offsetof(struct nora_proxy_output, scale),
                    SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Make", "s", NULL, offsetof(struct nora_proxy_output, make),
                    SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("SerialNumber", "s", NULL,
                    offsetof(struct nora_proxy_output, serial_number),
                    SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Model", "s", NULL,
                    offsetof(struct nora_proxy_output, model),
                    SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_VTABLE_END};

#endif // PROXY_OUTPUT_MANAGEMENT_H_