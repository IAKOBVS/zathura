/* SPDX-License-Identifier: Zlib */

#ifndef NOTES_ELEMENT_OBJECT_H
#define NOTES_ELEMENT_OBJECT_H

#include "types.h"

#include <gtk/gtk.h>

#define ZATHURA_TYPE_NOTES_ELEMENT_OBJECT (zathura_notes_element_object_get_type())

/* GObject wrapping zathura_note_t so it can live in a GListModel */
G_DECLARE_FINAL_TYPE(ZathuraNotesElementObject, zathura_notes_element_object, ZATHURA, NOTES_ELEMENT_OBJECT, GObject)

struct _ZathuraNotesElementObject {
  GObject parent_instance;
  char* page_label; /**< page string for display */
  char* text;       /**< note text for display */
  guint64 id;       /**< note id for lookup */
};

#endif
