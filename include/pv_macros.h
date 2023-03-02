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

#ifndef _PV_MACROS_H
#define _PV_MACROS_H

/*
 * Macros to help with instance declarations.
 * 
 * Three macros are provided to help with Wire/Register/Module instancing. One macro,
 * inst_no_init(), creates an instance with the provided name. The otehr macro,
 * inst_with_init(), is similar in that a name is provided, but in addition, an
 * initialization parameter is also provdied. A third macro, top_level(), is provided
 * to define a module at top level (no parent).
 *
 * The implementation of the macros is somewhat complex. 
 * See https://stackoverflow.com/questions/3046889/optional-parameters-with-c-macros.
 */
#define inst_no_init(inst_name)                 inst_name = {this, #inst_name}
#define inst_with_init(inst_name, init)         inst_name = {this, #inst_name, init}
#define top_level(inst_name)                    inst_name = {NULL, #inst_name}
#define inst_get_3rd_arg(arg1, arg2, arg3, ...) arg3
#define inst_chooser(...)                       inst_get_3rd_arg(__VA_ARGS__, inst_with_init, inst_no_init, )
#define instance(...)                           inst_chooser(__VA_ARGS__)(__VA_ARGS__)

#endif // _PV_MACROS_H
