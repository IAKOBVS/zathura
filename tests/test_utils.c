/* SPDX-License-Identifier: Zlib */

#include "utils.h"

#include "index-element-object.h"

static void test_file_valid_extension(void) {
  g_assert_false(file_valid_extension(NULL, NULL));
  g_assert_false(file_valid_extension((void*)0xDEAD, NULL));
  g_assert_false(file_valid_extension(NULL, "pdf"));
}

static void test_first_page_column(void) {
  g_assert_true(find_first_page_column("1:2", 1) == 1);
  g_assert_true(find_first_page_column("1:2", 2) == 2);
  g_assert_true(find_first_page_column("1:2", 3) == 2);
  g_assert_true(find_first_page_column("2", 1) == 1);
  g_assert_true(find_first_page_column("2", 2) == 2);
  g_assert_true(find_first_page_column("3:1:2", 1) == 1);
  g_assert_true(find_first_page_column("3:1:2", 2) == 1);
  g_assert_true(find_first_page_column("3:1:2", 7) == 2);
}

static void test_parse_search_limit(void) {
  zathura_search_limit_t limit = ZATHURA_SEARCH_LIMIT_FIRST;
  g_assert_true(parse_search_limit("all", &limit));
  g_assert_cmpuint(limit, ==, ZATHURA_SEARCH_LIMIT_ALL);
  g_assert_true(parse_search_limit("first", &limit));
  g_assert_cmpuint(limit, ==, ZATHURA_SEARCH_LIMIT_FIRST);
  g_assert_true(parse_search_limit("page", &limit));
  g_assert_cmpuint(limit, ==, ZATHURA_SEARCH_LIMIT_PAGE);

  g_assert_false(parse_search_limit(NULL, &limit));
  g_assert_false(parse_search_limit("", &limit));
  g_assert_false(parse_search_limit("bogus", &limit));
}

static void test_search_limit_stops(void) {
  /* all: keep searching regardless */
  g_assert_false(search_limit_stops(ZATHURA_SEARCH_LIMIT_ALL, true, true));
  g_assert_false(search_limit_stops(ZATHURA_SEARCH_LIMIT_ALL, false, true));
  g_assert_false(search_limit_stops(ZATHURA_SEARCH_LIMIT_ALL, false, false));

  /* first: stop at the first page with results, wherever it is */
  g_assert_true(search_limit_stops(ZATHURA_SEARCH_LIMIT_FIRST, true, true));
  g_assert_true(search_limit_stops(ZATHURA_SEARCH_LIMIT_FIRST, false, true));
  g_assert_false(search_limit_stops(ZATHURA_SEARCH_LIMIT_FIRST, false, false));
  g_assert_false(search_limit_stops(ZATHURA_SEARCH_LIMIT_FIRST, true, false));

  /* page: like first, but always search through the current page */
  g_assert_false(search_limit_stops(ZATHURA_SEARCH_LIMIT_PAGE, true, true));
  g_assert_true(search_limit_stops(ZATHURA_SEARCH_LIMIT_PAGE, false, true));
  g_assert_false(search_limit_stops(ZATHURA_SEARCH_LIMIT_PAGE, false, false));
  g_assert_false(search_limit_stops(ZATHURA_SEARCH_LIMIT_PAGE, true, false));
}

static void test_search_page_index(void) {
  /* forward from page 3 of 10 */
  g_assert_cmpuint(search_page_index(10, 3, 1, 0), ==, 3);
  g_assert_cmpuint(search_page_index(10, 3, 1, 7), ==, 0);
  g_assert_cmpuint(search_page_index(10, 3, 1, 8), ==, 1);
  /* backward from page 3 of 10 wraps to the last page */
  g_assert_cmpuint(search_page_index(10, 3, -1, 0), ==, 3);
  g_assert_cmpuint(search_page_index(10, 3, -1, 3), ==, 0);
  g_assert_cmpuint(search_page_index(10, 3, -1, 4), ==, 9);
  /* backward from the first page */
  g_assert_cmpuint(search_page_index(10, 0, -1, 0), ==, 0);
  g_assert_cmpuint(search_page_index(10, 0, -1, 1), ==, 9);
}

/* build a per-page state array: arguments are (len, current) pairs, -1,-1 for empty pages */
#define STATES(...)                                                                                                    \
  ((const zathura_page_search_state_t[]){__VA_ARGS__}),                                                                \
      sizeof((zathura_page_search_state_t[]){__VA_ARGS__}) / sizeof(zathura_page_search_state_t)

static void test_select_target_advance(void) {
  unsigned int page = 1234;
  int idx           = -99;

  /* no results anywhere */
  g_assert_false(search_select_target(STATES({.num_results = 0, .current = -1}, {.num_results = 0, .current = -1}), 1,
                                      1, false, &page, &idx));

  /* pages whose selected result was cleared (current == -1) are skipped, as in
   * the original navigation: they are only re-entered by a fresh search */
  const zathura_page_search_state_t pages[] = {
      {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {4, -1},
  };
  g_assert_false(search_select_target(pages, 6, 1, 1, false, &page, &idx));

  /* advance within the current page (forward and backward) */
  const zathura_page_search_state_t cur[] = {{3, 1}};
  g_assert_true(search_select_target(cur, 1, 0, 1, false, &page, &idx));
  g_assert_cmpint(idx, ==, 2);
  g_assert_true(search_select_target(cur, 1, 0, -1, false, &page, &idx));
  g_assert_cmpint(idx, ==, 0);

  /* single-page documents wrap around within the page */
  const zathura_page_search_state_t one[] = {{3, 2}};
  g_assert_true(search_select_target(one, 1, 0, 1, false, &page, &idx));
  g_assert_cmpint(idx, ==, 0);
  const zathura_page_search_state_t one_bwd[] = {{3, 0}};
  g_assert_true(search_select_target(one_bwd, 1, 0, -1, false, &page, &idx));
  g_assert_cmpint(idx, ==, 2);
}

static void test_select_target_cross_page(void) {
  unsigned int page = 1234;
  int idx           = -99;

  /* running off the end continues on the nearest following page with results */
  const zathura_page_search_state_t fwd[] = {{3, 2}, {0, -1}, {0, -1}, {0, -1}, {2, -1}, {0, -1}, {5, -1}};
  g_assert_true(search_select_target(fwd, 7, 0, 1, false, &page, &idx));
  g_assert_cmpuint(page, ==, 4);
  g_assert_cmpint(idx, ==, 0);

  /* ... wrapping around the document end if needed */
  const zathura_page_search_state_t wrap[] = {{3, 1}, {0, -1}, {0, -1}, {2, 1}};
  g_assert_true(search_select_target(wrap, 4, 3, 1, false, &page, &idx));
  g_assert_cmpuint(page, ==, 0);
  g_assert_cmpint(idx, ==, 0);

  /* backward: start at the last result of the previous page */
  const zathura_page_search_state_t bwd[] = {{0, -1}, {0, -1}, {2, -1}, {0, -1}, {3, 0}};
  g_assert_true(search_select_target(bwd, 5, 4, -1, false, &page, &idx));
  g_assert_cmpuint(page, ==, 2);
  g_assert_cmpint(idx, ==, 1);

  /* exhausted: nothing else anywhere */
  const zathura_page_search_state_t end[] = {{0, -1}, {0, -1}, {0, -1}, {2, 1}};
  g_assert_false(search_select_target(end, 4, 3, 1, false, &page, &idx));
}

static void test_select_target_new_search(void) {
  unsigned int page = 1234;
  int idx           = -99;

  /* fresh search jumps to the first match in scan order, not advancing */
  const zathura_page_search_state_t cur[] = {{2, 1}, {0, -1}, {4, 2}};
  g_assert_true(search_select_target(cur, 3, 0, 1, true, &page, &idx));
  g_assert_cmpuint(page, ==, 0);
  g_assert_cmpint(idx, ==, 0);

  /* backward fresh search starts at the last hit */
  g_assert_true(search_select_target(cur, 3, 0, -1, true, &page, &idx));
  g_assert_cmpuint(page, ==, 0);
  g_assert_cmpint(idx, ==, 1);

  /* pages without a selected result are skipped, even in fresh searches */
  const zathura_page_search_state_t unset[] = {{0, -1}, {3, -1}};
  g_assert_false(search_select_target(unset, 2, 0, 1, false, &page, &idx));

  /* invalid input */
  g_assert_false(search_select_target(NULL, 3, 0, 1, false, &page, &idx));
  g_assert_false(search_select_target(cur, 0, 0, 1, false, &page, &idx));
}

static void test_search_results_stale(void) {
  /* a different pattern invalidates stored results */
  g_assert_true(search_results_stale("what", "bug"));
  g_assert_true(search_results_stale(NULL, "bug"));
  g_assert_true(search_results_stale("bug", NULL));
  g_assert_true(search_results_stale("", "bug"));
  /* same pattern keeps them */
  g_assert_false(search_results_stale("bug", "bug"));
  g_assert_false(search_results_stale(NULL, NULL));
}

static void test_text_match(void) {
  g_assert_cmpuint(zathura_text_match("todo", "todo"), ==, ZATHURA_MATCH_EXACT);
  g_assert_cmpuint(zathura_text_match("todo", "to"), ==, ZATHURA_MATCH_PREFIX);
  g_assert_cmpuint(zathura_text_match("chapter-1-intro", "-1-in"), ==, ZATHURA_MATCH_SUBSTRING);
  g_assert_cmpuint(zathura_text_match("chapter-1-intro", "cint"), ==, ZATHURA_MATCH_SUBSEQUENCE);
  g_assert_cmpuint(zathura_text_match("Theory", "theory"), ==, ZATHURA_MATCH_EXACT);
  g_assert_cmpuint(zathura_text_match("theory", "THEO"), ==, ZATHURA_MATCH_PREFIX);
  g_assert_cmpuint(zathura_text_match("a-theory", "THEORY"), ==, ZATHURA_MATCH_SUBSTRING);
  g_assert_cmpuint(zathura_text_match("t-h-e-o-r-y", "Theory"), ==, ZATHURA_MATCH_SUBSEQUENCE);
  g_assert_cmpuint(zathura_text_match("anything", ""), ==, ZATHURA_MATCH_PREFIX);
  g_assert_cmpuint(zathura_text_match("foo", "bar"), ==, ZATHURA_MATCH_NONE);
  g_assert_cmpuint(zathura_text_match(NULL, "foo"), ==, ZATHURA_MATCH_NONE);
  g_assert_cmpuint(zathura_text_match("foo", NULL), ==, ZATHURA_MATCH_NONE);
}

static void test_page_number_write(void) {
  g_assert_false(zathura_page_number_write(NULL, 1));
  g_autofree char* dir = g_dir_make_tmp("zathura-page-test-XXXXXX", NULL);
  g_assert_nonnull(dir);
  g_autofree char* filename = g_build_filename(dir, "nested", "page", NULL);
  g_assert_true(zathura_page_number_write(filename, 7));
  g_autofree gchar* contents = NULL;
  gsize length               = 0;
  g_assert_true(g_file_get_contents(filename, &contents, &length, NULL));
  g_assert_cmpstr(contents, ==, "7\n");
  g_assert_true(zathura_page_number_write(filename, 42));
  g_assert_true(g_file_get_contents(filename, &contents, &length, NULL));
  g_assert_cmpstr(contents, ==, "42\n");
}

static void test_page_text_write(void) {
  g_autofree char* dir = g_dir_make_tmp("zathura-page-text-test-XXXXXX", NULL);
  g_assert_nonnull(dir);
  g_assert_false(zathura_page_text_write(NULL, "hello"));
  g_autofree char* filename = g_build_filename(dir, "nested", "text", NULL);
  g_assert_true(zathura_page_text_write(filename, "hello\nworld\n"));
  g_autofree gchar* contents = NULL;
  gsize length               = 0;
  g_assert_true(g_file_get_contents(filename, &contents, &length, NULL));
  g_assert_cmpstr(contents, ==, "hello\nworld\n");
  g_assert_true(zathura_page_text_write(filename, NULL));
  g_assert_true(g_file_get_contents(filename, &contents, &length, NULL));
  g_assert_cmpstr(contents, ==, "");
  g_assert_true(zathura_page_text_write(filename, "other"));
  g_assert_true(g_file_get_contents(filename, &contents, &length, NULL));
  g_assert_cmpstr(contents, ==, "other");
}

static void test_tracking_should_write(void) {
  /* disabled: never write even if page changed */
  g_assert_false(zathura_tracking_should_write(5, 4, false, false, false, false));
  g_assert_false(zathura_tracking_should_write(5, UINT_MAX, false, false, false, false));
  g_assert_false(zathura_tracking_should_write(5, 5, false, false, false, false));

  /* enabled and page changed: must write */
  g_assert_true(zathura_tracking_should_write(5, 4, true, false, true, false));
  g_assert_true(zathura_tracking_should_write(5, 4, false, true, false, true));
  g_assert_true(zathura_tracking_should_write(5, 4, true, true, true, true));
  g_assert_true(zathura_tracking_should_write(1, UINT_MAX, true, false, false, false));

  /* same page, not toggled: must NOT write (bug: previously rewrote every
   * statusbar update → high CPU) */
  g_assert_false(zathura_tracking_should_write(5, 5, true, false, true, false));
  g_assert_false(zathura_tracking_should_write(5, 5, false, true, false, true));
  g_assert_false(zathura_tracking_should_write(5, 5, true, true, true, true));

  /* same page but tracking just toggled on: must write */
  g_assert_true(zathura_tracking_should_write(5, 5, true, false, false, false));
  g_assert_true(zathura_tracking_should_write(5, 5, false, true, false, false));
  g_assert_true(zathura_tracking_should_write(5, 5, true, true, false, false));
  g_assert_false(
      zathura_tracking_should_write(5, 5, true, false, true, false)); /* only text toggled, page already enabled */
  g_assert_true(zathura_tracking_should_write(5, 5, false, true, false, false));
  /* toggled off: still no write */
  g_assert_false(zathura_tracking_should_write(5, 5, false, false, true, false));

  /* regression: busy retry when write fails — should_writereturns false on
   * second identical call, caller will advance last_page to avoid loop */
  g_assert_true(zathura_tracking_should_write(1, UINT_MAX, true, true, false, false));
  g_assert_false(zathura_tracking_should_write(1, 1, true, true, true, true));
}

static void test_visible_pages_should_update(void) {
  zathura_visible_state_t base = {0};
  base.document                = (void*)0x1;
  base.pos_x                   = 0.5;
  base.pos_y                   = 0.5;
  base.zoom                    = 1.0;
  base.rotation                = 0;
  base.pages_per_row           = 1;
  base.number_of_pages         = 100;
  base.first_page_column       = 1;
  base.view_width              = 800;
  base.view_height             = 600;

  zathura_visible_state_t same = base;
  g_assert_false(zathura_visible_pages_should_update(&base, &same));

  /* null handling */
  g_assert_true(zathura_visible_pages_should_update(NULL, &base));
  g_assert_true(zathura_visible_pages_should_update(&base, NULL));

  /* document pointer change */
  zathura_visible_state_t doc2 = base;
  doc2.document                = (void*)0x2;
  g_assert_true(zathura_visible_pages_should_update(&base, &doc2));

  /* each field that affects visibility must trigger update */
  zathura_visible_state_t mod = base;
  mod.pos_x                   = 0.6;
  g_assert_true(zathura_visible_pages_should_update(&base, &mod));
  mod       = base;
  mod.pos_y = 0.6;
  g_assert_true(zathura_visible_pages_should_update(&base, &mod));
  mod      = base;
  mod.zoom = 2.0;
  g_assert_true(zathura_visible_pages_should_update(&base, &mod));
  mod          = base;
  mod.rotation = 90;
  g_assert_true(zathura_visible_pages_should_update(&base, &mod));
  mod               = base;
  mod.pages_per_row = 2;
  g_assert_true(zathura_visible_pages_should_update(&base, &mod));
  mod                 = base;
  mod.number_of_pages = 101;
  g_assert_true(zathura_visible_pages_should_update(&base, &mod));
  mod                   = base;
  mod.first_page_column = 2;
  g_assert_true(zathura_visible_pages_should_update(&base, &mod));
  mod            = base;
  mod.view_width = 801;
  g_assert_true(zathura_visible_pages_should_update(&base, &mod));
  mod             = base;
  mod.view_height = 601;
  g_assert_true(zathura_visible_pages_should_update(&base, &mod));

  /* bug: idling with same state must NOT update (50% CPU regression) */
  for (int i = 0; i < 100; i++) {
    g_assert_false(zathura_visible_pages_should_update(&base, &same));
  }
}

static ZathuraIndexElementObject* make_index_element(const char* title, GListStore* children) {
  ZathuraIndexElementObject* item = g_object_new(ZATHURA_TYPE_INDEX_ELEMENT_OBJECT, NULL);
  item->title                     = g_markup_escape_text(title, -1);
  if (children != NULL) {
    item->children = g_object_ref(children);
    g_object_unref(children);
  }
  return item;
}

static void index_store_append(GListStore* store, const char* title, GListStore* children) {
  g_list_store_append(store, make_index_element(title, children));
}

static void free_index_tree(GListModel* model) {
  const guint n = g_list_model_get_n_items(model);
  for (guint i = 0; i < n; ++i) {
    ZathuraIndexElementObject* obj = g_list_model_get_item(model, i);
    if (obj != NULL) {
      if (obj->children != NULL) {
        free_index_tree(G_LIST_MODEL(obj->children));
      }
      g_free(obj->title);
      g_object_unref(obj);
    }
  }
  g_object_unref(model);
}

static void test_index_search_order_and_relevance(void) {
  GListStore* sub = g_list_store_new(ZATHURA_TYPE_INDEX_ELEMENT_OBJECT);
  index_store_append(sub, "Nested THEORY here", NULL);
  GListStore* root = g_list_store_new(ZATHURA_TYPE_INDEX_ELEMENT_OBJECT);
  index_store_append(root, "Theory intro", sub);
  index_store_append(root, "Other", NULL);
  index_store_append(root, "Appendix", NULL);

  GHashTable* relevant     = NULL;
  g_autoptr(GList) matches = zathura_index_search(G_LIST_MODEL(root), "theory", &relevant);
  g_assert_nonnull(matches);
  g_assert_nonnull(relevant);
  g_assert_cmpuint(g_list_length(matches), ==, 2);
  g_assert_cmpstr(((ZathuraIndexElementObject*)matches->data)->title, ==, "Nested THEORY here");
  g_assert_cmpstr(((ZathuraIndexElementObject*)matches->next->data)->title, ==, "Theory intro");
  g_assert_true(g_hash_table_contains(relevant, matches->data));
  g_assert_true(g_hash_table_contains(relevant, matches->next->data));
  /* ancestor of the nested match must be in the relevance set */
  ZathuraIndexElementObject* root0 = g_list_model_get_item(G_LIST_MODEL(root), 0);
  g_assert_true(g_hash_table_contains(relevant, root0));
  g_object_unref(root0);
  g_hash_table_unref(relevant);

  g_autoptr(GList) none = zathura_index_search(G_LIST_MODEL(root), "missing", &relevant);
  g_assert_null(none);
  g_assert_nonnull(relevant);
  g_assert_cmpuint(g_hash_table_size(relevant), ==, 0);
  g_hash_table_unref(relevant);

  g_autoptr(GList) all = zathura_index_search(G_LIST_MODEL(root), "", &relevant);
  g_assert_null(all);
  g_assert_null(relevant);

  free_index_tree(G_LIST_MODEL(root));
}

static void test_index_search_no_query_or_no_match(void) {
  GHashTable* relevant = NULL;
  g_autoptr(GList) r1  = zathura_index_search(NULL, "foo", &relevant);
  g_assert_null(r1);
  g_assert_null(relevant);

  GListStore* root = g_list_store_new(ZATHURA_TYPE_INDEX_ELEMENT_OBJECT);
  index_store_append(root, "Hello", NULL);
  g_autoptr(GList) r2 = zathura_index_search(G_LIST_MODEL(root), "missing", &relevant);
  g_assert_null(r2);
  g_assert_nonnull(relevant);
  g_assert_cmpuint(g_hash_table_size(relevant), ==, 0);
  g_hash_table_unref(relevant);
  free_index_tree(G_LIST_MODEL(root));
}

int main(int argc, char* argv[]) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/utils/file_valid_extension", test_file_valid_extension);
  g_test_add_func("/utils/first_page_column", test_first_page_column);
  g_test_add_func("/utils/parse_search_limit", test_parse_search_limit);
  g_test_add_func("/utils/search_limit_stops", test_search_limit_stops);
  g_test_add_func("/utils/search_page_index", test_search_page_index);
  g_test_add_func("/utils/select_target/advance", test_select_target_advance);
  g_test_add_func("/utils/select_target/cross_page", test_select_target_cross_page);
  g_test_add_func("/utils/select_target/new_search", test_select_target_new_search);
  g_test_add_func("/utils/search_results_stale", test_search_results_stale);
  g_test_add_func("/utils/text_match", test_text_match);
  g_test_add_func("/utils/page_number_write", test_page_number_write);
  g_test_add_func("/utils/page_text_write", test_page_text_write);
  g_test_add_func("/utils/tracking_should_write", test_tracking_should_write);
  g_test_add_func("/utils/visible_pages_should_update", test_visible_pages_should_update);
  g_test_add_func("/index/search/order_and_relevance", test_index_search_order_and_relevance);
  g_test_add_func("/index/search/no_query_or_no_match", test_index_search_no_query_or_no_match);
  return g_test_run();
}
