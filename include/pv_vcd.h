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
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

#ifndef _PV_VCD_H_
#define _PV_VCD_H_

// enter VCD namespace
namespace vcd {

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
     * At top, there is the master clock signal. Four events are then labled:
     *  1. The rising edge of clock is generated. This is emitted to the VCD file assuming we are dumping signals.
     *  2. The simulator is clocked, theerby forcing updates of all registers. And registers whose state changes are dumped.
     *  3. Register changes then force potential wire changes. When these quiesce, any wires changing value are dumped.
     *  4. The falling edge of clock is generated and emitted to the VCD. All wire change "mousetraps" are reset.
     *
     * Since dumping can be enabled for disabled at specified clock boundaries, event 4 is augmented:
     *  - If VCD dump is enabled at clock #SS, in clock (SS-1) we do a "$dumpon" at event 4.
     *  - Similarly, if VCD dumps are disabled at clock #ST, in clock ST the falling edge of clock is emitted, we do a "$dumpoff"
     * If VCD dump start/stop times are unspecified, one or both "$dumpon" and/or "$dumpoff" might be omitted.
     */

    // enum class for timescale
    enum class TS_time {
        t1 = 0,         // == 1
        t10,            // == 10
        t100            // == 100
    };

    // enum class for time units
    enum class TS_unit {
        s = 0,          // seconds  (10^0)
        ms,             // msec     (10^-3)
        us,             // usec     (10^-6)
        ns,             // nsec     (10^-9)
        ps,             // psec     (10^-12)
        fs              // fsec     (10^-15)
    };

    // VCD writer class
    class writer {
    public:
        // constructor: creates VCD stream (or sets error flag for object)
        writer(const std::string& file_name) {
            // attempt to open file and create a stream for it
            if (!vcd_file.open(file_name, std::ios::out)) {
                std::cerr << "File " << file_name << ": " << strerror(errno);
                file_is_open = false;
            }
            vcd_stream = new std::ostream(&vcd_file);
            file_is_open = true;
            is_emitting_change = true;

            // init timescale, clock rate, and ticks
            timescale = 1.0;
            clock_freq = 1.0;
            ticks_per_clock = 2ull;

            // init vcd_clock_ID; default value
            vcd_clock_ID = "*@";
        }

        // destructor: closes file
        virtual ~writer() {
            if (file_is_open) {
                vcd_file.close();
                delete vcd_stream;
            }
        }

        // getter for stream
        inline std::ostream& stream() { return *vcd_stream; }

        // setter/getter to set operating clock rate and ticks per clock
        inline float get_timescale() const { return timescale; }
        inline float get_clock_freq() const { return clock_freq; }
        inline uint64_t get_ticks_per_clock() const { return ticks_per_clock; }
        void set_clock_freq(const float freq) {
            clock_freq = freq;
            float f_ticks_per_clock = 1.0 / (clock_freq * timescale);
            ticks_per_clock = (f_ticks_per_clock < 2.0) ? 2ull : (uint64_t) f_ticks_per_clock;
        }

        // method to emit a VCD header
        void emit_header(const std::string& version, const TS_time time = TS_time::t1, const TS_unit unit = TS_unit::ns) {
            // make sure we are in a good state
            check_state();

            // convert time to string; save time scale period
            std::string time_str;
            switch (time) {
            case TS_time::t1:   time_str = "1 ";   timescale = 1;   break;
            case TS_time::t10:  time_str = "10 ";  timescale = 10;  break;
            case TS_time::t100: time_str = "100 "; timescale = 100; break;
            }

            // convert unit to string; update time scale period
            switch (unit) {
            case TS_unit::s:    time_str += "s";  break;
            case TS_unit::ms:   time_str += "ms"; timescale *= 1e-3;  break;
            case TS_unit::us:   time_str += "us"; timescale *= 1e-6;  break;
            case TS_unit::ns:   time_str += "ns"; timescale *= 1e-9;  break;
            case TS_unit::ps:   time_str += "ps"; timescale *= 1e-12; break;
            case TS_unit::fs:   time_str += "fs"; timescale *= 1e-15; break;
            }

            // compute ticks per clock
            float f_ticks_per_clock = 1.0 / (clock_freq * timescale);
            ticks_per_clock = (f_ticks_per_clock < 2.0) ? 2ull : (uint64_t) f_ticks_per_clock;

            // return as ascii string
            *vcd_stream << "$date " << get_zulu_time() << "$end\n";
            *vcd_stream << "$version " << version << " $end\n";
            *vcd_stream << "$timescale " << time_str << " $end\n";
        }

        // setter/getter for vcd_clock_ID
        inline const std::string& get_vcd_clock_ID() const { return vcd_clock_ID; }
        inline void set_vcd_clock_ID(const std::string id) { vcd_clock_ID = id; }

        /*** VCD COMMENT EMIT ***/
        // comment block
        inline void emit_comment(const std::string& comment) 
            { check_state(); *vcd_stream << "$comment" << std::endl << comment << std::endl << "$end" << std::endl; }

        /*** VCD HEADER EMITS ***/
        // scope/upscope
        inline void emit_scope(const std::string& module_name)
            { check_state(); *vcd_stream << "$scope module " << module_name << " $end" << std::endl; }
        inline void emit_upscope()
            { check_state(); *vcd_stream << "$upscope $end" << std::endl; }

        // wire/register defines
        inline void emit_definition(const std::string& type, const int width, const std::string& vcd_ID, const std::string& name)
            { check_state(); *vcd_stream << "$var " << type << " " << width << " " << vcd_ID << " " << name << " $end" << std::endl; }

        // emit vcd_clock_ID
        inline void emit_vcd_clock_ID()
            { check_state(); *vcd_stream << "$var wire 1 " << vcd_clock_ID << " clk $end\n"; }

        // end definitons
        inline void emit_end_definitions()
            { check_state(); *vcd_stream << "$enddefinitions $end" << std::endl; }

        /*** VCD CHANGE DUMP EMITS ***/
        // dump commands
        inline void emit_dumpall()  { check_state(); *vcd_stream << "$dumpall" << std::endl; }
        inline void emit_dumpoff()  { check_state(); *vcd_stream << "$dumpoff" << std::endl; }
        inline void emit_dumpon()   { check_state(); *vcd_stream << "$dumpon" << std::endl; }
        inline void emit_dumpvars() { check_state(); *vcd_stream << "$dumpvars" << std::endl; }
        inline void emit_dumpend()  { check_state(); *vcd_stream << "$end" << std::endl; }

        // emit tick and clock changes
        inline void emit_tick(const uint32_t clk_num, const bool is_posedge)
            { check_state(); if (is_emitting_change) *vcd_stream << "#" << vcd_compute_tick(clk_num, is_posedge) << std::endl; }
        inline void emit_clock(const bool is_x, const bool val)
            { check_state(); if (is_emitting_change) *vcd_stream << (is_x ? "x" : (val ? "1" :"0")) << vcd_clock_ID << std::endl; }

        // signal emits
        inline void emit_change(const std::string& id, const int width, const std::string& value)
            { check_state(); if (is_emitting_change) *vcd_stream << value << (width > 1 ? " " : "") << id << std::endl; }

        // return file open status
        inline bool is_open() const { return file_is_open; }

        // setter/getter for is_emitting_change
        inline void set_emitting_change(const bool en) { is_emitting_change = en; }
        inline bool get_emitting_change() const { return is_emitting_change; }

    private:
        // fields defining the open stream
        bool file_is_open;
        bool is_emitting_change;
        std::filebuf vcd_file;
        std::ostream* vcd_stream;

        // timescale and clock frequency; timescale is per tick
        // ticks_per_clock = a minimum of 2 even if clock frequency and timescale imply otherwise
        float timescale;
        float clock_freq;
        uint64_t ticks_per_clock;

        // VCD clock ID
        std::string vcd_clock_ID;

        // method to check if object is ok to print
        inline void check_state() {
            if (!file_is_open) {
                std::string estr = "VCD_Writer: bad file stream.";
                throw std::logic_error(estr);
            }
        }

        // method to emit current time (Zulu); utility only
        char* get_zulu_time() {
            time_t t; 
            time(&t);
            struct tm* tt = gmtime(&t);
            return asctime(tt);
        }

        // function to compute VCD tick number
        inline uint64_t vcd_compute_tick(const uint32_t clk_num, const bool is_posedge) {
            uint64_t tick_num = clk_num * ticks_per_clock;
            if (!is_posedge) tick_num +=  ticks_per_clock >> 1;
            return tick_num;
        }
    };

} // end namespace vcd

#endif //  _PV_VCD_H_
