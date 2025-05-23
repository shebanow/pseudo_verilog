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

 #ifndef _PV_MODULE_H_
 #define _PV_MODULE_H_

/*
 * Module class. This is a container base class for module implementations.
 * Subclasses of the Module class exactly mirror module declarations in Verilog.
 * Wires, registers, or other modules instanced within a subclass are hierarchically
 * instanced below that subclass. 
 */

// Forward declarations.
class WireBase;
class RegisterBase;
namespace vcd { class writer; }

// Declare the Module base class.
class Module {
public:
    // Module constructors: variants are based on whether instance name is pass
    // as std::string or char*.
    Module(const Module* p, const std::string& str) : parent_module(p), 
        root_instance(p ? p->root_instance : this), instance_name(str)
            { constructor_common(); } 
    Module(const Module* p, const char* str) : parent_module(p), 
        root_instance(p ? p->root_instance : this), instance_name(str)
            { constructor_common(); }

    // Module destructor. The destructor only removes itself from the parent's instance list.
    virtual ~Module()
        { if (parent_module != NULL) 
            const_cast<Module*>(parent_module)->remove_module_instance(this); }

    // Getters to return parent and root instances.
    inline const Module* parent() const { return parent_module; }
    inline const Module* top() const { return root_instance; }

    // Getters for name and instance name.
    inline const std::string name() const { return instance_name; }
    const std::string instanceName() const {
        if (!parent_module)
            return instance_name;
        else {
            std::string tmp = parent_module->instanceName() + "." + instance_name;
            return tmp;
        }
    }

    // Virtual method to get current simulation clock #. 
    // Only really implemented in Testbench.
    virtual const uint32_t get_clock() const 
        { return root_instance ? root_instance->get_clock() : 0u; }

    // Required implementation: called to update the module upon change in its
    // sensitivity lists (wires or registers). 
    virtual void eval() = 0;

    // Begin/End methods to allow const iterations over instance lists.
    // For access safety, direct access to the instance lists are not allowed.
    std::set<const Module*>::const_iterator m_begin() const 
        { return module_list.cbegin(); }
    std::set<const WireBase*>::const_iterator w_begin() const 
        { return wire_list.cbegin(); }
    std::set<const RegisterBase*>::const_iterator r_begin() const 
        { return register_list.cbegin(); }
    std::set<const Module*>::const_iterator m_end() const 
        { return module_list.cend(); }
    std::set<const WireBase*>::const_iterator w_end() const 
        { return wire_list.cend(); }
    std::set<const RegisterBase*>::const_iterator r_end() const 
        { return register_list.cend(); }

    // Getter/setter on eval_has_been_called flag.
    inline const bool get_eval_has_been_called() const 
        { return eval_has_been_called; }
    inline void set_eval_has_been_called(const bool flag) 
        { eval_has_been_called = flag; }

    // Method to force evaluation (eval()) this clock or next on yourself.
    inline void force_eval() 
        { const_cast<Module*>(root_instance)->trigger_module(this); }
    void force_eval_next_clock() 
        { needs_evaluation = true; }

    // Getters/setters on needs_evaluation.
    inline bool get_needs_evaluation() const 
        { return needs_evaluation; }
    inline void set_needs_evaluation(const bool flag) 
        { needs_evaluation = flag; }

protected:
    // Friend classes
    friend class WireBase;
    friend class RegisterBase;
    friend class vcd::writer;
    template <typename T> friend class WireTemplateBase;
    template <typename T, int W> friend class Register;

    // Virtual function to evaluate positive edge flops.
    virtual void pos_edge(const Module* m) {}

    // Methods to add submodules, wires, and register instances to the module
    void add_module_instance(const Module* m) 
        { (void) module_list.insert(module_list.end(), m); }
    void add_wire_instance(const WireBase* w) 
        { (void) wire_list.insert(wire_list.end(), w); }
    void add_register_instance(const RegisterBase* r) 
        { (void) register_list.insert(register_list.end(), r); }

    // Methods to remove submodules, wires, and register 
    // instances from the module.
    void remove_module_instance(const Module* m) 
        { module_list.erase(m); }
    void remove_wire_instance(const WireBase* w) 
        { wire_list.erase(w); }
    void remove_register_instance(const RegisterBase* r) 
        { register_list.erase(r); }

    // Methods to keep track of changed/unchanged wires and changed registers.
    // Actual implementation in Testbench; calls in Wires/Registers should be to root_instance.
    virtual void add_changed_wire(const WireBase* theWire) {}
    virtual void remove_changed_wire(const WireBase* theWire) {}
    virtual void add_changed_register(const RegisterBase* theRegister) {}

    // Virtual function overloaded in Testbench to trigger a module.
    virtual void trigger_module(const Module* theModule) {}

    // For value change tracing. Actual implementation in Testbench.
    virtual void trace_string_size(const std::string iname, const int width) {}
    virtual const pv::ValueChangeRecord get_trace_change(const std::string iname) 
        { pv::ValueChangeRecord dummy; return dummy; }
    virtual void set_trace_change(const std::string iname, const pv::ValueChangeRecord& vcr) {}

private:
    // Module parent; NULL => top level module. Keep track of root of Module instance tree.
    const Module* parent_module;
    const Module* root_instance;

    // Marker to indicate module needs evaluation
    bool needs_evaluation;

    // Module instance name.
    const std::string instance_name;

    // Data structures to keep track of module instances.
    std::set<const Module*> module_list;
    std::set<const WireBase*> wire_list;
    std::set<const RegisterBase*> register_list;

    // Flag to keep track if module has been evaluated this clock cycle or not.
    bool eval_has_been_called;

    // Keeping track of assigned VCD ID counts.
    virtual uint32_t& vcd_id_count() { static uint32_t tmp = 0; return tmp; }

    // Constructor common code. Records root of module instance 
    // tree and adds this instance to parent if it exists.
    void constructor_common() {
        eval_has_been_called = false;
        needs_evaluation = false;
        if (parent_module) {
            root_instance = parent_module->root_instance;
            const_cast<Module*>(parent_module)->add_module_instance(this);
        } else
            root_instance = this;
    }
};

 #endif //  _PV_MODULE_H_
