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
 *  Ties to Modules:
 *      - sensitize(): sensitize a Module to this wire.
 *      - desensitize(): desensitize a Module to this wire.
 *  Related to VCD dumps:
 *      - vcd_neg_edge_update(): emit changed values to VCD file; abstract method, implemented in subclass
 */

class WireBase {
protected:
    // Constructor/Destructor: protected so only subclass can use.
    WireBase(const Module* p, const char* str) : parent_module(p), wire_name(str) { constructor_common(); }
    WireBase(const Module* p, const std::string& nm) : parent_module(p), wire_name(nm) { constructor_common(); }
    virtual ~WireBase() {
        Module* m = const_cast<Module*>(parent_module);
        m->global.dissociate_wire_in_module(this, parent_module);
        m->global.wireList().erase(this);
        m->global.desensitize_wire(this);
    }

public:
    // General naming methods.
    const inline std::string name() const { return wire_name; }
    const std::string instanceName() const { 
        if (parent_module == NULL)
            return wire_name;
        else {
            std::string tmp = parent_module->instanceName() + "." + wire_name;
            return tmp;
        }
    }

    // Manually manage module sensitization.
    void sensitize(const Module* theModule) { const_cast<Module*>(parent_module)->global.sensitize_to_wire(theModule, this); }
    void desensitize(const Module* theModule) { const_cast<Module*>(parent_module)->global.desensitize_to_wire(theModule, this); }

    // Required getter for width of wire.
    virtual const int get_width() const = 0;

    // VCD related.
    std::string vcd_id_str;
    virtual void emit_vcd_definition(std::ostream* vcd_stream) const = 0;
    virtual void emit_vcd_dumpvars(std::ostream* vcd_stream) const = 0;
    virtual void emit_vcd_dumpon(std::ostream* vcd_stream) const = 0;
    virtual void emit_vcd_dumpoff(std::ostream* vcd_stream) const = 0;
    virtual void emit_vcd_neg_edge_update(std::ostream* vcd_stream) = 0;

protected:
    // Parent module and name.
    const Module* parent_module;
    const std::string wire_name;

    // Schedule modules based on sensitivity to this wire.
    void schedule_sensitized_modules() {
        set_m_data_t& runq = const_cast<Module*>(parent_module)->global.runq();
        umm_wm_iter_pair_t range = const_cast<Module*>(parent_module)->global.trigger_wire_module().equal_range(this);
        for ( ; range.first != range.second; range.first++)
            runq.insert(runq.cend(), range.first->second);
    }

private: 
    // Common constructor code.
    void constructor_common() {
        // Parent cannot be NULL.
        assert(parent_module != NULL);

        // Associate to parent module, add to global list of wires.
        const_cast<Module*>(parent_module)->global.associate_module_wire(parent_module, this);
        const_cast<Module*>(parent_module)->global.wireList().insert(this);

        // Initialize VCD ID.
        std::stringstream ss;
        ss << "@" << std::hex << parent_module->global.vcd_id_count()++;
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
 *      - The usual suspects...
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
    // Constructor/Destructor.
    WireTemplateBase(const Module* p, const char* str, const int W) : WireBase(p, str) { constructor_common(W); }
    WireTemplateBase(const Module* p, const std::string& nm, const int W) : WireBase(p, nm)  { constructor_common(W); }
    virtual ~WireTemplateBase() { if (is_default_printer) delete v2s; }

    // Wire value getter/setter.
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
            schedule_sensitized_modules();
        } 
        return *this;
    }

    // VCD string printer setter.
    void set_vcd_string_printer(const vcd::value2string_t<T>* p) { 
        if (is_default_printer) delete v2s;
        v2s = p;
        is_default_printer = false;
    }

    // width setter/getter
    void set_width(const int wv) { width = wv; }
    const int get_width() const { return width; }

    // X state setters/getters.
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

    // Get parent module reference.
    const Module* parent() const { return this->parent_module; }

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
    void emit_vcd_definition(std::ostream* vcd_stream) const
        { *vcd_stream << "$var wire " << width << " " << vcd_id_str << " " << wire_name << " $end" << std::endl; }
    void emit_vcd_dumpvars(std::ostream* vcd_stream) const
        { *vcd_stream << (is_x ? v2s->undefined() : (*v2s)(value)) << (width > 1 ? " " : "") << vcd_id_str << std::endl; }
    void emit_vcd_dumpon(std::ostream* vcd_stream) const
        { *vcd_stream << (is_x ? v2s->undefined() : (*v2s)(value)) << (width > 1 ? " " : "") << vcd_id_str << std::endl; }
    void emit_vcd_dumpoff(std::ostream* vcd_stream) const
        { *vcd_stream << v2s->undefined() << (width > 1 ? " " : "") << vcd_id_str << std::endl; }

    // VCD updates on negative edge of clock: if value has changed AND we are dumping VCD, print the change.
    void emit_vcd_neg_edge_update(std::ostream* vcd_stream) {
        if (vcd_stream && (is_x ? (is_x ^ was_x) : (was_x || value != old_value)))
            *vcd_stream << (is_x ? v2s->undefined() : (*v2s)(value)) << (width > 1 ? " " : "") << vcd_id_str << std::endl;
        was_x = is_x;
        old_value = value;
    }

protected:
    // Width parameter.
    int width;

    // Noting state changes in any clock.
    bool written_this_clock;

    // The current and old value of the wire. "old" means the value it had at the start of a clock.
    T value;
    T old_value;

    // Record the "x" state of the wire. This takes precedence over the bvalue of the wire.
    bool is_x;
    bool was_x;

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
    // Constructors/Destructor.
    Wire(const Module* p, const char* str) : WireTemplateBase<T>(p, str, W)
        { constructor_common(p, str, NULL); }
    Wire(const Module* p, const char* str, const T& init) : WireTemplateBase<T>(p, str, W)
        { constructor_common(p, str, &init); }
    Wire(const Module* p, const std::string& nm) : WireTemplateBase<T>(p, nm, W)
        { constructor_common(p, nm.c_str(), NULL); }
    Wire(const Module* p, const std::string& nm, const T& init) : WireTemplateBase<T>(p, nm, W)
        { constructor_common(p, nm.c_str(), &init); }
    virtual ~Wire() { this->desensitize(this->parent_module); }

    // Call superclass for assignment operator.
    inline Wire& operator=(const T& value) { return (Wire&) WireTemplateBase<T>::operator=(value); }

private:
    // Common constructor for all four variants.
    void constructor_common(const Module* p, const char* str, const T* init) {
        if (p) {
            this->sensitize(p);
            if (init) {
                this->value = *init;
                this->schedule_sensitized_modules();
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
    // Constructors/Destructor.
    Input(const Module* p, const char* str) : WireTemplateBase<T>(p, str, W)
        { constructor_common(p, str, NULL); }
    Input(const Module* p, const char* str, const T& init) : WireTemplateBase<T>(p, str, W) 
        { constructor_common(p, str, &init); }
    Input(const Module* p, const std::string& nm) : WireTemplateBase<T>(p, nm, W) 
        { constructor_common(p, nm.c_str(), NULL); }
    Input(const Module* p, const std::string& nm, const T& init) : WireTemplateBase<T>(p, nm, W) 
        { constructor_common(p, nm.c_str(), &init); }
    virtual ~Input() { this->desensitize(this->parent_module); }

    // Call superclass for assignment operator.
    inline Input& operator=(const T& value) { return (Input&) WireTemplateBase<T>::operator=(value); }

private:
    // Common constructor for all four variants.
    void constructor_common(const Module* p, const char* str, const T* init) {
        if (p) {
            // connect input to its parent and optionally initialize
            this->sensitize(p);
            if (init) {
                this->value = *init;
                this->schedule_sensitized_modules();
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
    // Constructors/Destructor.
    Output(const Module* p, const char* str) : WireTemplateBase<T>(p, str, W)
        { constructor_common(p, NULL); }
    Output(const Module* p, const char* str, const T& init) : WireTemplateBase<T>(p, str, W) 
        { constructor_common(p, &init); }
    Output(const Module* p, const std::string& nm) : WireTemplateBase<T>(p, nm, W) 
        { constructor_common(p, NULL); }
    Output(const Module* p, const std::string& nm, const T& init) : WireTemplateBase<T>(p, nm, W) 
        { constructor_common(p, &init); }
    virtual ~Output() {
        if (this->parent_module->parent())
            this->desensitize(this->parent_module->parent());
    }

    // Call superclass for assignment operator.
    inline Output& operator=(const T& value) { return (Output&) WireTemplateBase<T>::operator=(value); }

private:
    // Common constructor for all four variants.
    void constructor_common(const Module* p, const T* init) {
        if (p) {
            if (p->parent())
                this->sensitize(p->parent());
            if (init) {
                this->value = *init;
                this->schedule_sensitized_modules();
                this->clear_x_states();
            }
        } else
            throw std::invalid_argument("Output must be declared inside a module");
    }
};

#endif //  _PV_WIRES_H_
