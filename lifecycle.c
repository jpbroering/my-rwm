// SPDX-FileCopyrightText: © 2026 João Paulo Broering
// SPDX-License-Identifier: 0BSD

#include <stddef.h>

#include "lifecycle.h"

bool seat_refs_clear_window(struct SeatWindowRefs *refs, const struct Window *window) {
	bool was_op_window = refs->op_window == window;

	if (refs->focused == window) {
		refs->focused = NULL;
	}
	if (refs->hovered == window) {
		refs->hovered = NULL;
	}
	if (refs->interacted == window) {
		refs->interacted = NULL;
	}
	if (was_op_window) {
		refs->op_window = NULL;
	}

	return was_op_window;
}

void window_refs_clear_seat(struct WindowSeatRefs *refs, const struct Seat *seat) {
	if (refs->pointer_move_requested == seat) {
		refs->pointer_move_requested = NULL;
	}
	if (refs->pointer_resize_requested == seat) {
		refs->pointer_resize_requested = NULL;
	}
}
