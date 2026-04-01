# Moore Machine Simulator Library

A C library to build and simulate networks of Moore machines. 

It allows you to create individual automata (where the output is determined entirely by the current state), connect their inputs and outputs, and run step-by-step simulations of the entire system.

### Functions
* `ma_create_full()`: Instantiates a new automaton with defined input/output sizes, transition functions, output functions, and an initial state.
* `ma_create_simple()`: A helper function to create an automaton where the output function is an identity function.
* `ma_delete()`: Safely destroys an automaton, freeing all associated memory and automatically resolving (severing) any existing connections to or from other automata to prevent dangling pointers.
* `ma_connect()`: Binds a specific output pin of one automaton to an input pin of another.
* `ma_disconnect()`: Severs a specific input connection.
* `ma_set_input()`: Sets the boolean values for any unconnected external inputs.
* `ma_set_state()`: Forces the automaton into a specific state.
* `ma_step()`: Advances the internal state of an array of automata by exactly one synchronous step based on their current inputs and transition logic.
* `ma_get_output()`: Returns a pointer to the bit array representing the current output signals.

## Build Instructions

The library is designed to be built in a Linux environment using `gcc`. It is compiled as a shared library (`.so`) using the GNU C17 standard.

To build the library, run:

```bash
make libma.so
```

The build process uses the following compiler flags to ensure high performance and strict code quality:

```bash
-Wall -Wextra -Wno-implicit-fallthrough -std=gnu17 -fPIC -O2
```
