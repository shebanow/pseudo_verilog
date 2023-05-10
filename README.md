# The Pseudo-Verilog Header-only Library

This header-only library implements a Verilog-like C++ simulation infrastructure for hardware design.
This library is an alternative to SystemC (https://systemc.org/overview/systemc/) with the 
intent to create a more Verilog-lookalike design.
Signals in this library are quasi three state (0, 1, and 'x'). 

# A Simple Example

As an introduction, we'll begin with a simple example.
The following is a 4-bit up counter with enable:

```cpp
class Counter final : public Module {
public:
    // Constructor
    Counter(const Module* parent, const char* name) : Module(parent, name) {}
    
    // Ports
    Input<bool> instance(reset_x);    // active low reset input
    Input<bool> instance(enable);     // active high counter enable
    Output<int, 4> instance(value);   // 4-bit output count
    
    // Per-clock-cycle evaluation function
    void eval() {
        // If reset is asserted...
        if (!reset_x) {
            value = 0;
            counter <= 0;
            return;
        }
        
        // Otherwise, if counting is enabled...
        if (enable)
            counter <= (counter + 1) & 0xF;
    }
    
private:
    Register<int, 4> instance(counter); // The actual counter register
};
```

Dissecting the components of this example, a ```Counter``` module class is declared:

```cpp
class Counter final : public Module {
public:
    // Constructor
    Counter(const Module* parent, const char* name) : Module(parent, name) {}
```

The ```Counter``` class is a type of ```Module```. 
To that end, the heavy lifting in the constructor is actually
performed by the ```Module``` class - the body of the ```Counter``` constructor is blank. 
Two parameters are passed to the ```Counter``` constructor, a pointer to the "parent" module and an instance name.
The parent module is a module that contains the instance of the ```Counter``` class being constructed.
(A ```NULL``` parent module implies that the module is "top level" - this is usually reserved for
```Testbench``` instances.)

Next, the I/O ports of the class are declared:

```cpp
    // Ports
    Input<bool> instance(reset_x);    // active low reset input
    Input<bool> instance(enable);     // active high counter enable
    Output<int, 4> instance(value);   // 4-bit output count
```

In this case, there are two inputs and one output. 
The inputs are declared as type ```bool``` and by definition are single bit wires.
The solitary output is declared to be type ```int```, but the width is specified to be 4 bits.
(Note that the actual type is still ```int``` and all C++ operators will perform to the full range of ```int```;
the width only applies when signals are dumped to a VCD file.)

Skipping the implementation for now, a single register is declared:

```cpp
private:
    Register<int, 4> instance(counter); // The actual counter register
```
Like the output, this register is of type ```int``` with a width of 4 bits.
This register is declared private as it is internal to the implementation of the counter.

Finally, we get to the guts of the counter, its implementation:

```cpp
    // Per-clock-cycle evaluation function
    void eval() {
        // If reset is asserted...
        if (!reset_x) {
            value = 0;
            counter <= 0;
            return;
        }
        
        // Otherwise, if counting is enabled...
        if (enable)
            counter <= (counter + 1) & 0xF;
    }
```

The```eval()``` function can be thought of as the body of a Verilog ```always @()``` statement,
called whenever any input to the module is changed or if the register changes state.
At the head of the ```eval()``` function is a test of the input reset signal.
If reset is asserted (active low), the output port and register are driven to a known value.
The function then returns since there is nothing else to do that clock.
If reset is not asserted and enable is asserted, the counter is incremented (and qualified to stay within a 4-bit range).
This completes the implementation of the ```Counter``` class.

# Detailed Description

Seven primary classes are defined to help simulate hardware behavior:
* Module - container for implementation modules (aka, blocks).
* Wire, QWire, Input, & Output - containers for simulating ports and wires (akin to Verilog ports and wires). 
* Register - container for simulating registers (flip flops) (akin to Verilog "reg").
* Testbench - a type of Module employed to test implementations.

In addition, there are four other classes/functions that support the library:
* ```vcd::bitwidth<T>```: a class used to infer or specify the bit width of a type.
* ```int vcd::bitidth<T>()```: related function to return that bit width.
* ```std::string vcd::value2string_t<T>```: function to return a VCD-style string of some value.
* ```vcd::writer```: a class used to write VCD files.

## The ```Module``` Class

The ```Module``` class is a container intended to be subclassed by an actual implementation
of a module.
Any subclass of ```Module``` must define two or three methods:

* A class constructor of either or both forms:
  * ```Module(const Module* parent, const std::string& name)```
  * ```Module(const Module* parent, const char* str)```
* ```void eval()```: a module evaluation function.

The constructor specifies both a parent module and an instance name
if the module has no parent (i.e., a top level module), the parent module can be NULL.
Together, they help form an instance name (a path). 
The constructor should call the ```Module``` constructor passing the specified parent and instance name
to the superclass.
Within the body of the subclass constructor, any one-time specific module construction can take place.
Register, port, or wire initialization should **not** be done within the constructor.

The ```eval()``` function is called whenever:

* Any ```Register<>``` instanced within the module changes state, or
* Any ```Wire<>``` instanced within the module changes value, or
* Any ```Input<>``` instanced within the module changes value, or
* Any ```Output<>``` instanced within a **direct child** of the module changes value.

The last comment about ```Output<>``` ports is very important to understand. 
**Neither** ```Input<>``` nor ```Output<>``` ports can be instanced in a top level module (i.e., a module 
whose parent is ```NULL```). In addition, ```Output<>``` ports *sensitize* their grandparent,
not the parent module ("sensitize" meaning evaluate on change). 

In general, any ports within a subclass definition should be declared as ```public``` while
wires and registers should be declared as ```private```. Ports are visible to users of a subclass
while registers and wires are related to its implementation.

To help with declaring instances within a module (for example, wires, registers or other submodules), 
a macro is defined to make that easier (and cleaner to read). For example:
```cpp
  Wire<type> instance(myWire);
```
For non-module instances, the ```instance()``` macro can also specify an optional initialization value.
For example:
```cpp
   Register<type> instance(myReg, 0);
```
If an initialization value is provided, this value is "remembered" and a function ```reset_to_init_state()```
defined in the ```Testbench``` class can be used to restore this initialization value to all defined
registers and wire/port instances.

If required, a module can also declare a public ```init()``` method. There is no formal interface for ```init()``` - modules can define
whatever API they want. The intent of defining an ```init()``` method is to allow a module to initialize itself prior to simulation start,
but after all other modules are declared and instanced. In general, such methods should be declared as ```void```.

The ```Module``` class provides some additional methods that can assist in implementing subclasses. 
Methods to return the direct parent of an instance along with a method
to return the outermost grandparent of an instance:
```cpp
     const Module* parent() 
     const Module* top() 
```

The class also includes methods to return the module's name, full instance name, and module type:
```cpp
  const std::string name() const;
  const std::string instanceName() const;
```
The instance name will be a full "dotted" path name (e.g., ```top.next.name```); if a module is top level, the instance name and the module
name will be identical.

## The Signal Classes

There are four types of signals:

```cpp
    template <typename T, int W =-1> Input;
    template <typename T, int W =-1> Output;
    template <typename T, int W =-1> Wire;
    template <typename T, int W =-1> QWire;
```
In all cases, the parameter T specifies the type of the signal while the optional parameter W represents 
its bit width (the default value of -1 indicates that the library should infer with using T).
Note that post instancing of a signal, you can still set or get the bit width of a signal using these methods:
```cpp
    void set_width(const int width);
    int get_width();
```
As indicated before, width actually has no effect on operations upon the base class of a signal.
Width only is employed when dumping to VCD files.

Signals must be instanced within a module.
The ```Input``` and ```Output``` classes represent I/O ports.
As already mentioned, neither type can be instanced in a top level module (a module without a parent).
The ```Wire``` class is intended to be used internal to a module.
The ```QWire``` class is identical to the ```Wire``` class except that changes in value do not
trigger evaluation (```eval()```) of the containing module. The ```QWire``` class makes for an
ideal "probe" signal (much like a scope probe in a physical electrical circuit). 

Like the ```Module``` class, the signal classes also define methods to return the direct parent 
of an instance along with a method to return the outermost grandparent of an instance:
```cpp
     const Module* parent() 
     const Module* top() 
```

All signals have two possible states. Under normal conditions, a signal simply has the value range its base 
type allows. This value can be read usually using the signal as an ```rvalue``` in an expression.
However, you can force a read on this value by casting the signal to it's base type (i.e., ```(T) sig```).
A signal can also be in the ```X``` state as well. Three methods are defined to access a signal's
```X``` status:
```cpp
    bool value_is_x();
    bool value_was_x();
    void assign_x();
```
The first method ```value_is_x()``` returns true if the signal is current ```X```.
The second method ```value_was_x()``` returns true if the signal was ```X``` at the start of a clock cycle.
The third method ```assign_x()``` assigns an ```X``` value to the signal; it is treated
as an assignment to the signal (see below).
Note that a signal being in the ```X``` state has no effect on the value of the signal's base type. 
That is, you can still read or assign to that value (whether it is valid or not). Thus, for logic
that cares about this, it is important that code first test for ```X``` and then qualify any reads
accordingly. Also note that writing a value to a signal (via ```operator=()``` - see below) wipes
out any ```X``` state.

Reading a signal has no effect on its state. Writing to a signal can be made through assignment, for example:
```cpp
  x = 3;
```
If the signal had any other value before other than 3, post assignment, any 
modules connected to ```x``` will be reevaluated. 
Note that there is a danger here though with structure/class types: assignments to fields 
within a wire are not detected by the class. For example:
```cpp
struct example {
    int f1;
    int f2;
};
Wire<example> ex;

void eval() {
    ex.f1 = 3;
}
```
In this example, ```f1``` is a field of the structure ```example```; assigning to this field
uses normal C++ semantics, not the overloaded ```operator=()``` contained with the ```Wire<>```
class definition.
So, it is better to declare wires with the structure like this:
```cpp
struct example {
    Wire<int> f1;
    Wire<int> f2;
};
example ex;

void eval() {
    ex.f1 = 3;
}
```
This will ensure that the ```Wire<>``` class implementation will evaluate
any connected modules upon a wire state change.

The following is a list of assignment operators that are overloaded by the signal classes:
* ```operator=()```: simple assignment.
* ```operator+=()```: add-assign.
* ```operator-=()```: subtract-assign.
* ```operator*=()```: multiply-assign.
* ```operator/=()```: divide-assign.
* ```operator%=()```: modulo-assign.
* ```operator^=()```: bitwise XOR-assign.
* ```operator&=()```: bitwise AND-assign.
* ```operator|=()```: bitwise OR-assign.
* ```operator<<=()```: shift-left-assign.
* ```operator>>=()```: shift-right-assign.

In addition, both pre- and post- auto-increment (```++```) and auto-decrement (```--```) operators
are also considered as assignments.
Assigning or auto-incrementing/auto-decrementing to a signal class-type will 
trigger ```eval()``` calls per the rules defined above. 

Identical to the ```Module``` class, there are also a number of getters related to naming:
```cpp
  const std::string name() const;
  const std::string instanceName() const;
```

## The ```Register<>``` Template Class

The ```Register<typename T, int W =-1>``` template class defines clocked registers.
Registers must be instanced within a module.
In all cases, the parameter T specifies the type of the signal while the optional parameter W represents 
its bit width (the default value of -1 indicates that the library should infer with using T).
Unlike signals, it is perfectly ok to use with structure/class types as the base type for a register.
Note that post instancing of a register, as with signals,
you can still set or get the bit width of a register using these methods:
```cpp
    void set_width(const int width);
    int get_width();
```
Like signals, width actually has no effect on operations upon the base class of a register.
Width only is employed when dumping to VCD files.

Registers employ "source-replica" stages (more PC than "master-slave). The source stage is front end and
the replica stage is back end. Upon a clock edge, the source is copied to the replica. 
When writing a register, the source is updated. When reading from a register, the replica is provided.
Direct access to both the replica and source re provided through a pair of methods:
```cpp
    T& d();
    T& q();
```
The first method ```d()``` returns a reference to the source.
The second method ```q()``` returns a reference to the replica.

Clock itself is implied and under control of a ```Testbench``` instance (explained below).
This behavior mimics a clocked D flipflop with width equal to the base type ```T```.
Registers are not separately controlled by any enable; such behavior if desired would need to be explicitly coded.

Like the signal classes, the ```Register<>``` class also define methods to return the direct parent 
of an instance along with a method to return the outermost grandparent of an instance:
```cpp
     const Module* parent() 
     const Module* top() 
```

Like the signals classes, all registers have two possible states. 
Under normal conditions, a register simply has the value range its base 
type allows. This replica stage can be read using the signal as an ```rvalue``` in an expression.
However, you can force a read on this stage by casting the register to it's base type (i.e., ```(T) reg```).
A replica stage can also be in the ```X``` state as well. Two methods are defined to access a replica's
```X``` status:
```cpp
    bool value_is_x();
    bool value_will_be_x();
```
The first method ```value_is_x()``` returns true if the signal is current ```X```.
The second method ```value_will_be_x()``` returns true if the signal wil be an ```X``` after the next
clock edge.

Two other methods can be used to write an ```X``` to the source or both source and replica stages:
```cpp
    void assign_x();
    void reset_to_x();
```
The first method ```assign_x()``` just sets the source stage to ```X```.
The second method ```reset_to_x()```sets both the source and replica stages to ```X```.
Note that a register being in the ```X``` state has no effect on the 
value of the register's source or replica base type. 
That is, you can still read the replica or assign to the source value (whether it is valid or not). Thus, for logic
that cares about this, it is important that code first test for ```X``` and then qualify any reads
accordingly. Also note that writing a value to a signal (via ```operator=()``` - see below) wipes
out any ```X``` state.

To update a register's value, the ```operator<=()``` is overloaded.
This mimics Verilog's non-blocking assignment operator.
Assignments to registers thus take the for as in the following example:
```cpp
    Register<int> instance(x);
    void eval() {
        x <= 2;
    }
```
In this case, ```x``` is updated to the value 2 after the rising edge of clock.

Unlike the signal classes, the direct assignment operator (```operator=()```) and
all operator-assign operators (e.g., ```operator+=()```) are disabled for registers.
In addition, both pre- and post- auto-increment (```++```) and auto-decrement (```--```) operators
are also disabled.

Identical to the ```Module``` and signal classes, there are also a number of getters related to naming:
```cpp
  const std::string name() const;
  const std::string instanceName() const;
```

## The ```Testbench``` Class

The ```Testbench``` class is a specialized subclass of ```Module```. 
As a subclass, ```Testbench``` has all the public and protected features of ```Module``` including
the all important ```eval()``` function. 
In general, applications should not directly instance a ```Testbench``` object but instead should
subclass ```Testbench``` and instance that subclass.
Assuming a ```Testbench``` subclass instance is a top level module
(i.e., does not have a parent module), it can instance both ```Wire<>```,
```QWire<>```, and ```Register<>``` class fields, but cannot instance ```Input<>``` nor ```Output<>```
ports. Similarly, top-level subclass instances of a ```Module``` class can also be included as class members.

In addition to features of a ```Module```, subclasses of ```Testbench``` must also implement a ```main()```
method:
```cpp
    void main(int argc, char** argv);
```
Like the master implementation of ```main()```, a ```Testbench``` subclass ```main()``` is also passed program
command line arguments. However, these arguments will typically have command arguments pertinent to the main
program stripped before being called. Having command line arguments passed to a ```Testbench``` subclass
allows that subclass to configure itself prior to simulation.

Within ```main()```, after processing command line arguments, the method should perform a simulation of the
hardware system it intends to model. To that end, the ```Testbench``` subclass defines a ```simulation()```
method to simulate its system:
```cpp
    int simulation(const bool continue_clock_sequence = false);
```
This method will run a simulation until some kind of stop condition arises.
Upon exit, a return code is returned (see below on ```end_simulation()```).
The one argument to this method, ```continue_clock_sequence```, controls whether this is a new
simulation sequence or a continuation of a last simulation call. By default, a new simulation is assumed.
(For simple models, one simulation is perhaps enough. For more complex models which have multiple
test cases, it is perhaps easier to call ```simulation()``` once per test case, and as such each 
successive call is a clock count-wise continuation of the last simulation.) To assist with running
multiple simulations, there is a method to restore initial state:
```cpp
    void reset_to_instance_state();
```
This method will restore all signals and registers to the state they had when they were instanced.

The simplified algorithm implemented by ```simulation()``` is shown below (without VCD-related code):
```cpp
void simulation(const bool continue_clock_sequence = false) {
    // Clock, idle, and iteration counters.
    static uint32_t clock_num;
    uint32_t idle_cycles = 0;
    uint32_t iteration_count = 0;
    
    // Variables controlling simulation.
    extern uint32_t opt_cycle_limit;
    extern uint32_t opt_idle_limit;
    extern uint32_t opt_iteration_limit;
    extern bool exit_simulation;
    
    // The next call enqueues *all* instanced module's eval() code on the "eval_queue"
    enqueue_all_modules();
    
    // If we are not continuing a simulation from a prior one, set clock number back to 0.
    if (!continue_clock_sequence)
        clock_num = 0;
        
    // Main simulation loop: ends when exit_simulation is set OR we hit a clock cycle limit.
    do {
        // Increment clock number to the next numbered cycle.
        clock_num++;
        
        // "pre_clock(...)" is an optional user-defined method called before any change activity occurs.
        this->pre_clock(clock_num);
        
        // Advance all flops via a posedge clock
        pos_edge_clock();
        
        // We now check to see if we have hit an idle limit (if there is one).
        if (opt_idle_limit > 0 && eval_queue.empty() && ++idle_cycles == opt_idle_limit)
            throw std::runtime_error(...);
            
        // Evaluation loop: run until no more evaluations are scheduled.
        while (!eval_queue.empty()) {
            // Clear idle cycle count as we are not idle.
            idle_cycles = 0;
            
            // If there is a limit on iteration count, check it.
            if (opt_iteration_limit > 0 && iteration_count++ == opt_iteration_limit)
                throw std::runtime_error(...);
                
            // Make a copy of the evaluation queue, then clear the evaluation queue.
            eval_queue_copy = eval_queue; 
            eval_queue.clear();
            
            // Call "eval()" for every module enqueued on the copy of the evaluation queue.
            // This could cause new eval() calls to enqueue in the evaluation queue.
            for (Module* m = eval_queue.pop(); m != NULL; m = eval_queue.pop())
                m->eval();
        }
        
        // Clear iteration count for next loop.
        iteration_count = 0;
        
        // "post_clock(...)" is an optional user-defined method called after any change activity occurs.
        this->post_clock(clock_num);
        
        // If end of simulation requested, exit loop.
        if (exit_simulation)
            break;
    } while (opt_cycle_limit <= 0 || clock_num < opt_cycle_limit);
}
```
The code above includes inline comments to explain its behavior. The boolean ```exit_simulation```
can be set within module ```eval()``` code as they are being evaluated to force simulation exit.
There is a method to return the current clock cycle number in case this is needed:
```cpp
    const uint32_t get_clock();
```

As mentioned in the simplified code above, there are variables that control ```simulation()```.
Methods to set these variables are:
```cpp
    void set_vcd_writer(const vcd::writer* w);
    void set_idle_limit(const int32_t idle_limit);
    void set_cycle_limit(const int32_t cycle_limit);
    void set_iteration_limit(const int32_t iteration_limit);
 ```
The first method ```set_vcd_writer()``` installs a VCD dump writer (an instance of the ```vcd::writer``` class
defined later in this document). If not installed, no VCD file will be dumped (and VCD dumps can in fact be 
later turned off by calling this with a ```NULL``` argument). By default, there is no writer.

The second method ```set_idle_limit()``` sets a limit on the number of cycles the simulation can be idle
before throwing an error. This is the number of cycles in which no ```eval()``` was invoked for any module
instance in the hierarchy of a ```Testbench``` subclass instance. By default, there is no limit.

The third method ```set_cycle_limit()``` sets a cycle limit upon the simulation. That is, the maximum
number of clock cycles to run before quitting the simulation. (This limit applies across all ```simulation()```
calls even if they include a ```continue_clock_sequence``` condition.) By default, there is no inherent limit.

The fourth method ```set_iteration_limit()``` sets an upper bound on the number of iterations of evaluation
cycles within any one clock cycle. Recall that changes to signals or registers cause evaluations of their
instancing modules or grand-modules (for ```Output<>``` instances). Each clock, any pending evaluations
are executed. If there are no subsequent evaluations generated as a result of these evaluations, iterations
stop and simulation proceeds to the next clock. However, if ether are new evaluations, these are treated
as a new iteration. If logic is mis-designed, these iterations in theory could go on forever. 
(Consider a ring oscillator spread across multiple modules.)
The iteration limit restricts the number of such iterations so as to not allow an infinite loop.
By default, there is no limit.

Mirroring the setters defined above, there are getters to allow applications to access these parameters:
```cpp
    vcd::writer* get_vcd_writer();
    int32_t get_idle_limit();
    int32_t get_cycle_limit();
    int32_t get_iteration_limit();
```

Normally, a simulation would be restricted to run some finite number of clocks controlled via
```set_cycle_limit()```. However, an alternative is to have the simulated design via its testbench
end the simulation. A specific method is provided for this purpose:
```cpp
    template<typename ... Args> void end_simulation(const int code, const char* fmt, Args ... args);
```
This command will end a simulation at the end of the current clock cycle (after ```post_clock(...)``` 
is completed). The arguments to the method begin with a return ```code``` for the ```simulation()``` call.
Normal completion is signified using a return code of 0; abnormal completions can be signified with
any non-zero value, a user defined value. Note that the ```end_simulation()``` should not be called
by any ```eval()``` method as these methods can be speculatively called in some sense.

In addition to the return code, an optional return string can
be generated as well (with the same syntax as ```printf()```). If this is not desired, leave the ```fmt```
argument ```NULL```. Upon exit, ```simulation()``` returns the aforementioned exit code. And if 
```end_simulation()``` was never called, a return code of zero will be returned. If desired, the
exit string can be accessed via:
```cpp
    const std::string& error_string();
```

# VCD Generation

The library can be used to generate Verilog change dump (VCD) files.
In the ```vcd``` namespace, the ```writer``` class is defined to create a VCD writer object.
The constructor for a writer is defined as:
```cpp
    writer(const std::string& file_name);
```
It takes one argument, the name of the VCD file to create.
The resultant object created represents an open VCD file to be dumped to.
This file will be open until the writer object is destructed.
Note that if an error results in attempting to open the VCD file, 
an error diagnostic will be printed to stderr and the file will not be opened.
Applications can test for this using the ```is_open()``` method:
```cpp
    bool is_open();
```

To use the writer object, it must be bound to a ```Testbench``` subclass instance **before** the
```simulation()``` method in that class is invoked. As mentioned, this can be done via the 
```set_vcd_writer()``` method of the ```Testbench``` subclass.

Normally, once bound, the writer will dump all changes for all simulated clock cycles.
In some cases, this could result in very large VCD files.
To restrict the range of dumps, two methods are defined in the writer class:
````cpp
    void set_vcd_start_clock(const int32_t v);
    void set_vcd_stop_clock(const int32_t v);
````
They set the first and last clock to perform dumps. Either or both can be set. A negative value
passed to either disables their function. Two getters are defined to return their current values:
```cpp
  const int32_t get_vcd_start_clock();
  const int32_t get_vcd_stop_clock();
```

Normally, in a VCD file, the string ```*@``` is used as an ID for the ```clk``` (clock) signal implicitly defined.
This can be overridden using a method:
```cpp
    void set_vcd_clock_ID(const std::string id);
```
There is a corresponding getter to return the current string.
```cpp
    const std::string& get_vcd_clock_ID();
```
A caution: for normal variables, the writer class uses a prefix ```@``` followed by a hex code to label
variables. This cannot be changed.

VCD files require both a timescale and associated units value. Two enumerator classes are defined for this 
purpose.
```cpp
    // Enum class for timescale.
    enum class TS_time {
        t1 = 0,         // == 1
        t10,            // == 10
        t100            // == 100
    };

    // Enum class for time units.
    enum class TS_unit {
        s = 0,          // seconds  (10^0)
        ms,             // msec     (10^-3)
        us,             // usec     (10^-6)
        ns,             // nsec     (10^-9)
        ps,             // psec     (10^-12)
        fs              // fsec     (10^-15)
    };
```
A control method is used to set a simulation operating point in the writer:
```cpp
    void set_operating_point(const float freq, const TS_time time = TS_time::t1, const TS_unit unit = TS_unit::ns);
```
The first parameter ```freq``` defines the simulated frequency.
The ```time``` and ```unit``` parameters then define the timescale and unit for the VCD file.
The method then computes "ticks per clock" based on these parameters (the number of Verilog time
ticks for each clock cycle).
By default, unless set by this method, the VCD file assumes a frequency of 1 Hz and a timescale string of "1 s".
Four getter methods are defined to return these parameters:
```cpp
    float get_clock_freq();
    float get_timescale();
    uint64_t get_ticks_per_clock();
    const std::string& get_time_str();
```

# Programming Recommendations

The pseudo-verilog library is designed with the intent that there be one ```Testbench``` subclass instance
active at any one time. Although it is possible to have more than one, each will have its own ```simulation()```
method along with the implied clock it defines, and clocks between simulation modules will not be synchronized.
Furthermore, we have not tested what would happen if VCD dumps are enabled and the same ```vcd::writer``` instance
was bound to all instances of the ```Testbench``` subclasses - probably not good.

In coding a ```Testbench``` subclass, all testing work should be driven by the ```main(...)``` method.
The ```eval()``` method of the subclass is really intended for logic that is employed to drive the *device under
test* (DUT). The DUT is the real object of desire. In general, DUT should be a single instance (representing
a chip or IP block), but in theory a ```Testbench``` subclass could implement multiple submodule instances
and connect them within the ```eval()``` method.

To invoke simulation within ```main(...)```, we recommend guarding the ```simulation()``` call in a try-catch block:
```cpp
    try {
        this->simulation();
    } catch (const std::exception& e) {
        std::cerr << "Caught system error: " << e.what() << std::endl;
        exit(1);
    }
```
The simulation call can throw errors if idle cycles are exceeded or an iteration limit is reached. We also 
recommend preconfiguring simulation controls via these calls:
* ```void set_idle_limit(const int32_t idle_limit)```: set a limit on the number of idle cycles. Unless you are
very sure that deadlocks are impossible, we recommend setting some limit here. 
* ```void set_iteration_limit(const int32_t iteration_limit)```: set a limit on the number of iterations.
By default, iteration limit is set to 10; applications could ignore changing this parameter if desired.

As mentioned in the detailed description, it is possible to invoke ```simulation(...)``` multiple times, continuing
clock numbering across the calls.
A typical use pattern would look like this:
```cpp
    for (int test_case = 0; test_case < num_cases; test_case++) {
        reset_to_instance_state();
        ... set up test case ...
        try {
            this->simulation(test_case != 0);
        } catch (const std::exception& e) {
            std::cerr << "Caught system error: " << e.what() << std::endl;
            exit(1);
        }      
    }
```
Two facets of this code are very important to note:
* The call to ```reset_to_instance_state()```: this resets the simulator state back to what it was
when the objects in the simulator were instanced. Without this call, simulations after the first call
could be using a prior call's state with the unintended consequences that could follow from that.
* Passing the test in the call to ```simulation(...)``` (i.e., "```test_case != 0```"): this ensures 
that clock numbering continues from the last call except for the very first call. 

As also mentioned in the detailed description, in addition to per cycle change-driven ```eval()``` calls, the
Testbench class also allows its subclasses to optionally define either or both ```pre_clock(...)``` and/or
```post_clock(...)``` methods. If defined, these methods will be unconditionally called at the start and end
of every clock cycle, with the current simulation clock cycle number passed in as an argument. 
The only real difference between the two methods is the value of that clock cycle number relative to when
actual simulation occurs (before or after any change-drive ```eval()``` calls). 

A good potential use for the ```pre_clock(...)``` method is to conditionally activate some condition based on 
clock cycle number. For example, if some software-driven external event is to occur at clock cycle "key_event", 
pre_clock could be coded as:
```cpp
    void pre_clock(const uint32_t cycle_num) {
        extern uint32_t key_event_clock;
        
        if (cycle_num == key_event_clock) {
            ... do something ...
        }
    }
```
Similarly, the ```post_clock(...)``` method can be used to react to events within simulation at some clock
number. Or, it can be used unconditionally at every clock to detect events within simulation. The use of 
```pre_clock(...)``` and ```post_clock(...)``` methods is a good way to tie external software to a specific
timeline in the simulation. Finally, we recommend that ```post_clock(...)``` be used to end 
simulation based on simulator state as evaluation at that point is unconditional.
We **do not** recommend exiting simulation from within an ```eval()``` call as 
conditions in the simulator can change multiple times within a clock. For example, if error checking is done
within ```eval()``` and exit is called, the error could be transient, there for one ```eval()``` and not there for the
next. Calling for simulation exit is unconditional however and cannot be undone once called. 

# Example: A Traffic Light Controller (TLC)

We provide an example ```Module``` using the classic "traffic light controller" (TLC) usually 
taught in Logic Design 101 classes.

```cpp
/*
 * Sample pseudo-verilog file - TLC: traffic light controller.
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
#include <iostream>
#include <cstdint>
#include <string>
#include <getopt.h>
#include "pv.h"

enum color {
    red = 0,
    yellow = 1,
    green = 2
};

const char* color2str(const color c) {
    if (c == red) return "red";
    else if (c == yellow) return "yellow";
    else return "green";
}

class tlc : public Module {
public:
    // Constructors.
    tlc(const Module* p, const char* str) : Module(p, str) {}
    tlc(const Module* p, const std::string& nm) : Module(p, nm) {}

    // eval function
    void eval() {
        // Reset always takes precedence
        if (!reset_x) {
            // Init registers
            ew_state <= green;
            ns_state <= red;
            timer <= 0;
            ns_cycle <= false;

            // Init outputs
            east_west = green;
            north_south = red;

            // All done this clock.
            return;
        }

        // If we are running a north-south cycle...
        if (ns_cycle) {
            if (ns_state == green) {
                if (timer == 0) {
                    ns_state <= yellow;
                    timer <= delay;
                }
                else timer <= timer - 1;
            }
            else if (ns_state == yellow)
                ns_state <= red;
            else if (ns_state == red) {
                ns_cycle <= false;
                ew_state <= green;
            }
        }

        // Or an east-west cycle...
        else {
            if (ew_state == green) {
                if (timer == 0) {
                    ew_state <= yellow;
                    timer <= delay;
                }
                else timer <= timer - 1;
            }
            else if (ew_state == yellow)
                ew_state <= red;
            else if (ew_state == red) {
                ns_cycle <= true;
                ns_state <= green;
            }
        }

        // Drive traffic light outputs.
        north_south = ns_state;
        east_west = ew_state;
    }

    // Ports
    Input<bool> instance(reset_x);
    Input<uint32_t, 8> instance(delay);
    Output<color, 2> instance(east_west);
    Output<color, 2> instance(north_south);

private:
    Register<color, 2> instance(ew_state);
    Register<color, 2> instance(ns_state);
    Register<uint32_t, 8> instance(timer);
    Register<bool> instance(ns_cycle);
};
```

First, for readability, there is an enum declaration ```enum color``` which defines the three traffic signal colors along with a color code to string
conversion function ```const char* color2str(const color c)```. 

Next is the actual declaration of the TLC ```class tlc final : public Module {};```. Note that this is just the declaration; the actual instance is in
the main program:
```cpp
  tlc iTLC = { NULL, "iTLC" };
```

Two variants of the constructor are defined, one just allowing a character string to be provided as the instance name instead of a ```std::string```.
The main constructor then connects all wires and registers that the TLC should be sensitized on and performs initialization of the registers and wires.

The ```eval()``` method does all the heavy lifting for the TLC module, implementing the state machine that controls the traffic lights.
There are two modes: one where ```ns_cycle``` is true (meaning a north-south green light sequence) and one where it is false (meaning
an east-west green light sequence). A ```timer``` counter is provided to count off green light states.

The last public declaration in the TLC module are the three ports, one input ```delay``` wire that is the timer length and two
output wires representing the state of the north-south and east-west traffic lights.

Private to the TLC are the registers representing the state of the TLC.

The associated testbench for the TLC module is shown below:
```cpp
// TLC test bench
struct tlc_tb : public Testbench {
    // The TLC instance.
    tlc instance(iTLC);

    // Constructors.
    tlc_tb(const std::string& nm) : Testbench(nm) { opt_timer_ticks = 4; }
    tlc_tb(const char* str) : Testbench(str) { opt_timer_ticks = 4; }

    void tlc_usage() {
        extern char* prog_name;
        std::cerr << "usage: " << prog_name << " <program options> tlc [-t timer_ticks]" << std::endl;
        exit(1);
    }

    void main(int argc, char** argv) {
        int ch;
        int32_t cycle_limit = 32;

        // Process TLC-specific command line options
        while ((ch = getopt(argc, argv, "+t:")) != -1) {
            switch (ch) {
            case 't':
                opt_timer_ticks = atoi(optarg);
                break;
            default:
                tlc_usage();
                /* NOT REACHED */
                break;
            }
        }
        argc -= optind;
        if (argc) 
            tlc_usage();

        // Set simulation() options.
        set_cycle_limit(cycle_limit);
        set_iteration_limit(10);

        // Do the simulation
        this->begin_test();
        try {
            this->simulation();
        } catch (const std::exception& e) {
            std::cerr << "Caught system error: " << e.what() << std::endl;
            exit(1);
        }
        this->end_test_pass("TLC passed after %d clocks\n", this->get_clock());
    }

    // activity around clocks
    void eval() {
        if (!reset_done) {
            reset_done <= true;
            iTLC.delay = opt_timer_ticks - 1;
            iTLC.reset_x = false;
        } else
            iTLC.reset_x = true;
    }

    void post_clock(const uint32_t cycle_num) {
        printf("clock %u: East-West = %s, North-South = %s\n", cycle_num, color2str(iTLC.east_west), color2str(iTLC.north_south));
    }

    // Timer length option.
    int opt_timer_ticks;

    // Reset state machine (simple).
    Register<bool> instance(reset_done, false);
};
```