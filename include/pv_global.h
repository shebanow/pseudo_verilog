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

#ifndef _PV_GLOBAL_H_
#define _PV_GLOBAL_H_

/*
 * The "global data" class is employed as a general database for otehr classes in the pseudo-verilog package.
 * As the pseudo-verilog package is header-only, this class plays a trick to instantiate static data within
 * a static function so as to get BSS data allocation to be done by the compiler instead of requiring source code
 * to do that instantiation (and thus require a compile and library (.so or equivalent)). 
 *
 * This header includes a number of typedefs to help with later usage of the header. Naming:
 * - For all but "pair" definitions, a prefix identifies the type of the field. Decoder ring:
 *      - "set" = std::set<T>
 *      - "umm" = std::unordered_multimap<K, V>
 *      - "um"  = std::unordered_map<K, V>
 * - After the prefix (or the prefix for pair definitions), the type of relation is encoded:
 *      - "m" => Module*
 *      - "w" => WireBase*
 *      - "r" => RegisterBase*
 *      - "mm" => Module* -> Module*
 *      - "mw" => Module* -> WireBase*
 *      - "mr" => Module* -> RegisterBase*
 *      - "wm" => WireBase* -> Module*
 *      - "rm" => RegisterBase* -> Module*
 * - The type of typedef is specified next:
 *      - "data_t" = base data type
 *      - "iter_t" = an iterator of the base data type
 *      - "data_pair_t" = a pair<T1, T2> of the base data types T1 and T2
 *      - "iter_pair_t" = a pair<T1, T2> of the iterators of base data types T1 and T2
 * As all types are pointers or iterators, this header can be included before actual definitions
 * of the Module, Wire, and Register classes.
 */

// Forward references
class Module;
class WireBase;
class RegisterBase;

// Data type declarations, set
typedef typename std::set<const Module*> set_m_data_t;
typedef typename std::set<const WireBase*> set_w_data_t;
typedef typename std::set<const RegisterBase*> set_r_data_t;

// Data type declarations, unordered multimaps
typedef typename std::unordered_multimap<const Module*, const Module*> umm_mm_data_t;
typedef typename std::unordered_multimap<const Module*, const WireBase*> umm_mw_data_t;
typedef typename std::unordered_multimap<const Module*, const RegisterBase*> umm_mr_data_t;
typedef typename std::unordered_multimap<const WireBase*, const Module*> umm_wm_data_t;
typedef typename std::unordered_multimap<const RegisterBase*, const Module*> umm_rm_data_t;

// Data type declarations, unordered maps
typedef typename std::unordered_map<const Module*, const Module*> um_mm_data_t;
typedef typename std::unordered_map<const WireBase*, const Module*> um_wm_data_t;
typedef typename std::unordered_map<const RegisterBase*, const Module*> um_rm_data_t;

// Iterator type declarations, set
typedef typename std::set<const Module*>::iterator set_m_iter_t;
typedef typename std::set<const WireBase*>::iterator set_w_iter_t;
typedef typename std::set<const RegisterBase*>::iterator set_r_iter_t;

// Iterator type declarations, unordered multimap
typedef typename std::unordered_multimap<const Module*, const Module*>::iterator umm_mm_iter_t;
typedef typename std::unordered_multimap<const Module*, const WireBase*>::iterator umm_mw_iter_t;
typedef typename std::unordered_multimap<const Module*, const RegisterBase*>::iterator umm_mr_iter_t;
typedef typename std::unordered_multimap<const WireBase*, const Module*>::iterator umm_wm_iter_t;
typedef typename std::unordered_multimap<const RegisterBase*, const Module*>::iterator umm_rm_iter_t;

// Iterator type declarations, unordered map
typedef typename std::unordered_map<const Module*, const Module*>::iterator um_mm_iter_t;
typedef typename std::unordered_map<const WireBase*, const Module*>::iterator um_wm_iter_t;
typedef typename std::unordered_map<const RegisterBase*, const Module*>::iterator um_rm_iter_t;

// Pair type declarations using structure pointers
typedef typename std::pair<const Module*, const Module*> mm_data_pair_t;
typedef typename std::pair<const Module*, const WireBase*> mw_data_pair_t;
typedef typename std::pair<const Module*, const RegisterBase*> mr_data_pair_t;
typedef typename std::pair<const WireBase*, const Module*> wm_data_pair_t;
typedef typename std::pair<const RegisterBase*, const Module*> rm_data_pair_t;

// Pair type declarations using iterators, unordered multimap
typedef typename std::pair<umm_mm_iter_t, umm_mm_iter_t> umm_mm_iter_pair_t;
typedef typename std::pair<umm_mw_iter_t, umm_mw_iter_t> umm_mw_iter_pair_t;
typedef typename std::pair<umm_mr_iter_t, umm_mr_iter_t> umm_mr_iter_pair_t;
typedef typename std::pair<umm_wm_iter_t, umm_wm_iter_t> umm_wm_iter_pair_t;
typedef typename std::pair<umm_rm_iter_t, umm_rm_iter_t> umm_rm_iter_pair_t;

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
        set_m_data_t runq;

        // lists of all wires and registers
        set_w_data_t wireList;
        set_r_data_t registerList;

        // These three databases record the instances of modules/wires/registers in parent modules.
        // Schema is 1:many.
        umm_mm_data_t instanceDB_parent_child;
        umm_mw_data_t instanceDB_module_wire;
        umm_mr_data_t instanceDB_module_register;

        // These three databases record the parent modules of modules/wires/registers.
        // Schema is 1:1.
        um_mm_data_t instanceDB_child_parent;
        um_wm_data_t instanceDB_wire_module;
        um_rm_data_t instanceDB_register_module;

        // These two database record connections between wires/registers to modules (sensistivity).
        // Schema is 1:many.
        umm_wm_data_t trigger_wire_module;
        umm_rm_data_t trigger_register_module;

        // These two database record connections back from modules to  wires/registers that can trigger them.
        // Schema is 1:many.
        umm_mw_data_t trigger_module_wire;
        umm_mr_data_t trigger_module_register;

        // Track # of wire/regs declared.
        uint32_t vcd_id_count;
    };

    // This global function instantiates the above global structure and returns a reference to it.
    static pv_global_static_t& data() {
        static pv_global_static_t theData;
        return theData;
    }

public:
    // Data structure getters.
    inline set_m_data_t& runq()		                        { return data().runq; }
    inline set_w_data_t& wireList()			                { return data().wireList; }
    inline set_r_data_t& registerList()                     { return data().registerList; }
    inline umm_mm_data_t& instanceDB_parent_child()			{ return data().instanceDB_parent_child; }
    inline umm_mw_data_t& instanceDB_module_wire()			{ return data().instanceDB_module_wire; };
    inline umm_mr_data_t& instanceDB_module_register()		{ return data().instanceDB_module_register; }
    inline um_mm_data_t&  instanceDB_child_parent()			{ return data().instanceDB_child_parent; }
    inline um_wm_data_t&  instanceDB_wire_module()			{ return data().instanceDB_wire_module; }
    inline um_rm_data_t&  instanceDB_register_module()		{ return data().instanceDB_register_module; }
    inline umm_wm_data_t& trigger_wire_module()			    { return data().trigger_wire_module; }
    inline umm_rm_data_t& trigger_register_module()			{ return data().trigger_register_module; }
    inline umm_mw_data_t& trigger_module_wire()			    { return data().trigger_module_wire; }
    inline umm_mr_data_t& trigger_module_register()			{ return data().trigger_module_register; }
    static uint32_t& vcd_id_count()			                { return data().vcd_id_count; }

    // Bidirectional map association methods.
    inline void associate_parent_child(const Module *p, const Module* c) {
        instanceDB_parent_child().insert(std::make_pair(p, c));
        instanceDB_child_parent().insert(std::make_pair(c, p));
    }
    inline void associate_module_wire(const Module *m, const WireBase* w) {
        instanceDB_module_wire().insert(std::make_pair(m, w));
        instanceDB_wire_module().insert(std::make_pair(w, m));
    }
    inline void associate_module_register(const Module *m, const RegisterBase* r) {
        instanceDB_module_register().insert(std::make_pair(m, r));
        instanceDB_register_module().insert(std::make_pair(r, m));
    }

    // Birectional sensitization methods.
    inline void sensitize_to_wire(const Module *m, const WireBase* w) {
        trigger_module_wire().insert(std::make_pair(m, w));
        trigger_wire_module().insert(std::make_pair(w, m));
    }
    inline void sensitize_to_register(const Module *m, const RegisterBase* r) {
        trigger_module_register().insert(std::make_pair(m, r));
        trigger_register_module().insert(std::make_pair(r, m));
    }

    // Bidirectional map specific dissociation methods.
    void dissociate_wire_in_module(const WireBase* w, const Module *m) {
        umm_mw_data_t& fw_map = instanceDB_module_wire();
        umm_mw_iter_pair_t fw_range = fw_map.equal_range(m);
        for (umm_mw_iter_t it = fw_range.first; it != fw_range.second; it++) {
            if (it->second == w) {
                fw_map.erase(it);
                break;
            }
        }
        instanceDB_wire_module().erase(w);
    }
    void dissociate_register_in_module(const RegisterBase* r, const Module *m) {
        umm_mr_data_t& fw_map = instanceDB_module_register();
        umm_mr_iter_pair_t fw_range = fw_map.equal_range(m);
        for (umm_mr_iter_t it = fw_range.first; it != fw_range.second; it++) {
            if (it->second == r) {
                fw_map.erase(it);
                break;
            }
        }
        instanceDB_register_module().erase(r);
    }

    // Bidirectional map complete dissociation methods.
    void dissociate_child_in_parent(const Module* p, const Module *c) {
        umm_mm_data_t& map = instanceDB_parent_child();
        umm_mm_iter_pair_t range = map.equal_range(p);
        for (umm_mm_iter_t it = range.first; it != range.second; it++) {
            if (it->second == c) {
                map.erase(it);
                break;
            }
        }
        instanceDB_child_parent().erase(c);
    }
    void dissociate_all_children_in_parent(const Module *p) {
        umm_mm_data_t& map = instanceDB_parent_child();
        umm_mm_iter_pair_t range = map.equal_range(p);
        for_each(range.first, range.second, [this](mm_data_pair_t it) { instanceDB_child_parent().erase(it.second); });
        map.erase(p);
    }
    void dissociate_all_wires_in_module(const Module *m) {
        umm_mw_data_t& map = instanceDB_module_wire();
        umm_mw_iter_pair_t range = map.equal_range(m);
        for_each(range.first, range.second, [this](mw_data_pair_t it) { instanceDB_wire_module().erase(it.second); });
        map.erase(m);
    }
    void dissociate_all_registers_in_module(const Module *m) {
        umm_mr_data_t& map = instanceDB_module_register();
        umm_mr_iter_pair_t range = map.equal_range(m);
        for_each(range.first, range.second, [this](mr_data_pair_t it) { instanceDB_register_module().erase(it.second); });
        map.erase(m);
    }

    // Bidirectional specific desensitization.
    void desensitize_to_wire(const Module *m, const WireBase* w) {
        umm_mw_data_t& mw_map = trigger_module_wire();
        umm_mw_iter_pair_t mw_range = mw_map.equal_range(m);
        for ( ; mw_range.first != mw_range.second; mw_range.first++) {
            if (mw_range.first->first == m && mw_range.first->second == w) {
                mw_map.erase(mw_range.first);
                break;
            }
        }
        umm_wm_data_t& wm_map = trigger_wire_module();
        umm_wm_iter_pair_t wm_range = wm_map.equal_range(w);
        for ( ; wm_range.first != wm_range.second; wm_range.first++) {
            if (wm_range.first->first == w && wm_range.first->second == m) {
                wm_map.erase(wm_range.first);
                break;
            }
        }
    }
    void desensitize_to_register(const Module *m, const RegisterBase* r) {
        umm_mr_data_t& mr_map = trigger_module_register();
        umm_mr_iter_pair_t mr_range = mr_map.equal_range(m);
        for ( ; mr_range.first != mr_range.second; mr_range.first++) {
            if (mr_range.first->first == m && mr_range.first->second == r) {
                mr_map.erase(mr_range.first);
                break;
            }
        }
        umm_rm_data_t& rm_map = trigger_register_module();
        umm_rm_iter_pair_t rm_range = rm_map.equal_range(r);
        for ( ; rm_range.first != rm_range.second; rm_range.first++) {
            if (rm_range.first->first == r && rm_range.first->second == m) {
                rm_map.erase(rm_range.first);
                break;
            }
        }
    }
    void desensitize_all_wires_to_module(const Module* m) {
        trigger_module_wire().erase(m);
        umm_wm_data_t& wm_map = trigger_wire_module();
        for (umm_wm_iter_t it = wm_map.begin(); it != wm_map.end(); it++) {
            if (it->second == m) {
                wm_map.erase(it);
                break;
            }
        }
    }
    void desensitize_all_registers_to_module(const Module* m) {
        trigger_module_register().erase(m);
        umm_rm_data_t& rm_map = trigger_register_module();
        for (umm_rm_iter_t it = rm_map.begin(); it != rm_map.end(); it++) {
            if (it->second == m) {
                rm_map.erase(it);
                break;
            }
        }
    }

    // Bidirectional complete desensitization.
    void desensitize_wire(const WireBase* w) {
        for (umm_mw_iter_t it = trigger_module_wire().begin(); it != trigger_module_wire().end(); it++) {
            if (it->second == w) {
                trigger_module_wire().erase(it);
                break;
            }
        }
        trigger_wire_module().erase(w);
    }
    void desensitize_register(const RegisterBase* r) {
        for (umm_mr_iter_t it = trigger_module_register().begin(); it != trigger_module_register().end(); it++) {
            if (it->second == r) {
                trigger_module_register().erase(it);
                break;
            }
        }
        trigger_register_module().erase(r);
    }
};

#endif // _PV_GLOBAL_H_
