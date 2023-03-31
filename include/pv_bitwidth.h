
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

 #ifndef _PV_BITWIDTH_H_
 #define _PV_BITWIDTH_H_

/*
 * The bitwidth class is used to determine the width in bits of a particular data type.
 * Note: users can extend the class through other partial specializations of the template base class.
 * Typically required when computing the bit width of custom data types.
 * The VCD namespace is used for the bitwidth class as usage is related to dumping VCD files.
 */

namespace vcd {

    // Template class and template function used to determine a number of HW bits based on a type.
    template <typename T>
    extern int bitwidth();    

    // Generic type inference.
    template <typename T>
    struct _bitwidth {
        _bitwidth() : width(sizeof(T) << 3) {}
        const int width;
    }; 

    // T is bool.
    template <>
    struct _bitwidth<bool> {
        _bitwidth() : width(1) {}
        const int width;
    };

    // Template function to return # of bits in a type through type inference
    template <typename T>
    inline int bitwidth() {
        _bitwidth<T> bw;
        return bw.width;
    }

} // end namespace vcd

 #endif //  _PV_BITWIDTH_H_
