/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-disco-folder.c: abstract class for a disconnectable folder */

/* 
 * Authors: Dan Winship <danw@ximian.com>
 *
 * Copyright (C) 2001 Ximian, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "camel-disco-diary.h"
#include "camel-disco-folder.h"
#include "camel-disco-store.h"
#include "camel-exception.h"

#define CF_CLASS(o) (CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS (o)))
#define CDF_CLASS(o) (CAMEL_DISCO_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS (o)))

static CamelFolderClass *parent_class = NULL;

static void disco_refresh_info (CamelFolder *folder, CamelException *ex);
static void disco_sync (CamelFolder *folder, gboolean expunge, CamelException *ex);
static void disco_expunge (CamelFolder *folder, CamelException *ex);

static void disco_append_message (CamelFolder *folder, CamelMimeMessage *message,
				  const CamelMessageInfo *info, CamelException *ex);
static void disco_copy_messages_to (CamelFolder *source, GPtrArray *uids,
				    CamelFolder *destination, CamelException *ex);
static void disco_move_messages_to (CamelFolder *source, GPtrArray *uids,
				    CamelFolder *destination, CamelException *ex);


static void
camel_disco_folder_class_init (CamelDiscoFolderClass *camel_disco_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS (camel_disco_folder_class);

	parent_class = CAMEL_FOLDER_CLASS (camel_type_get_global_classfuncs (camel_folder_get_type ()));

	/* virtual method overload */
	camel_folder_class->refresh_info = disco_refresh_info;
	camel_folder_class->sync = disco_sync;
	camel_folder_class->expunge = disco_expunge;

	camel_folder_class->append_message = disco_append_message;
	camel_folder_class->copy_messages_to = disco_copy_messages_to;
	camel_folder_class->move_messages_to = disco_move_messages_to;
}

CamelType
camel_disco_folder_get_type (void)
{
	static CamelType camel_disco_folder_type = CAMEL_INVALID_TYPE;

	if (camel_disco_folder_type == CAMEL_INVALID_TYPE) {
		camel_disco_folder_type = camel_type_register (
			CAMEL_FOLDER_TYPE, "CamelDiscoFolder",
			sizeof (CamelDiscoFolder),
			sizeof (CamelDiscoFolderClass),
			(CamelObjectClassInitFunc) camel_disco_folder_class_init,
			NULL, NULL, NULL);
	}

	return camel_disco_folder_type;
}


static void
disco_refresh_info (CamelFolder *folder, CamelException *ex)
{
	if (camel_disco_store_status (CAMEL_DISCO_STORE (folder->parent_store)) != CAMEL_DISCO_STORE_ONLINE)
		return;
	CDF_CLASS (folder)->refresh_info_online (folder, ex);
}

static void
disco_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	if (expunge) {
		disco_expunge (folder, ex);
		if (camel_exception_is_set (ex))
			return;
	}

	switch (camel_disco_store_status (CAMEL_DISCO_STORE (folder->parent_store))) {
	case CAMEL_DISCO_STORE_ONLINE:
		CDF_CLASS (folder)->sync_online (folder, ex);
		break;

	case CAMEL_DISCO_STORE_OFFLINE:
		CDF_CLASS (folder)->sync_offline (folder, ex);
		break;
	}
}

static void
disco_expunge_uids (CamelFolder *folder, GPtrArray *uids, CamelException *ex)
{
	CamelDiscoStore *disco = CAMEL_DISCO_STORE (folder->parent_store);

	if (uids->len == 0)
		return;

	switch (camel_disco_store_status (disco)) {
	case CAMEL_DISCO_STORE_ONLINE:
		CDF_CLASS (folder)->expunge_uids_online (folder, uids, ex);
		break;

	case CAMEL_DISCO_STORE_OFFLINE:
		CDF_CLASS (folder)->expunge_uids_offline (folder, uids, ex);
#ifdef NOTYET
		if (!camel_exception_is_set (ex))
			camel_disco_diary_log (disco->diary, CAMEL_DISCO_DIARY_FOLDER_EXPUNGE, folder, uids);
#endif
		break;
	}
}

static void
disco_expunge (CamelFolder *folder, CamelException *ex)
{
	GPtrArray *uids;
	int i, count;
	CamelMessageInfo *info;

	uids = g_ptr_array_new ();
	count = camel_folder_summary_count (folder->summary);
	for (i = 0; i < count; i++) {
		info = camel_folder_summary_index (folder->summary, i);
		if (info->flags & CAMEL_MESSAGE_DELETED)
			g_ptr_array_add (uids, g_strdup (camel_message_info_uid (info)));
		camel_folder_summary_info_free (folder->summary, info);
	}

	disco_expunge_uids (folder, uids, ex);

	for (i = 0; i < uids->len; i++)
		g_free (uids->pdata[i]);
	g_ptr_array_free (uids, TRUE);
}

static void
disco_append_message (CamelFolder *folder, CamelMimeMessage *message,
		      const CamelMessageInfo *info, CamelException *ex)
{
	CamelDiscoStore *disco = CAMEL_DISCO_STORE (folder->parent_store);
	char *uid;

	switch (camel_disco_store_status (disco)) {
	case CAMEL_DISCO_STORE_ONLINE:
		uid = CDF_CLASS (folder)->append_online (folder, message, info, ex);
		break;

	case CAMEL_DISCO_STORE_OFFLINE:
		uid = CDF_CLASS (folder)->append_offline (folder, message, info, ex);
#ifdef NOTYET
		if (uid)
			camel_disco_diary_log (disco->diary, CAMEL_DISCO_DIARY_FOLDER_APPEND, folder, uid);
#endif
		break;
	}
	g_free (uid);
}

static void
disco_copy_messages_to (CamelFolder *source, GPtrArray *uids,
			CamelFolder *destination, CamelException *ex)
{
	CamelDiscoStore *disco = CAMEL_DISCO_STORE (source->parent_store);

	switch (camel_disco_store_status (disco)) {
	case CAMEL_DISCO_STORE_ONLINE:
		CDF_CLASS (source)->copy_online (source, uids, destination, ex);
		break;

	case CAMEL_DISCO_STORE_OFFLINE:
		CDF_CLASS (source)->copy_offline (source, uids, destination, ex);
#ifdef NOTYET
		if (!camel_exception_is_set (ex))
			camel_disco_diary_log (disco->diary, CAMEL_DISCO_DIARY_FOLDER_COPY, source, destination, uids);
#endif
		break;
	}
}

static void
disco_move_messages_to (CamelFolder *source, GPtrArray *uids,
			CamelFolder *destination, CamelException *ex)
{
	CamelDiscoStore *disco = CAMEL_DISCO_STORE (source->parent_store);

	switch (camel_disco_store_status (disco)) {
	case CAMEL_DISCO_STORE_ONLINE:
		CDF_CLASS (source)->move_online (source, uids, destination, ex);
		break;

	case CAMEL_DISCO_STORE_OFFLINE:
		CDF_CLASS (source)->move_offline (source, uids, destination, ex);
#ifdef NOTYET
		if (!camel_exception_is_set (ex))
			camel_disco_diary_log (disco->diary, CAMEL_DISCO_DIARY_FOLDER_MOVE, source, destination, uids);
#endif
		break;
	}
}


/**
 * camel_disco_folder_expunge_uids:
 * @folder: a (disconnectable) folder
 * @uids: array of UIDs to expunge
 * @ex: a CamelException
 *
 * This expunges the messages in @uids from @folder. It should take
 * whatever steps are needed to avoid expunging any other messages,
 * although in some cases it may not be possible to avoid expunging
 * messages that are marked deleted by another client at the same time
 * as the expunge_uids call is running.
 **/
void
camel_disco_folder_expunge_uids (CamelFolder *folder, GPtrArray *uids,
				 CamelException *ex)
{
	disco_expunge_uids (folder, uids, ex);
}
