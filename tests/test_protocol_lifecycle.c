// SPDX-FileCopyrightText: © 2026 João Paulo Broering
// SPDX-License-Identifier: 0BSD

/* Exercise the real callbacks and generated requests without a compositor.
 * Including the implementation keeps these tests out of the public API. */
#define main tinyrwm_main
#include "../tinyrwm.c"
#undef main

#undef NDEBUG
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>

struct Request {
	uint32_t object;
	uint16_t opcode;
};

struct Fixture {
	int peer;
	struct Window *window;
	struct Seat *seat;
	uint32_t window_id, node_id, seat_id, output_id;
	struct Request requests[256];
	size_t request_count;
};

static uint32_t object_id(void *object) {
	return wl_proxy_get_id(object);
}

static void capture(struct Fixture *fixture) {
	assert(wl_display_flush(display) >= 0);
	uint32_t wire[4096];
	size_t bytes = 0;
	for (;;) {
		assert(bytes < sizeof(wire));
		ssize_t received = recv(fixture->peer, (char *)wire + bytes,
				sizeof(wire) - bytes, MSG_DONTWAIT);
		if (received < 0 && errno == EINTR) {
			continue;
		}
		if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			break;
		}
		assert(received > 0);
		bytes += received;
	}
	assert(bytes > 0 && bytes % 4 == 0);
	fixture->request_count = 0;
	for (size_t i = 0; i < bytes / 4;) {
		assert(i + 1 < bytes / 4);
		uint32_t size = wire[i + 1] >> 16;
		assert(size >= 8 && size % 4 == 0);
		assert(i + size / 4 <= bytes / 4);
		assert(fixture->request_count < sizeof(fixture->requests) / sizeof(fixture->requests[0]));
		fixture->requests[fixture->request_count++] = (struct Request){
			.object = wire[i],
			.opcode = wire[i + 1] & 0xffff,
		};
		i += size / 4;
	}
}

static size_t count(struct Fixture *fixture, uint32_t object, uint16_t opcode) {
	size_t count = 0;
	for (size_t i = 0; i < fixture->request_count; i++) {
		struct Request *request = &fixture->requests[i];
		if (request->object == object && request->opcode == opcode) {
			count++;
		}
	}
	return count;
}

static void assert_destroy_only(struct Fixture *fixture, uint32_t object, uint16_t opcode) {
	assert(count(fixture, object, opcode) == 1);
	for (size_t i = 0; i < fixture->request_count; i++) {
		struct Request *request = &fixture->requests[i];
		assert(request->object != object || request->opcode == opcode);
	}
}

static void fixture_init(struct Fixture *fixture, bool children) {
	int fd[2];
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
	fixture->peer = fd[1];
	display = wl_display_connect_to_fd(fd[0]);
	assert(display != NULL);
	wm_init();
	window_manager_v1 = (void *)wl_proxy_create((void *)display, &river_window_manager_v1_interface);
	xkb_bindings_v1 = (void *)wl_proxy_create((void *)display, &river_xkb_bindings_v1_interface);
	assert(window_manager_v1 != NULL && xkb_bindings_v1 != NULL);
	if (!children) {
		return;
	}

	struct river_window_v1 *window = (void *)wl_proxy_create((void *)display, &river_window_v1_interface);
	struct river_seat_v1 *seat = (void *)wl_proxy_create((void *)display, &river_seat_v1_interface);
	struct river_output_v1 *output = (void *)wl_proxy_create((void *)display, &river_output_v1_interface);
	assert(window != NULL && seat != NULL && output != NULL);
	wm_handle_window(NULL, window_manager_v1, window);
	wm_handle_seat(NULL, window_manager_v1, seat);
	wm_handle_output(NULL, window_manager_v1, output);
	fixture->window = river_window_v1_get_user_data(window);
	fixture->seat = river_seat_v1_get_user_data(seat);
	assert(fixture->window != NULL && fixture->seat != NULL);
	fixture->window_id = object_id(window);
	fixture->node_id = object_id(fixture->window->node);
	fixture->seat_id = object_id(seat);
	fixture->output_id = object_id(output);
	wm_handle_manage_start(NULL, window_manager_v1);
	window_handle_dimensions(fixture->window, window, 640, 480);
	wm_handle_render_start(NULL, window_manager_v1);
	capture(fixture);
}

static void start_operation(struct Fixture *fixture, bool resize) {
	if (resize) {
		window_handle_pointer_resize_requested(fixture->window, fixture->window->obj,
				fixture->seat->obj, RIVER_WINDOW_V1_EDGES_RIGHT);
	} else {
		window_handle_pointer_move_requested(fixture->window, fixture->window->obj,
				fixture->seat->obj);
	}
	wm_handle_manage_start(NULL, window_manager_v1);
	wm_handle_render_start(NULL, window_manager_v1);
	assert(fixture->seat->window_refs.op_window == fixture->window);
	assert(fixture->seat->op == (resize ? SEAT_OP_RESIZE : SEAT_OP_MOVE));
	capture(fixture);
	assert(count(fixture, fixture->window_id, RIVER_WINDOW_V1_INFORM_RESIZE_START) == (resize ? 1 : 0));
}

static void fixture_finish(struct Fixture *fixture) {
	if (window_manager_v1 != NULL) {
		wm_handle_finished(NULL, window_manager_v1);
	}
	assert(wl_list_empty(&wm.windows));
	assert(wl_list_empty(&wm.seats));
	assert(wl_list_empty(&wm.outputs));
	assert(window_manager_v1 == NULL && xkb_bindings_v1 == NULL);
	wl_display_disconnect(display);
	close(fixture->peer);
}

int main(int argc, char **argv) {
	assert(argc == 2);
	struct Fixture fixture = {0};
	bool unavailable = strcmp(argv[1], "manager-unavailable") == 0;
	fixture_init(&fixture, !unavailable);
	if (unavailable) {
		wm_handle_unavailable(NULL, window_manager_v1);
		assert(should_exit && exit_status == 1);
		capture(&fixture);
	} else {
		bool resize = strcmp(argv[1], "seat-removed-moving") != 0;
		start_operation(&fixture, resize);
		if (strcmp(argv[1], "manager-finished") == 0) {
			wm_handle_finished(NULL, window_manager_v1);
			assert(should_exit && exit_status == 0);
			capture(&fixture);
			assert_destroy_only(&fixture, fixture.window_id, RIVER_WINDOW_V1_DESTROY);
			assert_destroy_only(&fixture, fixture.node_id, RIVER_NODE_V1_DESTROY);
			assert_destroy_only(&fixture, fixture.seat_id, RIVER_SEAT_V1_DESTROY);
			assert_destroy_only(&fixture, fixture.output_id, RIVER_OUTPUT_V1_DESTROY);
		} else if (strcmp(argv[1], "window-closed-resizing") == 0) {
			fixture.seat->window_refs.hovered = fixture.window;
			fixture.seat->window_refs.interacted = fixture.window;
			window_handle_closed(fixture.window, fixture.window->obj);
			wm_handle_manage_start(NULL, window_manager_v1);
			wm_handle_render_start(NULL, window_manager_v1);
			assert(wl_list_empty(&wm.windows));
			struct SeatWindowRefs *refs = &fixture.seat->window_refs;
			assert(!refs->focused && !refs->hovered && !refs->interacted && !refs->op_window);
			assert(fixture.seat->op == SEAT_OP_NONE);
			capture(&fixture);
			assert_destroy_only(&fixture, fixture.window_id, RIVER_WINDOW_V1_DESTROY);
			assert_destroy_only(&fixture, fixture.node_id, RIVER_NODE_V1_DESTROY);
			assert(count(&fixture, fixture.seat_id, RIVER_SEAT_V1_OP_END) == 1);
		} else {
			bool closed = strcmp(argv[1], "seat-and-window-removed") == 0;
			/* Requests queued for a removed seat must never reach window_manage. */
			fixture.window->seat_refs.pointer_move_requested = fixture.seat;
			fixture.window->seat_refs.pointer_resize_requested = fixture.seat;
			if (closed) {
				window_handle_closed(fixture.window, fixture.window->obj);
			}
			seat_handle_removed(fixture.seat, fixture.seat->obj);
			wm_handle_manage_start(NULL, window_manager_v1);
			wm_handle_render_start(NULL, window_manager_v1);
			assert(wl_list_empty(&wm.seats));
			if (closed) {
				assert(wl_list_empty(&wm.windows));
			} else {
				assert(!fixture.window->seat_refs.pointer_move_requested);
				assert(!fixture.window->seat_refs.pointer_resize_requested);
			}
			capture(&fixture);
			assert_destroy_only(&fixture, fixture.seat_id, RIVER_SEAT_V1_DESTROY);
			assert(count(&fixture, fixture.window_id, RIVER_WINDOW_V1_INFORM_RESIZE_END) == (resize && !closed ? 1 : 0));
			if (closed) {
				assert_destroy_only(&fixture, fixture.window_id, RIVER_WINDOW_V1_DESTROY);
			}
		}
	}
	fixture_finish(&fixture);
	return 0;
}
