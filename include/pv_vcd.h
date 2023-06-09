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

#ifndef _PV_VCD_H_
#define _PV_VCD_H_

/*
 * A VCD file is generated in 4 phases:
 *  A. Header is dumped.
 *  B. Signal hirearchy is dumped.
 *  C. Initial $dumpvars at tick #0.
 *  D. Signal dumping per the upcoming timing diagram.
 *
 * VCD signal change are generated according to the following timing diagram:
 *
 *     ┌──────────────────────────────────────┐                                       ┌────
 *     │                                      │                                       │    
 * ────┘                                      └───────────────────────────────────────┘    
 *     ▲    ▲           ▲                     ▲                                            
 *     │    │           │                     │                                            
 *     │    │ ─ ─ ─ ─ ─ │                     │                                            
 *     │    │           │                     │                                            
 *    .─.  .─.         .─.                   .─.                                           
 *   ( 1 )( 2 )       ( 3 )                 ( 4 )                                          
 *    `─'  `─'         `─'                   `─'                                           
 *
 * At top, there is the master clock signal. Four events are then labled: 1. The rising edge of
 * clock is generated. This is emitted to the VCD file assuming we are dumping signals. 2. The
 * simulator is clocked, theerby forcing updates of all registers. And registers whose state
 * changes are dumped. 3. Register changes then force potential wire changes. When these quiesce,
 * any wires changing value are dumped. 4. The falling edge of clock is generated and emitted to
 * the VCD. All wire change "mousetraps" are reset.
 *
 * Since dumping can be enabled for disabled at specified clock boundaries, event 4 is augmented:
 *  - If VCD dump is enabled at clock #SS, in clock (SS-1) we do a "$dumpon" at event 4.
 *  - Similarly, if VCD dumps are disabled at clock #ST, in clock ST the falling edge of clock is
 *    emitted, we do a "$dumpoff" If VCD dump start/stop times are unspecified, one or
 *    both "$dumpon" and/or "$dumpoff" might be omitted.
 *
 * This header implements a "writer" class in the "vcd" namespace that is used by simulators to
 * create and write VCD files.
 */

// Update this macro as needed for signficant release changes to this header.
#define PV_VCD_VERSION "PseudoVerilog vcd::writer 1.0"

// Enter VCD namespace.
namespace vcd {

    // Enum class for timescale.
    enum class TS_time {
        t1 = 0,         // == 1
        t10,            // == 10
        t100            // == 100
    };

    // Enum class for time units.
    enum class TS_unit {
        s = 0,          // seconds  (10^0)
        ms,             // msec     (10^-3)
        us,             // usec     (10^-6)
        ns,             // nsec     (10^-9)
        ps,             // psec     (10^-12)
        fs              // fsec     (10^-15)
    };

    // VCD writer class.
    class writer {
    public:
        // Constructor: creates VCD stream (or sets error flag for object).
        writer(const std::string& file_name) {
            // Attempt to open file and create a stream for it.
            if (!vcd_file.open(file_name, std::ios::out)) {
                std::cerr << "File " << file_name << ": " << strerror(errno);
                file_is_open = false;
            } else {
                vcd_stream = new std::ostream(&vcd_file);
                file_is_open = true;
            }
            is_emitting_change = true;

            // Set default VCD options.
            opt_vcd_start_clock = -1;
            opt_vcd_stop_clock = -1;

            // Init timescale, clock rate, and ticks.
            timescale = 1.0;
            clock_freq = 1.0;
            ticks_per_clock = 2ull;
            time_str = "1 s";

            // Init vcd_clock_ID; default value.
            vcd_clock_ID = "*@";
        }
        writer() = delete;
        writer(const writer& w) = delete;

        // Destructor: closes file.
        virtual ~writer() {
            if (file_is_open) {
                vcd_file.close();
                delete vcd_stream;
            }
        }

        // Class setters.
        inline void set_vcd_start_clock(const int32_t v) { opt_vcd_start_clock = v; }
        inline void set_vcd_stop_clock(const int32_t v) { opt_vcd_stop_clock = v; }
        inline const int32_t get_vcd_start_clock() { return opt_vcd_start_clock; }
        inline const int32_t get_vcd_stop_clock() { return opt_vcd_stop_clock; }
        inline void set_vcd_clock_ID(const std::string id) { vcd_clock_ID = id; }

        // Class getters.
        inline std::ostream* get_stream() { return vcd_stream; }
        inline float get_timescale() const { return timescale; }
        inline float get_clock_freq() const { return clock_freq; }
        inline uint64_t get_ticks_per_clock() const { return ticks_per_clock; }
        inline const std::string& get_time_str() const { return time_str; }
        inline const std::string& get_vcd_clock_ID() const { return vcd_clock_ID; }

        // Return file open status.
        inline bool is_open() const { return file_is_open; }

        // Method to set operating attributes (frequency and timescale).
        void set_operating_point(const float freq, const TS_time time = TS_time::t1, 
            const TS_unit unit = TS_unit::ns) {
                // Convert time to string; save time scale period.
                switch (time) {
                case TS_time::t1:   time_str = "1 ";   timescale = 1;   break;
                case TS_time::t10:  time_str = "10 ";  timescale = 10;  break;
                case TS_time::t100: time_str = "100 "; timescale = 100; break;
                }

                // Convert unit to string; update time scale period.
                switch (unit) {
                case TS_unit::s:    time_str += "s";  break;
                case TS_unit::ms:   time_str += "ms"; timescale *= 1e-3;  break;
                case TS_unit::us:   time_str += "us"; timescale *= 1e-6;  break;
                case TS_unit::ns:   time_str += "ns"; timescale *= 1e-9;  break;
                case TS_unit::ps:   time_str += "ps"; timescale *= 1e-12; break;
                case TS_unit::fs:   time_str += "fs"; timescale *= 1e-15; break;
                }

                // Install clock rate and compute ticks/clock.
                clock_freq = freq;
                float f_ticks_per_clock = 1.0 / (clock_freq * timescale);
                ticks_per_clock = (f_ticks_per_clock < 2.0) ? 2ull : (uint64_t) f_ticks_per_clock;
        }

        // Method to emit a VCD header.
        void emit_header() {
            check_state();
            *vcd_stream << "$date " << get_zulu_time() << "$end\n";
            *vcd_stream << "$version " << PV_VCD_VERSION << "\n$end\n";
            *vcd_stream << "$timescale " << time_str << "\n$end\n";
        }

        /*** VCD COMMENT EMIT ***/
        // Comment block.
        inline void emit_comment(const std::string& comment) 
            { check_state(); *vcd_stream << "$comment" << std::endl 
                << comment << std::endl << "$end" << std::endl; }

        /*** VCD HEADER EMITS ***/
        // Scope/upscope.
        inline void emit_scope(const std::string& module_name)
            { check_state(); *vcd_stream << "$scope module " << module_name 
                << " $end" << std::endl; }
        inline void emit_upscope()
            { check_state(); *vcd_stream << "$upscope $end" << std::endl; }

        // Wire/register defines.
        inline void emit_definition(const std::string& type, const int width, 
            const std::string& vcd_ID, const std::string& name)
                { check_state(); *vcd_stream << "$var " << type << " " << width 
                    << " " << vcd_ID << " " << name << " $end" << std::endl; }

        // Emit vcd_clock_ID.
        inline void emit_vcd_clock_ID()
            { check_state(); *vcd_stream << "$var wire 1 " << vcd_clock_ID << " clk $end\n"; }

        // End definitons.
        inline void emit_end_definitions()
            { check_state(); *vcd_stream << "$enddefinitions $end" << std::endl; }

        /*** VCD CHANGE DUMP EMITS ***/
        // Dump commands.
        inline void emit_dumpall()  { check_state(); *vcd_stream << "$dumpall" << std::endl; }
        inline void emit_dumpoff()  { check_state(); *vcd_stream << "$dumpoff" << std::endl; }
        inline void emit_dumpon()   { check_state(); *vcd_stream << "$dumpon" << std::endl; }
        inline void emit_dumpvars() { check_state(); *vcd_stream << "$dumpvars" << std::endl; }
        inline void emit_dumpend()  { check_state(); *vcd_stream << "$end" << std::endl; }

        // Emit tick and clock changes.
        inline void emit_pos_edge_tick(const uint32_t clk_num) {
            check_state();
            if (is_emitting_change)
                *vcd_stream << "#" << clk_num * ticks_per_clock << std::endl;
        }
        inline void emit_neg_edge_tick(const uint32_t clk_num) {
            check_state();
            if (is_emitting_change)
                *vcd_stream << "#" << clk_num * ticks_per_clock + (ticks_per_clock >> 1) << std::endl;
        }
        inline void emit_pos_edge_clock()
            { check_state(); if (is_emitting_change) *vcd_stream << "1" << vcd_clock_ID << std::endl; }
        inline void emit_neg_edge_clock()
            { check_state(); if (is_emitting_change) *vcd_stream << "0" << vcd_clock_ID << std::endl; }
        inline void emit_x_clock()
            { check_state(); if (is_emitting_change) *vcd_stream <<  "x" << vcd_clock_ID << std::endl; }

        // Signal emits.
        inline void emit_change(const std::string& id, const int width, const std::string& value)
            { check_state(); if (is_emitting_change) *vcd_stream << value 
                << (width > 1 ? " " : "") << id << std::endl; }

        // Setter/getter for is_emitting_change.
        inline void set_emitting_change(const bool en) { is_emitting_change = en; }
        inline bool get_emitting_change() const { return is_emitting_change; }

        // VCD definition.
        void vcd_definition(const Module* m, const bool define_clock = false) {
            // Make sure file is open.
            check_state();

            // Enter new scope for module "m"; define clock if asked for.
            this->emit_scope(m->name());
            if (define_clock)
                this->emit_vcd_clock_ID();

            // Dump local wires.
            for (std::set<const WireBase*>::const_iterator it = m->w_begin(); it != m->w_end(); it++)
                const_cast<WireBase*>(*it)->emit_vcd_definition(vcd_stream);

            // Dump local registers.
            for (std::set<const RegisterBase*>::const_iterator it = m->r_begin(); it != m->r_end(); it++)
                const_cast<RegisterBase*>(*it)->emit_vcd_definition(vcd_stream);

            // Recursively dump any submodules.
            for (std::set<const Module*>::const_iterator it = m->m_begin(); it != m->m_end(); it++)
                this->vcd_definition(*it, false);

            // Exit current scope.
            this->emit_upscope();
        }

        // VCD dumpDars.
        void vcd_dumpvars(const Module* m) {
            // Make sure file is open.
            check_state();

            if (is_emitting_change) { 
                // Dump local wires.
                for (std::set<const WireBase*>::const_iterator it = m->w_begin(); it != m->w_end(); it++)
                    const_cast<WireBase*>(*it)->emit_vcd_dumpvars(vcd_stream);

                // Dump local registers.
                for (std::set<const RegisterBase*>::const_iterator it = m->r_begin(); it != m->r_end(); it++)
                    const_cast<RegisterBase*>(*it)->emit_vcd_dumpvars(vcd_stream);

                // Recursively dump any submodules.
                for (std::set<const Module*>::const_iterator it = m->m_begin(); it != m->m_end(); it++)
                    this->vcd_dumpvars(*it);
            }
        }

        // VCD dumpon.
        void vcd_dumpon(const Module* m) {
            // Make sure file is open.
            check_state();

            if (is_emitting_change) { 
                // Dump local wires.
                for (std::set<const WireBase*>::const_iterator it = m->w_begin(); it != m->w_end(); it++)
                    const_cast<WireBase*>(*it)->emit_vcd_dumpon(vcd_stream);

                // Dump local registers.
                for (std::set<const RegisterBase*>::const_iterator it = m->r_begin(); it != m->r_end(); it++)
                    const_cast<RegisterBase*>(*it)->emit_vcd_dumpon(vcd_stream);

                // Recursively dump any submodules.
                for (std::set<const Module*>::const_iterator it = m->m_begin(); it != m->m_end(); it++)
                    this->vcd_dumpon(*it);
            }
        }

        // VCD dumpoff.
        void vcd_dumpoff(const Module* m) {
            // Make sure file is open.
            check_state();

            if (is_emitting_change) { 
                // Dump local wires.
                for (std::set<const WireBase*>::const_iterator it = m->w_begin(); it != m->w_end(); it++)
                    const_cast<WireBase*>(*it)->emit_vcd_dumpoff(vcd_stream);

                // Dump local registers.
                for (std::set<const RegisterBase*>::const_iterator it = m->r_begin(); it != m->r_end(); it++)
                    const_cast<RegisterBase*>(*it)->emit_vcd_dumpoff(vcd_stream);

                // Recursively dump any submodules.
                for (std::set<const Module*>::const_iterator it = m->m_begin(); it != m->m_end(); it++)
                    this->vcd_dumpoff(*it);
            }
        }

    private:
        // Fields defining the open stream.
        bool file_is_open;
        bool is_emitting_change;
        std::filebuf vcd_file;
        std::ostream* vcd_stream;

        // Writer options.
        int32_t opt_vcd_start_clock;
        int32_t opt_vcd_stop_clock;

        // timescale and clock frequency; timescale is per tick.
        // ticks_per_clock = a minimum of 2 even if clock frequency and timescale imply otherwise.
        float timescale;
        float clock_freq;
        uint64_t ticks_per_clock;
        std::string time_str;

        // VCD clock ID.
        std::string vcd_clock_ID;

        // Method to check if object is ok to print.
        inline void check_state() const {
            if (!file_is_open) {
                std::string estr = "VCD writer: bad file stream.";
                throw std::ios_base::failure(estr);
            }
        }

        // Method to emit current time (Zulu); utility only.
        char* get_zulu_time() {
            time_t t; 
            time(&t);
            struct tm* tt = gmtime(&t);
            return asctime(tt);
        }
    };

} // End namespace vcd.

#endif //  _PV_VCD_H_
