#include <stdio.h>
#include <string.h>
#include <unity.h>

static void print_banner(const char *text);

void app_main(void)
{
    /* These are the different ways of running registered tests.
     * In practice, only one of them is usually needed.
     *
     * UNITY_BEGIN() and UNITY_END() calls tell Unity to print a summary
     * (number of tests executed/failed/ignored) of tests executed between these calls.
     */

    print_banner("Running all the registered tests");
    UNITY_BEGIN();
    unity_run_all_tests();
    // unity_run_tests_by_tag("[relay]", false);
    UNITY_END();
}

static void print_banner(const char *text)
{
    printf("\n#### %s #####\n\n", text);
}

// to allow 'log timestamp' configuration setting
char *esp_log_system_timestamp() {
    return "unknown time";
}