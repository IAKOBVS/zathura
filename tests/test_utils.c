/* SPDX-License-Identifier: Zlib */

#include "utils.h"

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
  return g_test_run();
}
