/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-cal-shell-module.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <string.h>
#include <glib/gi18n.h>
#include <libecal/e-cal.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-source-list.h>
#include <libedataserver/e-source-group.h>

#include "shell/e-shell.h"
#include "shell/e-shell-module.h"
#include "shell/e-shell-window.h"

#include "calendar/gui/calendar-config.h"

#include "e-cal-shell-view.h"

#define MODULE_NAME		"calendar"
#define MODULE_ALIASES		""
#define MODULE_SCHEMES		"calendar"
#define MODULE_SEARCHES		"caltypes.xml"
#define MODULE_SORT_ORDER	400

#define CONTACTS_BASE_URI	"contacts://"
#define WEATHER_BASE_URI	"weather://"
#define WEB_BASE_URI		"webcal://"
#define PERSONAL_RELATIVE_URI	"system"

/* Module Entry Point */
void e_shell_module_init (GTypeModule *type_module);

static void
cal_module_ensure_sources (EShellModule *shell_module)
{
	/* XXX This is basically the same algorithm across all modules.
	 *     Maybe we could somehow integrate this into EShellModule? */

	ESourceList *source_list;
	ESourceGroup *on_this_computer;
	ESourceGroup *on_the_web;
	ESourceGroup *contacts;
	ESourceGroup *weather;
	ESource *birthdays;
	ESource *personal;
	GSList *groups, *iter;
	const gchar *data_dir;
	gchar *base_uri;
	gchar *filename;
	gchar *property;

	on_this_computer = NULL;
	on_the_web = NULL;
	contacts = NULL;
	weather = NULL;
	birthdays = NULL;
	personal = NULL;

	if (!e_cal_get_sources (&source_list, E_CAL_SOURCE_TYPE_EVENT, NULL)) {
		g_warning ("Could not get calendar sources from GConf!");
		return;
	}

	/* Share the source list with all calendar views.  This
	 * is accessible via e_cal_shell_view_get_source_list().
	 * Note: EShellModule takes ownership of the reference.
	 *
	 * XXX I haven't yet decided if I want to add a proper
	 *     EShellModule API for this.  The mail module would
	 *     not use it. */
	g_object_set_data_full (
		G_OBJECT (shell_module), "source-list",
		source_list, (GDestroyNotify) g_object_unref);

	data_dir = e_shell_module_get_data_dir (shell_module);
	filename = g_build_filename (data_dir, "local", NULL);
	base_uri = g_filename_to_uri (filename, NULL, NULL);
	g_free (filename);

	groups = e_source_list_peek_groups (source_list);
	for (iter = groups; iter != NULL; iter = iter->next) {
		ESourceGroup *source_group = iter->data;
		const gchar *group_base_uri;

		group_base_uri = e_source_group_peek_base_uri (source_group);

		/* Compare only "file://" part.  if the user's home
		 * changes, we do not want to create another group. */
		if (on_this_computer == NULL &&
			strncmp (base_uri, group_base_uri, 7) == 0)
			on_this_computer = source_group;

		else if (on_the_web == NULL &&
			strcmp (WEB_BASE_URI, group_base_uri) == 0)
			on_the_web = source_group;

		else if (contacts == NULL &&
			strcmp (CONTACTS_BASE_URI, group_base_uri) == 0)
			contacts = source_group;

		else if (weather == NULL &&
			strcmp (WEATHER_BASE_URI, group_base_uri) == 0)
			weather = source_group;
	}

	if (on_this_computer != NULL) {
		GSList *sources;
		const gchar *group_base_uri;

		sources = e_source_group_peek_sources (on_this_computer);
		group_base_uri = e_source_group_peek_base_uri (on_this_computer);

		/* Make sure this group includes a "Personal" source. */
		for (iter = sources; iter != NULL; iter = iter->next) {
			ESource *source = iter->data;
			const gchar *relative_uri;

			relative_uri = e_source_peek_relative_uri (source);
			if (relative_uri == NULL)
				continue;

			if (strcmp (PERSONAL_RELATIVE_URI, relative_uri) != 0)
				continue;

			personal = source;
			break;
		}

		/* Make sure we have the correct base URI.  This can
		 * change when the user's home directory changes. */
		if (strcmp (base_uri, group_base_uri) != 0) {
			e_source_group_set_base_uri (
				on_this_computer, base_uri);

			/* XXX We shouldn't need this sync call here as
			 *     set_base_uri() results in synching to GConf,
			 *     but that happens in an idle loop and too late
			 *     to prevent the user from seeing a "Cannot
			 *     Open ... because of invalid URI" error. */
			e_source_list_sync (source_list, NULL);
		}

	} else {
		ESourceGroup *source_group;
		const gchar *name;

		name = _("On This Computer");
		source_group = e_source_group_new (name, base_uri);
		e_source_list_add_group (source_list, source_group, -1);
		g_object_unref (source_group);
	}

	if (personal == NULL) {
		ESource *source;
		GSList *selected;
		const gchar *name;
		gchar *primary;

		name = _("Personal");
		source = e_source_new (name, PERSONAL_RELATIVE_URI);
		e_source_group_add_source (on_this_computer, source, -1);
		g_object_unref (source);

		primary = calendar_config_get_primary_calendar ();
		selected = calendar_config_get_calendars_selected ();

		if (primary == NULL && selected == NULL) {
			const gchar *uid;

			uid = e_source_peek_uid (source);
			selected = g_slist_prepend (NULL, g_strdup (uid));

			calendar_config_set_primary_calendar (uid);
			calendar_config_set_calendars_selected (selected);
		}

		g_slist_foreach (selected, (GFunc) g_free, NULL);
		g_slist_free (selected);
		g_free (primary);
	}

	if (on_the_web == NULL) {
		ESourceGroup *source_group;
		const gchar *name;

		name = _("On The Web");
		source_group = e_source_group_new (name, WEB_BASE_URI);
		e_source_list_add_group (source_list, source_group, -1);
		g_object_unref (source_group);
	}

	if (contacts != NULL) {
		GSList *sources;

		sources = e_source_group_peek_sources (contacts);

		if (sources != NULL) {
			GSList *trash;

			/* There is only one source under Contacts. */
			birthdays = E_SOURCE (sources->data);
			sources = g_slist_next (sources);

			/* Delete any other sources in this group.
			 * Earlier versions allowed you to create
			 * additional sources under Contacts. */
			trash = g_slist_copy (sources);
			while (trash != NULL) {
				ESource *source = trash->data;
				e_source_group_remove_source (contacts, source);
				trash = g_slist_delete_link (trash, trash);
			}
				
		}
	} else {
		ESourceGroup *source_group;
		const gchar *name;

		name = _("Contacts");
		source_group = e_source_group_new (name, CONTACTS_BASE_URI);
		e_source_list_add_group (source_list, source_group, -1);
		g_object_unref (source_group);

		/* This is now a borrowed reference. */
		contacts = source_group;
	}

	/* XXX e_source_group_get_property() returns a newly-allocated
	 *     string when it could just as easily return a const string.
	 *     Unfortunately, fixing that would break the API. */
	property = e_source_group_get_property (contacts, "create_source");
	if (property == NULL)
		e_source_group_set_property (contacts, "create_source", "no");
	g_free (property);

	if (birthdays == NULL) {
		ESource *source;
		const gchar *name;

		name = _("Birthdays & Anniversaries");
		source = e_source_new (name, "/");
		e_source_group_add_source (contacts, source, -1);
		g_object_unref (source);

		/* This is now a borrowed reference. */
		birthdays = source;
	}

	if (e_source_get_property (birthdays, "delete") == NULL)
		e_source_set_property (birthdays, "delete", "no");

	if (e_source_peek_color_spec (birthdays) == NULL)
		e_source_set_color_spec (birthdays, "#DDBECE");

	if (weather == NULL) {
		ESourceGroup *source_group;
		const gchar *name;

		name = _("Weather");
		source_group = e_source_group_new (name, WEATHER_BASE_URI);
		e_source_list_add_group (source_list, source_group, -1);
		g_object_unref (source_group);
	}

	g_free (base_uri);
}

static void
action_appointment_new_cb (GtkAction *action,
                           EShellWindow *shell_window)
{
}

static void
action_appointment_all_day_new_cb (GtkAction *action,
                                   EShellWindow *shell_window)
{
}

static void
action_meeting_new_cb (GtkAction *action,
                       EShellWindow *shell_window)
{
}

static void
action_calendar_new_cb (GtkAction *action,
                        EShellWindow *shell_window)
{
}

static GtkActionEntry item_entries[] = {

	{ "appointment-new",
	  "appointment-new",
	  N_("_Appointment"),  /* XXX Need C_() here */
	  "<Control>a",
	  N_("Create a new appointment"),
	  G_CALLBACK (action_appointment_new_cb) },

	{ "appointment-all-day-new",
	  "stock_new-24h-appointment",
	  N_("All Day A_ppointment"),
	  NULL,
	  N_("Create a new all-day appointment"),
	  G_CALLBACK (action_appointment_all_day_new_cb) },

	{ "meeting-new",
	  "stock_new-meeting",
	  N_("M_eeting"),
	  "<Control>e",
	  N_("Create a new meeting request"),
	  G_CALLBACK (action_meeting_new_cb) }
};

static GtkActionEntry source_entries[] = {

	{ "calendar-new",
	  "x-office-calendar",
	  N_("Cale_ndar"),
	  NULL,
	  N_("Create a new calendar"),
	  G_CALLBACK (action_calendar_new_cb) }
};

static gboolean
cal_module_handle_uri (EShellModule *shell_module,
                       const gchar *uri)
{
	/* FIXME */
	return FALSE;
}

static void
cal_module_window_created (EShellModule *shell_module,
                           EShellWindow *shell_window)
{
	const gchar *module_name;

	module_name = G_TYPE_MODULE (shell_module)->name;

	e_shell_window_register_new_item_actions (
		shell_window, module_name,
		item_entries, G_N_ELEMENTS (item_entries));

	e_shell_window_register_new_source_actions (
		shell_window, module_name,
		source_entries, G_N_ELEMENTS (source_entries));
}

static EShellModuleInfo module_info = {

	MODULE_NAME,
	MODULE_ALIASES,
	MODULE_SCHEMES,
	MODULE_SEARCHES,
	MODULE_SORT_ORDER
};

void
e_shell_module_init (GTypeModule *type_module)
{
	EShell *shell;
	EShellModule *shell_module;

	shell_module = E_SHELL_MODULE (type_module);
	shell = e_shell_module_get_shell (shell_module);

	/* Register the GType for ECalShellView. */
	e_cal_shell_view_get_type (type_module);

	e_shell_module_set_info (shell_module, &module_info);

	cal_module_ensure_sources (shell_module);

	g_signal_connect_swapped (
		shell, "handle-uri",
		G_CALLBACK (cal_module_handle_uri), shell_module);

	g_signal_connect_swapped (
		shell, "window-created",
		G_CALLBACK (cal_module_window_created), shell_module);
}
