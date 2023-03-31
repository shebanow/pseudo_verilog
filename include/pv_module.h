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
    // Module constructors: variants are based on whether instance name is pass as std::string or char*.
    Module(const Module* p, const std::string& str) : parent_module(p), nm(str) { constructor_common(); }
    Module(const Module* p, const char* str) : parent_module(p), nm(str) { constructor_common(); }

    // Module destructor. The destructor attempts to unwind all references to this module instance.
    virtual ~Module() {
        // If module has a parent, disconnect instance of this from that parent and desensitize outputs
        if (parent_module) {
            global.dissociate_child_in_parent(parent_module, this);
            for (std::set<const WireBase*>::const_iterator it = output_list.begin(); it != output_list.end(); it++)
                global.desensitize_to_wire(parent_module, *it);
        }

        // For all submodules instanced by this module, remove wire/register relationships.
        global.dissociate_all_children_in_parent(this);
        global.dissociate_all_wires_in_module(this);
        global.desensitize_all_wires_to_module(this);
        global.dissociate_all_registers_in_module(this);
        global.desensitize_all_registers_to_module(this);

        // Remove from RUNQ if there
        global.runq().erase(this);
    }

    // Required implementation: called to update the module upon change in its sensitivity lists (wires or registers). 
    virtual void eval() = 0;

    // Getters for type, name and instance name.
    inline const std::string name() const { return nm; }
    const std::string instanceName() const {
        if (!parent_module)
            return nm;
        else {
            std::string tmp = parent_module->instanceName() + "." + nm;
            return tmp;
        }
    }

    // Getter to return parent.
    inline const Module* parent() const { return parent_module; }

    // Methods to manually sensitize/desensitize this module to wire and register connections.
    void sensitize_module_to_wire(const WireBase* theWire) { global.sensitize_to_wire(this, theWire); }
    void desensitize_module_to_wire(const WireBase* theWire) { global.desensitize_to_wire(this, theWire); }
    void sensitize_module_to_register(const RegisterBase* theRegister) { global.sensitize_to_register(this, theRegister); }
    void desensitize_module_to_register(const RegisterBase* theRegister) { global.desensitize_to_register(this, theRegister); }

protected:
    // This instances a global data structure (class static).
    global_data_t global;

private:
    // Friend classes
    friend class WireBase;
    friend class RegisterBase;
    friend class vcd::writer;

    // Output list: list of Outputs in this module; need to auto-sensitize the parent of this module to these outputs.
    std::set<const WireBase*> output_list;

    // Module parent; NULL => top level module.
    const Module* parent_module;

    // Module instance name.
    const std::string nm;

    // Constructor common code.
    void constructor_common() {
        if (parent_module) {
            // Relate parent to child (aka this).
            global.associate_parent_child(parent_module, this);

            // Sensitize parent to outputs of this module.
            for (std::set<const WireBase*>::const_iterator it = output_list.begin(); it != output_list.end(); it++)
                global.sensitize_to_wire(parent_module, *it);
        }
    }
};

 #endif //  _PV_MODULE_H_
