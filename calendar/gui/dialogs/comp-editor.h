/* Evolution calendar - Framework for a calendar component editor dialog
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef COMP_EDITOR_H
#define COMP_EDITOR_H

#include <gtk/gtk.h>
#include <libecal/e-cal.h>
#include "../itip-utils.h"
#include "comp-editor-page.h"

/* Standard GObject macros */
#define TYPE_COMP_EDITOR \
	(comp_editor_get_type ())
#define COMP_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), TYPE_COMP_EDITOR, CompEditor))
#define COMP_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), TYPE_COMP_EDITOR, CompEditorClass))
#define IS_COMP_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), TYPE_COMP_EDITOR))
#define IS_COMP_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), TYPE_COMP_EDITOR))
#define COMP_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), TYPE_COMP_EDITOR, CompEditorClass))

G_BEGIN_DECLS

typedef struct _CompEditor CompEditor;
typedef struct _CompEditorClass CompEditorClass;
typedef struct _CompEditorPrivate CompEditorPrivate;

struct _CompEditor {
	GtkWindow parent;
	CompEditorPrivate *priv;
};

struct _CompEditorClass {
	GtkWindowClass parent_class;
	const gchar *help_section;

	/* Virtual functions */
	void (*edit_comp) (CompEditor *page, ECalComponent *comp);
	void (*object_created) (CompEditor *page);
	gboolean (*send_comp) (CompEditor *page, ECalComponentItipMethod method);

	void (*show_categories) (CompEditor *editor, gboolean visible);
	void (*show_role) (CompEditor *editor, gboolean visible);
	void (*show_rsvp) (CompEditor *editor, gboolean visible);
	void (*show_status) (CompEditor *editor, gboolean visible);
	void (*show_time_zone) (CompEditor *editor, gboolean visible);
	void (*show_type) (CompEditor *editor, gboolean visible);
};

typedef enum {
	COMP_EDITOR_NEW_ITEM = 1<<0,
	COMP_EDITOR_MEETING = 1<<1,
	COMP_EDITOR_DELEGATE = 1<<2,
	COMP_EDITOR_USER_ORG = 1<<3,
	COMP_EDITOR_IS_ASSIGNED = 1<<4,
	COMP_EDITOR_IS_SHARED = 1 << 5
} CompEditorFlags;

GType		comp_editor_get_type		(void);
void		comp_editor_set_changed		(CompEditor *editor,
						 gboolean changed);
gboolean	comp_editor_get_changed		(CompEditor *editor);
void		comp_editor_set_needs_send	(CompEditor *editor,
						 gboolean needs_send);
gboolean	comp_editor_get_needs_send	(CompEditor *editor);
void		comp_editor_set_existing_org	(CompEditor *editor,
						gboolean existing_org);
gboolean	comp_editor_get_existing_org	(CompEditor *editor);
void		comp_editor_set_user_org	(CompEditor *editor,
						gboolean user_org);
gboolean	comp_editor_get_user_org	(CompEditor *editor);
void		comp_editor_set_group_item	(CompEditor *editor,
						 gboolean is_group_item);
gboolean	comp_editor_get_group_item	(CompEditor *editor);
void		comp_editor_set_classification	(CompEditor *editor,
						 ECalComponentClassification classification);
ECalComponentClassification
		comp_editor_get_classification	(CompEditor *editor);
void		comp_editor_set_summary		(CompEditor *editor,
						 const gchar *summary);
const gchar *	comp_editor_get_summary		(CompEditor *editor);
void		comp_editor_append_page		(CompEditor *editor,
						 CompEditorPage *page,
						 const gchar *label,
						 gboolean add);
void		comp_editor_remove_page		(CompEditor *editor,
						 CompEditorPage *page);
void		comp_editor_show_page		(CompEditor *editor,
						 CompEditorPage *page);
void		comp_editor_set_client		(CompEditor *editor,
						 ECal *client);
ECal *		comp_editor_get_client		(CompEditor *editor);
void		comp_editor_edit_comp		(CompEditor *ee,
						 ECalComponent *comp);
ECalComponent *	comp_editor_get_comp		(CompEditor *editor);
ECalComponent *	comp_editor_get_current_comp	(CompEditor *editor,
						 gboolean *correct);
gboolean	comp_editor_save_comp		(CompEditor *editor,
						 gboolean send);
void		comp_editor_delete_comp		(CompEditor *editor);
gboolean	comp_editor_send_comp		(CompEditor *editor,
						 ECalComponentItipMethod method);
GSList *	comp_editor_get_mime_attach_list(CompEditor *editor);
gboolean	comp_editor_close		(CompEditor *editor);


void		comp_editor_sensitize_attachment_bar
						(CompEditor *editor,
						 gboolean set);
void		comp_editor_set_flags		(CompEditor *editor,
						 CompEditorFlags flags);
CompEditorFlags
		comp_editor_get_flags		(CompEditor *editor);
GtkUIManager *	comp_editor_get_ui_manager	(CompEditor *editor);
GtkAction *	comp_editor_get_action		(CompEditor *editor,
						 const gchar *action_name);
GtkActionGroup *
		comp_editor_get_action_group	(CompEditor *editor,
                                            	 const gchar *group_name);
GtkWidget *	comp_editor_get_managed_widget	(CompEditor *editor,
						 const gchar *widget_path);

G_END_DECLS

#endif
