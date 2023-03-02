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
#include <set>
#include "pv_global.h"

 #ifndef _PV_MODULE_H_
 #define _PV_MODULE_H_

/*
 * General note about "static" structures declared in this header. In the Module class,
 * there are a set iof "global" data types declared. These data structures are defined
 * in the "pv_global.h" header. As designed, all such declarations are static (aka "global") -
 * there is one such data structure for all instances of the Module class. These "header-only"
 * declarations take the place of actual "static" defines in a ".cpp"/".cc" file, and consequently
 * obviate the need for a separately compiled and linked library module. As to why they are
 * declare as part of the Module class, this header file appears before most others, and specifically
 * the wire.h and register.h headers, *and* Modules must be instanced before any Wire or Register
 * can be instanced.
 *
 * There are three categories of such declarations:
 *  1. "xxxInstanceDB": three of them: Module, WireBase, and RegisterBase. These data structures
 *      record which instances of each class are currently active.
 *  2. "xxxConnectionDB": two of them recording the sensistation of Wires/Registers to Modules.
 *  3. "runQ": a list of Module pointers that have been triggered by wire or registers changes they
 *      are sensitized to.
 */

/*
 * Module class.
 * This is a container base class for module implementations.
 * Besides the constructor, one virtual function must be implemented
 * by the subclass inheriting from this class, the operator() functor.
 * This is called each time this module must be evaluated. In Verilog,
 * for a module "m", this is equivalent to "always @(*) m;", i.e.,
 * a behavioral evaluation.
 */

class WireBase;                     // forward declaration
class RegisterBase;                 // forward declaration
namespace vcd { class writer; }     // forward declaration
class Module {
public:
    // Module constructors/destructor.
    Module(const Module* p, const std::string& str) : parent_module(p), nm(str) { constructor_common(); }
    Module(const Module* p, const char* str) : parent_module(p), nm(str) { constructor_common(); }
    virtual ~Module() {
        // TODO: this needs to be fixed.
        // If module has a parent, disconnect instance of this from that parent and desensitize outputs
        if (parent_module) {
            // global.
            const_cast<Module*>(parent_module)->moduleInstanceDB.erase(parent_module, this);
            for (std::set<const WireBase*>::const_iterator it = output_list.begin(); it != output_list.end(); it++)
                global.desensitize_to_wire(parent_module, *it);
        }

        // For all submodules instanced by this module, remove the relationship.
        // (This loop should not execute if destructors in submodules work correctly.)
        UMM_pair_t<const Module*, const Module*> module_range = moduleInstanceDB.find_all_lr(this);
        for (UMM_iter_t<const Module*, const WireBase*> it = module_range.first; it != module_range.second; it++)
            moduleInstanceDB.erase(this, it->second);

        // For all wires instanced by this module, remove instance relationship.
        // (This loop should not execute if destructors in wires work correctly.)
        UMM_pair_t<const Module*, const WireBase*> wire_range = wireInstanceDB.find_all_lr(this);
        for (UMM_iter_t<const Module*, const WireBase*> it = wire_range.first; it != wire_range.second; it++)
            wireInstanceDB.erase(this, it->second);

        // For all wires sensitizing this module, remove connection relationship.
        wire_range = wireConnectionDB.find_all_rl(this);
        for (UMM_iter_t<const Module*, const WireBase*> it = wire_range.first; it != wire_range.second; it++)
            wireConnectionDB.erase(it->second, this);

        // For all registers instanced by this module, remove
        // (This loop should not execute if destructors in registers work correctly.)
        UMM_pair_t<const Module*, const RegisterBase*> module_register_range = registerInstanceDB.find_all_lr(this);
        for (UMM_iter_t<const Module*, const RegisterBase*> it = module_register_range.first; it != module_register_range.second; it++)
            registerInstanceDB.erase(this, it->second);

        // For all registers sensitizing this module, remove connection relationship.
        module_register_range = registerConnectionDB.find_all_rl(this);
        for (UMM_iter_t<const Module*, const RegisterBase*> it = module_register_range.first; it != module_register_range.second; it++)
            registerConnectionDB.erase(it->second, this);

        // Remove from RUNQ if there
        runq.erase(this);
    }

    // Required implementation: called to update the module upon change in its sensitivity lists
    // (wires or registers). 
    virtual void eval() = 0;

    // Getters for type, name and instance name.
    // inline const std::string typeName() const { return type_printer(*this); }
    inline const std::string name() const { return nm; }
    const std::string instanceName() const {
        if (!parent_module)
            return nm;
        else {
            std::string tmp = parent_module->instanceName() + "." + nm;
            return tmp;
        }
    }

    // Getter to return parent
    inline const Module* parent() const { return parent_module; }

    // functions to add/remove wire and register connections
    void sensitize_module_to_wire(const WireBase* theWire) 
        { global.sensitize_to_wire(this, theWire); }
    void desensitize_module_to_wire(const WireBase* theWire)
        { global.desensitize_to_wire(this, theWire); }
    void sensitize_module_to_register(const RegisterBase* theRegister) 
        { global.sensitize_to_register(this, theRegister); }
    void desensitize_module_to_register(const RegisterBase* theRegister)
        { global.desensitize_to_register(this, theRegister); }

    // TODO: do we still want to emply a set here?
    // connect/disconnect output, connect output list
    inline void connect_output(const WireBase* output) const { static_cast<std::set<const WireBase*> >(output_list).insert(output); }
    inline void disconnect_output(const WireBase* output) const { static_cast<std::set<const WireBase*> >(output_list).erase(output); }

private:
    // Friend classes
    friend class WireBase;
    friend class RegisterBase;
    friend class vcd::writer;

    // This instances a global data structure (class statics)
    global_data_t global;

    // output list
    std::set<const WireBase*> output_list;

    // module parent; NULL => top level module.
    const Module* parent_module;

    // module instance name
    const std::string nm;

    // Wire instance registration/deregistration
    inline void add_wire_instance(const WireBase* wire)
        { wireInstanceDB.insert(this, wire); }
    inline void remove_wire_instance(const WireBase* wire)
        { wireInstanceDB.erase(this, wire); }

    // Schedule Modules dependent on a wire changing
    void schedule_wire_module_update(const WireBase* theWire) {
        UMM_pair_t<const WireBase*, const Module*> wire_module_range = wireConnectionDB.find_all_rl(theWire);
        for (UMM_iter_t<const WireBase*, const Module*> it = wire_module_range.first; it != wire_module_range.second; it++)
            runq.insert(it->second);
    }

    // Register instance registration/deregistration
    inline void add_register_instance(const RegisterBase* reg)
        { registerInstanceDB.insert(this, reg); }
    inline void remove_register_instance(const RegisterBase* reg)
        { registerInstanceDB.erase(this, reg); }

    // Schedule Modules dependent on a register changing
    void schedule_register_module_update(const RegisterBase* theRegister) {
        UMM_pair_t<const RegisterBase*, const Module*> register_module_range = registerConnectionDB.find_all_rl(theRegister);
        for (UMM_iter_t<const RegisterBase*, const Module*> it = module_register_range.first; it != module_register_range.second; it++)
            runq.insert(it->second);
    }

    // Register connection deregistration.
    void remove_register_sensitization(const RegisterBase* theRegister) {
        UMM_pair_t<const RegisterBase*, const Module*> register_module_range = registerConnectionDB.find_all_rl(theRegister);
        for (UMM_iter_t<const RegisterBase*, const Module*> it = register_module_range.first; it != register_module_range.second; it++)
            registerConnectionDB.erase(it->second, this);
    }

    // TODO: this is buggy: base class constructor called before subclass => there are no outputs when the constructor is called,
    // as the subclass constructor is what creates outputs. What we really want is the wire class for Output should auto add itsef to the 
    // grandparent => this constructor does nothing in that regard.
    // constructor common code
    void constructor_common() {
        this->connect_outputs_to_parent();
        if (parent_module) {
            // Relate parent to child (aka this).
            global.insert_parent_child(parent_module, this);

            // Sensitize parent to outputs of this module.
            for (std::set<const WireBase*>::const_iterator it = output_list.begin(); it != output_list.end(); it++)
                global.sensitize_to_wire(parent_module, *it);
        }
    }
};

 #endif //  _PV_MODULE_H_
