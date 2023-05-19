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
 * This header actually defines 6 classes/subclasses:
 *
 *                         ┌─────────────────────┐
 *                         │      WireBase       │
 *                         └──────────┬──────────┘
 *                                    │
 *                         ┌──────────▼──────────┐
 *                         │ WireTemplateBase<T> │
 *                         └──────────┬──────────┘
 *                                    │
 *        ┌──────────────────┬────────┴────────┬─────────────────┐
 * ┌──────▼──────┐    ┌──────▼──────┐   ┌──────▼──────┐   ┌──────▼──────┐
 * │ Input<T,W>  │    │  Wire<T,W>  │   │ QWire<T,W>  │   │ Output<T,W> │
 * └─────────────┘    └─────────────┘   └─────────────┘   └─────────────┘
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
 * Lastly, below the WireTemplateBase<T> subclass are four variant subsubclasses, Inputs, Wires, QWires, and Outputs.
 * Inputs are intended to be instances as input ports on a module, and the subsubclass auto-sensitizes its
 * parent container to input changes. The Wire subsubclass are intended to be instances with its parent container
 * module as well,and also auto-sensitizes its parent to value changes. QWires are like Wires except changes to
 * QWires DO NOT cause reevaluation of their container Module.  Functionally, Inputs, Wires, and QWires are identical.
 * Finally, Outputs are intended to be instances as output ports of a module, and the subsubclass auto-sensitizes its
 * grandparent (if it exists) to value changes. All four subsubclasses are "final", meaning they cannot be further
 * subclassed. All four classes also support setting a custom wire width (in bits) as well as allowing for initialization
 * when instanced (non-initialized begin in an 'x' state).
 */

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
 *      - parent(): return pointer to parent Module
 *      - top(): returns pointer to topmost module instance (a Testbench).
 *      - {virtual, abstract} get_width() - returns width of wire in bits.
 */

class WireBase {
protected:
    // Constructor/Destructor: protected so only subclass can use. Note that every wire type must have a parent module.
    // The test to set root instance checks for null parent regardless in case the constructor is erroneously invoked; 
    // the common_constructor() call will catch the error. 
    WireBase(const Module* p, const char* str) : parent_module(p), root_instance(p ? p->root_instance : NULL), wire_name(str)
        { constructor_common(); }
    WireBase(const Module* p, const std::string& nm) : parent_module(p), root_instance(p ? p->root_instance : NULL), wire_name(nm)
        { constructor_common(); }
    virtual ~WireBase() { const_cast<Module*>(parent_module)->remove_wire_instance(this); }

public:
    // Disable default and copy constructors.
    WireBase() = delete;
    WireBase(const WireBase& w) = delete;

    // General naming methods.
    const inline std::string name() const { return wire_name; }
    const std::string instanceName() const { return parent_module->instanceName() + "." + wire_name; }

    // Get parent module & top level (root/testbench) pointer. Both are non-NULL.
    inline const Module* parent() const { return parent_module; }
    inline const Module* top() const { return root_instance; }

    // Required getter for width of wire.
    virtual const int get_width() const = 0;

protected:
    // Friend classes.
    friend class Testbench;
    friend class vcd::writer; 

    // Parent modules and name.
    const Module* parent_module;
    const Module* root_instance;
    const std::string wire_name;

    // Save sensitized module (if any)
    Module *sensitized_module;

    // Virtual method to assign an 'x' state to a wire. Implemented in WireTemplateBase<T>.
    virtual void assign_x() = 0;

    // Virtual method to reset wire to the state it had when instanced. Implemented in WireTemplateBase<T>.
    virtual void reset_to_instance_state() = 0;

    // Virtual method for mandatory negative clock edge update. Implemented in WireTemplateBase<T>.
    virtual void neg_edge_update() = 0;

    // VCD related. The virtual methods below can't be implemented in the base class as the data type
    // is not known in the base class. However, we do want methods using the Wire-type classes the ability
    // to execute these methods in a type-independent manner, hence the virtual functions below.
    std::string vcd_id_str;
    virtual void emit_vcd_definition(std::ostream* vcd_stream) const = 0;
    virtual void emit_vcd_dumpvars(std::ostream* vcd_stream) const = 0;
    virtual void emit_vcd_dumpon(std::ostream* vcd_stream) const = 0;
    virtual void emit_vcd_dumpoff(std::ostream* vcd_stream) const = 0;
    virtual void emit_vcd_neg_edge_update(std::ostream* vcd_stream) = 0;

private: 
    // Common constructor code.
    void constructor_common() {
        // Parent cannot be NULL. 
        if (parent_module == NULL)
            throw std::invalid_argument("Wire must be declared withing a Module");

        // Associate to parent module. Default is no sensitization.
        const_cast<Module*>(parent_module)->add_wire_instance(this);
        sensitized_module = NULL;

        // Initialize VCD ID.
        std::stringstream ss;
        ss << "@" << std::hex << const_cast<Module*>(root_instance)->vcd_id_count()++;
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
 *  Width setter/getter:
 *      - set_width(): set the width of the wire
 *      - get_width(): return the width of the wire
 *  Value getter/setter:
 *      - operator T(): return current value of the wire
 *      - operator=(): assign a value to the wire and return reference to the object
 *  X state getters/setters:
 *      - value_is_x(): return true if current value is an "X".
 * 		- value_was_x(): return true if current value was an "X".
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
 */

template <typename T>
class WireTemplateBase : public WireBase {
protected:
    // Constructor/Destructor.
    WireTemplateBase(const Module* p, const char* str, const int W, const T* init) : WireBase(p, str), v2s(def_printer) { constructor_common(W, init); }
    WireTemplateBase(const Module* p, const std::string& nm, const int W, const T* init) : WireBase(p, nm), v2s(def_printer) { constructor_common(W, init); }
    virtual ~WireTemplateBase() {}

public:
    // Disable default and copy constructors.
    WireTemplateBase() = delete;
    WireTemplateBase(const WireTemplateBase& w) = delete;

    // Width setter/getter.
    void set_width(const int wv) { width = wv; v2s.set_width(width); }
    const int get_width() const { return width; }

    // Wire value getter/setter.
    inline operator T() const { return value; }
    WireTemplateBase& operator=(const T& v) { common_assignment(false, v); return *this; }
    template <typename U> WireTemplateBase& operator=(const U& v) { common_assignment(false, (T) v); return *this; }

    // General wire->wire assignment (same or different source type).
    WireTemplateBase& operator=(const WireTemplateBase& wv)
        { common_assignment(wv.is_x, wv.value); return *this; }
    template <typename U> WireTemplateBase& operator=(const WireTemplateBase<U>& wv)
        { common_assignment(wv.is_x, (T) wv.value); return *this; }

    // X state setters/getters.
    inline bool value_is_x() const { return is_x; }
    inline bool value_was_x() const { return was_x; }
    inline void assign_x() { common_assignment(true, value); }

    // VCD string printer setter (override default).
    inline void set_vcd_string_printer(const vcd::value2string_t<T>& printer) { v2s = printer; }

    // Operator-assign overloads with "T' type value.
    inline WireTemplateBase& operator+=(const T& v)
		{ T new_v = value + v; common_assignment(is_x, new_v); return *this; }
    inline WireTemplateBase& operator-=(const T& v)
		{ T new_v = value - v; common_assignment(is_x, new_v); return *this; }
    inline WireTemplateBase& operator*=(const T& v)
		{ T new_v = value * v; common_assignment(is_x, new_v); return *this; }
    inline WireTemplateBase& operator/=(const T& v)
		{ T new_v = value / v; common_assignment(is_x, new_v); return *this; }
    inline WireTemplateBase& operator%=(const T& v)
		{ T new_v = value % v; common_assignment(is_x, new_v); return *this; }
    inline WireTemplateBase& operator^=(const T& v)
		{ T new_v = value ^ v; common_assignment(is_x, new_v); return *this; }
    inline WireTemplateBase& operator&=(const T& v)
		{ T new_v = value & v; common_assignment(is_x, new_v); return *this; }
    inline WireTemplateBase& operator|=(const T& v)
		{ T new_v = value | v; common_assignment(is_x, new_v); return *this; }

    // Operator-assign overloads with "WireTemplateBase<T>" type value.
    inline WireTemplateBase& operator+=(const WireTemplateBase& wv)
		{ T new_v = value + wv.v; common_assignment(is_x|wv.is_x, new_v); return *this; }
    inline WireTemplateBase& operator-=(const WireTemplateBase& wv)
		{ T new_v = value - wv.v; common_assignment(is_x|wv.is_x, new_v); return *this; }
    inline WireTemplateBase& operator*=(const WireTemplateBase& wv)
		{ T new_v = value * wv.v; common_assignment(is_x|wv.is_x, new_v); return *this; }
    inline WireTemplateBase& operator/=(const WireTemplateBase& wv)
		{ T new_v = value / wv.v; common_assignment(is_x|wv.is_x, new_v); return *this; }
    inline WireTemplateBase& operator%=(const WireTemplateBase& wv)
		{ T new_v = value % wv.v; common_assignment(is_x|wv.is_x, new_v); return *this; }
    inline WireTemplateBase& operator^=(const WireTemplateBase& wv)
		{ T new_v = value ^ wv.v; common_assignment(is_x|wv.is_x, new_v); return *this; }
    inline WireTemplateBase& operator&=(const WireTemplateBase& wv)
		{ T new_v = value & wv.v; common_assignment(is_x|wv.is_x, new_v); return *this; }
    inline WireTemplateBase& operator|=(const WireTemplateBase& wv)
		{ T new_v = value | wv.v; common_assignment(is_x|wv.is_x, new_v); return *this; }

    // Operator-assign overloads with "WireTemplateBase<U>" type value.
    template <typename U> inline WireTemplateBase& operator+=(const WireTemplateBase& wv)
		{ T new_v = value + (T) wv.v; common_assignment(is_x|wv.is_x, new_v); return *this; }
    template <typename U> inline WireTemplateBase& operator-=(const WireTemplateBase& wv)
		{ T new_v = value - (T) wv.v; common_assignment(is_x|wv.is_x, new_v); return *this; }
    template <typename U> inline WireTemplateBase& operator*=(const WireTemplateBase& wv)
		{ T new_v = value * (T) wv.v; common_assignment(is_x|wv.is_x, new_v); return *this; }
    template <typename U> inline WireTemplateBase& operator/=(const WireTemplateBase& wv)
		{ T new_v = value / (T) wv.v; common_assignment(is_x|wv.is_x, new_v); return *this; }
    template <typename U> inline WireTemplateBase& operator%=(const WireTemplateBase& wv)
		{ T new_v = value % (T) wv.v; common_assignment(is_x|wv.is_x, new_v); return *this; }
    template <typename U> inline WireTemplateBase& operator^=(const WireTemplateBase& wv)
		{ T new_v = value ^ (T) wv.v; common_assignment(is_x|wv.is_x, new_v); return *this; }
    template <typename U> inline WireTemplateBase& operator&=(const WireTemplateBase& wv)
		{ T new_v = value & (T) wv.v; common_assignment(is_x|wv.is_x, new_v); return *this; }
    template <typename U> inline WireTemplateBase& operator|=(const WireTemplateBase& wv)
		{ T new_v = value | (T) wv.v; common_assignment(is_x|wv.is_x, new_v); return *this; }

    // Shift-assign overloads.
    inline WireTemplateBase& operator>>=(const int& v)
		{ T new_v = value >> v; common_assignment(is_x, new_v); return *this; }
    inline WireTemplateBase& operator<<=(const int& v)
		{ T new_v = value << v; common_assignment(is_x, new_v); return *this; }

    // Auto increment/decrement overloads.
    inline WireTemplateBase& operator++()   
		{ T new_v = value + 1; common_assignment(is_x, new_v); return *this; }
    inline WireTemplateBase& operator--()   
		{ T new_v = value - 1; common_assignment(is_x, new_v); return *this; }
    inline WireTemplateBase  operator++(int)
		{ WireTemplateBase tmp = *this; T new_v = value + 1; common_assignment(is_x, new_v); return tmp; }
    inline WireTemplateBase  operator--(int)
		{ WireTemplateBase tmp = *this; T new_v = value - 1; common_assignment(is_x, new_v); return tmp; }

protected:
    // Width parameter.
    int width;

    // Record the "x" state of the wire. This takes precedence over the value of the wire.
    // "is_x" is the current state of the wire, "was_x" is the state it had at the start of a clock, and "init_x" is the
    // state it had when the wire was created.
    bool is_x;
    bool was_x;
    bool init_x;

    // The current and old value of the wire. "old" means the value it had at the start of a clock.
    T value;
    T old_value;
    T init_value;

    // Method to wipe both past and present X states w/o triggering an eval.
    inline void clear_x_states() { is_x = was_x = false; }

private:
    // Friend classes.
    friend class Testbench;
    friend class vcd::writer;

    // Reset state of wire back to the state it had when it was instanced.
    // This call does NOT trigger evals.
    void reset_to_instance_state() {
        was_x = is_x = init_x;
        old_value = value = init_value;
    }

    // VCD string printer (default)
    vcd::value2string_t<T> def_printer = { value }; 
    vcd::value2string_t<T>& v2s;

    // VCD dump methods.
    // Should NOT be called if vcd_stream is NULL (i.e., we are not dumping a VCD).
    void emit_vcd_definition(std::ostream* vcd_stream) const
        { *vcd_stream << "$var wire " << this->width << " " << vcd_id_str << " " << wire_name << vcd::width2index(this->width) << " $end" << std::endl; }
    void emit_vcd_dumpvars(std::ostream* vcd_stream) const
        { *vcd_stream << (is_x ? v2s.undefined() : (v2s)(value)) << (width > 1 ? " " : "") << vcd_id_str << std::endl; }
    void emit_vcd_dumpon(std::ostream* vcd_stream) const
        { *vcd_stream << (is_x ? v2s.undefined() : (v2s)(value)) << (width > 1 ? " " : "") << vcd_id_str << std::endl; }
    void emit_vcd_dumpoff(std::ostream* vcd_stream) const
        { *vcd_stream << v2s.undefined() << (width > 1 ? " " : "") << vcd_id_str << std::endl; }

    // VCD updates on negative edge of clock: if value has changed print the change.
    // Should NOT be called if vcd_stream is NULL (i.e., we are not dumping a VCD).
    void emit_vcd_neg_edge_update(std::ostream* vcd_stream) {
        if (is_x ? !was_x : (was_x || value != old_value))
            *vcd_stream << (is_x ? v2s.undefined() : (v2s)(value)) << (width > 1 ? " " : "") << vcd_id_str << std::endl;
    }

    // Mandatory negedge update call.
    inline void neg_edge_update() {
        was_x = is_x;
        old_value = value;
    }

    // Common assignment code for all cases (whether transition to X or regular value).
    void common_assignment(const bool to_x, const T& v) {
        // If assigning a X, value "v" is ignored.
        if (to_x) {
            // If wire was an X, then wire is unchanged. Otherwise, mark it as changed.
            if (was_x)
                const_cast<Module*>(root_instance)->remove_changed_wire(this);
            else
                const_cast<Module*>(root_instance)->add_changed_wire(this);

            // If the wire is not X, treat this as a potential change and force eval on any sensitized module.
            if (!is_x && sensitized_module != NULL)
                const_cast<Module*>(root_instance)->trigger_module(sensitized_module);

            // Wire is now X.
            is_x = true;
        }

        // Change logic if not to 'x' state:
        // 
        //  was_x   is_x    v != old_value  v != value  |   change?     trigger?
        //  --------------------------------------------------------------------
        //  0       0       0               0           |   N           N
        //  0       0       0               1           |   N           Y
        //  0       0       1               0           |   Y           N
        //  0       0       1               1           |   Y           Y
        //  0       1       0               -           |   N           Y
        //  0       1       1               -           |   Y           Y
        //  1       0       -               0           |   Y           N
        //  1       0       -               1           |   Y           Y
        //  1       1       -               -           |   Y           Y
        // 
        else {
            if (was_x || v != old_value)
                const_cast<Module*>(root_instance)->add_changed_wire(this);
            else
                const_cast<Module*>(root_instance)->remove_changed_wire(this);
            if ((is_x || v != value) && sensitized_module != NULL)
                const_cast<Module*>(root_instance)->trigger_module(sensitized_module);

            // Clear any X state and save new value.
            is_x = false;
            value = v;
        }
    }

    // Common constructor code.
    // Set default VCD string printer and initialize wire to X state.
    // If initializer is provided, use it; otherwise, set state to 'X'.
    void constructor_common(const int W, const T* init) {
        width = (W > 0) ? W : vcd::bitwidth<T>();
        v2s.set_width(width);
        if (init) {
            init_x = was_x = is_x = false;
            init_value = old_value = value = *init;
            if (this->sensitized_module)
                const_cast<Module*>(this->root_instance)->trigger_module(sensitized_module);
        } else
            init_x = was_x = is_x = true;
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
    Wire(const Module* p, const char* str) : WireTemplateBase<T>(p, str, W, NULL) { constructor_common(p); }
    Wire(const Module* p, const char* str, const T& init) : WireTemplateBase<T>(p, str, W, &init) { constructor_common(p); }
    Wire(const Module* p, const std::string& nm) : WireTemplateBase<T>(p, nm, W, NULL) { constructor_common(p); }
    Wire(const Module* p, const std::string& nm, const T& init) : WireTemplateBase<T>(p, nm, W, &init) { constructor_common(p); }
    virtual ~Wire() {}

    // Call superclass for assignment operator.
    inline Wire& operator=(const T& value) { return (Wire&) WireTemplateBase<T>::operator=(value); }
    template <typename U> inline Wire& operator=(const U& value) { return (Wire&) WireTemplateBase<T>::operator=((T) value); }
    inline Wire& operator=(const WireTemplateBase<T>& wv) { return (Wire&) WireTemplateBase<T>::operator=(wv); }
    template <typename U> inline Wire& operator=(const WireTemplateBase<U>& wv) { return (Wire&) WireTemplateBase<T>::operator=(wv); }

private:
    // Common constructor for all four variants.
    void constructor_common(const Module* p) {
        if (p) this->sensitized_module = const_cast<Module*>(p);
        else throw std::invalid_argument("Wire must be declared inside a module");
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
    QWire(const Module* p, const char* str) : WireTemplateBase<T>(p, str, W, NULL) { constructor_common(p); }
    QWire(const Module* p, const char* str, const T& init) : WireTemplateBase<T>(p, str, W, &init) { constructor_common(p); }
    QWire(const Module* p, const std::string& nm) : WireTemplateBase<T>(p, nm, W, NULL) { constructor_common(p); }
    QWire(const Module* p, const std::string& nm, const T& init) : WireTemplateBase<T>(p, nm, W, &init) { constructor_common(p); }
    virtual ~QWire() {}

    // Call superclass for assignment operator.
    inline QWire& operator=(const T& value) { return (QWire&) WireTemplateBase<T>::operator=(value); }
    template <typename U> inline QWire& operator=(const U& value) { return (QWire&) WireTemplateBase<T>::operator=((T) value); }
    inline QWire& operator=(const WireTemplateBase<T>& wv) { return (QWire&) WireTemplateBase<T>::operator=(wv); }
    template <typename U> inline QWire& operator=(const WireTemplateBase<U>& wv) { return (QWire&) WireTemplateBase<T>::operator=(wv); }

private:
    // Common constructor for all four variants.
    void constructor_common(const Module* p) {
        if (!p) throw std::invalid_argument("QWire must be declared inside a module");
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
    Input(const Module* p, const char* str) : WireTemplateBase<T>(p, str, W, NULL) { constructor_common(p); }
    Input(const Module* p, const char* str, const T& init) : WireTemplateBase<T>(p, str, W, &init) { constructor_common(p); }
    Input(const Module* p, const std::string& nm) : WireTemplateBase<T>(p, nm, W, NULL) { constructor_common(p); }
    Input(const Module* p, const std::string& nm, const T& init) : WireTemplateBase<T>(p, nm, W, &init) { constructor_common(p); }
    virtual ~Input() {}

    // Call superclass for assignment operator.
    inline Input& operator=(const T& value) { return (Input&) WireTemplateBase<T>::operator=(value); }
    template <typename U> inline Input& operator=(const U& value) { return (Input&) WireTemplateBase<T>::operator=((T) value); }
    inline Input& operator=(const WireTemplateBase<T>& wv) { return (Input&) WireTemplateBase<T>::operator=(wv); }
    template <typename U> inline Input& operator=(const WireTemplateBase<U>& wv) { return (Input&) WireTemplateBase<T>::operator=(wv); }

private:
    // Common constructor for all four variants.
    void constructor_common(const Module* p) {
        if (p) this->sensitized_module = const_cast<Module*>(p);
        else throw std::invalid_argument("Input must be declared inside a module");
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
    Output(const Module* p, const char* str) : WireTemplateBase<T>(p, str, W, NULL) { constructor_common(p); }
    Output(const Module* p, const char* str, const T& init) : WireTemplateBase<T>(p, str, W, &init) { constructor_common(p); }
    Output(const Module* p, const std::string& nm) : WireTemplateBase<T>(p, nm, W, NULL) { constructor_common(p); }
    Output(const Module* p, const std::string& nm, const T& init) : WireTemplateBase<T>(p, nm, W, &init) { constructor_common(p); }
    virtual ~Output() {}

    // Call superclass for assignment operator.
    inline Output& operator=(const T& value) { return (Output&) WireTemplateBase<T>::operator=(value); }
    template <typename U> inline Output& operator=(const U& value) { return (Output&) WireTemplateBase<T>::operator=((T) value); }
    inline Output& operator=(const WireTemplateBase<T>& wv) { return (Output&) WireTemplateBase<T>::operator=(wv); }
    template <typename U> inline Output& operator=(const WireTemplateBase<U>& wv) { return (Output&) WireTemplateBase<T>::operator=(wv); }

private:
    // Common constructor for all four variants.
    void constructor_common(const Module* p) {
        if (p) {
            if (p->parent()) this->sensitized_module = const_cast<Module*>(p->parent());
            else throw std::invalid_argument("Output cannot be declared on a top-level module (i.e., a Testbench)");
        } else throw std::invalid_argument("Output must be declared inside a module");
    }
};

#endif //  _PV_WIRES_H_
