/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@helixcode.com>
 *
 * Copyright 1999, Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * EDayView - displays the Day & Work-Week views of the calendar.
 */

#include <math.h>
#include <time.h>
#include <gnome.h>
#include <gdk/gdkx.h>

#include "e-day-view.h"
#include "e-day-view-time-item.h"
#include "e-day-view-top-item.h"
#include "e-day-view-main-item.h"
#include "main.h"
#include "timeutil.h"
#include "popup-menu.h"
#include "eventedit.h"
#include "../e-util/e-canvas.h"
#include "../widgets/e-text/e-text.h"

/* Images */
#include "bell.xpm"
#include "recur.xpm"

/* The minimum amount of space wanted on each side of the date string. */
#define E_DAY_VIEW_DATE_X_PAD	4

#define E_DAY_VIEW_LARGE_FONT	\
	"-adobe-utopia-regular-r-normal-*-*-240-*-*-p-*-iso8859-*"
#define E_DAY_VIEW_LARGE_FONT_FALLBACK	\
	"-adobe-helvetica-bold-r-normal-*-*-240-*-*-p-*-iso8859-*"

/* The offset from the top/bottom of the canvas before auto-scrolling starts.*/
#define E_DAY_VIEW_AUTO_SCROLL_OFFSET	16

/* The time between each auto-scroll, in milliseconds. */
#define E_DAY_VIEW_AUTO_SCROLL_TIMEOUT	50

/* The number of timeouts we skip before we start scrolling. */
#define E_DAY_VIEW_AUTO_SCROLL_DELAY	5

/* The number of pixels the mouse has to be moved with the button down before
   we start a drag. */
#define E_DAY_VIEW_DRAG_START_OFFSET	4

/* Drag and Drop stuff. */
enum {
	TARGET_CALENDAR_EVENT
};
static GtkTargetEntry target_table[] = {
	{ "application/x-e-calendar-event",     0, TARGET_CALENDAR_EVENT }
};
static guint n_targets = sizeof(target_table) / sizeof(target_table[0]);

static void e_day_view_class_init (EDayViewClass *class);
static void e_day_view_init (EDayView *day_view);
static void e_day_view_destroy (GtkObject *object);
static void e_day_view_realize (GtkWidget *widget);
static void e_day_view_unrealize (GtkWidget *widget);
static void e_day_view_style_set (GtkWidget *widget,
				  GtkStyle  *previous_style);
static void e_day_view_size_allocate (GtkWidget *widget,
				      GtkAllocation *allocation);
static gint e_day_view_focus_in (GtkWidget *widget,
				 GdkEventFocus *event);
static gint e_day_view_focus_out (GtkWidget *widget,
				  GdkEventFocus *event);
static gint e_day_view_key_press (GtkWidget *widget,
				  GdkEventKey *event);

static void e_day_view_on_canvas_realized (GtkWidget *widget,
					   EDayView *day_view);

static gboolean e_day_view_on_top_canvas_button_press (GtkWidget *widget,
						       GdkEventButton *event,
						       EDayView *day_view);
static gboolean e_day_view_on_top_canvas_button_release (GtkWidget *widget,
							 GdkEventButton *event,
							 EDayView *day_view);
static gboolean e_day_view_on_top_canvas_motion (GtkWidget *widget,
						 GdkEventMotion *event,
						 EDayView *day_view);

static gboolean e_day_view_on_main_canvas_button_press (GtkWidget *widget,
							GdkEventButton *event,
							EDayView *day_view);
static gboolean e_day_view_on_main_canvas_button_release (GtkWidget *widget,
							  GdkEventButton *event,
							  EDayView *day_view);
static gboolean e_day_view_on_main_canvas_motion (GtkWidget *widget,
						  GdkEventMotion *event,
						  EDayView *day_view);
static gboolean e_day_view_convert_event_coords (EDayView *day_view,
						 GdkEvent *event,
						 GdkWindow *window,
						 gint *x_return,
						 gint *y_return);
static void e_day_view_update_selection (EDayView *day_view,
					 gint row,
					 gint col);
static void e_day_view_update_long_event_resize (EDayView *day_view,
						 gint day);
static void e_day_view_update_resize (EDayView *day_view,
				      gint row);
static void e_day_view_finish_long_event_resize (EDayView *day_view);
static void e_day_view_finish_resize (EDayView *day_view);
static void e_day_view_abort_resize (EDayView *day_view,
				     guint32 time);


static gboolean e_day_view_on_long_event_button_press (EDayView		*day_view,
						       gint		 event_num,
						       GdkEventButton	*event,
						       EDayViewPosition  pos,
						       gint		 event_x,
						       gint		 event_y);
static gboolean e_day_view_on_event_button_press (EDayView	 *day_view,
						  gint		  day,
						  gint		  event_num,
						  GdkEventButton *event,
						  EDayViewPosition pos,
						  gint		  event_x,
						  gint		  event_y);
static void e_day_view_on_long_event_click (EDayView *day_view,
					    gint event_num,
					    GdkEventButton  *bevent,
					    EDayViewPosition pos,
					    gint	     event_x,
					    gint	     event_y);
static void e_day_view_on_event_click (EDayView *day_view,
				       gint day,
				       gint event_num,
				       GdkEventButton  *event,
				       EDayViewPosition pos,
				       gint		event_x,
				       gint		event_y);
static void e_day_view_on_event_double_click (EDayView *day_view,
					      gint day,
					      gint event_num);
static void e_day_view_on_event_right_click (EDayView *day_view,
					     GdkEventButton *bevent,
					     gint day,
					     gint event_num);

static void e_day_view_recalc_num_rows	(EDayView	*day_view);

static EDayViewPosition e_day_view_convert_position_in_top_canvas (EDayView *day_view,
								   gint x,
								   gint y,
								   gint *day_return,
								   gint *event_num_return);
static EDayViewPosition e_day_view_convert_position_in_main_canvas (EDayView *day_view,
								    gint x,
								    gint y,
								    gint *day_return,
								    gint *row_return,
								    gint *event_num_return);
static gboolean e_day_view_find_event_from_item (EDayView *day_view,
						 GnomeCanvasItem *item,
						 gint *day_return,
						 gint *event_num_return);
static gboolean e_day_view_find_event_from_ico (EDayView *day_view,
						iCalObject *ico,
						gint *day_return,
						gint *event_num_return);

static void e_day_view_reload_events (EDayView *day_view);
static void e_day_view_free_events (EDayView *day_view);
static void e_day_view_free_event_array (EDayView *day_view,
					 GArray *array);
static int e_day_view_add_event (iCalObject *ico,
				 time_t	  start,
				 time_t	  end,
				 gpointer data);
static void e_day_view_update_event_label (EDayView *day_view,
					   gint day,
					   gint event_num);
static void e_day_view_update_long_event_label (EDayView *day_view,
						gint event_num);

static void e_day_view_layout_long_events (EDayView *day_view);
static void e_day_view_layout_long_event (EDayView	   *day_view,
					  EDayViewEvent *event,
					  guint8	   *grid);
static void e_day_view_reshape_long_events (EDayView *day_view);
static void e_day_view_reshape_long_event (EDayView *day_view,
					   gint event_num);
static void e_day_view_layout_day_events (EDayView *day_view,
					  gint	day);
static void e_day_view_layout_day_event (EDayView   *day_view,
					 gint	    day,
					 EDayViewEvent *event,
					 guint8	   *grid,
					 guint16   *group_starts);
static void e_day_view_expand_day_event (EDayView	   *day_view,
					 gint	    day,
					 EDayViewEvent *event,
					 guint8	   *grid);
static void e_day_view_recalc_cols_per_row (EDayView *day_view,
					    gint      day,
					    guint16  *group_starts);
static void e_day_view_reshape_day_events (EDayView *day_view,
					   gint day);
static void e_day_view_reshape_day_event (EDayView *day_view,
					  gint	day,
					  gint	event_num);
static void e_day_view_reshape_main_canvas_resize_bars (EDayView *day_view);
static void e_day_view_reshape_resize_long_event_rect_item (EDayView *day_view);
static void e_day_view_reshape_resize_rect_item (EDayView *day_view);

static void e_day_view_ensure_events_sorted (EDayView *day_view);
static gint e_day_view_event_sort_func (const void *arg1,
					const void *arg2);

static void e_day_view_start_editing_event (EDayView *day_view,
					    gint day,
					    gint event_num,
					    gchar *initial_text);
static void e_day_view_stop_editing_event (EDayView *day_view);
static gboolean e_day_view_on_text_item_event (GnomeCanvasItem *item,
					       GdkEvent *event,
					       EDayView *day_view);
static void e_day_view_on_editing_started (EDayView *day_view,
					   GnomeCanvasItem *item);
static void e_day_view_on_editing_stopped (EDayView *day_view,
					   GnomeCanvasItem *item);

static void e_day_view_get_selection_range (EDayView *day_view,
					    time_t *start,
					    time_t *end);
static time_t e_day_view_convert_grid_position_to_time (EDayView *day_view,
							gint col,
							gint row);

static void e_day_view_check_auto_scroll (EDayView *day_view,
					  gint event_y);
static void e_day_view_start_auto_scroll (EDayView *day_view,
					  gboolean scroll_up);
static void e_day_view_stop_auto_scroll (EDayView *day_view);
static gboolean e_day_view_auto_scroll_handler (gpointer data);

static void e_day_view_on_new_appointment (GtkWidget *widget,
					   gpointer data);
static void e_day_view_on_edit_appointment (GtkWidget *widget,
					    gpointer data);
static void e_day_view_on_delete_occurance (GtkWidget *widget,
					    gpointer data);
static void e_day_view_on_delete_appointment (GtkWidget *widget,
					      gpointer data);
static void e_day_view_on_unrecur_appointment (GtkWidget *widget,
					       gpointer data);
static EDayViewEvent* e_day_view_get_popup_menu_event (EDayView *day_view);

static gint e_day_view_on_top_canvas_drag_motion (GtkWidget      *widget,
						  GdkDragContext *context,
						  gint            x,
						  gint            y,
						  guint           time,
						  EDayView	  *day_view);
static void e_day_view_update_top_canvas_drag (EDayView *day_view,
					       gint day);
static void e_day_view_reshape_top_canvas_drag_item (EDayView *day_view);
static gint e_day_view_on_main_canvas_drag_motion (GtkWidget      *widget,
						   GdkDragContext *context,
						   gint            x,
						   gint            y,
						   guint           time,
						   EDayView	  *day_view);
static void e_day_view_reshape_main_canvas_drag_item (EDayView *day_view);
static void e_day_view_update_main_canvas_drag (EDayView *day_view,
						gint row,
						gint day);
static void e_day_view_on_top_canvas_drag_leave (GtkWidget      *widget,
						 GdkDragContext *context,
						 guint           time,
						 EDayView	     *day_view);
static void e_day_view_on_main_canvas_drag_leave (GtkWidget      *widget,
						  GdkDragContext *context,
						  guint           time,
						  EDayView	 *day_view);
static void e_day_view_on_drag_begin (GtkWidget      *widget,
				      GdkDragContext *context,
				      EDayView	   *day_view);
static void e_day_view_on_drag_end (GtkWidget      *widget,
				    GdkDragContext *context,
				    EDayView       *day_view);
static void e_day_view_on_drag_data_get (GtkWidget          *widget,
					 GdkDragContext     *context,
					 GtkSelectionData   *selection_data,
					 guint               info,
					 guint               time,
					 EDayView		*day_view);
static void e_day_view_on_top_canvas_drag_data_received (GtkWidget	*widget,
							 GdkDragContext *context,
							 gint            x,
							 gint            y,
							 GtkSelectionData *data,
							 guint           info,
							 guint           time,
							 EDayView	*day_view);
static void e_day_view_on_main_canvas_drag_data_received (GtkWidget      *widget,
							  GdkDragContext *context,
							  gint            x,
							  gint            y,
							  GtkSelectionData *data,
							  guint           info,
							  guint           time,
							  EDayView	 *day_view);


static GtkTableClass *parent_class;


GtkType
e_day_view_get_type (void)
{
	static GtkType e_day_view_type = 0;

	if (!e_day_view_type){
		GtkTypeInfo e_day_view_info = {
			"EDayView",
			sizeof (EDayView),
			sizeof (EDayViewClass),
			(GtkClassInitFunc) e_day_view_class_init,
			(GtkObjectInitFunc) e_day_view_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		parent_class = gtk_type_class (GTK_TYPE_TABLE);
		e_day_view_type = gtk_type_unique (GTK_TYPE_TABLE,
						   &e_day_view_info);
	}

	return e_day_view_type;
}


static void
e_day_view_class_init (EDayViewClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	/* Method override */
	object_class->destroy		= e_day_view_destroy;

	widget_class->realize		= e_day_view_realize;
	widget_class->unrealize		= e_day_view_unrealize;
	widget_class->style_set		= e_day_view_style_set;
 	widget_class->size_allocate	= e_day_view_size_allocate;
	widget_class->focus_in_event	= e_day_view_focus_in;
	widget_class->focus_out_event	= e_day_view_focus_out;
	widget_class->key_press_event	= e_day_view_key_press;
}


static void
e_day_view_init (EDayView *day_view)
{
	GdkColormap *colormap;
	gboolean success[E_DAY_VIEW_COLOR_LAST];
	gint day, nfailed;
	GnomeCanvasGroup *canvas_group;

	GTK_WIDGET_SET_FLAGS (day_view, GTK_CAN_FOCUS);

	colormap = gtk_widget_get_colormap (GTK_WIDGET (day_view));

	day_view->calendar = NULL;

	day_view->long_events = g_array_new (FALSE, FALSE,
					     sizeof (EDayViewEvent));
	day_view->long_events_sorted = TRUE;
	day_view->long_events_need_layout = FALSE;
	day_view->long_events_need_reshape = FALSE;

	for (day = 0; day < E_DAY_VIEW_MAX_DAYS; day++) {
		day_view->events[day] = g_array_new (FALSE, FALSE,
						     sizeof (EDayViewEvent));
		day_view->events_sorted[day] = TRUE;
		day_view->need_layout[day] = FALSE;
		day_view->need_reshape[day] = FALSE;
	}

	/* FIXME: Initialize lower, upper, day_starts. */
	day_view->days_shown = 1;

	day_view->mins_per_row = 30;
	day_view->date_format = E_DAY_VIEW_DATE_FULL;
	day_view->rows_in_top_display = 0;
	day_view->first_hour_shown = 0;
	day_view->first_minute_shown = 0;
	day_view->last_hour_shown = 24;
	day_view->last_minute_shown = 0;
	day_view->main_gc = NULL;
	e_day_view_recalc_num_rows (day_view);

	day_view->work_day_start_hour = 9;
	day_view->work_day_start_minute = 0;
	day_view->work_day_end_hour = 17;
	day_view->work_day_end_minute = 0;
	day_view->scroll_to_work_day = TRUE;

	day_view->editing_event_day = -1;
	day_view->editing_event_num = -1;

	day_view->resize_bars_event_day = -1;
	day_view->resize_bars_event_num = -1;

	day_view->selection_start_row = -1;
	day_view->selection_start_col = -1;
	day_view->selection_end_row = -1;
	day_view->selection_end_col = -1;
	day_view->selection_drag_pos = E_DAY_VIEW_DRAG_NONE;
	day_view->selection_in_top_canvas = FALSE;

	day_view->resize_drag_pos = E_DAY_VIEW_POS_NONE;

	day_view->pressed_event_day = -1;

	day_view->drag_event_day = -1;
	day_view->drag_last_day = -1;

	day_view->auto_scroll_timeout_id = 0;

	/* Create the large font. */
	day_view->large_font = gdk_font_load (E_DAY_VIEW_LARGE_FONT);
	if (!day_view->large_font)
		day_view->large_font = gdk_font_load (E_DAY_VIEW_LARGE_FONT_FALLBACK);
	if (!day_view->large_font)
		g_warning ("Couldn't load font");

	
	/* Allocate the colors. */
#if 1
	day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING].red   = 255 * 257;
	day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING].green = 255 * 257;
	day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING].blue  = 131 * 257;

	day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING].red   = 211 * 257;
	day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING].green = 208 * 257;
	day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING].blue  =   6 * 257;
#else

	/* FG: MistyRose1, LightPink3 | RosyBrown | MistyRose3. */

	day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING].red   = 255 * 257;
	day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING].green = 228 * 257;
	day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING].blue  = 225 * 257;

	day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING].red   = 238 * 257;
	day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING].green = 162 * 257;
	day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING].blue  = 173 * 257;
#endif

	day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR].red   = 0;
	day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR].green = 0;
	day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR].blue  = 65535;

	day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND].red   = 65535;
	day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND].green = 65535;
	day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND].blue  = 65535;

	day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER].red   = 0;
	day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER].green = 0;
	day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER].blue  = 0;

	nfailed = gdk_colormap_alloc_colors (colormap, day_view->colors,
					     E_DAY_VIEW_COLOR_LAST, FALSE,
					     TRUE, success);
	if (nfailed)
		g_warning ("Failed to allocate all colors");



	/*
	 * Top Canvas
	 */
	day_view->top_canvas = e_canvas_new ();
	gtk_table_attach (GTK_TABLE (day_view), day_view->top_canvas,
			  1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show (day_view->top_canvas);
	gtk_signal_connect_after (GTK_OBJECT (day_view->top_canvas), "button_press_event",
				  GTK_SIGNAL_FUNC (e_day_view_on_top_canvas_button_press),
				  day_view);
	gtk_signal_connect_after (GTK_OBJECT (day_view->top_canvas), "button_release_event",
				  GTK_SIGNAL_FUNC (e_day_view_on_top_canvas_button_release),
				  day_view);
	gtk_signal_connect_after (GTK_OBJECT (day_view->top_canvas), "motion_notify_event",
				  GTK_SIGNAL_FUNC (e_day_view_on_top_canvas_motion),
				  day_view);
	gtk_signal_connect_after (GTK_OBJECT (day_view->top_canvas),
				  "drag_motion",
				  GTK_SIGNAL_FUNC (e_day_view_on_top_canvas_drag_motion),
				  day_view);
	gtk_signal_connect_after (GTK_OBJECT (day_view->top_canvas),
				  "drag_leave",
				  GTK_SIGNAL_FUNC (e_day_view_on_top_canvas_drag_leave),
				  day_view);
	gtk_signal_connect (GTK_OBJECT (day_view->top_canvas),
			    "drag_begin",
			    GTK_SIGNAL_FUNC (e_day_view_on_drag_begin),
			    day_view);
	gtk_signal_connect (GTK_OBJECT (day_view->top_canvas),
			    "drag_end",
			    GTK_SIGNAL_FUNC (e_day_view_on_drag_end),
			    day_view);
	gtk_signal_connect (GTK_OBJECT (day_view->top_canvas),
			    "drag_data_get",
			    GTK_SIGNAL_FUNC (e_day_view_on_drag_data_get),
			    day_view);
	gtk_signal_connect (GTK_OBJECT (day_view->top_canvas),
			    "drag_data_received",
			    GTK_SIGNAL_FUNC (e_day_view_on_top_canvas_drag_data_received),
			    day_view);

	canvas_group = GNOME_CANVAS_GROUP (GNOME_CANVAS (day_view->top_canvas)->root);

	day_view->top_canvas_item =
		gnome_canvas_item_new (canvas_group,
				       e_day_view_top_item_get_type (),
				       "EDayViewTopItem::day_view", day_view,
				       NULL);

	day_view->resize_long_event_rect_item =
		gnome_canvas_item_new (canvas_group,
				       gnome_canvas_rect_get_type(),
				       "fill_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND],
				       "outline_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
				       NULL);
	gnome_canvas_item_hide (day_view->resize_long_event_rect_item);

	day_view->drag_long_event_rect_item =
		gnome_canvas_item_new (canvas_group,
				       gnome_canvas_rect_get_type (),
				       "width_pixels", 1,
				       "fill_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND],
				       "outline_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
				       NULL);
	gnome_canvas_item_hide (day_view->drag_long_event_rect_item);

	day_view->drag_long_event_item =
		gnome_canvas_item_new (canvas_group,
				       e_text_get_type (),
				       "anchor", GTK_ANCHOR_NW,
				       "line_wrap", TRUE,
				       "clip", TRUE,
				       "max_lines", 1,
				       "editable", TRUE,
				       NULL);
	gnome_canvas_item_hide (day_view->drag_long_event_item);

	/*
	 * Main Canvas
	 */
	day_view->main_canvas = e_canvas_new ();
	gtk_table_attach (GTK_TABLE (day_view), day_view->main_canvas,
			  1, 2, 1, 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (day_view->main_canvas);
	gtk_signal_connect (GTK_OBJECT (day_view->main_canvas), "realize",
			    GTK_SIGNAL_FUNC (e_day_view_on_canvas_realized),
			    day_view);
	gtk_signal_connect_after (GTK_OBJECT (day_view->main_canvas),
				  "button_press_event",
				  GTK_SIGNAL_FUNC (e_day_view_on_main_canvas_button_press),
				  day_view);
	gtk_signal_connect_after (GTK_OBJECT (day_view->main_canvas),
				  "button_release_event",
				  GTK_SIGNAL_FUNC (e_day_view_on_main_canvas_button_release),
				  day_view);
	gtk_signal_connect_after (GTK_OBJECT (day_view->main_canvas),
				  "motion_notify_event",
				  GTK_SIGNAL_FUNC (e_day_view_on_main_canvas_motion),
				  day_view);
	gtk_signal_connect_after (GTK_OBJECT (day_view->main_canvas),
				  "drag_motion",
				  GTK_SIGNAL_FUNC (e_day_view_on_main_canvas_drag_motion),
				  day_view);
	gtk_signal_connect_after (GTK_OBJECT (day_view->main_canvas),
				  "drag_leave",
				  GTK_SIGNAL_FUNC (e_day_view_on_main_canvas_drag_leave),
				  day_view);
	gtk_signal_connect (GTK_OBJECT (day_view->main_canvas),
			    "drag_begin",
			    GTK_SIGNAL_FUNC (e_day_view_on_drag_begin),
			    day_view);
	gtk_signal_connect (GTK_OBJECT (day_view->main_canvas),
			    "drag_end",
			    GTK_SIGNAL_FUNC (e_day_view_on_drag_end),
			    day_view);
	gtk_signal_connect (GTK_OBJECT (day_view->main_canvas),
			    "drag_data_get",
			    GTK_SIGNAL_FUNC (e_day_view_on_drag_data_get),
			    day_view);
	gtk_signal_connect (GTK_OBJECT (day_view->main_canvas),
			    "drag_data_received",
			    GTK_SIGNAL_FUNC (e_day_view_on_main_canvas_drag_data_received),
			    day_view);

	canvas_group = GNOME_CANVAS_GROUP (GNOME_CANVAS (day_view->main_canvas)->root);

	day_view->main_canvas_item =
		gnome_canvas_item_new (canvas_group,
				       e_day_view_main_item_get_type (),
				       "EDayViewMainItem::day_view", day_view,
				       NULL);

	day_view->resize_rect_item =
		gnome_canvas_item_new (canvas_group,
				       gnome_canvas_rect_get_type(),
				       "fill_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND],
				       "outline_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
				       NULL);
	gnome_canvas_item_hide (day_view->resize_rect_item);

	day_view->resize_bar_item =
		gnome_canvas_item_new (canvas_group,
				       gnome_canvas_rect_get_type(),
				       "fill_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR],
				       "outline_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
				       NULL);
	gnome_canvas_item_hide (day_view->resize_bar_item);

	day_view->main_canvas_top_resize_bar_item =
		gnome_canvas_item_new (canvas_group,
				       gnome_canvas_rect_get_type (),
				       "width_pixels", 1,
				       "fill_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR],
				       "outline_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
				       NULL);
	gnome_canvas_item_hide (day_view->main_canvas_top_resize_bar_item);

	day_view->main_canvas_bottom_resize_bar_item =
		gnome_canvas_item_new (canvas_group,
				       gnome_canvas_rect_get_type (),
				       "width_pixels", 1,
				       "fill_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR],
				       "outline_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
				       NULL);
	gnome_canvas_item_hide (day_view->main_canvas_bottom_resize_bar_item);


	day_view->drag_rect_item =
		gnome_canvas_item_new (canvas_group,
				       gnome_canvas_rect_get_type (),
				       "width_pixels", 1,
				       "fill_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND],
				       "outline_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
				       NULL);
	gnome_canvas_item_hide (day_view->drag_rect_item);

	day_view->drag_bar_item =
		gnome_canvas_item_new (canvas_group,
				       gnome_canvas_rect_get_type (),
				       "width_pixels", 1,
				       "fill_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR],
				       "outline_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
				       NULL);
	gnome_canvas_item_hide (day_view->drag_bar_item);

	day_view->drag_item =
		gnome_canvas_item_new (canvas_group,
				       e_text_get_type (),
				       "anchor", GTK_ANCHOR_NW,
				       "line_wrap", TRUE,
				       "clip", TRUE,
				       "editable", TRUE,
				       NULL);
	gnome_canvas_item_hide (day_view->drag_item);


	/*
	 * Times Canvas
	 */
	day_view->time_canvas = e_canvas_new ();
	gtk_layout_set_vadjustment (GTK_LAYOUT (day_view->time_canvas),
				    GTK_LAYOUT (day_view->main_canvas)->vadjustment);
	gtk_table_attach (GTK_TABLE (day_view), day_view->time_canvas,
			  0, 1, 1, 2,
			  GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (day_view->time_canvas);

	canvas_group = GNOME_CANVAS_GROUP (GNOME_CANVAS (day_view->time_canvas)->root);

	day_view->time_canvas_item =
		gnome_canvas_item_new (canvas_group,
				       e_day_view_time_item_get_type (),
				       "EDayViewTimeItem::day_view", day_view,
				       NULL);


	/*
	 * Scrollbar.
	 */
	day_view->vscrollbar = gtk_vscrollbar_new (GTK_LAYOUT (day_view->main_canvas)->vadjustment);
	gtk_table_attach (GTK_TABLE (day_view), day_view->vscrollbar,
			  2, 3, 1, 2, 0, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (day_view->vscrollbar);


	/* Create the pixmaps. */
	day_view->reminder_icon = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &day_view->reminder_mask, NULL, bell_xpm);
	day_view->recurrence_icon = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &day_view->recurrence_mask, NULL, recur_xpm);


	/* Create the cursors. */
	day_view->normal_cursor = gdk_cursor_new (GDK_TOP_LEFT_ARROW);
	day_view->move_cursor = gdk_cursor_new (GDK_FLEUR);
	day_view->resize_width_cursor = gdk_cursor_new (GDK_SB_H_DOUBLE_ARROW);
	day_view->resize_height_cursor = gdk_cursor_new (GDK_SB_V_DOUBLE_ARROW);
	day_view->last_cursor_set_in_top_canvas = NULL;
	day_view->last_cursor_set_in_main_canvas = NULL;

	/* Set up the drop sites. */
	gtk_drag_dest_set (GTK_WIDGET (day_view->top_canvas),
			   GTK_DEST_DEFAULT_ALL,
			   target_table, n_targets,
			   GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drag_dest_set (GTK_WIDGET (day_view->main_canvas),
			   GTK_DEST_DEFAULT_ALL,
			   target_table, n_targets,
			   GDK_ACTION_COPY | GDK_ACTION_MOVE);
}


/* Turn off the background of the canvas windows. This reduces flicker
   considerably when scrolling. (Why isn't it in GnomeCanvas?). */
static void
e_day_view_on_canvas_realized (GtkWidget *widget,
			       EDayView *day_view)
{
	gdk_window_set_back_pixmap (GTK_LAYOUT (widget)->bin_window,
				    NULL, FALSE);
}


/**
 * e_day_view_new:
 * @Returns: a new #EDayView.
 *
 * Creates a new #EDayView.
 **/
GtkWidget *
e_day_view_new (void)
{
	GtkWidget *day_view;

	day_view = GTK_WIDGET (gtk_type_new (e_day_view_get_type ()));

	return day_view;
}


static void
e_day_view_destroy (GtkObject *object)
{
	EDayView *day_view;
	gint day;

	day_view = E_DAY_VIEW (object);

	e_day_view_stop_auto_scroll (day_view);

	gdk_cursor_destroy (day_view->normal_cursor);
	gdk_cursor_destroy (day_view->move_cursor);
	gdk_cursor_destroy (day_view->resize_width_cursor);
	gdk_cursor_destroy (day_view->resize_height_cursor);

	e_day_view_free_events (day_view);
	g_array_free (day_view->long_events, TRUE);
	for (day = 0; day < E_DAY_VIEW_MAX_DAYS; day++)
		g_array_free (day_view->events[day], TRUE);

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}


static void
e_day_view_realize (GtkWidget *widget)
{
	EDayView *day_view;

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(*GTK_WIDGET_CLASS (parent_class)->realize)(widget);

	day_view = E_DAY_VIEW (widget);
	day_view->main_gc = gdk_gc_new (widget->window);
}


static void
e_day_view_unrealize (GtkWidget *widget)
{
	EDayView *day_view;

	day_view = E_DAY_VIEW (widget);

	gdk_gc_unref (day_view->main_gc);
	day_view->main_gc = NULL;

	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		(*GTK_WIDGET_CLASS (parent_class)->unrealize)(widget);
}


static void
e_day_view_style_set (GtkWidget *widget,
		      GtkStyle  *previous_style)
{
	EDayView *day_view;
	GdkFont *font;
	gint top_rows, top_canvas_height;
	gint month, max_month_width, max_abbr_month_width, number_width;
	gint hour, max_large_hour_width;
	gint minute, max_minute_width, i;
	GDate date;
	gchar buffer[128];
	gint times_width;

	if (GTK_WIDGET_CLASS (parent_class)->style_set)
		(*GTK_WIDGET_CLASS (parent_class)->style_set)(widget, previous_style);

	day_view = E_DAY_VIEW (widget);
	font = widget->style->font;

	/* Recalculate the height of each row based on the font size. */
	day_view->row_height = font->ascent + font->descent + E_DAY_VIEW_EVENT_BORDER_HEIGHT * 2 + E_DAY_VIEW_EVENT_Y_PAD * 2 + 2 /* FIXME */;
	day_view->row_height = MAX (day_view->row_height, E_DAY_VIEW_ICON_HEIGHT + E_DAY_VIEW_ICON_Y_PAD + 2);
	GTK_LAYOUT (day_view->main_canvas)->vadjustment->step_increment = day_view->row_height;

	day_view->top_row_height = font->ascent + font->descent + E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT * 2 + E_DAY_VIEW_LONG_EVENT_Y_PAD * 2 + E_DAY_VIEW_TOP_CANVAS_Y_GAP;
	day_view->top_row_height = MAX (day_view->top_row_height, E_DAY_VIEW_ICON_HEIGHT + E_DAY_VIEW_ICON_Y_PAD + 2 + E_DAY_VIEW_TOP_CANVAS_Y_GAP);

	/* Set the height of the top canvas based on the row height and the
	   number of rows needed (min 1 + 1 for the dates + 1 space for DnD).*/
	top_rows = MAX (1, day_view->rows_in_top_display);
	top_canvas_height = (top_rows + 2) * day_view->top_row_height;
	gtk_widget_set_usize (day_view->top_canvas, -1, top_canvas_height);

	/* Find the biggest full month name. */
	g_date_clear (&date, 1);
	g_date_set_dmy (&date, 20, 1, 2000);
	max_month_width = 0;
	for (month = 1; month <= 12; month++) {
		g_date_set_month (&date, month);

		g_date_strftime (buffer, 128, "%B", &date);
		max_month_width = MAX (max_month_width, 
				       gdk_string_width (font, buffer));

		g_date_strftime (buffer, 128, "%b", &date);
		max_abbr_month_width = MAX (max_abbr_month_width, 
					    gdk_string_width (font, buffer));
	}
	number_width = gdk_string_width (font, "31 ");
	day_view->long_format_width = number_width + max_month_width
		+ E_DAY_VIEW_DATE_X_PAD;
	day_view->abbreviated_format_width = number_width
		+ max_abbr_month_width + E_DAY_VIEW_DATE_X_PAD;

	/* Calculate the widths of all the time strings necessary. */
	for (hour = 0; hour < 24; hour++) {
		sprintf (buffer, "%02i", hour);
		day_view->small_hour_widths[hour] = gdk_string_width (font, buffer);
		day_view->large_hour_widths[hour] = gdk_string_width (day_view->large_font, buffer);
		day_view->max_small_hour_width = MAX (day_view->max_small_hour_width, day_view->small_hour_widths[hour]);
		max_large_hour_width = MAX (max_large_hour_width, day_view->large_hour_widths[hour]);
	}
	day_view->max_large_hour_width = max_large_hour_width;

	for (minute = 0, i = 0; minute < 60; minute += 5, i++) {
		sprintf (buffer, "%02i", minute);
		day_view->minute_widths[i] = gdk_string_width (font, buffer);
		max_minute_width = MAX (max_minute_width, day_view->minute_widths[i]);
	}
	day_view->max_minute_width = max_minute_width;
	day_view->colon_width = gdk_string_width (font, ":");

	/* Calculate the width of the time column. */
	times_width = e_day_view_time_item_get_column_width (E_DAY_VIEW_TIME_ITEM (day_view->time_canvas_item));
	gtk_widget_set_usize (day_view->time_canvas, times_width, -1);
}


/* This recalculates the sizes of each column. */
static void
e_day_view_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	EDayView *day_view;
	gfloat width, offset;
	gint col, day;
	gdouble old_width, old_height, new_width, new_height;
	gint scroll_y;

	g_print ("In e_day_view_size_allocate\n");

	day_view = E_DAY_VIEW (widget);

	(*GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);

	/* Calculate the column sizes, using floating point so that pixels
	   get divided evenly. Note that we use one more element than the
	   number of columns, to make it easy to get the column widths. */
	width = day_view->main_canvas->allocation.width;
	width /= day_view->days_shown;
	offset = 0;
	for (col = 0; col <= day_view->days_shown; col++) {
		day_view->day_offsets[col] = floor (offset + 0.5);
		offset += width;
	}

	/* Calculate the days widths based on the offsets. */
	for (col = 0; col < day_view->days_shown; col++) {
		day_view->day_widths[col] = day_view->day_offsets[col + 1] - day_view->day_offsets[col];
	}

	/* Determine which date format to use, based on the column widths. */
	if (day_view->day_widths[0] > day_view->long_format_width)
		day_view->date_format = E_DAY_VIEW_DATE_FULL;
	else if (day_view->day_widths[0] > day_view->abbreviated_format_width)
		day_view->date_format = E_DAY_VIEW_DATE_ABBREVIATED;
	else
		day_view->date_format = E_DAY_VIEW_DATE_SHORT;

	/* Set the scroll region of the top canvas to its allocated size. */
	gnome_canvas_get_scroll_region (GNOME_CANVAS (day_view->top_canvas),
					NULL, NULL, &old_width, &old_height);
	new_width = day_view->top_canvas->allocation.width;
	new_height = day_view->top_canvas->allocation.height;
	if (old_width != new_width || old_height != new_height)
		gnome_canvas_set_scroll_region (GNOME_CANVAS (day_view->top_canvas),
						0, 0, new_width, new_height);

	/* Set the scroll region of the time canvas to its allocated width,
	   but with the height the same as the main canvas. */
	gnome_canvas_get_scroll_region (GNOME_CANVAS (day_view->time_canvas),
					NULL, NULL, &old_width, &old_height);
	new_width = day_view->time_canvas->allocation.width;
	new_height = MAX (day_view->rows * day_view->row_height, day_view->main_canvas->allocation.height);
	if (old_width != new_width || old_height != new_height)
		gnome_canvas_set_scroll_region (GNOME_CANVAS (day_view->time_canvas),
						0, 0, new_width, new_height);

	/* Set the scroll region of the main canvas to its allocated width,
	   but with the height depending on the number of rows needed. */
	gnome_canvas_get_scroll_region (GNOME_CANVAS (day_view->main_canvas),
					NULL, NULL, &old_width, &old_height);
	new_width = day_view->main_canvas->allocation.width;
	if (old_width != new_width || old_height != new_height)
		gnome_canvas_set_scroll_region (GNOME_CANVAS (day_view->main_canvas),
						0, 0, new_width, new_height);

	/* Scroll to the start of the working day, if this is the initial
	   allocation. */
	if (day_view->scroll_to_work_day) {
		scroll_y = e_day_view_convert_time_to_position (day_view, day_view->work_day_start_hour, day_view->work_day_start_minute);
		gnome_canvas_scroll_to (GNOME_CANVAS (day_view->main_canvas),
					0, scroll_y);
		day_view->scroll_to_work_day = FALSE;
	}

	/* Flag that we need to reshape the events. Note that changes in height
	   don't matter, since the rows are always the same height. */
	if (old_width != new_width) {
		g_print ("Need reshape\n");

		day_view->long_events_need_reshape = TRUE;
		for (day = 0; day < E_DAY_VIEW_MAX_DAYS; day++)
			day_view->need_reshape[day] = TRUE;

		e_day_view_check_layout (day_view);
	}
}


static gint
e_day_view_focus_in (GtkWidget *widget, GdkEventFocus *event)
{
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_DAY_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	g_print ("In e_day_view_focus_in\n");

	GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
	gtk_widget_draw_focus (widget);

	return FALSE;
}


static gint
e_day_view_focus_out (GtkWidget *widget, GdkEventFocus *event)
{
	EDayView *day_view;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_DAY_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	g_print ("In e_day_view_focus_out\n");

	day_view = E_DAY_VIEW (widget);

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);

	/* Get rid of selection. */
	day_view->selection_start_col = -1;

	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);

	return FALSE;
}


void
e_day_view_set_calendar		(EDayView	*day_view,
				 GnomeCalendar	*calendar)
{
	day_view->calendar = calendar;

	/* FIXME: free current events? */
}


void
e_day_view_update_event	(EDayView	*day_view,
			 iCalObject	*ico,
			 int		 flags)
{
	gint day, event_num;

	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	g_print ("In e_day_view_update_event\n");

	/* We only care about events. */
	if (ico && ico->type != ICAL_EVENT)
		return;

	/* If one non-recurring event was added, we can just add it. */
	if (flags == CHANGE_NEW && !ico->recur) {
		if (ico->dtstart < day_view->upper
		    && ico->dtend > day_view->lower) {
			e_day_view_add_event (ico, ico->dtstart, ico->dtend,
					      day_view);
			e_day_view_check_layout (day_view);

			gtk_widget_queue_draw (day_view->top_canvas);
			gtk_widget_queue_draw (day_view->main_canvas);
		}
		return;

	/* If only the summary changed, we can update that easily. */
	} else if (!(flags & ~CHANGE_SUMMARY)) {
		if (e_day_view_find_event_from_ico (day_view, ico,
						    &day, &event_num)) {
			if (day == E_DAY_VIEW_LONG_EVENT) {
				e_day_view_update_long_event_label (day_view,
								    event_num);
				/* For long events we also have to reshape it
				   as the text is centered. */
				e_day_view_reshape_long_event (day_view,
							       event_num);
			} else {
				e_day_view_update_event_label (day_view, day,
							       event_num);
			}

			return;
		}
	}

	e_day_view_reload_events (day_view);
}


/* This updates the text shown for an event. If the event start or end do not
   lie on a row boundary, the time is displayed before the summary. */
static void
e_day_view_update_event_label (EDayView *day_view,
			       gint day,
			       gint event_num)
{
	EDayViewEvent *event;
	gchar *text;
	gboolean free_text;
	gint offset, start_minute, end_minute;

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	if (event->start_minute % day_view->mins_per_row != 0
	    || event->end_minute % day_view->mins_per_row != 0) {
		offset = day_view->first_hour_shown * 60
			+ day_view->first_minute_shown;
		start_minute = offset + event->start_minute;
		end_minute = offset + event->end_minute;
		text = g_strdup_printf ("%02i:%02i-%02i:%02i %s",
					start_minute / 60,
					start_minute % 60,
					end_minute / 60,
					end_minute % 60,
					event->ico->summary);
		free_text = TRUE;
	} else {
		text = event->ico->summary;
		free_text = FALSE;
	}

	gnome_canvas_item_set (event->canvas_item,
			       "text", event->ico->summary ? event->ico->summary : "",
			       NULL);

	if (free_text)
		g_free (text);
}


static void
e_day_view_update_long_event_label (EDayView *day_view,
				    gint      event_num)
{
	EDayViewEvent *event;

	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	gnome_canvas_item_set (event->canvas_item,
			       "text", event->ico->summary ? event->ico->summary : "",
			       NULL);
}


/* Finds the day and index of the event with the given canvas item.
   If is is a long event, -1 is returned as the day.
   Returns TRUE if the event was found. */
static gboolean
e_day_view_find_event_from_item (EDayView *day_view,
				 GnomeCanvasItem *item,
				 gint *day_return,
				 gint *event_num_return)
{
	EDayViewEvent *event;
	gint day, event_num;

	for (day = 0; day < day_view->days_shown; day++) {
		for (event_num = 0; event_num < day_view->events[day]->len;
		     event_num++) {
			event = &g_array_index (day_view->events[day],
						EDayViewEvent, event_num);
			if (event->canvas_item == item) {
				*day_return = day;
				*event_num_return = event_num;
				return TRUE;
			}
		}
	}

	for (event_num = 0; event_num < day_view->long_events->len;
	     event_num++) {
		event = &g_array_index (day_view->long_events,
					EDayViewEvent, event_num);
		if (event->canvas_item == item) {
			*day_return = E_DAY_VIEW_LONG_EVENT;
			*event_num_return = event_num;
			return TRUE;
		}
	}

	return FALSE;
}


/* Finds the day and index of the event containing the iCalObject.
   If is is a long event, E_DAY_VIEW_LONG_EVENT is returned as the day.
   Returns TRUE if the event was found. */
static gboolean
e_day_view_find_event_from_ico (EDayView *day_view,
				iCalObject *ico,
				gint *day_return,
				gint *event_num_return)
{
	EDayViewEvent *event;
	gint day, event_num;

	for (day = 0; day < day_view->days_shown; day++) {
		for (event_num = 0; event_num < day_view->events[day]->len;
		     event_num++) {
			event = &g_array_index (day_view->events[day],
						EDayViewEvent, event_num);
			if (event->ico == ico) {
				*day_return = day;
				*event_num_return = event_num;
				return TRUE;
			}
		}
	}

	for (event_num = 0; event_num < day_view->long_events->len;
	     event_num++) {
		event = &g_array_index (day_view->long_events,
					EDayViewEvent, event_num);
		if (event->ico == ico) {
			*day_return = E_DAY_VIEW_LONG_EVENT;
			*event_num_return = event_num;
			return TRUE;
		}
	}

	return FALSE;
}


/* Note that the times must be the start and end of days. */
void
e_day_view_set_interval	(EDayView	*day_view,
			 time_t		 lower,
			 time_t		 upper)
{
	time_t tmp_lower, day_starts[E_DAY_VIEW_MAX_DAYS + 1];
	gint day;

	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	g_print ("In e_day_view_set_interval\n");

	if (lower == day_view->lower && upper == day_view->upper)
		return;

	/* Check that the first time is the start of a day. */
	tmp_lower = time_day_begin (lower);
	g_return_if_fail (lower == tmp_lower);

	/* Calculate the start of each day shown, and check that upper is
	   valid. */
	day_starts[0] = lower;
	for (day = 1; day <= E_DAY_VIEW_MAX_DAYS; day++) {
		day_starts[day] = time_add_day (day_starts[day - 1], 1);
		/* Check if we have reached the upper time. */
		if (day_starts[day] == upper) {
			day_view->days_shown = day;
			break;
		}

		/* Check that we haven't gone past the upper time. */
		g_return_if_fail (day_starts[day] < upper);
	}

	/* Now that we know that lower & upper are valid, update the fields
	   in the EDayView. */
	day_view->lower = lower;
	day_view->upper = upper;

	for (day = 0; day <= day_view->days_shown; day++) {
		day_view->day_starts[day] = day_starts[day];
	}

	e_day_view_reload_events (day_view);
}


gint
e_day_view_get_days_shown	(EDayView	*day_view)
{
	g_return_val_if_fail (E_IS_DAY_VIEW (day_view), -1);

	return day_view->days_shown;
}


void
e_day_view_set_days_shown	(EDayView	*day_view,
				 gint		 days_shown)
{
	g_return_if_fail (E_IS_DAY_VIEW (day_view));
	g_return_if_fail (days_shown >= 1);
	g_return_if_fail (days_shown <= E_DAY_VIEW_MAX_DAYS);

	if (day_view->days_shown != days_shown) {
		day_view->days_shown = days_shown;

		/* FIXME: Update everything. */
	}
}


gint
e_day_view_get_mins_per_row	(EDayView	*day_view)
{
	g_return_val_if_fail (E_IS_DAY_VIEW (day_view), -1);

	return day_view->mins_per_row;
}


void
e_day_view_set_mins_per_row	(EDayView	*day_view,
				 gint		 mins_per_row)
{
	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	if (mins_per_row != 5 && mins_per_row != 10 && mins_per_row != 15
	    && mins_per_row != 30 && mins_per_row != 60) {
		g_warning ("Invalid minutes per row setting");
		return;
	}

	if (day_view->mins_per_row != mins_per_row) {
		day_view->mins_per_row = mins_per_row;

		/* FIXME: Update positions & display. */
	}
}


/* This recalculates the number of rows to display, based on the time range
   shown and the minutes per row. */
static void
e_day_view_recalc_num_rows	(EDayView	*day_view)
{
	gint hours, minutes, total_minutes;

	hours = day_view->last_hour_shown - day_view->first_hour_shown;
	/* This could be negative but it works out OK. */
	minutes = day_view->last_minute_shown - day_view->first_minute_shown;
	total_minutes = hours * 60 + minutes;
	day_view->rows = total_minutes / day_view->mins_per_row;
}


/* Converts an hour and minute to a row in the canvas. */
gint
e_day_view_convert_time_to_row	(EDayView	*day_view,
				 gint		 hour,
				 gint		 minute)
{
	gint total_minutes, start_minute, offset;

	total_minutes = hour * 60 + minute;
	start_minute = day_view->first_hour_shown * 60
		+ day_view->first_minute_shown;
	offset = total_minutes - start_minute;

	return offset / day_view->mins_per_row;
}


/* Converts an hour and minute to a y coordinate in the canvas. */
gint
e_day_view_convert_time_to_position (EDayView	*day_view,
				     gint	 hour,
				     gint	 minute)
{
	gint total_minutes, start_minute, offset;

	total_minutes = hour * 60 + minute;
	start_minute = day_view->first_hour_shown * 60
		+ day_view->first_minute_shown;
	offset = total_minutes - start_minute;

	return offset * day_view->row_height / day_view->mins_per_row;
}


static gboolean
e_day_view_on_top_canvas_button_press (GtkWidget *widget,
				       GdkEventButton *event,
				       EDayView *day_view)
{
	gint event_x, event_y, scroll_x, scroll_y, day, event_num;
	EDayViewPosition pos;

	g_print ("In e_day_view_on_top_canvas_button_press\n");

	/* Convert the coords to the main canvas window, or return if the
	   window is not found. */
	if (!e_day_view_convert_event_coords (day_view, (GdkEvent*) event,
					      GTK_LAYOUT (widget)->bin_window,
					      &event_x, &event_y))
		return FALSE;

	/* The top canvas doesn't scroll, but just in case. */
	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (widget),
					 &scroll_x, &scroll_y);
	event_x += scroll_x;
	event_y += scroll_y;

	pos = e_day_view_convert_position_in_top_canvas (day_view,
							 event_x, event_y,
							 &day, &event_num);

	if (pos == E_DAY_VIEW_POS_OUTSIDE)
		return FALSE;

	if (pos != E_DAY_VIEW_POS_NONE)
		return e_day_view_on_long_event_button_press (day_view,
							      event_num,
							      event, pos,
							      event_x,
							      event_y);

	e_day_view_stop_editing_event (day_view);

	if (event->button == 1) {
		if (!GTK_WIDGET_HAS_FOCUS (day_view))
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

		if (gdk_pointer_grab (GTK_LAYOUT (widget)->bin_window, FALSE,
				      GDK_POINTER_MOTION_MASK
				      | GDK_BUTTON_RELEASE_MASK,
				      FALSE, NULL, event->time) == 0) {
			day_view->selection_start_row = -1;
			day_view->selection_start_col = day;
			day_view->selection_end_row = -1;
			day_view->selection_end_col = day;
			day_view->selection_drag_pos = E_DAY_VIEW_DRAG_END;
			day_view->selection_in_top_canvas = TRUE;

			/* FIXME: Optimise? */
			gtk_widget_queue_draw (day_view->top_canvas);
			gtk_widget_queue_draw (day_view->main_canvas);
		}
	} else if (event->button == 3) {
		e_day_view_on_event_right_click (day_view, event, -1, -1);
	}

	return TRUE;
}


static gboolean
e_day_view_convert_event_coords (EDayView *day_view,
				 GdkEvent *event,
				 GdkWindow *window,
				 gint *x_return,
				 gint *y_return)
{
	gint event_x, event_y, win_x, win_y;
	GdkWindow *event_window;;

	/* Get the event window, x & y from the appropriate event struct. */
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
		event_x = event->button.x;
		event_y = event->button.y;
		event_window = event->button.window;
		break;
	case GDK_MOTION_NOTIFY:
		event_x = event->motion.x;
		event_y = event->motion.y;
		event_window = event->motion.window;
		break;
	default:
		/* Shouldn't get here. */
		g_assert_not_reached ();
		return FALSE;
	}

	while (event_window && event_window != window
	       && event_window != GDK_ROOT_PARENT()) {
		gdk_window_get_position (event_window, &win_x, &win_y);
		event_x += win_x;
		event_y += win_y;
		event_window = gdk_window_get_parent (event_window);
	}

	*x_return = event_x;
	*y_return = event_y;

	if (event_window != window)
		g_print ("Couldn't find event window\n");

	return (event_window == window) ? TRUE : FALSE;
}


static gboolean
e_day_view_on_main_canvas_button_press (GtkWidget *widget,
					GdkEventButton *event,
					EDayView *day_view)
{
	gint event_x, event_y, scroll_x, scroll_y, row, day, event_num;
	EDayViewPosition pos;

	g_print ("In e_day_view_on_main_canvas_button_press\n");

	/* Convert the coords to the main canvas window, or return if the
	   window is not found. */
	if (!e_day_view_convert_event_coords (day_view, (GdkEvent*) event,
					      GTK_LAYOUT (widget)->bin_window,
					      &event_x, &event_y))
		return FALSE;

	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (widget),
					 &scroll_x, &scroll_y);
	event_x += scroll_x;
	event_y += scroll_y;

	/* Find out where the mouse is. */
	pos = e_day_view_convert_position_in_main_canvas (day_view,
							  event_x, event_y,
							  &day, &row,
							  &event_num);

	if (pos == E_DAY_VIEW_POS_OUTSIDE)
		return FALSE;

	if (pos != E_DAY_VIEW_POS_NONE)
		return e_day_view_on_event_button_press (day_view, day,
							 event_num, event, pos,
							 event_x, event_y);

	e_day_view_stop_editing_event (day_view);

	/* Start the selection drag. */
	if (event->button == 1) {
		if (!GTK_WIDGET_HAS_FOCUS (day_view))
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

		if (gdk_pointer_grab (GTK_LAYOUT (widget)->bin_window, FALSE,
				      GDK_POINTER_MOTION_MASK
				      | GDK_BUTTON_RELEASE_MASK,
				      FALSE, NULL, event->time) == 0) {
			day_view->selection_start_row = row;
			day_view->selection_start_col = day;
			day_view->selection_end_row = row;
			day_view->selection_end_col = day;
			day_view->selection_drag_pos = E_DAY_VIEW_DRAG_END;
			day_view->selection_in_top_canvas = FALSE;

			/* FIXME: Optimise? */
			gtk_widget_queue_draw (day_view->top_canvas);
			gtk_widget_queue_draw (day_view->main_canvas);
		}
	} else if (event->button == 3) {
		e_day_view_on_event_right_click (day_view, event, -1, -1);
	}

	return TRUE;
}


static gboolean
e_day_view_on_long_event_button_press (EDayView		*day_view,
				       gint		 event_num,
				       GdkEventButton	*event,
				       EDayViewPosition  pos,
				       gint		 event_x,
				       gint		 event_y)
{
	if (event->button == 1) {
		if (event->type == GDK_BUTTON_PRESS) {
			e_day_view_on_long_event_click (day_view, event_num,
							event, pos,
							event_x, event_y);
			return TRUE;
		} else if (event->type == GDK_2BUTTON_PRESS) {
			e_day_view_on_event_double_click (day_view, -1,
							  event_num);
			return TRUE;
		}
	} else if (event->button == 3) {
		e_day_view_on_event_right_click (day_view, event, 
						 E_DAY_VIEW_LONG_EVENT,
						 event_num);
		return TRUE;
	}
	return FALSE;
}


static gboolean
e_day_view_on_event_button_press (EDayView	  *day_view,
				  gint		   day,
				  gint		   event_num,
				  GdkEventButton  *event,
				  EDayViewPosition pos,
				  gint		   event_x,
				  gint		   event_y)
{
	if (event->button == 1) {
		if (event->type == GDK_BUTTON_PRESS) {
			e_day_view_on_event_click (day_view, day, event_num,
						   event, pos,
						   event_x, event_y);
			return TRUE;
		} else if (event->type == GDK_2BUTTON_PRESS) {
			e_day_view_on_event_double_click (day_view, day,
							  event_num);
			return TRUE;
		}
	} else if (event->button == 3) {
		e_day_view_on_event_right_click (day_view, event,
						 day, event_num);
		return TRUE;
	}
	return FALSE;
}


static void
e_day_view_on_long_event_click (EDayView *day_view,
				gint event_num,
				GdkEventButton  *bevent,
				EDayViewPosition pos,
				gint	     event_x,
				gint	     event_y)
{
	EDayViewEvent *event;
	gint start_day, end_day, day;
	gint item_x, item_y, item_w, item_h;

	g_print ("In e_day_view_on_long_event_click\n");

	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	/* Ignore clicks on the EText while editing. */
	if (pos == E_DAY_VIEW_POS_EVENT
	    && E_TEXT (event->canvas_item)->editing)
		return;

	if (pos == E_DAY_VIEW_POS_LEFT_EDGE
	    || pos == E_DAY_VIEW_POS_RIGHT_EDGE) {
		if (!e_day_view_find_long_event_days (day_view, event,
						      &start_day, &end_day))
			return;

		/* Grab the keyboard focus, so the event being edited is saved
		   and we can use the Escape key to abort the resize. */
		if (!GTK_WIDGET_HAS_FOCUS (day_view))
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

		if (gdk_pointer_grab (GTK_LAYOUT (day_view->top_canvas)->bin_window, FALSE,
				      GDK_POINTER_MOTION_MASK
				      | GDK_BUTTON_RELEASE_MASK,
				      FALSE, NULL, bevent->time) == 0) {

			day_view->resize_event_day = E_DAY_VIEW_LONG_EVENT;
			day_view->resize_event_num = event_num;
			day_view->resize_drag_pos = pos;
			day_view->resize_start_row = start_day;
			day_view->resize_end_row = end_day;

			/* Create the edit rect if necessary. */
			e_day_view_reshape_resize_long_event_rect_item (day_view);

			/* Make sure the text item is on top. */
			gnome_canvas_item_raise_to_top (day_view->resize_long_event_rect_item);

			/* Raise the event's item, above the rect as well. */
			gnome_canvas_item_raise_to_top (event->canvas_item);
		}
	} else if (e_day_view_get_long_event_position (day_view, event_num,
						       &start_day, &end_day,
						       &item_x, &item_y,
						       &item_w, &item_h)) {
		/* Remember the item clicked and the mouse position,
		   so we can start a drag if the mouse moves. */
		day_view->pressed_event_day = E_DAY_VIEW_LONG_EVENT;
		day_view->pressed_event_num = event_num;

		day_view->drag_event_x = event_x;
		day_view->drag_event_y = event_y;

		e_day_view_convert_position_in_top_canvas (day_view,
							   event_x, event_y,
							   &day, NULL);
		day_view->drag_event_offset = day - start_day;
		g_print ("Y offset: %i\n", day_view->drag_event_offset);
	}
}


static void
e_day_view_on_event_click (EDayView *day_view,
			   gint day,
			   gint event_num,
			   GdkEventButton  *bevent,
			   EDayViewPosition pos,
			   gint		  event_x,
			   gint		  event_y)
{
	EDayViewEvent *event;
	gint tmp_day, row, start_row;

	g_print ("In e_day_view_on_event_click\n");

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	/* Ignore clicks on the EText while editing. */
	if (pos == E_DAY_VIEW_POS_EVENT
	    && E_TEXT (event->canvas_item)->editing)
		return;

	if (pos == E_DAY_VIEW_POS_TOP_EDGE
	    || pos == E_DAY_VIEW_POS_BOTTOM_EDGE) {
		/* Grab the keyboard focus, so the event being edited is saved
		   and we can use the Escape key to abort the resize. */
		if (!GTK_WIDGET_HAS_FOCUS (day_view))
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

		if (gdk_pointer_grab (GTK_LAYOUT (day_view->main_canvas)->bin_window, FALSE,
				      GDK_POINTER_MOTION_MASK
				      | GDK_BUTTON_RELEASE_MASK,
				      FALSE, NULL, bevent->time) == 0) {

			day_view->resize_event_day = day;
			day_view->resize_event_num = event_num;
			day_view->resize_drag_pos = pos;
			day_view->resize_start_row = event->start_minute / day_view->mins_per_row;
			day_view->resize_end_row = (event->end_minute - 1) / day_view->mins_per_row;

			day_view->resize_bars_event_day = day;
			day_view->resize_bars_event_num = event_num;

			/* Create the edit rect if necessary. */
			e_day_view_reshape_resize_rect_item (day_view);

			e_day_view_reshape_main_canvas_resize_bars (day_view);

			/* Make sure the text item is on top. */
			gnome_canvas_item_raise_to_top (day_view->resize_rect_item);
			gnome_canvas_item_raise_to_top (day_view->resize_bar_item);

			/* Raise the event's item, above the rect as well. */
			gnome_canvas_item_raise_to_top (event->canvas_item);
		}

	} else {
		/* Remember the item clicked and the mouse position,
		   so we can start a drag if the mouse moves. */
		day_view->pressed_event_day = day;
		day_view->pressed_event_num = event_num;

		day_view->drag_event_x = event_x;
		day_view->drag_event_y = event_y;

		e_day_view_convert_position_in_main_canvas (day_view,
							    event_x, event_y,
							    &tmp_day, &row,
							    NULL);
		start_row = event->start_minute / day_view->mins_per_row;
		day_view->drag_event_offset = row - start_row;
		g_print ("Y offset: %i Row: %i Start: %i\n",
			 day_view->drag_event_offset, row, start_row);
	}
}


static void
e_day_view_reshape_resize_long_event_rect_item (EDayView *day_view)
{
	gint day, event_num, start_day, end_day;
	gint item_x, item_y, item_w, item_h;
	gdouble x1, y1, x2, y2;

	day = day_view->resize_event_day;
	event_num = day_view->resize_event_num;

	/* If we're not resizing an event, or the event is not shown,
	   hide the resize bars. */
	if (day_view->resize_drag_pos == E_DAY_VIEW_POS_NONE
	    || !e_day_view_get_long_event_position (day_view, event_num,
						    &start_day, &end_day,
						    &item_x, &item_y,
						    &item_w, &item_h)) {
		gnome_canvas_item_hide (day_view->resize_long_event_rect_item);
		return;
	}

	x1 = item_x;
	y1 = item_y;
	x2 = item_x + item_w - 1;
	y2 = item_y + item_h - 1;

	gnome_canvas_item_set (day_view->resize_long_event_rect_item,
			       "x1", x1,
			       "y1", y1,
			       "x2", x2,
			       "y2", y2,
			       NULL);
	gnome_canvas_item_show (day_view->resize_long_event_rect_item);
}


static void
e_day_view_reshape_resize_rect_item (EDayView *day_view)
{
	gint day, event_num;
	gint item_x, item_y, item_w, item_h;
	gdouble x1, y1, x2, y2;

	day = day_view->resize_event_day;
	event_num = day_view->resize_event_num;

	/* If we're not resizing an event, or the event is not shown,
	   hide the resize bars. */
	if (day_view->resize_drag_pos == E_DAY_VIEW_POS_NONE
	    || !e_day_view_get_event_position (day_view, day, event_num,
					       &item_x, &item_y,
					       &item_w, &item_h)) {
		gnome_canvas_item_hide (day_view->resize_rect_item);
		return;
	}

	x1 = item_x;
	y1 = item_y;
	x2 = item_x + item_w - 1;
	y2 = item_y + item_h - 1;

	gnome_canvas_item_set (day_view->resize_rect_item,
			       "x1", x1 + E_DAY_VIEW_BAR_WIDTH - 1,
			       "y1", y1,
			       "x2", x2,
			       "y2", y2,
			       NULL);
	gnome_canvas_item_show (day_view->resize_rect_item);

	gnome_canvas_item_set (day_view->resize_bar_item,
			       "x1", x1,
			       "y1", y1,
			       "x2", x1 + E_DAY_VIEW_BAR_WIDTH - 1,
			       "y2", y2,
			       NULL);
	gnome_canvas_item_show (day_view->resize_bar_item);
}


static void
e_day_view_on_event_double_click (EDayView *day_view,
				  gint day,
				  gint event_num)
{
	g_print ("In e_day_view_on_event_double_click\n");

}


static void
e_day_view_on_event_right_click (EDayView *day_view,
				 GdkEventButton *bevent,
				 gint day,
				 gint event_num)
{
	EDayViewEvent *event;
	int have_selection, not_being_edited, items, i;
	struct menu_item *context_menu;
	
	static struct menu_item main_items[] = {
		{ N_("New appointment..."), (GtkSignalFunc) e_day_view_on_new_appointment, NULL, TRUE }
	};

	static struct menu_item child_items[] = {
		{ N_("Edit this appointment..."), (GtkSignalFunc) e_day_view_on_edit_appointment, NULL, TRUE },
		{ N_("Delete this appointment"), (GtkSignalFunc) e_day_view_on_delete_appointment, NULL, TRUE },
		{ NULL, NULL, NULL, TRUE },
		{ N_("New appointment..."), (GtkSignalFunc) e_day_view_on_new_appointment, NULL, TRUE }
	};

	static struct menu_item recur_child_items[] = {
		{ N_("Make this appointment movable"), (GtkSignalFunc) e_day_view_on_unrecur_appointment, NULL, TRUE },
		{ N_("Edit this appointment..."), (GtkSignalFunc) e_day_view_on_edit_appointment, NULL, TRUE },
		{ N_("Delete this occurance"), (GtkSignalFunc) e_day_view_on_delete_occurance, NULL, TRUE },
		{ N_("Delete all occurances"), (GtkSignalFunc) e_day_view_on_delete_appointment, NULL, TRUE },
		{ NULL, NULL, NULL, TRUE },
		{ N_("New appointment..."), (GtkSignalFunc) e_day_view_on_new_appointment, NULL, TRUE }
	};

	g_print ("In e_day_view_on_event_right_click\n");

	have_selection = (day_view->selection_start_col != -1);

	if (event_num == -1) {
		items = 1;
		context_menu = &main_items[0];
		context_menu[0].sensitive = have_selection;
	} else {
		if (day == E_DAY_VIEW_LONG_EVENT)
			event = &g_array_index (day_view->long_events,
						EDayViewEvent, event_num);
		else
			event = &g_array_index (day_view->events[day],
						EDayViewEvent, event_num);

		/* Check if the event is being edited in the event editor. */
		not_being_edited = (event->ico->user_data == NULL);

		if (event->ico->recur) {
			items = 6;
			context_menu = &recur_child_items[0];
			context_menu[3].sensitive = not_being_edited;
			context_menu[5].sensitive = have_selection;
		} else {
			items = 4;
			context_menu = &child_items[0];
			context_menu[3].sensitive = have_selection;
		}
		/* These settings are common for each context sensitive menu */
		context_menu[0].sensitive = not_being_edited;
		context_menu[1].sensitive = not_being_edited;
		context_menu[2].sensitive = not_being_edited;
	}

	for (i = 0; i < items; i++)
		context_menu[i].data = day_view;
		     
	day_view->popup_event_day = day;
	day_view->popup_event_num = event_num;
	popup_menu (context_menu, items, bevent);
}


static void
e_day_view_on_new_appointment (GtkWidget *widget, gpointer data)
{
	EDayView *day_view;
	GtkWidget *event_editor;
	iCalObject *ico;
	
	day_view = E_DAY_VIEW (data);

	ico = ical_new ("", user_name, "");
	ico->new = 1;
	
	e_day_view_get_selection_range (day_view, &ico->dtstart, &ico->dtend);
	event_editor = event_editor_new (day_view->calendar, ico);
	gtk_widget_show (event_editor);
}


static void
e_day_view_on_edit_appointment (GtkWidget *widget, gpointer data)
{
	EDayView *day_view;
	EDayViewEvent *event;
	GtkWidget *event_editor;

	day_view = E_DAY_VIEW (data);

	event = e_day_view_get_popup_menu_event (day_view);
	if (event == NULL)
		return;

	event_editor = event_editor_new (day_view->calendar, event->ico);
	gtk_widget_show (event_editor);
}


static void
e_day_view_on_delete_occurance (GtkWidget *widget, gpointer data)
{
	EDayView *day_view;
	EDayViewEvent *event;

	day_view = E_DAY_VIEW (data);

	event = e_day_view_get_popup_menu_event (day_view);
	if (event == NULL)
		return;

	ical_object_add_exdate (event->ico, event->start);
	gnome_calendar_object_changed (day_view->calendar, event->ico,
				       CHANGE_DATES);
	save_default_calendar (day_view->calendar);
}


static void
e_day_view_on_delete_appointment (GtkWidget *widget, gpointer data)
{
	EDayView *day_view;
	EDayViewEvent *event;

	day_view = E_DAY_VIEW (data);

	event = e_day_view_get_popup_menu_event (day_view);
	if (event == NULL)
		return;

	gnome_calendar_remove_object (day_view->calendar, event->ico);
	save_default_calendar (day_view->calendar);
}


static void
e_day_view_on_unrecur_appointment (GtkWidget *widget, gpointer data)
{
	EDayView *day_view;
	EDayViewEvent *event;
	iCalObject *ico;

	day_view = E_DAY_VIEW (data);

	event = e_day_view_get_popup_menu_event (day_view);
	if (event == NULL)
		return;
	
	/* New object */
	ico = ical_object_duplicate (event->ico);
	g_free (ico->recur);
	ico->recur = 0;
	ico->dtstart = event->start;
	ico->dtend   = event->end;
	
	/* Duplicate, and eliminate the recurrency fields */
	ical_object_add_exdate (event->ico, event->start);
	gnome_calendar_object_changed (day_view->calendar, event->ico,
				       CHANGE_ALL);
	gnome_calendar_add_object (day_view->calendar, ico);
	save_default_calendar (day_view->calendar);
}


static EDayViewEvent*
e_day_view_get_popup_menu_event (EDayView *day_view)
{
	if (day_view->popup_event_num == -1)
		return NULL;

	if (day_view->popup_event_day == E_DAY_VIEW_LONG_EVENT)
		return &g_array_index (day_view->long_events,
				       EDayViewEvent,
				       day_view->popup_event_num);
	else
		return &g_array_index (day_view->events[day_view->popup_event_day],
				       EDayViewEvent,
				       day_view->popup_event_num);
}


static gboolean
e_day_view_on_top_canvas_button_release (GtkWidget *widget,
					 GdkEventButton *event,
					 EDayView *day_view)
{

	g_print ("In e_day_view_on_top_canvas_button_release\n");

	if (day_view->selection_drag_pos != E_DAY_VIEW_DRAG_NONE) {
		day_view->selection_drag_pos = E_DAY_VIEW_DRAG_NONE;
		gdk_pointer_ungrab (event->time);
	} else if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE) {
		e_day_view_finish_long_event_resize (day_view);
		gdk_pointer_ungrab (event->time);
	} else if (day_view->pressed_event_day != -1) {
		e_day_view_start_editing_event (day_view,
						day_view->pressed_event_day,
						day_view->pressed_event_num,
						NULL);
	}

	day_view->pressed_event_day = -1;

	return FALSE;
}


static gboolean
e_day_view_on_main_canvas_button_release (GtkWidget *widget,
					  GdkEventButton *event,
					  EDayView *day_view)
{

	g_print ("In e_day_view_on_main_canvas_button_release\n");

	if (day_view->selection_drag_pos != E_DAY_VIEW_DRAG_NONE) {
		day_view->selection_drag_pos = E_DAY_VIEW_DRAG_NONE;
		gdk_pointer_ungrab (event->time);
		e_day_view_stop_auto_scroll (day_view);
	} else if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE) {
		e_day_view_finish_resize (day_view);
		gdk_pointer_ungrab (event->time);
		e_day_view_stop_auto_scroll (day_view);
	} else if (day_view->pressed_event_day != -1) {
		e_day_view_start_editing_event (day_view,
						day_view->pressed_event_day,
						day_view->pressed_event_num,
						NULL);
	}

	day_view->pressed_event_day = -1;

	return FALSE;
}


static gboolean
e_day_view_on_top_canvas_motion (GtkWidget *widget,
				 GdkEventMotion *mevent,
				 EDayView *day_view)
{
	EDayViewPosition pos;
	gint event_x, event_y, scroll_x, scroll_y, canvas_x, canvas_y;
	gint day, event_num;
	GdkCursor *cursor;

#if 0
	g_print ("In e_day_view_on_top_canvas_motion\n");
#endif

	/* Convert the coords to the main canvas window, or return if the
	   window is not found. */
	if (!e_day_view_convert_event_coords (day_view, (GdkEvent*) mevent,
					      GTK_LAYOUT (widget)->bin_window,
					      &event_x, &event_y))
		return FALSE;

	/* The top canvas doesn't scroll, but just in case. */
	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (widget),
					 &scroll_x, &scroll_y);
	canvas_x = event_x + scroll_x;
	canvas_y = event_y + scroll_y;

	pos = e_day_view_convert_position_in_top_canvas (day_view,
							 canvas_x, canvas_y,
							 &day, &event_num);

	if (day_view->selection_drag_pos != E_DAY_VIEW_DRAG_NONE) {
		e_day_view_update_selection (day_view, -1, day);
		return TRUE;
	} else if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE) {
		if (pos != E_DAY_VIEW_POS_OUTSIDE) {
			e_day_view_update_long_event_resize (day_view, day);
			return TRUE;
		}
	} else if (day_view->pressed_event_day == E_DAY_VIEW_LONG_EVENT) {
		GtkTargetList *target_list;

		g_print ("Checking whether to start drag - Pressed %i,%i Canvas: %i,%i\n", day_view->drag_event_x, day_view->drag_event_y, canvas_x, canvas_y);

		if (abs (canvas_x - day_view->drag_event_x) > E_DAY_VIEW_DRAG_START_OFFSET
		    || abs (canvas_y - day_view->drag_event_y) > E_DAY_VIEW_DRAG_START_OFFSET) {
			day_view->drag_event_day = day_view->pressed_event_day;
			day_view->drag_event_num = day_view->pressed_event_num;
			day_view->pressed_event_day = -1;

			target_list = gtk_target_list_new (target_table,
							   n_targets);
			gtk_drag_begin (widget, target_list,
					GDK_ACTION_COPY | GDK_ACTION_MOVE,
					1, (GdkEvent*)mevent);
			gtk_target_list_unref (target_list);
		}
	} else {
		cursor = day_view->normal_cursor;
		switch (pos) {
		case E_DAY_VIEW_POS_LEFT_EDGE:
		case E_DAY_VIEW_POS_RIGHT_EDGE:
			cursor = day_view->resize_width_cursor;
			break;
		default:
			break;
		}
		/* Only set the cursor if it is different to last one set. */
		if (day_view->last_cursor_set_in_top_canvas != cursor) {
			day_view->last_cursor_set_in_top_canvas = cursor;
			gdk_window_set_cursor (widget->window, cursor);
		}

	}

	return FALSE;
}


static gboolean
e_day_view_on_main_canvas_motion (GtkWidget *widget,
				  GdkEventMotion *mevent,
				  EDayView *day_view)
{
	EDayViewPosition pos;
	gint event_x, event_y, scroll_x, scroll_y, canvas_x, canvas_y;
	gint row, day, event_num;
	GdkCursor *cursor;

#if 0
	g_print ("In e_day_view_on_main_canvas_motion\n");
#endif

	/* Convert the coords to the main canvas window, or return if the
	   window is not found. */
	if (!e_day_view_convert_event_coords (day_view, (GdkEvent*) mevent,
					      GTK_LAYOUT (widget)->bin_window,
					      &event_x, &event_y))
		return FALSE;

	day_view->last_mouse_x = event_x;
	day_view->last_mouse_y = event_y;

	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (widget),
					 &scroll_x, &scroll_y);
	canvas_x = event_x + scroll_x;
	canvas_y = event_y + scroll_y;

	pos = e_day_view_convert_position_in_main_canvas (day_view,
							  canvas_x, canvas_y,
							  &day, &row,
							  &event_num);

	if (day_view->selection_drag_pos != E_DAY_VIEW_DRAG_NONE) {
		if (pos != E_DAY_VIEW_POS_OUTSIDE) {
			e_day_view_update_selection (day_view, row, day);
			e_day_view_check_auto_scroll (day_view, event_y);
			return TRUE;
		}
	} else if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE) {
		if (pos != E_DAY_VIEW_POS_OUTSIDE) {
			e_day_view_update_resize (day_view, row);
			e_day_view_check_auto_scroll (day_view, event_y);
			return TRUE;
		}
	} else if (day_view->pressed_event_day != -1) {
		GtkTargetList *target_list;

		g_print ("Checking whether to start drag - Pressed %i,%i Canvas: %i,%i\n", day_view->drag_event_x, day_view->drag_event_y, canvas_x, canvas_y);

		if (abs (canvas_x - day_view->drag_event_x) > E_DAY_VIEW_DRAG_START_OFFSET
		    || abs (canvas_y - day_view->drag_event_y) > E_DAY_VIEW_DRAG_START_OFFSET) {
			day_view->drag_event_day = day_view->pressed_event_day;
			day_view->drag_event_num = day_view->pressed_event_num;
			day_view->pressed_event_day = -1;

			target_list = gtk_target_list_new (target_table,
							   n_targets);
			gtk_drag_begin (widget, target_list,
					GDK_ACTION_COPY | GDK_ACTION_MOVE,
					1, (GdkEvent*)mevent);
			gtk_target_list_unref (target_list);
		}
	} else {
		cursor = day_view->normal_cursor;
		switch (pos) {
		case E_DAY_VIEW_POS_LEFT_EDGE:
			cursor = day_view->move_cursor;
			break;
		case E_DAY_VIEW_POS_TOP_EDGE:
		case E_DAY_VIEW_POS_BOTTOM_EDGE:
			cursor = day_view->resize_height_cursor;
			break;
		default:
			break;
		}
		/* Only set the cursor if it is different to last one set. */
		if (day_view->last_cursor_set_in_main_canvas != cursor) {
			day_view->last_cursor_set_in_main_canvas = cursor;
			gdk_window_set_cursor (widget->window, cursor);
		}
	}

	return FALSE;
}


static void
e_day_view_update_selection (EDayView *day_view,
			     gint row,
			     gint col)
{
	gint tmp_row, tmp_col;
	gboolean need_redraw = FALSE;

#if 0
	g_print ("Updating selection %i,%i\n", col, row);
#endif

	day_view->selection_in_top_canvas = (row == -1) ? TRUE : FALSE;

	if (day_view->selection_drag_pos == E_DAY_VIEW_DRAG_START) {
		if (row != day_view->selection_start_row
		    || col != day_view->selection_start_col) {
			need_redraw = TRUE;
			day_view->selection_start_row = row;
			day_view->selection_start_col = col;
		}
	} else {
		if (row != day_view->selection_end_row
		    || col != day_view->selection_end_col) {
			need_redraw = TRUE;
			day_view->selection_end_row = row;
			day_view->selection_end_col = col;
		}
	}

	/* Switch the drag position if necessary. */
	if (day_view->selection_start_col > day_view->selection_end_col
	    || (day_view->selection_start_col == day_view->selection_end_col
		&& day_view->selection_start_row > day_view->selection_end_row)) {
		tmp_row = day_view->selection_start_row;
		tmp_col = day_view->selection_start_col;
		day_view->selection_start_col = day_view->selection_end_col;
		day_view->selection_start_row = day_view->selection_end_row;
		day_view->selection_end_col = tmp_col;
		day_view->selection_end_row = tmp_row;
		if (day_view->selection_drag_pos == E_DAY_VIEW_DRAG_START)
			day_view->selection_drag_pos = E_DAY_VIEW_DRAG_END;
		else
			day_view->selection_drag_pos = E_DAY_VIEW_DRAG_START;
	}

	/* FIXME: Optimise? */
	if (need_redraw) {
		gtk_widget_queue_draw (day_view->top_canvas);
		gtk_widget_queue_draw (day_view->main_canvas);
	}
}


static void
e_day_view_update_long_event_resize (EDayView *day_view,
				     gint day)
{
	EDayViewEvent *event;
	gint event_num;
	gboolean need_reshape = FALSE;

#if 1
	g_print ("Updating resize Day:%i\n", day);
#endif

	event_num = day_view->resize_event_num;
	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	if (day_view->resize_drag_pos == E_DAY_VIEW_POS_LEFT_EDGE) {
		day = MIN (day, day_view->resize_end_row);
		if (day != day_view->resize_start_row) {
			need_reshape = TRUE;
			day_view->resize_start_row = day;
			
		}
	} else {
		day = MAX (day, day_view->resize_start_row);
		if (day != day_view->resize_end_row) {
			need_reshape = TRUE;
			day_view->resize_end_row = day;
		}
	}

	/* FIXME: Optimise? */
	if (need_reshape) {
		e_day_view_reshape_long_event (day_view, event_num);
		e_day_view_reshape_resize_long_event_rect_item (day_view);
		gtk_widget_queue_draw (day_view->top_canvas);
	}
}


static void
e_day_view_update_resize (EDayView *day_view,
			  gint row)
{
	EDayViewEvent *event;
	gint day, event_num;
	gboolean need_reshape = FALSE;

#if 0
	g_print ("Updating resize Row:%i\n", row);
#endif

	day = day_view->resize_event_day;
	event_num = day_view->resize_event_num;
	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	if (day_view->resize_drag_pos == E_DAY_VIEW_POS_TOP_EDGE) {
		row = MIN (row, day_view->resize_end_row);
		if (row != day_view->resize_start_row) {
			need_reshape = TRUE;
			day_view->resize_start_row = row;
			
		}
	} else {
		row = MAX (row, day_view->resize_start_row);
		if (row != day_view->resize_end_row) {
			need_reshape = TRUE;
			day_view->resize_end_row = row;
		}
	}

	/* FIXME: Optimise? */
	if (need_reshape) {
		e_day_view_reshape_day_event (day_view, day, event_num);
		e_day_view_reshape_resize_rect_item (day_view);
		e_day_view_reshape_main_canvas_resize_bars (day_view);
		gtk_widget_queue_draw (day_view->main_canvas);
	}
}


/* This converts the resize start or end row back to a time and updates the
   event. */
static void
e_day_view_finish_long_event_resize (EDayView *day_view)
{
	EDayViewEvent *event;
	gint event_num;


	g_print ("In e_day_view_finish_long_event_resize\n");

	event_num = day_view->resize_event_num;
	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	if (day_view->resize_drag_pos == E_DAY_VIEW_POS_LEFT_EDGE) {
		event->ico->dtstart = day_view->day_starts[day_view->resize_start_row];
	} else {
		event->ico->dtend = day_view->day_starts[day_view->resize_end_row + 1];
	}

	gnome_canvas_item_hide (day_view->resize_long_event_rect_item);

	day_view->resize_drag_pos = E_DAY_VIEW_POS_NONE;

	/* Notify calendar of change */
	gnome_calendar_object_changed (day_view->calendar, event->ico,
				       CHANGE_DATES);
	save_default_calendar (day_view->calendar);
}


/* This converts the resize start or end row back to a time and updates the
   event. */
static void
e_day_view_finish_resize (EDayView *day_view)
{
	EDayViewEvent *event;
	gint day, event_num;


	g_print ("In e_day_view_finish_resize\n");

	day = day_view->resize_event_day;
	event_num = day_view->resize_event_num;
	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	if (day_view->resize_drag_pos == E_DAY_VIEW_POS_TOP_EDGE) {
		event->ico->dtstart = e_day_view_convert_grid_position_to_time (day_view, day, day_view->resize_start_row);
	} else {
		event->ico->dtend = e_day_view_convert_grid_position_to_time (day_view, day, day_view->resize_end_row + 1);
	}

	gnome_canvas_item_hide (day_view->resize_rect_item);
	gnome_canvas_item_hide (day_view->resize_bar_item);

	/* Hide the horizontal bars. */
	g_print ("Hiding resize bars\n");
	day_view->resize_bars_event_day = -1;
	day_view->resize_bars_event_num = -1;
	gnome_canvas_item_hide (day_view->main_canvas_top_resize_bar_item);
	gnome_canvas_item_hide (day_view->main_canvas_bottom_resize_bar_item);

	day_view->resize_drag_pos = E_DAY_VIEW_POS_NONE;

	/* Notify calendar of change */
	gnome_calendar_object_changed (day_view->calendar, event->ico,
				       CHANGE_DATES);
	save_default_calendar (day_view->calendar);
}


static void
e_day_view_abort_resize (EDayView *day_view,
			 guint32 time)
{
	gint day, event_num;

	if (day_view->resize_drag_pos == E_DAY_VIEW_POS_NONE)
		return;

	g_print ("In e_day_view_abort_resize\n");

	day_view->resize_drag_pos = E_DAY_VIEW_POS_NONE;
	gdk_pointer_ungrab (time);

	day = day_view->resize_event_day;
	event_num = day_view->resize_event_num;

	if (day == E_DAY_VIEW_LONG_EVENT) {
		e_day_view_reshape_long_event (day_view, event_num);
		gtk_widget_queue_draw (day_view->top_canvas);
	
		day_view->last_cursor_set_in_top_canvas = day_view->normal_cursor;
		gdk_window_set_cursor (day_view->top_canvas->window,
				       day_view->normal_cursor);
		gnome_canvas_item_hide (day_view->resize_long_event_rect_item);
	} else {
		e_day_view_reshape_day_event (day_view, day, event_num);
		e_day_view_reshape_main_canvas_resize_bars (day_view);
		gtk_widget_queue_draw (day_view->main_canvas);
	
		day_view->last_cursor_set_in_main_canvas = day_view->normal_cursor;
		gdk_window_set_cursor (day_view->main_canvas->window,
				       day_view->normal_cursor);
		gnome_canvas_item_hide (day_view->resize_rect_item);
		gnome_canvas_item_hide (day_view->resize_bar_item);
	}
}


static void
e_day_view_reload_events (EDayView *day_view)
{
	e_day_view_free_events (day_view);

	/* Reset all our indices. */
	day_view->editing_event_day = -1;
	day_view->popup_event_day = -1;
	day_view->resize_bars_event_day = -1;
	day_view->resize_event_day = -1;
	day_view->pressed_event_day = -1;
	day_view->drag_event_day = -1;

	if (day_view->calendar) {
		calendar_iterate (day_view->calendar->cal,
				  day_view->lower,
				  day_view->upper,
				  e_day_view_add_event,
				  day_view);
	}

	e_day_view_check_layout (day_view);

	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
}


static void
e_day_view_free_events (EDayView *day_view)
{
	gint day;

	e_day_view_free_event_array (day_view, day_view->long_events);

	for (day = 0; day < E_DAY_VIEW_MAX_DAYS; day++)
		e_day_view_free_event_array (day_view, day_view->events[day]);
}


static void
e_day_view_free_event_array (EDayView *day_view,
			     GArray *array)
{
	EDayViewEvent *event;
	gint event_num;

	for (event_num = 0; event_num < array->len; event_num++) {
		event = &g_array_index (array, EDayViewEvent, event_num);
		if (event->canvas_item)
			gtk_object_destroy (GTK_OBJECT (event->canvas_item));
	}

	g_array_set_size (array, 0);
}


/* This adds one event to the view, adding it to the appropriate array. */
static int
e_day_view_add_event (iCalObject *ico,
		      time_t	  start,
		      time_t	  end,
		      gpointer	  data)

{
	EDayView *day_view;
	EDayViewEvent event;
	gint day;
	struct tm start_tm, end_tm;

	day_view = E_DAY_VIEW (data);

	/* Check that the event times are valid. */
	g_return_val_if_fail (start <= end, TRUE);
	g_return_val_if_fail (start < day_view->upper, TRUE);
	g_return_val_if_fail (end > day_view->lower, TRUE);

	start_tm = *(localtime (&start));
	end_tm = *(localtime (&end));
	
	event.ico = ico;
	event.start = start;
	event.end = end;
	event.canvas_item = NULL;

	/* Calculate the start & end minute, relative to the
	   top of the display. FIXME. */
	event.start_minute = start_tm.tm_hour * 60 + start_tm.tm_min;
	event.end_minute = end_tm.tm_hour * 60 + end_tm.tm_min;

	event.start_row_or_col = -1;
	event.num_columns = -1;

	/* Find out which array to add the event to. */
	for (day = 0; day < day_view->days_shown; day++) {
		if (start >= day_view->day_starts[day]
		    && end <= day_view->day_starts[day + 1]) {

			/* Special case for when the appointment ends at
			   midnight, i.e. the start of the next day. */
			if (end == day_view->day_starts[day + 1]) {

				/* If the event last the entire day, then we
				   skip it here so it gets added to the top
				   canvas. */
				if (start == day_view->day_starts[day])
				    break;

				event.end_minute = 24 * 60;
			}

			g_array_append_val (day_view->events[day], event);
			day_view->events_sorted[day] = FALSE;
			day_view->need_layout[day] = TRUE;
			return TRUE;
		}
	}

	/* The event wasn't within one day so it must be a long event,
	   i.e. shown in the top canvas. */
	g_array_append_val (day_view->long_events, event);
	day_view->long_events_sorted = FALSE;
	day_view->long_events_need_layout = TRUE;
	return TRUE;
}


/* This lays out the short (less than 1 day) events in the columns. 
   Any long events are simply skipped. */
void
e_day_view_check_layout (EDayView *day_view)
{
	gint day;

	/* Don't bother if we aren't visible. */
	if (!GTK_WIDGET_VISIBLE (day_view))
	    return;

	/* Make sure the events are sorted (by start and size). */
	e_day_view_ensure_events_sorted (day_view);

	for (day = 0; day < day_view->days_shown; day++) {
		if (day_view->need_layout[day])
			e_day_view_layout_day_events (day_view, day);

		if (day_view->need_layout[day]
		    || day_view->need_reshape[day]) {
			e_day_view_reshape_day_events (day_view, day);

			if (day_view->resize_bars_event_day == day)
				e_day_view_reshape_main_canvas_resize_bars (day_view);
		}

		day_view->need_layout[day] = FALSE;
		day_view->need_reshape[day] = FALSE;
	}

	if (day_view->long_events_need_layout)
		e_day_view_layout_long_events (day_view);

	if (day_view->long_events_need_layout
	    || day_view->long_events_need_reshape)
		e_day_view_reshape_long_events (day_view);

	day_view->long_events_need_layout = FALSE;
	day_view->long_events_need_reshape = FALSE;
}


static void
e_day_view_layout_long_events (EDayView *day_view)
{
	EDayViewEvent *event;
	gint event_num, old_rows_in_top_display, top_canvas_height, top_rows;
	guint8 *grid;

	/* This is a temporary 2-d grid which is used to place events.
	   Each element is 0 if the position is empty, or 1 if occupied.
	   We allocate the maximum size possible here, assuming that each
	   event will need its own row. */
	grid = g_new0 (guint8,
		       day_view->long_events->len * E_DAY_VIEW_MAX_DAYS);

	/* Reset the number of rows in the top display to 0. It will be
	   updated as events are layed out below. */
	old_rows_in_top_display = day_view->rows_in_top_display;
	day_view->rows_in_top_display = 0;

	/* Iterate over the events, finding which days they cover, and putting
	   them in the first free row available. */
	for (event_num = 0; event_num < day_view->long_events->len;
	     event_num++) {
		event = &g_array_index (day_view->long_events,
					EDayViewEvent, event_num);
		e_day_view_layout_long_event (day_view, event, grid);
	}

	/* Free the grid. */
	g_free (grid);

	/* Set the height of the top canvas based on the row height and the
	   number of rows needed (min 1 + 1 for the dates + 1 space for DnD).*/
	if (day_view->rows_in_top_display != old_rows_in_top_display) {
		top_rows = MAX (1, day_view->rows_in_top_display);
		top_canvas_height = (top_rows + 2) * day_view->top_row_height;
		gtk_widget_set_usize (day_view->top_canvas, -1,
				      top_canvas_height);
	}
}


static void
e_day_view_layout_long_event (EDayView	   *day_view,
			      EDayViewEvent *event,
			      guint8	   *grid)
{
	gint start_day, end_day, free_row, day, row;

	event->num_columns = 0;

	if (!e_day_view_find_long_event_days (day_view, event,
					      &start_day, &end_day))
		return;

	/* Try each row until we find a free one. */
	row = 0;
	do {
		free_row = row;
		for (day = start_day; day <= end_day; day++) {
			if (grid[row * E_DAY_VIEW_MAX_DAYS + day]) {
				free_row = -1;
				break;
			}
		}
		row++;
	} while (free_row == -1);

	event->start_row_or_col = free_row;
	event->num_columns = 1;

	/* Mark the cells as full. */
	for (day = start_day; day <= end_day; day++) {
		grid[free_row * E_DAY_VIEW_MAX_DAYS + day] = 1;
	}

	/* Update the number of rows in the top canvas if necessary. */
	day_view->rows_in_top_display = MAX (day_view->rows_in_top_display,
					     free_row + 1);
}


static void
e_day_view_reshape_long_events (EDayView *day_view)
{
	EDayViewEvent *event;
	gint event_num;

	for (event_num = 0; event_num < day_view->long_events->len;
	     event_num++) {
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					event_num);

		if (event->num_columns == 0) {
			if (event->canvas_item) {
				gtk_object_destroy (GTK_OBJECT (event->canvas_item));
				event->canvas_item = NULL;
			}
		} else {
			e_day_view_reshape_long_event (day_view, event_num);
		}
	}
}


static void
e_day_view_reshape_long_event (EDayView *day_view,
			       gint	 event_num)
{
	EDayViewEvent *event;
	gint start_day, end_day, item_x, item_y, item_w, item_h;
	gint text_x, text_w, num_icons, icons_width, width, time_width;
	iCalObject *ico;
	gint min_text_x, max_text_w;
	gdouble text_width;
	gboolean show_icons = TRUE, use_max_width = FALSE;

	if (!e_day_view_get_long_event_position (day_view, event_num,
						 &start_day, &end_day,
						 &item_x, &item_y,
						 &item_w, &item_h)) {
		if (event->canvas_item) {
			gtk_object_destroy (GTK_OBJECT (event->canvas_item));
			event->canvas_item = NULL;
		}
		return;
	}

	/* Take off the border and padding. */
	item_x += E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH + E_DAY_VIEW_LONG_EVENT_X_PAD;
	item_w -= (E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH + E_DAY_VIEW_LONG_EVENT_X_PAD) * 2;
	item_y += E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT + E_DAY_VIEW_LONG_EVENT_Y_PAD;
	item_h -= (E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT + E_DAY_VIEW_LONG_EVENT_Y_PAD) * 2;

	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);
	/* We don't show the icons while resizing, since we'd have to
	   draw them on top of the resize rect. Nor when editing. */
	num_icons = 0;
	ico = event->ico;

	if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE
	    && day_view->resize_event_day == E_DAY_VIEW_LONG_EVENT
	    && day_view->resize_event_num == event_num)
		show_icons = FALSE;

	if (day_view->editing_event_day == E_DAY_VIEW_LONG_EVENT
	    && day_view->editing_event_num == event_num) {
		g_print ("Reshaping long event which is being edited.\n");
		show_icons = FALSE;
		use_max_width = TRUE;
	}

	if (show_icons) {
		if (ico->dalarm.enabled || ico->malarm.enabled
		    || ico->palarm.enabled || ico->aalarm.enabled)
			num_icons++;
		if (ico->recur)
			num_icons++;
	}

	/* FIXME: Handle item_w & item_h <= 0. */

	if (!event->canvas_item) {
		event->canvas_item =
			gnome_canvas_item_new (GNOME_CANVAS_GROUP (GNOME_CANVAS (day_view->top_canvas)->root),
					       e_text_get_type (),
					       "font_gdk", GTK_WIDGET (day_view)->style->font,
					       "anchor", GTK_ANCHOR_NW,
					       "clip", TRUE,
					       "max_lines", 1,
					       "editable", TRUE,
					       NULL);
		gtk_signal_connect (GTK_OBJECT (event->canvas_item), "event",
				    GTK_SIGNAL_FUNC (e_day_view_on_text_item_event),
				    day_view);
		e_day_view_update_long_event_label (day_view, event_num);
	}

	/* Calculate its position. We first calculate the ideal position which
	   is centered with the icons. We then make sure we haven't gone off
	   the left edge of the available space. Finally we make sure we don't
	   go off the right edge. */
	icons_width = (E_DAY_VIEW_ICON_WIDTH + E_DAY_VIEW_ICON_X_PAD)
		* num_icons;
	time_width = day_view->max_small_hour_width + day_view->colon_width
		+ day_view->max_minute_width;

	if (use_max_width) {
		text_x = item_x;
		text_w = item_w;
	} else {
		/* Get the requested size of the label. */
		gtk_object_get (GTK_OBJECT (event->canvas_item),
				"text_width", &text_width,
				NULL);

		width = text_width + icons_width;
		text_x = item_x + (item_w - width) / 2;

		min_text_x = item_x;
		if (event->start > day_view->day_starts[start_day])
			min_text_x += time_width + E_DAY_VIEW_LONG_EVENT_TIME_X_PAD;

		text_x = MAX (text_x, min_text_x);

		max_text_w = item_x + item_w - text_x;
		if (event->end < day_view->day_starts[end_day + 1])
			max_text_w -= time_width + E_DAY_VIEW_LONG_EVENT_TIME_X_PAD;

		text_w = MIN (width, max_text_w);
	}

	/* Now take out the space for the icons. */
	text_x += icons_width;
	text_w -= icons_width;

	gnome_canvas_item_set (event->canvas_item,
			       "x", (gdouble) text_x,
			       "y", (gdouble) item_y,
			       "clip_width", (gdouble) text_w,
			       "clip_height", (gdouble) item_h,
			       NULL);
}


/* Find the start and end days for the event. */
gboolean
e_day_view_find_long_event_days (EDayView	*day_view,
				 EDayViewEvent	*event,
				 gint		*start_day_return,
				 gint		*end_day_return)
{
	gint day, start_day, end_day;

	start_day = -1;
	end_day = -1;

	for (day = 0; day < day_view->days_shown; day++) {
		if (start_day == -1
		    && event->start < day_view->day_starts[day + 1])
			start_day = day;
		if (event->end > day_view->day_starts[day])
			end_day = day;
	}

	/* Sanity check. */
	if (start_day < 0 || start_day >= day_view->days_shown
	    || end_day < 0 || end_day >= day_view->days_shown
	    || end_day < start_day) {
		g_warning ("Invalid date range for event");
		return FALSE;
	}

	*start_day_return = start_day;
	*end_day_return = end_day;

	return TRUE;
}


static void
e_day_view_layout_day_events (EDayView *day_view,
			      gint	day)
{
	EDayViewEvent *event;
	gint row, event_num;
	guint8 *grid;

	/* This is a temporary array which keeps track of rows which are
	   connected. When an appointment spans multiple rows then the number
	   of columns in each of these rows must be the same (i.e. the maximum
	   of all of them). Each element in the array corresponds to one row
	   and contains the index of the first row in the group of connected
	   rows. */
	guint16 group_starts[12 * 24];

	/* Reset the cols_per_row array, and initialize the connected rows. */
	for (row = 0; row < day_view->rows; row++) {
		day_view->cols_per_row[day][row] = 0;
		group_starts[row] = row;
	}

	/* This is a temporary 2-d grid which is used to place events.
	   Each element is 0 if the position is empty, or 1 if occupied. */
	grid = g_new0 (guint8, day_view->rows * E_DAY_VIEW_MAX_COLUMNS);


	/* Iterate over the events, finding which rows they cover, and putting
	   them in the first free column available. Increment the number of
	   events in each of the rows it covers, and make sure they are all
	   in one group. */
	for (event_num = 0; event_num < day_view->events[day]->len;
	     event_num++) {
		event = &g_array_index (day_view->events[day], EDayViewEvent,
					event_num);

		e_day_view_layout_day_event (day_view, day, event,
					     grid, group_starts);
	}

	/* Recalculate the number of columns needed in each row. */
	e_day_view_recalc_cols_per_row (day_view, day, group_starts);

	/* Iterate over the events again, trying to expand events horizontally
	   if there is enough space. */
	for (event_num = 0; event_num < day_view->events[day]->len;
	     event_num++) {
		event = &g_array_index (day_view->events[day], EDayViewEvent,
					event_num);
		e_day_view_expand_day_event (day_view, day, event, grid);
	}

	/* Free the grid. */
	g_free (grid);
}


/* Finds the first free position to place the event in.
   Increments the number of events in each of the rows it covers, and makes
   sure they are all in one group. */
static void
e_day_view_layout_day_event (EDayView	   *day_view,
			     gint	    day,
			     EDayViewEvent *event,
			     guint8	   *grid,
			     guint16	   *group_starts)
{
	gint start_row, end_row, free_col, col, row, group_start;

	start_row = event->start_minute / day_view->mins_per_row;
	end_row = (event->end_minute - 1) / day_view->mins_per_row;

	/* Try each column until we find a free one. */
	for (col = 0; col < E_DAY_VIEW_MAX_COLUMNS; col++) {
		free_col = col;
		for (row = start_row; row <= end_row; row++) {
			if (grid[row * E_DAY_VIEW_MAX_COLUMNS + col]) {
				free_col = -1;
				break;
			}
		}

		if (free_col != -1)
			break;
	}

	/* If we can't find space for the event, mark it as not displayed. */
	if (free_col == -1) {
		event->num_columns = 0;
		return;
	}

	/* The event is assigned 1 col initially, but may be expanded later. */
	event->start_row_or_col = free_col;
	event->num_columns = 1;

	/* Determine the start index of the group. */
	group_start = group_starts[start_row];

	/* Increment number of events in each of the rows the event covers.
	   We use the cols_per_row array for this. It will be sorted out after
	   all the events have been layed out. Also make sure all the rows that
	   the event covers are in one group. */
	for (row = start_row; row <= end_row; row++) {
		grid[row * E_DAY_VIEW_MAX_COLUMNS + free_col] = 1;
		day_view->cols_per_row[day][row]++;
		group_starts[row] = group_start;
	}

	/* If any following rows should be in the same group, add them. */
	for (row = end_row + 1; row < day_view->rows; row++) {
		if (group_starts[row] > end_row)
			break;
		group_starts[row] = group_start;
	}
}


/* For each group of rows, find the max number of events in all the
   rows, and set the number of cols in each of the rows to that. */
static void
e_day_view_recalc_cols_per_row (EDayView *day_view,
				gint	  day,
				guint16  *group_starts)
{
	gint start_row = 0, row, next_start_row, max_events;

	while (start_row < day_view->rows) {

		max_events = 0;
		for (row = start_row; row < day_view->rows && group_starts[row] == start_row; row++)
			max_events = MAX (max_events, day_view->cols_per_row[day][row]);

		next_start_row = row;

		for (row = start_row; row < next_start_row; row++)
			day_view->cols_per_row[day][row] = max_events;

		start_row = next_start_row;
	}
}


/* Expands the event horizontally to fill any free space. */
static void
e_day_view_expand_day_event (EDayView	   *day_view,
			     gint	    day,
			     EDayViewEvent *event,
			     guint8	   *grid)
{
	gint start_row, end_row, col, row;
	gboolean clashed;

	start_row = event->start_minute / day_view->mins_per_row;
	end_row = (event->end_minute - 1) / day_view->mins_per_row;

	/* Try each column until we find a free one. */
	clashed = FALSE;
	for (col = event->start_row_or_col + 1; col < day_view->cols_per_row[day][start_row]; col++) {
		for (row = start_row; row <= end_row; row++) {
			if (grid[row * E_DAY_VIEW_MAX_COLUMNS + col]) {
				clashed = TRUE;
				break;
			}
		}

		if (clashed)
			break;

		event->num_columns++;
	}
}


/* This creates or updates the sizes of the canvas items for one day of the
   main canvas. */
static void
e_day_view_reshape_day_events (EDayView *day_view,
			       gint	 day)
{
	gint event_num;

	g_print ("In e_day_view_reshape_day_events\n");

	for (event_num = 0; event_num < day_view->events[day]->len;
	     event_num++) {
		e_day_view_reshape_day_event (day_view, day, event_num);
	}
}


static void
e_day_view_reshape_day_event (EDayView *day_view,
			      gint	day,
			      gint	event_num)
{
	EDayViewEvent *event;
	gint item_x, item_y, item_w, item_h;
	gint num_icons, icons_offset;
	iCalObject *ico;

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);
	ico = event->ico;

	if (!e_day_view_get_event_position (day_view, day, event_num,
					    &item_x, &item_y,
					    &item_w, &item_h)) {
		if (event->canvas_item) {
			gtk_object_destroy (GTK_OBJECT (event->canvas_item));
			event->canvas_item = NULL;
		}
	} else {
		/* Skip the border and padding. */
		item_x += E_DAY_VIEW_BAR_WIDTH + E_DAY_VIEW_EVENT_X_PAD;
		item_w -= E_DAY_VIEW_BAR_WIDTH + E_DAY_VIEW_EVENT_X_PAD * 2;
		item_y += E_DAY_VIEW_EVENT_BORDER_HEIGHT + E_DAY_VIEW_EVENT_Y_PAD;
		item_h -= (E_DAY_VIEW_EVENT_BORDER_HEIGHT + E_DAY_VIEW_EVENT_Y_PAD) * 2;

		/* We don't show the icons while resizing, since we'd have to
		   draw them on top of the resize rect. */
		num_icons = 0;
		if (day_view->resize_drag_pos == E_DAY_VIEW_POS_NONE
		    || day_view->resize_event_day != day
		    || day_view->resize_event_num != event_num) {
			if (ico->dalarm.enabled || ico->malarm.enabled
			    || ico->palarm.enabled || ico->aalarm.enabled)
				num_icons++;
			if (ico->recur)
				num_icons++;
		}

		if (num_icons > 0) {
			if (item_h >= (E_DAY_VIEW_ICON_HEIGHT + E_DAY_VIEW_ICON_Y_PAD) * num_icons)
				icons_offset = E_DAY_VIEW_ICON_WIDTH + E_DAY_VIEW_ICON_X_PAD * 2;
			else
				icons_offset = (E_DAY_VIEW_ICON_WIDTH + E_DAY_VIEW_ICON_X_PAD) * num_icons + E_DAY_VIEW_ICON_X_PAD;
			item_x += icons_offset;
			item_w -= icons_offset;
		}

		/* FIXME: Handle item_w & item_h <= 0. */

		if (!event->canvas_item) {
			event->canvas_item =
				gnome_canvas_item_new (GNOME_CANVAS_GROUP (GNOME_CANVAS (day_view->main_canvas)->root),
						       e_text_get_type (),
						       "font_gdk", GTK_WIDGET (day_view)->style->font,
						       "anchor", GTK_ANCHOR_NW,
						       "line_wrap", TRUE,
						       "editable", TRUE,
						       "clip", TRUE,
						       NULL);
			gtk_signal_connect (GTK_OBJECT (event->canvas_item),
					    "event",
					    GTK_SIGNAL_FUNC (e_day_view_on_text_item_event),
					    day_view);
			e_day_view_update_event_label (day_view, day,
						       event_num);
		}

		gnome_canvas_item_set (event->canvas_item,
				       "x", (gdouble) item_x,
				       "y", (gdouble) item_y,
				       "clip_width", (gdouble) item_w,
				       "clip_height", (gdouble) item_h,
				       NULL);
	}
}


/* This creates or resizes the horizontal bars used to resize events in the
   main canvas. */
static void
e_day_view_reshape_main_canvas_resize_bars (EDayView *day_view)
{
	gint day, event_num;
	gint item_x, item_y, item_w, item_h;
	gdouble x, y, w, h;

	day = day_view->resize_bars_event_day;
	event_num = day_view->resize_bars_event_num;

	/* If we're not editing an event, or the event is not shown,
	   hide the resize bars. */
	if (day != -1 && day == day_view->drag_event_day
	    && event_num == day_view->drag_event_num) {
		gtk_object_get (GTK_OBJECT (day_view->drag_rect_item),
				"x1", &x,
				"y1", &y,
				"x2", &w,
				"y2", &h,
				NULL);
		w -= x;
		x++;
		h -= y;
	} else if (day != -1
		   && e_day_view_get_event_position (day_view, day, event_num,
						     &item_x, &item_y,
						     &item_w, &item_h)) {
		x = item_x + E_DAY_VIEW_BAR_WIDTH;
		y = item_y;
		w = item_w - E_DAY_VIEW_BAR_WIDTH;
		h = item_h;
	} else {
		gnome_canvas_item_hide (day_view->main_canvas_top_resize_bar_item);
		gnome_canvas_item_hide (day_view->main_canvas_bottom_resize_bar_item);
		return;
	}

	gnome_canvas_item_set (day_view->main_canvas_top_resize_bar_item,
			       "x1", x - E_DAY_VIEW_BAR_WIDTH,
			       "y1", y - E_DAY_VIEW_BAR_HEIGHT,
			       "x2", x + w - 1,
			       "y2", y - 1,
			       NULL);
	gnome_canvas_item_show (day_view->main_canvas_top_resize_bar_item);

	gnome_canvas_item_set (day_view->main_canvas_bottom_resize_bar_item,
			       "x1", x - E_DAY_VIEW_BAR_WIDTH,
			       "y1", y + h,
			       "x2", x + w - 1,
			       "y2", y + h + E_DAY_VIEW_BAR_HEIGHT - 1,
			       NULL);
	gnome_canvas_item_show (day_view->main_canvas_bottom_resize_bar_item);
}


static void
e_day_view_ensure_events_sorted (EDayView *day_view)
{
	gint day;

	/* Sort the long events. */
	if (!day_view->long_events_sorted) {
		qsort (day_view->long_events->data,
		       day_view->long_events->len,
		       sizeof (EDayViewEvent),
		       e_day_view_event_sort_func);
		day_view->long_events_sorted = TRUE;
	}

	/* Sort the events for each day. */
	for (day = 0; day < day_view->days_shown; day++) {
		if (!day_view->events_sorted[day]) {
			qsort (day_view->events[day]->data,
			       day_view->events[day]->len,
			       sizeof (EDayViewEvent),
			       e_day_view_event_sort_func);
			day_view->events_sorted[day] = TRUE;
		}
	}
}


static gint
e_day_view_event_sort_func (const void *arg1,
			    const void *arg2)
{
	EDayViewEvent *event1, *event2;

	event1 = (EDayViewEvent*) arg1;
	event2 = (EDayViewEvent*) arg2;

	if (event1->start < event2->start)
		return -1;
	if (event1->start > event2->start)
		return 1;

	if (event1->end > event2->end)
		return -1;
	if (event1->end < event2->end)
		return 1;

	return 0;
}


static gint
e_day_view_key_press (GtkWidget *widget, GdkEventKey *event)
{
	EDayView *day_view;
	iCalObject *ico;
	gint day, event_num;
	gchar *initial_text;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_DAY_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	day_view = E_DAY_VIEW (widget);

	g_print ("In e_day_view_key_press\n");

	/* The Escape key aborts a resize operation. */
	if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE) {
		if (event->keyval == GDK_Escape) {
			e_day_view_abort_resize (day_view, event->time);
		}
		return FALSE;
	}


	if (day_view->selection_start_col == -1)
		return FALSE;

	/* We only want to start an edit with a return key or a simple
	   character. */
	if (event->keyval == GDK_Return) {
		initial_text = NULL;
	} else if ((event->keyval < 0x20)
		   || (event->keyval > 0xFF)
		   || (event->length == 0)
		   || (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK))) {
		return FALSE;
	} else {
		initial_text = event->string;
	}

	/* Add a new event covering the selected range.
	   Note that user_name is a global variable. */
	ico = ical_new ("", user_name, "");
	ico->new = 1;

	e_day_view_get_selection_range (day_view, &ico->dtstart, &ico->dtend);

	gnome_calendar_add_object (day_view->calendar, ico);
	save_default_calendar (day_view->calendar);

	/* gnome_calendar_add_object() should have resulted in a call to
	   e_day_view_update_event(), so the new event should now be layed out.
	   So we try to find it so we can start editing it. */
	if (e_day_view_find_event_from_ico (day_view, ico, &day, &event_num)) {
		/* Start editing the new event. */
		e_day_view_start_editing_event (day_view, day, event_num,
						initial_text);
	}

	return TRUE;
}


static void
e_day_view_start_editing_event (EDayView *day_view,
				gint	  day,
				gint	  event_num,
				gchar    *initial_text)
{
	EDayViewEvent *event;

	/* If we are already editing the event, just return. */
	if (day == day_view->editing_event_day
	    && event_num == day_view->editing_event_num)
		return;

	if (day == E_DAY_VIEW_LONG_EVENT) {
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					event_num);
	} else {
		event = &g_array_index (day_view->events[day], EDayViewEvent,
					event_num);
	}

	/* If the event is not shown, don't try to edit it. */
	if (!event->canvas_item)
		return;

	if (initial_text) {
		gnome_canvas_item_set (event->canvas_item,
				       "text", initial_text,
				       NULL);
	}

	e_canvas_item_grab_focus (event->canvas_item);
}


/* This stops the current edit. If accept is TRUE the event summary is update,
   else the edit is cancelled. */
static void
e_day_view_stop_editing_event (EDayView *day_view)
{
	GtkWidget *toplevel;

	/* Check we are editing an event. */
	if (day_view->editing_event_day == -1)
		return;

	/* Set focus to the toplevel so the item loses focus. */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (day_view));
	if (toplevel && GTK_IS_WINDOW (toplevel))
		gtk_window_set_focus (GTK_WINDOW (toplevel), NULL);
}


static gboolean
e_day_view_on_text_item_event (GnomeCanvasItem *item,
			       GdkEvent *event,
			       EDayView *day_view)
{
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
		/* Only let the EText handle the event while editing. */
		if (!E_TEXT (item)->editing)
			gtk_signal_emit_stop_by_name (GTK_OBJECT (item),
						      "event");
		break;
	case GDK_FOCUS_CHANGE:
		if (event->focus_change.in)
			e_day_view_on_editing_started (day_view, item);
		else
			e_day_view_on_editing_stopped (day_view, item);

		return FALSE;
	default:
		break;
	}

	return FALSE;
}


static void
e_day_view_on_editing_started (EDayView *day_view,
			       GnomeCanvasItem *item)
{
	gint day, event_num;

	if (!e_day_view_find_event_from_item (day_view, item,
					      &day, &event_num))
		return;

	g_print ("In e_day_view_on_editing_started Day:%i Event:%i\n",
		 day, event_num);

	day_view->editing_event_day = day;
	day_view->editing_event_num = event_num;

	if (day == E_DAY_VIEW_LONG_EVENT) {
		e_day_view_reshape_long_event (day_view, event_num);
	} else {
		day_view->resize_bars_event_day = day;
		day_view->resize_bars_event_num = event_num;
		e_day_view_reshape_main_canvas_resize_bars (day_view);
	}
}


static void
e_day_view_on_editing_stopped (EDayView *day_view,
			       GnomeCanvasItem *item)
{
	gint day, event_num;
	gboolean editing_long_event = FALSE;
	EDayViewEvent *event;
	gchar *text = NULL;

	/* Note: the item we are passed here isn't reliable, so we just stop
	   the edit of whatever item was being edited. We also receive this
	   event twice for some reason. */
	day = day_view->editing_event_day;
	event_num = day_view->editing_event_num;

	/* If no item is being edited, just return. */
	if (day == -1)
		return;

	g_print ("In e_day_view_on_editing_stopped Day:%i Event:%i\n",
		 day, event_num);

	if (day == E_DAY_VIEW_LONG_EVENT) {
		editing_long_event = TRUE;
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					event_num);
	} else {
		event = &g_array_index (day_view->events[day], EDayViewEvent,
					event_num);

		/* Hide the horizontal bars. */
		gnome_canvas_item_hide (day_view->main_canvas_top_resize_bar_item);
		gnome_canvas_item_hide (day_view->main_canvas_bottom_resize_bar_item);
	}

	/* Reset the edit fields. */
	day_view->editing_event_day = -1;
	day_view->editing_event_num = -1;

	day_view->resize_bars_event_day = -1;
	day_view->resize_bars_event_num = -1;

	gtk_object_get (GTK_OBJECT (event->canvas_item),
			"text", &text,
			NULL);

	/* Only update the summary if necessary. */
	if (text && event->ico->summary
	    && !strcmp (text, event->ico->summary)) {
		g_free (text);

		if (day == E_DAY_VIEW_LONG_EVENT)
			e_day_view_reshape_long_event (day_view, event_num);
		return;
	}

	if (event->ico->summary)
		g_free (event->ico->summary);

	event->ico->summary = text;

	/* Notify calendar of change. This will result in a call to update,
	   which will reset the event label as appropriate. */
	gnome_calendar_object_changed (day_view->calendar, event->ico,
				       CHANGE_SUMMARY);
	save_default_calendar (day_view->calendar);
}


/* Converts the selected range into a start and end time. */
static void
e_day_view_get_selection_range (EDayView *day_view,
				time_t *start,
				time_t *end)
{
	/* Check if the selection is only in the top canvas, in which case
	   we can simply use the day_starts array. */
	if (day_view->selection_in_top_canvas) {
		*start = day_view->day_starts[day_view->selection_start_col];
		*end = day_view->day_starts[day_view->selection_end_col + 1];
	} else {
		/* Convert the start col + row into a time. */
		*start = e_day_view_convert_grid_position_to_time (day_view, day_view->selection_start_col, day_view->selection_start_row);
		*end = e_day_view_convert_grid_position_to_time (day_view, day_view->selection_end_col, day_view->selection_end_row + 1);
	}
}


/* FIXME: It is possible that we may produce an invalid time due to daylight
   saving times (i.e. when clocks go forward there is a range of time which
   is not valid). I don't know the best way to handle daylight saving time. */
static time_t
e_day_view_convert_grid_position_to_time (EDayView *day_view,
					  gint col,
					  gint row)
{
	struct tm *tmp_tm;
	time_t val;
	gint minutes;

	/* Calulate the number of minutes since the start of the day. */
	minutes = day_view->first_hour_shown * 60
		+ day_view->first_minute_shown
		+ row * day_view->mins_per_row;

	/* A special case for midnight, where we can use the start of the
	   next day. */
	if (minutes == 60 * 24)
		return day_view->day_starts[col + 1];

	/* We convert the start of the day to a struct tm, then set the
	   hour and minute, then convert it back to a time_t. */
	tmp_tm = localtime (&day_view->day_starts[col]);

	tmp_tm->tm_hour = minutes / 60;
	tmp_tm->tm_min = minutes % 60;
	tmp_tm->tm_isdst = -1;

	val = mktime (tmp_tm);
	return val;
}


/* This starts or stops auto-scrolling when dragging a selection or resizing
   an event. */
static void
e_day_view_check_auto_scroll (EDayView *day_view,
			      gint event_y)
{
	g_print ("Event Y:%i\n", event_y);
	if (event_y < E_DAY_VIEW_AUTO_SCROLL_OFFSET)
		e_day_view_start_auto_scroll (day_view, TRUE);
	else if (event_y >= day_view->main_canvas->allocation.height
		 - E_DAY_VIEW_AUTO_SCROLL_OFFSET)
		e_day_view_start_auto_scroll (day_view, FALSE);
	else
		e_day_view_stop_auto_scroll (day_view);
}


static void
e_day_view_start_auto_scroll (EDayView *day_view,
			      gboolean scroll_up)
{
	if (day_view->auto_scroll_timeout_id == 0) {
		day_view->auto_scroll_timeout_id = g_timeout_add (E_DAY_VIEW_AUTO_SCROLL_TIMEOUT, e_day_view_auto_scroll_handler, day_view);
		day_view->auto_scroll_delay = E_DAY_VIEW_AUTO_SCROLL_DELAY;
	}
	day_view->auto_scroll_up = scroll_up;
}


static void
e_day_view_stop_auto_scroll (EDayView *day_view)
{
	if (day_view->auto_scroll_timeout_id != 0) {
		gtk_timeout_remove (day_view->auto_scroll_timeout_id);
		day_view->auto_scroll_timeout_id = 0;
	}
}


static gboolean
e_day_view_auto_scroll_handler (gpointer data)
{
	EDayView *day_view;
	EDayViewPosition pos;
	gint scroll_x, scroll_y, new_scroll_y, canvas_x, canvas_y, row, day;
	GtkAdjustment *adj;

	g_return_val_if_fail (E_IS_DAY_VIEW (data), FALSE);

	day_view = E_DAY_VIEW (data);

	GDK_THREADS_ENTER ();

	if (day_view->auto_scroll_delay > 0) {
		day_view->auto_scroll_delay--;
		GDK_THREADS_LEAVE ();
		return TRUE;
	}

	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (day_view->main_canvas),
					 &scroll_x, &scroll_y);

	adj = GTK_LAYOUT (day_view->main_canvas)->vadjustment;

	if (day_view->auto_scroll_up)
		new_scroll_y = MAX (scroll_y - adj->step_increment, 0);
	else
		new_scroll_y = MIN (scroll_y + adj->step_increment,
				    adj->upper - adj->page_size);

	if (new_scroll_y != scroll_y) {
		/* NOTE: This reduces flicker, but only works if we don't use
		   canvas items which have X windows. */
		gtk_layout_freeze (GTK_LAYOUT (day_view->main_canvas));

		gnome_canvas_scroll_to (GNOME_CANVAS (day_view->main_canvas),
					scroll_x, new_scroll_y);

		gtk_layout_thaw (GTK_LAYOUT (day_view->main_canvas));
	}

	canvas_x = day_view->last_mouse_x + scroll_x;
	canvas_y = day_view->last_mouse_y + new_scroll_y;

	/* Update the selection/resize/drag if necessary. */
	pos = e_day_view_convert_position_in_main_canvas (day_view,
							  canvas_x, canvas_y,
							  &day, &row, NULL);

	if (pos != E_DAY_VIEW_POS_OUTSIDE) {
		if (day_view->selection_drag_pos != E_DAY_VIEW_DRAG_NONE) {
			e_day_view_update_selection (day_view, row, day);
		} else if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE) {
			e_day_view_update_resize (day_view, row);
		} else if (day_view->drag_item->object.flags
			   & GNOME_CANVAS_ITEM_VISIBLE) {
			e_day_view_update_main_canvas_drag (day_view, row,
							    day);
		}
	}

	GDK_THREADS_LEAVE ();
	return TRUE;
}


gboolean
e_day_view_get_event_position (EDayView *day_view,
			       gint day,
			       gint event_num,
			       gint *item_x,
			       gint *item_y,
			       gint *item_w,
			       gint *item_h)
{
	EDayViewEvent *event;
	gint start_row, end_row, cols_in_row, start_col, num_columns;

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	/* If the event is flagged as not displayed, return FALSE. */
	if (event->num_columns == 0)
		return FALSE;

	start_row = event->start_minute / day_view->mins_per_row;
	end_row = (event->end_minute - 1) / day_view->mins_per_row;
	cols_in_row = day_view->cols_per_row[day][start_row];
	start_col = event->start_row_or_col;
	num_columns = event->num_columns;

	/* If the event is being resize, use the resize position. */
	if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE
	    && day_view->resize_event_day == day
	    && day_view->resize_event_num == event_num) {
		if (day_view->resize_drag_pos == E_DAY_VIEW_POS_TOP_EDGE)
			start_row = day_view->resize_start_row;
		else if (day_view->resize_drag_pos == E_DAY_VIEW_POS_BOTTOM_EDGE)
			end_row = day_view->resize_end_row;
	}

	*item_x = day_view->day_offsets[day] + day_view->day_widths[day] * start_col / cols_in_row;
	*item_w = day_view->day_widths[day] * num_columns / cols_in_row - E_DAY_VIEW_GAP_WIDTH;
	*item_y = start_row * day_view->row_height;
	*item_h = (end_row - start_row + 1) * day_view->row_height;

	return TRUE;
}


gboolean
e_day_view_get_long_event_position	(EDayView	*day_view,
					 gint		 event_num,
					 gint		*start_day,
					 gint		*end_day,
					 gint		*item_x,
					 gint		*item_y,
					 gint		*item_w,
					 gint		*item_h)
{
	EDayViewEvent *event;

	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	/* If the event is flagged as not displayed, return FALSE. */
	if (event->num_columns == 0)
		return FALSE;

	if (!e_day_view_find_long_event_days (day_view, event,
					      start_day, end_day))
		return FALSE;

	/* If the event is being resize, use the resize position. */
	if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE
	    && day_view->resize_event_day == E_DAY_VIEW_LONG_EVENT
	    && day_view->resize_event_num == event_num) {
		if (day_view->resize_drag_pos == E_DAY_VIEW_POS_LEFT_EDGE)
			*start_day = day_view->resize_start_row;
		else if (day_view->resize_drag_pos == E_DAY_VIEW_POS_RIGHT_EDGE)
			*end_day = day_view->resize_end_row;
	}

	*item_x = day_view->day_offsets[*start_day] + E_DAY_VIEW_BAR_WIDTH;
	*item_w = day_view->day_offsets[*end_day + 1] - *item_x
		- E_DAY_VIEW_GAP_WIDTH;
	*item_y = (event->start_row_or_col + 1) * day_view->top_row_height;
	*item_h = day_view->top_row_height - E_DAY_VIEW_TOP_CANVAS_Y_GAP;
	return TRUE;
}


/* Converts a position within the entire top canvas to a day & event and
   a place within the event if appropriate. */
static EDayViewPosition
e_day_view_convert_position_in_top_canvas (EDayView *day_view,
					   gint x,
					   gint y,
					   gint *day_return,
					   gint *event_num_return)
{
	EDayViewEvent *event;
	gint day, row, col;
	gint event_num, start_day, end_day, item_x, item_y, item_w, item_h;

	if (x < 0 || y < 0)
		return E_DAY_VIEW_POS_OUTSIDE;

	row = y / day_view->top_row_height - 1;

	day = -1;
	for (col = 1; col <= day_view->days_shown; col++) {
		if (x < day_view->day_offsets[col]) {
			day = col - 1;	
			break;
		}
	}
	if (day == -1)
		return E_DAY_VIEW_POS_OUTSIDE;

	*day_return = day;

	/* If only the grid position is wanted, return. */
	if (event_num_return == NULL)
		return E_DAY_VIEW_POS_NONE;

	for (event_num = 0; event_num < day_view->long_events->len;
	     event_num++) {
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					event_num);

		if (event->start_row_or_col != row)
			continue;

		if (!e_day_view_get_long_event_position (day_view, event_num,
							 &start_day, &end_day,
							 &item_x, &item_y,
							 &item_w, &item_h))
			continue;

		if (x < item_x)
			continue;

		if (x >= item_x + item_w)
			continue;

		*event_num_return = event_num;

		if (x < item_x + E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH
		    + E_DAY_VIEW_LONG_EVENT_X_PAD)
			return E_DAY_VIEW_POS_LEFT_EDGE;

		if (x >= item_x + item_w - E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH
		    - E_DAY_VIEW_LONG_EVENT_X_PAD)
			return E_DAY_VIEW_POS_RIGHT_EDGE;

		return E_DAY_VIEW_POS_EVENT;
	}

	return E_DAY_VIEW_POS_NONE;
}


/* Converts a position within the entire main canvas to a day, row, event and
   a place within the event if appropriate. */
static EDayViewPosition
e_day_view_convert_position_in_main_canvas (EDayView *day_view,
					    gint x,
					    gint y,
					    gint *day_return,
					    gint *row_return,
					    gint *event_num_return)
{
	gint day, row, col, event_num;
	gint item_x, item_y, item_w, item_h;

	/* Check the position is inside the canvas, and determine the day
	   and row. */
	if (x < 0 || y < 0)
		return E_DAY_VIEW_POS_OUTSIDE;

	row = y / day_view->row_height;
	if (row >= day_view->rows)
		return E_DAY_VIEW_POS_OUTSIDE;

	day = -1;
	for (col = 1; col <= day_view->days_shown; col++) {
		if (x < day_view->day_offsets[col]) {
			day = col - 1;	
			break;
		}
	}
	if (day == -1)
		return E_DAY_VIEW_POS_OUTSIDE;

	*day_return = day;
	*row_return = row;

	/* If only the grid position is wanted, return. */
	if (event_num_return == NULL)
		return E_DAY_VIEW_POS_NONE;

	/* Check the selected item first, since the horizontal resizing bars
	   may be above other events. */
	if (day_view->resize_bars_event_day == day) {
		if (e_day_view_get_event_position (day_view, day,
						   day_view->resize_bars_event_num,
						   &item_x, &item_y,
						   &item_w, &item_h)) {
			if (x >= item_x && x < item_x + item_w) {
				*event_num_return = day_view->resize_bars_event_num;
				if (y >= item_y - E_DAY_VIEW_BAR_HEIGHT
				    && y < item_y + E_DAY_VIEW_EVENT_BORDER_HEIGHT)
					return E_DAY_VIEW_POS_TOP_EDGE;
				if (y >= item_y + item_h - E_DAY_VIEW_EVENT_BORDER_HEIGHT
				    && y < item_y + item_h + E_DAY_VIEW_BAR_HEIGHT)
					return E_DAY_VIEW_POS_BOTTOM_EDGE;
			}
		}
	}

	/* Try to find the event at the found position. */
	*event_num_return = -1;
	for (event_num = 0; event_num < day_view->events[day]->len;
	     event_num++) {
		if (!e_day_view_get_event_position (day_view, day, event_num,
						    &item_x, &item_y,
						    &item_w, &item_h))
			continue;

		if (x < item_x || x >= item_x + item_w
		    || y < item_y || y >= item_y + item_h)
			continue;

		*event_num_return = event_num;

		if (x < item_x + E_DAY_VIEW_BAR_WIDTH)
			return E_DAY_VIEW_POS_LEFT_EDGE;

		if (y < item_y + E_DAY_VIEW_EVENT_BORDER_HEIGHT
		    + E_DAY_VIEW_EVENT_Y_PAD)
			return E_DAY_VIEW_POS_TOP_EDGE;

		if (y >= item_y + item_h - E_DAY_VIEW_EVENT_BORDER_HEIGHT
		    - E_DAY_VIEW_EVENT_Y_PAD)
			return E_DAY_VIEW_POS_BOTTOM_EDGE;

		return E_DAY_VIEW_POS_EVENT;
	}

	return E_DAY_VIEW_POS_NONE;
}


static gint
e_day_view_on_top_canvas_drag_motion (GtkWidget      *widget,
				      GdkDragContext *context,
				      gint            x,
				      gint            y,
				      guint           time,
				      EDayView	     *day_view)
{
	gint scroll_x, scroll_y;

	g_print ("In e_day_view_on_top_canvas_drag_motion\n");

	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (widget),
					 &scroll_x, &scroll_y);
	day_view->drag_event_x = x + scroll_x;
	day_view->drag_event_y = y + scroll_y;

	e_day_view_reshape_top_canvas_drag_item (day_view);

	return TRUE;
}


static void
e_day_view_reshape_top_canvas_drag_item (EDayView *day_view)
{
	EDayViewPosition pos;
	gint x, y, day;

	/* Calculate the day & start row of the event being dragged, using
	   the current mouse position. */
	x = day_view->drag_event_x;
	y = day_view->drag_event_y;
	pos = e_day_view_convert_position_in_top_canvas (day_view, x, y,
							 &day, NULL);
	/* This shouldn't really happen in a drag. */
	if (pos == E_DAY_VIEW_POS_OUTSIDE)
		return;

	if (day_view->drag_event_day == E_DAY_VIEW_LONG_EVENT)
		day -= day_view->drag_event_offset;
	day = MAX (day, 0);

	e_day_view_update_top_canvas_drag (day_view, day);
}


static void
e_day_view_update_top_canvas_drag (EDayView *day_view,
				   gint day)
{
	EDayViewEvent *event = NULL;
	gint row, num_days, start_day, end_day;
	gdouble item_x, item_y, item_w, item_h;
	GdkFont *font;
	gchar *text;


	/* Calculate the event's position. If the event is in the same
	   position we started in, we use the same columns. */
	row = day_view->rows_in_top_display + 1;
	num_days = 1;

	if (day_view->drag_event_day == E_DAY_VIEW_LONG_EVENT) {
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					day_view->drag_event_num);
		row = event->start_row_or_col + 1;

		if (!e_day_view_find_long_event_days (day_view, event,
						      &start_day, &end_day))
			return;

		num_days = end_day - start_day + 1;

		/* Make sure we don't go off the screen. */
		day = MIN (day, day_view->days_shown - num_days);

	} else if (day_view->drag_event_day != -1) {
		event = &g_array_index (day_view->events[day_view->drag_event_day],
					EDayViewEvent,
					day_view->drag_event_num);
	}

	/* If the position hasn't changed, just return. */
	if (day_view->drag_last_day == day
	    && (day_view->drag_long_event_item->object.flags
		& GNOME_CANVAS_ITEM_VISIBLE))
		return;

	day_view->drag_last_day = day;


	item_x = day_view->day_offsets[day] + E_DAY_VIEW_BAR_WIDTH;
	item_w = day_view->day_offsets[day + num_days] - item_x
		- E_DAY_VIEW_GAP_WIDTH;
	item_y = row * day_view->top_row_height;
	item_h = day_view->top_row_height - E_DAY_VIEW_TOP_CANVAS_Y_GAP;


	g_print ("Moving to %g,%g %gx%g\n", item_x, item_y, item_w, item_h);

	/* Set the positions of the event & associated items. */
	gnome_canvas_item_set (day_view->drag_long_event_rect_item,
			       "x1", item_x,
			       "y1", item_y,
			       "x2", item_x + item_w - 1,
			       "y2", item_y + item_h - 1,
			       NULL);

	font = GTK_WIDGET (day_view)->style->font;
	gnome_canvas_item_set (day_view->drag_long_event_item,
			       "font_gdk", font,
			       "x", item_x + E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH + E_DAY_VIEW_LONG_EVENT_X_PAD,
			       "y", item_y + E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT + E_DAY_VIEW_LONG_EVENT_Y_PAD,
			       "clip_width", item_w - (E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH + E_DAY_VIEW_LONG_EVENT_X_PAD) * 2,
			       "clip_height", item_h - (E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT + E_DAY_VIEW_LONG_EVENT_Y_PAD) * 2,
			       NULL);

	if (!(day_view->drag_long_event_rect_item->object.flags & GNOME_CANVAS_ITEM_VISIBLE)) {
		gnome_canvas_item_raise_to_top (day_view->drag_long_event_rect_item);
		gnome_canvas_item_show (day_view->drag_long_event_rect_item);
	}

	/* Set the text, if necessary. We don't want to set the text every
	   time it moves, so we check if it is currently invisible and only
	   set the text then. */
	if (!(day_view->drag_long_event_item->object.flags
	      & GNOME_CANVAS_ITEM_VISIBLE)) {
		if (event)
			text = event->ico->summary;
		else
			text = NULL;

		gnome_canvas_item_set (day_view->drag_long_event_item,
				       "text", text ? text : "",
				       NULL);
		gnome_canvas_item_raise_to_top (day_view->drag_long_event_item);
		gnome_canvas_item_show (day_view->drag_long_event_item);
	}
}


static gint
e_day_view_on_main_canvas_drag_motion (GtkWidget      *widget,
				       GdkDragContext *context,
				       gint            x,
				       gint            y,
				       guint           time,
				       EDayView	      *day_view)
{
	gint scroll_x, scroll_y;

	g_print ("In e_day_view_on_main_canvas_drag_motion\n");

	day_view->last_mouse_x = x;
	day_view->last_mouse_y = y;

	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (widget),
					 &scroll_x, &scroll_y);
	day_view->drag_event_x = x + scroll_x;
	day_view->drag_event_y = y + scroll_y;

	e_day_view_reshape_main_canvas_drag_item (day_view);
	e_day_view_reshape_main_canvas_resize_bars (day_view);

	e_day_view_check_auto_scroll (day_view, y);

	return TRUE;
}


static void
e_day_view_reshape_main_canvas_drag_item (EDayView *day_view)
{
	EDayViewPosition pos;
	gint x, y, day, row;

	/* Calculate the day & start row of the event being dragged, using
	   the current mouse position. */
	x = day_view->drag_event_x;
	y = day_view->drag_event_y;
	pos = e_day_view_convert_position_in_main_canvas (day_view, x, y,
							  &day, &row, NULL);
	/* This shouldn't really happen in a drag. */
	if (pos == E_DAY_VIEW_POS_OUTSIDE)
		return;

	if (day_view->drag_event_day != -1
	    && day_view->drag_event_day != E_DAY_VIEW_LONG_EVENT)
		row -= day_view->drag_event_offset;
	row = MAX (row, 0);

	e_day_view_update_main_canvas_drag (day_view, row, day);
}


static void
e_day_view_update_main_canvas_drag (EDayView *day_view,
				    gint row,
				    gint day)
{
	EDayViewEvent *event = NULL;
	gint cols_in_row, start_col, num_columns, num_rows, start_row, end_row;
	gdouble item_x, item_y, item_w, item_h;
	GdkFont *font;
	gchar *text;

	/* If the position hasn't changed, just return. */
	if (day_view->drag_last_day == day
	    && day_view->drag_last_row == row
	    && (day_view->drag_item->object.flags & GNOME_CANVAS_ITEM_VISIBLE))
		return;

	day_view->drag_last_day = day;
	day_view->drag_last_row = row;

	/* Calculate the event's position. If the event is in the same
	   position we started in, we use the same columns. */
	cols_in_row = 1;
	start_col = 0;
	num_columns = 1;
	num_rows = 1;

	if (day_view->drag_event_day == E_DAY_VIEW_LONG_EVENT) {
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					day_view->drag_event_num);
	} else if (day_view->drag_event_day != -1) {
		event = &g_array_index (day_view->events[day_view->drag_event_day],
					EDayViewEvent,
					day_view->drag_event_num);
		start_row = event->start_minute / day_view->mins_per_row;
		end_row = (event->end_minute - 1) / day_view->mins_per_row;
		num_rows = end_row - start_row + 1;
	}

	if (day_view->drag_event_day == day && start_row == row) {
		cols_in_row = day_view->cols_per_row[day][row];
		start_col = event->start_row_or_col;
		num_columns = event->num_columns;
	}

	item_x = day_view->day_offsets[day]
		+ day_view->day_widths[day] * start_col / cols_in_row;
	item_w = day_view->day_widths[day] * num_columns / cols_in_row
		- E_DAY_VIEW_GAP_WIDTH;
	item_y = row * day_view->row_height;
	item_h = num_rows * day_view->row_height;

	g_print ("Moving to %g,%g %gx%g\n", item_x, item_y, item_w, item_h);

	/* Set the positions of the event & associated items. */
	gnome_canvas_item_set (day_view->drag_rect_item,
			       "x1", item_x + E_DAY_VIEW_BAR_WIDTH - 1,
			       "y1", item_y,
			       "x2", item_x + item_w - 1,
			       "y2", item_y + item_h - 1,
			       NULL);

	gnome_canvas_item_set (day_view->drag_bar_item,
			       "x1", item_x,
			       "y1", item_y,
			       "x2", item_x + E_DAY_VIEW_BAR_WIDTH - 1,
			       "y2", item_y + item_h - 1,
			       NULL);

	font = GTK_WIDGET (day_view)->style->font;
	gnome_canvas_item_set (day_view->drag_item,
			       "font_gdk", font,
			       "x", item_x + E_DAY_VIEW_BAR_WIDTH + E_DAY_VIEW_EVENT_X_PAD,
			       "y", item_y + E_DAY_VIEW_EVENT_BORDER_HEIGHT + E_DAY_VIEW_EVENT_Y_PAD,
			       "clip_width", item_w - E_DAY_VIEW_BAR_WIDTH - E_DAY_VIEW_EVENT_X_PAD * 2,
			       "clip_height", item_h - (E_DAY_VIEW_EVENT_BORDER_HEIGHT + E_DAY_VIEW_EVENT_Y_PAD) * 2,
			       NULL);

	if (!(day_view->drag_bar_item->object.flags & GNOME_CANVAS_ITEM_VISIBLE)) {
		gnome_canvas_item_raise_to_top (day_view->drag_bar_item);
		gnome_canvas_item_show (day_view->drag_bar_item);
	}

	if (!(day_view->drag_rect_item->object.flags & GNOME_CANVAS_ITEM_VISIBLE)) {
		gnome_canvas_item_raise_to_top (day_view->drag_rect_item);
		gnome_canvas_item_show (day_view->drag_rect_item);
	}

	/* Set the text, if necessary. We don't want to set the text every
	   time it moves, so we check if it is currently invisible and only
	   set the text then. */
	if (!(day_view->drag_item->object.flags & GNOME_CANVAS_ITEM_VISIBLE)) {
		if (event)
			text = event->ico->summary;
		else
			text = NULL;

		gnome_canvas_item_set (day_view->drag_item,
				       "text", text ? text : "",
				       NULL);
		gnome_canvas_item_raise_to_top (day_view->drag_item);
		gnome_canvas_item_show (day_view->drag_item);
	}
}


static void
e_day_view_on_top_canvas_drag_leave (GtkWidget      *widget,
				     GdkDragContext *context,
				     guint           time,
				     EDayView	     *day_view)
{
	g_print ("In e_day_view_on_top_canvas_drag_leave\n");

	day_view->drag_last_day = -1;

	gnome_canvas_item_hide (day_view->drag_long_event_rect_item);
	gnome_canvas_item_hide (day_view->drag_long_event_item);
}


static void
e_day_view_on_main_canvas_drag_leave (GtkWidget      *widget,
				      GdkDragContext *context,
				      guint           time,
				      EDayView	     *day_view)
{
	g_print ("In e_day_view_on_main_canvas_drag_leave\n");

	day_view->drag_last_day = -1;

	e_day_view_stop_auto_scroll (day_view);

	gnome_canvas_item_hide (day_view->drag_rect_item);
	gnome_canvas_item_hide (day_view->drag_bar_item);
	gnome_canvas_item_hide (day_view->drag_item);

	/* Hide the resize bars if they are being used in the drag. */
	if (day_view->drag_event_day == day_view->resize_bars_event_day
	    && day_view->drag_event_num == day_view->resize_bars_event_num) {
		gnome_canvas_item_hide (day_view->main_canvas_top_resize_bar_item);
		gnome_canvas_item_hide (day_view->main_canvas_bottom_resize_bar_item);
	}
}


static void
e_day_view_on_drag_begin (GtkWidget      *widget,
			  GdkDragContext *context,
			  EDayView	 *day_view)
{
	EDayViewEvent *event;
	gint day, event_num;

	g_print ("In e_day_view_on_main_canvas_drag_begin\n");

	day = day_view->drag_event_day;
	event_num = day_view->drag_event_num;

	/* These should both be set. */
	g_return_if_fail (day != -1);
	g_return_if_fail (event_num != -1);

	if (day == E_DAY_VIEW_LONG_EVENT)
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					event_num);
	else
		event = &g_array_index (day_view->events[day], EDayViewEvent,
					event_num);

	/* Hide the text item, since it will be shown in the special drag
	   items. */
	gnome_canvas_item_hide (event->canvas_item);
}


static void
e_day_view_on_drag_end (GtkWidget      *widget,
			GdkDragContext *context,
			EDayView       *day_view)
{
	EDayViewEvent *event;
	gint day, event_num;

	g_print ("In e_day_view_on_main_canvas_drag_end\n");

	day = day_view->drag_event_day;
	event_num = day_view->drag_event_num;

	/* If the calendar has already been updated in drag_data_received()
	   we just return. */
	if (day == -1 || event_num == -1)
		return;

	if (day == E_DAY_VIEW_LONG_EVENT) {
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					event_num);
		gtk_widget_queue_draw (day_view->top_canvas);
	} else {
		event = &g_array_index (day_view->events[day], EDayViewEvent,
					event_num);
		gtk_widget_queue_draw (day_view->main_canvas);
	}

	/* Show the text item again. */
	gnome_canvas_item_show (event->canvas_item);

	day_view->drag_event_day = -1;
	day_view->drag_event_num = -1;
}


static void
e_day_view_on_drag_data_get (GtkWidget          *widget,
			     GdkDragContext     *context,
			     GtkSelectionData   *selection_data,
			     guint               info,
			     guint               time,
			     EDayView		*day_view)
{
	EDayViewEvent *event;
	gint day, event_num;
	gchar *event_uid;

	g_print ("In e_day_view_on_drag_data_get\n");

	day = day_view->drag_event_day;
	event_num = day_view->drag_event_num;

	/* These should both be set. */
	g_return_if_fail (day != -1);
	g_return_if_fail (event_num != -1);

	if (day == E_DAY_VIEW_LONG_EVENT)
		event = &g_array_index (day_view->long_events,
					EDayViewEvent, event_num);
	else
		event = &g_array_index (day_view->events[day],
					EDayViewEvent, event_num);

	event_uid = event->ico->uid;
	g_return_if_fail (event_uid != NULL);

	if (info == TARGET_CALENDAR_EVENT) {
		gtk_selection_data_set (selection_data,	selection_data->target,
					8, event_uid, strlen (event_uid));
	}
}


static void  
e_day_view_on_top_canvas_drag_data_received  (GtkWidget          *widget,
					      GdkDragContext     *context,
					      gint                x,
					      gint                y,
					      GtkSelectionData   *data,
					      guint               info,
					      guint               time,
					      EDayView	         *day_view)
{
	EDayViewEvent *event;
	EDayViewPosition pos;
	gint day, row, scroll_x, scroll_y, start_day, end_day, num_days;
	gchar *event_uid;

	g_print ("In e_day_view_on_top_canvas_drag_data_received\n");

	if ((data->length >= 0) && (data->format == 8)) {
		pos = e_day_view_convert_position_in_top_canvas (day_view,
								 x, y, &day,
								 NULL);
		if (pos != E_DAY_VIEW_POS_OUTSIDE) {
			num_days = 1;
			if (day_view->drag_event_day == E_DAY_VIEW_LONG_EVENT) {
				event = &g_array_index (day_view->long_events, EDayViewEvent,
							day_view->drag_event_num);
				day -= day_view->drag_event_offset;
				day = MAX (day, 0);

				e_day_view_find_long_event_days (day_view,
								 event,
								 &start_day,
								 &end_day);
				num_days = end_day - start_day + 1;
				/* Make sure we don't go off the screen. */
				day = MIN (day, day_view->days_shown - num_days);
			} else if (day_view->drag_event_day != -1) {
				event = &g_array_index (day_view->events[day_view->drag_event_day],
							EDayViewEvent,
							day_view->drag_event_num);
			}

			event_uid = data->data;

			g_print ("Dropped Day:%i UID:%s\n", day, event_uid);

			if (!event_uid || !event->ico->uid
			    || strcmp (event_uid, event->ico->uid))
				g_warning ("Unexpected event UID");

			event->ico->dtstart = day_view->day_starts[day];
			event->ico->dtend = day_view->day_starts[day + num_days];

			gtk_drag_finish (context, TRUE, TRUE, time);

			/* Reset this since it will be invalid. */
			day_view->drag_event_day = -1;

			/* Notify calendar of change */
			gnome_calendar_object_changed (day_view->calendar,
						       event->ico,
						       CHANGE_DATES);
			save_default_calendar (day_view->calendar);

			return;
		}
	}

	gtk_drag_finish (context, FALSE, FALSE, time);
}


static void  
e_day_view_on_main_canvas_drag_data_received  (GtkWidget          *widget,
					       GdkDragContext     *context,
					       gint                x,
					       gint                y,
					       GtkSelectionData   *data,
					       guint               info,
					       guint               time,
					       EDayView		  *day_view)
{
	EDayViewEvent *event;
	EDayViewPosition pos;
	gint day, start_row, end_row, num_rows;
	gchar *event_uid;

	g_print ("In e_day_view_on_main_canvas_drag_data_received\n");

	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (widget),
					 &scroll_x, &scroll_y);
	x += scroll_x;
	y += scroll_y;

	if ((data->length >= 0) && (data->format == 8)) {
		pos = e_day_view_convert_position_in_main_canvas (day_view,
								  x, y, &day,
								  &row, NULL);
		if (pos != E_DAY_VIEW_POS_OUTSIDE) {
			num_rows = 1;
			if (day_view->drag_event_day == E_DAY_VIEW_LONG_EVENT) {
				event = &g_array_index (day_view->long_events, EDayViewEvent,
							day_view->drag_event_num);
			} else if (day_view->drag_event_day != -1) {
				event = &g_array_index (day_view->events[day_view->drag_event_day],
							EDayViewEvent,
							day_view->drag_event_num);
				row -= day_view->drag_event_offset;

				/* Calculate time offset from start row. */
				start_row = event->start_minute / day_view->mins_per_row;
				end_row = (event->end_minute - 1) / day_view->mins_per_row;
				num_rows = end_row - start_row + 1;
			}

			event_uid = data->data;

			g_print ("Dropped Day:%i Row:%i UID:%s\n", day, row,
				 event_uid);

			if (!event_uid || !event->ico->uid
			    || strcmp (event_uid, event->ico->uid))
				g_warning ("Unexpected event UID");

			event->ico->dtstart = e_day_view_convert_grid_position_to_time (day_view, day, row);
			event->ico->dtend = e_day_view_convert_grid_position_to_time (day_view, day, row + num_rows);

			gtk_drag_finish (context, TRUE, TRUE, time);

			/* Reset this since it will be invalid. */
			day_view->drag_event_day = -1;

			/* Notify calendar of change */
			gnome_calendar_object_changed (day_view->calendar,
						       event->ico,
						       CHANGE_DATES);
			save_default_calendar (day_view->calendar);

			return;
		}
	}

	gtk_drag_finish (context, FALSE, FALSE, time);
}

