NOTE: this doc is out of date and needs revision!

# The Pseudo-Verilog Header-only Library

This header-only library implements a Verilog-like C++ simulation infrastructure for hardware design.
This library is an alternative to SystemC (https://systemc.org/overview/systemc/) with the intent to create a more Verliog-look design.
Signals in this library are quasi three state (0, 1, and 'x'). 

# A Simple Example

The following example is a 4-bit up counter with enable:

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
The parent module is a module that constain the instance of the ```Counter``` class being constructed.

Next, the I/O ports of the class are decalred:

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

The```eval()``` is called whenever any input to the module is changed or if the register changes state.
(This will be explained in more detail later.)
At the head of the ```eval()``` function is a test of the input reset signal.
If reset is asserted (active low), the output port and register are driven to a known value.
The function then returns since there is nothing else to do that clock.
If reset is not asserted and enable is asserted, the counter is incremented (and qualified to stay within a 4-bit range).
This completes the implementation of the ```Counter``` class.

# Simulation Infrastructure

Four primary classes are defined to help simulate hardware behavior:
* Module - container for implementation modules (aka, blocks).
* Wire/QWire/Input/Output - containers for simulating ports and wires (akin to Verilog wires). 
* Register - container for simulating registers (flip flops) (akin to Verilog "reg").
* Testbench - a type of Module employed to test implementations.

## The ```Module``` Class

The ```Module``` class is a container intended to be subclassed by an actual implementation
of a module. The subclass must define two methods:
* ```Module(const Module* parent, const std::string& name)```: the class constructor.
* ```virtual void eval()```: a module evaluation function.

The constructor specifies both a parent module and an instance name (and if the module has no parent (i.e., a top level module), 
the parent module can be NULL). Together, they help form an instance name (a path). The ```eval()``` function is called when any register
or wire the module is sensitized on changes value; it is expected that the module then reevaluate its own state.

As a trivial example, assume we want to define as subclass ```Demo```:
```cpp
class Demo final : public Module {
public:
  Demo(const Module* parent, const std::string& name) : Module(parent, name) {
    ... code that builds Demo ...
  }
  
  virtual void eval() {
    ... code that evaluates the Demo module ...
  }
  
  /* Port Declarations */
  Wire<type> port1;
  Wire<type> port2;
  etc.
 
private:
  /* Private instances as required */
};
```
The ```Demo``` constructor should "build" the module. This includes making wire and register "connections" as 
needed to signal evaluations of this module. To actually get the construction to happen though, you have to instance the new module.
```cpp
Demo demo(myParent, "demo");
```
The ```Demo``` ```eval()``` function represents work to be performed when any input to a module or any register within the module
changes state. Generally speaking, for module-module communication, ```Wire``` types should be declared and instanced to carry information
from one module to the next. If a module connects itself to these wires during the construction phase, any change of information
on these wires will cause this module's to be called. A similar process would happen upon any change in internal register state
on the next clock edge if the module connects itself to its own registers.

Module ports should be declared as wires (and be public). In this case, the ports are not formally distinguished
as either input or output (as wires can be either). Instead, input versus output is determined by convention: modules 
creating instances of ```Demo``` should only write to input ports and the ```Demo``` class should only write to its outputs.
Furthermore, the ```Demo``` class constructor should add connections to its inputs in its constructor so that it is evaluated 
on any change to those inputs.

To help with declaring instances within a module (for example, wires, registers or other submodules), a macro is defined to make that
easired (and cleaner to read). For example:
```cpp
  Wire<type> instance(myWire);
```
The ```instance()``` macro is defined in ```main.h```:
```cpp
   #define instance(inst_name) inst_name = {this, #inst_name}
```
This macro takes care of declaring the instance correctly and automatically includes ```this``` as the parent module pointer (on the assumption that 
instances are occuring in the declaration of a module).

If required, a module can also declare a public ```init()``` method. There is no formal interface for ```init()``` - modules can define
whatever API they want. The intent of defining an ```init()``` method is to allow a module to initialize itself prior to simulation start,
but after all other modules are declared and instanced. In general, such methods should be declared as ```void```.

One final note: the ```Module``` class also includes methods to return the module's name, full instance name, and module type:
```cpp
  const std::string name() const;
  const std::string instanceName() const;
  inline const std::string typeName() const;
```
The instance name will be a full "dotted" path name (e.g., ```top.next.name```); if a module is top level, the instance name and the module
name will be identical.

## The ```Wire<>``` Template Class

The ```Wire``` class is a template class. That is, it can take any type as a base for the information communicated on the
wire. An example of an instance of a wire declaration:
```cpp
Wire<int> instance(x);
```
This declares ```x``` as a wire (whose name is 'x') and is of type ```int```.

Reading a wire has no effect on its state. Writing to a wire can be made through assignment:
```cpp
  x = 3;
```
If the wire had any other value before other than 3, post assignment, any modules connected to ```x``` will be reevaluated. 
(There is a danger here though with structure/class types: assignments to fields within a wire are not detected by the 
class. So, it is better to delare a non-templated instance of the wire type outside the wire, make changes to that instance,
and then assign the whole structure to the wire. This will ensure that the ```Wire<>``` class implementation will evaluate
any connected modules upon a wire state change. Or alternatively, declare a structure made up of individual wires and registers.)

As mentioned before, modules can connect themselves to wires during module construction time:
```cpp
class Demo : public Module {
  Demo(const Module* parent, const std::string& name) final : Module(parent, name) {
    (void) x.connect(this);
  }
  
  virtual void eval() {
    ... code that evaluates the Demo module ...
  }
  
private:
  Wire<int> instance(x);
};
```
The call to ```connect()``` above connects the calling module to the wire. (Once connected, there is no way to sever
the connection however (at present).) The prototype for connect is:
```cpp
inline bool connect(const Module* theModule);
```
The function returns true if this is the first connection call for the module and false if it is already connected.

To set the initial state of a wire prior to simulation, there is an ```init``` method:
```cpp
  void init(const T& v);
```
This method sets the initial state of the wire and schedules for evaluation any modules sensitized on this wire.

Identical to the ```Module``` class, there are also a number of getters related to naming:
```cpp
  const std::string name() const;
  const std::string instanceName() const;
  inline const std::string typeName() const;
```

## The ```Register<>``` Template Class

Like the ```Wire<>``` template class, the ```Register<>``` template class also accepts an arbitrary type as its data container.
However, unlike ```Wire<>```, assignments to ```Register<>``` instances have deferred effect. This simulates the behavior of
edge-triggered flip flops. 

All registers are linked to a common ```clock()``` function:
```cpp
  static void clock();
```
This function is a class method, meaning all instances shareone implementation of this function. 
The ```clock``` function should be called by the main simulation driver (more later). 
Writes to a register are delayed until ```clock``` is called. In fact, if there are multiple writes before the next call of
```clock()```, only the last write takes effect. Reading from a register always returns the last value written before the last
call to ```clock()```.

As with the ```Wire<>``` template class, the ```Register<>``` template class has an identical ```connect()``` method:
```cpp
inline bool connect(const Module* theModule);
```

Identical to the ```Module``` class, there are also a number of getters related to naming:
```cpp
  const std::string name() const;
  const std::string instanceName() const;
  inline const std::string typeName() const;
```

The register's parent can be accessed via a getter ```parent()``` that returns a ```const Module*``` to the parent Module.
There is also one class getter, ```uint64_t cycle_count() const;```. This function returns the current clock cycle number.

## The ```scheduler``` Namespace

Tying all of the above together, there is a namespace ```scheduler``` with four functions defined:
* ```void scheduler::schedule_eval(Module* m);```: called by the ```Wire``` and ```Register``` classes upon state change to 
schedule evaluation of affected modules. Normally, user code should not call this function, but if necessary, a module can 
force itself to be evaluated in this manner without a atste change.
* ```bool scheduler::eval_pending();```: returns true if any module is scheduled to be evaluated.
* ```void scheduler::clear__iterations();```: clears an iteration counter. Normally, we expect few passes on the number of evaluation 
attempts. However, with very complex module-wire-register interdependencies, we could evaluate modules many times between clocks.
However, there is a limit, and if this limit is exceeded, further evaluation is suspended. This call clears this limit counter.
* ```bool scheduler::eval();```: this is the actual evaluation pass. Causes all pending scheduled modules to be evaluated.

A prototype simulation engine could be constructed as follows:
```cpp
bool end_simulation = false;

... instance all top-level Modules, Wires, and Registers; perform module construction. ...

while (!end_simulation) {
  Register<int>.clock();    // can be any type really
  while (scheduler::eval_pending()) {
    if (!scheduler::eval()) {
      std::cerr << "Simulation evaluation passes exceeded at clock cycle " << Register<int>.cycle_count() << std::endl;
      exit(1);
    }
  }
  scheduler::clear_iterations();
}
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
