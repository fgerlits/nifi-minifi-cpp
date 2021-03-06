/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
    *
    *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include "api/nanofi.h"
#include "api/ecu.h"
#include "core/string_utils.h"
#include "core/cstructs.h"
#include "core/file_utils.h"
#include "core/flowfiles.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>

int main(int argc, char** argv) {
    if (argc < 7) {
        printf("Error: must run ./log_aggregator <file> <interval> <delimiter> <hostname> <tcp port number> <nifi port uuid>\n");
        exit(1);
    }

    tailfile_input_params input_params = init_logaggregate_input(argv);

    uint64_t intrvl = 0;
    uint64_t port_num = 0;
    if (validate_input_params(&input_params, &intrvl, &port_num) < 0) {
        return 1;
    }

    setup_signal_action();
    nifi_proc_params params = setup_nifi_processor(&input_params, "LogAggregator", on_trigger_logaggregator);

    set_standalone_property(params.processor, "file_path", input_params.file);
    set_standalone_property(params.processor, "delimiter", input_params.delimiter);

    struct CRawSiteToSiteClient * client = createClient(input_params.instance, port_num, input_params.nifi_port_uuid);

    char uuid_str[37];
    get_proc_uuid_from_processor(params.processor, uuid_str);

    while (!stopped) {
        flow_file_record * new_ff = invoke(params.processor);
        struct processor_params * pp = NULL;
        HASH_FIND_STR(procparams, uuid_str, pp);
        if (pp) {
            transmit_payload(client, pp->ff_list, 0);
            delete_all_flow_files_from_proc(uuid_str);
        }
        free_flowfile(new_ff);
        sleep(intrvl);
    }

    printf("log aggregator processor stopped\n");
    if (client) {
        destroyClient(client);
    }
    clear_content_repo(params.instance);
    delete_all_flow_files_from_proc(uuid_str);
    free_standalone_processor(params.processor);
    free_nanofi_instance(params.instance);
    free_proc_params(uuid_str);
    return 0;
}
