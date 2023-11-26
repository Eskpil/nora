#include <stdbool.h>

#include "server.h"
#include "tree.h"
#include "view.h"

bool nora_tree_container_is_ownable(struct nora_tree_container *container) {
  return container->ownable;
}

struct nora_tree_container *nora_tree_container_find_container_by_surface(
    struct nora_tree_container *parent, struct wlr_surface *surface) {
  struct nora_tree_container *container, *tmp;
  wl_list_for_each_safe(container, tmp, &parent->children, link) {
    if (container->surface == surface) {
      return container;
    }

    return nora_tree_container_find_container_by_surface(container, surface);
  }

  return NULL;
}

struct nora_tree_container *nora_tree_workspace_find_container_by_surface(
    struct nora_tree_workspace *workspace, struct wlr_surface *surface) {
  struct nora_tree_container *container, *tmp;
  wl_list_for_each_safe(container, tmp, &workspace->containers, link) {
    if (container->surface == surface)
      return container;
  }

  return NULL;
}

struct nora_tree_container *
nora_tree_output_find_container_by_surface(struct nora_tree_output *output,
                                           struct wlr_surface *surface) {
  // For example, layer shells do not belong to a workspace but rather a output.
  struct nora_tree_container *container, *tmp_container;
  wl_list_for_each_safe(container, tmp_container, &output->containers, link) {
    if (container->surface == surface) {
      return container;
    }
  }

  struct nora_tree_workspace *workspace, *tmp_workspace;
  wl_list_for_each_safe(workspace, tmp_workspace, &output->workspaces, link) {
    struct nora_tree_container *container =
        nora_tree_workspace_find_container_by_surface(workspace, surface);
    if (container != NULL) {
      return container;
    }
  }

  return NULL;
}

struct nora_tree_container *
nora_tree_root_find_container_by_surface(struct nora_tree_root *root,
                                         struct wlr_surface *surface) {
  struct nora_tree_output *output, *tmp;
  wl_list_for_each_safe(output, tmp, &root->outputs, link) {
    struct nora_tree_container *container =
        nora_tree_output_find_container_by_surface(output, surface);
    if (container != NULL) {
      return container;
    }
  }

  return NULL;
}

void nora_tree_container_insert_child(struct nora_tree_container *parent,
                                      struct nora_tree_container *child) {
  wl_list_insert(&parent->children, &child->link);
}

void nora_tree_workspace_insert_container(
    struct nora_tree_workspace *workspace,
    struct nora_tree_container *container) {
  wl_list_insert(&workspace->containers, &container->link);
}

struct nora_tree_workspace *
nora_tree_output_current_workspace(struct nora_tree_output *output) {
  struct nora_tree_workspace *workspace, *tmp;
  wl_list_for_each_safe(workspace, tmp, &output->workspaces, link) {
    return workspace;
  }

  return NULL;
}

void nora_tree_output_insert_container(struct nora_tree_output *output,
                                       struct nora_tree_container *container) {
  // check if the container can be owned by workspace
  if (nora_tree_container_is_ownable(container)) {
    struct nora_tree_workspace *workspace =
        nora_tree_output_current_workspace(output);
    assert(workspace != NULL);
    nora_tree_workspace_insert_container(workspace, container);
    return;
  }

  wl_list_insert(&output->containers, &container->link);
}

void nora_tree_workspace_disable(struct nora_tree_workspace *workspace) {
  wlr_scene_node_set_enabled(&workspace->scene_tree->node, false);
}

void nora_tree_workspace_enable(struct nora_tree_workspace *workspace) {
  wlr_scene_node_set_enabled(&workspace->scene_tree->node, true);
}

void nora_tree_output_prepare_present(struct nora_tree_output *output) {
  struct nora_tree_workspace *workspace, *tmp;
  wl_list_for_each_safe(workspace, tmp, &output->workspaces, link) {
    if (workspace != nora_tree_output_current_workspace(output)) {
      nora_tree_workspace_disable(workspace);
    } else {
      nora_tree_workspace_enable(workspace);
    }
  }
}

struct nora_tree_container *
nora_tree_root_find_container_at(struct nora_tree_root *root,
                                 struct wlr_surface **surface, double lx,
                                 double ly, double *sx, double *sy) {
  struct wlr_scene_node *node =
      wlr_scene_node_at(&root->scene->tree.node, lx, ly, sx, sy);
  if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
    return NULL;
  }

  struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
  struct wlr_scene_surface *scene_surface =
      wlr_scene_surface_try_from_buffer(scene_buffer);
  if (!scene_surface) {
    return NULL;
  }

  *surface = scene_surface->surface;
  return node->parent->node.data;
}

struct wlr_scene *nora_tree_root_present_scene(struct nora_tree_root *root) {
  struct nora_tree_output *output, *tmp;
  wl_list_for_each_safe(output, tmp, &root->outputs, link) {
    nora_tree_output_prepare_present(output);
  }

  return root->scene;
}

struct nora_tree_root *nora_tree_root_create(struct nora_server *server) {
  struct nora_tree_root *tree_root = calloc(1, sizeof(*tree_root));

  wl_list_init(&tree_root->outputs);

  tree_root->scene = wlr_scene_create();
  tree_root->scene_output_layout = wlr_scene_attach_output_layout(
      tree_root->scene, server->desktop.output_layout);

  if (server->presentation != NULL)
    wlr_scene_set_presentation(tree_root->scene, server->presentation);

  if (server->linux_dmabuf_v1 != NULL)
    wlr_scene_set_linux_dmabuf_v1(tree_root->scene, server->linux_dmabuf_v1);

  return tree_root;
}

struct nora_tree_workspace *
nora_tree_root_current_workspace(struct nora_tree_root *root) {
  if (wl_list_length(&root->outputs) <= 0) {
    wlr_log(WLR_ERROR, "no outputs present");
    exit(1);
  }

  struct nora_tree_output *output, *tmp;
  wl_list_for_each_safe(output, tmp, &root->outputs, link) {
    wlr_log(WLR_INFO, "here");
    return nora_tree_output_current_workspace(output);
  }

  return NULL;
}

struct nora_tree_container *nora_tree_container_create() {
  struct nora_tree_container *tree_container =
      calloc(1, sizeof(*tree_container));
  wl_list_init(&tree_container->children);
  return tree_container;
}

struct nora_tree_workspace *
nora_tree_workspace_create(struct nora_tree_output *tree_output) {
  struct nora_tree_workspace *tree_workspace =
      calloc(1, sizeof(*tree_workspace));

  tree_workspace->scene_tree =
      wlr_scene_tree_create(&tree_output->root->scene->tree);
  tree_workspace->output = tree_output;

  wl_list_init(&tree_workspace->containers);

  return tree_workspace;
}

void nora_tree_root_attach_output(struct nora_tree_root *root,
                                  struct nora_output *output) {
  struct nora_tree_output *tree_output = calloc(1, sizeof(*tree_output));

  tree_output->root = root;
  tree_output->output = output;

  wl_list_init(&tree_output->workspaces);
  wl_list_init(&tree_output->containers);

  // TODO: Improve this. For example, with workspace names etc.
  struct nora_tree_workspace *workspace =
      nora_tree_workspace_create(tree_output);

  wl_list_insert(&tree_output->workspaces, &workspace->link);
  wl_list_insert(&root->outputs, &tree_output->link);
}
