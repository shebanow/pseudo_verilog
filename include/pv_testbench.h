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

 #ifndef _PV_TESTBENCH_H_
 #define _PV_TESTBENCH_H_

/*
 * The Testbench class is used as a template for constructing testbenches to test modules in a model.
 * As a Testbench is a subclass of Module, testbenches have an eval() function as well, called upon changes to outputs of
 * the DUT it is testing. Also like Module, a Testbench can comtain other simulatable (i.e., clockable) components and 
 * be sensitized accordingly. Testbenches differ from regular modules in that there is a main() function that accepts
 * command line arguments to be processed as well as pre_clock() and post_clock() functions, and most importantly, a
 * simulation function which actually does all the clocking. The main() function initializes the testbench, processing
 * remaining command line arguments. The pre_clock() and post_clock() functions are then called at the start and end
 * of each clock cycle respectively. Finally, the simulation() function runs the model for as many clocks as required or
 * allowed, driving the eval functions as needed.
 */

struct Testbench : public Module {
    // Constructors, std::string and char* variants.
    Testbench(const std::string& str) : Module(NULL, str) { constructor_common(); }
    Testbench(const char* str) : Module(NULL, str) { constructor_common(); }

    // Main method that must be overloaded to implement argument processing, other construction time init.
    // Called after construction and before simulation().
    virtual void main(int argc, char** argv) = 0;

    // Optional pre and post clock calls to overload.
    virtual void pre_clock(const uint32_t cycle_num) {}
    virtual void post_clock(const uint32_t cycle_num) {}

    /* 
     * Running a simulation. Code below runs all test cases. 
     *
     * Parameters controlling a simulation:
     * - set_vcd_writer(): point to a VCD writer class (by default NULL => no VCD dump)
     *      - writer->set_vcd_start_clock(): when to start dumping to a VCD
     *      - writer->set_vcd_stop_clock(): when to stop dumping to a VCD; vcd_stop_clock > vcd_start_clock
     * - set_idle_limit(): set a limit on the number of clock cycles in which no activity takes place; -1 => no limit
     * - set_cycle_limit(): set a limit on the number of clocks we can run; -1 => no limit
     * - set_iteration_limit(): set a limit on the number of eval() iterations per clock; -1 => no limit
     * - end_simulation(): call when you want to end a simulation now.
     */

    // Simulation parameter getters.
    inline const int32_t get_cycle_limit() const { return opt_cycle_limit; }
    inline const int32_t get_iteration_limit() const { return opt_iteration_limit; }
    inline const int32_t get_idle_limit() const { return opt_idle_limit; }
    inline const vcd::writer* get_vcd_writer() const { return writer; }

    // Simulation parameter setters.
    inline void set_vcd_writer(const vcd::writer* w) { writer = const_cast<vcd::writer*>(w); }
    inline void set_idle_limit(const int32_t idle_limit) { opt_idle_limit = idle_limit; }
    inline void set_cycle_limit(const int32_t cycle_limit) { opt_cycle_limit = cycle_limit; }
    inline void set_iteration_limit(const int32_t iteration_limit) { opt_iteration_limit = iteration_limit; }

    // Simulation runtime getter.
    inline const uint32_t get_clock() const { return clock_num; }

    // Simulation run time control.
    inline void end_simulation() { exit_simulation = true; }

    // The main simulation method.
    void simulation() {
        uint32_t idle_cycles = 0;
        uint32_t iteration_count = 0;
        bool had_stop_event = false;
        exit_simulation = false;

        // If we are writing a VCD, generate header and definitions, initial state.
        // If dump start clock is positive non-zero, also execute a VCD dumpoff() command.
        if (writer != NULL && writer->is_open()) {
            vcd_generate_header();
            vcd_dumpvars(0);
            if (writer->get_vcd_start_clock() > 0)
                writer->vcd_dumpoff(this);
        }

        // Run simulation cycles.
        for (clock_num = 1; !exit_simulation && (opt_cycle_limit <= 0 || clock_num <= opt_cycle_limit); clock_num++) {
            // run pre-clock edge against the loaded test bench
            this->pre_clock(clock_num);

            // If VCD dumps are active, handle start/stop clock events
            if (writer != NULL && writer->is_open()) {
                // Handle VCD stop clock.
                if (writer->get_vcd_stop_clock() > 0 && writer->get_vcd_stop_clock() == clock_num) {
                    writer->emit_pos_edge_tick(clock_num);
                    writer->vcd_dumpoff(this);
                    had_stop_event = true;
                }

                // Handle VCD start clock.
                if (writer->get_vcd_start_clock() > 0 && writer->get_vcd_start_clock() == clock_num) {
                    writer->set_emitting_change(true);
                    writer->emit_pos_edge_tick(clock_num);
                    writer->vcd_dumpon(this);
                } else {
                    writer->emit_pos_edge_tick(clock_num);
                    writer->emit_pos_edge_clock();
                }
            }

            // Clock all flops.
            std::ostream* vcd_stream = (writer && writer->is_open() && writer->get_emitting_change()) ? writer->get_stream() : NULL;
            set_r_data_t& rlist = this->global.registerList();
            for (set_r_iter_t it = rlist.begin(); it != rlist.end(); it++)
                const_cast<RegisterBase*>(*it)->pos_edge(vcd_stream);

            // Now, based on changes casued by register updates, propagate until idle.
            if (opt_idle_limit > 0 && this->global.runq().empty() && ++idle_cycles == opt_idle_limit) {
                std::stringstream err_str;
                err_str << "Simulation failed: idle cycle limit exceeded at clock cycle " << clock_num;
                throw std::runtime_error(err_str.str());
            }
            while (!this->global.runq().empty()) {
                // We were non-idle, so set idles cycles to 0.
                idle_cycles = 0;

                // If iteration limit in a clock exceeded, fail simulator.
                if (opt_iteration_limit > 0 && iteration_count++ == opt_iteration_limit) {
                    std::stringstream err_str;
                    err_str << "Simulation failed: iteration limit exceeded at clock cycle " << clock_num;
                    throw std::runtime_error(err_str.str());
                }

                // Make a copy of run queue, then clear it.
                std::set<const Module*> to_do_list(this->global.runq());
                this->global.runq().clear();

                // Iterate through to do list, updating each module
                for (std::set<const Module*>::const_iterator it = to_do_list.begin(); it != to_do_list.end(); it++)
                    const_cast<Module*>(*it)->eval();
            }

            // Negative edge clock calls.
            if (writer != NULL && writer->is_open() && writer->get_emitting_change()) {
                for (set_w_iter_t it = this->global.wireList().begin(); it != this->global.wireList().end(); it++)
                    const_cast<WireBase *>(*it)->emit_vcd_neg_edge_update(writer->get_stream());
                writer->emit_neg_edge_tick(clock_num);
                writer->emit_neg_edge_clock();
            }

            // End of clock: clear iteration limit.
            // Run post-clock edge against the loaded test bench.
            iteration_count = 0;
            this->post_clock(clock_num);
        }

        // If had a VCD stop clock and it triggered, need to add final dump of 'x values.
        if (writer != NULL && writer->is_open() && had_stop_event) {
            writer->set_emitting_change(true);
            writer->emit_pos_edge_tick(clock_num);
            writer->emit_x_clock();
            writer->vcd_dumpoff(this);
        }

        // Clear clock num just in case it is read again.
        clock_num = 0;
    }

    // Methods supporting test case state.
    void begin_test() { in_tc = true; }
    template<typename ... Args> void end_test_pass(const char* fmt, Args ... args) {
        printf(fmt, args ...);
        in_tc = false;
        pass_count++;
    }
    template<typename ... Args> void end_test_fail(const char* fmt, Args ... args) {
        printf(fmt, args ...);
        in_tc = false;
        fail_count++;
    }
    inline const bool in_test() const { return in_tc; }
    inline const int n_pass() const { return pass_count; }
    inline const int n_fail() const { return fail_count; }

protected:
    // VCD writer if enabled.
    vcd::writer* writer;

private:
    // Simulation parameters.
    int32_t opt_cycle_limit;
    int32_t opt_iteration_limit;
    int32_t opt_idle_limit;

    // Test case parameters.
    bool in_tc;   
    int pass_count;
    int fail_count;
    bool exit_simulation;

    // Clock cycle counter.
    uint32_t clock_num;

    // VCD helper: generate header and definitions.
    void vcd_generate_header() {
        writer->emit_header();
        writer->vcd_definition(this, true);
        writer->emit_end_definitions();
    }

    // VCD helper: vcd_dumpvars: performs VCD dumpvars command.
    void vcd_dumpvars(const uint32_t clock_num) {
        writer->emit_pos_edge_tick(clock_num);
        writer->emit_dumpvars();
        writer->emit_pos_edge_clock();
        writer->vcd_dumpvars(this);
        writer->emit_dumpend();
        writer->set_emitting_change(true);
    }

    // VCD helper: vcd_dumpon: performs VCD dumpon command.
    void vcd_dumpon() {
        writer->emit_dumpon();
        writer->emit_pos_edge_clock();
        writer->vcd_dumpon(this);
        writer->emit_dumpend();
        writer->set_emitting_change(true);
    }

    // VCD helper: vcd_dumpoff: performs VCD dumpoff command.
    void vcd_dumpoff() {
        writer->emit_dumpoff();
        writer->emit_x_clock();
        writer->vcd_dumpoff(this);
        writer->emit_dumpend();
        writer->set_emitting_change(false);
    }

    // Common constructor code.
    void constructor_common() {
        // Set default simulation parameters.
        opt_cycle_limit = -1;
        opt_iteration_limit = -1;
        opt_idle_limit = -1;
        writer = NULL;

        // Set default test case parameters.
        in_tc = false;
        pass_count = fail_count = 0;
        exit_simulation = false;

        // Reset initial clock cycle counter.
        clock_num = 0;
    }
};

 #endif //  _PV_TESTBENCH_H_
