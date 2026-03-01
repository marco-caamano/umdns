#ifndef UMDNS_TABLE_H
#define UMDNS_TABLE_H

#include <stddef.h>

#include "umdns/common.h"

void umdns_table_print_results(const umdns_result_t *results, size_t result_count);
size_t umdns_table_deduplicate_results(umdns_result_t *results, size_t result_count);

#endif
