/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-storage-set.c
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <gtk/gtkobject.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktypeutils.h>

#include <gal/util/e-util.h>

#include "e-storage-set-view.h"
#include "e-storage-set.h"


#define PARENT_TYPE GTK_TYPE_OBJECT

static GtkObjectClass *parent_class = NULL;

/* This is just to make GHashTable happy.  */
struct _NamedStorage {
	char *name;
	EStorage *storage;
};
typedef struct _NamedStorage NamedStorage;

struct _EStorageSetPrivate {
	GList *storages;	/* EStorage */
	GHashTable *name_to_named_storage;

	EFolderTypeRegistry *folder_type_registry;
};

enum {
	NEW_STORAGE,
	REMOVED_STORAGE,
	NEW_FOLDER,
	UPDATED_FOLDER,
	REMOVED_FOLDER,
	CLOSE_FOLDER,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


static NamedStorage *
named_storage_new (EStorage *storage)
{
	NamedStorage *new;

	new = g_new (NamedStorage, 1);
	new->name    = g_strdup (e_storage_get_name (storage));
	new->storage = storage;

	return new;
}

static void
named_storage_destroy (NamedStorage *named_storage)
{
	g_free (named_storage->name);
	g_free (named_storage);
}

static gboolean
name_to_named_storage_foreach_destroy (void *key,
				       void *value,
				       void *user_data)
{
	NamedStorage *named_storage;

	named_storage = (NamedStorage *) value;
	named_storage_destroy (named_storage);

	return TRUE;
}


/* "Callback converter", from `EStorageResultCallback' to
   `EStorageSetResultCallback'.  */

struct _StorageCallbackConverterData {
	EStorageSet *storage_set;
	EStorageSetResultCallback storage_set_result_callback;
	void *data;
};
typedef struct _StorageCallbackConverterData StorageCallbackConverterData;

static StorageCallbackConverterData *
storage_callback_converter_data_new (EStorageSet *storage_set,
				     EStorageSetResultCallback callback,
				     void *data)
{
	StorageCallbackConverterData *new;

	new = g_new (StorageCallbackConverterData, 1);
	new->storage_set                 = storage_set;
	new->storage_set_result_callback = callback;
	new->data                        = data;

	return new;
}

static void
storage_callback_converter (EStorage *storage,
			    EStorageResult result,
			    void *data)
{
	StorageCallbackConverterData *converter_data;

	converter_data = (StorageCallbackConverterData *) data;

	(* converter_data->storage_set_result_callback) (converter_data->storage_set,
							 result,
							 converter_data->data);

	g_free (converter_data);
}


/* Handling for signals coming from the EStorages.  */

static char *
make_full_path (EStorage *storage,
		const char *path)
{
	const char *storage_name;
	char *full_path;

	storage_name = e_storage_get_name (storage);

	if (strcmp (path, G_DIR_SEPARATOR_S) == 0)
		full_path = g_strconcat (G_DIR_SEPARATOR_S, storage_name,
					 NULL);
	else if (! g_path_is_absolute (path))
		full_path = g_strconcat (G_DIR_SEPARATOR_S, storage_name,
					 G_DIR_SEPARATOR_S, path, NULL);
	else
		full_path = g_strconcat (G_DIR_SEPARATOR_S, storage_name,
					 path, NULL);

	return full_path;
}

static void
storage_new_folder_cb (EStorage *storage,
		       const char *path,
		       void *data)
{
	EStorageSet *storage_set;
	char *full_path;

	storage_set = E_STORAGE_SET (data);

	full_path = make_full_path (storage, path);
	gtk_signal_emit (GTK_OBJECT (storage_set), signals[NEW_FOLDER], full_path);
	g_free (full_path);
}

static void
storage_updated_folder_cb (EStorage *storage,
			   const char *path,
			   void *data)
{
	EStorageSet *storage_set;
	char *full_path;

	storage_set = E_STORAGE_SET (data);

	full_path = make_full_path (storage, path);
	gtk_signal_emit (GTK_OBJECT (storage_set), signals[UPDATED_FOLDER], full_path);
	g_free (full_path);
}

static void
storage_removed_folder_cb (EStorage *storage,
			   const char *path,
			   void *data)
{
	EStorageSet *storage_set;
	char *full_path;

	storage_set = E_STORAGE_SET (data);

	full_path = make_full_path (storage, path);
	gtk_signal_emit (GTK_OBJECT (storage_set), signals[REMOVED_FOLDER], full_path);
	g_free (full_path);
}

static void
storage_close_folder_cb (EStorage *storage,
			 const char *path,
			 void *data)
{
	EStorageSet *storage_set;
	char *full_path;

	storage_set = E_STORAGE_SET (data);

	full_path = make_full_path (storage, path);
	gtk_signal_emit (GTK_OBJECT (storage_set), signals[CLOSE_FOLDER], full_path);
	g_free (full_path);
}


static EStorage *
get_storage_for_path (EStorageSet *storage_set,
		      const char *path,
		      const char **subpath_return)
{
	EStorage *storage;
	char *storage_name;
	const char *first_separator;

	g_return_val_if_fail (g_path_is_absolute (path), NULL);
	g_return_val_if_fail (path[1] != G_DIR_SEPARATOR, NULL);

	/* Skip initial separator.  */
	path++;

	first_separator = strchr (path, G_DIR_SEPARATOR);

	if (first_separator == NULL || first_separator[1] == 0) {
		storage = e_storage_set_get_storage (storage_set, path);
		*subpath_return = G_DIR_SEPARATOR_S;
	} else {
		storage_name = g_strndup (path, first_separator - path);
		storage = e_storage_set_get_storage (storage_set, storage_name);
		g_free (storage_name);

		*subpath_return = first_separator;
	}

	return storage;
}

static void
signal_new_folder_for_all_folders_under_paths (EStorageSet *storage_set,
					       EStorage *storage,
					       GList *path_list)
{
	GList *p;

	for (p = path_list; p != NULL; p = p->next) {
		GList *sub_path_list;
		const char *path;
		char *path_with_storage;

		path = (const char *) p->data;

		path_with_storage = g_strconcat (G_DIR_SEPARATOR_S, e_storage_get_name (storage), path, NULL);
		gtk_signal_emit (GTK_OBJECT (storage_set), signals[NEW_FOLDER], path_with_storage);
		g_free (path_with_storage);

		sub_path_list = e_storage_get_subfolder_paths (storage, path);

		signal_new_folder_for_all_folders_under_paths (storage_set, storage, sub_path_list);

		e_free_string_list (sub_path_list);
	}
}

static void
signal_new_folder_for_all_folders_in_storage (EStorageSet *storage_set,
					      EStorage *storage)
{
	GList *path_list;

	path_list = e_storage_get_subfolder_paths (storage, G_DIR_SEPARATOR_S);

	signal_new_folder_for_all_folders_under_paths (storage_set, storage, path_list);

	e_free_string_list (path_list);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EStorageSet *storage_set;
	EStorageSetPrivate *priv;

	storage_set = E_STORAGE_SET (object);
	priv = storage_set->priv;

	e_free_object_list (priv->storages);

	gtk_object_unref (GTK_OBJECT (priv->folder_type_registry));

	g_hash_table_foreach (priv->name_to_named_storage,
			      (GHFunc) name_to_named_storage_foreach_destroy, NULL);
	g_hash_table_destroy (priv->name_to_named_storage);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EStorageSetClass *klass)
{
	GtkObjectClass *object_class;

	parent_class = gtk_type_class (gtk_object_get_type ());
	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = destroy;

	signals[NEW_STORAGE] = 
		gtk_signal_new ("new_storage",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EStorageSetClass, new_storage),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);
	signals[REMOVED_STORAGE] = 
		gtk_signal_new ("removed_storage",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EStorageSetClass, removed_storage),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);
	signals[NEW_FOLDER] = 
		gtk_signal_new ("new_folder",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EStorageSetClass, new_folder),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);
	signals[UPDATED_FOLDER] = 
		gtk_signal_new ("updated_folder",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EStorageSetClass, updated_folder),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);
	signals[REMOVED_FOLDER] = 
		gtk_signal_new ("removed_folder",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EStorageSetClass, removed_folder),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);
	signals[CLOSE_FOLDER] = 
		gtk_signal_new ("close_folder",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EStorageSetClass, close_folder),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EStorageSet *storage_set)
{
	EStorageSetPrivate *priv;

	g_return_if_fail (E_IS_STORAGE_SET (storage_set));

	priv = g_new (EStorageSetPrivate, 1);
	priv->storages              = NULL;
	priv->name_to_named_storage = g_hash_table_new (g_str_hash, g_str_equal);
	priv->folder_type_registry  = NULL;

	storage_set->priv = priv;
}


void
e_storage_set_construct (EStorageSet *storage_set,
			 EFolderTypeRegistry *folder_type_registry)
{
	g_return_if_fail (storage_set != NULL);
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));

	GTK_OBJECT_UNSET_FLAGS (storage_set, GTK_FLOATING);

	gtk_object_ref (GTK_OBJECT (folder_type_registry));
	storage_set->priv->folder_type_registry = folder_type_registry;
}

EStorageSet *
e_storage_set_new (EFolderTypeRegistry *folder_type_registry)
{
	EStorageSet *new;

	new = gtk_type_new (e_storage_set_get_type ());

	e_storage_set_construct (new, folder_type_registry);

	return new;
}


GList *
e_storage_set_get_storage_list (EStorageSet *storage_set)
{
	EStorageSetPrivate *priv;
	GList *list;
	GList *p;

	g_return_val_if_fail (storage_set != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);

	priv = storage_set->priv;

	list = NULL;
	for (p = priv->storages; p != NULL; p = p->next) {
		gtk_object_ref (GTK_OBJECT (p->data));
		list = g_list_prepend (list, p->data);
	}

	return g_list_reverse (list); /* Lame.  */
}

/**
 * e_storage_set_add_storage:
 * @storage_set: 
 * @storage: 
 * 
 * Add @storage to @storage_set.  Notice that will ref the storage.
 **/
gboolean
e_storage_set_add_storage (EStorageSet *storage_set,
			   EStorage *storage)
{
	EStorageSetPrivate *priv;
	const char *storage_name;
	NamedStorage *named_storage;

	g_return_val_if_fail (storage_set != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), FALSE);
	g_return_val_if_fail (storage != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE (storage), FALSE);

	priv = storage_set->priv;

	storage_name = e_storage_get_name (storage);
	if (g_hash_table_lookup (priv->name_to_named_storage, storage_name) != NULL)
		return FALSE;

	gtk_object_ref (GTK_OBJECT (storage));

	gtk_signal_connect (GTK_OBJECT (storage), "new_folder",
			    GTK_SIGNAL_FUNC (storage_new_folder_cb), storage_set);
	gtk_signal_connect (GTK_OBJECT (storage), "updated_folder",
			    GTK_SIGNAL_FUNC (storage_updated_folder_cb), storage_set);
	gtk_signal_connect (GTK_OBJECT (storage), "removed_folder",
			    GTK_SIGNAL_FUNC (storage_removed_folder_cb), storage_set);
	gtk_signal_connect (GTK_OBJECT (storage), "close_folder",
			    GTK_SIGNAL_FUNC (storage_close_folder_cb), storage_set);

	priv->storages = g_list_append (priv->storages, storage);

	named_storage = named_storage_new (storage);
	g_hash_table_insert (priv->name_to_named_storage, named_storage->name, named_storage);

	gtk_signal_emit (GTK_OBJECT (storage_set), signals[NEW_STORAGE], storage);

	signal_new_folder_for_all_folders_in_storage (storage_set, storage);

	return TRUE;
}

gboolean
e_storage_set_remove_storage (EStorageSet *storage_set,
			      EStorage *storage)
{
	EStorageSetPrivate *priv;
	NamedStorage *named_storage;

	g_return_val_if_fail (storage_set != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), FALSE);
	g_return_val_if_fail (storage != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE (storage), FALSE);

	priv = storage_set->priv;

	named_storage = g_hash_table_lookup (priv->name_to_named_storage,
					     e_storage_get_name (storage));
	if (named_storage == NULL)
		return FALSE;

	g_hash_table_remove (priv->name_to_named_storage, named_storage->name);
	named_storage_destroy (named_storage);

	priv->storages = g_list_remove (priv->storages, storage);

	gtk_signal_emit (GTK_OBJECT (storage_set), signals[REMOVED_STORAGE], storage);
	gtk_object_unref (GTK_OBJECT (storage));

	return TRUE;
}

void
e_storage_set_remove_all_storages (EStorageSet *storage_set)
{
	EStorageSetPrivate *priv;
	GList *p;

	g_return_if_fail (storage_set != NULL);
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));

	priv = storage_set->priv;

	for (p = priv->storages; p != NULL; p = p->next) {
		EStorage *storage;

		storage = E_STORAGE (p->data);

		gtk_signal_emit (GTK_OBJECT (storage_set), signals[REMOVED_STORAGE], storage);
		gtk_object_unref (GTK_OBJECT (storage));
	}

	g_hash_table_foreach_remove (priv->name_to_named_storage,
				     name_to_named_storage_foreach_destroy,
				     NULL);

	g_list_free (priv->storages);
	priv->storages = NULL;
}


EStorage *
e_storage_set_get_storage (EStorageSet *storage_set,
			   const char *name)
{
	EStorageSetPrivate *priv;
	NamedStorage *named_storage;

	g_return_val_if_fail (storage_set != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	priv = storage_set->priv;

	named_storage = g_hash_table_lookup (priv->name_to_named_storage, name);
	if (named_storage == NULL)
		return NULL;
	else
		return named_storage->storage;
}

EFolder *
e_storage_set_get_folder (EStorageSet *storage_set,
			  const char *path)
{
	EStorage *storage;
	const char *subpath;

	g_return_val_if_fail (storage_set != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);
	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (g_path_is_absolute (path), NULL);

	storage = get_storage_for_path (storage_set, path, &subpath);
	if (storage == NULL)
		return NULL;

	return e_storage_get_folder (storage, subpath);
}


static void
storage_set_view_folder_opened (EStorageSetView *storage_set_view,
				const char *path,
				EStorageSet *storage_set)
{
	EStorage *storage;
	const char *subpath;

	storage = get_storage_for_path (storage_set, path, &subpath);
	if (storage == NULL)
		return;

	e_storage_async_open_folder (storage, subpath);
}

GtkWidget *
e_storage_set_new_view (EStorageSet *storage_set, BonoboUIContainer *container)
{
	GtkWidget *storage_set_view;

	g_return_val_if_fail (storage_set != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);

	storage_set_view = e_storage_set_view_new (storage_set, container);
	gtk_signal_connect (GTK_OBJECT (storage_set_view), "folder_opened",
			    GTK_SIGNAL_FUNC (storage_set_view_folder_opened),
			    storage_set);

	return storage_set_view;
}


void
e_storage_set_async_create_folder  (EStorageSet *storage_set,
				    const char *path,
				    const char *type,
				    const char *description,
				    EStorageSetResultCallback callback,
				    void *data)
{
	EStorage *storage;
	const char *subpath;
	StorageCallbackConverterData *converter_data;

	g_return_if_fail (storage_set != NULL);
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));
	g_return_if_fail (path != NULL);
	g_return_if_fail (g_path_is_absolute (path));
	g_return_if_fail (type != NULL);
	g_return_if_fail (description != NULL);
	g_return_if_fail (callback != NULL);

	storage = get_storage_for_path (storage_set, path, &subpath);

	converter_data = storage_callback_converter_data_new (storage_set, callback, data);

	e_storage_async_create_folder (storage, subpath, type, description,
				       storage_callback_converter, converter_data);
}

void
e_storage_set_async_remove_folder  (EStorageSet *storage_set,
				    const char *path,
				    EStorageSetResultCallback callback,
				    void *data)
{
	EStorage *storage;
	const char *subpath;
	StorageCallbackConverterData *converter_data;

	g_return_if_fail (storage_set != NULL);
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));
	g_return_if_fail (path != NULL);
	g_return_if_fail (g_path_is_absolute (path));
	g_return_if_fail (callback != NULL);

	storage = get_storage_for_path (storage_set, path, &subpath);

	converter_data = storage_callback_converter_data_new (storage_set, callback, data);

	e_storage_async_remove_folder (storage, subpath,
				       storage_callback_converter, converter_data);
}

void
e_storage_set_async_xfer_folder (EStorageSet *storage_set,
				 const char *source_path,
				 const char *destination_path,
				 gboolean remove_source,
				 EStorageSetResultCallback callback,
				 void *data)
{
	EStorage *source_storage;
	EStorage *destination_storage;
	const char *source_subpath;
	const char *destination_subpath;
	StorageCallbackConverterData *converter_data;

	g_return_if_fail (storage_set != NULL);
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));
	g_return_if_fail (source_path != NULL);
	g_return_if_fail (g_path_is_absolute (source_path));
	g_return_if_fail (destination_path != NULL);
	g_return_if_fail (g_path_is_absolute (destination_path));
	g_return_if_fail (callback != NULL);

	source_storage = get_storage_for_path (storage_set, source_path, &source_subpath);
	destination_storage = get_storage_for_path (storage_set, destination_path, &destination_subpath);

	if (source_storage != destination_storage) {
		g_warning ("e_storage_set_async_xfer_folder(): "
			   "Attempt to xfer folders between different storages -- not supported yet.");
		(* callback) (storage_set, E_STORAGE_UNSUPPORTEDOPERATION, data);
		return;
	}

	converter_data = storage_callback_converter_data_new (storage_set, callback, data);

	e_storage_async_xfer_folder (source_storage,
 				     source_subpath, destination_subpath, remove_source,
				     storage_callback_converter, converter_data);
}


EFolderTypeRegistry *
e_storage_set_get_folder_type_registry (EStorageSet *storage_set)
{
	g_return_val_if_fail (storage_set != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);

	return storage_set->priv->folder_type_registry;
}


/**
 * e_storage_set_get_path_for_physical_uri:
 * @storage_set: A storage set
 * @physical_uri: A physical URI
 * 
 * Retrieve the path of the folder whose physical URI matches @physical_uri.
 * 
 * Return value: 
 **/
char *
e_storage_set_get_path_for_physical_uri (EStorageSet *storage_set,
					 const char *physical_uri)
{
	EStorageSetPrivate *priv;
	GList *p;

	g_return_val_if_fail (storage_set != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);
	g_return_val_if_fail (physical_uri != NULL, NULL);

	priv = storage_set->priv;

	for (p = priv->storages; p != NULL; p = p->next) {
		EStorage *storage;
		char *storage_path;

		storage = E_STORAGE (p->data);

		storage_path = e_storage_get_path_for_physical_uri (storage, physical_uri);
		if (storage_path != NULL) {
			char *storage_set_path;

			storage_set_path = g_strconcat (G_DIR_SEPARATOR_S,
							e_storage_get_name (storage),
							storage_path,
							NULL);
			g_free (storage_path);

			return storage_set_path;
		}
	}

	return NULL;
}


E_MAKE_TYPE (e_storage_set, "EStorageSet", EStorageSet, class_init, init, PARENT_TYPE)
