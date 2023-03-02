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
#include "pv_global.h"
#include "pv_module.h"

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
 *      - connect(): connect a Module to this register
 *      - disconnect(): disconnect a Module from this register
 *  Clocking:
 *      - clock(): run a clock cycle on this register
 *  Module info:
 *      - parent(): return pointer to parent Module
 */

class RegisterBase {
protected:
    // Constructor/Destructor
    RegisterBase(const Module* p, const char* str) : parent_module(p), reg_name(str) { constructor_common(); }
    RegisterBase(const Module* p, const std::string& str) : parent_module(p), reg_name(str) { constructor_common(); }
    virtual ~RegisterBase() {
        registerList.erase(this);
        parent_module->remove_register_instance(this);
        parent_module->remove_register_sensitization(this);
    }

    // to be overridden by subclass
    virtual void pos_edge() = 0;

    // On clock edge, copy sources to replicas for all dependent registers
    // TODO: this should really enqueue into a RUNQ to be evaluated by testbench.
    // void schedule_dependents() {
        // for (std::set<const Module*>::const_iterator it = connections.begin(); it != connections.end(); it++)
            // simulator::eval(*it);
    // }

    // bit width of this register
    int width;

    // Set of all defined registers
    static_set<const RegisterBase*> registerList;

    // set of connected Modules
    // std::set<const Module*> connections;

public: 
    // Type, name and instance name getters
    inline const std::string typeName() const { return type_printer(*this); }
    inline const std::string name() const { return reg_name; }
    const std::string instanceName() const {
        if (parent_module == NULL) return reg_name;
        else {
            std::string tmp;
            tmp = parent_module->instanceName() + "." + reg_name;
            return tmp;
        }
    }

    // "connect" method: connects a module to the register (dependency)
    // to avoid infinite recursion, this method should NOT be called by any method in the Module class
    inline void connect(const Module* theModule) { theModule->sensitize_module_to_register(this); }

    // "disconnect" method: disconnects a module to the register (dependency)
    // to avoid infinite recursion, this method should NOT be called by any method in the Module class
    void disconnect(const Module* theModule) { theModule->sensitize_module_to_register(this); }

    // Clock the register
    static void clock() {
        // UMM_pair_t<const Module*, const RegisterBase*> model_register_range = registerInstanceDB.find_all_lr(this);
        // for (UMM_iter_t<const Module*, const RegisterBase*> it = model_register_range.first; it != model_register_range.second; it++)
        for (std::set<RegisterBase*>::iterator it = registerList.begin(); it != registerList.end(); it++)
            (*it)->pos_edge();
    }

    // Getter for parent
    const Module* parent() const { return parent_module; }

    // VCD related
    virtual const int get_width() const = 0;
    std::string vcd_id_str;

/* 

TODO: delete

    // vcd definition/dump methods
    virtual void vcd_definition() = 0;
    virtual void vcd_dumpvars() const = 0;
    virtual void vcd_dumpon() const = 0;
    virtual void vcd_dumpoff() const = 0;

*/

/* 

TODO: delete

    // Multi-purpose method to manage set of all declared Registers.
    // (1) if chng == NULL, registerList remains unchanged.
    // (2) if chng != NULL && !do_delete, "chng" is inserted into registerList
    // (3) if chng != NULL && do_delete, "chng" is removed from registerList
    // In all cases, a reference to a globally defined registerList is returned.
    static std::set<const RegisterBase*>& registerList(const RegisterBase* chng = NULL, bool do_delete = false) {
        static std::set<const RegisterBase*> registerList;

        if (chng) {
            if (do_delete) registerList.erase(chng);
            else registerList.insert(registerList.end(), chng);
        }
        return registerList;
    }

*/

private:
    // Friend classes
    friend class Module;        // TODO: is this needed?
    friend class vcd::writer;

    // parent Module instance
    const Module* parent_module;

    // Register name
    const std::string reg_name;

    // common constuctor code
    void constructor_common() {
        // ensure this register has a parent, add connection to that parent, add register to global list of registers
        assert(parent_module != NULL);
        const_cast<Module*>(parent_module)->add_register_instance(this);
        registerList(this, false);

        // initialize VCD ID
        std::stringstream ss;
        ss << "@" << std::hex << simulator::vcd_id_count++;
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
 *      - vcd_definition(): emit VCD definition of reg
 *      - vcd_dumpvars(): dump initial values of reg
 *      - vcd_dumpon(): synonym to dumpvars
 *      - vcd_dumpoff(): dump value as 'x'
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
        const char* font_str = (source != v) ? ansi_color_code::bold_red_font() : ansi_color_code::default_font();
        std::stringstream sss; sss << source;
        std::stringstream ssr; ssr << replica;
        std::stringstream ssn; ssn << v;
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

/* 

TODO: ALL this needs to move to VCD

    // vcd definition
    void vcd_definition() 
        { if (simulator::vcd_file) simulator::vcd_file->emit_definition("reg", width, vcd_id_str, name()); }

    // vcd dump methods
    void vcd_dumpvars() const
        { if (simulator::vcd_file) simulator::vcd_file->emit_change(vcd_id_str, width, replica_x ? v2s->undefined() : (*v2s)(replica)); }
    void vcd_dumpon() const
        { vcd_dumpvars(); }
    void vcd_dumpoff() const
        { if (simulator::vcd_file) simulator::vcd_file->emit_change(vcd_id_str, width, v2s->undefined()); }

*/

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
    inline void pos_edge() {
        if (replica_x ? (replica_x ^ source_x) : (source_x || replica != source)) {
            // TODO: this has to change if VCD is after register/module
            if (simulator::vcd_file->get_emitting_change())
                simulator::vcd_file->emit_change(vcd_id_str, width, replica_x ? v2s->undefined() : (*v2s)(replica));
            parent_module->schedule_register_module_update(this);
        }
        replica = source;
        replica_x = source_x;
    }

    // common code for all constructors
    void constructor_common(const Module* p, const char* str, const T* init) {
        // error checking
        if (!p)
            throw std::invalid_argument("Register must be declared inside a module");

        // default is init to 'x'
        source_x = replica_x = true;

        // save bit width
        this->width = (W > 0) ? W : vcd::bitwidth<T>();

        // create VCD string printer
        v2s = new vcd::value2string_t<T>(replica); 
        v2s->set_width(this->width);
        is_default_printer = true;

        // connect to parent and optionally initialize
        this->connect(p);
        if (init) {
            replica = source = *init;
            source_x = replica_x = false;
            parent_module->schedule_register_module_update(this);
        }
    }
};

 #endif //  _PV_REGISTER_H_
