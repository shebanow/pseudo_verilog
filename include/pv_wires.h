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

 #ifndef _PV_WIRES_H_
 #define _PV_WIRES_H_

/*
 * Wire base class
 * This is a non-templated class, designed this way so that external infrastructure can iterate
 * through a list of wires without knowing its base type (from the templated class). 
 *
 * Public methods in this class:
 *  General naming:
 *      - typeName(): return a string with the name of the type of this wire.
 *      - name(): return non-hierarchical name of this wire.
 *      - instanceName(): return hierarchical name of this wire.
 *  Ties to Modules:
 *      - connect(): sensitize a Module to this wire.
 *      - disconnect(): desensitize a Module to this wire.
 *  Related to VCD dumps:
 *      - vcd_neg_edge_update(): emit changed values to VCD file; abstract method, implemented in subclass
 */

class WireBase {
protected:
    // Constructor/Destructor: protected so only subclass can use.
    WireBase(const Module* p, const char* str) : parent_module(p), wire_name(str) { constructor_common(); }
    WireBase(const Module* p, const std::string& nm) : parent_module(p), wire_name(nm) { constructor_common(); }
    virtual ~WireBase() {
        wireList(this, true);
        const_cast<Module*>(parent_module)->remove_wire_instance(this);
    }

public:
    // General naming methods
    inline const std::string typeName() const { return type_printer(*this); }
    const inline std::string name() const { return wire_name; }
    const std::string instanceName() const { 
        if (parent_module == NULL)
            return wire_name;
        else {
            std::string tmp = parent_module->instanceName() + "." + wire_name;
            return tmp;
        }
    }

    // "connect" method: connects a module to wire (dependency)
    // to avoid infinite recursion, this method should NOT be called by any method in the Module class
    void connect(const Module* theModule) { add_module_connection(theModule); }

    // "disconnect" method: disconnects a module to wire (dependency)
    // to avoid infinite recursion, this method should NOT be called by any method in the Module class
    void disconnect(const Module* theModule) { remove_module_connection(theModule); }

    // VCD related
    virtual void vcd_neg_edge_update() = 0;
    virtual const int get_width() const = 0;
    std::string vcd_id_str;

    // vcd definition/dump methods
    virtual void vcd_definition() = 0;
    virtual void vcd_dumpvars() const = 0;
    virtual void vcd_dumpon() const = 0;
    virtual void vcd_dumpoff() const = 0;

    // Multi-purpose method to manage set of all declared Wires.
    // (1) if chng == NULL, wireList remains unchanged.
    // (2) if chng != NULL && !do_delete, "chng" is inserted into wireList
    // (3) if chng != NULL && do_delete, "chng" is removed from wireList
    // In all cases, a reference to a globally defined wireList is returned.
    static std::set<const WireBase*>& wireList(const WireBase* chng = NULL, bool do_delete = false) {
        static std::set<const WireBase*> wireList;

        if (chng) {
            if (do_delete) wireList.erase(chng);
            else wireList.insert(wireList.end(), chng);
        }
        return wireList;
    }

protected:
    // Parent module and name
    const Module* parent_module;
    const std::string wire_name;

    // set of connected Modules
    // std::set<const Module*> connections;

    // function to iterate over connected modules and schedule them.
    void schedule_connections() {
        for (std::set<const Module*>::const_iterator it = connections.begin(); it != connections.end(); it++)
            simulator::eval(*it);
    }

private: 
    // Class/function friends
    friend class Module;            // TODO: is this needed?
    friend int simulator::run();    // TODO: is this needed?

    // add module connection
    void add_module_connection(const Module* theModule) {
        connections.insert(connections.end(), theModule);
        const_cast<Module*>(theModule)->sensitize_module_to_wire(this);
    }

    // remove module connection
    void remove_module_connection(const Module* theModule) {
        connections.erase(theModule);
        const_cast<Module*>(theModule)->desensitize_module_to_wire(this);
    }

    // common constructor code
    void constructor_common() {
        // parent cannot be NULL
        assert(parent_module != NULL);

        // add to wireList, Module instance list
        wireList(this, false);
        const_cast<Module*>(parent_module)->add_wire_instance(this);

        // initialize VCD ID
        std::stringstream ss;
        ss << "@" << std::hex << simulator::vcd_id_count++;
        vcd_id_str = ss.str();
    }
};

/*
 * Wire template base class.
 * This is a container for all wire types (input, output, and generic wires).
 * Linkages between instances of this class are maintained by the WireBase superclass.
 *
 * Of note, wires can have a width (in bits). By default, when a wire is created, its width is infered
 * from the template type (T). This of course can be overridden by a setter to set the actual width
 * a user wants. The specific wire subclasses (Wire, Input, Output) all have optional width parameters
 * that do this from the template declaration.
 *
 * Public methods in this class:
 *  Value getter/setter:
 *      - operator T(): return current value of the wire
 *      - operator=(): assign a value to the wire and return reference to the object
 *  Width setter/getter:
 *      - set_width(): set the width of the wire
 *      - get_width(): return the width of the wire
 *  VCD string printer:
 *      - set_vcd_string_printer(): override default string printer
 *  "X" state related:
 *      - value_is_x(): returns true if current value is "x"
 *      - value_was_x(): returns true if value was an "x" at the start of the clock.
 *      - set_x_value(): set the current X state to true or false
 *      - clear_x_states: clears both current and "was" states of 'x' false; use for initialization
 *      - assign_x(): transitions wire to "x"
 *  Parent module getter:
 *      - parent(): return pointer to parent Module
 *  Operator overloads:
 *      - the usual suspects...
 *  VCD related:
 *      - vcd_neg_edge_update(): checks if wire changed and emits to VCD if enabled.
 *      - vcd_definition(): emit VCD definition of wire
 *      - vcd_dumpvars(): dump initial values of wire
 *      - vcd_dumpon(): synonym to dumpvars
 *      - vcd_dumpoff(): dump value as 'x'
 */

template <typename T>
class WireTemplateBase : public WireBase {
public:
    // Constructor/Destructor
    WireTemplateBase(const Module* p, const char* str, const int W) : WireBase(p, str) { constructor_common(W); }
    WireTemplateBase(const Module* p, const std::string& nm, const int W) : WireBase(p, nm)  { constructor_common(W); }
    virtual ~WireTemplateBase() { if (is_default_printer) delete v2s; }

    // Wire value getter/setter
    inline operator T() const { return value; }
    inline WireTemplateBase& operator=(const T& v) {
        // If this is the first write to this wire in the current clock, save old value.
        // After this update, it also clearly cannot have "x" state either. 
        if (!this->written_this_clock) {
            was_x = is_x;
            old_value = value;
            this->written_this_clock = true;
        }
        is_x = false;

        // If the value changed, update it, and then schedule updates to conections
        if (value != v) {
            value = v;
            schedule_connections();
        } 
        return *this;
    }

    // VCD string printer setter
    void set_vcd_string_printer(const vcd::value2string_t<T>* p) { 
        if (is_default_printer) delete v2s;
        v2s = p;
        is_default_printer = false;
    }

    // width setter/getter
    void set_width(const int wv) { width = wv; }
    const int get_width() const { return width; }

    // X state setters/getters
    inline bool value_is_x() const { return is_x; }
    inline bool value_was_x() const { return was_x; }
    inline void set_x_value(const bool nx) { is_x = nx; }
    inline void clear_x_states() { is_x = was_x = false; }
    void assign_x() {
        if (!this->written_this_clock) {
            was_x = is_x;
            old_value = value;
            this->written_this_clock = true;
        }
        is_x = true;
    }

    // get parent
    const Module* parent() const { return this->parent_module; }

    // Operator overloads
    inline WireTemplateBase& operator+=(const T& v) { value += v; return *this; }
    inline WireTemplateBase& operator-=(const T& v) { value -= v; return *this; }
    inline WireTemplateBase& operator*=(const T& v) { value *= v; return *this; }
    inline WireTemplateBase& operator/=(const T& v) { value /= v; return *this; }
    inline WireTemplateBase& operator%=(const T& v) { value %= v; return *this; }
    inline WireTemplateBase& operator^=(const T& v) { value ^= v; return *this; }
    inline WireTemplateBase& operator&=(const T& v) { value &= v; return *this; }
    inline WireTemplateBase& operator|=(const T& v) { value |= v; return *this; }
    inline WireTemplateBase& operator>>=(const int& v) { value >>= v; return *this; }
    inline WireTemplateBase& operator<<=(const int& v) { value <<= v; return *this; }
    inline WireTemplateBase& operator++() { ++value; return *this; }
    inline WireTemplateBase& operator--() { --value; return *this; }
    inline WireTemplateBase  operator++(int) { WireTemplateBase tmp = *this; ++value; return tmp; }
    inline WireTemplateBase  operator--(int) { WireTemplateBase tmp = *this; --value; return tmp; }

    // vcd definition
    void vcd_definition() 
        { if (simulator::vcd_file) simulator::vcd_file->emit_definition("wire", width, vcd_id_str, wire_name); }

    // vcd dump methods
    void vcd_dumpvars() const
        { if (simulator::vcd_file) simulator::vcd_file->emit_change(vcd_id_str, width, is_x ? v2s->undefined() : (*v2s)(value)); }
    void vcd_dumpon() const
        { vcd_dumpvars(); }
    void vcd_dumpoff() const
        { if (simulator::vcd_file) simulator::vcd_file->emit_change(vcd_id_str, width, v2s->undefined()); }

    // VCD updates on negative edge of clock: if value has changed AND we are dumping VCD, print the change.
    void vcd_neg_edge_update() {
        if (simulator::vcd_file->get_emitting_change() && (is_x ? (is_x ^ was_x) : (was_x || value != old_value)))
            simulator::vcd_file->emit_change(vcd_id_str, width, is_x ? v2s->undefined() : (*v2s)(value));
        was_x = is_x;
        old_value = value;
    }

protected:
    // wire parameter
    int width;

    // state changes
    bool written_this_clock;

    // The current and old value of the wire. "old" means the value it had at the start of a clock.
    T value;
    T old_value;

    // Record the "x" state of the wire. This takes precedence over the bvalue of the wire.
    bool is_x;
    bool was_x;

private:
    // vcd string printer
    vcd::value2string_t<T>* v2s;
    bool is_default_printer;

    // common constructor code
    void constructor_common(const int W) {
        // set default width
        width = (W > 0) ? W : vcd::bitwidth<T>();

        // set up default VCD string printer
        v2s = new vcd::value2string_t<T>(value); 
        v2s->set_width(width);
        is_default_printer = true;

        // init value change flag and X states
        written_this_clock = false;
        was_x = is_x = true;
    }
};

/*
 * Wire: a specialization of the WireTemplateBase class. Wires are intended for use inside a Module, and will
 * auto-sensitize the parent module to changes in Wire state.
 */

template <typename T, int W = -1>
class Wire final : public WireTemplateBase<T> {
public:
    // Constructor/Destructor
    Wire(const Module* p, const char* str) : WireTemplateBase<T>(p, str, W)
        { constructor_common(p, str, NULL); }
    Wire(const Module* p, const char* str, const T& init) : WireTemplateBase<T>(p, str, W)
        { constructor_common(p, str, &init); }
    Wire(const Module* p, const std::string& nm) : WireTemplateBase<T>(p, nm, W)
        { constructor_common(p, nm.c_str(), NULL); }
    Wire(const Module* p, const std::string& nm, const T& init) : WireTemplateBase<T>(p, nm, W)
        { constructor_common(p, nm.c_str(), &init); }
    virtual ~Wire() { this->disconnect(this->parent_module); }

    // call superclass for assignment operator
    inline Wire& operator=(const T& value) { return (Wire&) WireTemplateBase<T>::operator=(value); }

private:
    void constructor_common(const Module* p, const char* str, const T* init) {
        if (p) {
            // connect wire to its parent and optionally initialize
            this->connect(p);
            if (init) {
                this->value = *init;
                this->schedule_connections();
                this->clear_x_states();
            }
        } else
            throw std::invalid_argument("Wire must be declared inside a module");
    }
};

/*
 * Input: a specialization of the WireTemplateBase class.
 * Inputs are intended for use inside a Module as an I/O port to the Module,
 * and will auto-sensitize the parent module to changes in Wire state.
 */

template <typename T, int W = -1>
class Input final : public WireTemplateBase<T> {
public:
    // Constructor/Destructor
    Input(const Module* p, const char* str) : WireTemplateBase<T>(p, str, W)
        { constructor_common(p, str, NULL); }
    Input(const Module* p, const char* str, const T& init) : WireTemplateBase<T>(p, str, W) 
        { constructor_common(p, str, &init); }
    Input(const Module* p, const std::string& nm) : WireTemplateBase<T>(p, nm, W) 
        { constructor_common(p, nm.c_str(), NULL); }
    Input(const Module* p, const std::string& nm, const T& init) : WireTemplateBase<T>(p, nm, W) 
        { constructor_common(p, nm.c_str(), &init); }
    virtual ~Input() { this->disconnect(this->parent_module); }

    // call superclass for assignment operator
    inline Input& operator=(const T& value) { return (Input&) WireTemplateBase<T>::operator=(value); }

private:
    void constructor_common(const Module* p, const char* str, const T* init) {
        if (p) {
            // connect input to its parent and optionally initialize
            this->connect(p);
            if (init) {
                this->value = *init;
                this->schedule_connections();
                this->clear_x_states();
            }
        } else
            throw std::invalid_argument("Input must be declared inside a module");
    }
};

/*
 * Output: a specialization of the WireTemplateBase class.
 * Outputs are intended for use inside a Module as an I/O port to the Module,
 * and will add themselves as output ports on the Module. Later, as the Module is
 * instanced, those outputs will auto-sensitize themselves on the parent Module.
 */

template <typename T, int W = -1>
class Output final : public WireTemplateBase<T> {
public:
    // Constructor/Destructor
    Output(const Module* p, const char* str) : WireTemplateBase<T>(p, str, W)
        { constructor_common(p, NULL); }
    Output(const Module* p, const char* str, const T& init) : WireTemplateBase<T>(p, str, W) 
        { constructor_common(p, &init); }
    Output(const Module* p, const std::string& nm) : WireTemplateBase<T>(p, nm, W) 
        { constructor_common(p, NULL); }
    Output(const Module* p, const std::string& nm, const T& init) : WireTemplateBase<T>(p, nm, W) 
        { constructor_common(p, &init); }
    virtual ~Output() {
        // If there is a grandparent, disconnect from it.
        if (this->parent_module->parent())
            this->parent_module->parent()->desensitize_module_to_wire(this);
    }
        

    // call superclass for assignment operator
    inline Output& operator=(const T& value) { return (Output&) WireTemplateBase<T>::operator=(value); }

private:
    void constructor_common(const Module* p, const T* init) {
        if (p) {
            // if parent has a parent, connect the output to that grandparent
            if (p->parent())
                p->parent()->sensitize_module_to_wire(this);

            // If has an initializer, do that.
            if (init) {
                this->value = *init;
                this->schedule_connections();
                this->clear_x_states();
            }
        } else
            throw std::invalid_argument("Output must be declared inside a module");
    }
};

 #endif //  _PV_WIRES_H_
