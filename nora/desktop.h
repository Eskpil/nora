#ifndef NORA_DESKTOP_H_
#define NORA_DESKTOP_H_

#include "server.h"

struct nora_desktop *nora_desktop_create(struct nora_server *server);

void nora_desktop_insert_view(struct nora_desktop *desktop, struct nora_view *view);
void nora_desktop_remove_view(struct nora_desktop *desktop, struct nora_view *view);

#endif // NORA_DESKTOP_H_
