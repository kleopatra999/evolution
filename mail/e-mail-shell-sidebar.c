/*
 * e-mail-shell-sidebar.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>  
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-mail-shell-sidebar.h"

#include <string.h>
#include <camel/camel.h>

#include "e-mail-shell-module.h"

#define E_MAIL_SHELL_SIDEBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SHELL_SIDEBAR, EMailShellSidebarPrivate))

struct _EMailShellSidebarPrivate {
	GtkWidget *folder_tree;
};

enum {
	PROP_0,
	PROP_FOLDER_TREE
};

static gpointer parent_class;

static void
mail_shell_sidebar_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FOLDER_TREE:
			g_value_set_object (
				value, e_mail_shell_sidebar_get_folder_tree (
				E_MAIL_SHELL_SIDEBAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_shell_sidebar_dispose (GObject *object)
{
	EMailShellSidebarPrivate *priv;

	priv = E_MAIL_SHELL_SIDEBAR_GET_PRIVATE (object);

	if (priv->folder_tree != NULL) {
		g_object_unref (priv->folder_tree);
		priv->folder_tree = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
mail_shell_sidebar_finalize (GObject *object)
{
	EMailShellSidebarPrivate *priv;

	priv = E_MAIL_SHELL_SIDEBAR_GET_PRIVATE (object);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
mail_shell_sidebar_constructed (GObject *object)
{
	EMailShellSidebarPrivate *priv;
	EShellSidebar *shell_sidebar;
	EShellModule *shell_module;
	EShellView *shell_view;
	GtkWidget *container;
	GtkWidget *widget;

	priv = E_MAIL_SHELL_SIDEBAR_GET_PRIVATE (object);

	/* Chain up to parent's constructed method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	shell_sidebar = E_SHELL_SIDEBAR (object);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_module = e_shell_view_get_shell_module (shell_view);

	/* Build sidebar widgets. */

	container = GTK_WIDGET (object);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = em_folder_tree_new (shell_module);
	em_folder_tree_set_excluded (EM_FOLDER_TREE (widget), 0);
	em_folder_tree_enable_drag_and_drop (EM_FOLDER_TREE (widget));
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->folder_tree = g_object_ref (widget);
	gtk_widget_show (widget);
}

static guint32
mail_shell_sidebar_check_state (EShellSidebar *shell_sidebar)
{
#if 0
	EMailShellSidebar *mail_shell_sidebar;
	EShellModule *shell_module;
	EShellView *shell_view;
	EMFolderTree *folder_tree;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	CamelFolder *folder;
	CamelStore *local_store;
	CamelStore *store;
	gchar *full_name;
	gchar *uri;
	gboolean is_virtual = FALSE;
	gboolean can_delete = TRUE;
	gboolean is_outbox = FALSE;
	gboolean is_store;
	guint32 folder_flags = 0;
	guint32 state = 0;

	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_module = e_shell_view_get_shell_module (shell_view);
	local_store = e_mail_shell_module_get_local_store (shell_module);

	mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (shell_sidebar);
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	tree_view = GTK_TREE_VIEW (folder_tree);

	selection = gtk_tree_view_get_selection (tree_view);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return 0;

	gtk_tree_model_get (
		model, &iter,
		COL_POINTER_CAMEL_STORE, &store,
		COL_STRING_FULL_NAME, &full_name,
		COL_BOOL_IS_STORE, &is_store,
		COL_UINT_FLAGS, &folder_flags,
		COL_STRING_URI, &uri, NULL);

	if (!is_store) {
		if (strcmp (full_name, CAMEL_VJUNK_NAME) == 0)
			is_virtual = TRUE;

		if (strcmp (full_name, CAMEL_VTRASH_NAME) == 0)
			is_virtual = TRUE;

		folder = em_folder_tree_get_selected_folder (folder_tree);
		is_outbox = em_utils_folder_is_outbox (folder, NULL);
	}

	if (is_virtual)
		state |= E_MAIL_SHELL_SIDEBAR_ALLOWS_CHILDREN;
	if (is_outbox)
		state |= E_MAIL_SHELL_SIDEBAR_FOLDER_IS_OUTBOX;
	if (is_store)
		state |= E_MAIL_SHELL_SIDEBAR_FOLDER_IS_STORE;

	return state;
#endif
        return 0;
}

static void
mail_shell_sidebar_class_init (EMailShellSidebarClass *class)
{
	GObjectClass *object_class;
	EShellSidebarClass *shell_sidebar_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailShellSidebarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = mail_shell_sidebar_get_property;
	object_class->dispose = mail_shell_sidebar_dispose;
	object_class->finalize = mail_shell_sidebar_finalize;
	object_class->constructed = mail_shell_sidebar_constructed;

	shell_sidebar_class = E_SHELL_SIDEBAR_CLASS (class);
	shell_sidebar_class->check_state = mail_shell_sidebar_check_state;

	g_object_class_install_property (
		object_class,
		PROP_FOLDER_TREE,
		g_param_spec_object (
			"folder-tree",
			NULL,
			NULL,
			EM_TYPE_FOLDER_TREE,
			G_PARAM_READABLE));
}

static void
mail_shell_sidebar_init (EMailShellSidebar *mail_shell_sidebar)
{
	mail_shell_sidebar->priv =
		E_MAIL_SHELL_SIDEBAR_GET_PRIVATE (mail_shell_sidebar);

	/* Postpone widget construction until we have a shell view. */
}

GType
e_mail_shell_sidebar_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMailShellSidebarClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) mail_shell_sidebar_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMailShellSidebar),
			0,     /* n_preallocs */
			(GInstanceInitFunc) mail_shell_sidebar_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_SHELL_SIDEBAR, "EMailShellSidebar",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_mail_shell_sidebar_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_MAIL_SHELL_SIDEBAR,
		"shell-view", shell_view, NULL);
}

EMFolderTree *
e_mail_shell_sidebar_get_folder_tree (EMailShellSidebar *mail_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_MAIL_SHELL_SIDEBAR (mail_shell_sidebar), NULL);

	return EM_FOLDER_TREE (mail_shell_sidebar->priv->folder_tree);
}
