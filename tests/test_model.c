// SPDX-FileCopyrightText: © 2026 João Paulo Broering
// SPDX-License-Identifier: 0BSD

#undef NDEBUG
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "../model.h"

static struct Workspace *setup(struct ModelOutput *output, struct ModelWindow *windows,
		size_t count, int32_t width, int32_t height) {
	assert(model_output_init(output, 9, (struct ModelRect){0, 0, width, height}));
	for (size_t i = 0; i < count; i++) {
		model_window_init(&windows[i]);
	}
	return model_output_active(output);
}

static void rect_is(struct ModelRect rect, int32_t x, int32_t y, int32_t width, int32_t height) {
	assert(rect.x == x && rect.y == y && rect.width == width && rect.height == height);
}

static size_t check_tree(struct BspNode *node, struct BspNode *parent, struct Workspace *workspace) {
	assert(node->parent == parent);
	assert(node->rect.width >= 0 && node->rect.height >= 0);
	if (node->window != NULL) {
		assert(!node->children[0] && !node->children[1]);
		assert(node->window->workspace == workspace && node->window->tile == node);
		assert(!node->window->floating);
		rect_is(node->window->rect, node->rect.x, node->rect.y, node->rect.width, node->rect.height);
		return 1;
	}
	assert(node->children[0] && node->children[1]);
	assert(node->ratio <= BSP_RATIO_SCALE);
	struct ModelRect a = node->children[0]->rect, b = node->children[1]->rect;
	assert(a.x == node->rect.x && a.y == node->rect.y);
	if (node->axis == BSP_AXIS_X) {
		assert(b.x == (int64_t)a.x + a.width && b.y == a.y);
		assert(a.height == node->rect.height && b.height == node->rect.height);
		assert((int64_t)a.width + b.width == node->rect.width);
	} else {
		assert(b.y == (int64_t)a.y + a.height && b.x == a.x);
		assert(a.width == node->rect.width && b.width == node->rect.width);
		assert((int64_t)a.height + b.height == node->rect.height);
	}
	return check_tree(node->children[0], node, workspace) + check_tree(node->children[1], node, workspace);
}

static void check_workspace(struct Workspace *workspace) {
	size_t tiled = 0;
	struct ModelWindow *prev = NULL;
	bool focused = workspace->focused == NULL, focused_tiled = workspace->focused_tiled == NULL;
	for (struct ModelWindow *window = workspace->first; window != NULL; window = window->next) {
		assert(window->prev == prev && window->workspace == workspace);
		assert((window->tile == NULL) == window->floating);
		tiled += window->tile != NULL;
		focused |= workspace->focused == window;
		focused_tiled |= workspace->focused_tiled == window && !window->floating;
		prev = window;
	}
	assert(prev == workspace->last && focused && focused_tiled);
	if (tiled == 0) {
		assert(workspace->root == NULL && workspace->focused_tiled == NULL);
	} else {
		assert(workspace->focused_tiled != NULL);
		assert(check_tree(workspace->root, NULL, workspace) == tiled);
		struct ModelRect bounds = workspace->output->bounds;
		rect_is(workspace->root->rect, bounds.x, bounds.y, bounds.width, bounds.height);
	}
}

static void test_outputs(void) {
	struct ModelOutput a = {0}, b = {0};
	struct ModelWindow windows[3];
	struct Workspace *workspace = setup(&a, windows, 3, 1000, 600);
	assert(a.workspace_count == 9 && workspace->number == 1 && workspace->output == &a);
	assert(!model_output_workspace(&a, 0) && !model_output_workspace(&a, 10));
	assert(model_output_init(&b, 3, (struct ModelRect){-800, -100, 800, 600}));
	assert(model_workspace_add(workspace, &windows[0]));
	assert(model_workspace_add(model_output_workspace(&a, 2), &windows[1]));
	assert(model_workspace_add(model_output_active(&b), &windows[2]));
	rect_is(windows[0].rect, 0, 0, 1000, 600);
	rect_is(windows[1].rect, 0, 0, 1000, 600);
	rect_is(windows[2].rect, -800, -100, 800, 600);
	assert(!model_workspace_add(workspace, &windows[2]));
	assert(model_output_set_bounds(&a, (struct ModelRect){20, 30, 600, 900}));
	rect_is(windows[0].rect, 20, 30, 600, 900);
	rect_is(windows[1].rect, 20, 30, 600, 900);
	rect_is(windows[2].rect, -800, -100, 800, 600);
	model_output_finish(&a);
	assert(!windows[0].workspace && !windows[0].tile && !windows[0].prev && !windows[0].next);
	assert(!windows[1].workspace && !windows[1].tile);
	assert(windows[2].workspace == model_output_active(&b));
	assert(model_workspace_add(model_output_workspace(&b, 3), &windows[0]));
	model_output_finish(&a); /* Idempotent. */
	model_output_finish(&b);
	assert(!windows[0].workspace && !windows[2].workspace);
	assert(!model_output_init(&a, 0, (struct ModelRect){0}));
	assert(!model_output_init(&a, SIZE_MAX, (struct ModelRect){0}));
	assert(!model_output_init(&a, 1, (struct ModelRect){0, 0, -1, 2}));
	assert(!model_output_init(&a, 1, (struct ModelRect){INT32_MAX, 0, 1, 2}));
	assert(!a.workspaces && !a.workspace_count);
}

static void test_splits(void) {
	struct ModelOutput output = {0};
	struct ModelWindow windows[4];
	struct Workspace *workspace = setup(&output, windows, 4, 1001, 600);
	assert(model_workspace_add(workspace, &windows[0]));
	assert(workspace->root == windows[0].tile);
	assert(model_workspace_add(workspace, &windows[1]));
	rect_is(windows[0].rect, 0, 0, 500, 600);
	rect_is(windows[1].rect, 500, 0, 501, 600);
	assert(workspace->root->ratio == BSP_RATIO_SCALE / 2);
	/* Explicit focus chooses the leaf to split; a tall leaf splits along Y. */
	model_workspace_focus(&windows[0]);
	assert(model_workspace_add(workspace, &windows[2]));
	rect_is(windows[0].rect, 0, 0, 500, 300);
	rect_is(windows[2].rect, 0, 300, 500, 300);
	rect_is(windows[1].rect, 500, 0, 501, 600);
	assert(workspace->focused == &windows[2] && workspace->focused_tiled == &windows[2]);
	/* Floating windows belong to the workspace but never occupy a leaf. */
	windows[3].floating = true;
	windows[3].rect = (struct ModelRect){50, 60, 200, 100};
	windows[3].floating_rect = windows[3].rect;
	assert(model_workspace_add(workspace, &windows[3]));
	assert(!windows[3].tile && workspace->focused == &windows[3]);
	assert(workspace->focused_tiled == &windows[2]);
	model_workspace_layout(workspace);
	rect_is(windows[3].rect, 50, 60, 200, 100);
	check_workspace(workspace);
	model_output_finish(&output);
	assert(windows[3].floating);
	rect_is(windows[3].floating_rect, 50, 60, 200, 100);

	workspace = setup(&output, windows, 2, 600, 600);
	assert(model_workspace_add(workspace, &windows[0]));
	assert(model_workspace_add(workspace, &windows[1]));
	assert(workspace->root->axis == BSP_AXIS_X); /* Square tie is deterministic. */
	model_output_finish(&output);
}

static struct Workspace *grid(struct ModelOutput *output, struct ModelWindow windows[4]) {
	struct Workspace *workspace = setup(output, windows, 4, 1200, 800);
	assert(model_workspace_add(workspace, &windows[0]));
	assert(model_workspace_add(workspace, &windows[1]));
	model_workspace_focus(&windows[0]);
	assert(model_workspace_add(workspace, &windows[2]));
	model_workspace_focus(&windows[1]);
	assert(model_workspace_add(workspace, &windows[3]));
	return workspace;
}

static void test_focus(void) {
	struct ModelOutput output = {0};
	struct ModelWindow windows[4];
	struct Workspace *workspace = grid(&output, windows);
	assert(model_neighbor(&windows[0], DIRECTION_RIGHT) == &windows[1]);
	assert(model_neighbor(&windows[0], DIRECTION_DOWN) == &windows[2]);
	assert(!model_neighbor(&windows[0], DIRECTION_LEFT));
	assert(!model_neighbor(&windows[0], DIRECTION_UP));
	assert(model_neighbor(&windows[3], DIRECTION_LEFT) == &windows[2]);
	assert(model_neighbor(&windows[3], DIRECTION_UP) == &windows[1]);
	model_workspace_focus(&windows[0]);
	assert(!model_focus_direction(workspace, DIRECTION_LEFT));
	assert(workspace->focused == &windows[0]);
	assert(model_focus_direction(workspace, DIRECTION_RIGHT));
	assert(workspace->focused == &windows[1]);
	assert(model_focus_direction(workspace, DIRECTION_DOWN));
	assert(workspace->focused == &windows[3]);
	/* With the left half unsplit, the equally distant right leaves tie. */
	model_workspace_remove(&windows[2]);
	assert(model_neighbor(&windows[0], DIRECTION_RIGHT) == &windows[1]);
	assert(!model_neighbor(NULL, DIRECTION_RIGHT));
	assert(!model_neighbor(&windows[0], (enum Direction)100));
	model_output_finish(&output);
}

static void test_swaps(void) {
	struct ModelOutput output = {0};
	struct ModelWindow windows[4];
	struct Workspace *workspace = grid(&output, windows);
	struct BspNode *root = workspace->root;
	struct BspNode *left = root->children[0], *right = root->children[1];
	struct BspNode *a = windows[0].tile, *b = windows[1].tile;
	model_workspace_focus(&windows[0]);
	assert(model_resize_split(&windows[0], DIRECTION_RIGHT, 100));
	uint64_t ratio = root->ratio;
	assert(model_swap_direction(&windows[0], DIRECTION_RIGHT));
	assert(workspace->root == root && root->children[0] == left && root->children[1] == right);
	assert(root->ratio == ratio && windows[0].tile == b && windows[1].tile == a);
	assert(a->window == &windows[1] && b->window == &windows[0]);
	assert(workspace->focused == &windows[0]);
	rect_is(windows[0].rect, 700, 0, 500, 400);
	rect_is(windows[1].rect, 0, 0, 700, 400);
	assert(!model_swap_direction(&windows[0], DIRECTION_RIGHT));
	check_workspace(workspace);
	model_output_finish(&output);
}

static void test_resize(void) {
	struct ModelOutput output = {0};
	struct ModelWindow windows[4];
	struct Workspace *workspace = grid(&output, windows);
	assert(!model_resize_split(&windows[0], DIRECTION_LEFT, 20));
	assert(!model_resize_split(&windows[0], DIRECTION_UP, 20));
	assert(!model_resize_split(&windows[0], DIRECTION_RIGHT, 0));
	assert(model_resize_split(&windows[0], DIRECTION_RIGHT, 100));
	assert(windows[0].rect.width == 700 && windows[1].rect.width == 500);
	assert(model_resize_split(&windows[0], DIRECTION_DOWN, 80));
	assert(windows[0].rect.height == 480 && windows[2].rect.height == 320);
	assert(windows[1].rect.height == 400 && windows[3].rect.height == 400);
	assert(model_resize_split(&windows[2], DIRECTION_UP, 30));
	assert(windows[0].rect.height == 450 && windows[2].rect.height == 350);
	assert(model_resize_split(&windows[1], DIRECTION_LEFT, 200));
	assert(windows[0].rect.width == 500 && windows[1].rect.width == 700);
	model_window_set_minimum(&windows[1], 300, 1);
	model_window_set_minimum(&windows[3], 400, 1);
	assert(model_resize_split(&windows[0], DIRECTION_RIGHT, UINT32_MAX));
	assert(windows[0].rect.width == 800 && windows[1].rect.width == 400);
	assert(!model_resize_split(&windows[0], DIRECTION_RIGHT, 1));
	assert(model_output_set_bounds(&output, (struct ModelRect){0, 0, 1800, 800}));
	assert(windows[0].rect.width == 1200 && windows[1].rect.width == 600);
	check_workspace(workspace);
	model_output_finish(&output);
}

static void test_remove(void) {
	struct ModelOutput output = {0};
	struct ModelWindow windows[4];
	struct Workspace *workspace = grid(&output, windows);
	struct BspNode *root = workspace->root, *right = root->children[1];
	assert(model_resize_split(&windows[1], DIRECTION_DOWN, 50));
	uint64_t ratio = right->ratio;
	model_workspace_focus(&windows[0]);
	model_workspace_remove(&windows[0]);
	assert(workspace->focused == &windows[2] && workspace->root == root);
	assert(root->children[0] == windows[2].tile && windows[2].tile->parent == root);
	model_workspace_remove(&windows[2]);
	assert(workspace->root == right && !right->parent && right->ratio == ratio);
	assert(workspace->focused == &windows[1]);
	rect_is(windows[1].rect, 0, 0, 1200, 450);
	model_workspace_remove(&windows[1]);
	assert(workspace->root == windows[3].tile && !workspace->root->parent);
	rect_is(windows[3].rect, 0, 0, 1200, 800);
	model_workspace_remove(&windows[3]);
	assert(!workspace->root && !workspace->first && !workspace->last);
	assert(!workspace->focused && !workspace->focused_tiled);
	for (size_t i = 0; i < 4; i++) {
		assert(!windows[i].workspace && !windows[i].tile && !windows[i].prev && !windows[i].next);
	}
	model_workspace_remove(&windows[0]);
	assert(model_workspace_add(workspace, &windows[0]));
	check_workspace(workspace);
	model_output_finish(&output);
}

static void test_minima(void) {
	struct ModelOutput output = {0};
	struct ModelWindow windows[3];
	struct Workspace *workspace = setup(&output, windows, 3, 1000, 600);
	model_window_set_minimum(&windows[0], 700, 100);
	model_window_set_minimum(&windows[1], 200, 100);
	assert(model_workspace_add(workspace, &windows[0]));
	assert(model_workspace_add(workspace, &windows[1]));
	assert(windows[0].rect.width == 700 && windows[1].rect.width == 300);
	assert(workspace->root->ratio == BSP_RATIO_SCALE / 2);
	model_window_set_minimum(&windows[0], 0, 0);
	assert(windows[0].min_width == 1 && windows[0].min_height == 1);
	assert(windows[0].rect.width == 500 && windows[1].rect.width == 500);
	/* Recursive hints: the right column's height minima add; widths take max. */
	model_window_set_minimum(&windows[1], 300, 400);
	model_window_set_minimum(&windows[2], 400, 150);
	assert(model_workspace_add(workspace, &windows[2]));
	assert(windows[1].rect.height == 400 && windows[2].rect.height == 200);
	assert(workspace->root->min_width == 401 && workspace->root->min_height == 550);
	/* Infeasible minima are relaxed without overlap or negative extents. */
	model_window_set_minimum(&windows[0], 600, 100);
	assert(model_output_set_bounds(&output, (struct ModelRect){-5, -6, 100, 60}));
	assert(windows[0].rect.width == 60 && windows[1].rect.width == 40);
	check_workspace(workspace);
	assert(model_output_set_bounds(&output, (struct ModelRect){0, 0, 1, 1}));
	check_workspace(workspace);
	assert(model_output_set_bounds(&output, (struct ModelRect){0}));
	check_workspace(workspace);
	assert(!model_resize_split(&windows[0], DIRECTION_RIGHT, 10));
	model_window_set_minimum(&windows[0], INT32_MAX, INT32_MAX);
	model_window_set_minimum(&windows[1], INT32_MAX, INT32_MAX);
	model_window_set_minimum(&windows[2], INT32_MAX, INT32_MAX);
	assert(model_output_set_bounds(&output, (struct ModelRect){INT32_MIN, 0, INT32_MAX, INT32_MAX}));
	check_workspace(workspace);
	assert(!model_output_set_bounds(&output, (struct ModelRect){INT32_MAX, 0, 1, 1}));
	check_workspace(workspace);
	model_output_finish(&output);
}

static uint32_t random_next(uint32_t *state) {
	*state ^= *state << 13;
	*state ^= *state >> 17;
	*state ^= *state << 5;
	return *state;
}

static void test_transitions(void) {
	struct ModelOutput output = {0};
	struct ModelWindow windows[40];
	struct Workspace *workspace = setup(&output, windows, 40, 1920, 1080);
	uint32_t random = 7;
	for (size_t i = 0; i < 2000; i++) {
		struct ModelWindow *window = &windows[random_next(&random) % 40];
		enum Direction direction = random_next(&random) % 4;
		switch (random_next(&random) % 6) {
		case 0:
			if (window->workspace == NULL) {
				window->floating = random_next(&random) % 5 == 0;
				assert(model_workspace_add(workspace, window));
			}
			break;
		case 1:
			model_workspace_remove(window);
			break;
		case 2:
			model_workspace_focus(window);
			model_focus_direction(workspace, direction);
			break;
		case 3:
			model_swap_direction(window, direction);
			break;
		case 4:
			model_resize_split(window, direction, random_next(&random) % 100);
			break;
		case 5:
			model_window_set_minimum(window, random_next(&random) % 300, random_next(&random) % 300);
			break;
		}
		check_workspace(workspace);
	}
	model_output_finish(&output);
	for (size_t i = 0; i < 40; i++) {
		assert(!windows[i].workspace && !windows[i].tile);
	}
}

int main(int argc, char **argv) {
	assert(argc == 2);
	if (strcmp(argv[1], "outputs") == 0) test_outputs();
	else if (strcmp(argv[1], "splits") == 0) test_splits();
	else if (strcmp(argv[1], "focus") == 0) test_focus();
	else if (strcmp(argv[1], "swaps") == 0) test_swaps();
	else if (strcmp(argv[1], "resize") == 0) test_resize();
	else if (strcmp(argv[1], "remove") == 0) test_remove();
	else if (strcmp(argv[1], "minima") == 0) test_minima();
	else if (strcmp(argv[1], "transitions") == 0) test_transitions();
	else return 1;
	return 0;
}
