/*
 * Simulator infrastructure source code.
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
#include "simulator.h"

// Simulator controls
int simulator::opt_iteration_limit = 10;
int simulator::opt_cycle_limit = 0;

// VCD related
int simulator::opt_vcd_start_clock = 0;
int simulator::opt_vcd_stop_clock = ~0;
vcd::writer* simulator::vcd_file = NULL;
int simulator::vcd_id_count = 0;

/*
 * Simulator namespace
 */

namespace simulator {

    // simulation controls
    bool exit_simulation = false;
    int run_exit_code = 0;

    // vcd flags
    bool had_stop_event = false;

    // master clock counter
    uint32_t clock_num = 0;

    // defined testbench to run
    Testbench *theTestbench = NULL;

    // runQ: set of modules to evaluate; use std::set so there are no duplicates
    std::set<const Module*> runQ;

    // function to add a module to the run queue
    void eval(const Module* m) {
        debugMsg(6, "eval %s\n", m->instanceName().c_str());
        (void) runQ.insert(runQ.end(), m);
    }

    // function to run simulation
    int run() {
        int iteration_count = 0;

        // if VCD start clock is spec'd, we need to effectively dump off now.
        if (simulator::opt_vcd_start_clock > 0)
            simulator::vcd_dumpoff();

        // run the simulation until cycle limit is reached (if any) or simulation naturally ends.
        assert(theTestbench != NULL);
        debugMsg(3, "Run(): start simulation (cycle limit = %d, iteration limit = %d)\n", opt_cycle_limit, opt_iteration_limit);
        for (clock_num = 1; !exit_simulation && (opt_cycle_limit <= 0 || clock_num <= opt_cycle_limit); clock_num++) {
            // run pre-clock edge against the loaded test bench
            theTestbench->pre_clock(clock_num);

            // handle VCD stop clock
            if (simulator::opt_vcd_stop_clock > 0 && simulator::opt_vcd_stop_clock == clock_num) {
                simulator::vcd_file->emit_tick(clock_num, true);
                vcd_dumpoff();
                had_stop_event = true;
            }

            // handle VCD start clock
            if (simulator::opt_vcd_start_clock > 0 && simulator::opt_vcd_start_clock == clock_num) {
                simulator::vcd_file->set_emitting_change(true);
                simulator::vcd_file->emit_tick(clock_num, true);
                simulator::vcd_dumpon();
            } else {
                simulator::vcd_file->emit_tick(clock_num, true);
                simulator::vcd_file->emit_clock(false, true);
            }

            // develop a clock edge, then evaluate propagation from that edge until propagation
            // stops or we run up against the iteration limit.
            RegisterBase::clock();
            if (runQ.empty()) {
                debugMsg(2, "At cycle %u, runQ is empty!\n", clock_num);
                return run_exit_code;
            }
            while (!runQ.empty()) {
                // if iteration limit in a clock exceeded, fail simulator
                if (iteration_count++ == opt_iteration_limit) {
                    std::stringstream err_str;
                    err_str << "Simulation failed: iteration limit exceeded at clock cycle " << clock_num;
                    throw std::runtime_error(err_str.str());
                }

                // make a copy of run queue, then clear it.
                debugMsg(6, "Run(): runQ depth was %lu\n", runQ.size());
                std::set<const Module*> to_do_list(runQ);
                runQ.clear();

                // iterate through to do list, updating each module
                // An FYI: inner loop has "*it" as the value of the iterator.
                // It is a pointer to a module, so we dereference that to execute the evaluation function.
                for (std::set<const Module*>::const_iterator it = to_do_list.begin(); it != to_do_list.end(); it++) {
                    Module *m = const_cast<Module*>(*it);
                    m->eval();
                }
                debugMsg(6, "Run(): runQ depth now %lu\n", runQ.size());
            }

            // negative edge clock calls
            if (simulator::vcd_file->get_emitting_change()) {
                for (std::set<const WireBase*>::const_iterator it = WireBase::wireList.begin(); it != WireBase::wireList.end(); it++)
                    const_cast<WireBase *>(*it)->vcd_neg_edge_update();
                simulator::vcd_file->emit_tick(clock_num, false);
                simulator::vcd_file->emit_clock(false, false);
            }

            // end of clock: clear iteration limit.
            // run post-clock edge against the loaded test bench
            iteration_count = 0;
            theTestbench->post_clock(clock_num);
        }

        // if had a VCD stop clock and it triggered, need to add final dump of 'x values
        if (had_stop_event) {
            simulator::vcd_file->set_emitting_change(true);
            simulator::vcd_file->emit_tick(clock_num, true);
            simulator::vcd_file->emit_clock(true, false);
            theTestbench->vcd_dumpoff();
        }

        // end sim run
        debugMsg(3, "Run(): end simulation, exit code = %d, cycle count %u\n", run_exit_code, clock_num);
        return run_exit_code;
    }

    void end_simulation(const int exit_code = 0) {
        run_exit_code = exit_code;
        exit_simulation = true;
    }

/*************************************************************************************************************************
 *                                                  -- SIMULATOR VCD SECTION GENERATORS --
 *************************************************************************************************************************/

void vcd_generate_header(const std::string& version, const vcd::TS_time time, const vcd::TS_unit units) {
    simulator::vcd_file->emit_header(version, time, units);
    theTestbench->vcd_definition(true);
    simulator::vcd_file->emit_end_definitions();
}

void vcd_dumpvars(const uint32_t clock_num) {
    assert(simulator::vcd_file->is_open());
    simulator::vcd_file->emit_tick(clock_num, true);
    simulator::vcd_file->emit_dumpvars();
    simulator::vcd_file->emit_clock(false, false);
    theTestbench->vcd_dumpvars();
    simulator::vcd_file->emit_dumpend();
    simulator::vcd_file->set_emitting_change(true);
}

void vcd_dumpon() {
    assert(simulator::vcd_file->is_open());
    simulator::vcd_file->emit_dumpon();
    simulator::vcd_file->emit_clock(false, true);
    theTestbench->vcd_dumpon();
    simulator::vcd_file->emit_dumpend();
    simulator::vcd_file->set_emitting_change(true);
}

void vcd_dumpoff() {
    assert(simulator::vcd_file->is_open());
    simulator::vcd_file->emit_dumpoff();
    simulator::vcd_file->emit_clock(true, false);
    theTestbench->vcd_dumpoff();
    simulator::vcd_file->emit_dumpend();
    simulator::vcd_file->set_emitting_change(false);
}

}   // end namespace simulator
