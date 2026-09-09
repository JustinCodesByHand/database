
#define ASSERT_EQ_INT(expected, actual)                                       \
    do {                                                                      \
        long long _e = (long long)(expected), _a = (long long)(actual);       \
        if (_e != _a) FAIL("expected %lld, got %lld", _e, _a);                \
    } while (0)

#define ASSERT_EQ_STR(expected, actual)                                       \
    do {                                                                      \
        const char *_e = (expected), *_a = (actual);                          \
        if (_a == NULL) FAIL("expected \"%s\", got NULL", _e);                 \
        if (strcmp(_e, _a) != 0)                                              \
            FAIL("expected \"%s\", got \"%s\"", _e, _a);                       \
    } while (0)

#define ASSERT_EQ_MEM(expected, actual, n)                                    \
    do {                                                                      \
        if (memcmp((expected), (actual), (n)) != 0)                           \
            FAIL("memory differs over %zu bytes", (size_t)(n));               \
    } while (0)

#define ASSERT_STR_CONTAINS(haystack, needle)                                 \
    do {                                                                      \
        const char *_h = (haystack);                                          \
        if (_h == NULL || strstr(_h, (needle)) == NULL)                       \
            FAIL("expected to find \"%s\" in \"%s\"", (needle),                \
                 _h ? _h : "(null)");                                          \
    } while (0)

#define ASSERT_NULL(ptr)     ASSERT((ptr) == NULL)
#define ASSERT_NOT_NULL(ptr) ASSERT((ptr) != NULL)

/* Runs one test function. */
#define RUN_TEST(fn)                                                          \
    do {                                                                      \
        current_test = #fn;                                                   \
        int before = tests_failed;                                            \
        tests_run++;                                                          \
        fn();                                                                 \
        if (tests_failed == before) printf("  ok   %s\n", #fn);               \
    } while (0)

#define SUITE(name) printf("\n%s\n", name)

#endif /* MINIDB_TEST_H */