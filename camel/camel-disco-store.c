/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-disco-store.c: abstract class for a disconnectable remote store */

/*
 *  Authors: Dan Winship <danw@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "camel-disco-store.h"
#include "camel-exception.h"

#define CDS_CLASS(o) (CAMEL_DISCO_STORE_CLASS (CAMEL_OBJECT_GET_CLASS (o)))

static CamelRemoteStoreClass *remote_store_class = NULL;

static gboolean disco_connect (CamelService *service, CamelException *ex);
static gboolean disco_disconnect (CamelService *service, gboolean clean, CamelException *ex);
static CamelFolder *disco_get_folder (CamelStore *store, const char *name,
				      guint32 flags, CamelException *ex);
static CamelFolderInfo *disco_get_folder_info (CamelStore *store,
					       const char *top, guint32 flags,
					       CamelException *ex);

static void
camel_disco_store_class_init (CamelDiscoStoreClass *camel_disco_store_class)
{
	CamelServiceClass *camel_service_class =
		CAMEL_SERVICE_CLASS (camel_disco_store_class);
	CamelStoreClass *camel_store_class =
		CAMEL_STORE_CLASS (camel_disco_store_class);

	remote_store_class = CAMEL_REMOTE_STORE_CLASS (camel_type_get_global_classfuncs (camel_remote_store_get_type ()));

	/* virtual method overload */
	camel_service_class->connect = disco_connect;
	camel_service_class->disconnect = disco_disconnect;

	camel_store_class->get_folder = disco_get_folder;
	camel_store_class->get_folder_info = disco_get_folder_info;
}

static void
camel_disco_store_init (CamelDiscoStore *store)
{
	/* Hack */
	if (getenv ("CAMEL_OFFLINE"))
		store->status = CAMEL_DISCO_STORE_OFFLINE;
	else
		store->status = CAMEL_DISCO_STORE_ONLINE;
}

CamelType
camel_disco_store_get_type (void)
{
	static CamelType camel_disco_store_type = CAMEL_INVALID_TYPE;

	if (camel_disco_store_type == CAMEL_INVALID_TYPE) {
		camel_disco_store_type = camel_type_register (
			CAMEL_REMOTE_STORE_TYPE, "CamelDiscoStore",
			sizeof (CamelDiscoStore),
			sizeof (CamelDiscoStoreClass),
			(CamelObjectClassInitFunc) camel_disco_store_class_init,
			NULL,
			(CamelObjectInitFunc) camel_disco_store_init,
			NULL);
	}

	return camel_disco_store_type;
}

static gboolean
disco_connect (CamelService *service, CamelException *ex)
{
	CamelDiscoStore *store = CAMEL_DISCO_STORE (service);

	if (!CAMEL_SERVICE_CLASS (remote_store_class)->connect (service, ex))
		return FALSE;

	switch (camel_disco_store_status (store)) {
	case CAMEL_DISCO_STORE_ONLINE:
		return CDS_CLASS (service)->connect_online (service, ex);

	case CAMEL_DISCO_STORE_OFFLINE:
		return CDS_CLASS (service)->connect_offline (service, ex);
	}

	/* Not reached */
	return TRUE;
}

static gboolean
disco_disconnect (CamelService *service, gboolean clean, CamelException *ex)
{
	CamelDiscoStore *store = CAMEL_DISCO_STORE (service);

	switch (camel_disco_store_status (store)) {
	case CAMEL_DISCO_STORE_ONLINE:
		if (!CDS_CLASS (service)->disconnect_online (service, clean, ex))
			return FALSE;
		break;

	case CAMEL_DISCO_STORE_OFFLINE:
		if (!CDS_CLASS (service)->disconnect_offline (service, clean, ex))
			return FALSE;
		break;
	}

	return CAMEL_SERVICE_CLASS (remote_store_class)->disconnect (service, clean, ex);
}

static CamelFolder *
disco_get_folder (CamelStore *store, const char *name,
		  guint32 flags, CamelException *ex)
{
	CamelDiscoStore *disco_store = CAMEL_DISCO_STORE (store);

	switch (camel_disco_store_status (disco_store)) {
	case CAMEL_DISCO_STORE_ONLINE:
		return CDS_CLASS (store)->get_folder_online (store, name, flags, ex);

	case CAMEL_DISCO_STORE_OFFLINE:
		return CDS_CLASS (store)->get_folder_offline (store, name, flags, ex);
	}

	/* Not reached */
	return NULL;
}

static CamelFolderInfo *
disco_get_folder_info (CamelStore *store, const char *top,
		       guint32 flags, CamelException *ex)
{
	CamelDiscoStore *disco_store = CAMEL_DISCO_STORE (store);

	switch (camel_disco_store_status (disco_store)) {
	case CAMEL_DISCO_STORE_ONLINE:
		return CDS_CLASS (store)->get_folder_info_online (store, top, flags, ex);

	case CAMEL_DISCO_STORE_OFFLINE:
		/* Can't edit subscriptions while offline */
		if ((store->flags & CAMEL_STORE_SUBSCRIPTIONS) &&
		    !(flags & CAMEL_STORE_FOLDER_INFO_SUBSCRIBED)) {
			camel_disco_store_check_online (disco_store, ex);
			return NULL;
		}

		return CDS_CLASS (store)->get_folder_info_offline (store, top, flags, ex);
	}

	/* Not reached */
	return NULL;
}


CamelDiscoStoreStatus
camel_disco_store_status (CamelDiscoStore *store)
{
	g_return_val_if_fail (CAMEL_IS_DISCO_STORE (store), CAMEL_DISCO_STORE_ONLINE);

	return store->status;
}

gboolean
camel_disco_store_check_online (CamelDiscoStore *store, CamelException *ex)
{
	if (camel_disco_store_status (store) != CAMEL_DISCO_STORE_ONLINE) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				     _("You must be working online to "
				       "complete this operation"));
		return FALSE;
	}

	return TRUE;
}
