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
	int32_t args[4];
};

struct Fixture {
	int peer;
	struct Window *window;
	struct Seat *seat;
	struct Output *output;
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
		struct Request *request = &fixture->requests[fixture->request_count++];
		*request = (struct Request){
			.object = wire[i],
			.opcode = wire[i + 1] & 0xffff,
		};
		for (size_t arg = 0; arg < 4 && arg + 2 < size / 4; arg++) {
			memcpy(&request->args[arg], &wire[i + 2 + arg], sizeof(int32_t));
		}
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

static void fixture_init(struct Fixture *fixture, bool children, bool floating) {
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
	fixture->output = river_output_v1_get_user_data(output);
	assert(fixture->window != NULL && fixture->seat != NULL);
	/* These scenarios exercise the existing floating pointer operations. */
	fixture->window->model.floating = floating;
	fixture->window_id = object_id(window);
	fixture->node_id = object_id(fixture->window->node);
	fixture->seat_id = object_id(seat);
	fixture->output_id = object_id(output);
	output_handle_position(fixture->output, output, -100, 50);
	output_handle_dimensions(fixture->output, output, 1000, 600);
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

static void assert_args(struct Fixture *fixture, void *object, uint16_t opcode, int32_t a, int32_t b) {
	assert(count(fixture, object_id(object), opcode) == 1);
	for (size_t i = 0; i < fixture->request_count; i++) {
		struct Request *request = &fixture->requests[i];
		if (request->object == object_id(object) && request->opcode == opcode) {
			assert(request->args[0] == a && request->args[1] == b);
		}
	}
}

static struct Window *create_window(void) {
	struct river_window_v1 *obj = (void *)wl_proxy_create((void *)display, &river_window_v1_interface);
	assert(obj != NULL);
	wm_handle_window(NULL, window_manager_v1, obj);
	return river_window_v1_get_user_data(obj);
}

static struct Output *create_output(void) {
	struct river_output_v1 *obj = (void *)wl_proxy_create((void *)display, &river_output_v1_interface);
	assert(obj != NULL);
	wm_handle_output(NULL, window_manager_v1, obj);
	return river_output_v1_get_user_data(obj);
}

static void manage_render(struct Fixture *fixture) {
	wm_handle_manage_start(NULL, window_manager_v1);
	wm_handle_render_start(NULL, window_manager_v1);
	capture(fixture);
}

static void test_tiled_layout(struct Fixture *fixture) {
	struct Window *a = fixture->window, *b = create_window(), *c = create_window();
	manage_render(fixture);
	assert(a->model.workspace == b->model.workspace && b->model.workspace == c->model.workspace);
	assert_args(fixture, a->obj, RIVER_WINDOW_V1_PROPOSE_DIMENSIONS, 500, 600);
	assert_args(fixture, b->obj, RIVER_WINDOW_V1_PROPOSE_DIMENSIONS, 500, 300);
	assert_args(fixture, c->obj, RIVER_WINDOW_V1_PROPOSE_DIMENSIONS, 500, 300);
	assert_args(fixture, a->node, RIVER_NODE_V1_SET_POSITION, -100, 50);
	assert_args(fixture, b->node, RIVER_NODE_V1_SET_POSITION, 400, 50);
	assert_args(fixture, c->node, RIVER_NODE_V1_SET_POSITION, 400, 350);
	window_handle_closed(b, b->obj);
	manage_render(fixture);
	assert_args(fixture, c->obj, RIVER_WINDOW_V1_PROPOSE_DIMENSIONS, 500, 600);
	assert(fixture->seat->window_refs.focused == c);
	output_handle_position(fixture->output, fixture->output->obj, 10, 20);
	output_handle_dimensions(fixture->output, fixture->output->obj, 1200, 800);
	window_handle_dimensions_hint(a, a->obj, 900, 0, 0, 0);
	manage_render(fixture);
	assert_args(fixture, a->obj, RIVER_WINDOW_V1_PROPOSE_DIMENSIONS, 900, 800);
	assert_args(fixture, c->obj, RIVER_WINDOW_V1_PROPOSE_DIMENSIONS, 300, 800);
	assert_args(fixture, a->node, RIVER_NODE_V1_SET_POSITION, 10, 20);
	assert_args(fixture, c->node, RIVER_NODE_V1_SET_POSITION, 910, 20);
}

static void test_output_removal(struct Fixture *fixture) {
	struct Output *second = create_output();
	output_handle_position(second, second->obj, -1200, 0);
	output_handle_dimensions(second, second->obj, 1200, 800);
	output_handle_removed(fixture->output, fixture->output->obj);
	manage_render(fixture);
	assert(fixture->window->model.workspace->output == &second->model);
	assert_args(fixture, fixture->window->obj, RIVER_WINDOW_V1_PROPOSE_DIMENSIONS, 1200, 800);
	assert_args(fixture, fixture->window->node, RIVER_NODE_V1_SET_POSITION, -1200, 0);
	output_handle_removed(second, second->obj);
	manage_render(fixture);
	assert(!fixture->window->model.workspace && !fixture->window->model.tile);
	struct Output *third = create_output();
	output_handle_dimensions(third, third->obj, 800, 600);
	manage_render(fixture);
	assert(fixture->window->model.workspace->output == &third->model);
	assert_args(fixture, fixture->window->obj, RIVER_WINDOW_V1_PROPOSE_DIMENSIONS, 800, 600);
}

static void test_tiled_pointer(struct Fixture *fixture) {
	window_handle_pointer_resize_requested(fixture->window, fixture->window->obj,
			fixture->seat->obj, RIVER_WINDOW_V1_EDGES_RIGHT);
	window_handle_pointer_move_requested(fixture->window, fixture->window->obj, fixture->seat->obj);
	manage_render(fixture);
	assert(fixture->seat->op == SEAT_OP_NONE && !fixture->seat->window_refs.op_window);
	assert(fixture->window->model.tile != NULL);
	assert(count(fixture, fixture->seat_id, RIVER_SEAT_V1_OP_START_POINTER) == 0);
	assert(count(fixture, fixture->window_id, RIVER_WINDOW_V1_INFORM_RESIZE_START) == 0);
}

static void test_focused_insertion(struct Fixture *fixture) {
	struct Window *a = fixture->window, *b = create_window();
	manage_render(fixture);
	assert(fixture->seat->window_refs.focused == b);
	/* Interaction and creation can arrive before the same manage_start. */
	seat_handle_window_interaction(fixture->seat, fixture->seat->obj, a->obj);
	struct Window *c = create_window();
	manage_render(fixture);
	assert_args(fixture, a->obj, RIVER_WINDOW_V1_PROPOSE_DIMENSIONS, 500, 300);
	assert_args(fixture, b->obj, RIVER_WINDOW_V1_PROPOSE_DIMENSIONS, 500, 600);
	assert_args(fixture, c->node, RIVER_NODE_V1_SET_POSITION, -100, 350);
}

int main(int argc, char **argv) {
	assert(argc == 2);
	struct Fixture fixture = {0};
	bool unavailable = strcmp(argv[1], "manager-unavailable") == 0;
	bool tiled = strcmp(argv[1], "tiled-layout") == 0 || strcmp(argv[1], "output-removal") == 0 ||
		strcmp(argv[1], "tiled-pointer") == 0 || strcmp(argv[1], "focused-insertion") == 0;
	fixture_init(&fixture, !unavailable, !tiled);
	if (strcmp(argv[1], "tiled-layout") == 0) {
		test_tiled_layout(&fixture);
	} else if (strcmp(argv[1], "output-removal") == 0) {
		test_output_removal(&fixture);
	} else if (strcmp(argv[1], "tiled-pointer") == 0) {
		test_tiled_pointer(&fixture);
	} else if (strcmp(argv[1], "focused-insertion") == 0) {
		test_focused_insertion(&fixture);
	} else if (unavailable) {
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
