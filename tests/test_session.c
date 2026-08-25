/* SPDX-License-Identifier: Zlib */

#include <girara/log.h>
#include <girara-gtk/session.h>
#include <girara-gtk/settings.h>

#include <cairo-pdf.h>
#include <cairo.h>
#include <glib/gstdio.h>

#include "commands.h"
#include "index-element-object.h"
#include "utils.h"
#include "zathura.h"

#include "tests.h"

static void test_girara_create(void) {
  setup_logger();

  girara_session_t* session = girara_session_create();
  g_assert_nonnull(session);
  girara_session_destroy(session);
}

static void test_girara_init(void) {
  setup_logger();

  girara_session_t* session = girara_session_create();
  g_assert_nonnull(session);
  g_assert_true(girara_session_init(session, "test"));
  girara_session_destroy(session);
}

static void test_create(void) {
  setup_logger();
  girara_set_log_level(GIRARA_ERROR);

  zathura_t* zathura = zathura_create();
  g_assert_nonnull(zathura);
  g_assert_nonnull(g_getenv("G_TEST_SRCDIR"));
  zathura_set_config_dir(zathura, g_getenv("G_TEST_SRCDIR"));
  g_assert_true(zathura_init(zathura));
  zathura_free(zathura);
}

#ifdef CAIRO_HAS_PDF_SURFACE
/* write a word on every page of a small pdf */
static void pdf_write_pages(cairo_t* cr, const char** words, unsigned int count) {
  for (unsigned int i = 0; i < count; ++i) {
    cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 24);
    cairo_move_to(cr, 50, 100);
    cairo_show_text(cr, words[i]);
    cairo_show_page(cr);
  }
}

static char* create_test_pdf_at(const char* basename) {
  g_autofree char* path = g_build_filename(g_getenv("G_TEST_BUILDDIR"), basename, NULL);

  const char** words;
  unsigned int count;
  if (g_strcmp0(basename, "test-search-far-hit.pdf") == 0) {
    /* "bug" on pages 6 and 8 (indexes 5 and 7) */
    static const char* far_words[] = {"what", "lorem", "lorem", "lorem", "lorem",
                                      "bug",  "lorem", "bug",   "lorem", "lorem"};
    words                          = far_words;
    count                          = G_N_ELEMENTS(far_words);
  } else {
    static const char* default_words[] = {"what",  "lorem", "lorem", "what",  "lorem",
                                          "lorem", "lorem", "bug",   "lorem", "lorem"};
    words                              = default_words;
    count                              = G_N_ELEMENTS(default_words);
  }

  cairo_surface_t* surface = cairo_pdf_surface_create(path, 400, 300);
  g_assert_nonnull(surface);
  cairo_t* cr = cairo_create(surface);
  pdf_write_pages(cr, words, count);

  cairo_destroy(cr);
  cairo_surface_flush(surface);
  cairo_status_t status = cairo_surface_status(surface);
  cairo_surface_destroy(surface);
  g_assert_cmpint(status, ==, CAIRO_STATUS_SUCCESS);
  return g_steal_pointer(&path);
}

static char* create_test_pdf(void) {
  return create_test_pdf_at("test-search.pdf");
}

/* "bug" only on page 6 (index 5), so a limited search from page 1 finds
 * nothing and only pressing n can extend the scan onto it */
static char* create_test_pdf_far_hit(void) {
  return create_test_pdf_at("test-search-far-hit.pdf");
}
#endif
#ifdef CAIRO_HAS_PDF_SURFACE
/* Regression: navigating with n after a limited search must not jump to stale
 * results of an earlier search pattern. */
static void test_search_stale_results(void) {
  setup_logger();
  girara_set_log_level(GIRARA_ERROR);

  zathura_t* zathura = zathura_create();
  g_assert_nonnull(zathura);
  zathura_set_config_dir(zathura, g_getenv("G_TEST_SRCDIR"));
  g_assert_true(zathura_init(zathura));

  g_autofree char* path = create_test_pdf();
  if (document_open(zathura, path, NULL, NULL, 1, NULL) == false) {
    zathura_free(zathura);
    g_test_skip("no document plugin available");
    return;
  }
  g_assert_nonnull(zathura->document);

  /* limit the search so that only parts of the document are scanned */
  g_assert_true(girara_setting_set(zathura->ui.session, "search-limit", "page"));

  /* first search with limit=page: only the matches on the current page are
   * stored and selected */
  girara_argument_t arg_fwd = {.n = FORWARD, .data = NULL};
  g_assert_true(cmd_search(zathura->ui.session, "what", &arg_fwd));
  g_assert_cmpint(zathura->global.total_search_results, >=, 1);

  /* move away from the selected match */
  g_assert_true(page_set(zathura, 6));

  /* searching for a different pattern clears the old results; this must not
   * trip assertions on pages whose result list is already gone */
  /* second search for a different pattern: "bug" is only on page 8, so pages
   * behind are never re-scanned and must not keep old results */
  g_assert_true(cmd_search(zathura->ui.session, "bug", &arg_fwd));
  g_assert_cmpuint(zathura_document_get_current_page_number(zathura->document), ==, 7);

  /* n: no further "bug" match is known, so this must not navigate anywhere -
   * in particular not onto the stale "what" match on page 4 */
  girara_argument_t arg_nav = {.n = FORWARD, .data = NULL};
  bool navigated            = search_document(zathura, &arg_nav, true);
  if (navigated == true) {
    g_test_fail_printf("n navigated to stale results of the previous search pattern");
  }

  document_close(zathura, false);
  zathura_free(zathura);
  g_unlink(path);
}

/* n must extend the search beyond the pages already known, accumulating the
 * results of all visited pages for as long as the pattern stays the same */
static void test_search_accumulate(void) {
  setup_logger();
  girara_set_log_level(GIRARA_ERROR);

  zathura_t* zathura = zathura_create();
  g_assert_nonnull(zathura);
  zathura_set_config_dir(zathura, g_getenv("G_TEST_SRCDIR"));
  g_assert_true(zathura_init(zathura));

  g_autofree char* path = create_test_pdf_far_hit();
  if (document_open(zathura, path, NULL, NULL, 1, NULL) == false) {
    zathura_free(zathura);
    g_test_skip("no document plugin available");
    return;
  }
  g_assert_nonnull(zathura->document);

  g_assert_true(girara_setting_set(zathura->ui.session, "search-limit", "page"));

  /* fresh search: the scan stops at page 6 (index 5), the only one with a
   * match; later pages are unknown so far */
  girara_argument_t arg_fwd = {.n = FORWARD, .data = NULL};
  g_assert_true(cmd_search(zathura->ui.session, "bug", &arg_fwd));
  g_assert_cmpuint(zathura_document_get_current_page_number(zathura->document), ==, 5);
  g_assert_cmpint(zathura->global.total_search_results, >=, 1);
  const int total_after_first = zathura->global.total_search_results;

  /* n: extends the scan onto page 8 (index 7) */
  girara_argument_t arg_nav = {.n = FORWARD, .data = NULL};
  bool navigated            = search_document(zathura, &arg_nav, true);
  if (navigated == false) {
    navigated = search_continue(zathura, FORWARD);
  }
  if (navigated == false) {
    g_test_fail_printf("n did not extend the search onto the following pages");
  } else {
    g_assert_cmpuint(zathura_document_get_current_page_number(zathura->document), ==, 7);
    /* the matches found earlier must still be accumulated */
    if (zathura->global.total_search_results < total_after_first + 1) {
      g_test_fail_printf("continuing the search discarded previously accumulated results");
    }
  }

  /* n again: past the last match the search wraps around to the first one */
  navigated = search_document(zathura, &arg_nav, true);
  if (navigated == false) {
    navigated = search_continue(zathura, FORWARD);
  }
  if (navigated == true && zathura_document_get_current_page_number(zathura->document) != 5) {
    g_test_fail_printf("wrapping around with n did not return to the first match");
  }

  document_close(zathura, false);
  zathura_free(zathura);
  g_unlink(path);
}
#endif

/* build an index element with the given title and an optional children store */
static ZathuraIndexElementObject* make_index_element(const char* title, GListStore* children) {
  ZathuraIndexElementObject* item = g_object_new(ZATHURA_TYPE_INDEX_ELEMENT_OBJECT, NULL);
  item->title                     = g_markup_escape_text(title, -1);
  if (children != NULL) {
    item->children = g_object_ref(children);
    g_object_unref(children);
  }
  return item;
}

static void index_store_append(GListStore* store, ZathuraIndexElementObject* item) {
  g_list_store_append(store, item);
  g_object_unref(item);
}

static void test_index_search_show_match(void) {
  setup_logger();
  girara_set_log_level(GIRARA_ERROR);

  zathura_t* zathura = zathura_create();
  g_assert_nonnull(zathura);
  zathura_set_config_dir(zathura, g_getenv("G_TEST_SRCDIR"));
  g_assert_true(zathura_init(zathura));

  /* index tree: "Theory intro" | "Other" { "Nested THEORY here" } | "Appendix" */
  GListStore* nested              = g_list_store_new(ZATHURA_TYPE_INDEX_ELEMENT_OBJECT);
  ZathuraIndexElementObject* deep = make_index_element("Nested THEORY here", NULL);
  index_store_append(nested, deep);

  GListStore* root                    = g_list_store_new(ZATHURA_TYPE_INDEX_ELEMENT_OBJECT);
  ZathuraIndexElementObject* other    = make_index_element("Other", nested);
  ZathuraIndexElementObject* appendix = make_index_element("Appendix", NULL);
  index_store_append(root, make_index_element("Theory intro", NULL));
  index_store_append(root, other);
  index_store_append(root, appendix);

  /* search for something that only matches below a collapsed node */
  GHashTable* relevant = NULL;
  GList* matches       = zathura_index_search(G_LIST_MODEL(root), "nested", &relevant);
  g_assert_nonnull(relevant);
  g_assert_cmpuint(g_list_length(matches), ==, 1);
  g_assert_true(g_list_nth_data(matches, 0) == deep);

  /* build the index widgets like sc_toggle_index does */
  zathura->ui.index = gtk_scrolled_window_new();
  g_assert_nonnull(zathura->ui.index);
  GtkTreeListModel* tree =
      gtk_tree_list_model_new(G_LIST_MODEL(root), FALSE, FALSE, index_create_child_model, NULL, NULL);
  GtkSingleSelection* sel     = gtk_single_selection_new(G_LIST_MODEL(tree));
  GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
  GtkListView* view           = GTK_LIST_VIEW(gtk_list_view_new(GTK_SELECTION_MODEL(sel), factory));
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(zathura->ui.index), GTK_WIDGET(view));

  /* before the search nothing is expanded: only the three top-level rows exist */
  g_assert_cmpuint(g_list_model_get_n_items(G_LIST_MODEL(sel)), ==, 3);

  /* showing the match must expand its ancestors and select its row */
  g_assert_true(index_show_match(zathura, relevant, deep));
  guint n_items       = g_list_model_get_n_items(G_LIST_MODEL(sel));
  guint target_pos    = G_MAXUINT;
  bool other_expanded = false;
  for (guint i = 0; i < n_items; ++i) {
    g_autoptr(GtkTreeListRow) row            = g_list_model_get_item(G_LIST_MODEL(sel), i);
    g_autoptr(ZathuraIndexElementObject) obj = gtk_tree_list_row_get_item(row);
    if (obj == deep) {
      target_pos = i;
    } else if (obj == other) {
      other_expanded = gtk_tree_list_row_get_expanded(row);
    }
  }
  g_assert_cmpuint(target_pos, !=, G_MAXUINT);
  g_assert_true(other_expanded);
  g_assert_cmpuint(gtk_single_selection_get_selected(sel), ==, target_pos);

  /* an element outside of the search results cannot be shown */
  g_assert_false(index_show_match(zathura, relevant, appendix));

  g_list_free(matches);
  g_hash_table_unref(relevant);
  g_object_unref(root);
  zathura_free(zathura);
}

int main(int argc, char* argv[]) {
  setup_logger();

  gtk_init();
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/girara/session/create", test_girara_create);
  g_test_add_func("/girara/session/init", test_girara_init);
  g_test_add_func("/session/create", test_create);
#ifdef CAIRO_HAS_PDF_SURFACE
  g_test_add_func("/session/search-stale-results", test_search_stale_results);
  g_test_add_func("/session/search-accumulate", test_search_accumulate);
#endif
  g_test_add_func("/session/index-search-show-match", test_index_search_show_match);
  return g_test_run();
}
