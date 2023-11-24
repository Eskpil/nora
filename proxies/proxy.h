#ifndef PROXIES_PROXY_H_
#define PROXIES_PROXY_H_

#include <stdint.h>
#include <wayland-client-core.h>
#include <wayland-client.h>

#include <systemd/sd-bus-vtable.h>
#include <systemd/sd-bus.h>
#include <wayland-util.h>

#define TICKET_COUNT 64

struct nora_interface_ticket {
  struct wl_interface wl_interface;
  uint32_t version;
  enum { pending, completed } status;

  void *answer;
};

struct nora_proxy {
  struct sd_bus *bus;

  struct wl_display *display;
  struct wl_registry *registry;

  struct nora_interface_ticket tickets[TICKET_COUNT];
  size_t ticket_i;
};

struct nora_proxy *nora_proxy_create();
void nora_proxy_destroy(struct nora_proxy *proxy);
void nora_proxy_flush(struct nora_proxy *proxy);

void nora_proxy_add_vtable(struct nora_proxy *proxy, const char *interface,
                           struct sd_bus_vtable *vtable);

void nora_proxy_bind(struct nora_proxy *proxy, struct wl_interface interface,
                     uint32_t version);
void *nora_proxy_extract(struct nora_proxy *proxy,
                         struct wl_interface interface, uint32_t version);

#endif // PROXIES_PROXY_H_