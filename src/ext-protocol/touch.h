#include <wlr/types/wlr_touch.h>

typedef struct TouchGroup {
	struct wl_list link;
	struct wlr_touch *touch;
	Monitor *m;
} TouchGroup;

static void createtouch(struct wlr_touch *touch);
static void touchdown(struct wl_listener *listener, void *data);
static void touchup(struct wl_listener *listener, void *data);
static void touchframe(struct wl_listener *listener, void *data);
static void touchmotion(struct wl_listener *listener, void *data);
static struct wl_list touches;

static struct wl_listener touch_down = {.notify = touchdown};
static struct wl_listener touch_frame = {.notify = touchframe};
static struct wl_listener touch_motion = {.notify = touchmotion};
static struct wl_listener touch_up = {.notify = touchup};

void createtouch(struct wlr_touch *wlr_touch)
{
	TouchGroup *touch = ecalloc(1, sizeof(TouchGroup));

	touch->touch = wlr_touch;
	wl_list_insert(&touches, &touch->link);
	wlr_cursor_attach_input_device(cursor, &wlr_touch->base);
}

void
touchdown(struct wl_listener *listener, void *data)
{
	struct wlr_touch_down_event *event = data;
	double lx, ly;
	double sx, sy;
	struct wlr_surface *surface;
	Client *c = NULL;
	uint32_t serial = 0;
	Monitor *m;

	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	// Map the input to the appropriate output, to ensure that rotation is
	// handled.
	wl_list_for_each(m, &mons, link) {
		if (m == NULL || m->wlr_output == NULL) {
			continue;
		}
		if (event->touch->output_name != NULL && 0 != strcmp(event->touch->output_name, m->wlr_output->name)) {
			continue;
		}

		wlr_cursor_map_input_to_output(cursor, &event->touch->base, m->wlr_output);
	}

	wlr_cursor_absolute_to_layout_coords(cursor, &event->touch->base, event->x, event->y, &lx, &ly);

	/* Find the client under the pointer and send the event along. */
	xytonode(lx, ly, &surface, &c, NULL, &sx, &sy);
	if (sloppyfocus && c && c->scene->node.enabled && !client_is_unmanaged(c))
		focusclient(c, 0);
	if (surface != NULL) {
		wlr_seat_touch_point_focus(seat, surface, event->time_msec, event->touch_id, sx, sy);
		serial = wlr_seat_touch_notify_down(seat, surface, event->time_msec, event->touch_id, sx, sy);
	}
    else {
		wlr_seat_touch_point_clear_focus(seat, event->time_msec, event->touch_id);
        return;
    }

	if (serial && wlr_seat_touch_num_points(seat) == 1) {
		/* Emulate a mouse click if the touch event wasn't handled */
		struct wlr_pointer_button_event *button_event = data;
		struct wlr_pointer_motion_absolute_event *motion_event = data;
		double dx, dy;

		wlr_cursor_absolute_to_layout_coords(cursor, &motion_event->pointer->base, motion_event->x, motion_event->y, &lx, &ly);
		wlr_cursor_warp_closest(cursor, &motion_event->pointer->base, lx, ly);
		dx = lx - cursor->x;
		dy = ly - cursor->y;
		motionnotify(motion_event->time_msec, &motion_event->pointer->base, dx, dy, dx, dy);

		button_event->button = BTN_LEFT;
		button_event->state = WL_POINTER_BUTTON_STATE_PRESSED;
		buttonpress(listener, button_event);
	}
}

void
touchup(struct wl_listener *listener, void *data)
{
	struct wlr_touch_up_event *event = data;

	if (!wlr_seat_touch_get_point(seat, event->touch_id)) {
		return;
	}

	if (wlr_seat_touch_num_points(seat) == 1) {
		struct wlr_pointer_button_event *button_event = data;

		button_event->button = BTN_LEFT;
		button_event->state = WL_POINTER_BUTTON_STATE_RELEASED;
		buttonpress(listener, button_event);
	}

	wlr_seat_touch_notify_up(seat, event->time_msec, event->touch_id);
	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);
}

void
touchframe(struct wl_listener *listener, void *data)
{
	wlr_seat_touch_notify_frame(seat);
	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);
}

void
touchmotion(struct wl_listener *listener, void *data)
{
	struct wlr_touch_motion_event *event = data;
	double lx, ly;
	double sx, sy;
	struct wlr_surface *surface;
	Client *c = NULL;

	if (!wlr_seat_touch_get_point(seat, event->touch_id)) {
		return;
	}

	wlr_cursor_absolute_to_layout_coords(cursor, &event->touch->base, event->x, event->y, &lx, &ly);
	xytonode(lx, ly, &surface, &c, NULL, &sx, &sy);

        if (sloppyfocus && c && c->scene->node.enabled && !client_is_unmanaged(c))
            focusclient(c, 0);
    if(surface != NULL) {
		wlr_seat_touch_point_focus(seat, surface, event->time_msec, event->touch_id, sx, sy);
    }
    else {
		wlr_seat_touch_point_clear_focus(seat, event->time_msec, event->touch_id);
    }

	wlr_seat_touch_notify_motion(seat, event->time_msec, event->touch_id, sx, sy);
	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);
}
