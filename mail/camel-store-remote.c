/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#include <mail-dbus.h>
#include <evo-dbus.h>
#include <dbind.h>
#include <camel/camel-folder.h>
#include "camel-store-remote.h"
#include "camel-object-remote.h"

extern GHashTable *folder_hash;
GHashTable *folder_rhash = NULL;

CamelObjectRemote *camel_store_get_folder_remote(CamelStoreRemote * store,
					   const char *folder_name,
					   guint32 flags,
					   CamelException *ex)
{
	DBusError error;
	char *err = NULL, *shash = NULL;
	CamelObjectRemote *object = NULL;

	if (!folder_rhash)
		folder_rhash = g_hash_table_new (g_str_hash, g_str_equal);

	dbus_error_init(&error);
	/* Invoke the appropriate dbind call to CamelStoreGetFolder */
	if (dbind_context_method_call(evolution_dbus_peek_context(),
				      CAMEL_DBUS_NAME,
				      CAMEL_STORE_OBJECT_PATH,
				      CAMEL_STORE_INTERFACE,
				      "camel_store_get_folder",
				      &error,
				      "ssu=>ss", store->object_id, folder_name,
				      flags, &shash, &err))
		object = g_hash_table_lookup (folder_rhash, shash);
		if (!object) {
			object = g_new0(CamelObjectRemote, 1);
			object->object_id = shash;
			object->type = CAMEL_RO_FOLDER;
			g_hash_table_insert (folder_rhash, shash, object);
		}
	else {
		g_warning ("camel_store_get_folder error: %s\n", error.message);
		dbus_error_free (&error);
	}
	
	printf("Back folder %p: %s: %p\n", folder_hash, shash, object);

	return object;
}

static CamelObjectRemote *camel_store_get_specific_folder_remote(CamelStoreRemote *
							   store,
							   const char *method)
{
	gboolean ret;
	DBusError error;
	char *err;
	char *fhash;

	if (!folder_rhash)
		folder_rhash = g_hash_table_new (g_str_hash, g_str_equal);

	dbus_error_init(&error);
	/* Invoke the appropriate dbind call to CamelStoreGetFolder */
	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					method,
					&error,
					"s=>ss", store->object_id, &fhash,
					&err);

	if (ret != DBUS_HANDLER_RESULT_HANDLED) {
		return NULL;
	} else {
		CamelObjectRemote *object = g_hash_table_lookup (folder_rhash, fhash);
		if (!object) {
			object = g_new0(CamelObjectRemote, 1);
			object->object_id = fhash;
			object->type = CAMEL_RO_FOLDER;
			g_hash_table_insert (folder_rhash, fhash, object);
		}	
		return object;
	}
}

CamelObjectRemote *camel_store_get_inbox_remote(CamelStoreRemote * store, CamelException *ex)
{
	return (camel_store_get_specific_folder_remote
		(store, "camel_store_get_inbox"));
}

CamelObjectRemote *camel_store_get_trash_remote(CamelStoreRemote * store, CamelException *ex)
{
	return (camel_store_get_specific_folder_remote
		(store, "camel_store_get_trash"));
}

CamelObjectRemote *camel_store_get_junk_remote(CamelStoreRemote * store, CamelException *ex)
{
	return (camel_store_get_specific_folder_remote
		(store, "camel_store_get_junk"));
}

void
camel_store_delete_folder_remote(CamelStoreRemote * store,
				 const char *folder_name, CamelException *ex)
{
	gboolean ret;
	DBusError error;
	char *err;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_store_delete_folder",
					&error,
					"ss=>s", store->object_id, folder_name,
					&err);
}

void
camel_store_rename_folder_remote(CamelStoreRemote * store,
				 const char *old_folder_name,
				 const char *new_folder_name, CamelException *ex)
{
#warning "CamelException needs to be remoted after which the err can be used"
#warning "error should also be used"
	gboolean ret;
	DBusError error;
	char *err;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_store_rename_folder",
					&error,
					"sss=>s", store->object_id,
					old_folder_name, new_folder_name, &err);
}

void camel_store_sync_remote(CamelStoreRemote * store, int expunge, CamelException *ex)
{
	gboolean ret;
	DBusError error;
	char *err;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_store_sync",
					&error,
					"si=>s", store->object_id,
					expunge, &err);
}

gboolean camel_store_supports_subscriptions_remote(CamelStoreRemote * store)
{
	gboolean ret;
	DBusError error;
	int supports;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_store_supports_subscriptions",
					&error,
					"s=>i", store->object_id, &supports);

	return (gboolean) (supports);
}

gboolean camel_store_folder_subscribed_remote(CamelStoreRemote * store,
					      const char *folder_name)
{
	gboolean ret;
	DBusError error;
	int subscribed;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_store_folder_subscribed",
					&error,
					"ss=>i", store->object_id, folder_name,
					&subscribed);

	return (gboolean) (subscribed);
}

void camel_store_subscribe_folder_remote(CamelStoreRemote * store,
					 const char *folder_name, CamelException *ex)
{
	gboolean ret;
	DBusError error;
	char *err;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_store_subscribe_folder",
					&error,
					"ss=>s", store->object_id, folder_name,
					&err);

	return;
}

void camel_store_unsubscribe_folder_remote(CamelStoreRemote * store,
					   const char *folder_name, CamelException *ex)
{
	gboolean ret;
	DBusError error;
	char *err;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_store_unsubscribe_folder",
					&error,
					"ss=>s", store->object_id, folder_name,
					&err);

	return;
}

void camel_store_noop_remote(CamelStoreRemote * store, CamelException *ex)
{
	gboolean ret;
	DBusError error;
	char *err;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_store_noop",
					&error, "s=>s", store->object_id, &err);
	return;
}

int
camel_store_folder_uri_equal_remote(CamelStoreRemote * store, const char *uri0,
				    const char *uri1)
{
	gboolean ret;
	DBusError error;
	int equality;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_store_folder_uri_equal",
					&error,
					"sss=>i", store->object_id, uri0, uri1,
					&equality);

	return equality;
}

void camel_isubscribe_subscribe_remote(CamelStoreRemote * store,
				       const char *folder_name, CamelException *ex)
{
	gboolean ret;
	DBusError error;
	char *err;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_isubscribe_subscribe",
					&error,
					"ss=>s", store->object_id, folder_name,
					&err);

	return;
}

void camel_isubscribe_unsubscribe_remote(CamelStoreRemote * store,
					 const char *folder_name, CamelException *ex)
{
	gboolean ret;
	DBusError error;
	char *err;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_isubscribe_unsubscribe",
					&error,
					"ss=>s", store->object_id, folder_name,
					&err);

	return;
}

guint32 camel_store_get_mode_remote(CamelStoreRemote * store)
{
	gboolean ret;
	DBusError error;
	guint32 mode;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_store_get_mode",
					&error,
					"s=>u", store->object_id, &mode);

	return mode;
}

guint32 camel_store_get_flags_remote(CamelStoreRemote * store)
{
	gboolean ret;
	DBusError error;
	guint32 flags;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_store_get_flags",
					&error,
					"s=>u", store->object_id, &flags);

	return flags;
}

void camel_store_set_mode_remote(CamelStoreRemote * store, guint32 mode)
{
	gboolean ret;
	DBusError error;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_store_set_mode",
					&error, "su", store->object_id, &mode);

	return;
}

void camel_store_set_flags_remote(CamelStoreRemote * store, guint32 flags)
{
	gboolean ret;
	DBusError error;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_store_set_flags",
					&error, "su", store->object_id, flags);

	return;
}

char *camel_store_get_url_remote(CamelStoreRemote *store)
{
	gboolean ret;
	DBusError error;
	char *url;

	g_return_val_if_fail (store != NULL, NULL);

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_store_get_url",
					&error, "s=>s", store->object_id, &url);

	return url;
}

int camel_store_get_url_flags_remote (CamelStoreRemote *store)
{
	gboolean ret;
	DBusError error;
	int url_flags;;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_store_get_url_flags",
					&error, "s=>i", store->object_id, &url_flags);

	return url_flags;
}

CamelFolderInfo * camel_store_get_folder_info_remote (CamelStoreRemote *store, const char *top, guint32 flags, CamelException *ex)
{
	gboolean ret;
	DBusError error;
	gpointer info;
	int ptr = NULL;
	char *err;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_store_get_folder_info",
					&error, "ssu=>is", store->object_id, top ? top : "", flags, &ptr, &error);
	if (!ret) {
		g_warning ("camel_store_get_folder_info_remote: failed : %s\n", error.message);
		return NULL;
	}

	info = (gpointer)ptr;
	
	return info;
}

char * camel_store_get_service_name_remote (CamelStoreRemote *store, gboolean brief)
{
	gboolean ret;
	DBusError error;
	char *name = NULL;

	dbus_error_init(&error);

	ret = dbind_context_method_call(evolution_dbus_peek_context(),
					CAMEL_DBUS_NAME,
					CAMEL_STORE_OBJECT_PATH,
					CAMEL_STORE_INTERFACE,
					"camel_service_get_name",
					&error, "si=>s", store->object_id, brief, &name);

	return name;
}

gint camel_store_get_status_remote (CamelStoreRemote *store)
{
	gboolean ret;
	DBusError error;
	char *name = NULL;
	/* Implement it.*/
	return CAMEL_SERVICE_DISCONNECTED;
}

CamelFolderInfo * camel_store_create_folder_remote (CamelStoreRemote *store, char *parent, char * name, CamelException *ex)
{
	return NULL;
}

int
camel_store_get_state_remote (CamelStoreRemote *store)
{
	abort ();	
	return -1;
}
