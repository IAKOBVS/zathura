/* SPDX-License-Identifier: Zlib */

#include <girara/log.h>
#include <girara-gtk/session.h>
#include <girara-gtk/settings.h>

#include <cairo-pdf.h>
#include <cairo.h>
#include <glib/gstdio.h>

#include "commands.h"
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
  return g_test_run();
}
