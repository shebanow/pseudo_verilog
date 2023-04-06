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

#include <typeinfo>
#include <boost/core/demangle.hpp>

#ifndef _PV_WIRES_H_
#define _PV_WIRES_H_

/*
 * This header actually defines 5 classes/subclasses:
 *
 *                      ┌──────────────────────┐
 *                      │       WireBase       │
 *                      └───────────┬──────────┘
 *                                  │
 *                      ┌───────────▼──────────┐
 *                      │ WireTemplateBase<T>  │
 *                      └───────────┬──────────┘
 *               ┌──────────────────┼─────────────────┐
 *        ┌──────▼──────┐    ┌──────▼──────┐   ┌──────▼──────┐
 *        │ Input<T,W>  │    │ Wire<T, W>  │   │ Output<T,W> │
 *        └─────────────┘    └─────────────┘   └─────────────┘
 * 
 * The superclass WireBase is at the top of the hierarchy. It provides the basis for all wire types:
 * its instance name (local and global), means to sensitize/desensitize this wire to a Module, and
 * support for VCD dumps on a wire. It's not a template class so as to allow all wire instances to be
 * grouped in a single global database as well as allow iteration across all wire instances.
 *
 * Below that is a templated base class, WireTemplateBase whose type is T. All generic operator overloads
 * are implemented in this subclass. In addition, means to get wire state/set wire state are included along
 * with trigger propogation to all connected modules. Lastly, VCD change detection support is also included.
 *
 * Lastly, below the WireTemplateBase<T> subclass are three variant subsubclasses, Inputs, Wires, and Outputs.
 * Inputs are intended to be instances as input ports on a module, and the subsubclass auto-sensitizes its
 * parent container to input changes. The Wire subsubclass are intended to be instances with its parent container
 * module as well,and also auto-sensitizes its parent to value changes. Functionally, Inputs and Wires are identical.
 * Finally, Outputs are intended to be instances as output ports of a modile, and the subsubclass auto-sensitizes its
 * grandparent (if it exists) to value changes. All three subsubclasses are "final", meaning they cannot be further
 * subclassed. All three classes support setting a custom wire width (in bits) as well as allowing for initialization
 * when instanced (non-initialized begin in an 'x' state).
 *
 * Note, under consideration is the addition of an QWire subsubclass, similar to Wire except that its does
 * not auto-sensitize its parent. The main purpose of having a QWire type would be to allow VCD dumps of intermediate
 * nodes without autotriggering container modules.
 */

// VCD class forward declaration.
 namespace vcd { class writer; }

/*
 * Wire base class.
 * This is a non-templated class, designed this way so that external infrastructure can iterate
 * through a list of wires without knowing its base type (from the templated class). 
 *
 * Public methods in this class:
 *  General naming:
 *      - name(): return non-hierarchical name of this wire.
 *      - instanceName(): return hierarchical name of this wire.
 *  Getters:
 *      - {virtual, abstract} get_width() - returns width of wire in bits.
 *  Related to VCD dumps:
 *      - {virtual, abstract} emit_vcd_definition() - print the definition of a wire to a VCD stream.
 *      - {virtual, abstract} emit_vcd_dumpvars() - print the initial state of a wire to a VCD stream.
 *      - {virtual, abstract} emit_vcd_dumpon() - functionally equivalent to emit_vcd_dumpvars().
 *      - {virtual, abstract} emit_vcd_dumpoff() - pringt 'x' states for the wire to a VCD stream.
 *      - {virtual, abstract} emit_vcd_neg_edge_update(): emit changed values to VCD stream.
 */

class WireBase {
protected:
    // Constructor/Destructor: protected so only subclass can use.
    WireBase(const Module* p, const char* str) : parent_module(p), root_instance(p ? p->root_instance : NULL), wire_name(str)
        { constructor_common(); }
    WireBase(const Module* p, const std::string& nm) : parent_module(p), root_instance(p ? p->root_instance : NULL), wire_name(nm)
        { constructor_common(); }
    virtual ~WireBase() { const_cast<Module*>(parent_module)->remove_wire_instance(self); }

public:
    // General naming methods.
    const inline std::string name() const { return wire_name; }
    const std::string instanceName() const { 
        std::string tmp = parent_module->instanceName() + "." + wire_name;
        return tmp;
    }

    // Required getter for width of wire.
    virtual const int get_width() const = 0;

    // VCD related. The virtual methods below can't be implemented in the base class as the data type
    // is not known in the base class. However, we do want methods using the Wire-type classes the ability
    // to execute these methods in a type-independent manner, hence the virtual functions below.
    std::string vcd_id_str;
    virtual void emit_vcd_definition(std::ostream* vcd_stream) const = 0;
    virtual void emit_vcd_dumpvars(std::ostream* vcd_stream) const = 0;
    virtual void emit_vcd_dumpon(std::ostream* vcd_stream) const = 0;
    virtual void emit_vcd_dumpoff(std::ostream* vcd_stream) const = 0;
    virtual void emit_vcd_neg_edge_update(std::ostream* vcd_stream) = 0;

protected:
    // Parent modules and name.
    const Module* parent_module;
    const Module* root_instance;
    const std::string wire_name;

    // Save sensitized module (if any)
    Module *sensitized_module;

    // Reference to self for instance list.
    std::set<const WireBase*>::iterator self;

private: 
    // Common constructor code.
    void constructor_common() {
        // Parent cannot be NULL. 
        if (parent_module == NULL)
            throw std::invalid_argument("Wire must be declared withing a Module");

        // Associate to parent module. Default is no sensitization.
        self = const_cast<Module*>(parent_module->top())->add_wire_instance(this);
        sensitized_module = NULL;

        // Initialize VCD ID.
        std::stringstream ss;
        ss << "@" << std::hex << const_cast<Module*>(parent_module->top())->vcd_id_count()++;
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
 *  Parent module getter:
 *      - parent(): return pointer to parent Module
 *      - top(): returns pointer to topmost module instance (a Testbench).
 *  Width setter/getter:
 *      - set_width(): set the width of the wire
 *      - get_width(): return the width of the wire
 *  Value getter/setter:
 *      - operator T(): return current value of the wire
 *      - operator=(): assign a value to the wire and return reference to the object
 *  X state getters/setters:
 *      - value_is_x(): return true if current value is an "X".
 * 		- value_was_x(): return true if current value was an "X".
 * 		- clear_x_states(): clear both current and prior X states.
 * 		- assign_x(): make current state "X".
 *  VCD string printer:
 *      - set_vcd_string_printer(): override default string printer
 *  "X" state related:
 *      - value_is_x(): returns true if current value is "x"
 *      - value_was_x(): returns true if value was an "x" at the start of the clock.
 *      - set_x_value(): set the current X state to true or false
 *      - clear_x_states: clears both current and "was" states of 'x' false; use for initialization
 *      - assign_x(): transitions wire to "x"
 *  Operator overloads:
 *      - The usual suspects...
 *  VCD related:
 *      - emit_vcd_definition() - print the definition of a wire to a VCD stream.
 *      - emit_vcd_dumpvars() - print the initial state of a wire to a VCD stream.
 *      - emit_vcd_dumpon() - functionally equivalent to emit_vcd_dumpvars().
 *      - emit_vcd_dumpoff() - pringt 'x' states for the wire to a VCD stream.
 *      - emit_vcd_neg_edge_update(): emit changed values to VCD stream.
 */

template <typename T>
class WireTemplateBase : public WireBase {
public:
    // Constructor/Destructor.
    WireTemplateBase(const Module* p, const char* str, const int W) : WireBase(p, str) { constructor_common(W); }
    WireTemplateBase(const Module* p, const std::string& nm, const int W) : WireBase(p, nm)  { constructor_common(W); }
    virtual ~WireTemplateBase() { if (is_default_printer) delete v2s; }

    // Get parent module & top level (root/testbench) pointer. Both are non-NULL.
    const Module* parent() const { return this->parent_module; }
    const Module* top() const { return this->root_instance; }

    // Width setter/getter.
    void set_width(const int wv) { width = wv; }
    const int get_width() const { return width; }

    // Wire value getter/setter.
    inline operator T() const { return value; }
    WireTemplateBase& operator=(const T& v) {
        // printf("Calling wire assign %s %s = %s\n",
          //   boost::core::demangle(typeid(*this).name()).c_str(), this->instanceName().c_str(), (is_x ? v2s->undefined() : (*v2s)(v)).c_str());
        if ((is_x || value != v) && sensitized_module)
            const_cast<Module*>(root_instance)->trigger_module(sensitized_module);
        is_x = false; value = v;
        return *this;
    }

    // X state setters/getters.
    inline bool value_is_x() const { return is_x; }
    inline bool value_was_x() const { return was_x; }
    inline void clear_x_states() { is_x = was_x = false; }
    void assign_x() {
        if (!is_x && sensitized_module)
            const_cast<Module*>(root_instance)->trigger_module(sensitized_module);
        is_x = true;
    }

    // VCD string printer setter.
    void set_vcd_string_printer(const vcd::value2string_t<T>* printer) { 
        if (is_default_printer) delete v2s;
        is_default_printer = false;
        v2s = printer;
    }

    // Operator overloads.
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

    // VCD dump methods.
    // Should NOT be called if vcd_stream is NULL (i.e., we are not dumping a VCD).
    void emit_vcd_definition(std::ostream* vcd_stream) const
        { *vcd_stream << "$var wire " << width << " " << vcd_id_str << " " << wire_name << " $end" << std::endl; }
    void emit_vcd_dumpvars(std::ostream* vcd_stream) const
        { *vcd_stream << (is_x ? v2s->undefined() : (*v2s)(value)) << (width > 1 ? " " : "") << vcd_id_str << std::endl; }
    void emit_vcd_dumpon(std::ostream* vcd_stream) const
        { *vcd_stream << (is_x ? v2s->undefined() : (*v2s)(value)) << (width > 1 ? " " : "") << vcd_id_str << std::endl; }
    void emit_vcd_dumpoff(std::ostream* vcd_stream) const
        { *vcd_stream << v2s->undefined() << (width > 1 ? " " : "") << vcd_id_str << std::endl; }

    // VCD updates on negative edge of clock: if value has changed print the change.
    // Should NOT be called if vcd_stream is NULL (i.e., we are not dumping a VCD).
    void emit_vcd_neg_edge_update(std::ostream* vcd_stream) {
        if (is_x ? (is_x ^ was_x) : (was_x || value != old_value))
            *vcd_stream << (is_x ? v2s->undefined() : (*v2s)(value)) << (width > 1 ? " " : "") << vcd_id_str << std::endl;
        was_x = is_x;
        old_value = value;
    }

protected:
    // Width parameter.
    int width;

    // Record the "x" state of the wire. This takes precedence over the value of the wire.
    bool is_x;
    bool was_x;

    // The current and old value of the wire. "old" means the value it had at the start of a clock.
    T value;
    T old_value;

private:
    // VCD string printer.
    vcd::value2string_t<T>* v2s;
    bool is_default_printer;

    // Common constructor code.
    void constructor_common(const int W) {
        // set default width
        width = (W > 0) ? W : vcd::bitwidth<T>();

        // set up default VCD string printer
        v2s = new vcd::value2string_t<T>(value); 
        v2s->set_width(width);
        is_default_printer = true;

        // init X states
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
    // Constructors/Destructor.
    Wire(const Module* p, const char* str) : WireTemplateBase<T>(p, str, W)
        { constructor_common(p, str, NULL); }
    Wire(const Module* p, const char* str, const T& init) : WireTemplateBase<T>(p, str, W)
        { constructor_common(p, str, &init); }
    Wire(const Module* p, const std::string& nm) : WireTemplateBase<T>(p, nm, W)
        { constructor_common(p, nm.c_str(), NULL); }
    Wire(const Module* p, const std::string& nm, const T& init) : WireTemplateBase<T>(p, nm, W)
        { constructor_common(p, nm.c_str(), &init); }
    virtual ~Wire() {}

    // Call superclass for assignment operator.
    inline Wire& operator=(const T& value) { return (Wire&) WireTemplateBase<T>::operator=(value); }

private:
    // Common constructor for all four variants.
    void constructor_common(const Module* p, const char* str, const T* init) {
        if (p) {
            this->sensitized_module = const_cast<Module*>(p);
// printf("Create %s %s: sensitizes %s\n",
    // boost::core::demangle(typeid(*this).name()).c_str(), this->instanceName().c_str(), p->instanceName().c_str());
            if (init) {
                this->value = *init;
                this->clear_x_states();
                const_cast<Module*>(this->root_instance)->trigger_module(this->sensitized_module);
            }
        } else
            throw std::invalid_argument("Wire must be declared inside a module");
    }
};

/*
 * QWire: a specialization of the WireTemplateBase class. Same as a regular wire except the parent
 * module is NOT sensitized to a QWire instance. ('Q' is for quiet.)
 */

template <typename T, int W = -1>
class QWire final : public WireTemplateBase<T> {
public:
    // Constructors/Destructor.
    QWire(const Module* p, const char* str) : WireTemplateBase<T>(p, str, W)
        { constructor_common(p, str, NULL); }
    QWire(const Module* p, const char* str, const T& init) : WireTemplateBase<T>(p, str, W)
        { constructor_common(p, str, &init); }
    QWire(const Module* p, const std::string& nm) : WireTemplateBase<T>(p, nm, W)
        { constructor_common(p, nm.c_str(), NULL); }
    QWire(const Module* p, const std::string& nm, const T& init) : WireTemplateBase<T>(p, nm, W)
        { constructor_common(p, nm.c_str(), &init); }
    virtual ~QWire() {}

    // Call superclass for assignment operator.
    inline QWire& operator=(const T& value) { return (QWire&) WireTemplateBase<T>::operator=(value); }

private:
    // Common constructor for all four variants.
    void constructor_common(const Module* p, const char* str, const T* init) {
        if (p) {
            if (init) {
                this->value = *init;
                this->clear_x_states();
            }
        } else
            throw std::invalid_argument("QWire must be declared inside a module");
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
    // Constructors/Destructor.
    Input(const Module* p, const char* str) : WireTemplateBase<T>(p, str, W)
        { constructor_common(p, str, NULL); }
    Input(const Module* p, const char* str, const T& init) : WireTemplateBase<T>(p, str, W) 
        { constructor_common(p, str, &init); }
    Input(const Module* p, const std::string& nm) : WireTemplateBase<T>(p, nm, W) 
        { constructor_common(p, nm.c_str(), NULL); }
    Input(const Module* p, const std::string& nm, const T& init) : WireTemplateBase<T>(p, nm, W) 
        { constructor_common(p, nm.c_str(), &init); }
    virtual ~Input() {}

    // Call superclass for assignment operator.
    inline Input& operator=(const T& value) { return (Input&) WireTemplateBase<T>::operator=(value); }

private:
    // Common constructor for all four variants.
    void constructor_common(const Module* p, const char* str, const T* init) {
        if (p) {
// printf("Create %s %s: sensitizes %s\n",
    // boost::core::demangle(typeid(*this).name()).c_str(), this->instanceName().c_str(), p->instanceName().c_str());
            // connect input to its parent and optionally initialize
            this->sensitized_module = const_cast<Module*>(p);
            if (init) {
                this->value = *init;
                this->clear_x_states();
                const_cast<Module*>(this->root_instance)->trigger_module(this->sensitized_module);
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
    // Constructors/Destructor.
    Output(const Module* p, const char* str) : WireTemplateBase<T>(p, str, W)
        { constructor_common(p, NULL); }
    Output(const Module* p, const char* str, const T& init) : WireTemplateBase<T>(p, str, W) 
        { constructor_common(p, &init); }
    Output(const Module* p, const std::string& nm) : WireTemplateBase<T>(p, nm, W) 
        { constructor_common(p, NULL); }
    Output(const Module* p, const std::string& nm, const T& init) : WireTemplateBase<T>(p, nm, W) 
        { constructor_common(p, &init); }
    virtual ~Output() {}

    // Call superclass for assignment operator.
    inline Output& operator=(const T& value) { return (Output&) WireTemplateBase<T>::operator=(value); }

private:
    // Common constructor for all four variants.
    void constructor_common(const Module* p, const T* init) {
        if (p) {
// printf("Create %s %s: sensitizes %s\n",
    // boost::core::demangle(typeid(*this).name()).c_str(), this->instanceName().c_str(), p->parent()->instanceName().c_str());
            if (p->parent())
                this->sensitized_module = const_cast<Module*>(p->parent());
            else 
                throw std::invalid_argument("Output cannot be declared on a top-level module (i.e., a Testbench)");
            if (init) {
                this->value = *init;
                this->clear_x_states();
                const_cast<Module*>(this->root_instance)->trigger_module(this->sensitized_module);
            }
        } else
            throw std::invalid_argument("Output must be declared inside a module");
    }
};

#endif //  _PV_WIRES_H_
