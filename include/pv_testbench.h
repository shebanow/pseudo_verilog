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

class Testbench : public Module {
public:
    // Constructors, std::string and char* variants.
    Testbench(const std::string& nm) : Module(NULL, nm) { constructor_common(); }
    Testbench(const char* str) : Module(NULL, str) { constructor_common(); }
    Testbench() = delete;
    Testbench(const Testbench& tb) = delete;

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

    // The main simulation method.
    int simulation(const bool continue_clock_sequence = false) {
        uint32_t idle_cycles = 0;
        uint32_t iteration_count = 0;
        bool had_stop_event = false;
        uint32_t start_clock_num = clock_num;
        exit_simulation = false;
        exit_code = 0;

        // If we are writing a VCD, generate header and definitions, initial state.
        // If dump start clock is positive non-zero, also execute a VCD dumpoff() command.
        if (!continue_clock_sequence && writer != NULL && writer->is_open()) {
            vcd_generate_header();
            vcd_dumpvars(0);
            if (writer->get_vcd_start_clock() > 0) {
                writer->emit_dumpoff();
                writer->emit_x_clock();
                writer->vcd_dumpoff(this);
                writer->emit_dumpend();
                writer->set_emitting_change(false);
            } else
                vcd_generate_falling_edge(0);
        }

        // Trigger all modules at least once so as to make sure simulation is "kick started."
        trigger_all_modules(this);

        // Run simulation cycles. 
        // The first test resets clock_num to 0 if we are *not* continuing a clock sequence from any prior simulation.
        // By default, this is the case. This control is useful if a test bench uses multiple simulation calls.
        if (!continue_clock_sequence)
            clock_num = 0;
        do {
            // Increment clock number to the next numbered cycle.
            clock_num++;

            // run pre-clock edge against the loaded test bench
            this->pre_clock(clock_num);

            // If VCD dumps are active, handle start/stop clock events
            if (writer != NULL && writer->is_open()) {
                // Handle VCD stop clock.
                if (writer->get_vcd_stop_clock() > 0 && writer->get_vcd_stop_clock() == clock_num) {
                    writer->emit_pos_edge_tick(clock_num);
                    vcd_dumpoff();
                    had_stop_event = true;
                }

                // Handle VCD start clock.
                if (writer->get_vcd_start_clock() > 0 && writer->get_vcd_start_clock() == clock_num) {
                    writer->set_emitting_change(true);
                    writer->emit_pos_edge_tick(clock_num);
                    vcd_dumpon();
                } else {
                    writer->emit_pos_edge_tick(clock_num);
                    writer->emit_pos_edge_clock();
                }
            }

            // Clock all flops.
            this->pos_edge(this);
            if (writer && writer->is_open() && writer->get_emitting_change())
                for (std::set<const RegisterBase*>::const_iterator it = changed_registers.begin(); it != changed_registers.end(); it++)
                    const_cast<RegisterBase*>(*it)->emit_register(writer->get_stream());
            changed_registers.clear();

            // Guard the following code with a try-catch block as it can throw exceptions.
            try {
                // Based on changes casued by register updates, propagate until idle.
                if (opt_idle_limit > 0 && triggered.empty() && ++idle_cycles == opt_idle_limit) {
                    std::stringstream err_str;
                    err_str << "Simulation failed: idle cycle limit exceeded at clock cycle " << clock_num;
                    throw std::runtime_error(err_str.str());
                }
                while (!triggered.empty()) {
                    // We were non-idle, so set idles cycles to 0.
                    idle_cycles = 0;

                    // If iteration limit in a clock exceeded, fail simulator.
                    if (opt_iteration_limit > 0 && iteration_count++ == opt_iteration_limit) {
                        std::stringstream err_str;
                        err_str << "Simulation failed: iteration limit exceeded at clock cycle " << clock_num;
                        throw std::runtime_error(err_str.str());
                    }

                    // Make a copy of run queue, then clear it.
                    std::set<const Module*> to_do_list(triggered);
                    triggered.clear();

                    // Iterate through to do list, updating each module
                    for (std::set<const Module*>::const_iterator it = to_do_list.begin(); it != to_do_list.end(); it++)
                        const_cast<Module*>(*it)->eval();
                }
            } catch (const std::exception& e) {
                std::stringstream sstr;
                sstr << "Caught system error: " << e.what() << std::endl;
                exit_string = sstr.str();
                exit_code = -1;
                exit_simulation = true;
            }
            
            // Negative edge clock calls.
            if (writer != NULL && writer->is_open() && writer->get_emitting_change()) {
                for (std::set<const WireBase*>::const_iterator it = changed_wires.begin(); it != changed_wires.end(); it++)
                    const_cast<WireBase *>(*it)->emit_vcd_neg_edge_update(writer->get_stream());
                changed_wires.clear();
                vcd_generate_falling_edge(clock_num);
            }

            // End of clock: clear iteration limit.
            iteration_count = 0;

            // Run post-clock edge against the loaded test bench.
            this->post_clock(clock_num);

            // If end of simulation requested, exit loop.
            if (exit_simulation)
                break;
        } while (opt_cycle_limit <= 0 || clock_num < opt_cycle_limit);

        // If had a VCD stop clock and it triggered, need to add final dump of 'x values.
        if (writer != NULL && writer->is_open() && had_stop_event) {
            writer->set_emitting_change(true);
            writer->emit_pos_edge_tick(clock_num);
            writer->emit_x_clock();
            writer->vcd_dumpoff(this);
        }

        // Save # of clocks simulation ran for.
        run_time_delta = clock_num - start_clock_num;
        cummulative_run_time_delta += run_time_delta;

        // All done, return exit code.
        return exit_code;
    }

    // Support code to end simulation.
    // Marks end of simulation, sets exit code to code, and formats an optional error string.
    // (Leave fmt string NULL if no string desired). 
    template<typename ... Args> void end_simulation(const int code, const char* fmt, Args ... args) {
        static char buffer[256]; 
        exit_simulation = true;
        exit_code = code;
        if (fmt) {
            snprintf(buffer, 255, fmt, args ...);
            exit_string = buffer;
        } else
            exit_string.clear();
    }

    // Return error string.
    const std::string& error_string() const { return exit_string; }

    // Run time length getters.
    const uint32_t run_time() const { return run_time_delta; }
    const uint32_t cummulative_run_time() const { return cummulative_run_time_delta; }

    /*
     * Method to reset all modules to their initial state when instanced.
     */
    inline void reset_to_instance_state() { reset_module_to_init_state(this); }

protected:
    // VCD writer if enabled.
    vcd::writer* writer;

private:
    // Simulation parameters.
    int32_t opt_cycle_limit;
    int32_t opt_iteration_limit;
    int32_t opt_idle_limit;

    // Simulation control parameters.
    bool exit_simulation;
    int exit_code;
    std::string exit_string;

    // Clock cycle counter.
    uint32_t clock_num;

    // Runtime counters.
    uint32_t run_time_delta;
    uint32_t cummulative_run_time_delta;

    // Module "run queue" (list of triggered modules)
    std::set<const Module*> triggered;

    // Tracking changed wires and registers
    std::set<const WireBase*> changed_wires;
    std::set<const RegisterBase*> changed_registers;

    // Counter tio record how many VCDs have been issued.
    uint32_t vcd_id_counter;

    /* 
     * vcd_id_count(): return current count of VCD IDs assigned.
     */
    uint32_t& vcd_id_count() { return vcd_id_counter; }

    // Method to enqueue a Module to be evaluated ("eval()"). 
    void trigger_module(const Module* theModule) { triggered.insert(theModule); }

    // Method to trigger all module instances below and including some module.
    void trigger_all_modules(const Module* m) {
        trigger_module(m);
        for (std::set<const Module*>::const_iterator it = m->m_begin(); it != m->m_end(); it++)
            trigger_all_modules(*it);
    }

    // Method to reset a module and all it instances to its instance state.
    void reset_module_to_init_state(const Module* m) {
        for (std::set<const WireBase*>::const_iterator it = m->w_begin(); it != m->w_end(); it++)
            const_cast<WireBase*>(*it)->reset_to_instance_state();
        for (std::set<const RegisterBase*>::const_iterator it = m->r_begin(); it != m->r_end(); it++)
            const_cast<RegisterBase*>(*it)->reset_to_instance_state();
        for (std::set<const Module*>::const_iterator it = m->m_begin(); it != m->m_end(); it++)
            reset_module_to_init_state(*it);
    }

    // Methods to add/remove changed wires and registers
    void add_changed_wire(const WireBase* theWire) { changed_wires.insert(theWire); }
    void remove_changed_wire(const WireBase* theWire) { changed_wires.erase(theWire); }
    void add_changed_register(const RegisterBase* theRegister) { changed_registers.insert(theRegister); }

    // Methods to recursively clock all registers.
    void pos_edge(const Module* m) {
        // Clock all local registers first.
        for (std::set<const RegisterBase*>::const_iterator it = m->r_begin(); it != m->r_end(); it++)
            const_cast<RegisterBase*>(*it)->pos_edge();

        // Descend into child modules to clock their registers.
        for (std::set<const Module*>::const_iterator it = m->m_begin(); it != m->m_end(); it++)
            this->pos_edge(*it);
    }

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

    // VCD helper: generate a falling edge at specified clock.
    void vcd_generate_falling_edge(const uint32_t clock_num) {
        writer->emit_neg_edge_tick(clock_num);
        writer->emit_neg_edge_clock();
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
        exit_simulation = false;
        exit_code = 0;
        exit_string.clear();

        // Init runtime counters.
        run_time_delta = 0u;
        cummulative_run_time_delta = 0u;

        // Reset initial clock cycle and VCD ID counters.
        clock_num = 0;
        vcd_id_counter = 0;
    }
};

 #endif //  _PV_TESTBENCH_H_
