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

/*
 * 
 * This header defines a class and a subclass:
 *
 *        ┌─────────────────────────┐
 *        │      RegisterBase       │
 *        └────────────┬────────────┘
 *                     │             
 *        ┌────────────▼────────────┐
 *        │ RegisterTemplateBase<T> │
 *        └─────────────────────────┘
 * 
 * The superclass RegisterBase is at the top of the hierarchy. It provides the basis register types:
 * its instance name (local and global), means to sensitize/desensitize this register to a Module, and
 * support for VCD dumps on a register. It's not a template class so as to allow all register instances to be
 * grouped in a single global database as well as allow iteration across all register instances.
 *
 * Below that is a templated base class, RegisterTemplateBase whose type is T. All generic operator overloads
 * are implemented in this subclass. In addition, means to get register state/set register state are included along
 * with trigger propogation to all connected modules. Lastly, VCD change detection support is also included.
 * Register state itself is represented using a pair of "source" (aka "master") and "replica" (aka "slave")
 * states (and mirrored with Boolean "x_" values to represent X states). On a positive edge of clock, the
 * source states are copied into the replica states, and if a state change occur, modules sensitized to a
 * register are triggered for eval() scheduling. Operator overloads are also implemented in this subclass.
 * Of note: classical assignments ('=') are disabled and replaced with the '<=' operator to echo a non-blocking
 * assignment in Verilog.
 */

// Forward declarations.
namespace vcd { class writer; } 
class Testbench;

/*
 * RegisterBase base class.
 * This class is used as a base class for the Register template class as a generic
 * "bucket" for the somewhat more complex templated Register class. 
 *
 * Public methods in this class:
 *  General naming:
 *      - name(): returns instance name as a string
 *      - instanceName(): returns hierarchical instance name as a string
 *  Module info:
 *      - parent(): return pointer to parent Module.
 *      - top(): returns pointer to topmost module instance (a Testbench).
 *  Related to VCD dumps:
 *      - {virtual, abstract} emit_vcd_definition() - print the definition of a register to a VCD stream.
 *      - {virtual, abstract} emit_vcd_dumpvars() - print the initial state of a register to a VCD stream.
 *      - {virtual, abstract} emit_vcd_dumpon() - functionally equivalent to emit_vcd_dumpvars().
 *      - {virtual, abstract} emit_vcd_dumpoff() - print 'x' states for the register to a VCD stream.
 *
 * Protected methods in this class:
 *      - {virtual, abstract} pos_edge() - execute a positive edge clock on this register; pass stream pointer if dumping to a VCD.
 */

class RegisterBase {
protected:
    // Constructors/Destructor: protected so only subclass can use.
    RegisterBase(const Module* p, const char* str) : parent_module(p), root_instance(p ? p->root_instance : NULL), register_name(str)
        { constructor_common(); }
    RegisterBase(const Module* p, const std::string& str) : parent_module(p), root_instance(p ? p->root_instance : NULL), register_name(str)
        { constructor_common(); }
    virtual ~RegisterBase() { const_cast<Module*>(parent_module)->remove_register_instance(this); }

public: 
    // Disallow general public use (constructor and copy constructor).
    RegisterBase() = delete;
    RegisterBase(const RegisterBase& r) = delete;

    // General naming methods.
    inline const std::string name() const { return register_name; }
    const std::string instanceName() const {
        std::string tmp = const_cast<Module*>(parent_module)->instanceName() + "." + register_name;
        return tmp;
    }

    // Getter for parent module & top level (root/testbench) pointer. Both are non-NULL.
    const Module* parent() const { return parent_module; }
    const Module* top() const { return root_instance; }

    // Virtual X state setters. Actual implementation in Register<T>.
    virtual void assign_x() {}
    virtual void reset_to_x() {}

protected:
    // Parent modules and name.
    const Module* parent_module;
    const Module* root_instance;
    const std::string register_name;

    // How to clock this register, to be overridden by subclass
    virtual void pos_edge() = 0;

    // VCD related.
    std::string vcd_id_str;
    virtual void emit_vcd_definition(std::ostream* vcd_stream) = 0;
    virtual void emit_vcd_dumpvars(std::ostream* vcd_stream) const = 0;
    virtual void emit_vcd_dumpon(std::ostream* vcd_stream) const = 0;
    virtual void emit_vcd_dumpoff(std::ostream* vcd_stream) const = 0;
    virtual void emit_register(std::ostream* vcd_stream) const = 0;

    // Optional tracing.
    bool tracing;

private:
    // Friend classes.
    friend class Testbench; 
    friend class vcd::writer;

    // Virtual method to reset register to the state it had when instanced. Actual implementation in Register<T>.
    virtual void reset_to_instance_state() {}

    // Restore replica: copy replica value back to source. Actual implementation in Register<T>.
    virtual void restore_replica() {}

    // Common constuctor code.
    void constructor_common() {
        // Parent cannot be NULL.
        if (parent_module == NULL)
            throw std::invalid_argument("Register must be declared withing a Module");

        // Associate to parent module.
        const_cast<Module*>(parent_module)->add_register_instance(this);

        // Initialize VCD ID.
        std::stringstream ss;
        ss << "@" << std::hex << const_cast<Module*>(root_instance)->vcd_id_count()++;
        vcd_id_str = ss.str();

        // Initialize trace stream off.
        tracing = false;
    }
};

/*
 * Register template class.
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
 *  "X" state getters:
 *      - value_is_x(): returns true if replica value is "x"
 *      - value_will_be_x(): returns true if source value is "x"
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
    // Constructors/Destructor.
    // Disallow default constructor and copy constructor.
    Register(const Module* p, const char* str) : RegisterBase(p, str), v2s(def_printer)
        { constructor_common(p, str, NULL); }
    Register(const Module* p, const char* str, const T& init) : RegisterBase(p, str), v2s(def_printer)
        { constructor_common(p, str, &init); }
    Register(const Module* p, const std::string& str) : RegisterBase(p, str), v2s(def_printer)
        { constructor_common(p, str.c_str(), NULL); }
    Register(const Module* p, const std::string& str, const T& init) : RegisterBase(p, str), v2s(def_printer)
        { constructor_common(p, str.c_str(), &init); }
    Register() = delete;
    Register(const Register& r) = delete;
    virtual ~Register() {}

    // Getters.
    inline operator T() const { return replica; }
    inline T& d() { return source; }
    inline T& q() { return replica; }

    // Disallow direct assignment (blocking).
    Register& operator=(const T& v) = delete;

    // Same type register->register non-blocking assignment (<=).
    Register& operator<=(const Register& v) {
        source_x = v.replica_x;
        source = v.replica;
        return *this;
    }

    // General non-register->register non-blocking assignment (<=).
    template <typename U>
    Register& operator<=(const Register<U>& v) {
        source_x = v.source_x;
        source = v.source;
        return *this;
    }

    // General non-register->register non-blocking assignment (<=).
    template <typename U>
    Register& operator<=(const U& v) {
        source_x = false;
        source = v;
        return *this;
    }

    // Width setter/getter.
    void set_width(const int wv) { width = wv; v2s.set_width(width); }
    const int get_width() const { return width; }

    // X state getters.
    inline bool value_is_x() const { return replica_x; }
    inline bool value_will_be_x() const { return source_x; }

    // X state setters.
    inline void assign_x() { source_x = true; }
    void reset_to_x() {
        if (!replica_x) {
            const_cast<Module*>(root_instance)->trigger_module(parent_module);
            const_cast<Module*>(root_instance)->add_changed_register(this);
        }
        replica_x = source_x = true;
    }

    // VCD string printer setter.
    void set_vcd_string_printer(const vcd::value2string_t<T>& printer) { v2s = printer; }

    // Disallow op-assignments.
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

    // Set up a trace or tear it down.
    inline void enable_trace(const bool en) {
        tracing = en;
        if (en) const_cast<Module*>(root_instance)->trace_string_size(instanceName(), width);
    }
    inline void trace() { enable_trace(true); }
    inline void untrace() { enable_trace(false); }

private:
    // Friend classes.
    friend class Testbench; 
    friend class vcd::writer;

    // Bit width of this register.
    int width;

    // In order to follow more PC conventions, the term master/slave is replaced with the
    // MYSQL terminology (see https://en.wikipedia.org/wiki/Master/slave_(technology)).
    // Both value and 'X' states are captured.
    T source;       // was "master"
    T replica;      // was "slave"
    T init_state;

    // Similar, but represents 'X' states of this register.
    bool source_x;
    bool replica_x;
    bool init_x;

    // Reset register to state it had when instanced.
    // Note that this method does NOT trigger eval().
    void reset() {
        source = replica = init_state;
        source_x = replica_x = init_x;
    }

    // Restore replica: copy replica value back to source.
    inline void restore_replica() {
        source = replica;
        source_x = replica_x;
    }

    // Implement a positive clock edge on this register.
    inline void pos_edge() {
        bool change = false;

        // Check to see if a change in value
        if (replica_x ? !source_x : (source_x || replica != source)) {
            const_cast<Module*>(root_instance)->trigger_module(parent_module);
            const_cast<Module*>(root_instance)->add_changed_register(this);
            change = true;
        }

        // If tracing...
        if (tracing && change) {
            pv::ValueChangeRecord vcr = const_cast<Module*>(root_instance)->get_trace_change(instanceName());
            if (vcr.type == 'U') {
                vcr.type = 'R';
                vcr.start_value = replica_x ? v2s.undefined() : (v2s)(replica);
            }
            vcr.end_value = source_x ? v2s.undefined() : (v2s)(source);
            vcr.is_changed = true;
            vcr.NTR++;
            const_cast<Module*>(root_instance)->set_trace_change(instanceName(), vcr);
        }

        // Now do the assignment
        replica = source;
        replica_x = source_x;
    }

    // VCD string printer.
    vcd::value2string_t<T> def_printer = { replica }; 
    vcd::value2string_t<T>& v2s;

    // VCD printer methods.
    // Should NOT be called if vcd_stream is NULL (i.e., we are not dumping a VCD).
    inline void emit_vcd_definition(std::ostream* vcd_stream)
        { *vcd_stream << "$var reg " << width << " " << vcd_id_str << " " << name() << vcd::width2index(width) << " $end" << std::endl; }
    inline void emit_vcd_dumpvars(std::ostream* vcd_stream) const
        { *vcd_stream << (replica_x ? v2s.undefined() : (v2s)(replica)) << (width > 1 ? " " : "") << vcd_id_str << std::endl; }
    inline void emit_vcd_dumpon(std::ostream* vcd_stream) const
        { *vcd_stream << (replica_x ? v2s.undefined() : (v2s)(replica)) << (width > 1 ? " " : "") << vcd_id_str << std::endl; }
    inline void emit_vcd_dumpoff(std::ostream* vcd_stream) const
        { *vcd_stream << v2s.undefined() << (width > 1 ? " " : "") << vcd_id_str << std::endl; }
    inline void emit_register(std::ostream* vcd_stream) const
        { *vcd_stream << (replica_x ? v2s.undefined() : (v2s)(replica)) << (width > 1 ? " " : "") << vcd_id_str << std::endl; }

    // Common code for all constructors.
    void constructor_common(const Module* p, const char* str, const T* init) {
        // Error checking.
        if (!p)
            throw std::invalid_argument("Register must be declared inside a module");

        // Save bit width.
        width = (W > 0) ? W : vcd::bitwidth<T>();

        // Set default printer bit width.
        v2s.set_width(width);

        // Connect to parent and optionally initialize.
        if (init) {
            init_state = replica = source = *init;
            init_x = source_x = replica_x = false;
        } else
            init_x = source_x = replica_x = true;
    }
};

 #endif //  _PV_REGISTER_H_
