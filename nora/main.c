#include "server.h"

int main(void) {
    struct nora_server_config config = {};

    struct nora_server *server = nora_server_create(config);

    if (!nora_server_run(server)) {
        nora_server_destroy(server);
        return 1;    
    }

    return 0;
}
