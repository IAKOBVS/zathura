/* SPDX-License-Identifier: Zlib */

#ifndef ZATHURA_NOTES_H
#define ZATHURA_NOTES_H

#include "types.h"
#include <gtk/gtk.h>

/* forward declaration to avoid pulling in zathura.h here */
typedef struct zathura_s zathura_t;

/**
 * A single user annotation: the selected text on a page, the user's note and
 * (optionally) the selection rectangles for later highlighting.
 */
typedef struct {
  unsigned int page;     /**< 0-based page index */
  char* text;            /**< selected text (may be NULL) */
  char* note;            /**< the user note (may be NULL) */
  girara_list_t* rects;  /**< list of g_malloc'd zathura_rectangle_t*, or NULL */
  char* time;            /**< ISO-8601 timestamp (may be NULL) */
  guint64 id;            /**< stable identifier */
} zathura_note_t;

/**
 * A snapshot of a finished text selection, captured until the user attaches a
 * note to it.
 */
typedef struct {
  unsigned int page;    /**< 0-based page index */
  char* text;           /**< selected text */
  girara_list_t* rects; /**< list of g_malloc'd zathura_rectangle_t*, or NULL */
} zathura_selection_t;

/* derive the side-car file name: /a/b/paper.pdf -> /a/b/.paper.pdf-notes.
 * Returns NULL when document_path is NULL (e.g. documents opened from stdin). */
char* zathura_notes_path_for_document(const char* document_path);

zathura_note_t* zathura_note_new(unsigned int page, const char* text, girara_list_t* rects, const char* note);
void zathura_note_free(zathura_note_t* note);

/* load/save all notes of the current document (keyed by its path). The
 * in-memory list zathura->notes must already be initialised. */
bool zathura_notes_load(zathura_t* zathura);
bool zathura_notes_save(zathura_t* zathura);

void zathura_notes_add(zathura_t* zathura, zathura_note_t* note);
bool zathura_notes_remove(zathura_t* zathura, guint64 id);
zathura_note_t* zathura_notes_find(zathura_t* zathura, guint64 id);
void zathura_selection_free(zathura_selection_t* selection);

/* pure (de)serialization, usable without a zathura_t instance */
bool zathura_notes_serialize(girara_list_t* notes, char** data, gsize* length);
bool zathura_notes_deserialize(const char* data, girara_list_t* notes);

/* a GListModel of ZathuraNoteElementObject for the notes panel */
GListModel* zathura_notes_build_model(zathura_t* zathura);

#endif
