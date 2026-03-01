#include "umdns/table.h"

#include <stdio.h>

void umdns_table_print_results(const umdns_result_t *results, size_t result_count) {
    size_t idx;

    printf("%-28s %-24s %-28s %-18s %-6s %s\n", "INSTANCE", "HOST", "SERVICE", "ADDRESS", "PORT", "TXT");
    printf("%-28s %-24s %-28s %-18s %-6s %s\n", "----------------------------", "------------------------", "----------------------------", "------------------", "------", "------------------------------");

    for (idx = 0; idx < result_count; ++idx) {
        printf("%-28s %-24s %-28s %-18s %-6u %s\n",
               results[idx].instance,
               results[idx].hostname,
               results[idx].service_type,
               results[idx].address,
               results[idx].port,
               results[idx].txt);
    }
}
