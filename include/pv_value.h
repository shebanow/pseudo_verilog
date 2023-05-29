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

 #ifndef _PV_VALUE_H_
 #define _PV_VALUE_H_

/*
 * Value change record for tracing.
 */

namespace pv {
    // This record records information about traced entities.
    struct ValueChangeRecord {
        char type;                              // variable type (U<nknown>, R<egister>, I<nput>, O<utput>, W<ire>, Q<Wire>)
        std::string start_value;                // value type had at start of clock
        std::string end_value;                  // value type had at end of clock
        bool is_changed;                        // did value change
        int NST;                                // # of times written but unchanged (static) this clock
        int NTR;                                // # of times changed (transitions) this clock
    };

    // This data structure records the maximum length of strings needed to represent value change records.
    // Two fields: the max length of any instance name ever traced and the max bit width of any such instance.
    struct ValueChangeRecordSizes {   
        int max_instance_name_len;              // max number of characters in any instance name
        int max_width;                          // maximum width
    };
} // end namespace pv

/*
 * The vcd::value2string_t template class is used to convert a data value to a Verilog VCD-printable string.
 * Note: users can extend the class through other partial specializations of the template class.
 * Typically required when generating value strings of custom data types.
 * The VCD namespace is used for the vcd::value2string_t class as this class's usage is related to dumping VCD files.
 */

namespace vcd {
    // value2string_base_t(const T& v): base class to print a type as required for a VCD file.
    // The value2string_t is a class to be used for converting a value of type T to a string for inclusion in a VCD. 
    class value2string_base_t {
    public:
        // Constructor takes bit width as argument.
        value2string_base_t(const int wv) : w(wv) {}
        value2string_base_t(const value2string_base_t& vb) = delete;

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

        // "w" is the saved width of the value printer (in bits).
        int w;
    };

    // Generic type implementation of value2string_t: this class can be specialized by a user as needed.
    template <typename T>
    struct value2string_t : public value2string_base_t {
        // Constructor: takes an argument which is some variable of the type to be printed.
        value2string_t(const T& v) : value2string_base_t(bitwidth<T>()) {}
        value2string_t(const value2string_t<T>& v) = delete;

        // Functor to print value as a string.
        std::string operator()(const T& v, const bool add_b_prefix = true) const {
            uint64_t iv = (uint64_t) v;
            return value2string((uint64_t) iv, add_b_prefix);
        }
    };

    // "bool" specialization of value2string_t
    template <>
    struct value2string_t<bool> : public value2string_base_t {
        value2string_t(const bool& v) : value2string_base_t(1) {}
        inline std::string operator()(const bool& v) const {
            std::string str = v ? "1" : "0";
            return str;
        }
    };

    // "float" specialization of value2string_t
    template <>
    struct value2string_t<float> : public value2string_base_t {
        value2string_t(const float& v) : value2string_base_t(32) {}
        std::string operator()(const float& v, const bool add_b_prefix = true) const {
            union { float r; uint32_t v; } uv;
            uv.r = v;
            return value2string((uint64_t) uv.v, add_b_prefix);
        }
    };

    // "double" specialization of value2string_t
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
     * width2index(): convert a bit width to an index string (i.e., "[msb:lsb]").
     */

    inline std::string width2index(const int w) {
        std::string str = "";
        if (w <= 1) return str;
        str += " [";
        str += std::to_string(w-1);
        str += ":0]";
        return str;
    }

} // end namespace vcd

 #endif //  _PV_VALUE_H_
