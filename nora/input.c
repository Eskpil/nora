#include <stdbool.h>

#include "server.h"
#include "view.h"

#include <wlr/util/log.h>

static void keyboard_handle_modifiers(struct wl_listener *listener,
                                      void *data) {
  /* This event is raised when a modifier key, such as shift or alt, is
   * pressed. We simply communicate this to the client. */
  struct nora_keyboard *keyboard =
      wl_container_of(listener, keyboard, modifiers);
  /*
   * A seat can only have one keyboard, but this is a limitation of the
   * Wayland protocol - not wlroots. We assign all connected keyboards to the
   * same seat. You can swap out the underlying wlr_keyboard like this and
   * wlr_seat handles this transparently.
   */
  wlr_seat_set_keyboard(keyboard->server->input.seat, keyboard->wlr_keyboard);
  /* Send modifiers to the client. */
  wlr_seat_keyboard_notify_modifiers(keyboard->server->input.seat,
                                     &keyboard->wlr_keyboard->modifiers);
}

static void keyboard_handle_key(struct wl_listener *listener, void *data) {
  /* This event is raised when a key is pressed or released. */
  struct nora_keyboard *keyboard = wl_container_of(listener, keyboard, key);
  struct nora_server *server = keyboard->server;
  struct wlr_keyboard_key_event *event = data;
  struct wlr_seat *seat = server->input.seat;

  /* Translate libinput keycode -> xkbcommon */
  // uint32_t keycode = event->keycode + 8;
  /* Get a list of keysyms based on the keymap for this keyboard */
  // const xkb_keysym_t *syms;
  // int nsyms = xkb_state_key_get_syms(
  //		keyboard->wlr_keyboard->xkb_state, keycode, &syms);

  bool handled = false;
  // uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
  // if ((modifiers & WLR_MODIFIER_ALT) &&
  //		event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
  //	/* If alt is held down and this button was _pressed_, we attempt to
  //	 * process it as a compositor keybinding. */
  //	for (int i = 0; i < nsyms; i++) {
  //		handled = handle_keybinding(server, syms[i]);
  //	}
  // }

  if (!handled) {
    /* Otherwise, we pass it along to the client. */
    wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
    wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode,
                                 event->state);
  }
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
  /* This event is raised by the keyboard base wlr_input_device to signal
   * the destruction of the wlr_keyboard. It will no longer receive events
   * and should be destroyed.
   */
  struct nora_keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
  wl_list_remove(&keyboard->modifiers.link);
  wl_list_remove(&keyboard->key.link);
  wl_list_remove(&keyboard->destroy.link);
  wl_list_remove(&keyboard->link);
  free(keyboard);
}

static void new_keyboard(struct nora_server *server,
                         struct wlr_input_device *device) {
  struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);

  struct nora_keyboard *keyboard = calloc(1, sizeof(struct nora_keyboard));
  keyboard->server = server;
  keyboard->wlr_keyboard = wlr_keyboard;

  /* We need to prepare an XKB keymap and assign it to the keyboard. This
   * assumes the defaults (e.g. layout = "us"). */
  struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  struct xkb_keymap *keymap =
      xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);

  wlr_keyboard_set_keymap(wlr_keyboard, keymap);
  xkb_keymap_unref(keymap);
  xkb_context_unref(context);
  wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

  /* Here we set up listeners for keyboard events. */
  keyboard->modifiers.notify = keyboard_handle_modifiers;
  wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
  keyboard->key.notify = keyboard_handle_key;
  wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);
  keyboard->destroy.notify = keyboard_handle_destroy;
  wl_signal_add(&device->events.destroy, &keyboard->destroy);

  wlr_seat_set_keyboard(server->input.seat, keyboard->wlr_keyboard);

  /* And add the keyboard to our list of keyboards */
  wl_list_insert(&server->input.keyboards, &keyboard->link);
}

static void new_pointer(struct nora_server *server,
                        struct wlr_input_device *device) {
  /* We don't do anything special with pointers. All of our pointer handling
   * is proxied through wlr_cursor. On another compositor, you might take this
   * opportunity to do libinput configuration on the device to set
   * acceleration, etc. */
  wlr_cursor_attach_input_device(server->input.cursor, device);
}

static void reset_cursor_mode(struct nora_server *server) {
  /* Reset the cursor mode to passthrough. */
  server->input.cursor_mode = NORA_CURSOR_PASSTHROUGH;
  server->input.grabbed_view = NULL;
}

static void process_cursor_move(struct nora_server *server, uint32_t time) {
  /* Move the grabbed view to the new position. */
  struct nora_view *view = server->input.grabbed_view;

  if (view->kind == NORA_VIEW_KIND_XDG_TOPLEVEL) {
    wlr_scene_node_set_position(&view->xdg_toplevel.scene_tree->node,
                                server->input.cursor->x - server->input.grab_x,
                                server->input.cursor->y - server->input.grab_y);
    return;
  }

  wlr_log(WLR_ERROR, "Tried to move a non XDG surface");
  exit(1);
}

static void process_cursor_resize(struct nora_server *server, uint32_t time) {
  /*
   * Resizing the grabbed view can be a little bit complicated, because we
   * could be resizing from any corner or edge. This not only resizes the view
   * on one or two axes, but can also move the view if you resize from the top
   * or left edges (or top-left corner).
   *
   * Note that I took some shortcuts here. In a more fleshed-out compositor,
   * you'd wait for the client to prepare a buffer at the new size, then
   * commit any movement that was prepared.
   */
  struct nora_view *view = server->input.grabbed_view;

  if (view->kind != NORA_VIEW_KIND_XDG_TOPLEVEL) {
    wlr_log(WLR_ERROR, "Tried to resize a non XDG surface");
    exit(1);
  }

  double border_x = server->input.cursor->x - server->input.grab_x;
  double border_y = server->input.cursor->y - server->input.grab_y;
  int new_left = server->input.grab_geobox.x;
  int new_right = server->input.grab_geobox.x + server->input.grab_geobox.width;
  int new_top = server->input.grab_geobox.y;
  int new_bottom =
      server->input.grab_geobox.y + server->input.grab_geobox.height;

  if (server->input.resize_edges & WLR_EDGE_TOP) {
    new_top = border_y;
    if (new_top >= new_bottom) {
      new_top = new_bottom - 1;
    }
  } else if (server->input.resize_edges & WLR_EDGE_BOTTOM) {
    new_bottom = border_y;
    if (new_bottom <= new_top) {
      new_bottom = new_top + 1;
    }
  }
  if (server->input.resize_edges & WLR_EDGE_LEFT) {
    new_left = border_x;
    if (new_left >= new_right) {
      new_left = new_right - 1;
    }
  } else if (server->input.resize_edges & WLR_EDGE_RIGHT) {
    new_right = border_x;
    if (new_right <= new_left) {
      new_right = new_left + 1;
    }
  }

  struct wlr_box geo_box;
  wlr_xdg_surface_get_geometry(view->xdg_toplevel.xdg_toplevel->base, &geo_box);
  wlr_scene_node_set_position(&view->xdg_toplevel.scene_tree->node,
                              new_left - geo_box.x, new_top - geo_box.y);

  int new_width = new_right - new_left;
  int new_height = new_bottom - new_top;
  wlr_xdg_toplevel_set_size(view->xdg_toplevel.xdg_toplevel, new_width,
                            new_height);
}

static void process_cursor_motion(struct nora_server *server, uint32_t time) {
  /* If the mode is non-passthrough, delegate to those functions. */
  if (server->input.cursor_mode == NORA_CURSOR_MOVE) {
    process_cursor_move(server, time);
    return;
  } else if (server->input.cursor_mode == NORA_CURSOR_RESIZE) {
    process_cursor_resize(server, time);
    return;
  }

  /* Otherwise, find the view under the pointer and send the event along. */
  double sx, sy;
  struct wlr_seat *seat = server->input.seat;
  struct wlr_surface *surface = NULL;
  struct nora_view *view =
      nora_view_at(server, server->input.cursor->x, server->input.cursor->y,
                   &surface, &sx, &sy);
  if (!view) {
    /* If there's no view under the cursor, set the cursor image to a
     * default. This is what makes the cursor image appear when you move it
     * around the screen, not over any views. */
    wlr_cursor_set_xcursor(server->input.cursor, server->input.cursor_mgr,
                           "default");
  }
  if (surface) {
    /*
     * Send pointer enter and motion events.
     *
     * The enter event gives the surface "pointer focus", which is distinct
     * from keyboard focus. You get pointer focus by moving the pointer over
     * a window.
     *
     * Note that wlroots will avoid sending duplicate enter/motion events if
     * the surface has already has pointer focus or if the client is already
     * aware of the coordinates passed.
     */
    wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
    wlr_seat_pointer_notify_motion(seat, time, sx, sy);
  } else {
    /* Clear pointer focus so future button events and such are not sent to
     * the last client to have the cursor over it. */
    wlr_seat_pointer_clear_focus(seat);
  }
}

void nora_input_cursor_motion(struct wl_listener *listener, void *data) {
  /* This event is forwarded by the cursor when a pointer emits a _relative_
   * pointer motion event (i.e. a delta) */
  struct nora_server *server =
      wl_container_of(listener, server, input.cursor_motion);
  struct wlr_pointer_motion_event *event = data;
  /* The cursor doesn't move unless we tell it to. The cursor automatically
   * handles constraining the motion to the output layout, as well as any
   * special configuration applied for the specific input device which
   * generated the event. You can pass NULL for the device if you want to move
   * the cursor around without any input. */
  wlr_cursor_move(server->input.cursor, &event->pointer->base, event->delta_x,
                  event->delta_y);
  process_cursor_motion(server, event->time_msec);
}

void nora_input_cursor_motion_absolute(struct wl_listener *listener,
                                       void *data) {
  /* This event is forwarded by the cursor when a pointer emits an _absolute_
   * motion event, from 0..1 on each axis. This happens, for example, when
   * wlroots is running under a Wayland window rather than KMS+DRM, and you
   * move the mouse over the window. You could enter the window from any edge,
   * so we have to warp the mouse there. There is also some hardware which
   * emits these events. */
  struct nora_server *server =
      wl_container_of(listener, server, input.cursor_motion_absolute);
  struct wlr_pointer_motion_absolute_event *event = data;
  wlr_cursor_warp_absolute(server->input.cursor, &event->pointer->base,
                           event->x, event->y);
  process_cursor_motion(server, event->time_msec);
}

void nora_input_cursor_button(struct wl_listener *listener, void *data) {
  /* This event is forwarded by the cursor when a pointer emits a button
   * event. */
  struct nora_server *server =
      wl_container_of(listener, server, input.cursor_button);
  struct wlr_pointer_button_event *event = data;
  /* Notify the client with pointer focus that a button press has occurred */
  wlr_seat_pointer_notify_button(server->input.seat, event->time_msec,
                                 event->button, event->state);
  double sx, sy;
  struct wlr_surface *surface = NULL;
  struct nora_view *view =
      nora_view_at(server, server->input.cursor->x, server->input.cursor->y,
                   &surface, &sx, &sy);
  if (event->state == WLR_BUTTON_RELEASED) {
    /* If you released any buttons, we exit interactive move/resize mode. */
    reset_cursor_mode(server);
  } else {
    /* Focus that client if the button was _pressed_ */
    nora_focus_view(view, surface);
  }
}

void nora_input_cursor_axis(struct wl_listener *listener, void *data) {
  /* This event is forwarded by the cursor when a pointer emits an axis event,
   * for example when you move the scroll wheel. */
  struct nora_server *server =
      wl_container_of(listener, server, input.cursor_axis);
  struct wlr_pointer_axis_event *event = data;
  /* Notify the client with pointer focus of the axis event. */
  wlr_seat_pointer_notify_axis(server->input.seat, event->time_msec,
                               event->orientation, event->delta,
                               event->delta_discrete, event->source);
}

void nora_input_cursor_frame(struct wl_listener *listener, void *data) {
  /* This event is forwarded by the cursor when a pointer emits an frame
   * event. Frame events are sent after regular pointer events to group
   * multiple events together. For instance, two axis events may happen at the
   * same time, in which case a frame event won't be sent in between. */
  struct nora_server *server =
      wl_container_of(listener, server, input.cursor_frame);
  /* Notify the client with pointer focus of the frame event. */
  wlr_seat_pointer_notify_frame(server->input.seat);
}

void nora_new_input(struct wl_listener *listener, void *data) {
  /* This is event is forwareded when a new input device is made avaliable
   * the seat.
   */
  struct nora_server *server =
      wl_container_of(listener, server, input.new_input);

  struct wlr_input_device *device = data;
  switch (device->type) {
  case WLR_INPUT_DEVICE_KEYBOARD:
    new_keyboard(server, device);
    break;
  case WLR_INPUT_DEVICE_POINTER:
    new_pointer(server, device);
    break;
  default:
    break;
  }
  /* We need to let the wlr_seat know what our capabilities are, which is
   * communiciated to the client. In TinyWL we always have a cursor, even if
   * there are no pointer devices, so we always include that capability. */
  uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
  if (!wl_list_empty(&server->input.keyboards)) {
    caps |= WL_SEAT_CAPABILITY_KEYBOARD;
  }
  wlr_seat_set_capabilities(server->input.seat, caps);
}

void nora_input_seat_request_cursor(struct wl_listener *listener, void *data) {
  struct nora_server *server =
      wl_container_of(listener, server, input.request_cursor);
  /* This event is raised by the seat when a client provides a cursor image */
  struct wlr_seat_pointer_request_set_cursor_event *event = data;
  struct wlr_seat_client *focused_client =
      server->input.seat->pointer_state.focused_client;
  /* This can be sent by any client, so we check to make sure this one is
   * actually has pointer focus first. */
  if (focused_client == event->seat_client) {
    /* Once we've vetted the client, we can tell the cursor to use the
     * provided surface as the cursor image. It will set the hardware cursor
     * on the output that it's currently on and continue to do so as the
     * cursor moves between outputs. */
    wlr_cursor_set_surface(server->input.cursor, event->surface,
                           event->hotspot_x, event->hotspot_y);
  }
}

void nora_input_seat_request_set_selection(struct wl_listener *listener,
                                           void *data) {
  /* This event is raised by the seat when a client wants to set the selection,
   * usually when the user copies something. wlroots allows compositors to
   * ignore such requests if they so choose, but in tinywl we always honor
   */
  struct nora_server *server =
      wl_container_of(listener, server, input.request_set_selection);
  struct wlr_seat_request_set_selection_event *event = data;
  wlr_seat_set_selection(server->input.seat, event->source, event->serial);
}
