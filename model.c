// SPDX-FileCopyrightText: © 2026 João Paulo Broering
// SPDX-License-Identifier: 0BSD

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "model.h"

static int32_t maximum(int32_t a, int32_t b) {
	return a > b ? a : b;
}

static int32_t minimum_sum(int32_t a, int32_t b) {
	return a > INT32_MAX - b ? INT32_MAX : a + b;
}

static bool valid_rect(struct ModelRect rect) {
	return rect.width >= 0 && rect.height >= 0 &&
		(int64_t)rect.x + rect.width <= INT32_MAX &&
		(int64_t)rect.y + rect.height <= INT32_MAX;
}

bool model_output_init(struct ModelOutput *output, size_t count, struct ModelRect bounds) {
	if (count == 0 || count > SIZE_MAX / sizeof(struct Workspace) || !valid_rect(bounds)) {
		return false;
	}
	struct Workspace *workspaces = calloc(count, sizeof(*workspaces));
	if (workspaces == NULL) {
		return false;
	}
	*output = (struct ModelOutput){
		.bounds = bounds,
		.workspaces = workspaces,
		.workspace_count = count,
	};
	for (size_t i = 0; i < count; i++) {
		workspaces[i].output = output;
		workspaces[i].number = i + 1;
	}
	return true;
}

void model_output_finish(struct ModelOutput *output) {
	for (size_t i = 0; i < output->workspace_count; i++) {
		struct Workspace *workspace = &output->workspaces[i];
		while (workspace->first != NULL) {
			model_workspace_remove(workspace->first);
		}
	}
	free(output->workspaces);
	memset(output, 0, sizeof(*output));
}

bool model_output_set_bounds(struct ModelOutput *output, struct ModelRect bounds) {
	if (!valid_rect(bounds)) {
		return false;
	}
	output->bounds = bounds;
	for (size_t i = 0; i < output->workspace_count; i++) {
		model_workspace_layout(&output->workspaces[i]);
	}
	return true;
}

struct Workspace *model_output_workspace(struct ModelOutput *output, size_t number) {
	if (number == 0 || number > output->workspace_count) {
		return NULL;
	}
	return &output->workspaces[number - 1];
}

struct Workspace *model_output_active(struct ModelOutput *output) {
	return output->active < output->workspace_count ? &output->workspaces[output->active] : NULL;
}

void model_window_init(struct ModelWindow *window) {
	*window = (struct ModelWindow){.min_width = 1, .min_height = 1};
}

void model_window_set_minimum(struct ModelWindow *window, int32_t width, int32_t height) {
	window->min_width = maximum(width, 1);
	window->min_height = maximum(height, 1);
	if (window->workspace != NULL) {
		model_workspace_layout(window->workspace);
	}
}

static void measure(struct BspNode *node) {
	if (node->window != NULL) {
		node->min_width = node->window->min_width;
		node->min_height = node->window->min_height;
		return;
	}
	struct BspNode *a = node->children[0], *b = node->children[1];
	measure(a);
	measure(b);
	node->min_width = node->axis == BSP_AXIS_X ? minimum_sum(a->min_width, b->min_width) :
		maximum(a->min_width, b->min_width);
	node->min_height = node->axis == BSP_AXIS_Y ? minimum_sum(a->min_height, b->min_height) :
		maximum(a->min_height, b->min_height);
}

/* Preserve both minima when feasible. Otherwise share space in proportion to
 * the minima, keeping each child nonempty whenever at least two pixels exist. */
static int32_t split_position(struct BspNode *node, int32_t extent, int64_t requested) {
	struct BspNode *a = node->children[0], *b = node->children[1];
	int32_t min_a = node->axis == BSP_AXIS_X ? a->min_width : a->min_height;
	int32_t min_b = node->axis == BSP_AXIS_X ? b->min_width : b->min_height;
	if ((int64_t)min_a + min_b <= extent) {
		if (requested < min_a) {
			return min_a;
		}
		if (requested > extent - min_b) {
			return extent - min_b;
		}
		return requested;
	}
	int32_t cut = (int64_t)extent * min_a / ((int64_t)min_a + min_b);
	if (extent >= 2) {
		if (cut < 1) {
			cut = 1;
		} else if (cut >= extent) {
			cut = extent - 1;
		}
	}
	return cut;
}

static void arrange(struct BspNode *node, struct ModelRect rect) {
	node->rect = rect;
	if (node->window != NULL) {
		node->window->rect = rect;
		return;
	}
	int32_t extent = node->axis == BSP_AXIS_X ? rect.width : rect.height;
	int32_t cut = split_position(node, extent, (uint64_t)extent * node->ratio / BSP_RATIO_SCALE);
	struct ModelRect a = rect, b = rect;
	if (node->axis == BSP_AXIS_X) {
		a.width = cut;
		b.x += cut;
		b.width -= cut;
	} else {
		a.height = cut;
		b.y += cut;
		b.height -= cut;
	}
	arrange(node->children[0], a);
	arrange(node->children[1], b);
}

void model_workspace_layout(struct Workspace *workspace) {
	if (workspace->root != NULL) {
		measure(workspace->root);
		arrange(workspace->root, workspace->output->bounds);
	}
}

void model_workspace_focus(struct ModelWindow *window) {
	if (window->workspace != NULL) {
		window->workspace->focused = window;
		if (window->tile != NULL) {
			window->workspace->focused_tiled = window;
		}
	}
}

static struct ModelWindow *first_tiled(struct BspNode *node) {
	if (node == NULL) {
		return NULL;
	}
	while (node->window == NULL) {
		node = node->children[0];
	}
	return node->window;
}

bool model_workspace_add(struct Workspace *workspace, struct ModelWindow *window) {
	if (window->workspace != NULL) {
		return false;
	}
	struct BspNode *leaf = NULL, *split = NULL, *target = NULL;
	if (!window->floating) {
		leaf = calloc(1, sizeof(*leaf));
		if (leaf == NULL) {
			return false;
		}
		leaf->window = window;
		if (workspace->root != NULL) {
			target = workspace->focused_tiled->tile;
			split = calloc(1, sizeof(*split));
			if (split == NULL) {
				free(leaf);
				return false;
			}
		}
	}
	if (split != NULL) {
		split->parent = target->parent;
		split->axis = target->rect.width >= target->rect.height ? BSP_AXIS_X : BSP_AXIS_Y;
		split->ratio = BSP_RATIO_SCALE / 2;
		split->children[0] = target;
		split->children[1] = leaf;
		if (target->parent == NULL) {
			workspace->root = split;
		} else {
			target->parent->children[target->parent->children[1] == target] = split;
		}
		target->parent = split;
		leaf->parent = split;
	} else if (leaf != NULL) {
		workspace->root = leaf;
	}
	window->workspace = workspace;
	window->tile = leaf;
	window->prev = workspace->last;
	window->next = NULL;
	if (workspace->last != NULL) {
		workspace->last->next = window;
	} else {
		workspace->first = window;
	}
	workspace->last = window;
	model_workspace_focus(window);
	model_workspace_layout(workspace);
	return true;
}

void model_workspace_remove(struct ModelWindow *window) {
	struct Workspace *workspace = window->workspace;
	if (workspace == NULL) {
		return;
	}
	struct BspNode *leaf = window->tile;
	struct ModelWindow *replacement = NULL;
	if (leaf != NULL) {
		struct BspNode *parent = leaf->parent;
		if (parent == NULL) {
			workspace->root = NULL;
		} else {
			struct BspNode *sibling = parent->children[parent->children[0] == leaf];
			sibling->parent = parent->parent;
			if (parent->parent == NULL) {
				workspace->root = sibling;
			} else {
				parent->parent->children[parent->parent->children[1] == parent] = sibling;
			}
			replacement = first_tiled(sibling);
			free(parent);
		}
		free(leaf);
	}
	if (window->prev != NULL) {
		window->prev->next = window->next;
	} else {
		workspace->first = window->next;
	}
	if (window->next != NULL) {
		window->next->prev = window->prev;
	} else {
		workspace->last = window->prev;
	}
	if (workspace->focused_tiled == window) {
		workspace->focused_tiled = replacement != NULL ? replacement : first_tiled(workspace->root);
	}
	if (workspace->focused == window) {
		workspace->focused = workspace->focused_tiled != NULL ? workspace->focused_tiled : workspace->last;
	}
	window->workspace = NULL;
	window->tile = NULL;
	window->prev = NULL;
	window->next = NULL;
	model_workspace_layout(workspace);
}

static uint64_t magnitude(int64_t value) {
	return value < 0 ? -value : value;
}

struct Neighbor {
	struct ModelWindow *window;
	uint64_t distance, perpendicular;
};

static void find_neighbor(struct BspNode *node, struct ModelWindow *origin,
		enum Direction direction, struct Neighbor *best) {
	if (node->window == NULL) {
		find_neighbor(node->children[0], origin, direction, best);
		find_neighbor(node->children[1], origin, direction, best);
		return;
	}
	struct ModelWindow *candidate = node->window;
	int64_t dx = 2 * ((int64_t)candidate->rect.x - origin->rect.x) + candidate->rect.width - origin->rect.width;
	int64_t dy = 2 * ((int64_t)candidate->rect.y - origin->rect.y) + candidate->rect.height - origin->rect.height;
	if ((direction == DIRECTION_LEFT && dx >= 0) ||
			(direction == DIRECTION_RIGHT && dx <= 0) ||
			(direction == DIRECTION_UP && dy >= 0) ||
			(direction == DIRECTION_DOWN && dy <= 0)) {
		return;
	}
	uint64_t distance = magnitude(dx) + magnitude(dy);
	uint64_t perpendicular = direction == DIRECTION_LEFT || direction == DIRECTION_RIGHT ? magnitude(dy) : magnitude(dx);
	if (best->window == NULL || distance < best->distance ||
			(distance == best->distance && perpendicular < best->perpendicular)) {
		*best = (struct Neighbor){candidate, distance, perpendicular};
	}
}

struct ModelWindow *model_neighbor(struct ModelWindow *window, enum Direction direction) {
	if (window == NULL || window->tile == NULL || direction < DIRECTION_LEFT || direction > DIRECTION_DOWN) {
		return NULL;
	}
	struct Neighbor best = {0};
	find_neighbor(window->workspace->root, window, direction, &best);
	return best.window;
}

bool model_focus_direction(struct Workspace *workspace, enum Direction direction) {
	struct ModelWindow *neighbor = model_neighbor(workspace->focused, direction);
	if (neighbor == NULL) {
		return false;
	}
	model_workspace_focus(neighbor);
	return true;
}

bool model_swap_direction(struct ModelWindow *window, enum Direction direction) {
	struct ModelWindow *neighbor = model_neighbor(window, direction);
	if (neighbor == NULL) {
		return false;
	}
	struct BspNode *a = window->tile, *b = neighbor->tile;
	a->window = neighbor;
	b->window = window;
	window->tile = b;
	neighbor->tile = a;
	model_workspace_layout(window->workspace);
	return true;
}

bool model_resize_split(struct ModelWindow *window, enum Direction direction, uint32_t step) {
	if (window == NULL || window->tile == NULL || step == 0 ||
			direction < DIRECTION_LEFT || direction > DIRECTION_DOWN) {
		return false;
	}
	enum BspAxis axis = direction == DIRECTION_LEFT || direction == DIRECTION_RIGHT ? BSP_AXIS_X : BSP_AXIS_Y;
	bool positive = direction == DIRECTION_RIGHT || direction == DIRECTION_DOWN;
	struct BspNode *child = window->tile;
	for (struct BspNode *node = child->parent; node != NULL; child = node, node = node->parent) {
		if (node->axis != axis || (node->children[0] == child) != positive) {
			continue;
		}
		int32_t extent = axis == BSP_AXIS_X ? node->rect.width : node->rect.height;
		int32_t cut = axis == BSP_AXIS_X ? node->children[0]->rect.width : node->children[0]->rect.height;
		int32_t resized = split_position(node, extent, (int64_t)cut + (positive ? (int64_t)step : -(int64_t)step));
		if (resized == cut || extent == 0) {
			return false;
		}
		node->ratio = ((uint64_t)resized * BSP_RATIO_SCALE + extent - 1) / extent;
		model_workspace_layout(window->workspace);
		return true;
	}
	return false;
}
