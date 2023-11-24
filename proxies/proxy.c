#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus-protocol.h>
#include <systemd/sd-bus.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-util.h>

#include "proxy.h"

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface,
                                   uint32_t version) {
  struct nora_proxy *proxy = data;

  for (size_t i = 0; proxy->ticket_i > i; ++i) {
    struct nora_interface_ticket *ticket = &proxy->tickets[i];
    if (strcmp(interface, ticket->wl_interface.name) == 0 &&
        version == ticket->version) {
      ticket->answer = wl_registry_bind(registry, name, &ticket->wl_interface,
                                        ticket->version);
      ticket->status = completed;
    }
  }
}

static void registry_handle_global_remove(void *data,
                                          struct wl_registry *registry,
                                          uint32_t name) {
  // This space deliberately left blank
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

struct nora_proxy *nora_proxy_create() {
  struct nora_proxy *proxy = calloc(1, sizeof(*proxy));

  proxy->display = wl_display_connect(NULL);
  if (!proxy->display) {
    fprintf(
        stderr,
        "could not connect to wayland display, is the compositor running?\n");
    exit(1);
  }

  proxy->registry = wl_display_get_registry(proxy->display);

  wl_registry_add_listener(proxy->registry, &registry_listener, proxy);

  int ret = sd_bus_default_user(&proxy->bus);
  if (0 > ret) {
    fprintf(stderr, "could not connect to dbus: (%s)", strerror(ret));
    exit(1);
  }

  ret = sd_bus_request_name(proxy->bus, "org.nora.proxies", 0);
  if (0 > ret) {
    fprintf(stderr, "could not request dbus name: (%s)", strerror(-ret));
    exit(1);
  }

  printf("is trusted: %d\n", sd_bus_is_trusted(proxy->bus));
  printf("is anonymous: %d\n", sd_bus_is_anonymous(proxy->bus));

  // ret = sd_bus_add_match(
  //     proxy->bus, NULL,
  //     "type='method_call',interface='org.freedesktop.DBus.Properties'", NULL,
  //     NULL);
  // if (ret < 0) {
  //   fprintf(stderr, "Failed to add D-Bus match rule: %s\n", strerror(-ret));
  //   exit(1);
  // }

  return proxy;
}

void nora_proxy_add_vtable(struct nora_proxy *proxy, const char *interface,
                           struct sd_bus_vtable *vtable) {
  int ret =
      sd_bus_add_object_vtable(proxy->bus, NULL, "/", interface, vtable, proxy);

  if (0 > ret) {
    fprintf(stderr, "could not request dbus name: (%s)", strerror(-ret));
    exit(1);
  }
}

void nora_proxy_bind(struct nora_proxy *proxy, struct wl_interface interface,
                     uint32_t version) {
  struct nora_interface_ticket ticket = {
      .version = version,
      .wl_interface = interface,
      .status = pending,
      .answer = NULL,
  };

  proxy->tickets[proxy->ticket_i++] = ticket;
}

void *nora_proxy_extract(struct nora_proxy *proxy,
                         struct wl_interface interface, uint32_t version) {
  for (size_t i = 0; proxy->ticket_i > i; ++i) {
    struct nora_interface_ticket ticket = proxy->tickets[i];
    if (ticket.wl_interface.name == interface.name &&
        ticket.version == version && ticket.status == completed &&
        ticket.answer) {
      return ticket.answer;
    }
  }

  // Safe to exit here because the problem most likely lies in the developer.
  fprintf(stderr, "ticket for interface (%s@%d) is not completed\n",
          interface.name, version);
  exit(1);

  return NULL;
}

void nora_proxy_flush(struct nora_proxy *proxy) {
  wl_display_roundtrip(proxy->display);
}

void nora_proxy_destroy(struct nora_proxy *proxy) {
  sd_bus_unref(proxy->bus);

  free(proxy);
}
