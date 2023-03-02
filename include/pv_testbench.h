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

 #ifndef _PV_TESTBENCH_H_
 #define _PV_TESTBENCH_H_

/*
 * The Testbench class is used as a template for constructing testbenches to test modules in a model.
 * It includes support for directed random testing of I/O, and is similar in
 * concept to UVM testing with System Verilog. See https://verificationguide.com/uvm/uvm-testbench/. 
 *
 * As a Testbench is a subclass of Module, testbenches have an eval() function as well, called upon changes to outputs of
 * the DUT it is testing. Also like Module, a Testbench can comtain other simulatable (i.e., clockable) components and 
 * be sensitized accordingly. Testbenches differ from regular modules in that there is a main() function that accepts
 * command line arguments to be processed as well as pre_clock() and post_clock() functions. The main() function
 * initializes the testbench, processing remaining command line arguments. The pre_clock() and post_clock() functions
 * are called at the start and end of each clock cycle respectively.
 */

struct Testbench : public Module {
    // constructor simply calls superclass
    Testbench(const std::string& str) : Module(NULL, str) { in_tc = false; pass_count = fail_count = 0; }
    Testbench(const char* str) : Module(NULL, str) { in_tc = false; pass_count = fail_count = 0; }

    // method that must be overloaded to implement argument processing, other construction time init,
    // called after construction and before first eval.
    virtual void main(int argc, char** argv) = 0;

    // pre and post clock calls
    virtual void pre_clock(const uint32_t cycle_num) {}
    virtual void post_clock(const uint32_t cycle_num) {}

    // methods supporting test cases
    virtual void begin_test() { in_tc = true; }
    template<typename ... Args> void end_test_pass(const char* fmt, Args ... args) {
        printf(fmt, args ...);
        in_tc = false;
        pass_count++;
    }
    template<typename ... Args> void end_test_fail(const char* fmt, Args ... args) {
        printf(fmt, args ...);
        in_tc = false;
        fail_count++;
    }
    inline const bool in_test() const { return in_tc; }
    inline const int n_pass() const { return pass_count; }
    inline const int n_fail() const { return fail_count; }

private:
    bool in_tc;         // true if in a test case
    int pass_count;     // number of cases that passed
    int fail_count;     // number of cases that failed
};

 #endif //  _PV_TESTBENCH_H_
