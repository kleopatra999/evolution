/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-nntp-folder.c : Abstract class for an email folder */

/* 
 * Author : Chris Toshok <toshok@helixcode.com> 
 *
 * Copyright (C) 2000 Helix Code .
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


#include <config.h> 

#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "camel-folder-summary.h"
#include "camel-nntp-resp-codes.h"
#include "camel-nntp-store.h"
#include "camel-nntp-folder.h"
#include "camel-nntp-store.h"
#include "camel-nntp-utils.h"

#include "string-utils.h"
#include "camel-stream-mem.h"
#include "camel-data-wrapper.h"
#include "camel-mime-message.h"
#include "camel-folder-summary.h"

#include "camel-exception.h"

static CamelFolderClass *parent_class=NULL;

/* Returns the class for a CamelNNTPFolder */
#define CNNTPF_CLASS(so) CAMEL_NNTP_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CNNTPS_CLASS(so) CAMEL_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))


static void
nntp_refresh_info (CamelFolder *folder, CamelException *ex)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);

	/* load the summary if we have that ability */
	if (folder->has_summary_capability) {
		const gchar *root_dir_path;

		root_dir_path = camel_nntp_store_get_toplevel_dir (CAMEL_NNTP_STORE(folder->parent_store));

		nntp_folder->summary_file_path = g_strdup_printf ("%s/%s-ev-summary",
							  root_dir_path,
							  folder->name);

		nntp_folder->summary = camel_folder_summary_new ();
		camel_folder_summary_set_filename (nntp_folder->summary,
						   nntp_folder->summary_file_path);

		if (-1 == camel_folder_summary_load (nntp_folder->summary)) {
			/* Bad or nonexistant summary file */
			camel_nntp_get_headers (CAMEL_FOLDER( folder )->parent_store,
						nntp_folder, ex);
			if (camel_exception_get_id (ex))
				return;

			/* XXX check return value */
			camel_folder_summary_save (nntp_folder->summary);
		}
	}
}

static void
nntp_folder_sync (CamelFolder *folder, gboolean expunge, 
		  CamelException *ex)
{
	CamelNNTPStore *store;

	camel_folder_summary_save (CAMEL_NNTP_FOLDER(folder)->summary);

	store = CAMEL_NNTP_STORE (camel_folder_get_parent_store (folder));

	if (store->newsrc)
		camel_nntp_newsrc_write (store->newsrc);
}

static gint
nntp_folder_get_message_count (CamelFolder *folder)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER(folder);

	g_assert (folder);
	g_assert (nntp_folder->summary);

        return camel_folder_summary_count(nntp_folder->summary);
}

static guint32
nntp_folder_get_message_flags (CamelFolder *folder, const char *uid)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);
	CamelMessageInfo *info = camel_folder_summary_uid (nntp_folder->summary, uid);

	return info->flags;
}

static void
nntp_folder_set_message_flags (CamelFolder *folder, const char *uid,
			       guint32 flags, guint32 set)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);
	CamelMessageInfo *info = camel_folder_summary_uid (nntp_folder->summary, uid);

	info->flags = set;

	if (set & CAMEL_MESSAGE_SEEN) {
		int article_num;
		CamelNNTPStore *nntp_store = CAMEL_NNTP_STORE (camel_folder_get_parent_store (folder));

		sscanf (uid, "%d", &article_num);

		camel_nntp_newsrc_mark_article_read (nntp_store->newsrc,
						     folder->name,
						     article_num);
	}

	camel_folder_summary_touch (nntp_folder->summary);
}

static CamelMimeMessage *
nntp_folder_get_message (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	CamelStream *message_stream = NULL;
	CamelMimeMessage *message = NULL;
	CamelStore *parent_store;
	char *buf;
	int buf_len;
	int buf_alloc;
	int status;
	gboolean done;
	char *message_id;

	/* get the parent store */
	parent_store = camel_folder_get_parent_store (folder);

	message_id = strchr (uid, ',') + 1;
	status = camel_nntp_command (CAMEL_NNTP_STORE( parent_store ), ex, NULL, "ARTICLE %s", message_id);

	/* if the message_id was not found, raise an exception and return */
	if (status == NNTP_NO_SUCH_ARTICLE) {
		camel_exception_setv (ex, 
				     CAMEL_EXCEPTION_FOLDER_INVALID_UID,
				     "message %s not found.",
				      message_id);
		return NULL;
	}
	else if (status != NNTP_ARTICLE_FOLLOWS) {
		/* XXX */
		g_warning ("weird nntp error %d\n", status);
		return NULL;
	}

	/* XXX ick ick ick.  read the entire message into a buffer and
	   then create a stream_mem for it. */
	buf_alloc = 2048;
	buf_len = 0;
	buf = g_malloc(buf_alloc);
	done = FALSE;

	buf[0] = 0;

	while (!done) {
		int line_length;
		char *line;

		if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (parent_store), &line, ex) < 0) {
			g_error ("recv_line failed while building message\n");
			break;
		}

		/* XXX check exception */

		line_length = strlen ( line );

		if (!strcmp(line, ".")) {
			done = TRUE;
			g_free (line);
		}
		else {
			if (buf_len + line_length > buf_alloc) {
				buf_alloc *= 2;
				buf = g_realloc (buf, buf_alloc);
			}
			strcat(buf, line);
			strcat(buf, "\n");
			buf_len += strlen(line) + 1;
			g_free (line);
		}
	}

	/* create a stream bound to the message */
	message_stream = camel_stream_mem_new_with_buffer(buf, buf_len);

	message = camel_mime_message_new ();
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER(message), message_stream);

	camel_object_unref (CAMEL_OBJECT (message_stream));

#if 0
	gtk_signal_connect (CAMEL_OBJECT (message), "message_changed", message_changed, folder);
#endif

	g_free (buf);

	return message;
}

static GPtrArray *
nntp_folder_get_uids (CamelFolder *folder)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);
	GPtrArray *out;
	CamelMessageInfo *message_info;
	int i;
	int count = camel_folder_summary_count (nntp_folder->summary);

	out = g_ptr_array_new ();
	g_ptr_array_set_size (out, count);
	
	for (i = 0; i < count; i++) {
		message_info = camel_folder_summary_index (nntp_folder->summary, i);
		out->pdata[i] = g_strdup (message_info->uid);
	}
	
	return out;
}

static GPtrArray *
nntp_folder_get_summary (CamelFolder *folder)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);

	return nntp_folder->summary->messages;
}

static GPtrArray *
nntp_folder_get_subfolder_info (CamelFolder *folder)
{
	CamelNNTPNewsrc *newsrc;
	GPtrArray *names, *info;
	CamelFolderInfo *fi;
	int i;
		
	/* Only top-level folder has subfolders. */
	if (*folder->name)
		return NULL;

	newsrc = CAMEL_NNTP_STORE (camel_folder_get_parent_store (folder))->newsrc;
	if (!newsrc)
		return NULL;

	info = g_ptr_array_new ();
	names = camel_nntp_newsrc_get_subscribed_group_names (newsrc);
	for (i = 0; i < names->len; i++) {
		fi = g_new (CamelFolderInfo, 1);
		fi->name = fi->full_name = names->pdata[i];
		/* FIXME */
		fi->message_count = 0;
		fi->unread_message_count = 0;
		g_ptr_array_add (info, fi);
	}
	camel_nntp_newsrc_free_group_names (newsrc, names);
	
	return info;
}

static GPtrArray*
nntp_folder_search_by_expression (CamelFolder *folder, const char *expression, CamelException *ex)
{
	g_assert (0);
	return NULL;
}

static const CamelMessageInfo*
nntp_folder_get_message_info (CamelFolder *folder, const char *uid)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);

	return camel_folder_summary_uid (nntp_folder->summary, uid);
}

static void           
nntp_folder_finalize (CamelObject *object)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (object);

	g_free (nntp_folder->summary_file_path);
}

static void
camel_nntp_folder_class_init (CamelNNTPFolderClass *camel_nntp_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS (camel_nntp_folder_class);

	parent_class = CAMEL_FOLDER_CLASS (camel_type_get_global_classfuncs (camel_folder_get_type ()));
		
	/* virtual method definition */

	/* virtual method overload */
	camel_folder_class->refresh_info = nntp_refresh_info;
	camel_folder_class->sync = nntp_folder_sync;
	camel_folder_class->get_message_count = nntp_folder_get_message_count;
	camel_folder_class->set_message_flags = nntp_folder_set_message_flags;
	camel_folder_class->get_message_flags = nntp_folder_get_message_flags;
	camel_folder_class->get_message = nntp_folder_get_message;
	camel_folder_class->get_uids = nntp_folder_get_uids;
	camel_folder_class->free_uids = camel_folder_free_deep;
	camel_folder_class->get_summary = nntp_folder_get_summary;
	camel_folder_class->free_summary = camel_folder_free_nop;
	camel_folder_class->get_subfolder_info = nntp_folder_get_subfolder_info;
	camel_folder_class->free_subfolder_info = camel_folder_free_deep;
	camel_folder_class->search_by_expression = nntp_folder_search_by_expression;
	camel_folder_class->get_message_info = nntp_folder_get_message_info;
}

CamelType
camel_nntp_folder_get_type (void)
{
	static CamelType camel_nntp_folder_type = CAMEL_INVALID_TYPE;
	
	if (camel_nntp_folder_type == CAMEL_INVALID_TYPE)	{
		camel_nntp_folder_type = camel_type_register (CAMEL_FOLDER_TYPE, "CamelNNTPFolder",
							      sizeof (CamelNNTPFolder),
							      sizeof (CamelNNTPFolderClass),
							      (CamelObjectClassInitFunc) camel_nntp_folder_class_init,
							      NULL,
							      (CamelObjectInitFunc) NULL,
							      (CamelObjectFinalizeFunc) nntp_folder_finalize);
	}
	
	return camel_nntp_folder_type;
}

CamelFolder *
camel_nntp_folder_new (CamelStore *parent, const char *folder_name, CamelException *ex)
{
	CamelFolder *folder = CAMEL_FOLDER (camel_object_new (CAMEL_NNTP_FOLDER_TYPE));

	camel_folder_construct (folder, parent, folder_name, folder_name);

	/* set flags */
	if (!*folder->name) {
		/* the root folder is the only folder that has "subfolders" */
		folder->can_hold_folders = TRUE;
		folder->can_hold_messages = FALSE;
	}
	else {
		folder->can_hold_folders = FALSE;
		folder->can_hold_messages = TRUE;
		folder->has_summary_capability = TRUE;
	}

	camel_folder_refresh_info (folder, ex);
	if (camel_exception_is_set (ex)) {
		camel_object_unref (CAMEL_OBJECT (folder));
		folder = NULL;
	}
	return folder;
}
