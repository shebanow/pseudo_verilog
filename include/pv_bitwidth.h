
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

// enter VCD namespace
namespace vcd {

    // template class and template function used to determine a number of HW bits based on a type.
    template <typename T>
    extern int bitwidth();    // fwd declaration

    // generic type inference.
    template <typename T>
    struct _bitwidth {
        _bitwidth() : width(sizeof(T) << 3) {}
        const int width;
    }; 

    // T is bool
    template <>
    struct _bitwidth<bool> {
        _bitwidth() : width(1) {}
        const int width;
    };

/*
    TODO: should remain in fmod.

    // T is a type of fixed_point_base_t
    template <int M, int N>
    struct _bitwidth<fixed_point_t<M,N> > {
        _bitwidth() : width(M + N) {}
        const int width;
    };

    // T is a type of complex_t
    template <typename U>
    struct _bitwidth<complex_t<U> > {
        _bitwidth() : width(bitwidth<U>() << 1) {}
        const int width;
    };
*/

    // template function to return # of bits in a type through type inference
    template <typename T>
    inline int bitwidth() {
        _bitwidth<T> bw;
        return bw.width;
    }

} // end namespace vcd

 #endif //  _PV_BITWIDTH_H_
