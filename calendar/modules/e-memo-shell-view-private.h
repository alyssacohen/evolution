/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-memo-shell-view-private.h
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

#ifndef E_MEMO_SHELL_VIEW_PRIVATE_H
#define E_MEMO_SHELL_VIEW_PRIVATE_H

#include "e-memo-shell-view.h"

#include <string.h>
#include <glib/gi18n.h>

#include "e-util/e-dialog-utils.h"
#include "e-util/e-error.h"
#include "e-util/e-util.h"
#include "e-util/gconf-bridge.h"

#include "calendar/gui/e-calendar-selector.h"
#include "calendar/gui/e-memo-preview.h"
#include "calendar/gui/print.h"
#include "calendar/gui/dialogs/calendar-setup.h"
#include "calendar/gui/dialogs/copy-source-dialog.h"
#include "calendar/gui/dialogs/memo-editor.h"

#include "e-memo-shell-content.h"
#include "e-memo-shell-sidebar.h"
#include "e-memo-shell-view-actions.h"

#define E_MEMO_SHELL_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MEMO_SHELL_VIEW, EMemoShellViewPrivate))

/* Shorthand, requires a variable named "shell_window". */
#define ACTION(name) \
	(E_SHELL_WINDOW_ACTION_##name (shell_window))
#define ACTION_GROUP(name) \
	(E_SHELL_WINDOW_ACTION_GROUP_##name (shell_window))

/* For use in dispose() methods. */
#define DISPOSE(obj) \
	G_STMT_START { \
	if ((obj) != NULL) { g_object_unref (obj); (obj) = NULL; } \
	} G_STMT_END

/* ETable Specifications */
#define ETSPEC_FILENAME		"e-memo-table.etspec"

G_BEGIN_DECLS

/* List these in the order to be displayed. */
enum {
	MEMO_SEARCH_SUMMARY_CONTAINS,
	MEMO_SEARCH_DESCRIPTION_CONTAINS,
	MEMO_SEARCH_ANY_FIELD_CONTAINS
};

struct _EMemoShellViewPrivate {

	/*** Module Data ***/

	ESourceList *source_list;

	/*** UI Management ***/

	GtkActionGroup *memo_actions;

	/*** Other Stuff ***/

	/* These are just for convenience. */
	EMemoShellContent *memo_shell_content;
	EMemoShellSidebar *memo_shell_sidebar;

	EActivity *activity;
};

void		e_memo_shell_view_private_init
					(EMemoShellView *memo_shell_view,
					 EShellViewClass *shell_view_class);
void		e_memo_shell_view_private_constructed
					(EMemoShellView *memo_shell_view);
void		e_memo_shell_view_private_dispose
					(EMemoShellView *memo_shell_view);
void		e_memo_shell_view_private_finalize
					(EMemoShellView *memo_shell_view);

/* Private Utilities */

void		e_memo_shell_view_actions_init
					(EMemoShellView *memo_shell_view);
void		e_memo_shell_view_open_memo
					(EMemoShellView *memo_shell_view,
					 ECalModelComponent *comp_data);
void		e_memo_shell_view_set_status_message
					(EMemoShellView *memo_shell_view,
					 const gchar *status_message);
void		e_memo_shell_view_sidebar_update
					(EMemoShellView *memo_shell_view);

G_END_DECLS

#endif /* E_MEMO_SHELL_VIEW_PRIVATE_H */
