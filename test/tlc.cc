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
// #include <stdio.h>
// #include <stdlib.h>
// #include <iostream>
// #include <unistd.h>
// #include <functional>
// #include <memory>
// #include <strings.h>
#include <getopt.h>
#include "pv.h"
#include "tlc.h"

// program name
char* prog_name;

void usage(int argc, char** argv) {
    std::cerr << "usage: " << argv[0] << " <fmod options> <model name> <model options>\n    where <fmod options> are:" << std::endl;
    std::cerr << "        -h, --help\t:\tprints help" << std::endl;
    std::cerr << "        -v{n}, --verbose{=n}\t:\tbe verbose (at optional level)" << std::endl;
    std::cerr << "        -L{n}, --iterations={n}\t:\tsets the max number of eval() iterations per clock cycle" << std::endl;
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

    dut_tb = new tlc_tb("tlc_tb");
    argc--; argv++; optind = 0;
    dut_tb->main(argc, argv);

/*
    // if VCD is enabled, create the file
    if (simulator::opt_vcd_enable) {
        simulator::vcd_file = new vcd::writer(simulator::opt_vcd_file_name);
        if (!simulator::vcd_file->is_open()) {
            delete simulator::vcd_file;
            exit(1);
        }
    }

    // if VCD is enabled, generate the header and emit hierarchy definition ahead of simulation
    // fmod is using an assumed clock speed of 100 MHz with a 1ns timescale (10 ticks per clock)
    if (simulator::opt_vcd_enable) {
        simulator::vcd_file->set_clock_freq(100e6);
        simulator::vcd_generate_header("1.0", vcd::TS_time::t1, vcd::TS_unit::ns);
        simulator::vcd_dumpvars(0);
    }
*/

    // Run TLC test
    dut_tb->begin_test();
    try {
        dut_tb->simulation();
    } catch (const std::exception& e) {
        std::cerr << "Caught system error: " << e.what() << std::endl;
        exit(1);
    }
    dut_tb->end_test_pass("TLC passed after %d clocks\n", dut_tb->get_clock());

    // sim complete
    delete dut_tb;
    return 0;
}

