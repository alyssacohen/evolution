/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Evolution calendar Free/Busy utilities and types
 *
 * Copyright (C) 2004 Ximian, Inc.
 *
 * Author: Gary Ekker <gekker@novell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <glib.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-source-list.h>
#include <libedataserverui/e-passwords.h>
#include <libecal/e-cal-time-util.h>
#include <libgnome/gnome-i18n.h>
#include "calendar-config.h"
#include "common/authentication.h"
#include "itip-utils.h"
#include "e-pub-utils.h"

static gboolean updated_last_pub_time = FALSE;

void
e_pub_uri_from_xml (EPublishUri *uri, const gchar *xml)
{
	xmlDocPtr doc;
	xmlNodePtr root, p;
	xmlChar *location, *enabled, *frequency;
	xmlChar *username, *publish_time;
	GSList *l = NULL;
	
	uri->location = NULL;
	doc = xmlParseDoc ((char *)xml);
	if (doc == NULL) {
		uri->location = NULL;
		return;
	}
	
	root = doc->children;
	if (strcmp (root->name, "uri") != 0) {
		return;
	}
	location = xmlGetProp (root, "location");
	enabled = xmlGetProp (root, "enabled");
	frequency = xmlGetProp (root, "frequency");
	username = xmlGetProp (root, "username");
	publish_time = xmlGetProp (root, "publish_time");
	
	if (location != NULL)
		uri->location = g_strdup (location);
	if (enabled != NULL)
		uri->enabled = atoi (enabled);
	if (frequency != NULL)
		uri->publish_freq = atoi (frequency);
	if (username != NULL)
		uri->username = g_strdup (username);
	if (publish_time != NULL)
		uri->last_pub_time = g_strdup (publish_time);
	
	uri->password = g_strdup ("");

	for (p = root->children; p != NULL; p = p->next) {
		xmlChar *uid = xmlGetProp (p, "uid");

		l = g_slist_append (l, uid);
	}	
	uri->calendars = l;
	
	xmlFree(location);
	xmlFree(enabled);
	xmlFreeDoc(doc);
	return;
}

gchar *
e_pub_uri_to_xml (EPublishUri *uri)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	gchar *enabled, *frequency;
	GSList *cals = NULL;
	xmlChar *xml_buffer;
	char *returned_buffer;
	int xml_buffer_size;

	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (uri->location != NULL, NULL);

	doc = xmlNewDoc ("1.0");

	root = xmlNewDocNode (doc, NULL, "uri", NULL);
	enabled = g_strdup_printf ("%d", uri->enabled);
	frequency = g_strdup_printf ("%d", uri->publish_freq);
	xmlSetProp (root, "location", uri->location);
	xmlSetProp (root, "enabled", enabled);
	xmlSetProp (root, "frequency", frequency);
	xmlSetProp (root, "username", uri->username);
	xmlSetProp (root, "publish_time", uri->last_pub_time);
	
	for (cals = uri->calendars; cals != NULL; cals = cals->next) {
		xmlNodePtr node;
		
		node = xmlNewChild (root, NULL, "source", NULL);
		xmlSetProp (node, "uid", cals->data);
	}
	xmlDocSetRootElement (doc, root);

	xmlDocDumpMemory (doc, &xml_buffer, &xml_buffer_size);
	xmlFreeDoc (doc);

	returned_buffer = g_malloc (xml_buffer_size + 1);
	memcpy (returned_buffer, xml_buffer, xml_buffer_size);
	returned_buffer [xml_buffer_size] = '\0';
	xmlFree (xml_buffer);
	g_free (enabled);

	return returned_buffer;
}

static gboolean 
is_publish_time (EPublishUri *uri) {
	icaltimezone *utc;
	struct icaltimetype current_itt, adjust_itt;

	utc = icaltimezone_get_utc_timezone ();
	current_itt = icaltime_current_time_with_zone (utc);		

	if (!uri->last_pub_time ||(strlen (uri->last_pub_time)== 0)) {
		uri->last_pub_time = g_strdup (icaltime_as_ical_string (current_itt));
		return TRUE;
		
	} else {
		struct icaltimetype adjust_itt = icaltime_from_string (uri->last_pub_time);
		
		switch (uri->publish_freq) {
			case URI_PUBLISH_DAILY:
				icaltime_adjust (&adjust_itt, 1, 0, 0, 0);
				if (icaltime_compare_date_only (adjust_itt, current_itt ) < 0) {
					uri->last_pub_time = g_strdup (icaltime_as_ical_string (current_itt));
					return TRUE;
				}
				break;
			case URI_PUBLISH_WEEKLY:
				icaltime_adjust (&adjust_itt, 7, 0, 0, 0);
				if (icaltime_compare_date_only (adjust_itt, current_itt ) < 0) {
					uri->last_pub_time = g_strdup (icaltime_as_ical_string (current_itt));
					return TRUE;
				}
				break;
		}
	}
	
	return FALSE;
}

/* FIXME we need to add a timeout function to check if the next publishing time is reached. 
   If the freqency is daily and evolution runs continuously for more than one day, the freebusy
   will only be published for the first day */
void
e_pub_publish (gboolean publish) {
	icaltimezone *utc;
	time_t start = time (NULL), end;
	GSList *uri_config_list, *l, *uri_list = NULL;
	ESourceList *source_list;
	GConfClient *gconf_client;
	gboolean published = FALSE;
	
	gconf_client = gconf_client_get_default ();
	source_list = e_source_list_new_for_gconf (gconf_client, "/apps/evolution/calendar/sources");
	
	utc = icaltimezone_get_utc_timezone ();	
	start = time_day_begin_with_zone (start, utc);
	end = time_add_week_with_zone (start, 6, utc);
		
	uri_config_list = calendar_config_get_free_busy ();

	for (l = uri_config_list; l != NULL; l = l->next) {
		GSList *p =NULL, *q;
		EPublishUri *uri;
		ECalComponent *clone = NULL;
		gboolean cloned = FALSE;
		ECal *client = NULL;
		char *prompt;
		gboolean remember = FALSE;
		gchar *password;

		gchar *xml = (gchar *)l->data;
		
		uri = g_new0 (EPublishUri, 1);		
		e_pub_uri_from_xml (uri, xml);
	
		/*FIXME this is just a hack to avoid publishing again and again 
		  ,we need to make the last publish time a seperate key */
		if (updated_last_pub_time) {
			updated_last_pub_time = FALSE;
			return;
		}	
		
		/* TODO: make sure we're online */
		/* skip this url if it isn't enabled or if it is manual */
		if (!uri->enabled) {
			uri_config_list = g_slist_next (uri_config_list);
			continue;
		}	
		
		if (!publish) {
			/* a g_idle publish, make sure we are not set to user only */
			if (uri->publish_freq == URI_PUBLISH_USER) {
				uri_config_list = g_slist_next (uri_config_list);
				continue;
			}
			
			/* If not is it time to publish again? */
			publish = is_publish_time (uri);
				
		}
		
		/* User published or config change */
		if (publish) {
			/* We still need to set the last_pub_time */
			uri->last_pub_time = 0;
			is_publish_time (uri);
			q = NULL;
			for (p = uri->calendars; p != NULL; p = p->next) {
				
				GList *comp_list = NULL;
				gchar *source_uid;
				ESource * source;
				char *email = NULL;
				GError *error = NULL;
				GList *users = NULL;
	
				source_uid = g_strdup (p->data);
				source =  e_source_list_peek_source_by_uid (source_list, source_uid);

				if (source)
					client = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_EVENT);

				if (!client) {
					g_warning (G_STRLOC ": Could not publish Free/Busy: Calendar backend no longer exists");
					g_free (source_uid);
					g_free (p->data);
					q = g_slist_append (q, p);

					continue;
				}
			
				if (!e_cal_open (client, TRUE, &error)) {
					g_warning ("Could not open the calendar %s \n", error->message);
					g_error_free (error);
					error = NULL;

					g_object_unref (client);
					client = NULL;
					g_free (source_uid);
					continue;
				}
				
				if (e_cal_get_cal_address (client, &email, &error)) {
					if (email && *email)	
						users = g_list_append (users, email);
				} else {
					g_warning ("Could not get the email: %s \n", error->message);
					g_error_free (error);
					error = NULL;
				}

				if (e_cal_get_free_busy ((ECal *) client, users,
							 start, end, 
							 &comp_list, &error)) {
					GList *l;
	
					for (l = comp_list; l; l = l->next) {
						ECalComponent *comp = E_CAL_COMPONENT (l->data);
					
						cloned = itip_publish_begin (comp, (ECal *) client, cloned, &clone);
						g_object_unref (comp);
					}
					g_list_free (comp_list);
				} else {
					g_warning ("Could not get the free busy information %s \n", error->message);
					g_error_free (error);
					error = NULL;
				}
				
				if (users) 
					g_list_free (users);

				g_free (email);
				g_object_unref (client);
				client = NULL;
				g_free (source_uid);
			} 
		}

		for(p = q; p!=NULL; p = p->next) {
			uri->calendars = g_slist_delete_link (uri->calendars, p->data);
		}
		g_slist_free (q);
		
		/* add password to the uri */
		password = e_passwords_get_password ("Calendar", 
				(gchar *)uri->location);

		if (!password) {
			prompt = g_strdup_printf (_("Enter the password for %s"), (gchar *)uri->location);
			password = e_passwords_ask_password (_("Enter password"), 
					"Calendar", (gchar *)uri->location, 
					prompt,
					E_PASSWORDS_REMEMBER_FOREVER|E_PASSWORDS_SECRET|E_PASSWORDS_ONLINE,
					&remember, NULL);

			g_free (prompt);

			if (!password) {
				g_slist_free (p);
				continue;
			}
		}

		g_slist_free (p);

		if (cloned && clone)
			published = itip_publish_comp ((ECal *) client,
					uri->location,
					uri->username, 
					password, &clone);

		xml = e_pub_uri_to_xml (uri);
		if (xml != NULL) {
			uri_list = g_slist_append (uri_list, xml);
		}
		g_free (uri);
	}
	
	if (published) {
		/* Update gconf so we have the last_pub_time */
		calendar_config_set_free_busy (uri_list);
		updated_last_pub_time = TRUE;
	}
		
	g_slist_free (uri_config_list);
	g_slist_free (uri_list);
}
