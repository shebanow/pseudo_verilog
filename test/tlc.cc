/*
 * Copyright (c) 2023 Michael C Shebanow
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <iostream>
#include <getopt.h>
#include "pv.h"
#include "tlc.h"

// program name
char* prog_name;

// program options
bool opt_verbose = false;
int opt_iteration_limit = -1;
int opt_clock_limit = 32;
bool opt_vcd_enable = false;
std::string opt_vcd_file_name;
int opt_vcd_start_clock = -1;
int opt_vcd_stop_clock = -1;

// from getopt manual: 2nd argument can be:
//      no_argument: no argument, 3rd is NULL, 4th is default value
//      required_argument: argument is required, 3rd is &int flag (or NULL if string: use optarg), 4th is default value
static struct option options[] {
    // generic help; must ALWAYS first
    { "help", no_argument, NULL, 0 },
    { "verbose", no_argument, NULL, 0 },

    // other long arguments
    { "iterations", required_argument, &opt_iteration_limit, 10 },
    { "clocks", required_argument, &opt_clock_limit, 32 },
    { "vcd", required_argument, NULL, 0 },
    { "vcd_start", required_argument, &opt_vcd_start_clock, 0 },
    { "vcd_stop", required_argument, &opt_vcd_stop_clock, -1 },

    // null termination
    { 0, 0, 0, 0 }
};

void usage(int argc, char** argv) {
    std::cerr << "usage: " << argv[0] << "    where options are:" << std::endl;
    std::cerr << "        -h, --help\t:\tprints help" << std::endl;
    std::cerr << "        -v, --verbose\t:\tbe verbose" << std::endl;
    std::cerr << "        -L{n}, --iterations={n}\t:\tsets the max number of eval() iterations per clock cycle" << std::endl;
    std::cerr << "        -c{n}, --clocks={n}\t:\tsets the max number of clock cycles" << std::endl;
    std::cerr << "        --vcd <file>\t:\tdump a VCD file for the simulation" << std::endl;
    std::cerr << "        --vcd_start=<n>\t:\tset a start time for VCD dumping (default = 0)" << std::endl;
    std::cerr << "        --vcd_stop=<n>\t:\tset a stop time for VCD dumping (default is none)" << std::endl;
    exit(1);
}

//
// Main program
//

int main(int argc, char **argv) {
    tlc_tb* dut_tb;
    bool opt_vcd_enable = false;
    std::string opt_vcd_file_name;

    int ch;
    int option_index;

    // option processing
    prog_name = argv[0];
    while ((ch = getopt_long(argc, argv, "+hvL:c:", options, &option_index)) != -1) {
        switch (ch) {
        // NOTE: non-int long opts are not handled in the '0' case yet.
        // If any, will need to add a check for that and implement as a special check based on option_index
        // (strcmp(options[option_index].name, "<option>") == 0).
        case 0:
            if (!option_index) usage(argc, argv);
            else if (option_index == 1) 
                opt_verbose = optarg ? (atoi(optarg) != 0) : true;
            else if (option_index == 4) { // vcd
                opt_vcd_enable = true;
                opt_vcd_file_name = optarg;
            } else if (options[option_index].flag != NULL)
                *options[option_index].flag = atoi(optarg);
            break;
        case 'v':
            opt_verbose = optarg ? atoi(optarg) : 1;
            break;
        case 'L':
            opt_iteration_limit = atoi(optarg);
            break;
        case 'c':
            opt_clock_limit = atoi(optarg);
            break;
        case 'h':
        case '?':
        case ':':
            usage(argc, argv);
            break;
        default:
            break;
        }
    }

    // Trim argc/argv of already processed args.
    argc -= optind;
    argv += optind;
    optind = 0;

    // Option error checking.
    if (opt_vcd_start_clock >= 0 && opt_vcd_stop_clock >= 0 && opt_vcd_start_clock >= opt_vcd_stop_clock) {
        std::cerr << "VCD start clock (" << opt_vcd_start_clock << ") must be less than stop clock (" << opt_vcd_stop_clock << ")\n";
        exit(1);
    }

    // If VCD is enabled, create the file.
    vcd::writer* vcd_file = NULL;
    if (opt_vcd_enable) {
        // Open VCD file.
        vcd_file = new vcd::writer(opt_vcd_file_name);
        if (!vcd_file->is_open()) {
            delete vcd_file;
            exit(1);
        }

        // handle VCD options
        if (opt_vcd_start_clock >= 0) 
            vcd_file->set_vcd_start_clock(opt_vcd_start_clock);
        if (opt_vcd_stop_clock >= 0)
            vcd_file->set_vcd_stop_clock(opt_vcd_stop_clock);

        // Set operating point and install VCD writer.
        vcd_file->set_operating_point(100e6, vcd::TS_time::t1, vcd::TS_unit::ns);
    }

    // Create testbench for TLC. Attach VCD file if it exists.
    // Handle simulator iteration options. Process remaining command line args and run the simulation.
    dut_tb = new tlc_tb("tlc_tb");
    if (opt_iteration_limit >= 0)
        dut_tb->set_iteration_limit(opt_iteration_limit);
    if (opt_clock_limit >= 0)
        dut_tb->set_cycle_limit(opt_clock_limit);
    if (vcd_file)
        dut_tb->set_vcd_writer(vcd_file);
    dut_tb->main(argc, argv);

    // sim complete
    if (vcd_file) delete vcd_file;
    delete dut_tb;
return 0;
}

