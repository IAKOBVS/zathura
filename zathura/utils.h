/* SPDX-License-Identifier: Zlib */

#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <gtk/gtk.h>
#include <girara-gtk/types.h>
#include <girara-gtk/internal.h>

#include "document.h"

typedef struct page_offset_s {
  int x;
  int y;
} page_offset_t;

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

#endif // UTILS_H
