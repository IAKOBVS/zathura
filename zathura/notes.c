/* SPDX-License-Identifier: Zlib */

#include "notes.h"

#include "document.h"
#include "notes-element-object.h"
#include "utils.h"
#include "zathura.h"

#include <json-glib/json-glib.h>
#include <glib/gi18n.h>

char* zathura_notes_path_for_document(const char* document_path) {
  if (document_path == NULL) {
    return NULL;
  }

  char* dir  = g_path_get_dirname(document_path);
  char* base = g_path_get_basename(document_path);
  char* path = g_strconcat(dir, "/.", base, "-notes", NULL);
  g_free(dir);
  g_free(base);

  return path;
}

static guint64 note_id(unsigned int page, const char* text) {
  g_autofree char* key = g_strdup_printf("%u|%s", page, text != NULL ? text : "");
  return (guint64)g_str_hash(key);
}

zathura_note_t* zathura_note_new(unsigned int page, const char* text, girara_list_t* rects, const char* note) {
  zathura_note_t* self = g_malloc0(sizeof(zathura_note_t));
  self->page  = page;
  self->text  = text != NULL ? g_strdup(text) : NULL;
  self->note  = note != NULL ? g_strdup(note) : NULL;
  self->rects = rects; /* takes ownership */
  self->time  = g_date_time_format_iso8601(g_date_time_new_now_local());
  self->id    = note_id(page, text);
  return self;
}

void zathura_note_free(zathura_note_t* note) {
  if (note == NULL) {
    return;
  }
  g_free(note->text);
  g_free(note->note);
  g_free(note->time);
  if (note->rects != NULL) {
    girara_list_free(note->rects);
  }
  g_free(note);
}

void zathura_selection_free(zathura_selection_t* selection) {
  if (selection == NULL) {
    return;
  }
  g_free(selection->text);
  if (selection->rects != NULL) {
    girara_list_free(selection->rects);
  }
  g_free(selection);
}

static zathura_note_t* note_from_json_object(JsonObject* object) {
  if (object == NULL) {
    return NULL;
  }

  g_autofree char* text = g_strdup(json_object_get_string_member(object, "text"));
  g_autofree char* note = g_strdup(json_object_get_string_member(object, "note"));
  g_autofree char* time = g_strdup(json_object_get_string_member(object, "time"));
  unsigned int page     = (unsigned int)json_object_get_int_member(object, "page");

  girara_list_t* rects = NULL;
  if (json_object_has_member(object, "rects")) {
    JsonArray* rects_array = json_object_get_array_member(object, "rects");
    guint rects_len        = json_array_get_length(rects_array);
    if (rects_len > 0) {
      rects = girara_list_new_with_free(g_free);
      for (guint j = 0; j < rects_len; ++j) {
        JsonArray* r  = json_array_get_array_element(rects_array, j);
        zathura_rectangle_t* rect = g_malloc(sizeof(zathura_rectangle_t));
        rect->x1 = json_array_get_double_element(r, 0);
        rect->y1 = json_array_get_double_element(r, 1);
        rect->x2 = json_array_get_double_element(r, 2);
        rect->y2 = json_array_get_double_element(r, 3);
        girara_list_append(rects, rect);
      }
    }
  }

  zathura_note_t* note_obj = g_malloc0(sizeof(zathura_note_t));
  note_obj->page  = page;
  note_obj->text  = g_steal_pointer(&text);
  note_obj->note  = g_steal_pointer(&note);
  note_obj->time  = g_strdup(time);
  note_obj->rects = rects;
  note_obj->id    = note_id(page, note_obj->text);
  return note_obj;
}

static void note_to_builder(JsonBuilder* builder, zathura_note_t* note) {
  json_builder_begin_object(builder);

  json_builder_set_member_name(builder, "page");
  json_builder_add_int_value(builder, note->page);
  json_builder_set_member_name(builder, "text");
  json_builder_add_string_value(builder, note->text != NULL ? note->text : "");
  json_builder_set_member_name(builder, "note");
  json_builder_add_string_value(builder, note->note != NULL ? note->note : "");
  json_builder_set_member_name(builder, "time");
  json_builder_add_string_value(builder, note->time != NULL ? note->time : "");

  json_builder_set_member_name(builder, "rects");
  json_builder_begin_array(builder);
  if (note->rects != NULL) {
    for (size_t j = 0; j < girara_list_size(note->rects); ++j) {
      zathura_rectangle_t* rect = girara_list_nth(note->rects, j);
      json_builder_begin_array(builder);
      json_builder_add_double_value(builder, rect->x1);
      json_builder_add_double_value(builder, rect->y1);
      json_builder_add_double_value(builder, rect->x2);
      json_builder_add_double_value(builder, rect->y2);
      json_builder_end_array(builder);
    }
  }
  json_builder_end_array(builder);

  json_builder_end_object(builder);
}

bool zathura_notes_serialize(girara_list_t* notes, char** data, gsize* length) {
  g_return_val_if_fail(notes != NULL, false);
  g_return_val_if_fail(data != NULL, false);

  g_autoptr(JsonBuilder) builder = json_builder_new();
  json_builder_begin_array(builder);
  for (size_t i = 0; i < girara_list_size(notes); ++i) {
    note_to_builder(builder, girara_list_nth(notes, i));
  }
  json_builder_end_array(builder);

  g_autoptr(JsonGenerator) generator = json_generator_new();
  g_autoptr(JsonNode) root            = json_builder_get_root(builder);
  json_generator_set_root(generator, root);
  json_generator_set_pretty(generator, TRUE);

  gsize len = 0;
  *data     = json_generator_to_data(generator, &len);
  if (length != NULL) {
    *length = len;
  }
  return *data != NULL;
}

bool zathura_notes_deserialize(const char* data, girara_list_t* notes) {
  g_return_val_if_fail(notes != NULL, false);
  if (data == NULL) {
    return false;
  }

  g_autoptr(JsonParser) parser = json_parser_new();
  if (json_parser_load_from_data(parser, data, -1, NULL) == false) {
    return false;
  }

  JsonNode* root = json_parser_get_root(parser);
  if (root == NULL || JSON_NODE_HOLDS_ARRAY(root) == false) {
    return false;
  }

  JsonArray* array = json_node_get_array(root);
  guint length     = json_array_get_length(array);
  for (guint i = 0; i < length; ++i) {
    JsonObject* object = json_array_get_object_element(array, i);
    zathura_note_t* note = note_from_json_object(object);
    if (note != NULL) {
      girara_list_append(notes, note);
    }
  }

  return true;
}

bool zathura_notes_load(zathura_t* zathura) {
  g_return_val_if_fail(zathura != NULL, false);
  if (zathura->document == NULL || zathura->notes_obj.notes == NULL) {
    return false;
  }

  const char* document_path = zathura_document_get_path(zathura->document);
  if (document_path == NULL) {
    return false;
  }

  g_autofree char* path = zathura_notes_path_for_document(document_path);
  if (path == NULL || g_file_test(path, G_FILE_TEST_EXISTS) == false) {
    return true; /* no side-car file yet: an empty list is valid */
  }

  g_autofree char* data = NULL;
  gsize length = 0;
  if (g_file_get_contents(path, &data, &length, NULL) == FALSE) {
    girara_notify(zathura->ui.session, GIRARA_WARNING, _("Failed to read note file: %s"), path);
    return false;
  }

  return zathura_notes_deserialize(data, zathura->notes_obj.notes);
}

bool zathura_notes_save(zathura_t* zathura) {
  g_return_val_if_fail(zathura != NULL, false);
  if (zathura->document == NULL || zathura->notes_obj.notes == NULL) {
    return false;
  }

  const char* document_path = zathura_document_get_path(zathura->document);
  if (document_path == NULL) {
    return false;
  }

  g_autofree char* path = zathura_notes_path_for_document(document_path);
  if (path == NULL) {
    return false;
  }

  g_autofree char* data = NULL;
  gsize length = 0;
  if (zathura_notes_serialize(zathura->notes_obj.notes, &data, &length) == false) {
    return false;
  }

  if (g_file_set_contents(path, data, length, NULL) == FALSE) {
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Failed to write note file: %s"), path);
    return false;
  }

  return true;
}

void zathura_notes_add(zathura_t* zathura, zathura_note_t* note) {
  g_return_if_fail(zathura != NULL);
  g_return_if_fail(note != NULL);
  girara_list_append(zathura->notes_obj.notes, note);
}

bool zathura_notes_remove(zathura_t* zathura, guint64 id) {
  g_return_val_if_fail(zathura != NULL, false);
  if (zathura->notes_obj.notes == NULL) {
    return false;
  }

  bool removed = false;
  girara_list_t* keep = girara_list_new();
  for (size_t i = 0; i < girara_list_size(zathura->notes_obj.notes); ++i) {
    zathura_note_t* note = girara_list_nth(zathura->notes_obj.notes, i);
    if (note->id == id) {
      zathura_note_free(note);
      removed = true;
    } else {
      girara_list_append(keep, note);
    }
  }
  girara_list_free(zathura->notes_obj.notes);
  zathura->notes_obj.notes = keep;
  return removed;
}

zathura_note_t* zathura_notes_find(zathura_t* zathura, guint64 id) {
  g_return_val_if_fail(zathura != NULL, NULL);
  if (zathura->notes_obj.notes == NULL) {
    return NULL;
  }
  for (size_t i = 0; i < girara_list_size(zathura->notes_obj.notes); ++i) {
    zathura_note_t* note = girara_list_nth(zathura->notes_obj.notes, i);
    if (note->id == id) {
      return note;
    }
  }
  return NULL;
}

GListModel* zathura_notes_build_model(zathura_t* zathura) {
  GListStore* store = g_list_store_new(ZATHURA_TYPE_NOTES_ELEMENT_OBJECT);
  if (zathura == NULL || zathura->notes_obj.notes == NULL) {
    return G_LIST_MODEL(store);
  }

  for (size_t i = 0; i < girara_list_size(zathura->notes_obj.notes); ++i) {
    zathura_note_t* note    = girara_list_nth(zathura->notes_obj.notes, i);
    ZathuraNotesElementObject* object = g_object_new(ZATHURA_TYPE_NOTES_ELEMENT_OBJECT, NULL);
    object->id          = note->id;
    object->page_label  = g_strdup_printf("p. %u", note->page + 1);
    object->text        = g_strdup_printf("%s%s", note->note != NULL ? note->note : "",
                                       note->note != NULL && note->text != NULL ? ": " : "");
    if (object->text != NULL && note->text != NULL && note->note != NULL) {
      /* append a hint of the selected text (what the note refers to) */
      char* merged = g_strconcat(object->text, note->text, NULL);
      g_free(object->text);
      object->text = merged;
    } else if (note->text != NULL) {
      g_free(object->text);
      object->text = g_strdup(note->text);
    }
    g_list_store_append(store, G_OBJECT(object));
    g_object_unref(object);
  }

  return G_LIST_MODEL(store);
}
