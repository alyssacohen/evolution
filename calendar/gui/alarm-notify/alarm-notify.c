/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- *
 *  Evolution calendar - Alarm notification service object
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <bonobo/bonobo-main.h>
#include <libedataserver/e-url.h>
#include <libedataserverui/e-passwords.h>
#include <libecal/e-cal.h>
#include "alarm-notify.h"
#include "alarm-queue.h"
#include "config-data.h"
#include "common/authentication.h"

/* Private part of the AlarmNotify structure */
struct _AlarmNotifyPrivate {
	/* Mapping from EUri's to LoadedClient structures */
	/* FIXME do we need per source type uri hashes? or perhaps we
	   just need to hash based on source */
	GHashTable *uri_client_hash [E_CAL_SOURCE_TYPE_LAST];
        ESourceList *source_lists [E_CAL_SOURCE_TYPE_LAST];	
	ESourceList *selected_calendars;
        GMutex *mutex;
};

EThread *alarm_operation_thread;	/* for operations that can (or should) be queued */
EMsgPort *alarm_reply_port;
static GIOChannel *alarm_reply_channel;

#define d(x) x


static void alarm_notify_class_init (AlarmNotifyClass *klass);
static void alarm_notify_init (AlarmNotify *an, AlarmNotifyClass *klass);
static void alarm_notify_finalize (GObject *object);


static BonoboObjectClass *parent_class;


BONOBO_TYPE_FUNC_FULL(AlarmNotify, GNOME_Evolution_Calendar_AlarmNotify, BONOBO_TYPE_OBJECT, alarm_notify)

/* Class initialization function for the alarm notify service */
static void
alarm_notify_class_init (AlarmNotifyClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = alarm_notify_finalize;
}

typedef struct {
	AlarmNotify *an;
	ESourceList *source_list;
	GList *removals;
} ProcessRemovalsData;

static void
process_removal_in_hash (gpointer key, gpointer value, gpointer data)
{
	char *uri = key;
	ProcessRemovalsData *prd = data;
	GSList *groups, *sources, *p, *q;
	gboolean found = FALSE;
	
	/* search the list of selected calendars */
	groups = e_source_list_peek_groups (prd->source_list);
	for (p = groups; p != NULL; p = p->next) {
		ESourceGroup *group = E_SOURCE_GROUP (p->data);
		
		sources = e_source_group_peek_sources (group);
		for (q = sources; q != NULL; q = q->next) {
			ESource *source = E_SOURCE (q->data);
			char *source_uri;
			const char *completion = e_source_get_property (source, "alarm");
			
			source_uri = e_source_get_uri (source);
			if (strcmp (source_uri, uri) == 0)
				if (!completion || !g_ascii_strcasecmp (completion, "true"))
					found = TRUE;
			
			g_free (source_uri);

			if (found)
				return;
		}
	}

	/* not found, so list it for removal */
	prd->removals = g_list_prepend (prd->removals, uri);
}

static void
list_changed_cb (ESourceList *source_list, gpointer data)
{
	AlarmNotify *an = data;
	AlarmNotifyPrivate *priv;
	GSList *groups, *sources, *p, *q;
	ECalSourceType source_type = E_CAL_SOURCE_TYPE_LAST;
	ProcessRemovalsData prd;
	GList *l;
	int i;

	g_signal_handlers_block_by_func(source_list, list_changed_cb, data);
	
	priv = an->priv;

	/* Figure out the source type */
	for (i = 0; i < E_CAL_SOURCE_TYPE_LAST; i++) {
		if (source_list == priv->source_lists[i]) {
			source_type = i;
			break;
		}
	}
	if (source_type == E_CAL_SOURCE_TYPE_LAST)
		return;
	
	/* process the additions */
	groups = e_source_list_peek_groups (source_list);
	for (p = groups; p != NULL; p = p->next) {
		ESourceGroup *group = E_SOURCE_GROUP (p->data);
		
		sources = e_source_group_peek_sources (group);
		for (q = sources; q != NULL; q = q->next) {
			ESource *source = E_SOURCE (q->data);
			char *uri;
			const char *completion = e_source_get_property (source, "alarm");
			
			if (completion  && !g_ascii_strcasecmp (completion, "false")) 
				continue;
			
			uri = e_source_get_uri (source);
			if (!g_hash_table_lookup (priv->uri_client_hash[source_type], uri)) {
				d (printf("%s:%d (list_changed_cb) - Adding Calendar %s\n", __FILE__, __LINE__, uri));				
				alarm_notify_add_calendar (an, source_type, source, FALSE);
			}
			g_free (uri);
		}
	}

	/* process the removals */
	prd.an = an;
	prd.source_list = priv->source_lists[source_type];
	prd.removals = NULL;
	g_hash_table_foreach (priv->uri_client_hash[source_type], (GHFunc) process_removal_in_hash, &prd);

	for (l = prd.removals; l; l = l->next) {
		d (printf("%s:%d (list_changed_cb) - Removing Calendar %s\n", __FILE__, __LINE__, l->data));		
		alarm_notify_remove_calendar (an, source_type, l->data);
	}
	g_list_free (prd.removals);
	g_signal_handlers_unblock_by_func(source_list, list_changed_cb, data);
	
}

ESourceList *
alarm_notify_get_selected_calendars (AlarmNotify *an)
{
	return an->priv->selected_calendars;
}

static void
load_calendars (AlarmNotify *an, ECalSourceType source_type)
{
	AlarmNotifyPrivate *priv;
	ESourceList *source_list;
	GSList *groups, *sources, *p, *q;

	priv = an->priv;
	
	if (!e_cal_get_sources (&source_list, source_type, NULL)) {
		d (printf("%s:%d (load_calendars) - Cannont get sources\n ", __FILE__, __LINE__));
		priv->source_lists[source_type] = NULL;

		return;
	}

	groups = e_source_list_peek_groups (source_list);
	for (p = groups; p != NULL; p = p->next) {
		ESourceGroup *group = E_SOURCE_GROUP (p->data);
		
		sources = e_source_group_peek_sources (group);
		for (q = sources; q != NULL; q = q->next) {
			ESource *source = E_SOURCE (q->data);
			char *uri;
			const char *completion = e_source_get_property (source, "alarm");
			
			if (completion  && !g_ascii_strcasecmp (completion, "false")) 
				continue;
			
			uri = e_source_get_uri (source);
			d (printf("%s:%d (load_calendars) - Loading Calendar %s \n", __FILE__, __LINE__, uri));			
			alarm_notify_add_calendar (an, source_type, source, FALSE);
			g_free (uri);
			
		}
	}

	g_signal_connect_object (source_list, "changed", G_CALLBACK (list_changed_cb), an, 0);
	priv->source_lists[source_type] = source_list;
}

/* Object initialization function for the alarm notify system */
static void
alarm_notify_init (AlarmNotify *an, AlarmNotifyClass *klass)
{
	AlarmNotifyPrivate *priv;
	int i;

	priv = g_new0 (AlarmNotifyPrivate, 1);
	an->priv = priv;
	priv->mutex = g_mutex_new ();
	priv->selected_calendars = config_data_get_calendars ("/apps/evolution/calendar/sources");

	d (printf("%s:%d (alarm_notify_init) - Initing Alarm Notify\n", __FILE__, __LINE__));
	
	for (i = 0; i < E_CAL_SOURCE_TYPE_LAST; i++)
		priv->uri_client_hash[i] = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

	alarm_queue_init (an);

	for (i = 0; i < E_CAL_SOURCE_TYPE_LAST; i++)
		load_calendars (an, i);
}

static void
dequeue_client (gpointer key, gpointer value, gpointer user_data)
{
	ECal *client = value;

	d (printf("%s:%d (dequeue_client) - Removing client %d\n ", __FILE__, __LINE__, client));
	alarm_queue_remove_client (client);
}

/* Finalize handler for the alarm notify system */
static void
alarm_notify_finalize (GObject *object)
{
	AlarmNotify *an;
	AlarmNotifyPrivate *priv;
	int i;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ALARM_NOTIFY (object));
	
	d (printf("%s:%d (alarm_notify_finalize) - Finalize \n ", __FILE__, __LINE__));
	
	an = ALARM_NOTIFY (object);
	priv = an->priv;

	for (i = 0; i < E_CAL_SOURCE_TYPE_LAST; i++) {
		g_hash_table_foreach (priv->uri_client_hash[i], dequeue_client, NULL);
		g_hash_table_destroy (priv->uri_client_hash[i]);
	}

	alarm_queue_done ();

	g_mutex_free (priv->mutex);
	g_free (priv);

	e_thread_destroy(alarm_operation_thread);
	g_io_channel_unref(alarm_reply_channel);
	e_msgport_destroy(alarm_reply_port);
	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}



static guint
alarm_channel_setup(EMsgPort **port, GIOChannel **channel, GIOFunc func)
{
	GSource *source;
	guint id;

	d (printf("%s:%d (alarm_channel_setup) - Channel Setup\n ", __FILE__, __LINE__));
	*port = e_msgport_new();
#ifndef G_OS_WIN32
	*channel = g_io_channel_unix_new(e_msgport_fd(*port));
#else
	*channel = g_io_channel_win32_new_socket(e_msgport_fd(*port));
#endif
	source = g_io_create_watch(*channel, G_IO_IN);
	g_source_set_callback(source, (GSourceFunc)func, *port, NULL);
	g_source_set_can_recurse(source, FALSE);
	id = g_source_attach(source, NULL);
	g_source_unref(source);

	return id;
}

static void
alarm_msg_destroy(EThread *e, EMsg *msg, void *data)
{
	AlarmMsg *m = (AlarmMsg *)msg;

	/* Free the private */
	g_free (m->data); /* Mostly it is a structure allocated as a carrier*/
	g_free (m);
}

static gboolean
alarm_msgport_replied(GIOChannel *source, GIOCondition cond, void *d)
{
	EMsgPort *port = (EMsgPort *)d;
	AlarmMsg *m;

	while (( m = (AlarmMsg *)e_msgport_get(port))) {
		d (printf("%s:%d (alarm_msgport_replied) - %p: Replied to GUI thread\n", __FILE__, __LINE__, m));
		alarm_msg_destroy(NULL, m, NULL);
	}

	return TRUE;
}

static void
alarm_msg_received(EThread *e, EMsg *msg, void *data)
{
	AlarmMsg *m = (AlarmMsg *)msg;
	
	d(printf("%s:%d (alarm_msg_received) - %p: Received at thread %" G_GINT64_MODIFIER "x\n", __FILE__, __LINE__, m, e_util_pthread_id(pthread_self())));	
	if (m->receive_msg) {
		m->receive_msg (e, m, data);
	}
}

/**
 * alarm_notify_new:
 * 
 * Creates a new #AlarmNotify object.
 * 
 * Return value: A newly-created #AlarmNotify, or NULL if its corresponding
 * CORBA object could not be created.
 **/
AlarmNotify *
alarm_notify_new (void)
{
	AlarmNotify *an;
	
	d (printf("%s:%d (alarm_notify_new) - Alarm Notify New \n ", __FILE__, __LINE__));

	/* Create a thread for alarm queue operation*/	
	alarm_channel_setup(&alarm_reply_port, &alarm_reply_channel, alarm_msgport_replied);

	alarm_operation_thread = e_thread_new(E_THREAD_QUEUE);
	e_thread_set_msg_destroy(alarm_operation_thread, alarm_msg_destroy, 0);
	e_thread_set_msg_received(alarm_operation_thread, alarm_msg_received, 0);
	e_thread_set_reply_port(alarm_operation_thread, alarm_reply_port);

	an = g_object_new (TYPE_ALARM_NOTIFY,
			   "poa", bonobo_poa_get_threaded (ORBIT_THREAD_HINT_PER_REQUEST, NULL),
			   NULL);
	
	return an;
}

static void
cal_opened_cb (ECal *client, ECalendarStatus status, gpointer user_data)
{
	AlarmNotifyPrivate *priv;
	AlarmNotify *an = ALARM_NOTIFY (user_data);

	priv = an->priv;
	
	d (printf("%s:%d (cal_opened_cb) - Calendar Status %d\n", __FILE__, __LINE__, status==E_CALENDAR_STATUS_OK));
	
	if (status == E_CALENDAR_STATUS_OK)
		alarm_queue_add_client (client);
	else {
		g_hash_table_remove (priv->uri_client_hash[e_cal_get_source_type (client)],
				     e_cal_get_uri (client));
		g_signal_handlers_disconnect_matched (G_OBJECT (client), G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, an);
	}
}

/**
 * alarm_notify_add_calendar:
 * @an: An alarm notification service.
 * @uri: URI of the calendar to load.
 * @load_afterwards: Whether this calendar should be loaded in the future
 * when the alarm daemon starts up.
 * 
 * Tells the alarm notification service to load a calendar and start monitoring
 * its alarms.  It can optionally be made to save the URI of this calendar so
 * that it can be loaded in the future when the alarm daemon starts up.
 **/
void
alarm_notify_add_calendar (AlarmNotify *an, ECalSourceType source_type,  ESource *source, gboolean load_afterwards)
{
	AlarmNotifyPrivate *priv;
	ECal *client;
	EUri *e_uri;
	char *str_uri;
	char *pass_key;
	g_return_if_fail (an != NULL);
	g_return_if_fail (IS_ALARM_NOTIFY (an));

	/* Make sure the key used in for getting password is properly generated for all types of backends */
	priv = an->priv;
	str_uri = e_source_get_uri (source);
	e_uri = e_uri_new (str_uri);
	pass_key = e_uri_to_string (e_uri, FALSE);
	e_uri_free (e_uri);
	
	g_mutex_lock (an->priv->mutex);
	/* See if we already know about this uri */
	if (g_hash_table_lookup (priv->uri_client_hash[source_type], str_uri)) {
		g_mutex_unlock (an->priv->mutex);
		g_free (str_uri);
		g_free (pass_key);
		return;
	}
	/* if loading of this requires password and password is not currently availble in e-password
	   session skip this source loading. we do not really want to prompt for auth from alarm dameon*/

	if ((e_source_get_property (source, "auth") && 
	     (!e_passwords_get_password (e_source_get_property(source, "auth-domain"), pass_key)))) {
		g_mutex_unlock (an->priv->mutex);
		g_free (str_uri);
		g_free (pass_key);
		return;
	}
	
	client = auth_new_cal_from_source (source, source_type);

	if (client) {
		d (printf("%s:%d (alarm_notify_add_calendar) - Calendar Open Async... %d\n", __FILE__, __LINE__, client));	
		g_hash_table_insert (priv->uri_client_hash[source_type], g_strdup (str_uri), client);
		g_signal_connect (G_OBJECT (client), "cal_opened", G_CALLBACK (cal_opened_cb), an);
		e_cal_open_async (client, FALSE);
	}
	g_free (str_uri);
	g_free (pass_key);
	g_mutex_unlock (an->priv->mutex);
}

void
alarm_notify_remove_calendar (AlarmNotify *an, ECalSourceType source_type, const char *str_uri)
{
	AlarmNotifyPrivate *priv;
	ECal *client;
	
	priv = an->priv;

	client = g_hash_table_lookup (priv->uri_client_hash[source_type], str_uri);
	if (client) {
		d (printf("%s:%d (alarm_notify_remove_calendar) - Removing Client %d\n", __FILE__, __LINE__, client));
		alarm_queue_remove_client (client);
		g_hash_table_remove (priv->uri_client_hash[source_type], str_uri);
	}
}
