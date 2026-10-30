// SPDX-FileCopyrightText: © 2026 João Paulo Broering
// SPDX-License-Identifier: 0BSD

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Ratios have enough precision to represent every split of an int32_t extent. */
#define BSP_RATIO_SCALE UINT64_C(4294967296)

struct ModelRect {
	int32_t x, y, width, height;
};

enum Direction {
	DIRECTION_LEFT,
	DIRECTION_RIGHT,
	DIRECTION_UP,
	DIRECTION_DOWN,
};

enum BspAxis {
	BSP_AXIS_X, /* Side by side. */
	BSP_AXIS_Y, /* One above the other. */
};

struct Workspace;
struct ModelOutput;
struct BspNode;

/* Owned by the caller; removing it clears all workspace/tree references. */
struct ModelWindow {
	struct Workspace *workspace;
	struct BspNode *tile;
	struct ModelWindow *prev, *next;
	bool floating;
	struct ModelRect rect;
	struct ModelRect floating_rect;
	int32_t min_width, min_height;
};

struct BspNode {
	struct BspNode *parent;
	struct BspNode *children[2];
	struct ModelWindow *window; /* Non-NULL only for leaves. */
	enum BspAxis axis;
	uint64_t ratio; /* First child's fraction, in units of BSP_RATIO_SCALE. */
	struct ModelRect rect;
	int32_t min_width, min_height;
};

struct Workspace {
	struct ModelOutput *output;
	size_t number; /* One based. */
	struct BspNode *root;
	struct ModelWindow *first, *last;
	struct ModelWindow *focused, *focused_tiled;
};

struct ModelOutput {
	struct ModelRect bounds;
	struct Workspace *workspaces;
	size_t workspace_count;
	size_t active; /* Zero based; workspace switching is handled by the caller. */
};

/* Output init requires zero-initialized storage. Invalid geometry/count or an
 * allocation failure leaves it unchanged. No Wayland objects are owned here. */
bool model_output_init(struct ModelOutput *output, size_t count, struct ModelRect bounds);
void model_output_finish(struct ModelOutput *output);
bool model_output_set_bounds(struct ModelOutput *output, struct ModelRect bounds);
struct Workspace *model_output_workspace(struct ModelOutput *output, size_t number);
struct Workspace *model_output_active(struct ModelOutput *output);

void model_window_init(struct ModelWindow *window);
void model_window_set_minimum(struct ModelWindow *window, int32_t width, int32_t height);
/* Insert a detached window, splitting the last focused tile if tiled. An
 * allocation failure leaves both the workspace and window unchanged. */
bool model_workspace_add(struct Workspace *workspace, struct ModelWindow *window);
void model_workspace_remove(struct ModelWindow *window);
void model_workspace_focus(struct ModelWindow *window);
void model_workspace_layout(struct Workspace *workspace);

/* Directional operations use tiled windows only. Missing neighbors/boundaries
 * leave state unchanged. Resize step is supplied by the caller in pixels. */
struct ModelWindow *model_neighbor(struct ModelWindow *window, enum Direction direction);
bool model_focus_direction(struct Workspace *workspace, enum Direction direction);
bool model_swap_direction(struct ModelWindow *window, enum Direction direction);
bool model_resize_split(struct ModelWindow *window, enum Direction direction, uint32_t step);
