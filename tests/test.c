/*
 * Copyright (c) 2020 Elastos Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <signal.h>

#include <CUnit/Basic.h>

#include "config.h"
#include "suites/suites.h"

extern const char *config_file;

static
void signal_handler(int signum)
{
    exit(-1);
}

int test_main(int argc, char *argv[])
{
    CU_pSuite pSuite;
    CU_TestInfo *ti;
    int suites_cnt, cases_cnt, fail_cnt;
    int i, j;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (!load_config(config_file)) {
        fprintf(stderr, "Loading configure failed!\n");
        return -1;
    }

    if (CU_initialize_registry() != CUE_SUCCESS) {
        free_config();
        return CU_get_error();
    }

    suites_cnt = sizeof(suites) / sizeof(suites[0]) - 1;
    for (i = 0; i < suites_cnt; i++) {
        pSuite = CU_add_suite(suites[i].strName,
                              suites[i].pInit,
                              suites[i].pClean);
        if (!pSuite) {
            free_config();
            CU_cleanup_registry();
            return CU_get_error();
        }

        ti = suites[i].pCases();
        for (cases_cnt = 0; ti[cases_cnt].pName; cases_cnt++) ;

        for (j = 0; j < cases_cnt; j++) {
            if (CU_add_test(pSuite, ti[j].pName,
                            ti[j].pTestFunc) == NULL) {
                free_config();
                CU_cleanup_registry();
                return CU_get_error();
            }
        }
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);

    CU_basic_run_tests();

    fail_cnt = CU_get_number_of_tests_failed();
    if (fail_cnt > 0)
        fprintf(stderr, "Failure Case: %d\n", fail_cnt);

    free_config();
    CU_cleanup_registry();

    return fail_cnt;
}
