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
#include <algorithm>

 #ifndef _PV_VALUE2STRING_H_
 #define _PV_VALUE2STRING_H_

// enter VCD namespace
namespace vcd {

    /*
     * value2string_base_t(const T& v): general abstract functor class to print a type as required for a VCD file.
     *
     * The value2string_t is a class to be used for converting a value of type T to a string for 
     * inclusion in a VCD. 
     */

    class value2string_base_t {
    public:
        // Constructor takes bit width as argument
        value2string_base_t(const int wv) : w(wv) {}

        // Width setter/getter
        inline void set_width(const int wv) { w = wv; }
        inline const int get_width() const { return w; }

        // Method to return a string of all "x" values. This can be in the base class as we
        // do not need to know the type to generate the correct # of 'X' values.
        std::string undefined() const {
            std::string str = (w > 1) ? "b" : "";
            for (int i = 0; i < w; i++)
                str += "x";
            return str;
        }

    protected:
        // The value2string() method converts a uint64_t value at width "w" to a string.
        std::string value2string(uint64_t uv, const bool add_b_prefix = true) const {
            std::string str;
            for (int i = 0; i < w; i++) {
                str += (uv & 1ull) ? "1" : "0";
                uv >>= 1;
            }
            if (add_b_prefix && w > 1)
                str += "b";
            std::reverse(str.begin(), str.end());
            return str;
        }

        //"w" is the saved width of the value printer (in bits).
        int w;
    };

    /*
     * Generic type implementation of value2string_t: this class can be
     * specialized by a user as needed.
     */

    template <typename T>
    struct value2string_t : public value2string_base_t {
        // Constructor: takes an argument which is some variable of the type to be printed.
        value2string_t(const T& v) : value2string_base_t(bitwidth<T>()) {}

        // Functor to print value as a string.
        std::string operator()(const T& v, const bool add_b_prefix = true) const {
            uint64_t iv = (uint64_t) v;
            return value2string((uint64_t) iv, add_b_prefix);
        }
    };

    /*
     * "bool" specialization of value2string_t
     */

    template <>
    struct value2string_t<bool> : public value2string_base_t {
        value2string_t(const bool& v) : value2string_base_t(1) {}
        inline std::string operator()(const bool& v) const {
            std::string str = v ? "1" : "0";
            return str;
        }
    };

    /*
     * "float" specialization of value2string_t
     */

    template <>
    struct value2string_t<float> : public value2string_base_t {
        value2string_t(const float& v) : value2string_base_t(32) {}
        std::string operator()(const float& v, const bool add_b_prefix = true) const {
            union { float r; uint32_t v; } uv;
            uv.r = v;
            return value2string((uint64_t) uv.v, add_b_prefix);
        }
    };

    /*
     * "double" specialization of value2string_t
     */

    template <>
    struct value2string_t<double> : public value2string_base_t {
        value2string_t(const double& v) : value2string_base_t(64) {}
        std::string operator()(const double& v, const bool add_b_prefix = true) const {
            union { double r; uint64_t v; } uv;
            uv.r = v;
            return value2string(uv.v, add_b_prefix);
        }
    };

    /*
     * TODO: this needs to remain in fmod
     *
     * "fixed_point_t<M,N>" specialization of value2string_t

    template <int M, int N>
    struct value2string_t<fixed_point_t<M,N> > : public value2string_base_t {
        value2string_t(const fixed_point_t<M,N>& v) : value2string_base_t(M+N) {}
        std::string operator()(const fixed_point_t<M,N>& v, const bool add_b_prefix = true) const {
            std::string str;
            int64_t rv = v.raw_value();
            return value2string((uint64_t) rv, add_b_prefix);
        }
    };

     * "fixed_point_base_t" specialization of value2string_t

    template <>
    struct value2string_t<fixed_point_base_t> : public value2string_base_t {
        value2string_t(const fixed_point_base_t& v) : value2string_base_t(v.m() + v.n()) {}
        std::string operator()(const fixed_point_base_t& v, const bool add_b_prefix = true) const {
            std::string str;
            int64_t rv = v.raw_value();
            return value2string((uint64_t) rv, add_b_prefix);
        }
    };

     * "complex<T>" specialization of value2string_t

    template <typename T>
    struct value2string_t<complex_t<T> > : public value2string_base_t {
        value2string_t(const complex_t<T>& v) : value2string_base_t(bitwidth<T>() << 1) {}
        std::string operator()(const complex_t<T>& v, const bool add_b_prefix = true) const {
            std::string str = add_b_prefix ? "b" : "";
            value2string_t<T> re_functor(v.real);
            value2string_t<T> im_functor(v.imag);
            str += re_functor(v.real, false);
            str += im_functor(v.imag, false);
            return str;
        }
    };

     */

} // end namespace vcd

 #endif //  _PV_VALUE2STRING_H_
