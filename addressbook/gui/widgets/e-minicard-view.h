/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-minicard-view.h
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __E_MINICARD_VIEW_H__
#define __E_MINICARD_VIEW_H__

#include "e-minicard.h"
#include <gal/widgets/e-reflow-sorted.h>
#include <gal/widgets/e-selection-model-simple.h>
#include "addressbook/backend/ebook/e-book.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* EMinicardView - A canvas item container.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 * book         EBook           RW              book to query
 * query        string          RW              query string
 *
 * From EReflowSorted:   (you should really know what you're doing if you set these.)
 * compare_func  GCompareFunc   RW              compare function
 * string_func   EReflowStringFunc RW           string function
 *
 * From EReflow:
 * minimum_width double         RW              minimum width of the reflow.  width >= minimum_width
 * width        double          R               width of the reflow
 * height       double          RW              height of the reflow
 */

#define E_MINICARD_VIEW_TYPE			(e_minicard_view_get_type ())
#define E_MINICARD_VIEW(obj)			(GTK_CHECK_CAST ((obj), E_MINICARD_VIEW_TYPE, EMinicardView))
#define E_MINICARD_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_MINICARD_VIEW_TYPE, EMinicardViewClass))
#define E_IS_MINICARD_VIEW(obj) 		(GTK_CHECK_TYPE ((obj), E_MINICARD_VIEW_TYPE))
#define E_IS_MINICARD_VIEW_CLASS(klass) 	(GTK_CHECK_CLASS_TYPE ((obj), E_MINICARD_VIEW_TYPE))


typedef struct _EMinicardView       EMinicardView;
typedef struct _EMinicardViewClass  EMinicardViewClass;

struct _EMinicardView
{
	EReflowSorted parent;
	
	/* item specific fields */
	EBook *book;
	char *query;
	guint editable : 1;
	EBookView *book_view;

	ESelectionModelSimple *selection;

	EMinicard *drag_card;

	int get_view_idle;

	int canvas_destroy_id;
	int canvas_drag_data_get_id;

	int create_card_id, remove_card_id, modify_card_id, status_message_id;

	guint first_get_view : 1;
};

struct _EMinicardViewClass
{
	EReflowSortedClass parent_class;

	/*
	 * Signals
	 */
	void (*status_message) (EMinicardView *mini_view, const gchar *message);
};

GtkType    e_minicard_view_get_type (void);
void       e_minicard_view_remove_selection (EMinicardView *view,
					     EBookCallback  cb,
					     gpointer       closure);
void       e_minicard_view_jump_to_letter   (EMinicardView *view,
					     char           letter);
void       e_minicard_view_stop             (EMinicardView *view);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_MINICARD_VIEW_H__ */
