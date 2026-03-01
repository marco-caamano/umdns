#include "umdns/table.h"

#include <stdio.h>
#include <string.h>

static int umdns_results_equal(const umdns_result_t *a, const umdns_result_t *b) {
    return strcmp(a->instance, b->instance) == 0 &&
           strcmp(a->hostname, b->hostname) == 0 &&
           strcmp(a->service_type, b->service_type) == 0 &&
           strcmp(a->address, b->address) == 0 &&
           a->port == b->port &&
           strcmp(a->txt, b->txt) == 0 &&
           a->rrtype == b->rrtype;
}

size_t umdns_table_deduplicate_results(umdns_result_t *results, size_t result_count) {
    size_t write_idx = 0;
    size_t read_idx;
    size_t check_idx;
    int is_duplicate;

    for (read_idx = 0; read_idx < result_count; ++read_idx) {
        is_duplicate = 0;
        for (check_idx = 0; check_idx < write_idx; ++check_idx) {
            if (umdns_results_equal(&results[read_idx], &results[check_idx])) {
                is_duplicate = 1;
                break;
            }
        }
        
        if (!is_duplicate) {
            if (write_idx != read_idx) {
                memcpy(&results[write_idx], &results[read_idx], sizeof(umdns_result_t));
            }
            write_idx++;
        }
    }
    
    return write_idx;
}

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
