/*
 * e-cal-shell-view-private.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-cal-shell-view-private.h"

#include "calendar/gui/calendar-view-factory.h"
#include "widgets/menus/gal-view-factory-etable.h"

static void
cal_shell_view_process_completed_tasks (ECalShellView *cal_shell_view,
                                        gboolean config_changed)
{
#if 0
	ECalShellContent *cal_shell_content;
	ECalendarTable *task_table;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	e_calendar_table_process_completed_tasks (
		task_table, clients, config_changed);
#endif
}

static void
cal_shell_view_config_timezone_changed_cb (GConfClient *client,
                                           guint id,
                                           GConfEntry *entry,
                                           gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;

	e_cal_shell_view_update_timezone (cal_shell_view);
}

static struct tm
cal_shell_view_get_current_time (ECalendarItem *calitem,
                                 ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	struct icaltimetype tt;
	icaltimezone *timezone;
	ECalModel *model;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	model = e_cal_shell_content_get_model (cal_shell_content);
	timezone = e_cal_model_get_timezone (model);

	tt = icaltime_from_timet_with_zone (time (NULL), FALSE, timezone);

	return icaltimetype_to_tm (&tt);
}

static void
cal_shell_view_date_navigator_date_range_changed_cb (ECalShellView *cal_shell_view,
                                                     ECalendarItem *calitem)
{
	/* FIXME gnome-calendar.c calls update_query() here. */
}

static void
cal_shell_view_date_navigator_selection_changed_cb (ECalShellView *cal_shell_view,
                                                    ECalendarItem *calitem)
{
	/* FIXME */
}

static void
cal_shell_view_date_navigator_scroll_event_cb (ECalShellView *cal_shell_view,
                                               GdkEventScroll *event,
                                               ECalendar *date_navigator)
{
	ECalendarItem *calitem;
	GDate start_date, end_date;

	calitem = date_navigator->calitem;
	if (!e_calendar_item_get_selection (calitem, &start_date, &end_date))
		return;

	switch (event->direction) {
		case GDK_SCROLL_UP:
			g_date_subtract_months (&start_date, 1);
			g_date_subtract_months (&end_date, 1);
			break;

		case GDK_SCROLL_DOWN:
			g_date_add_months (&start_date, 1);
			g_date_add_months (&end_date, 1);
			break;

		default:
			g_return_if_reached ();
	}

	/* XXX Does ECalendarItem emit a signal for this?  If so, maybe
	 *     we could move this handler into ECalShellSidebar. */
	e_calendar_item_set_selection (calitem, &start_date, &end_date);

	cal_shell_view_date_navigator_date_range_changed_cb (
		cal_shell_view, calitem);
}

static gboolean
cal_shell_view_selector_popup_event_cb (EShellView *shell_view,
                                        ESource *primary_source,
                                        GdkEventButton *event)
{
	const gchar *widget_path;

	widget_path = "/calendar-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, event);

	return TRUE;
}

static void
cal_shell_view_memopad_popup_event_cb (EShellView *shell_view,
                                       GdkEventButton *event)
{
	const gchar *widget_path;

	widget_path = "/calendar-memopad-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, event);
}

static void
cal_shell_view_taskpad_popup_event_cb (EShellView *shell_view,
                                       GdkEventButton *event)
{
	const gchar *widget_path;

	widget_path = "/calendar-taskpad-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, event);
}

static void
cal_shell_view_user_created_cb (ECalShellView *cal_shell_view,
                                ECalendarView *calendar_view)
{
	ECalShellSidebar *cal_shell_sidebar;
	ECalModel *model;
	ECal *client;
	ESource *source;

	model = e_calendar_view_get_model (calendar_view);
	client = e_cal_model_get_default_client (model);
	source = e_cal_get_source (client);

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	e_cal_shell_sidebar_add_source (cal_shell_sidebar, source);

	e_cal_model_add_client (model, client);
}

static void
cal_shell_view_load_view_collection (EShellViewClass *shell_view_class)
{
	GalViewCollection *collection;
	GalViewFactory *factory;
	ETableSpecification *spec;
	const gchar *base_dir;
	gchar *filename;

	collection = shell_view_class->view_collection;

	base_dir = EVOLUTION_ETSPECDIR;
	spec = e_table_specification_new ();
	filename = g_build_filename (base_dir, ETSPEC_FILENAME, NULL);
	if (!e_table_specification_load_from_file (spec, filename))
		g_critical ("Unable to load ETable specification file "
			    "for calendars");
	g_free (filename);

	factory = calendar_view_factory_new (GNOME_CAL_DAY_VIEW);
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);

	factory = calendar_view_factory_new (GNOME_CAL_WORK_WEEK_VIEW);
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);

	factory = calendar_view_factory_new (GNOME_CAL_WEEK_VIEW);
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);

	factory = calendar_view_factory_new (GNOME_CAL_MONTH_VIEW);
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);

	factory = gal_view_factory_etable_new (spec);
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);
	g_object_unref (spec);

	gal_view_collection_load (collection);
}

static void
cal_shell_view_notify_view_id_cb (ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GalViewInstance *view_instance;
	const gchar *view_id;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	view_instance =
		e_cal_shell_content_get_view_instance (cal_shell_content);
	view_id = e_shell_view_get_view_id (E_SHELL_VIEW (cal_shell_view));

	/* A NULL view ID implies we're in a custom view.  But you can
	 * only get to a custom view via the "Define Views" dialog, which
	 * would have already modified the view instance appropriately.
	 * Furthermore, there's no way to refer to a custom view by ID
	 * anyway, since custom views have no IDs. */
	if (view_id == NULL)
		return;

	gal_view_instance_set_current_view_id (view_instance, view_id);
}

void
e_cal_shell_view_private_init (ECalShellView *cal_shell_view,
                               EShellViewClass *shell_view_class)
{
	if (!gal_view_collection_loaded (shell_view_class->view_collection))
		cal_shell_view_load_view_collection (shell_view_class);

	g_signal_connect (
		cal_shell_view, "notify::view-id",
		G_CALLBACK (cal_shell_view_notify_view_id_cb), NULL);
}

void
e_cal_shell_view_private_constructed (ECalShellView *cal_shell_view)
{
	ECalShellViewPrivate *priv = cal_shell_view->priv;
	ECalShellContent *cal_shell_content;
	ECalShellSidebar *cal_shell_sidebar;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellWindow *shell_window;
	EShellView *shell_view;
	GnomeCalendar *calendar;
	ECalendar *date_navigator;
	EMemoTable *memo_table;
	ECalendarTable *task_table;
	ESourceSelector *selector;
	ECalModel *model;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	e_shell_window_add_action_group (shell_window, "calendar");
	e_shell_window_add_action_group (shell_window, "calendar-filter");

	/* Cache these to avoid lots of awkward casting. */
	priv->cal_shell_backend = g_object_ref (shell_backend);
	priv->cal_shell_content = g_object_ref (shell_content);
	priv->cal_shell_sidebar = g_object_ref (shell_sidebar);

	cal_shell_content = E_CAL_SHELL_CONTENT (shell_content);
	model = e_cal_shell_content_get_model (cal_shell_content);
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	memo_table = e_cal_shell_content_get_memo_table (cal_shell_content);
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	cal_shell_sidebar = E_CAL_SHELL_SIDEBAR (shell_sidebar);
	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);
	date_navigator = e_cal_shell_sidebar_get_date_navigator (cal_shell_sidebar);

	/* Give GnomeCalendar a handle to the date navigator. */
	gnome_calendar_set_date_navigator (calendar, date_navigator);

	e_calendar_item_set_get_time_callback (
		date_navigator->calitem, (ECalendarItemGetTimeCallback)
		cal_shell_view_get_current_time, cal_shell_view, NULL);

	/* KILL-BONOBO FIXME -- Need to connect to the "user-created"
	 *                      signal for each ECalendarView. */

	g_signal_connect_swapped (
		calendar, "dates-shown-changed",
		G_CALLBACK (e_cal_shell_view_update_sidebar),
		cal_shell_view);

	g_signal_connect_swapped (
		model, "notify::timezone",
		G_CALLBACK (e_cal_shell_view_update_timezone),
		cal_shell_view);

	g_signal_connect_swapped (
		date_navigator, "scroll-event",
		G_CALLBACK (cal_shell_view_date_navigator_scroll_event_cb),
		cal_shell_view);

	g_signal_connect_swapped (
		date_navigator->calitem, "date-range-changed",
		G_CALLBACK (cal_shell_view_date_navigator_date_range_changed_cb),
		cal_shell_view);

	g_signal_connect_swapped (
		date_navigator->calitem, "selection-changed",
		G_CALLBACK (cal_shell_view_date_navigator_selection_changed_cb),
		cal_shell_view);

	g_signal_connect_swapped (
		selector, "popup-event",
		G_CALLBACK (cal_shell_view_selector_popup_event_cb),
		cal_shell_view);

	g_signal_connect_swapped (
		memo_table, "popup-event",
		G_CALLBACK (cal_shell_view_memopad_popup_event_cb),
		cal_shell_view);

	g_signal_connect_swapped (
		memo_table, "status-message",
		G_CALLBACK (e_cal_shell_view_memopad_set_status_message),
		cal_shell_view);

	g_signal_connect_swapped (
		task_table, "popup-event",
		G_CALLBACK (cal_shell_view_taskpad_popup_event_cb),
		cal_shell_view);

	g_signal_connect_swapped (
		task_table, "status-message",
		G_CALLBACK (e_cal_shell_view_taskpad_set_status_message),
		cal_shell_view);

	g_signal_connect_swapped (
		e_memo_table_get_table (memo_table), "selection-change",
		G_CALLBACK (e_cal_shell_view_memopad_actions_update),
		cal_shell_view);

	g_signal_connect_swapped (
		e_calendar_table_get_table (task_table), "selection-change",
		G_CALLBACK (e_cal_shell_view_taskpad_actions_update),
		cal_shell_view);

	e_categories_register_change_listener (
		G_CALLBACK (e_cal_shell_view_update_search_filter),
		cal_shell_view);

	e_cal_shell_view_actions_init (cal_shell_view);
	e_cal_shell_view_update_sidebar (cal_shell_view);
        e_cal_shell_view_update_search_filter (cal_shell_view);
	e_cal_shell_view_update_timezone (cal_shell_view);
}

void
e_cal_shell_view_private_dispose (ECalShellView *cal_shell_view)
{
	ECalShellViewPrivate *priv = cal_shell_view->priv;
	GList *iter;

	DISPOSE (priv->cal_shell_backend);
	DISPOSE (priv->cal_shell_content);
	DISPOSE (priv->cal_shell_sidebar);

	if (priv->calendar_activity != NULL) {
		/* XXX Activity is not cancellable. */
		e_activity_complete (priv->calendar_activity);
		g_object_unref (priv->calendar_activity);
		priv->calendar_activity = NULL;
	}

	if (priv->memopad_activity != NULL) {
		/* XXX Activity is not cancellable. */
		e_activity_complete (priv->memopad_activity);
		g_object_unref (priv->memopad_activity);
		priv->memopad_activity = NULL;
	}

	if (priv->taskpad_activity != NULL) {
		/* XXX Activity is not cancellable. */
		e_activity_complete (priv->taskpad_activity);
		g_object_unref (priv->taskpad_activity);
		priv->taskpad_activity = NULL;
	}
}

void
e_cal_shell_view_private_finalize (ECalShellView *cal_shell_view)
{
	/* XXX Nothing to do? */
}

void
e_cal_shell_view_execute_search (ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalShellSidebar *cal_shell_sidebar;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	GnomeCalendar *calendar;
	ECalendar *date_navigator;
	GtkRadioAction *action;
	GString *string;
	FilterRule *rule;
	const gchar *format;
	const gchar *text;
	time_t start_range;
	time_t end_range;
	gboolean range_search;
	gchar *start, *end;
	gchar *query;
	gchar *temp;
	gint value;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	text = e_shell_content_get_search_text (shell_content);

	shell_window = e_shell_view_get_shell_window (shell_view);
	action = GTK_RADIO_ACTION (ACTION (CALENDAR_SEARCH_ANY_FIELD_CONTAINS));
	value = gtk_radio_action_get_current_value (action);

	if (text == NULL || *text == '\0') {
		text = "";
		value = CALENDAR_SEARCH_SUMMARY_CONTAINS;
	}

	switch (value) {
		default:
			text = "";
			/* fall through */

		case CALENDAR_SEARCH_SUMMARY_CONTAINS:
			format = "(contains? \"summary\" %s)";
			break;

		case CALENDAR_SEARCH_DESCRIPTION_CONTAINS:
			format = "(contains? \"description\" %s)";
			break;

		case CALENDAR_SEARCH_ANY_FIELD_CONTAINS:
			format = "(contains? \"any\" %s)";
			break;
	}

	/* Build the query. */
	string = g_string_new ("");
	e_sexp_encode_string (string, text);
	query = g_strdup_printf (format, string->str);
	g_string_free (string, TRUE);

	range_search = FALSE;
	start_range = end_range = 0;

	/* Apply selected filter. */
	value = e_shell_content_get_filter_value (shell_content);
	switch (value) {
		case CALENDAR_FILTER_ANY_CATEGORY:
			break;

		case CALENDAR_FILTER_UNMATCHED:
			temp = g_strdup_printf (
				"(and (has-categories? #f) %s)", query);
			g_free (query);
			query = temp;
			break;

		case CALENDAR_FILTER_ACTIVE_APPOINTMENTS:
			/* Show a year's worth of appointments. */
			start_range = time (NULL);
			end_range = time_add_day (start_range, 365);
			start = isodate_from_time_t (start_range);
			end = isodate_from_time_t (end_range);

			temp = g_strdup_printf (
				"(and %s (occur-in-time-range? "
				"(make-time \"%s\") (make-time \"%s\")))",
				query, start, end);
			g_free (query);
			query = temp;

			range_search = TRUE;
			break;

		case CALENDAR_FILTER_NEXT_7_DAYS_APPOINTMENTS:
			start_range = time (NULL);
			end_range = time_add_day (start_range, 7);
			start = isodate_from_time_t (start_range);
			end = isodate_from_time_t (end_range);

			temp = g_strdup_printf (
				"(and %s (occur-in-time-range? "
				"(make-time \"%s\") (make-time \"%s\")))",
				query, start, end);
			g_free (query);
			query = temp;

			range_search = TRUE;
			break;

		default:
		{
			GList *categories;
			const gchar *category_name;

			categories = e_categories_get_list ();
			category_name = g_list_nth_data (categories, value);
			g_list_free (categories);

			temp = g_strdup_printf (
				"(and (has-categories? \"%s\") %s)",
				category_name, query);
			g_free (query);
			query = temp;
			break;
		}
	}

	/* XXX This is wrong.  We need to programmatically construct a
	 *     FilterRule, tell it to build code, and pass the resulting
	 *     expressing string to ECalModel. */
	rule = filter_rule_new ();
	e_shell_content_set_search_rule (shell_content, rule);
	g_object_unref (rule);

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	date_navigator = e_cal_shell_sidebar_get_date_navigator (cal_shell_sidebar);

	if (range_search) {
		/* Switch to list view and hide the date navigator. */
		action = GTK_RADIO_ACTION (ACTION (CALENDAR_VIEW_LIST));
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
		gtk_widget_hide (GTK_WIDGET (date_navigator));
	} else {
		/* Ensure the date navigator is visible. */
		gtk_widget_show (GTK_WIDGET (date_navigator));
	}

	/* Submit the query. */
	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	gnome_calendar_set_search_query (
		calendar, query, range_search, start_range, end_range);
	g_free (query);
}

void
e_cal_shell_view_open_event (ECalShellView *cal_shell_view,
                             ECalModelComponent *comp_data)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	CompEditor *editor;
	CompEditorFlags flags = 0;
	ECalComponent *comp;
	icalcomponent *clone;
	icalproperty *prop;
	const gchar *uid;

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view));
	g_return_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data));

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	uid = icalcomponent_get_uid (comp_data->icalcomp);
	editor = comp_editor_find_instance (uid);

	if (editor != NULL)
		goto exit;

	comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	e_cal_component_set_icalcomponent (comp, clone);

	prop = icalcomponent_get_first_property (
		comp_data->icalcomp, ICAL_ATTENDEE_PROPERTY);
	if (prop != NULL)
		flags |= COMP_EDITOR_MEETING;

	if (itip_organizer_is_user (comp, comp_data->client))
		flags |= COMP_EDITOR_USER_ORG;

	if (itip_sentby_is_user (comp, comp_data->client))
		flags |= COMP_EDITOR_USER_ORG;

	if (!e_cal_component_has_attendees (comp))
		flags |= COMP_EDITOR_USER_ORG;

	editor = event_editor_new (comp_data->client, shell, flags);
	comp_editor_edit_comp (editor, comp);

	g_object_ref (comp);

exit:
	gtk_window_present (GTK_WINDOW (editor));
}

void
e_cal_shell_view_set_status_message (ECalShellView *cal_shell_view,
                                     const gchar *status_message,
                                     gdouble percent)
{
	EActivity *activity;
	EShellView *shell_view;
	EShellBackend *shell_backend;

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view));

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	activity = cal_shell_view->priv->calendar_activity;

	if (status_message == NULL || *status_message == '\0') {
		if (activity != NULL) {
			e_activity_complete (activity);
			g_object_unref (activity);
			activity = NULL;
		}

	} else if (activity == NULL) {
		activity = e_activity_new (status_message);
		e_activity_set_percent (activity, percent);
		e_shell_backend_add_activity (shell_backend, activity);

	} else {
		e_activity_set_percent (activity, percent);
		e_activity_set_primary_text (activity, status_message);
	}

	cal_shell_view->priv->calendar_activity = activity;
}

void
e_cal_shell_view_update_sidebar (ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	GnomeCalendar *calendar;
	GnomeCalendarViewType view;
	time_t start_time, end_time;
	struct tm start_tm, end_tm;
	struct icaltimetype start_tt, end_tt;
	icaltimezone *timezone;
	gchar buffer[512];
	gchar end_buffer[512];

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view));

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	calendar = e_cal_shell_view_get_calendar (cal_shell_view);

	gnome_calendar_get_visible_time_range (
		calendar, &start_time, &end_time);
	timezone = gnome_calendar_get_timezone (calendar);
	view = gnome_calendar_get_view (calendar);

	start_tt = icaltime_from_timet_with_zone (start_time, FALSE, timezone);
	start_tm.tm_year = start_tt.year - 1900;
	start_tm.tm_mon = start_tt.month - 1;
	start_tm.tm_mday = start_tt.day;
	start_tm.tm_hour = start_tt.hour;
	start_tm.tm_min = start_tt.minute;
	start_tm.tm_sec = start_tt.second;
	start_tm.tm_isdst = -1;
	start_tm.tm_wday = time_day_of_week (
		start_tt.day, start_tt.month - 1, start_tt.year);

	/* Subtract one from end_time so we don't get an extra day. */
	end_tt = icaltime_from_timet_with_zone (end_time - 1, FALSE, timezone);
	end_tm.tm_year = end_tt.year - 1900;
	end_tm.tm_mon = end_tt.month - 1;
	end_tm.tm_mday = end_tt.day;
	end_tm.tm_hour = end_tt.hour;
	end_tm.tm_min = end_tt.minute;
	end_tm.tm_sec = end_tt.second;
	end_tm.tm_isdst = -1;
	end_tm.tm_wday = time_day_of_week (
		end_tt.day, end_tt.month - 1, end_tt.year);

	switch (view) {
		case GNOME_CAL_DAY_VIEW:
		case GNOME_CAL_WORK_WEEK_VIEW:
		case GNOME_CAL_WEEK_VIEW:
			if (start_tm.tm_year == end_tm.tm_year &&
				start_tm.tm_mon == end_tm.tm_mon &&
				start_tm.tm_mday == end_tm.tm_mday) {
				e_utf8_strftime (
					buffer, sizeof (buffer),
					_("%A %d %b %Y"), &start_tm);
			} else if (start_tm.tm_year == end_tm.tm_year) {
				e_utf8_strftime (
					buffer, sizeof (buffer),
					_("%a %d %b"), &start_tm);
				e_utf8_strftime (
					end_buffer, sizeof (end_buffer),
					_("%a %d %b %Y"), &end_tm);
				strcat (buffer, " - ");
				strcat (buffer, end_buffer);
			} else {
				e_utf8_strftime (
					buffer, sizeof (buffer),
					_("%a %d %b %Y"), &start_tm);
				e_utf8_strftime (
					end_buffer, sizeof (end_buffer),
					_("%a %d %b %Y"), &end_tm);
				strcat (buffer, " - ");
				strcat (buffer, end_buffer);
			}
			break;

		case GNOME_CAL_MONTH_VIEW:
		case GNOME_CAL_LIST_VIEW:
			if (start_tm.tm_year == end_tm.tm_year) {
				if (start_tm.tm_mon == end_tm.tm_mon) {
					e_utf8_strftime (
						buffer,
						sizeof (buffer),
						"%d", &start_tm);
					e_utf8_strftime (
						end_buffer,
						sizeof (end_buffer),
						_("%d %b %Y"), &end_tm);
					strcat (buffer, " - ");
					strcat (buffer, end_buffer);
				} else {
					e_utf8_strftime (
						buffer,
						sizeof (buffer),
						_("%d %b"), &start_tm);
					e_utf8_strftime (
						end_buffer,
						sizeof (end_buffer),
						_("%d %b %Y"), &end_tm);
					strcat (buffer, " - ");
					strcat (buffer, end_buffer);
				}
			} else {
				e_utf8_strftime (
					buffer, sizeof (buffer),
					_("%d %b %Y"), &start_tm);
				e_utf8_strftime (
					end_buffer, sizeof (end_buffer),
					_("%d %b %Y"), &end_tm);
				strcat (buffer, " - ");
				strcat (buffer, end_buffer);
			}
			break;

		default:
			g_return_if_reached ();
	}

	e_shell_sidebar_set_secondary_text (shell_sidebar, buffer);
}

void
e_cal_shell_view_update_timezone (ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalShellSidebar *cal_shell_sidebar;
	icaltimezone *timezone;
	ECalModel *model;
	GList *clients, *iter;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	model = e_cal_shell_content_get_model (cal_shell_content);
	timezone = e_cal_model_get_timezone (model);

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	clients = e_cal_shell_sidebar_get_clients (cal_shell_sidebar);

	for (iter = clients; iter != NULL; iter = iter->next) {
		ECal *client = iter->data;

		if (e_cal_get_load_state (client) == E_CAL_LOAD_LOADED)
			e_cal_set_default_timezone (client, timezone, NULL);
	}

	g_list_free (clients);
}
