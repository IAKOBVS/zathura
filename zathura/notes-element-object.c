/* SPDX-License-Identifier: Zlib */

#include "notes-element-object.h"

G_DEFINE_TYPE(ZathuraNotesElementObject, zathura_notes_element_object, G_TYPE_OBJECT)

static void zathura_notes_element_object_dispose(GObject* object) {
  G_OBJECT_CLASS(zathura_notes_element_object_parent_class)->dispose(object);
}

static void zathura_notes_element_object_finalize(GObject* object) {
  ZathuraNotesElementObject* self = ZATHURA_NOTES_ELEMENT_OBJECT(object);
  g_clear_pointer(&self->page_label, g_free);
  g_clear_pointer(&self->text, g_free);
  G_OBJECT_CLASS(zathura_notes_element_object_parent_class)->finalize(object);
}

static void zathura_notes_element_object_class_init(ZathuraNotesElementObjectClass* klass) {
  GObjectClass* object_class    = G_OBJECT_CLASS(klass);
  object_class->dispose         = zathura_notes_element_object_dispose;
  object_class->finalize        = zathura_notes_element_object_finalize;
}

static void zathura_notes_element_object_init(ZathuraNotesElementObject* UNUSED(self)) {}
