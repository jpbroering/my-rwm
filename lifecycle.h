// SPDX-FileCopyrightText: © 2026 João Paulo Broering
// SPDX-License-Identifier: 0BSD

#pragma once

#include <stdbool.h>

struct Window;
struct Seat;

struct SeatWindowRefs {
	struct Window *focused;
	struct Window *hovered;
	struct Window *interacted;
	struct Window *op_window;
};

struct WindowSeatRefs {
	struct Seat *pointer_move_requested;
	struct Seat *pointer_resize_requested;
};

/* Clear seat references to window; return whether it was the op target. */
bool seat_refs_clear_window(struct SeatWindowRefs *refs, const struct Window *window);

/* Clear pointer-operation requests that refer to seat. */
void window_refs_clear_seat(struct WindowSeatRefs *refs, const struct Seat *seat);
