/* SPDX-License-Identifier: Zlib */

#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <gtk/gtk.h>
#include <girara-gtk/types.h>
#include <girara-gtk/internal.h>

#include "document.h"
#include "index-element-object.h"

typedef struct page_offset_s {
  int x;
  int y;
} page_offset_t;

/**
 * Quality of the match between a text and a search query. The values are
 * ordered from worst to best match.
 */
typedef enum {
  ZATHURA_MATCH_NONE = 0,    /**< No match at all */
  ZATHURA_MATCH_SUBSEQUENCE, /**< All characters of the query appear in order */
  ZATHURA_MATCH_SUBSTRING,   /**< Query appears somewhere inside the text */
  ZATHURA_MATCH_PREFIX,      /**< Text starts with the query */
  ZATHURA_MATCH_EXACT,       /**< Text equals the query */
} zathura_match_t;

/**
 * Check how well a text matches a search query. Matching is case-insensitive;
 * an empty query matches any text (ranked as prefix).
 * @param text The text to search in.
 * @param query The search query.
 * @return The quality of the match.
 */
zathura_match_t zathura_text_match(const char* text, const char* query);

/**
 * This function checks if the file has a valid extension. A extension is
 * evaluated as valid if it matches a supported filetype.
 *
 * @param zathura Zathura object
 * @param path The path to the file
 * @return true if the extension is valid, otherwise false
 */
bool file_valid_extension(zathura_t* zathura, const char* path);

/**
 * build a tree of index elements from the document outline
 *
 * @param session The session
 * @param tree the document index tree
 * @return root list model of ZathuraIndexElement objects
 */
GListModel* document_index_build_model(girara_session_t* session, girara_tree_node_t* tree);

/**
 * Search an index model (a tree of ZathuraIndexElementObject as built by
 * document_index_build_model) for titles matching the query. Matching is
 * case-insensitive; the results are returned in display order.
 * @param root the root list model of the index
 * @param query the search text
 * @param out_relevant if non-NULL, receives a newly allocated GHashTable (of
 *        the matched elements and all of their ancestors, keyed by pointer,
 *        to be freed with g_hash_table_unref), or NULL for an empty query
 * @return a newly allocated GList of borrowed ZathuraIndexElementObject
 *         instances (to be freed with g_list_free), or NULL for an empty query
 */
GList* zathura_index_search(GListModel* root, const char* query, GHashTable** out_relevant);

/**
 * GtkTreeListModelCreateModelFunc: expose the children store of an index
 * element as child model.
 * @param item a ZathuraIndexElementObject
 * @param user_data unused
 * @return the children list model or NULL for leaves
 */
GListModel* index_create_child_model(gpointer item, gpointer user_data);

/**
 * Expand every index row that is part of the given relevance set (as returned
 * by zathura_index_search), then select the target element's row and scroll it
 * into view.
 * @param zathura the zathura instance; ui.index must have been created already
 * @param relevant relevance set of matched elements and their ancestors
 * @param target the element to select
 * @return true if the target was found and selected
 */
bool index_show_match(zathura_t* zathura, GHashTable* relevant, ZathuraIndexElementObject* target);

/**
 * Write the current page number (one-based) to a file, creating missing parent
 * directories. The file contents are "<page>\n".
 * @param filename the file to write to
 * @param page_number the current page number, one-based
 * @return true if the file was written
 */
bool zathura_page_number_write(const char* filename, unsigned int page_number);

/**
 * Write the text of the current page to a file, creating missing parent
 * directories. A NULL text is written as an empty file.
 * @param filename the file to write to
 * @param text the page text
 * @return true if the file was written
 */
bool zathura_page_text_write(const char* filename, const char* text);

/**
 * Scrolls the document index to the current page
 *
 * @param zathura The zathura instance
 */
void index_scroll_to_current_page(zathura_t* zathura);

/**
 * Calculates the new coordinates based on the rotation and scale level of the
 * document for the given rectangle
 *
 * @param page Page where the rectangle should be
 * @param rectangle The rectangle
 * @return New rectangle
 */
zathura_rectangle_t recalc_rectangle(zathura_page_t* page, zathura_rectangle_t rectangle);

/**
 * Returns the page widget of the page
 *
 * @param zathura The zathura instance
 * @param page The page object
 * @return The page widget of the page
 * @return NULL if an error occurred
 */
GtkWidget* zathura_page_get_widget(zathura_t* zathura, zathura_page_t* page);

GtkWidget* zathura_page_get_widget_by_number(zathura_t* zathura, unsigned int page_number);

/**
 * Set if the search results should be drawn or not
 *
 * @param zathura Zathura instance
 * @param value true if they should be drawn, otherwise false
 */
void document_draw_search_results(zathura_t* zathura, bool value);

/**
 * Create zathura version string
 *
 * @param plugin_manager The plugin manager
 * @param markup Enable markup
 * @return Version string
 */
char* zathura_get_version_string(const zathura_plugin_manager_t* plugin_manager, bool markup);

/**
 * Get a pointer to the GdkClipboard of the current clipboard.
 *
 * @param zathura The zathura instance
 *
 * @return the current GdkClipboard, or NULL
 */
GdkClipboard* get_selection(zathura_t* zathura);

/**
 * Returns the valid zoom value which needs to lie in the interval of zoom_min
 * and zoom_max specified in the girara session
 *
 * @param[in] session The session
 * @param[in] zoom The proposed zoom value
 *
 * @return The corrected zoom value
 */
double zathura_correct_zoom_value(girara_session_t* session, const double zoom);

/**
 * Write a list of 'pages per row to first column' values as a colon separated string.
 *
 * For valid settings list, this is the inverse of parse_first_page_column_list.
 *
 * @param[in] first_page_columns The settings vector
 * @param[in] size The size of the settings vector
 *
 * @return The new settings string
 */
char* write_first_page_column(unsigned int* first_page_columns, unsigned int size);

/**
 * Parse a 'pages per row to first column' settings list.
 *
 * For valid settings list, this is the inverse of write_first_page_column_list.
 *
 * @param[in] first_page_column_list The settings list
 * @param[in] size A cell to return the size of the result, mandatory
 *
 * @return The values from the settings list as a new vector
 */
unsigned int* parse_first_page_column(const char* first_page_column_list, unsigned int* size);

/**
 * Extracts the column the first page should be rendered in from the specified
 * list of settings corresponding to the specified pages per row
 *
 * @param[in] first_page_column_list The settings list
 * @param[in] pages_per_row The current pages per row
 *
 * @return The column the first page should be rendered in
 */
unsigned int find_first_page_column(const char* first_page_column_list, const unsigned int pages_per_row);

/**
 * Cycle the column the first page should be rendered in.
 *
 * @param[in] first_page_column_list The settings list
 * @param[in] pages_per_row The current pages per row
 * @param[in] incr The value added to the current first page column setting
 *
 * @return The new modified settings list
 */
char* increment_first_page_column(const char* first_page_column_list, const unsigned int pages_per_row, int incr);

/**
 * Parse color string and print warning if color cannot be parsed.
 *
 * @param[out] color The color
 * @param[in] str Color string
 *
 * @return True if color string can be parsed, false otherwise.
 */
bool parse_color(GdkRGBA* color, const char* str);

/**
 * Flatten list of overlapping rectangles.
 *
 * @param[in] rectangles A list of rectangles
 *
 * @return List of rectangles
 */
girara_list_t* flatten_rectangles(girara_list_t* rectangles);

/**
 * Search through the document for the latest search item
 *
 * @param zathura The zathura instance
 * @param argument The used argument
 * @param disable_notify If true, don't notify no match found
 *
 * @return true if the view was moved to a search result
 */
bool search_document(zathura_t* zathura, girara_argument_t* argument, bool disable_notify);

/**
 * How many pages are searched by a new search (/ and ?)
 */
typedef enum {
  ZATHURA_SEARCH_LIMIT_ALL,   /* all pages */
  ZATHURA_SEARCH_LIMIT_FIRST, /* stop after the first page with results */
  ZATHURA_SEARCH_LIMIT_PAGE   /* like FIRST, but always search the current page completely */
} zathura_search_limit_t;

/**
 * Parse a search limit setting value.
 *
 * @param[in] limit String to parse ("all", "first" or "page")
 * @param[out] out The parsed value
 *
 * @return True if the string could be parsed, false otherwise
 */
bool parse_search_limit(const char* limit, zathura_search_limit_t* out);

/**
 * Decide whether a page-by-page search may stop after a page has been searched.
 *
 * @param[in] limit The active search limit
 * @param[in] is_current_page Whether the searched page is the page the search started from
 * @param[in] has_results Whether the searched page produced at least one result
 *
 * @return True if the search should stop
 */
bool search_limit_stops(zathura_search_limit_t limit, bool is_current_page, bool has_results);

/**
 * Compute the index of the i-th page visited by a page-by-page search.
 *
 * @param[in] num_pages Number of pages of the document
 * @param[in] start The page the search starts from
 * @param[in] step +1 or -1: direction of the search
 * @param[in] i Number of steps from the start page (wraps around)
 *
 * @return Index of the page to search
 */
unsigned int search_page_index(unsigned int num_pages, unsigned int start, int step, unsigned int i);

/**
 * Decide whether search results stored on the page widgets belong to a
 * different pattern than the given input and therefore must be discarded
 * before running a new search.
 *
 * @param[in] last_pattern Pattern the stored results belong to (or NULL)
 * @param[in] input The new pattern
 *
 * @return True if stored results are stale and must be cleared
 */
bool search_results_stale(const char* last_pattern, const char* input);

/**
 * Per-page search state as stored on the page widgets
 */
typedef struct {
  int num_results; /**< Number of search results on the page */
  int current;     /**< Currently selected result index or -1 */
} zathura_page_search_state_t;

/**
 * Select the next search match when navigating with n/N or jumping after a
 * fresh search. Pages are visited in scan order starting at current_page,
 * moving in direction diff and wrapping around at the document borders.
 *
 * @param[in] pages Per-page state, indexed by page number
 * @param[in] num_pages Number of entries in pages
 * @param[in] current_page Page the navigation starts from
 * @param[in] diff +1 or -1: direction of the navigation
 * @param[in] new_search True right after a fresh search: jump to the first
 *            (or last, when going backward) match of the nearest page with
 *            results instead of advancing from the selected one
 * @param[out] out_page Index of the selected page
 * @param[out] out_idx Index of the selected result within that page
 *
 * @return True if a target was found, false if no (further) cached match exists
 */
bool search_select_target(const zathura_page_search_state_t* pages, unsigned int num_pages, unsigned int current_page,
                          int diff, bool new_search, unsigned int* out_page, int* out_idx);

/**
 * Decide whether tracking files should be written (page number and/or text).
 * Mirrors the throttling in statusbar_page_number_update: only when tracking
 * is enabled and page changed or tracking just toggled on.
 *
 * @param[in] current_page 1-indexed current page (current_page_number + 1)
 * @param[in] last_page    1-indexed last written page (UINT_MAX for none)
 * @param[in] track_page   Whether track-page is enabled
 * @param[in] track_text   Whether track-text is enabled
 * @param[in] last_track_page Previous track-page value
 * @param[in] last_track_text Previous track-text value
 * @return True if files should be written
 */
bool zathura_tracking_should_write(unsigned int current_page, unsigned int last_page, bool track_page, bool track_text,
                                   bool last_track_page, bool last_track_text);

/**
 * State for visible-pages throttling (see callbacks.c:update_visible_pages).
 * All fields that affect visibility must be compared.
 */
typedef struct {
  void* document; /* zathura_document_t* as void* to avoid include cycle */
  double pos_x;
  double pos_y;
  double zoom;
  unsigned int rotation;
  unsigned int pages_per_row;
  unsigned int number_of_pages;
  unsigned int first_page_column;
  unsigned int view_width;
  unsigned int view_height;
} zathura_visible_state_t;

/**
 * Decide whether update_visible_pages needs to run.
 *
 * @param[in] last    Previous state (or zeroed for first call)
 * @param[in] current Current state
 * @return True if an update is required
 */
bool zathura_visible_pages_should_update(const zathura_visible_state_t* last, const zathura_visible_state_t* current);

#endif // UTILS_H
