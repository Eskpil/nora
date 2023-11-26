#ifndef NORA_TREE_H
#define NORA_TREE_H

#include <assert.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>

struct nora_output;
struct nora_server;
struct nora_view;

struct nora_tree_root {
  struct wl_list outputs;

  struct wlr_scene *scene;
  struct wlr_scene_output_layout *scene_output_layout;
};

struct nora_tree_output {
  struct wl_list link; // nora_tree_root::outputs
  struct wl_list workspaces;
  struct wl_list containers;

  struct nora_tree_root *root;

  struct nora_output *output;
};

struct nora_tree_workspace {
  struct wl_list link; // nora_tree_output::workspaces;
  struct wl_list containers;

  struct nora_tree_output *output;

  struct wlr_scene_tree *scene_tree;
};

struct nora_tree_container {
  struct wl_list
      link; // nora_tree_workspace::containers || nora_tree_output::containers
            // || nora_tree_container::children;
  struct wl_list children;

  struct nora_view *view;
  struct wlr_surface *surface;

  bool ownable;
};

struct nora_tree_root *nora_tree_root_create(struct nora_server *server);
void nora_tree_root_attach_output(struct nora_tree_root *root,
                                  struct nora_output *output);
struct nora_tree_container *
nora_tree_root_find_container_by_surface(struct nora_tree_root *root,
                                         struct wlr_surface *surface);
struct nora_tree_container *
nora_tree_root_find_container_at(struct nora_tree_root *root,
                                 struct wlr_surface **surface, double lx,
                                 double ly, double *sx, double *sy);
struct wlr_scene *nora_tree_root_present_scene(struct nora_tree_root *root);
struct nora_tree_workspace *
nora_tree_root_current_workspace(struct nora_tree_root *root);

struct nora_tree_workspace *
nora_tree_output_current_workspace(struct nora_tree_output *output);
void nora_tree_workspace_insert_container(
    struct nora_tree_workspace *workspace,
    struct nora_tree_container *container);
struct nora_tree_container *nora_tree_workspace_find_container_by_surface(
    struct nora_tree_workspace *workspace, struct wlr_surface *surface);
void nora_tree_workspace_disable(struct nora_tree_workspace *workspace);
void nora_tree_workspace_enable(struct nora_tree_workspace *workspace);

void nora_tree_output_insert_container(struct nora_tree_output *output,
                                       struct nora_tree_container *container);
struct nora_tree_container *
nora_tree_output_find_container_by_surface(struct nora_tree_output *output,
                                           struct wlr_surface *surface);
void nora_tree_output_prepare_present(struct nora_tree_output *output);

bool nora_tree_container_is_ownable(struct nora_tree_container *container);
struct nora_tree_container *nora_tree_container_find_container_by_surface(
    struct nora_tree_container *parent, struct wlr_surface *surface);
struct nora_tree_container *nora_tree_container_create();
void nora_tree_container_insert_child(struct nora_tree_container *parent,
                                      struct nora_tree_container *child);

#endif // NORA_TREE_H
