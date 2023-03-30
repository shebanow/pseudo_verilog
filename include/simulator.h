/**
 * Copyright (c) 2022 Ai Linear Inc. All Rights Reserved. Proprietary and Confidential.
 * @file simulator.h
 * @author m.shebanow
 * @date 2022/08/01
 * @brief FMOD simulator infrastructure
 */
#include <typeinfo>
#include <type_traits>
#include <stdexcept>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <set>
#include <map>
#include <random>
#include <ctime>
#include "pv.h"
#include "fixed_point.h"
#include "complex.h"

#ifndef _SIMULATOR_H_
#define _SIMULATOR_H_

/*************************************************************************************************************************
 *                                                  -- SIMULATOR --
 *************************************************************************************************************************/

/*
 * Simulator defines
 */

namespace simulator {
    // Function to add a module to the update list
    extern void eval(const Module* m);

    // run() constraints:
    extern int opt_iteration_limit;
    extern int opt_cycle_limit;

    // master clock counter
    extern uint32_t clock_num;

    // VCD CLI variables
    extern bool opt_vcd_enable;
    extern std::string opt_vcd_file_name;
    extern int opt_vcd_start_clock;
    extern int opt_vcd_stop_clock;

    // VCD file controls
    extern vcd::writer* vcd_file;

    // VCD section generators
    extern int vcd_id_count;
    extern void vcd_generate_header(const std::string& version, const vcd::TS_time time, const vcd::TS_unit units);
    extern void vcd_dumpvars(const uint32_t clock_num);
    extern void vcd_dumpon();
    extern void vcd_dumpoff();

    // Function to run simulation. 
    // Implicitly uses opt_iteration_limit and opt_cycle_limit to control run cycle behavior.
    // Returns 0 if no issues or exit code.
    extern int run();

    // testbenches can call this function when simulation should end.
    extern void end_simulation(const int exit_code);
}

/*************************************************************************************************************************
 *                                                  -- VCD RELATED --
 *************************************************************************************************************************/

namespace vcd {
    extern void dump_module_definitions(const Module* m, const bool def_clk);
    extern void emit_vars(const Module* m);
    extern void vcd_dumpvars();
}

/*
 * URV (uniform random variable) class.
 * Usage: 
 *      - constructor can specifiy an optional seed
 *      - functor call to generate a URV value.
 */

class URV {
public:
    URV(const unsigned seed = 0) {
        rv = new std::uniform_real_distribution<>(0.0, 1.0);
        gen.seed(seed);
    }
    virtual ~URV() { delete rv; }
    inline const double operator()() { return (*rv)(gen); }

private:
    static std::random_device rd;
    static std::mt19937 gen;
    std::uniform_real_distribution<> *rv;
};

/*
 * Additional simulator defines
 */

namespace simulator {
    // defined testbench to run
    extern Testbench *theTestbench;
}

#endif // _SIMULATOR_H_
