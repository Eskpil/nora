#include "output-management.h"

int nora_proxy_output_get_position(sd_bus *bus, const char *path,
                                   const char *interface, const char *property,
                                   sd_bus_message *reply, void *userdata,
                                   sd_bus_error *error) {
  struct nora_proxy_output *output = userdata;

  int ret = sd_bus_message_open_container(reply, 'a', "i");

  ret = sd_bus_message_append(reply, "i", output->position[0]);
  ret = sd_bus_message_append(reply, "i", output->position[1]);

  ret = sd_bus_message_close_container(reply);
  if (ret < 0)
    return ret;

  return 1;
  return 0;
}

int nora_proxy_output_get_physical_size(sd_bus *bus, const char *path,
                                        const char *interface,
                                        const char *property,
                                        sd_bus_message *reply, void *userdata,
                                        sd_bus_error *error) {
  struct nora_proxy_output *output = userdata;

  int ret = sd_bus_message_open_container(reply, 'a', "i");

  ret = sd_bus_message_append(reply, "i", output->physical_size[0]);
  ret = sd_bus_message_append(reply, "i", output->physical_size[1]);

  ret = sd_bus_message_close_container(reply);
  if (ret < 0)
    return ret;

  return 1;
}
