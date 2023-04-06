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
#include <ios>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <utility>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

#ifndef _PV_H_
#define _PV_H_

// Include subfile definitions
#include "pv_macros.h"          // defines "instance" macros
#include "pv_bitwidth.h"        // defines a bitwidth template class along with a templated function to compute bit width
#include "pv_value2string.h"    // defines a template class to convert values to VCD strings
#include "pv_module.h"          // defines "Module" superclass
#include "pv_wires.h"           // defines WireBase, WireTemplateBase superclasses; defines Wire, Input, and Output classes
#include "pv_register.h"        // defines RegisterBase superclass and templated Register class.
#include "pv_vcd.h"             // defines vcd::writer class, a VCD file writer
#include "pv_testbench.h"       // defines Testbench superclass, a special type of Module

#endif // _PV_H_
