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
#include <forward_list>
#include <unordered_map>
#include <algorithm>
#include <utility>

#ifndef _PV_GLOBAL_H_
#define _PV_GLOBAL_H_

/*
 * Forward references
 */

class Module;
class WireBase;
class RegisterBase;

// Data type declarations
typedef typename std::unordered_multimap<const Module*, const Module*> umm_mm_data_t;
typedef typename std::unordered_multimap<const Module*, const WireBase*> umm_mw_data_t;
typedef typename std::unordered_multimap<const Module*, const RegisterBase*> umm_mr_data_t;
typedef typename std::unordered_multimap<const WireBase*, const Module*> umm_wm_data_t;
typedef typename std::unordered_multimap<const RegisterBase*, const Module*> umm_rm_data_t;

// Iterator type declarations
typedef typename std::unordered_multimap<const Module*, const Module*>::iterator umm_mm_iter_t;
typedef typename std::unordered_multimap<const Module*, const WireBase*>::iterator umm_mw_iter_t;
typedef typename std::unordered_multimap<const Module*, const RegisterBase*>::iterator umm_mr_iter_t;
typedef typename std::unordered_multimap<const WireBase*, const Module*>::iterator umm_wm_iter_t;
typedef typename std::unordered_multimap<const RegisterBase*, const Module*>::iterator umm_rm_iter_t;

// Iterator type declarations
typedef typename std::pair<const Module*, const Module*> umm_mm_pair_t;
typedef typename std::pair<const Module*, const WireBase*> umm_mw_pair_t;
typedef typename std::pair<const Module*, const RegisterBase*> umm_mr_pair_t;
// typedef typename std::unordered_multimap<const WireBase*, const Module*>::iterator umm_wm_iter_t;
// typedef typename std::unordered_multimap<const RegisterBase*, const Module*>::iterator umm_rm_iter_t;

/*
 * The "static_data" class creates a global copy of a pv_global_t instance (common across all instances).
 * The class is usable in a header-only library. Usage is via the operator() overload which returns
 * a reference to the global pv_global_t instance. At that point, code can just reference the desired field.
 */

class global_data_t {
private:
    // Global data structure: contains all the data structures needed for global control of the simulator.
    struct pv_global_static_t {
        // The "run queue" (list of Modules needing evaluation)
        std::forward_list<const Module*> runq;

        // These three databases record the instances of modules/wires/registers in parent modules.
        // Schema is 1:many.
        std::unordered_multimap<const Module*, const Module*> instanceDB_parent_child;
        std::unordered_multimap<const Module*, const WireBase*> instanceDB_module_wire;
        std::unordered_multimap<const Module*, const RegisterBase*> instanceDB_module_register;

        // These three databases record the parent modules of modules/wires/registers.
        // Schema is 1:1.
        std::unordered_map<const Module*, const Module*> instanceDB_child_parent;
        std::unordered_map<const WireBase*, const Module*> instanceDB_wire_module;
        std::unordered_map<const RegisterBase*, const Module*> instanceDB_register_module;

        // These two database record connections between wires/registers to modules (sensistivity).
        // Schema is 1:many.
        std::unordered_multimap<const WireBase*, const Module*> trigger_wire_module;
        std::unordered_multimap<const RegisterBase*, const Module*> trigger_register_module;

        // These two database record connections back from modules to  wires/registers that can trigger them.
        // Schema is 1:many.
        std::unordered_multimap<const Module*, const WireBase*> trigger_module_wire;
        std::unordered_multimap<const Module*, const RegisterBase*> trigger_module_register;

        // Track # of wire/regs declared.
        uint32_t vcd_id_count;
    };

    // This global function instantiates the above global structure and returns a reference to it.
    static pv_global_static_t& data() {
        static pv_global_static_t theData;
        return theData;
    }

public:
    // Constructor (clears are redundant, but there for clarity)
    global_data_t() {
        printf("in global_data_t() constructor\n");
        pv_global_static_t& ds = data();
        ds.runq.clear();
        ds.instanceDB_parent_child.clear();
        ds.instanceDB_module_wire.clear();
        ds.instanceDB_module_register.clear();
        ds.instanceDB_child_parent.clear();
        ds.instanceDB_wire_module.clear();
        ds.instanceDB_register_module.clear();
        ds.trigger_wire_module.clear();
        ds.trigger_register_module.clear();
        ds.trigger_module_wire.clear();
        ds.trigger_module_register.clear();
        ds.vcd_id_count = 0;
    }

    // Data structure getters
    inline std::forward_list<const Module*>& runq()
        { return data().runq; }
    inline std::unordered_multimap<const Module*, const Module*>& instanceDB_parent_child() 
        { return data().instanceDB_parent_child; }
    inline std::unordered_multimap<const Module*, const WireBase*>& instanceDB_module_wire()
        { return data().instanceDB_module_wire; };
    inline std::unordered_multimap<const Module*, const RegisterBase*>& instanceDB_module_register()
        { return data().instanceDB_module_register; };
    inline std::unordered_map<const Module*, const Module*>& instanceDB_child_parent()
        { return data().instanceDB_child_parent; };
    inline std::unordered_map<const WireBase*, const Module*>& instanceDB_wire_module()
        { return data().instanceDB_wire_module; };
    inline std::unordered_map<const RegisterBase*, const Module*>& instanceDB_register_module()
        { return data().instanceDB_register_module; };
    inline std::unordered_multimap<const WireBase*, const Module*>& trigger_wire_module()
        { return data().trigger_wire_module; };
    inline std::unordered_multimap<const RegisterBase*, const Module*>& trigger_register_module()
        { return data().trigger_register_module; };
    inline std::unordered_multimap<const Module*, const WireBase*>& trigger_module_wire()
        { return data().trigger_module_wire; };
    inline std::unordered_multimap<const Module*, const RegisterBase*>& trigger_module_register()
        { return data().trigger_module_register; };
    inline uint32_t& vcd_id_count()
        { return data().vcd_id_count; }

    // Bidirectional map insertion methods
    inline void insert_parent_child(const Module *p, const Module* c) {
        instanceDB_parent_child().insert(std::make_pair(p, c));
        instanceDB_child_parent().insert(std::make_pair(c, p));
    }
    inline void insert_module_wire(const Module *m, const WireBase* w) {
        instanceDB_module_wire().insert(std::make_pair(m, w));
        instanceDB_wire_module().insert(std::make_pair(w, m));
    }
    inline void insert_module_register(const Module *m, const RegisterBase* r) {
        instanceDB_module_register().insert(std::make_pair(m, r));
        instanceDB_register_module().insert(std::make_pair(r, m));
    }
    inline void sensitize_to_wire(const Module *m, const WireBase* w) {
        trigger_module_wire().insert(std::make_pair(m, w));
        trigger_wire_module().insert(std::make_pair(w, m));
    }
    inline void sensitize_to_register(const Module *m, const RegisterBase* r) {
        trigger_module_register().insert(std::make_pair(m, r));
        trigger_register_module().insert(std::make_pair(r, m));
    }

    // Bidirectional map deletion methods
    void delete_all_children_in_parent(const Module *p) {
        umm_mm_data_t& map = instanceDB_parent_child();
        std::pair<umm_mm_iter_t, umm_mm_iter_t> range = map.equal_range(p);
        for_each(range.first, range.second, [this](umm_mm_pair_t it) { instanceDB_child_parent().erase(it.second); });
        map.erase(p);
    }
    void delete_all_wires_in_module(const Module *m) {
        umm_mw_data_t& map = instanceDB_module_wire();
        std::pair<umm_mw_iter_t, umm_mw_iter_t> range = map.equal_range(m);
        for_each(range.first, range.second, [this](umm_mw_pair_t it) { instanceDB_wire_module().erase(it.second); });
        map.erase(m);
    }
    void delete_all_registers_in_module(const Module *m) {
        umm_mr_data_t& map = instanceDB_module_register();
        std::pair<umm_mr_iter_t, umm_mr_iter_t> range = map.equal_range(m);
        for_each(range.first, range.second, [this](umm_mr_pair_t it) { instanceDB_register_module().erase(it.second); });
        map.erase(m);
    }
    void desensitize_to_wire(const Module *m, const WireBase* w) {
        // erase matching entry in forward map
        umm_mw_data_t& mw_map = trigger_module_wire();
        std::pair<umm_mw_iter_t, umm_mw_iter_t> mw_range = mw_map.equal_range(m);
        for ( ; mw_range.first != mw_range.second; mw_range.first++)
            if (mw_range.first->first == m && mw_range.first->second == w)
                mw_map.erase(mw_range.first);

        // erase matching entry in backward map
        umm_wm_data_t& wm_map = trigger_wire_module();
        std::pair<umm_wm_iter_t, umm_wm_iter_t> wm_range = wm_map.equal_range(w);
        for ( ; wm_range.first != wm_range.second; wm_range.first++)
            if (wm_range.first->first == w && wm_range.first->second == m)
                wm_map.erase(wm_range.first);
    }
    void desensitize_to_register(const Module *m, const RegisterBase* r) {
        // erase matching entry in forward map
        umm_mr_data_t& mr_map = trigger_module_register();
        std::pair<umm_mr_iter_t, umm_mr_iter_t> mr_range = mr_map.equal_range(m);
        for ( ; mr_range.first != mr_range.second; mr_range.first++)
            if (mr_range.first->first == m && mr_range.first->second == r)
                mr_map.erase(mr_range.first);

        // erase matching entry in backward map
        umm_rm_data_t& rm_map = trigger_register_module();
        std::pair<umm_rm_iter_t, umm_rm_iter_t> rm_range = rm_map.equal_range(r);
        for ( ; rm_range.first != rm_range.second; rm_range.first++)
            if (rm_range.first->first == r && rm_range.first->second == m)
                rm_map.erase(rm_range.first);
    }
};

#endif // _PV_GLOBAL_H_
