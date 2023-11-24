#ifndef NORA_INPUT_H_
#define NORA_INPUT_H_

void nora_input_cursor_motion(struct wl_listener *listener, void *data);
void nora_input_cursor_motion_absolute(struct wl_listener *listener, void *data);
void nora_input_cursor_button(struct wl_listener *listener, void *data);
void nora_input_cursor_axis(struct wl_listener *listener, void *data);
void nora_input_cursor_frame(struct wl_listener *listener, void *data);
void nora_new_input(struct wl_listener *listener, void *data);
void nora_input_seat_request_set_selection(struct wl_listener *listener, void *data);
void nora_input_seat_request_cursor(struct wl_listener *listener, void *data);

#endif // NORA_INPUT_H_

