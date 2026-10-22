// SPDX-FileCopyrightText: © 2026 João Paulo Broering
// SPDX-License-Identifier: 0BSD

#include <assert.h>
#include <stddef.h>

#include "../lifecycle.h"

struct Window {
	int unused;
};

struct Seat {
	int unused;
};

int main(void) {
	struct Window window = {0}, other_window = {0};
	struct Seat seat = {0}, other_seat = {0};

	struct SeatWindowRefs seat_refs = {
		.focused = &window,
		.hovered = &window,
		.interacted = &other_window,
		.op_window = &window,
	};
	assert(seat_refs_clear_window(&seat_refs, &window));
	assert(seat_refs.focused == NULL);
	assert(seat_refs.hovered == NULL);
	assert(seat_refs.interacted == &other_window);
	assert(seat_refs.op_window == NULL);
	assert(!seat_refs_clear_window(&seat_refs, &window));

	seat_refs.focused = &window;
	seat_refs.op_window = &other_window;
	assert(!seat_refs_clear_window(&seat_refs, &window));
	assert(seat_refs.focused == NULL);
	assert(seat_refs.op_window == &other_window);

	struct WindowSeatRefs window_refs = {
		.pointer_move_requested = &seat,
		.pointer_resize_requested = &other_seat,
	};
	window_refs_clear_seat(&window_refs, &seat);
	assert(window_refs.pointer_move_requested == NULL);
	assert(window_refs.pointer_resize_requested == &other_seat);

	window_refs.pointer_move_requested = &seat;
	window_refs.pointer_resize_requested = &seat;
	window_refs_clear_seat(&window_refs, &seat);
	assert(window_refs.pointer_move_requested == NULL);
	assert(window_refs.pointer_resize_requested == NULL);

	window_refs.pointer_move_requested = &other_seat;
	window_refs.pointer_resize_requested = &other_seat;
	window_refs_clear_seat(&window_refs, &seat);
	assert(window_refs.pointer_move_requested == &other_seat);
	assert(window_refs.pointer_resize_requested == &other_seat);

	return 0;
}
