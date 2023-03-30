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

 #ifndef _PV_REGISTER_H_
 #define _PV_REGISTER_H_

namespace vcd { class writer; }     // forward declaration

/*
 * RegisterBase base class.
 * This class is used as a base class for the Register template class as a generic
 * "bucket" for the somewhat more complex templated Register class. This class has two
 * main functions:
 *  - Maintain a doubly linked list for instances or the Register<> class
 *  - Clock all instances and advance a clock counter.
 * Constructor/destructor is protected as this class should not be instanced directly.
 *
 * Public methods in this class:
 *  General naming:
 *      - typeName(): returns type of register as a string
 *      - name(): returns instance name as a string
 *      - instanceName(): returns hierarchical instance name as a string
 *  Module sensitivity:
 *      - sensitize(): sensitize a Module to this register
 *      - desensitize(): desensitize a Module from this register
 *  Clocking:
 *      - clock(): run a clock cycle on this register
 *  Module info:
 *      - parent(): return pointer to parent Module
 */

class RegisterBase {
protected:
    // Constructor/Destructor
    RegisterBase(const Module* p, const char* str) : parent_module(p), global(const_cast<Module*>(p)->global), reg_name(str)
        { constructor_common(); }
    RegisterBase(const Module* p, const std::string& str) : parent_module(p), global(const_cast<Module*>(p)->global), reg_name(str)
        { constructor_common(); }
    virtual ~RegisterBase() {
        Module* m = const_cast<Module*>(parent_module);
        m->global.dissociate_register_in_module(this, parent_module);
        m->global.registerList().erase(this);
        m->global.desensitize_register(this);
    }

    // parent Module instance
    const Module* parent_module;

    // bit width of this register
    int width;

public: 
    // Type, name and instance name getters
    inline const std::string name() const { return reg_name; }
    const std::string instanceName() const {
        if (parent_module == NULL) return reg_name;
        else {
            std::string tmp;
            tmp = parent_module->instanceName() + "." + reg_name;
            return tmp;
        }
    }

    // Manually manage module sensitization.
    void sensitize(const Module* theModule) { const_cast<Module*>(parent_module)->global.sensitize_to_register(theModule, this); }
    void desensitize(const Module* theModule) { const_cast<Module*>(parent_module)->global.desensitize_to_register(theModule, this); }

    // How to clock this register, to be overridden by subclass
    virtual void pos_edge(std::ostream* vcd_stream) = 0;

    // Getter for parent
    const Module* parent() const { return parent_module; }

    // Getter for width (in bits)
    virtual const int get_width() const = 0;

    // VCD related
    std::string vcd_id_str;
    virtual void emit_vcd_definition(std::ostream* vcd_stream) = 0;
    virtual void emit_vcd_dumpvars(std::ostream* vcd_stream) const = 0;
    virtual void emit_vcd_dumpon(std::ostream* vcd_stream) const = 0;
    virtual void emit_vcd_dumpoff(std::ostream* vcd_stream) const = 0;

protected:
    // saving a reference from the parent module.
    global_data_t& global;

private:
    // Friend classes
    friend class Module; 
    friend class vcd::writer;

    // Register name
    const std::string reg_name;

    // common constuctor code
    void constructor_common() {
        // parent cannot be NULL
        assert(parent_module != NULL);

        // associate to parent module, add to global list of wires
        global.associate_module_register(parent_module, this);
        global.registerList().insert(this);

        // initialize VCD ID
        std::stringstream ss;
        ss << "@" << std::hex << global.vcd_id_count()++;
        vcd_id_str = ss.str();
    }
};

/*
 * @brief Register template class.
 * This is a container for registers (flip-flops). Linkages between instances
 * of this class are maintained by the RegisterBase superclass. This class implements
 * the getter (from the T replica), the setter (to the T source), a clock edge 
 * "clock" function to clock all register instances, and a method to connect
 * Modules to a register so that on output (Q) changes, those modules are updated.
 *
 * Template arguments:
 * T - the type of wire
 * W - optional bit width; if unspecified, width will be inferred from type
 *
 * Public methods in this class:
 *  Value getter/setter:
 *      - operator T(): returns value of output flop stage 
 *      - d(): returns reference to input flop stage; be careful with assignments
 *      - q(): returns reference to output flop stage; be careful with assignments
 *      - operator<=(): non-blocking assignment to input stage
 *  Width setter/getter:
 *      - set_width(): set the width of the register (overriding template)
 *      - get_width(): return the width of the register
 *  "X" state related:
 *      - value_is_x(): returns true if replica value is "x"
 *      - value_will_be_x(): returns true if source value is "x"
 *      - set_x_value(): set the source X state to true or false
 *      - clear_x_states: clears both source and replica states of 'x' false; use for initialization
 *      - assign_x(): sets source = "x"
 *  VCD related:
 *      - set_vcd_string_printer(): override default string printer; caller responsible for any object delete
 *      - emit_vcd_definition(): emit VCD definition of reg
 *      - emit_vcd_dumpvars(): dump initial values of reg
 *      - emit_vcd_dumpon(): synonym to dumpvars
 *      - emit_vcd_dumpoff(): dump value as 'x'
 *  Operator overloads: many are disabled as we don't want output stage changes
 */

template <typename T, int W = -1>
class Register final : public RegisterBase {
public:
    // Constructor/Destructor
    Register(const Module* p, const char* str) : RegisterBase(p, str)
        { constructor_common(p, str, NULL); }
    Register(const Module* p, const char* str, const T& init) : RegisterBase(p, str)
        { constructor_common(p, str, &init); }
    Register(const Module* p, const std::string& str) : RegisterBase(p, str)
        { constructor_common(p, str.c_str(), NULL); }
    Register(const Module* p, const std::string& str, const T& init) : RegisterBase(p, str)
        { constructor_common(p, str.c_str(), &init); }
    virtual ~Register() { if (is_default_printer) delete v2s; }

    // Getter
    inline operator T() const { return replica; }
    inline T& d() { return source; }
    inline T& q() { return replica; }

    // Disallow direct assignment (blocking)
    Register& operator=(const T& v) = delete;
    inline Register& operator<=(const T& v) {
        source = v;
        return *this;
    }

    // width setter/getter
    void set_width(const int wv) { width = wv; }
    const int get_width() const { return width; }

    // X state setters/getters
    inline bool value_is_x() const { return replica_x; }
    inline bool value_will_be_x() const { return source_x; }
    inline void set_x_value(const bool nx) { source_x = nx; }
    inline void clear_x_states() { source_x = replica_x = false; }
    inline void assign_x() { source_x = true; }

    // VCD string printer setter
    void set_vcd_string_printer(const vcd::value2string_t<T>* p) { 
        if (is_default_printer) delete v2s;
        v2s = p;
        is_default_printer = false;
    }

    // Disallow op-assignments
    Register& operator+=(const T& v) = delete;
    Register& operator-=(const T& v) = delete;
    Register& operator*=(const T& v) = delete;
    Register& operator/=(const T& v) = delete;
    Register& operator%=(const T& v) = delete;
    Register& operator^=(const T& v) = delete;
    Register& operator&=(const T& v) = delete;
    Register& operator|=(const T& v) = delete;

    // Disallow auto increment and auto decrement
    Register& operator++() = delete;
    Register& operator--() = delete;
    Register& operator++(int) = delete;
    Register& operator--(int) = delete;

    // vcd definition
    void emit_vcd_definition(std::ostream* vcd_stream)
        { *vcd_stream << "$var reg " << width << " " << vcd_id_str << " " << name() << " $end" << std::endl; }

    // vcd dump methods
    void emit_vcd_dumpvars(std::ostream* vcd_stream) const
        { *vcd_stream << (replica_x ? v2s->undefined() : (*v2s)(replica)) << (width > 1 ? " " : "") << vcd_id_str << std::endl; }
    void emit_vcd_dumpon(std::ostream* vcd_stream) const
        { *vcd_stream << (replica_x ? v2s->undefined() : (*v2s)(replica)) << (width > 1 ? " " : "") << vcd_id_str << std::endl; }
    void emit_vcd_dumpoff(std::ostream* vcd_stream) const
        { *vcd_stream << v2s->undefined() << (width > 1 ? " " : "") << vcd_id_str << std::endl; }

private:
    /*
     * In order to follow more PC conventions, the term master/slave is replaced with the
     * MYSQL terminology (see https://en.wikipedia.org/wiki/Master/slave_(technology)).
     */
    T source;       // was "master"
    T replica;      // was "slave"
    bool source_x;
    bool replica_x;

    // vcd string printer
    vcd::value2string_t<T>* v2s;
    bool is_default_printer;

    // implement a clock edge on this register
    inline void pos_edge(std::ostream* vcd_stream) {
        if (replica_x ? (replica_x ^ source_x) : (source_x || replica != source)) {
            // If VCD change dump is enabled, do so.
            if (vcd_stream)
                *vcd_stream << (replica_x ? v2s->undefined() : (*v2s)(replica)) << (width > 1 ? " " : "") << vcd_id_str << std::endl;

            // schedule dependent modules
            set_m_data_t& runq = global.runq();
            umm_rm_iter_pair_t range = global.trigger_register_module().equal_range(this);
            for ( ; range.first != range.second; range.first++)
                runq.insert(runq.cend(), range.first->second);
        }
        replica = source;
        replica_x = source_x;
    }

    // common code for all constructors
    void constructor_common(const Module* p, const char* str, const T* init) {
        // error checking
        if (!p)
            throw std::invalid_argument("Register must be declared inside a module");

        // save bit width
        this->width = (W > 0) ? W : vcd::bitwidth<T>();

        // create VCD string printer
        v2s = new vcd::value2string_t<T>(replica); 
        v2s->set_width(this->width);
        is_default_printer = true;

        // connect to parent and optionally initialize
        global.sensitize_to_register(parent_module, this);
        if (init) {
            replica = source = *init;
            source_x = replica_x = false;
            global.runq().insert(global.runq().cend(), parent_module);
        } else
            source_x = replica_x = true;
    }
};

 #endif //  _PV_REGISTER_H_
