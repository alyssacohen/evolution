/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or 
 *  modify it under the terms of the GNU General Public License as 
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 *  USA
 */

#include "camel/camel.h"
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>

#include "message-thread.h"

#define d(x)

int dump_tree(struct _container *c, int depth);

static void
container_add_child(struct _container *node, struct _container *child)
{
	d(printf("\nAdding child %p to parent %p \n", child, node));
	child->next = node->child;
	node->child = child;
	child->parent = node;
}

static void
container_unparent_child(struct _container *child)
{
	struct _container *c, *p = NULL, *last, *node;

	/* are we unparented? */
	if (child->parent == NULL) {
		return;
	}

	/* else remove child from its existing parent, and reparent */
	node = child->parent;
	c = (struct _container *)&node->child;
	d(printf("scanning children:\n"));
	while (c->next) {
		d(printf(" %p\n", c));
	        if (c->next==child) {
			d(printf("found node %p\n", child));
			c->next = c->next->next;
			child->parent = NULL;
			return;
		}
		c = c->next;
	}

	printf("DAMN, we shouldn't  be here!\n");
}

static void
container_parent_child(struct _container *parent, struct _container *child)
{
	struct _container *c, *p = NULL, *last, *node;

	/* are we already the right parent? */
	if (child->parent == parent)
		return;

	/* are we unparented? */
	if (child->parent == NULL) {
		container_add_child(parent, child);
		return;
	}

	/* else remove child from its existing parent, and reparent */
	node = child->parent;
	c = (struct _container *)&node->child;
	d(printf("scanning children:\n"));
	while (c->next) {
		d(printf(" %p\n", c));
	        if (c->next==child) {
			d(printf("found node %p\n", child));
			c->next = c->next->next;
			child->parent = NULL;
			container_add_child(parent, child);
			return;
		}
		c = c->next;
	}

	printf("DAMN, we shouldn't  be here!\n");
}

static void
prune_empty(struct _container **cp)
{
	struct _container *child, *next, *nextc, *c, *lastc;

	/* yes, this is intentional */
	lastc = (struct _container *)cp;
	while (lastc->next) {
		c = lastc->next;

		d(printf("checking message %p %p (%s)\n", c,
			 c->message, c->message?c->message->message_id:"<empty>"));
		if (c->message == NULL) {
			if (c->child == NULL) {
				d(printf("removing empty node\n"));
				lastc->next = c->next;
				continue;
			}
			if (c->parent || c->child->next==0) {
				d(printf("promoting child\n"));
				lastc->next = c->next; /* remove us */
				child = c->child;
				while (child) {
					next = child->next;

					child->parent = c->parent;
					child->next = lastc->next;
					lastc->next = child;

					child = next;
				}
				continue;
			}
		}
		prune_empty(&c->child);
		lastc = c;
	}
}

static void
hashloop(void *key, void *value, void *data)
{
	struct _container *c = value;
	char *keystr = key;
	struct _container *tail = data;

	if (c->parent == NULL) {
		c->next = tail->next;
		tail->next = c;
	}
}

static char *
get_root_subject(struct _container *c, int *re)
{
	char *s, *p;
	struct _container *scan;
	
	s = NULL;
	*re = FALSE;
	if (c->message)
		s = c->message->subject;
	else {
		/* one of the children will always have a message */
		scan = c->child;
		while (scan) {
			if (scan->message) {
				s = scan->message->subject;
				break;
			}
			scan = scan->next;
		}
	}
	if (s != NULL) {
		while (*s) {
			char cc;
			while (isspace(*s))
				s++;
			if (s[0] == 0)
				break;
			if ((s[0] == 'r' || s[0]=='R')
			    && (s[1] == 'e' || s[1]=='E')) {
				p = s+2;
				while (ispunct(*p) || isdigit(*p) && *p != ':')
					p++;
				if (*p==':') {
					*re = TRUE;
					s = p+1;
				} else
					break;
			} else
				break;
		}
		if (*s)
			return s;
	}
	return NULL;
}

/* this is pretty slow, but not used often */
static void
remove_node(struct _container **list, struct _container *node)
{
	struct _container *c;

	/* this is intentional, even if it looks funny */
	c = (struct _container *)list;
	while (c->next) {
		if (c->next == node) {
			c->next = c->next->next;
			break;
		}
		c = c->next;
	}
}

static void
group_root_set(struct _container **cp)
{
	GHashTable *subject_table = g_hash_table_new(g_str_hash, g_str_equal);
	struct _container *c, *clast, *scan, *container;
	char *s;
	int re;

	/* gather subject lines */ 
	d(printf("gathering subject lines\n"));
	clast = (struct _container *)cp;
	c = clast->next;
	while (c) {
		c->root_subject = get_root_subject(c, &c->re);
		if (c->root_subject) {
			container = g_hash_table_lookup(subject_table, c->root_subject);
			if (container == NULL
			    || (container->message == NULL && c->message)
			    || (container->re == TRUE && !c->re)) {
				g_hash_table_insert(subject_table, c->root_subject, c);
			}
		}
		c = c->next;
	}

	/* merge common subjects? */
	clast = (struct _container *)cp;
	while (clast->next) {
		c = clast->next;
		d(printf("checking %p %s\n", c, c->root_subject));
		if (c->root_subject
		    && (container = g_hash_table_lookup(subject_table, c->root_subject))
		    && (container != c)) {
			d(printf(" matching %p %s\n", container, container->root_subject));
			if (c->message == NULL && container->message == NULL) {
				d(printf("merge containers children\n"));
				/* steal the children from c onto container, and unlink c */
				scan = (struct _container *)&container->child;
				while (scan->next)
					scan = scan->next;
				scan->next = c->child;
				clast->next = c->next;
				continue;
			} if (c->message == NULL && container->message != NULL) {
				d(printf("container is non-empty parent\n"));
				remove_node(cp, container);
				container_add_child(c, container);
			} else if (c->message != NULL && container->message == NULL) {
				d(printf("container is empty child\n"));
				clast->next = c->next;
				container_add_child(container, c);
				continue;
			} else if (c->re && !container->re) {
				d(printf("container is re\n"));
				clast->next = c->next;
				container_add_child(container, c);
				continue;
			} else if (!c->re && container->re) {
				d(printf("container is not re\n"));
				remove_node(cp, container);
				container_add_child(c, container);
			} else {
				d(printf("subjects are common %p and %p\n", c, container));

				remove_node(cp, container);

				scan = g_malloc0(sizeof(*scan));
				scan->root_subject = c->root_subject;
				scan->re = c->re && container->re;
				scan->next = c->next;
				clast->next = scan;
				container_add_child(scan, c);
				container_add_child(scan, container);
				clast = scan;
				g_hash_table_insert(subject_table, scan->root_subject, scan);
				continue;
			}
		}
		clast = c;
	}
	g_hash_table_destroy(subject_table);
}

int
dump_tree(struct _container *c, int depth)
{
	char *p;
	struct _container *child;
	int count=0;

	p = alloca(depth*2+1);
	memset(p, ' ', depth*2);
	p[depth*2] = 0;

	while (c) {
		if (c->message) {
			printf("%s %p Subject: %s <%s>\n", p, c, c->message->subject, c->message->message_id);
			count += 1;
		} else {
			printf("%s %p <empty>\n", p, c);
		}
		if (c->child)
			count += dump_tree(c->child, depth+1);
		c = c->next;
	}
	return count;
}

void thread_messages_free(struct _container *head)
{
	printf("FIXME: Free message thread structure\n");
}

struct _container *
thread_messages(CamelMessageInfo **messages, int count)
{
	GHashTable *id_table;
	int i, msgs;
	struct _container *c, *p, *child, *head, *scan, *container;
	struct _header_references *ref;

	id_table = g_hash_table_new(g_str_hash, g_str_equal);
	for (i=0;i<count;i++) {
		CamelMessageInfo *mi;
		mi = messages[i];
		if (mi->message_id) {
			d(printf("doing : %s\n", mi->message_id));
			c = g_hash_table_lookup(id_table, mi->message_id);
			if (c) {
				c->message = mi;
			} else {
				c = g_malloc0(sizeof(*c));
				c->message = mi;
				g_hash_table_insert(id_table, mi->message_id, c);
			}
			container = c;
			ref = mi->references;
			p = NULL;
			child = container;
			head = NULL;
			d(printf("referencfes: "));
			while (ref) {
				d(printf(" %s\n", ref->id));
				if (ref->id == NULL) {
					printf("FUCK!?\n");
					ref = ref->next;
					continue;
				}
				d(printf("looking up reference: %s\n", ref->id));
				c = g_hash_table_lookup(id_table, ref->id);
				if (c == NULL) {
					d(printf("not found\n"));
					c = g_malloc0(sizeof(*c));
					g_hash_table_insert(id_table, ref->id, c);
				}
				if (c!=child) {
					container_parent_child(c, child);
				}
				child = c;
				if (head == NULL)
					head = c;
				ref = ref->next;
			}
			d(printf("\n"));
		} else {
			printf("No message id???\n");
			/* ?? */
		}
	}

	d(printf("\n\n"));
	/* build a list of root messages (no parent) */
	head = NULL;
	g_hash_table_foreach(id_table, hashloop, &head);

	g_hash_table_destroy(id_table);

	/* remove empty parent nodes */
	prune_empty(&head);

	/* find any siblings which missed out */
	group_root_set(&head);

#if 0
	printf("finished\n");
	i = dump_tree(head, 0);
	printf("%d count, %d msgs initially, %d items in tree\n", count, msgs, i);
#endif

	return head;
}

#ifdef STANDALONE

static char *
auth_callback(char *prompt, gboolean secret,
	      CamelService *service, char *item,
	      CamelException *ex)
{
	printf ("auth_callback called: %s\n", prompt);
	return NULL;
}

int
main (int argc, char**argv)
{
	CamelSession *session;
	CamelException *ex;
	CamelStore *store;
	gchar *store_url = "mbox:///home/notzed/evolution/local/Inbox";
	CamelFolder *folder;
	CamelMimeMessage *message;
	GList *uid_list;
	GPtrArray *summary;

	gtk_init (&argc, &argv);
	camel_init ();		
	ex = camel_exception_new ();
	
	session = camel_session_new (auth_callback);
	store = camel_session_get_store (session, store_url, ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exception caught in camel_session_get_store\n"
			"Full description : %s\n", camel_exception_get_description (ex));
		return -1;
	}

	folder = camel_store_get_folder (store, "mbox", TRUE, ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exception caught in camel_store_get_folder\n"
			"Full description : %s\n", camel_exception_get_description (ex));
		return -1;
	}

#if 0
	camel_folder_open (folder, FOLDER_OPEN_RW, ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exception caught when trying to open the folder\n"
			"Full description : %s\n", camel_exception_get_description (ex));
		return -1;
	}
#endif

	summary = camel_folder_get_summary(folder, ex);
	thread_messages((CamelMessageInfo **)summary->pdata, summary->len);

	return 0;
}

#endif

/*

  msgid: d
  references: a b c

  msgid: f
  references: c d

  msgid: e
  references: c

  a
   \
    b
     \
      c
       \
        d
        |\
        e f
 */
/*
  lookup d
    create new node d
  child = d
  loop on c b a
    lookup node?
    if no node, create node
    add child to node
    child = node
  endloop

 */
