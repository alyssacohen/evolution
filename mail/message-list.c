/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * message-list.c: Displays the messages.
 *                 Implements CORBA's Evolution::MessageList
 *
 * Author:
 *     Miguel de Icaza (miguel@helixcode.com)
 *     Bertrand Guiheneuf (bg@aful.org)
 *     And just about everyone else in evolution ...
 *
 * (C) 2000 Helix Code, Inc.
 */

#include <config.h>
#include <gnome.h>
#include <camel/camel-exception.h>
#include <camel/camel-folder.h>
#include <e-util/ename/e-name-western.h>
#include <camel/camel-folder-thread.h>
#include <e-util/e-memory.h>

#include <string.h>
#include <ctype.h>

#include "mail-config.h"
#include "message-list.h"
#include "mail-threads.h"
#include "mail-tools.h"
#include "mail-ops.h"
#include "Mail.h"

#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>
#include <gal/e-table/e-table-header-item.h>
#include <gal/e-table/e-table-item.h>

#include "art/mail-new.xpm"
#include "art/mail-read.xpm"
#include "art/mail-replied.xpm"
#include "art/attachment.xpm"
#include "art/priority-high.xpm"
#include "art/empty.xpm"
#include "art/score-lowest.xpm"
#include "art/score-lower.xpm"
#include "art/score-low.xpm"
#include "art/score-normal.xpm"
#include "art/score-high.xpm"
#include "art/score-higher.xpm"
#include "art/score-highest.xpm"

#define TIMEIT

#ifdef TIMEIT
#include <sys/time.h>
#include <unistd.h>
#endif

#define d(x)
#define t(x)

/*
 * Default sizes for the ETable display
 *
 */
#define N_CHARS(x) (CHAR_WIDTH * (x))

#define COL_ICON_WIDTH         (16)
#define COL_ATTACH_WIDTH       (16)
#define COL_CHECK_BOX_WIDTH    (16)
#define COL_FROM_EXPANSION     (24.0)
#define COL_FROM_WIDTH_MIN     (32)
#define COL_SUBJECT_EXPANSION  (30.0)
#define COL_SUBJECT_WIDTH_MIN  (32)
#define COL_SENT_EXPANSION     (24.0)
#define COL_SENT_WIDTH_MIN     (32)
#define COL_RECEIVED_EXPANSION (20.0)
#define COL_RECEIVED_WIDTH_MIN (32)
#define COL_TO_EXPANSION       (24.0)
#define COL_TO_WIDTH_MIN       (32)
#define COL_SIZE_EXPANSION     (6.0)
#define COL_SIZE_WIDTH_MIN     (32)

#define PARENT_TYPE (e_table_scrolled_get_type ())

struct _EMailAddress {
	ENameWestern *wname;
	gchar *address;
};

typedef struct _EMailAddress EMailAddress;

static ETableScrolledClass *message_list_parent_class;

static void on_cursor_change_cmd (ETableScrolled *table, int row, gpointer user_data);
static gint on_click (ETableScrolled *table, gint row, gint col, GdkEvent *event, MessageList *list);
static char *filter_date (const void *data);
static char *filter_size (const void *data);

static void save_tree_state(MessageList *ml);

static void folder_changed (CamelObject *o, gpointer event_data, gpointer user_data);
static void message_changed (CamelObject *o, gpointer event_data, gpointer user_data);

static void hide_save_state(MessageList *ml);
static void hide_load_state(MessageList *ml);

/* note: @changes is owned/freed by the caller */
static void mail_do_regenerate_messagelist (MessageList *list, const char *search, const char *hideexpr, CamelFolderChangeInfo *changes);

/* macros for working with id's (stored in the tree nodes) */
#define id_is_uid(id) (id[0] == 'u')/* is this a uid id? */
#define id_is_subject(id) (id[0] == 's') /* is this a subject id? */
#define id_uid(id) (&id[1])	/* get the uid part of the id */
#define id_subject(id) (&id[1])	/* get the subject part of the id */

enum {
	MESSAGE_SELECTED,
	LAST_SIGNAL
};

static guint message_list_signals [LAST_SIGNAL] = {0, };

static struct {
	char **image_base;
	GdkPixbuf  *pixbuf;
} states_pixmaps [] = {
	{ mail_new_xpm,		NULL },
	{ mail_read_xpm,	NULL },
	{ mail_replied_xpm,	NULL },
	{ empty_xpm,		NULL },
	{ attachment_xpm,	NULL },
	{ priority_high_xpm,	NULL },
	{ score_lowest_xpm,     NULL },
	{ score_lower_xpm,      NULL },
	{ score_low_xpm,        NULL },
	{ score_normal_xpm,     NULL },
	{ score_high_xpm,       NULL },
	{ score_higher_xpm,     NULL },
	{ score_highest_xpm,    NULL },
	{ NULL,			NULL }
};

enum DndTargetTyhpe {
	DND_TARGET_LIST_TYPE_URI,
};
#define URI_LIST_TYPE "text/uri-list"
static GtkTargetEntry drag_types[] = {
	{ URI_LIST_TYPE, 0, DND_TARGET_LIST_TYPE_URI },
};
static const int num_drag_types = sizeof (drag_types) / sizeof (drag_types[0]);

static EMailAddress *
e_mail_address_new (const char *address)
{
	CamelInternetAddress *cia;
	EMailAddress *new;
	const char *name = NULL, *addr = NULL;
	
	cia = camel_internet_address_new ();
	if (camel_address_unformat (CAMEL_ADDRESS (cia), address) == -1) {
		camel_object_unref (CAMEL_OBJECT (cia));
		return NULL;
	}
	camel_internet_address_get (cia, 0, &name, &addr);
	
	new = g_new (EMailAddress, 1);
	new->address = g_strdup (addr);
	if (name && *name) {
		new->wname = e_name_western_parse (name);
	} else {
		new->wname = NULL;
	}
	
	camel_object_unref (CAMEL_OBJECT (cia));
	
	return new;
}

static void
e_mail_address_free (EMailAddress *addr)
{
	g_return_if_fail (addr != NULL);
	
	g_free (addr->address);
	if (addr->wname)
		e_name_western_free (addr->wname);
	g_free (addr);
}

static gint
e_mail_address_compare (gconstpointer address1, gconstpointer address2)
{
	const EMailAddress *addr1 = address1;
	const EMailAddress *addr2 = address2;
	gint retval;
	
	g_return_val_if_fail (addr1 != NULL, 1);
	g_return_val_if_fail (addr2 != NULL, -1);
	
	if (!addr1->wname || !addr2->wname) {
		/* have to compare addresses, one or both don't have names */
		g_return_val_if_fail (addr1->address != NULL, 1);
		g_return_val_if_fail (addr2->address != NULL, -1);
		
		retval = g_strcasecmp (addr1->address, addr2->address);
	} else {
		if (!addr1->wname->last && !addr2->wname->last) {
			/* neither has a last name - default to address? */
			/* FIXME: what do we compare next? */
			g_return_val_if_fail (addr1->address != NULL, 1);
			g_return_val_if_fail (addr2->address != NULL, -1);
			
			retval = g_strcasecmp (addr1->address, addr2->address);
		} else {
			/* compare last names */
			if (!addr1->wname->last)
				retval = -1;
			else if (!addr2->wname->last)
				retval = 1;
			else {
				retval = g_strcasecmp (addr1->wname->last, addr2->wname->last);
				if (!retval) {
					/* last names are identical - compare first names */
					if (!addr1->wname->first)
						retval = -1;
					else if (!addr2->wname->first)
						retval = 1;
					else {
						retval = g_strcasecmp (addr1->wname->first, addr2->wname->first);
						if (!retval) {
							/* first names are identical - compare addresses */
							g_return_val_if_fail (addr1->address != NULL, 1);
							g_return_val_if_fail (addr2->address != NULL, -1);
							
							retval = g_strcasecmp (addr1->address, addr2->address);
						}
					}
				}
			}
		}
	}
	
	return retval;
}

static gint
address_compare (gconstpointer address1, gconstpointer address2)
{
	EMailAddress *addr1, *addr2;
	gint retval;
	
	addr1 = e_mail_address_new (address1);
	addr2 = e_mail_address_new (address2);
	retval = e_mail_address_compare (addr1, addr2);
	e_mail_address_free (addr1);
	e_mail_address_free (addr2);
	
	return retval;
}

static gint
subject_compare (gconstpointer subject1, gconstpointer subject2)
{
	char *sub1;
	char *sub2;
	
	/* trim off any "Re:"'s at the beginning of subject1 */
	sub1 = (char *) subject1;
	while (!g_strncasecmp (sub1, "Re:", 3)) {
		sub1 += 3;
		/* jump over any spaces */
		for ( ; *sub1 && isspace (*sub1); sub1++);
	}
	
	/* trim off any "Re:"'s at the beginning of subject2 */
	sub2 = (char *) subject2;
	while (!g_strncasecmp (sub2, "Re:", 3)) {
		sub2 += 3;
		/* jump over any spaces */
		for ( ; *sub2 && isspace (*sub2); sub2++);
	}
	
	/* jump over any spaces */
	for ( ; *sub1 && isspace (*sub1); sub1++);
	for ( ; *sub2 && isspace (*sub2); sub2++);
	
	return g_strcasecmp (sub1, sub2);
}

static gchar *
filter_size (const void *data)
{
	gint size = GPOINTER_TO_INT(data);
	gfloat fsize;
	
	if (size < 1024) {
		return g_strdup_printf ("%d", size);
	} else {
		fsize = ((gfloat) size) / 1024.0;
		if (fsize < 1024.0) {
			return g_strdup_printf ("%.2f K", fsize);
		} else {
			fsize /= 1024.0;
			return g_strdup_printf ("%.2f M", fsize);
		}
	}
}

/* Gets the uid of the message displayed at a given view row */
static const char *
get_message_uid (MessageList *message_list, int row)
{
	ETreeModel *model = (ETreeModel *)message_list->table_model;
	ETreePath *node;
	const char *uid;

	if (row >= e_table_model_row_count (message_list->table_model))
		return NULL;

	node = e_tree_model_node_at_row (model, row);
	g_return_val_if_fail (node != NULL, NULL);
	uid = e_tree_model_node_get_data (model, node);

	if (!id_is_uid(uid))
		return NULL;

	return id_uid(uid);
}

/* Gets the CamelMessageInfo for the message displayed at the given
 * view row.
 */
static const CamelMessageInfo *
get_message_info (MessageList *message_list, int row)
{
	const char *uid;
	
	uid = get_message_uid(message_list, row);
	if (uid)
		return camel_folder_get_message_info(message_list->folder, uid);

	return NULL;
}

/**
 * message_list_select:
 * @message_list: a MessageList
 * @base_row: the (model) row to start from
 * @direction: the direction to search in
 * @flags: a set of flag values
 * @mask: a mask for comparing against @flags
 *
 * This moves the message list selection to a suitable row. @base_row
 * lists the first (model) row to try, but as a special case, model
 * row -1 is mapped to the last row. @flags and @mask combine to specify
 * what constitutes a suitable row. @direction is
 * %MESSAGE_LIST_SELECT_NEXT if it should find the next matching
 * message, or %MESSAGE_LIST_SELECT_PREVIOUS if it should find the
 * previous. If no suitable row is found, the selection will be
 * unchanged but the message display will be cleared.
 **/
void
message_list_select (MessageList *message_list, int base_row,
		     MessageListSelectDirection direction,
		     guint32 flags, guint32 mask)
{
	const CamelMessageInfo *info;
	int vrow, mrow, last;
	ETable *et = message_list->table;

	switch (direction) {
	case MESSAGE_LIST_SELECT_PREVIOUS:
		last = -1;
		break;
	case MESSAGE_LIST_SELECT_NEXT:
		last = e_table_model_row_count (message_list->table_model);
		break;
	default:
		g_warning("Invalid argument to message_list_select");
		return;
	}

	if (base_row == -1)
		base_row = e_table_model_row_count(message_list->table_model) - 1;

	/* model_to_view_row etc simply dont work for sorted views.  Sigh. */
	vrow = e_table_model_to_view_row (et, base_row);

	/* We don't know whether to use < or > due to "direction" */
	while (vrow != last) {
		mrow = e_table_view_to_model_row (et, vrow);
		info = get_message_info (message_list, mrow);
		if (info && (info->flags & mask) == flags) {
			e_table_set_cursor_row (et, mrow);
			gtk_signal_emit(GTK_OBJECT (message_list), message_list_signals [MESSAGE_SELECTED], camel_message_info_uid(info));
			return;
		}
		vrow += direction;
	}

	gtk_signal_emit(GTK_OBJECT (message_list), message_list_signals [MESSAGE_SELECTED], NULL);
}

static void
message_list_drag_data_get (ETable             *table,
			    int                 row,
			    int                 col,
			    GdkDragContext     *context,
			    GtkSelectionData   *selection_data,
			    guint               info,
			    guint               time,
			    gpointer            user_data)
{
	MessageList *mlist = (MessageList *) user_data;
	const CamelMessageInfo *minfo = get_message_info (mlist, row);
	GPtrArray *uids = NULL;
	char *tmpl, *tmpdir, *filename, *subject;
	
	switch (info) {
	case DND_TARGET_LIST_TYPE_URI:
		/* drag & drop into nautilus */
		tmpl = g_strdup ("/tmp/evolution.XXXXXX");
#ifdef HAVE_MKDTEMP
		tmpdir = mkdtemp (tmpl);
#else
		tmpdir = mktemp (tmpl);
		if (tmpdir) {
			if (mkdir (tmpdir, S_IRWXU) == -1)
				tmpdir = NULL;
		}
#endif
		if (!tmpdir) {
			g_free (tmpl);
			return;
		}
		
		subject = g_strdup (camel_message_info_subject (minfo));
		e_filename_make_safe (subject);
		filename = g_strdup_printf ("%s/%s.eml", tmpdir, subject);
		g_free (subject);
		
		uids = g_ptr_array_new ();
		g_ptr_array_add (uids, g_strdup (mlist->cursor_uid));
		
		mail_do_save_messages (mlist->folder, uids, filename);
		
		gtk_selection_data_set (selection_data, selection_data->target, 8,
					(guchar *) filename, strlen (filename));
		
		g_free (tmpl);
		g_free (filename);
		break;
	default:
		break;
	}
}

/*
 * SimpleTableModel::col_count
 */
static int
ml_col_count (ETableModel *etm, void *data)
{
	return COL_LAST;
}

static void *
ml_duplicate_value (ETableModel *etm, int col, const void *value, void *data)
{
	switch (col){
	case COL_MESSAGE_STATUS:
	case COL_FLAGGED:
	case COL_SCORE:
	case COL_ATTACHMENT:
	case COL_DELETED:
	case COL_UNREAD:
	case COL_SENT:
	case COL_RECEIVED:
	case COL_SIZE:
		return (void *) value;

	case COL_FROM:
	case COL_SUBJECT:
	case COL_TO:
		return g_strdup (value);
	default:
		g_assert_not_reached ();
	}
	return NULL;
}

static void
ml_free_value (ETableModel *etm, int col, void *value, void *data)
{
	switch (col){
	case COL_MESSAGE_STATUS:
	case COL_FLAGGED:
	case COL_SCORE:
	case COL_ATTACHMENT:
	case COL_DELETED:
	case COL_UNREAD:
	case COL_SENT:
	case COL_RECEIVED:
	case COL_SIZE:
		break;

	case COL_FROM:
	case COL_SUBJECT:
	case COL_TO:
		g_free (value);
		break;
	default:
		g_assert_not_reached ();
	}
}

static void *
ml_initialize_value (ETableModel *etm, int col, void *data)
{
	switch (col){
	case COL_MESSAGE_STATUS:
	case COL_FLAGGED:
	case COL_SCORE:
	case COL_ATTACHMENT:
	case COL_DELETED:
	case COL_UNREAD:
	case COL_SENT:
	case COL_RECEIVED:
	case COL_SIZE:
		return NULL;

	case COL_FROM:
	case COL_SUBJECT:
	case COL_TO:
		return g_strdup("");
	default:
		g_assert_not_reached ();
	}

	return NULL;
}

static gboolean
ml_value_is_empty (ETableModel *etm, int col, const void *value, void *data)
{
	switch (col){
	case COL_MESSAGE_STATUS:
	case COL_FLAGGED:
	case COL_SCORE:
	case COL_ATTACHMENT:
	case COL_DELETED:
	case COL_UNREAD:
	case COL_SENT:
	case COL_RECEIVED:
	case COL_SIZE:
		return value == NULL;

	case COL_FROM:
	case COL_SUBJECT:
	case COL_TO:
		return !(value && *(char *)value);
	default:
		g_assert_not_reached ();
		return FALSE;
	}
}

static char *
ml_value_to_string (ETableModel *etm, int col, const void *value, void *data)
{
	switch (col){
	case COL_MESSAGE_STATUS:
		switch ((int) value) {
		case 0:
			return g_strdup (_("Unseen"));
			break;
		case 1:
			return g_strdup (_("Seen"));
			break;
		case 2:
			return g_strdup (_("Answered"));
			break;
		default:
			return g_strdup ("");
			break;
		}
		break;
		
	case COL_SCORE:
		switch ((int) value) {
		case -3:
			return g_strdup ("Lowest");
			break;
		case -2:
			return g_strdup ("Lower");
			break;
		case -1:
			return g_strdup ("Low");
			break;
		case 1:
			return g_strdup ("High");
			break;
		case 2:
			return g_strdup ("Higher");
			break;
		case 3:
			return g_strdup ("Highest");
			break;
		default:
			return g_strdup ("Normal");
			break;
		}
		break;
		
	case COL_ATTACHMENT:
	case COL_FLAGGED:
	case COL_DELETED:
	case COL_UNREAD:
		return g_strdup_printf("%d", (int) value);
		
	case COL_SENT:
	case COL_RECEIVED:
		return filter_date (value);
		
	case COL_SIZE:
		return filter_size (value);

	case COL_FROM:
	case COL_SUBJECT:
	case COL_TO:
		return g_strdup (value);
	default:
		g_assert_not_reached ();
		return NULL;
	}
}

static GdkPixbuf *
ml_tree_icon_at (ETreeModel *etm, ETreePath *path, void *model_data)
{
	/* we dont really need an icon ... */
	return NULL;
}

/* return true if there are any unread messages in the subtree */
static int
subtree_unread(MessageList *ml, ETreePath *node)
{
	const CamelMessageInfo *info;
	char *uid;

	while (node) {
		ETreePath *child;
		uid = e_tree_model_node_get_data((ETreeModel *)ml->table_model, node);
		if (uid == NULL) {
			g_warning("I got a NULL uid at node %p", node);
		} else if (id_is_uid(uid)
			   && (info = camel_folder_get_message_info(ml->folder, id_uid(uid)))) {
			if (!(info->flags & CAMEL_MESSAGE_SEEN))
				return TRUE;
		}
		if ((child = e_tree_model_node_get_first_child (E_TREE_MODEL (ml->table_model), node)))
			if (subtree_unread(ml, child))
				return TRUE;
		node = e_tree_model_node_get_next (E_TREE_MODEL (ml->table_model), node);
	}
	return FALSE;
}

static int
subtree_size(MessageList *ml, ETreePath *node)
{
	const CamelMessageInfo *info;
	char *uid;
	int size = 0;

	while (node) {
		ETreePath *child;
		uid = e_tree_model_node_get_data((ETreeModel *)ml->table_model, node);
		if (uid == NULL) {
			g_warning("I got a NULL uid at node %p", node);
		} else if (id_is_uid(uid)
			   && (info = camel_folder_get_message_info(ml->folder, id_uid(uid)))) {
			size += info->size;
		}
		if ((child = e_tree_model_node_get_first_child (E_TREE_MODEL (ml->table_model), node)))
			size += subtree_size(ml, child);

		node = e_tree_model_node_get_next (E_TREE_MODEL (ml->table_model), node);
	}
	return size;
}

static time_t
subtree_earliest(MessageList *ml, ETreePath *node, int sent)
{
	const CamelMessageInfo *info;
	char *uid;
	time_t earliest = 0, date;

	while (node) {
		ETreePath *child;
		uid = e_tree_model_node_get_data((ETreeModel *)ml->table_model, node);
		if (uid == NULL) {
			g_warning("I got a NULL uid at node %p", node);
		} else if (id_is_uid(uid)
			   && (info = camel_folder_get_message_info(ml->folder, id_uid(uid)))) {
			if (sent)
				date = info->date_sent;
			else
				date = info->date_received;
			if (earliest == 0 || date < earliest)
				earliest = date;
		}
		if ((child = e_tree_model_node_get_first_child (E_TREE_MODEL (ml->table_model), node))) {
			date = subtree_earliest(ml, child, sent);
			if (earliest == 0 || (date != 0 && date < earliest))
				earliest = date;
		}

		node = e_tree_model_node_get_next (E_TREE_MODEL (ml->table_model), node);
	}

	return earliest;
}

static void *
ml_tree_value_at (ETreeModel *etm, ETreePath *path, int col, void *model_data)
{
	MessageList *message_list = model_data;
	const CamelMessageInfo *msg_info;
	char *uid;
	static char *saved;

	/* simlated(tm) static dynamic memory (sigh) */
	if (saved) {
		g_free(saved);
		saved = 0;
	}

	/* retrieve the message information array */
	uid = e_tree_model_node_get_data (etm, path);
	if (uid == NULL) {
		uid="s ERROR ERROR - UNKNOWN ROW IN TREE";
		goto fake;
	}
	if (!id_is_uid(uid))
		goto fake;
	uid = id_uid(uid);

	msg_info = camel_folder_get_message_info (message_list->folder, uid);
	if (msg_info == NULL) {
		g_warning("UID for message-list not found in folder: %s", uid);
		return NULL;
	}
	
	switch (col){
	case COL_MESSAGE_STATUS:
		if (msg_info->flags & CAMEL_MESSAGE_ANSWERED)
			return GINT_TO_POINTER (2);
		else if (msg_info->flags & CAMEL_MESSAGE_SEEN)
			return GINT_TO_POINTER (1);
		else
			return GINT_TO_POINTER (0);
		
	case COL_FLAGGED:
		return (void *)((msg_info->flags & CAMEL_MESSAGE_FLAGGED) != 0);

	case COL_SCORE:
	{
		const char *tag;
		int score = 0;
		
		tag = camel_tag_get ((CamelTag **) &msg_info->user_tags, "score");
		if (tag)
			score = atoi (tag);
		
		return GINT_TO_POINTER (score);
	}
		
	case COL_ATTACHMENT:
		return (void *)((msg_info->flags & CAMEL_MESSAGE_ATTACHMENTS) != 0);
		
	case COL_FROM:
		return (char *)camel_message_info_from(msg_info);

	case COL_SUBJECT:
		return (char *)camel_message_info_subject(msg_info);
		
	case COL_SENT:
		return GINT_TO_POINTER (msg_info->date_sent);
		
	case COL_RECEIVED:
		return GINT_TO_POINTER (msg_info->date_received);
		
	case COL_TO:
		return (char *)camel_message_info_to(msg_info);

	case COL_SIZE:
		return GINT_TO_POINTER (msg_info->size);
		
	case COL_DELETED:
		return (void *)((msg_info->flags & CAMEL_MESSAGE_DELETED) != 0);
		
	case COL_UNREAD:
		return GINT_TO_POINTER (!(msg_info->flags & CAMEL_MESSAGE_SEEN));
		
	case COL_COLOUR:
	{
		const char *colour;

		colour = camel_tag_get ((CamelTag **) &msg_info->user_tags,
					"colour");
		if (colour)
			return (void *)colour;
		else if (msg_info->flags & CAMEL_MESSAGE_FLAGGED)
			/* FIXME: extract from the xpm somehow. */
			return "#A7453E";
		else
			return NULL;
	}
	}

	g_assert_not_reached ();
	
 fake:
	/* This is a fake tree parent */
	switch (col){
	case COL_UNREAD:
		/* this value should probably be cached, as it could take a bit
		   of processing to evaluate all the time */
		return (void *)subtree_unread(message_list, e_tree_model_node_get_first_child(etm, path));

	case COL_MESSAGE_STATUS:
	case COL_FLAGGED:
	case COL_SCORE:
	case COL_ATTACHMENT:
	case COL_DELETED:
	case COL_COLOUR:
		return (void *) 0;

	case COL_SENT:
		return (void *)subtree_earliest(message_list, e_tree_model_node_get_first_child(etm, path), TRUE);

	case COL_RECEIVED:
		return (void *)subtree_earliest(message_list, e_tree_model_node_get_first_child(etm, path), FALSE);

	case COL_SUBJECT:
		saved = g_strdup_printf(_("[ %s ]"), id_subject(uid));
		return saved;
		
	case COL_FROM: {
		ETreePath *child;
		
		/* the first child should always exist/etc */
		if ( (child = e_tree_model_node_get_first_child(etm, path))
		     && (uid = e_tree_model_node_get_data (etm, child))
		     && id_is_uid(uid)	
		     && (msg_info = camel_folder_get_message_info (message_list->folder, id_uid(uid))) ) {
			/* well, we could scan more children, build up a (more accurate) list, but this should do ok */
			saved = g_strdup_printf(_("%s, et al."), camel_message_info_from(msg_info));
		} else {
			return _("<unknown>");
		}
		return saved;
	}
	case COL_TO: {
		ETreePath *child;
		
		/* the first child should always exist/etc */
		if ( (child = e_tree_model_node_get_first_child(etm, path))
		     && (uid = e_tree_model_node_get_data (etm, child))
		     && id_is_uid(uid)	
		     && (msg_info = camel_folder_get_message_info (message_list->folder, id_uid(uid))) ) {
			/* well, we could scan more children, build up a (more accurate) list, but this should do ok */
			saved = g_strdup_printf(_("%s, et al."), camel_message_info_to(msg_info));
		} else {
			return _("<unknown>");
		}
		return saved;
	}
	case COL_SIZE:
		return GINT_TO_POINTER (subtree_size(message_list, e_tree_model_node_get_first_child(etm, path)));
	}
	g_assert_not_reached ();
	
	return NULL;
}

static void
ml_tree_set_value_at (ETreeModel *etm, ETreePath *path, int col,
		      const void *val, void *model_data)
{
	g_assert_not_reached ();
}

static gboolean
ml_tree_is_cell_editable (ETreeModel *etm, ETreePath *path, int col, void *model_data)
{
	return FALSE;
}

static void
message_list_init_images (void)
{
	int i;
	
	/*
	 * Only load once, and share
	 */
	if (states_pixmaps [0].pixbuf)
		return;
	
	for (i = 0; states_pixmaps [i].image_base; i++){
		states_pixmaps [i].pixbuf = gdk_pixbuf_new_from_xpm_data (
			(const char **) states_pixmaps [i].image_base);
	}
}

static char *
filter_date (const void *data)
{
	time_t date = GPOINTER_TO_INT (data);
	char buf[26], *p;

	if (date == 0)
		return g_strdup ("?");

#ifdef CTIME_R_THREE_ARGS
	ctime_r (&date, buf, 26);
#else
	ctime_r (&date, buf);
#endif

	p = strchr (buf, '\n');
	if (p)
		*p = '\0';

	return g_strdup (buf);
}

static ETableExtras *
message_list_create_extras (void)
{
	int i;
	GdkPixbuf *images [7];
	ETableExtras *extras;
	ECell *cell;

	extras = e_table_extras_new();
	e_table_extras_add_pixbuf(extras, "status", states_pixmaps [0].pixbuf);
	e_table_extras_add_pixbuf(extras, "score", states_pixmaps [11].pixbuf);
	e_table_extras_add_pixbuf(extras, "attachment", states_pixmaps [4].pixbuf);
	e_table_extras_add_pixbuf(extras, "flagged", states_pixmaps [5].pixbuf);
	
	e_table_extras_add_compare(extras, "address_compare", address_compare);
	e_table_extras_add_compare(extras, "subject_compare", subject_compare);
	
	for (i = 0; i < 3; i++)
		images [i] = states_pixmaps [i].pixbuf;
	
	e_table_extras_add_cell(extras, "render_message_status", e_cell_toggle_new (0, 3, images));

	for (i = 0; i < 2; i++)
		images [i] = states_pixmaps [i + 3].pixbuf;
	
	e_table_extras_add_cell(extras, "render_attachment", e_cell_toggle_new (0, 2, images));
	
	images [1] = states_pixmaps [5].pixbuf;
	e_table_extras_add_cell(extras, "render_flagged", e_cell_toggle_new (0, 2, images));

	for (i = 0; i < 7; i++)
		images[i] = states_pixmaps [i + 5].pixbuf;
	
	e_table_extras_add_cell(extras, "render_score", e_cell_toggle_new (0, 7, images));
	
	/* date cell */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	gtk_object_set (GTK_OBJECT (cell),
			"text_filter", filter_date,
			"strikeout_column", COL_DELETED,
			"bold_column", COL_UNREAD,
			"color_column", COL_COLOUR,
			NULL);
	e_table_extras_add_cell(extras, "render_date", cell);
	
	/* text cell */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	gtk_object_set (GTK_OBJECT (cell),
			"strikeout_column", COL_DELETED,
			"bold_column", COL_UNREAD,
			"color_column", COL_COLOUR,
			NULL);
	e_table_extras_add_cell(extras, "render_text", cell);
	
	e_table_extras_add_cell(extras, "render_tree", 
				e_cell_tree_new (NULL, NULL, /* let the tree renderer default the pixmaps */
						 TRUE, cell));

	/* size cell */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_RIGHT);
	gtk_object_set (GTK_OBJECT (cell),
			"text_filter", filter_size,
			"strikeout_column", COL_DELETED,
			"bold_column", COL_UNREAD,
			"color_column", COL_COLOUR,
			NULL);
	e_table_extras_add_cell(extras, "render_size", cell);

	return extras;
}

static void
save_header_state(MessageList *ml)
{
	char *filename;

	if (ml->folder == NULL || ml->table == NULL)
		return;

	filename = mail_config_folder_to_cachename(ml->folder, "et-header-");
	e_table_save_state(ml->table, filename);
	g_free(filename);
}

static char *
message_list_get_layout (MessageList *message_list)
{
	/* Default: Status, Attachments, Priority, From, Subject, Date */
	return g_strdup ("<ETableSpecification cursor-mode=\"line\" draw-grid=\"true\">"
			 "<ETableColumn model_col= \"0\" _title=\"Status\" pixbuf=\"status\" expansion=\"0.0\" minimum_width=\"18\" resizable=\"false\" cell=\"render_message_status\" compare=\"integer\" sortable=\"false\"/>"
			 "<ETableColumn model_col= \"1\" _title=\"Flagged\" pixbuf=\"flagged\" expansion=\"0.0\" minimum_width=\"18\" resizable=\"false\" cell=\"render_flagged\" compare=\"integer\"/>"
			 "<ETableColumn model_col= \"2\" _title=\"Score\" pixbuf=\"score\" expansion=\"0.0\" minimum_width=\"18\" resizable=\"false\" cell=\"render_score\" compare=\"integer\"/>"
			 "<ETableColumn model_col= \"3\" _title=\"Attachment\" pixbuf=\"attachment\" expansion=\"0.0\" minimum_width=\"18\" resizable=\"false\" cell=\"render_attachment\" compare=\"integer\" sortable=\"false\"/>"
			 "<ETableColumn model_col= \"4\" _title=\"From\" expansion=\"24.0\" minimum_width=\"32\" resizable=\"true\" cell=\"render_text\" compare=\"address_compare\"/>"
			 "<ETableColumn model_col= \"5\" _title=\"Subject\" expansion=\"30.0\" minimum_width=\"32\" resizable=\"true\" cell=\"render_tree\" compare=\"subject_compare\"/>"
			 "<ETableColumn model_col= \"6\" _title=\"Date\" expansion=\"24.0\" minimum_width=\"32\" resizable=\"true\" cell=\"render_date\" compare=\"integer\"/>"
			 "<ETableColumn model_col= \"7\" _title=\"Received\" expansion=\"20.0\" minimum_width=\"32\" resizable=\"true\" cell=\"render_date\" compare=\"integer\"/>"
			 "<ETableColumn model_col= \"8\" _title=\"To\" expansion=\"24.0\" minimum_width=\"32\" resizable=\"true\" cell=\"render_text\" compare=\"address_compare\"/>"
			 "<ETableColumn model_col= \"9\" _title=\"Size\" expansion=\"6.0\" minimum_width=\"32\" resizable=\"true\" cell=\"render_size\" compare=\"integer\"/>"
			 "<ETableState> <column source=\"0\"/> <column source=\"3\"/> <column source=\"1\"/>"
			 "<column source=\"4\"/> <column source=\"5\"/> <column source=\"6\"/>"
			 "<grouping> </grouping> </ETableState>"
			 "</ETableSpecification>");
}

static void
message_list_setup_etable(MessageList *message_list)
{
	/* build the spec based on the folder, and possibly from a saved file */
	/* otherwise, leave default */
	if (message_list->folder) {
		char *path;
		char *name;
		struct stat st;
		
		name = camel_service_get_name (CAMEL_SERVICE (message_list->folder->parent_store), TRUE);
		printf ("folder name is '%s'\n", name);
		path = mail_config_folder_to_cachename (message_list->folder, "et-header-");
		
		if (path && stat (path, &st) == 0 && st.st_size > 0 && S_ISREG (st.st_mode)) {
			/* build based on saved file */
			e_table_load_state (message_list->table, path);
		} else if (strstr (name, "/Drafts") || strstr (name, "/Outbox") || strstr (name, "/Sent")) {
			/* these folders have special defaults */
			char *state = "<ETableState>"
				"<column source=\"0\"/> <column source=\"1\"/> "
				"<column source=\"8\"/> <column source=\"5\"/> "
				"<column source=\"6\"/> <grouping> </grouping> </ETableState>";
			
			e_table_set_state (message_list->table, state);
		}
		
		g_free (path);
		g_free (name);
	}
}

/*
 * GtkObject::init
 */
static void
message_list_init (GtkObject *object)
{
	MessageList *message_list = MESSAGE_LIST (object);

	e_scroll_frame_set_policy (E_SCROLL_FRAME (message_list),
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

	message_list->uid_rowmap = g_hash_table_new (g_str_hash, g_str_equal);
	message_list->uid_pool = e_mempool_new(1024, 512, E_MEMPOOL_ALIGN_BYTE);
	message_list->hidden = NULL;
	message_list->hidden_pool = NULL;
	message_list->hide_before = ML_HIDE_NONE_START;
	message_list->hide_after = ML_HIDE_NONE_END;
}

static void
message_list_destroy (GtkObject *object)
{
	MessageList *message_list = MESSAGE_LIST (object);

	if (message_list->folder) {
		save_tree_state(message_list);
		save_header_state(message_list);
		hide_save_state(message_list);
	}
	
	gtk_object_unref (GTK_OBJECT (message_list->table_model));
	
	g_hash_table_destroy (message_list->uid_rowmap);
	e_mempool_destroy(message_list->uid_pool);

	if (message_list->idle_id != 0)
		g_source_remove(message_list->idle_id);
	
	if (message_list->seen_id)
		gtk_timeout_remove (message_list->seen_id);
	
	mail_tool_camel_lock_up();
	if (message_list->folder) {
		camel_object_unhook_event((CamelObject *)message_list->folder, "folder_changed",
					  folder_changed, message_list);
		camel_object_unhook_event((CamelObject *)message_list->folder, "message_changed",
					  message_changed, message_list);
		camel_object_unref (CAMEL_OBJECT (message_list->folder));
	}

	if (message_list->hidden) {
		g_hash_table_destroy(message_list->hidden);
		e_mempool_destroy(message_list->hidden_pool);
		message_list->hidden = NULL;
		message_list->hidden_pool = NULL;
	}
	mail_tool_camel_lock_down();
	
	GTK_OBJECT_CLASS (message_list_parent_class)->destroy (object);
}

/*
 * GtkObjectClass::init
 */
static void
message_list_class_init (GtkObjectClass *object_class)
{
	message_list_parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = message_list_destroy;

	message_list_signals[MESSAGE_SELECTED] =
		gtk_signal_new ("message_selected",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (MessageListClass, message_selected),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1, GTK_TYPE_STRING);

	gtk_object_class_add_signals(object_class, message_list_signals, LAST_SIGNAL);

	message_list_init_images ();
}

static void
message_list_construct (MessageList *message_list)
{
	ETableExtras *extras;
	char *spec;

	message_list->table_model = (ETableModel *)
		e_tree_simple_new (ml_col_count,
				   ml_duplicate_value,
				   ml_free_value,
				   ml_initialize_value,
				   ml_value_is_empty,
				   ml_value_to_string,
				   ml_tree_icon_at, ml_tree_value_at,
				   ml_tree_set_value_at,
				   ml_tree_is_cell_editable,
				   message_list);
	gtk_object_ref (GTK_OBJECT (message_list->table_model));
	gtk_object_sink (GTK_OBJECT (message_list->table_model));
	
	e_tree_model_root_node_set_visible ((ETreeModel *)message_list->table_model, FALSE);

	/*
	 * The etable
	 */
	spec = message_list_get_layout (message_list);
	extras = message_list_create_extras ();
	e_table_scrolled_construct (E_TABLE_SCROLLED (message_list),
				    message_list->table_model,
				    extras, spec, NULL);
	message_list->table =
		e_table_scrolled_get_table (E_TABLE_SCROLLED (message_list));
	g_free (spec);
	gtk_object_sink (GTK_OBJECT (extras));

	gtk_object_set (GTK_OBJECT (message_list->table), "drawfocus",
			FALSE, NULL);

	gtk_signal_connect (GTK_OBJECT (message_list->table), "cursor_change",
			    GTK_SIGNAL_FUNC (on_cursor_change_cmd),
			    message_list);

	gtk_signal_connect (GTK_OBJECT (message_list->table), "click",
			    GTK_SIGNAL_FUNC (on_click), message_list);
	
	/* drag & drop */
	e_table_drag_source_set (message_list->table, GDK_BUTTON1_MASK,
				 drag_types, num_drag_types, GDK_ACTION_MOVE);
	
	gtk_signal_connect (GTK_OBJECT (message_list->table), "drag_data_get",
			    GTK_SIGNAL_FUNC (message_list_drag_data_get),
			    message_list);
}

GtkWidget *
message_list_new (void)
{
	MessageList *message_list;

	message_list = MESSAGE_LIST (gtk_widget_new (message_list_get_type (),
						     "hadjustment", NULL,
						     "vadjustment", NULL,
						     NULL));
	message_list_construct (message_list);

	return GTK_WIDGET (message_list);
}

static void
clear_tree (MessageList *ml)
{
	ETreeModel *etm = E_TREE_MODEL (ml->table_model);

#ifdef TIMEIT
	struct timeval start, end;
	unsigned long diff;

	printf("Clearing tree\n");
	gettimeofday(&start, NULL);
#endif

	/* we also reset the uid_rowmap since it is no longer useful/valid anyway */
	g_hash_table_destroy (ml->uid_rowmap);
	ml->uid_rowmap = g_hash_table_new(g_str_hash, g_str_equal);
	e_mempool_flush(ml->uid_pool, TRUE);
	
	if (ml->tree_root) {
		/* we should be frozen already */
		e_tree_model_node_remove (etm, ml->tree_root);
	}

	ml->tree_root = e_tree_model_node_insert (etm, NULL, 0, NULL);
	e_tree_model_node_set_expanded (etm, ml->tree_root, TRUE);

#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Clearing tree took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif

}

/* we save the node id to the file if the node should be closed when
   we start up.  We only save nodeid's for messages with children */
static void
save_node_state(MessageList *ml, FILE *out, ETreePath *node)
{
	char *data;
	const CamelMessageInfo *info;

	while (node) {
		ETreePath *child = e_tree_model_node_get_first_child (E_TREE_MODEL (ml->table_model), node);
		if (child
		    && !e_tree_model_node_is_expanded((ETreeModel *)ml->table_model, node)) {
			data = e_tree_model_node_get_data((ETreeModel *)ml->table_model, node);
			if (data) {
				if (id_is_uid(data)) {
					info = camel_folder_get_message_info(ml->folder, id_uid(data));
					if (info) {
						fprintf(out, "%08x%08x\n", info->message_id.id.part.hi, info->message_id.id.part.lo);
					}
				} else {
					fprintf(out, "%s\n", data);
				}
			}
		}
		if (child) {
			save_node_state(ml, out, child);
		}
		node = e_tree_model_node_get_next (E_TREE_MODEL (ml->table_model), node);
	}
}

static GHashTable *
load_tree_state(MessageList *ml)
{
	char *filename, linebuf[10240];
	GHashTable *result;
	FILE *in;
	int len;

	result = g_hash_table_new(g_str_hash, g_str_equal);
	filename = mail_config_folder_to_cachename(ml->folder, "treestate-");
	in = fopen(filename, "r");
	if (in) {
		while (fgets(linebuf, sizeof(linebuf), in) != NULL) {
			len = strlen(linebuf);
			if (len) {
				linebuf[len-1] = 0;
				g_hash_table_insert(result, g_strdup(linebuf), (void *)1);
			}
		}
		fclose(in);
	}
	g_free(filename);
	return result;
}

/* save tree info */
static void
save_tree_state(MessageList *ml)
{
	char *filename;
	ETreePath *node;
	ETreePath *child;
	FILE *out;
	int rem;

	filename = mail_config_folder_to_cachename(ml->folder, "treestate-");
	out = fopen(filename, "w");
	if (out) {
		node = e_tree_model_get_root((ETreeModel *)ml->table_model);
		child = e_tree_model_node_get_first_child ((ETreeModel *)ml->table_model, node);
		if (node && child) {
			save_node_state(ml, out, child);
		}
		rem = ftell(out) == 0;
		fclose(out);
		/* remove the file if it was empty, should probably check first, but this is easier */
		if (rem)
			unlink(filename);
	}
	g_free(filename);
}

static void
free_node_state(void *key, void *value, void *data)
{
	g_free(key);
}

static void
free_tree_state(GHashTable *expanded_nodes)
{
	g_hash_table_foreach(expanded_nodes, free_node_state, 0);
	g_hash_table_destroy(expanded_nodes);
}

/* only call if we have a tree model */
/* builds the tree structure */
static void build_subtree (MessageList *ml, ETreePath *parent, CamelFolderThreadNode *c, int *row, GHashTable *);

static void build_subtree_diff (MessageList *ml, ETreePath *parent, ETreePath *path, CamelFolderThreadNode *c, int *row, GHashTable *expanded_nodes);

static void
build_tree (MessageList *ml, CamelFolderThread *thread, CamelFolderChangeInfo *changes)
{
	int row = 0;
	GHashTable *expanded_nodes;
	ETreeModel *etm = (ETreeModel *)ml->table_model;
	ETreePath *top;

#ifdef TIMEIT
	struct timeval start, end;
	unsigned long diff;

	printf("Building tree\n");
	gettimeofday(&start, NULL);
#endif
	expanded_nodes = load_tree_state(ml);

#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Loading tree state took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif

	if (ml->tree_root == NULL) {
		ml->tree_root =	e_tree_model_node_insert(etm, NULL, 0, NULL);
		e_tree_model_node_set_expanded(etm, ml->tree_root, TRUE);
	}

#define BROKEN_ETREE	/* avoid some broken code in etree(?) by not using the incremental update */

	top = e_tree_model_node_get_first_child(etm, ml->tree_root);
#ifndef BROKEN_ETREE
	if (top == NULL || changes == NULL) {
#endif
		e_tree_model_freeze(etm);
		clear_tree (ml);
		build_subtree(ml, ml->tree_root, thread->tree, &row, expanded_nodes);
		e_tree_model_thaw(etm);
#ifndef BROKEN_ETREE
	} else {
		build_subtree_diff(ml, ml->tree_root, top,  thread->tree, &row, expanded_nodes);
	}
#endif
	free_tree_state(expanded_nodes);
	
#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Building tree took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif

}

static char *
new_id_from_uid(MessageList *ml, const char *uid)
{
	char *res;
	int len;

	len = strlen(uid)+2;
	res = e_mempool_alloc(ml->uid_pool, len);
	res[0] = 'u';
	strcpy(res+1, uid);
	return res;
}

static char *
new_id_from_subject(MessageList *ml, const char *subject)
{
	char *res;
	int len;

	len = strlen(subject)+2;
	res = e_mempool_alloc(ml->uid_pool, len);
	res[0] = 's';
	strcpy(res+1, subject);
	return res;
}

/* this is about 20% faster than build_subtree_diff,
   entirely because e_tree_model_node_insert(xx, -1 xx)
   is faster than inserting to the right row :( */
/* Otherwise, this code would probably go as it does the same thing essentially */
static void
build_subtree (MessageList *ml, ETreePath *parent, CamelFolderThreadNode *c, int *row, GHashTable *expanded_nodes)
{
	ETreeModel *tree = E_TREE_MODEL (ml->table_model);
	ETreePath *node;
	char *id;
	int expanded = FALSE;

	while (c) {
		if (c->message) {
			id = new_id_from_uid(ml, camel_message_info_uid(c->message));
			g_hash_table_insert(ml->uid_rowmap, id_uid(id), GINT_TO_POINTER ((*row)++));
			if (c->child) {
				if (c->message) {
					char key[17];
					sprintf(key, "%08x%08x", c->message->message_id.id.part.hi, c->message->message_id.id.part.lo);
					expanded = !g_hash_table_lookup(expanded_nodes, key) != 0;
				} else
					expanded = TRUE;
			}
		} else {
			id = new_id_from_subject(ml, c->root_subject);
			if (c->child) {
				expanded = !g_hash_table_lookup(expanded_nodes, id) != 0;
			}
		}
		node = e_tree_model_node_insert(tree, parent, -1, id);
		if (c->child) {
			/* by default, open all trees */
			if (expanded)
				e_tree_model_node_set_expanded(tree, node, expanded);
			build_subtree(ml, node, c->child, row, expanded_nodes);
		}
		c = c->next;
	}
}

/* compares a thread tree node with the etable tree node to see if they point to
   the same object */
static int
node_equal(ETreeModel *etm, ETreePath *ap, CamelFolderThreadNode *bp)
{
	char *uid;

	uid = e_tree_model_node_get_data(etm, ap);

	if (id_is_uid(uid)) {
		if (bp->message && strcmp(id_uid(uid), camel_message_info_uid(bp->message))==0)
			return 1;
	} else if (id_is_subject(uid)) {
		if (bp->message == NULL && strcmp(id_subject(uid), bp->root_subject) == 0)
			return 1;
	}
	return 0;
}

/* adds a single node, retains save state, and handles adding children if required */
static void
add_node_diff(MessageList *ml, ETreePath *parent, ETreePath *path, CamelFolderThreadNode *c, int *row, int myrow, GHashTable *expanded_nodes)
{
	ETreeModel *etm = E_TREE_MODEL (ml->table_model);
	ETreePath *node;
	char *id;
	int expanded = FALSE;

	if (c->message) {
		id = new_id_from_uid(ml, camel_message_info_uid(c->message));
		/* need to remove the id first, as GHashTable' wont replace the key pointer for us */
		g_hash_table_remove(ml->uid_rowmap, id_uid(id));
		g_hash_table_insert(ml->uid_rowmap, id_uid(id), GINT_TO_POINTER (*row));
		if (c->child) {
			if (c->message) {
				char key[17];
				sprintf(key, "%08x%08x", c->message->message_id.id.part.hi, c->message->message_id.id.part.lo);
				expanded = !g_hash_table_lookup(expanded_nodes, key) != 0;
			} else
				expanded = TRUE;
		}
	} else {
		id = new_id_from_subject(ml, c->root_subject);
		if (c->child) {
			expanded = !g_hash_table_lookup(expanded_nodes, id) != 0;
		}
	}

	t(printf("Adding node: %s row %d\n", id, myrow));

	node = e_tree_model_node_insert(etm, parent, myrow, id);
	(*row)++;
	if (c->child) {
		e_tree_model_node_set_expanded(etm, node, expanded);
		t(printf("Building subtree ...\n"));
		build_subtree_diff(ml, node, NULL, c->child, row, expanded_nodes);
	}
}

/* removes node, children recursively and all associated data */
static void
remove_node_diff(MessageList *ml, ETreePath *node, int depth)
{
	ETreeModel *etm = E_TREE_MODEL (ml->table_model);
	ETreePath *cp, *cn;
	char *uid, *olduid;
	int oldrow;

	t(printf("Removing node: %s\n", (char *)e_tree_model_node_get_data(etm, node)));

	/* we depth-first remove all node data's ... */
	cp = e_tree_model_node_get_first_child(etm, node);
	while (cp) {
		cn = e_tree_model_node_get_next(etm, cp);
		remove_node_diff(ml, cp, depth+1);
		cp = cn;
	}

	/* and the rowid entry - if and only if it is referencing this node */
	uid = e_tree_model_node_get_data(etm, node);
	if (id_is_uid(uid)
	    && g_hash_table_lookup_extended(ml->uid_rowmap, id_uid(uid), (void *)&olduid, (void *)&oldrow)
	    && olduid == id_uid(uid)) {
		t(printf("removing rowid map entry: %s\n", id_uid(uid)));
		g_hash_table_remove(ml->uid_rowmap, id_uid(uid));
	}
	e_tree_model_node_set_data(etm, node, NULL);

	/* and only at the toplevel, remove the node (etree should optimise this remove somewhat) */
	if (depth == 0)
		e_tree_model_node_remove(etm, node);
}

/* applies a new tree structure to an existing tree, but only by changing things
   that have changed */
static void
build_subtree_diff(MessageList *ml, ETreePath *parent, ETreePath *path, CamelFolderThreadNode *c, int *row, GHashTable *expanded_nodes)
{
	ETreeModel *etm = E_TREE_MODEL (ml->table_model);
	ETreePath *ap, *ai, *at, *tmp;
	CamelFolderThreadNode *bp, *bi, *bt;
	int i, j, myrow = 0;

	ap = path;
	bp = c;

	while (ap || bp) {
		t(printf("Processing row: %d (subtree row %d)\n", *row, myrow));
		if (ap == NULL) {
			t(printf("out of old nodes\n"));
			/* ran out of old nodes - remaining nodes are added */
			add_node_diff(ml, parent, ap, bp, row, myrow, expanded_nodes);
			myrow++;
			bp = bp->next;
		} else if (bp == NULL) {
			t(printf("out of new nodes\n"));
			/* ran out of new nodes - remaining nodes are removed */
			tmp = e_tree_model_node_get_next(etm, ap);
			remove_node_diff(ml, ap, 0);
			ap = tmp;
		} else if (node_equal(etm, ap, bp)) {
			/*t(printf("nodes match, verify\n"));*/
			/* matching nodes, verify details/children */
			if (bp->message) {
				char *olduid;
				int oldrow;

				/* if this is a message row, check/update the row id map */
				if (g_hash_table_lookup_extended(ml->uid_rowmap, camel_message_info_uid(bp->message), (void *)&olduid, (void *)&oldrow)) {
					if (oldrow != (*row)) {
						g_hash_table_insert(ml->uid_rowmap, olduid, (void *)(*row));
					}
				} else {
					g_warning("Cannot find uid %s in table?", camel_message_info_uid(bp->message));
					/*g_assert_not_reached();*/
				}
			}
			*row = (*row)+1;
			myrow++;
			tmp = e_tree_model_node_get_first_child(etm, ap);
			/* make child lists match (if either has one) */
			if (bp->child || tmp) {
				build_subtree_diff(ml, ap, tmp, bp->child, row, expanded_nodes);
			}
			ap = e_tree_model_node_get_next(etm, ap);
			bp = bp->next;
		} else {
			t(printf("searching for matches\n"));
			/* we have to scan each side for a match */
			bi = bp->next;
			ai = e_tree_model_node_get_next(etm, ap);
			for (i=1;bi!=NULL;i++,bi=bi->next) {
				if (node_equal(etm, ap, bi))
					break;
			}
			for (j=1;ai!=NULL;j++,ai=e_tree_model_node_get_next(etm, ai)) {
				if (node_equal(etm, ai, bp))
					break;
			}
			if (i<j) {
				/* smaller run of new nodes - must be nodes to add */
				if (bi) {
					bt = bp;
					while (bt != bi) {
						t(printf("adding new node 0\n"));
						add_node_diff(ml, parent, NULL, bt, row, myrow, expanded_nodes);
						myrow++;
						bt = bt->next;
					}
					bp = bi;
				} else {
					t(printf("adding new node 1\n"));
					/* no match in new nodes, add one, try next */
					add_node_diff(ml, parent, NULL, bp, row, myrow, expanded_nodes);
					myrow++;
					bp = bp->next;
				}
			} else {
				/* bigger run of old nodes - must be nodes to remove */
				if (ai) {
					at = ap;
					while (at != ai) {
						t(printf("removing old node 0\n"));
						tmp = e_tree_model_node_get_next(etm, at);
						remove_node_diff(ml, at, 0);
						at = tmp;
					}
					ap = ai;
				} else {
					t(printf("adding new node 2\n"));
					/* didn't find match in old nodes, must be new node? */
					add_node_diff(ml, parent, NULL, bp, row, myrow, expanded_nodes);
					myrow++;
					bp = bp->next;
#if 0
					tmp = e_tree_model_node_get_next(etm, ap);
					remove_node_diff(etm, ap, 0);
					ap = tmp;
#endif
				}
			}
		}
	}
}

static void build_flat_diff(MessageList *ml, CamelFolderChangeInfo *changes);

static void
build_flat (MessageList *ml, GPtrArray *uids, CamelFolderChangeInfo *changes)
{
	ETreeModel *tree = E_TREE_MODEL (ml->table_model);
	ETreePath *node;
	char *uid;
	int i;

#ifdef TIMEIT
	struct timeval start, end;
	unsigned long diff;

	printf("Building flat\n");
	gettimeofday(&start, NULL);
#endif

#ifndef BROKEN_ETREE
	if (changes) {
		build_flat_diff(ml, changes);
	} else {
#endif
		e_tree_model_freeze(tree);
		clear_tree (ml);
		for (i = 0; i < uids->len; i++) {
			uid = new_id_from_uid(ml, uids->pdata[i]);
			node = e_tree_model_node_insert (tree, ml->tree_root, -1, uid);
			g_hash_table_insert (ml->uid_rowmap, id_uid(uid), GINT_TO_POINTER (i));
		}
		e_tree_model_thaw(tree);
#ifndef BROKEN_ETREE
	}
#endif

#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Building flat took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif

}

/* used to sort the rows to match list order */
struct _uidsort {
	int row;
	char *uid;
};

static int
sort_uid_cmp(const void *ap, const void *bp)
{
	const struct _uidsort *a = (struct _uidsort *)ap;
	const struct _uidsort *b = (struct _uidsort *)bp;

	if (a->row < b->row)
		return -1;
	else if (a->row > b->row)
		return 1;
	return 0;
}

static void
sort_uid_to_rows(MessageList *ml, GPtrArray *uids)
{
	struct _uidsort *uidlist;
	int i;

	uidlist = g_malloc(sizeof(struct _uidsort) * uids->len);
	for (i=0;i<uids->len;i++) {
		uidlist[i].row = (int)g_hash_table_lookup(ml->uid_rowmap, uids->pdata[i]);
		uidlist[i].uid = uids->pdata[i];
	}
	qsort(uidlist, uids->len, sizeof(struct _uidsort), sort_uid_cmp);
	for (i=0;i<uids->len;i++) {
		uids->pdata[i] = uidlist[i].uid;
	}
	g_free(uidlist);
}

static void
build_flat_diff(MessageList *ml, CamelFolderChangeInfo *changes)
{
	int row, i;
	ETreePath *node;
	char *uid;
	int oldrow;
	char *olduid;

#ifdef TIMEIT
	struct timeval start, end;
	unsigned long diff;
		
	gettimeofday(&start, NULL);
#endif

	printf("updating changes to display\n");

	/* remove individual nodes? */
	if (changes->uid_removed->len > 0) {
		/* first, we need to sort the row id's to match the summary order */
		sort_uid_to_rows(ml, changes->uid_removed);

		/* we remove from the end, so that the rowmap remains valid as we go */
		d(printf("Removing messages from view:\n"));
		for (i=changes->uid_removed->len-1;i>=0;i--) {
			d(printf(" %s\n", (char *)changes->uid_removed->pdata[i]));
			if (g_hash_table_lookup_extended(ml->uid_rowmap, changes->uid_removed->pdata[i], (void *)&olduid, (void *)&row)) {
				node = e_tree_model_node_at_row((ETreeModel *)ml->table_model, row);
				uid = e_tree_model_node_get_data((ETreeModel *)ml->table_model, node);
				if (uid && id_is_uid(uid) && !strcmp(id_uid(uid), changes->uid_removed->pdata[i])) {
					g_hash_table_remove(ml->uid_rowmap, olduid);
					e_tree_model_node_remove((ETreeModel *)ml->table_model, node);
					d(printf("  - removed\n"));
				} else {
					d(printf("  - is this the right uid, it doesn't match my map?\n"));
				}
			}
		}
	}

	/* add new nodes? - just append to the end */
	if (changes->uid_added->len > 0) {
		node = e_tree_model_node_get_last_child((ETreeModel *)ml->table_model, ml->tree_root);
		row = e_tree_model_row_of_node((ETreeModel *)ml->table_model, node) + 1;
		d(printf("Adding messages to view:\n"));
		for (i=0;i<changes->uid_added->len;i++) {
			d(printf(" %s\n", (char *)changes->uid_added->pdata[i]));
			uid = new_id_from_uid(ml, changes->uid_added->pdata[i]);
			node = e_tree_model_node_insert((ETreeModel *)ml->table_model, ml->tree_root, row, uid);
			g_hash_table_insert(ml->uid_rowmap, id_uid(uid), GINT_TO_POINTER (row));
			row++;
		}
	}

	/* now, check the rowmap, some rows might've changed (with removes) */
	if (changes->uid_removed->len) {
		d(printf("checking uid mappings\n"));
		row = 0;
		node = e_tree_model_node_get_first_child ((ETreeModel *)ml->table_model, ml->tree_root);
		while (node) {
			uid = e_tree_model_node_get_data((ETreeModel *)ml->table_model, node);
			if (id_is_uid(uid)) {
				if (g_hash_table_lookup_extended(ml->uid_rowmap, id_uid(uid), (void *)&olduid, (void *)&oldrow)) {
					if (oldrow != row) {
						d(printf("row %d moved to new row %d\n", oldrow, row));
						g_hash_table_insert(ml->uid_rowmap, olduid, (void *)row);
					}
				} else { /* missing?  shouldn't happen */
					g_warning("Uid vanished from rowmap?: %s\n", uid);
				}
			}
			row++;
			node = e_tree_model_node_get_next((ETreeModel *)ml->table_model, node);
		}
	}

#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Inserting changes took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif

}

static void
main_folder_changed (CamelObject *o, gpointer event_data, gpointer user_data)
{
	MessageList *ml = MESSAGE_LIST (user_data);
	CamelFolderChangeInfo *changes = (CamelFolderChangeInfo *)event_data;

	printf("folder changed event, changes = %p\n", changes);

	mail_do_regenerate_messagelist(ml, ml->search, NULL, changes);
}

static void
folder_changed (CamelObject *o, gpointer event_data, gpointer user_data)
{
	/* similarly to message_changed, copy the change list and propagate it to
	   the main thread and free it */
	CamelFolderChangeInfo *changes;

	if (event_data) {
		changes = camel_folder_change_info_new();
		camel_folder_change_info_cat(changes, (CamelFolderChangeInfo *)event_data);
	} else {
		changes = NULL;
	}
	mail_op_forward_event (main_folder_changed, o, changes, user_data);
}

static void
main_message_changed (CamelObject *o, gpointer uid, gpointer user_data)
{
	MessageList *message_list = MESSAGE_LIST (user_data);
	int row;

	row = GPOINTER_TO_INT (g_hash_table_lookup (message_list->uid_rowmap,
						    uid));
	if (row != -1)
		e_table_model_row_changed (message_list->table_model, row);

	g_free (uid);
}

static void
message_changed (CamelObject *o, gpointer event_data, gpointer user_data)
{
	/* Here we copy the data because our thread may free the copy that we would reference.
	 * The other thread would be passed a uid parameter that pointed to freed data.
	 * We copy it and free it in the handler. 
	 */
	mail_op_forward_event (main_message_changed, o, g_strdup ((gchar *)event_data), user_data);
}

void
message_list_set_folder (MessageList *message_list, CamelFolder *camel_folder)
{
	CamelException ex;

	g_return_if_fail (message_list != NULL);
	g_return_if_fail (camel_folder != NULL);
	g_return_if_fail (IS_MESSAGE_LIST (message_list));
	g_return_if_fail (CAMEL_IS_FOLDER (camel_folder));
	g_return_if_fail (camel_folder_has_summary_capability (camel_folder));

	if (message_list->folder == camel_folder)
		return;

	camel_exception_init (&ex);

	if (message_list->folder) {
		hide_save_state(message_list);
		camel_object_unhook_event((CamelObject *)message_list->folder, "folder_changed",
					  folder_changed, message_list);
		camel_object_unhook_event((CamelObject *)message_list->folder, "message_changed",
					  message_changed, message_list);
		camel_object_unref (CAMEL_OBJECT (message_list->folder));
	}

	message_list->folder = camel_folder;

	/* build the etable suitable for this folder */
	message_list_setup_etable(message_list);

	camel_object_hook_event(CAMEL_OBJECT (camel_folder), "folder_changed",
			   folder_changed, message_list);
	camel_object_hook_event(CAMEL_OBJECT (camel_folder), "message_changed",
			   message_changed, message_list);

	camel_object_ref (CAMEL_OBJECT (camel_folder));

	hide_load_state(message_list);

	clear_tree(message_list);
	mail_do_regenerate_messagelist (message_list, message_list->search, NULL, NULL);
}

E_MAKE_TYPE (message_list, "MessageList", MessageList, message_list_class_init, message_list_init, PARENT_TYPE);

static gboolean
on_cursor_change_idle (gpointer data)
{
	MessageList *message_list = data;

	printf("emitting cursor changed signal, for uid %s\n", message_list->cursor_uid);
	gtk_signal_emit(GTK_OBJECT (message_list), message_list_signals [MESSAGE_SELECTED], message_list->cursor_uid);

	message_list->idle_id = 0;
	return FALSE;
}

static void
on_cursor_change_cmd (ETableScrolled *table, int row, gpointer user_data)
{
	MessageList *message_list;
	
	message_list = MESSAGE_LIST (user_data);
	
	message_list->cursor_row = row;
	message_list->cursor_uid = get_message_uid (message_list, row);

	if (!message_list->idle_id) {
		message_list->idle_id =
			g_idle_add_full (G_PRIORITY_LOW, on_cursor_change_idle,
					 message_list, NULL);
	}
}

static gint
on_click (ETableScrolled *table, gint row, gint col, GdkEvent *event, MessageList *list)
{
	int flag;
	const CamelMessageInfo *info;

	if (col == COL_MESSAGE_STATUS)
		flag = CAMEL_MESSAGE_SEEN;
	else if (col == COL_FLAGGED)
		flag = CAMEL_MESSAGE_FLAGGED;
	else
		return FALSE;

	mail_tool_camel_lock_up();

	info = get_message_info(list, row);
	if (info == NULL) {
		mail_tool_camel_lock_down();
		return FALSE;
	}
	
	camel_folder_set_message_flags(list->folder, camel_message_info_uid(info), flag, ~info->flags);

	mail_tool_camel_lock_down();

	if (flag == CAMEL_MESSAGE_SEEN && list->seen_id) {
		gtk_timeout_remove (list->seen_id);
		list->seen_id = 0;
	}

	return TRUE;
}

struct message_list_foreach_data {
	MessageList *message_list;
	MessageListForeachFunc callback;
	gpointer user_data;
};

static void
mlfe_callback (int row, gpointer user_data)
{
	struct message_list_foreach_data *mlfe_data = user_data;
	const char *uid;

	uid = get_message_uid (mlfe_data->message_list, row);
	if (uid) {
		mlfe_data->callback (mlfe_data->message_list,
				     uid,
				     mlfe_data->user_data);
	}
}

void
message_list_foreach (MessageList *message_list,
		      MessageListForeachFunc callback,
		      gpointer user_data)
{
	struct message_list_foreach_data mlfe_data;

	mlfe_data.message_list = message_list;
	mlfe_data.callback = callback;
	mlfe_data.user_data = user_data;
	e_table_selected_row_foreach (message_list->table,
				      mlfe_callback, &mlfe_data);
}

/* set whether we are in threaded view or flat view */
void
message_list_set_threaded(MessageList *ml, gboolean threaded)
{
	if (ml->threaded ^ threaded) {
		ml->threaded = threaded;

		mail_do_regenerate_messagelist(ml, ml->search, NULL, NULL);
	}
}

void
message_list_set_search(MessageList *ml, const char *search)
{
	if (search == NULL || search[0] == '\0')
		if (ml->search == NULL || ml->search[0]=='\0')
			return;

	if (search != NULL && ml->search !=NULL && strcmp(search, ml->search)==0)
		return;

	mail_do_regenerate_messagelist(ml, search, NULL, NULL);
}

/* returns the number of messages displayable *after* expression hiding has taken place */
unsigned int   message_list_length(MessageList *ml)
{
	return ml->hide_unhidden;
}

/* add a new expression to hide, or set the range.
   @expr: A new search expression - all matching messages will be hidden.  May be %NULL.
   @lower: Use ML_HIDE_NONE_START to specify no messages hidden from the start of the list.
   @upper: Use ML_HIDE_NONE_END to specify no message hidden from the end of the list.

   For either @upper or @lower, use ML_HIDE_SAME, to keep the previously set hide range.
   If either range is negative, then the range is taken from the end of the available list
   of messages, once other hiding has been performed.  Use message_list_length() to find out
   how many messages are available for hiding.

   Example: hide_add(ml, NULL, -100, ML_HIDE_NONE_END) -> hide all but the last (most recent)
   100 messages.
*/
void	       message_list_hide_add(MessageList *ml, const char *expr, unsigned int lower, unsigned int upper)
{
	mail_tool_camel_lock_up();
	if (lower != ML_HIDE_SAME)
		ml->hide_before = lower;
	if (upper != ML_HIDE_SAME)
		ml->hide_after = upper;
	mail_tool_camel_lock_down();

	mail_do_regenerate_messagelist(ml, ml->search, expr, NULL);
}

/* hide specific uid's */
void	       message_list_hide_uids(MessageList *ml, GPtrArray *uids)
{
	int i;
	char *uid;

	/* first see if we need to do any work, if so, then do it all at once */
	for (i=0;i<uids->len;i++) {
		if (g_hash_table_lookup(ml->uid_rowmap, uids->pdata[i])) {
			mail_tool_camel_lock_up();
			if (ml->hidden == NULL) {
				ml->hidden = g_hash_table_new(g_str_hash, g_str_equal);
				ml->hidden_pool = e_mempool_new(512, 256, E_MEMPOOL_ALIGN_BYTE);
			}
	
			uid =  e_mempool_strdup(ml->hidden_pool, uids->pdata[i]);
			g_hash_table_insert(ml->hidden, uid, uid);
			for (;i<uids->len;i++) {
				if (g_hash_table_lookup(ml->uid_rowmap, uids->pdata[i])) {
					uid =  e_mempool_strdup(ml->hidden_pool, uids->pdata[i]);
					g_hash_table_insert(ml->hidden, uid, uid);
				}
			}
			mail_tool_camel_lock_down();
			mail_do_regenerate_messagelist(ml, ml->search, NULL, NULL);
			break;
		}
	}
}

/* no longer hide any messages */
void	       message_list_hide_clear(MessageList *ml)
{
	mail_tool_camel_lock_up();
	if (ml->hidden) {
		g_hash_table_destroy(ml->hidden);
		e_mempool_destroy(ml->hidden_pool);
		ml->hidden = NULL;
		ml->hidden_pool = NULL;
	}
	ml->hide_before = ML_HIDE_NONE_START;
	ml->hide_after = ML_HIDE_NONE_END;
	mail_tool_camel_lock_down();

	mail_do_regenerate_messagelist(ml, ml->search, NULL, NULL);
}

#define HIDE_STATE_VERSION (1)

/* version 1 file is:
   uintf	1
   uintf	hide_before
   uintf       	hide_after
   string*	uids
*/

static void hide_load_state(MessageList *ml)
{
	char *filename;
	FILE *in;
	guint32 version, lower, upper;

	filename = mail_config_folder_to_cachename(ml->folder, "hidestate-");
	in = fopen(filename, "r");
	if (in) {
		camel_folder_summary_decode_fixed_int32(in, &version);
		if (version == HIDE_STATE_VERSION) {
			mail_tool_camel_lock_up();
			if (ml->hidden == NULL) {
				ml->hidden = g_hash_table_new(g_str_hash, g_str_equal);
				ml->hidden_pool = e_mempool_new(512, 256, E_MEMPOOL_ALIGN_BYTE);
			}
			camel_folder_summary_decode_fixed_int32(in, &lower);
			ml->hide_before = lower;
			camel_folder_summary_decode_fixed_int32(in, &upper);
			ml->hide_after = upper;
			while (!feof(in)) {
				char *olduid, *uid;

				if (camel_folder_summary_decode_string(in, &olduid) != -1) {
					uid =  e_mempool_strdup(ml->hidden_pool, olduid);
					g_hash_table_insert(ml->hidden, uid, uid);
				}
			}
			mail_tool_camel_lock_down();
		}
		fclose(in);
	}
	g_free(filename);
}

static void hide_save_1(char *uid, char *keydata, FILE *out)
{
	camel_folder_summary_encode_string(out, uid);
}

/* save the hide state.  Note that messages are hidden by uid, if the uid's change, then
   this will become invalid, but is easy to reset in the ui */
static void hide_save_state(MessageList *ml)
{
	char *filename;
	FILE *out;

	filename = mail_config_folder_to_cachename(ml->folder, "hidestate-");
	mail_tool_camel_lock_up();
	if (ml->hidden == NULL && ml->hide_before == ML_HIDE_NONE_START && ml->hide_after == ML_HIDE_NONE_END) {
		unlink(filename);
	} else if ( (out = fopen(filename, "w")) ) {
		camel_folder_summary_encode_fixed_int32(out, HIDE_STATE_VERSION);
		camel_folder_summary_encode_fixed_int32(out, ml->hide_before);
		camel_folder_summary_encode_fixed_int32(out, ml->hide_after);
		if (ml->hidden)
			g_hash_table_foreach(ml->hidden, (GHFunc)hide_save_1, out);
		fclose(out);
	}
	mail_tool_camel_lock_down();
}

/* ** REGENERATE MESSAGELIST ********************************************** */

typedef struct regenerate_messagelist_input_s {
	MessageList *ml;
	CamelFolder *folder;
	char *search;
	char *hideexpr;
	CamelFolderChangeInfo *changes;
	gboolean dotree;	/* we are building a tree */
} regenerate_messagelist_input_t;

typedef struct regenerate_messagelist_data_s {
	GPtrArray *uids,	/* list of uid's to use, if realuids is NULL, this is the actual uid's from search, else a simple ptrarray */
		*realuids;	/* actual uid's from search/get_uid's, or NULL */
	CamelFolderThread *tree;
	CamelFolderChangeInfo *changes;
} regenerate_messagelist_data_t;

static gchar *describe_regenerate_messagelist (gpointer in_data, gboolean gerund);
static void setup_regenerate_messagelist   (gpointer in_data, gpointer op_data, CamelException *ex);
static void do_regenerate_messagelist      (gpointer in_data, gpointer op_data, CamelException *ex);
static void cleanup_regenerate_messagelist (gpointer in_data, gpointer op_data, CamelException *ex);

static gchar *describe_regenerate_messagelist (gpointer in_data, gboolean gerund)
{
	if (gerund)
		return g_strdup (_("Rebuilding message view"));
	else
		return g_strdup (_("Rebuild message view"));
}

static void setup_regenerate_messagelist (gpointer in_data, gpointer op_data, CamelException *ex)
{
	regenerate_messagelist_input_t *input = (regenerate_messagelist_input_t *) in_data;

	if (!IS_MESSAGE_LIST (input->ml)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No messagelist specified to regenerate");
		return;
	}

	gtk_object_ref (GTK_OBJECT (input->ml));
}

static void do_regenerate_messagelist (gpointer in_data, gpointer op_data, CamelException *ex)
{
	regenerate_messagelist_input_t *input = (regenerate_messagelist_input_t *) in_data;
	regenerate_messagelist_data_t *data = (regenerate_messagelist_data_t *) op_data;
	int i;
	GPtrArray *uids, *uidnew;

	mail_tool_camel_lock_up();

	if (input->search) {
		data->uids = camel_folder_search_by_expression(input->ml->folder, input->search, ex);
	} else {
		data->uids = camel_folder_get_uids (input->ml->folder);
	}

	if (camel_exception_is_set (ex)) {
		mail_tool_camel_lock_down();
		return;
	}

	/* see if we have a new expression to hide on */
	if (input->hideexpr) {
		uidnew = camel_folder_search_by_expression(input->ml->folder, input->hideexpr, ex);
		/* well, lets not abort just because this faileld ... */
		camel_exception_clear(ex);

		if (uidnew) {
			if (input->ml->hidden == NULL) {
				input->ml->hidden = g_hash_table_new(g_str_hash, g_str_equal);
				input->ml->hidden_pool = e_mempool_new(512, 256, E_MEMPOOL_ALIGN_BYTE);
			}
			
			for (i=0;i<uidnew->len;i++) {
				if (g_hash_table_lookup(input->ml->hidden, uidnew->pdata[i]) == 0) {
					char *uid = e_mempool_strdup(input->ml->hidden_pool, uidnew->pdata[i]);
					g_hash_table_insert(input->ml->hidden, uid, uid);
				}
			}
			camel_folder_search_free(input->ml->folder, uidnew);
		}
	}

	input->ml->hide_unhidden = data->uids->len;

	/* what semantics do we want from hide_before, hide_after?
	   probably <0 means measure from the end of the list */

	/* perform uid hiding */
	if (input->ml->hidden || input->ml->hide_before != ML_HIDE_NONE_START || input->ml->hide_after != ML_HIDE_NONE_END) {
		int start, end;
		uidnew = g_ptr_array_new();
		uids = data->uids;

		/* first, hide matches */
		if (input->ml->hidden) {
			for (i=0;i<uids->len;i++) {
				if (g_hash_table_lookup(input->ml->hidden, uids->pdata[i]) == 0)
					g_ptr_array_add(uidnew, uids->pdata[i]);
			}
		}

		/* then calculate the subrange visible and chop it out */
		input->ml->hide_unhidden = uidnew->len;

		if (input->ml->hide_before != ML_HIDE_NONE_START || input->ml->hide_after != ML_HIDE_NONE_END) {
			GPtrArray *uid2 = g_ptr_array_new();

			start = input->ml->hide_before;
			if (start < 0)
				start += input->ml->hide_unhidden;
			end = input->ml->hide_after;
			if (end < 0)
				end += input->ml->hide_unhidden;

			start = MAX(start, 0);
			end = MIN(end, uidnew->len);
			for (i=start;i<end;i++) {
				g_ptr_array_add(uid2, uidnew->pdata[i]);
			}

			g_ptr_array_free(uidnew, TRUE);
			uidnew = uid2;
		}
		data->realuids = uids;
		data->uids = uidnew;
	} else {
		data->realuids = NULL;
	}

	if (input->dotree && data->uids)
		data->tree = camel_folder_thread_messages_new(input->ml->folder, data->uids);
	else
		data->tree = NULL;

	mail_tool_camel_lock_down();
}

static void cleanup_regenerate_messagelist (gpointer in_data, gpointer op_data, CamelException *ex)
{
	regenerate_messagelist_input_t *input = (regenerate_messagelist_input_t *) in_data;
	regenerate_messagelist_data_t *data = (regenerate_messagelist_data_t *) op_data;
	GPtrArray *uids;
	ETreeModel *etm;

	etm = E_TREE_MODEL (input->ml->table_model);

	if (data->uids == NULL) { /*exception*/
		gtk_object_unref (GTK_OBJECT (input->ml));
		return;
	}

	if (input->dotree)
		build_tree(input->ml, data->tree, input->changes);
	else
		build_flat(input->ml, data->uids, input->changes);

	/* work out if we have aux uid's to free, otherwise free the real ones */
	uids = data->realuids;
	if (uids)
		g_ptr_array_free(data->uids, TRUE);
	else
		uids = data->uids;

	if (input->search)
		camel_folder_search_free (input->ml->folder, uids);
	else
		camel_folder_free_uids (input->ml->folder, uids);

	/* update what we have as our search string */
        if (input->ml->search && input->ml->search != input->search)
                g_free(input->ml->search);
	input->ml->search = input->search;

	if (data->tree)
		camel_folder_thread_messages_destroy(data->tree);

	g_free(input->hideexpr);

	if (input->changes)
		camel_folder_change_info_free(input->changes);

	gtk_object_unref (GTK_OBJECT (input->ml));
}

static const mail_operation_spec op_regenerate_messagelist =
{
	describe_regenerate_messagelist,
	sizeof (regenerate_messagelist_data_t),
	setup_regenerate_messagelist,
	do_regenerate_messagelist,
	cleanup_regenerate_messagelist
};

/* if changes == NULL, then update the whole list, otherwise just update the changes */
static void
mail_do_regenerate_messagelist (MessageList *list, const gchar *search, const char *hideexpr, CamelFolderChangeInfo *changes)
{
	regenerate_messagelist_input_t *input;

	/* This may get called on empty folder-browsers by the bonobo ui
	 * callback for threaded view.
	 */
	if (!list->folder)
		return;

	/* see if we need to goto the child thread at all anyway */
	/* currently the only case is the flat view with updates and no search */
	if (hideexpr == NULL && search == NULL && changes != NULL && !list->threaded) {
		build_flat_diff(list, changes);
		camel_folder_change_info_free(changes);
		return;
	}

	input = g_new (regenerate_messagelist_input_t, 1);
	input->ml = list;
	input->search = g_strdup (search);
	input->hideexpr = g_strdup(hideexpr);
	input->changes = changes;
	input->dotree = list->threaded;

	mail_operation_queue (&op_regenerate_messagelist, input, TRUE);
}
