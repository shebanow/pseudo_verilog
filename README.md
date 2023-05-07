NOTE: this doc is out of date and needs revision!

# The Pseudo-Verilog Header-only Library

This header-only library implements a Verilog-like C++ simulation infrastructure for hardware design.
This library is an alternative to SystemC (https://systemc.org/overview/systemc/) with the intent to create a more Verliog-look design.
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
    void simulation(const bool continue_clock_sequence = false);
```
This method will run a simulation until some kind of stop condition arises.
The one argument to this method, ```continue_clock_sequence```, controls whether this is a new
simulation sequence or a continuation of a last simulation call. By default, a new simulation is assumed.
(For simple models, one simulation is perhaps enough. For more complex models which have multiple
test cases, it is perhaps easier to call ```simulation()``` once per test case, and as such each 
successive call is a clock count-wise continuation of the last simulation.)

To control ```simulation()```, there are a number of control methods defined within the
```Testbench``` subclass:
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
# Example: A Traffic Light Controller (TLC)

We provide an example Module using the classic "traffic light controller" (TLC) usually taught in Logic Design 101 classes.

```cpp
/*
 * tlc.h - traffic light controller
 */
include <iostream>
include <cstdint>
include <string>
include "module.h"
include "wire.h"
include "register.h"

enum color {
    red,
    yellow,
    green
};

const char* color2str(const color c) {
    if (c == red) return "red";
    else if (c == yellow) return "yellow";
    else return "green";
}

class tlc final : public Module {
public:
    tlc(const Module* p, const char* str) : tlc(p, std::string(str)) {}
    tlc(const Module* p, const std::string& nm) : Module(p, nm) {
        // hookup input delay
        delay.connect(this);

        // hookup registers
        ew_state.connect(this);
        ns_state.connect(this);
        timer.connect(this);
        ns_cycle.connect(this);

        // initialize
        ns_state = red;
        ew_state = green;
        north_south = red;
        east_west = green;
        ns_cycle = false;
        timer = 0;
    }

    // eval function
    void eval() {
        if (ns_cycle) {
            if (ns_state == green) {
                if (timer == 0) {
                    ns_state = yellow;
                    timer = delay;
                }
                else timer = timer - 1;
            }
            else if (ns_state == yellow)
                ns_state = red;
            else if (ns_state == red) {
                ns_cycle = false;
                ew_state = green;
            }
        } else {
            if (ew_state == green) {
                if (timer == 0) {
                    ew_state = yellow;
                    timer = delay;
                }
                else timer = timer - 1;
            }
            else if (ew_state == yellow)
                ew_state = red;
            else if (ew_state == red) {
                ns_cycle = true;
                ns_state = green;
            }
        }
        north_south = ns_state;
        east_west = ew_state;
    }

    // Ports
    Wire<uint32_t> instance(delay);
    Wire<color> instance(east_west);
    Wire<color> instance(north_south);

private:
    Register<color> instance(ew_state);
    Register<color> instance(ns_state);
    Register<uint32_t> instance(timer);
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
