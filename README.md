Minispec
========

Minispec is a Hardware Description Language designed for teaching purposes. It
is inspired by Bluespec, and the current implementation heavily leverages the
Bluespec compiler, e.g., for type checking and to produce Verilog code.
However, Minispec has a minimalistic feature set that seeks to reduce its
learning curve. Key simplifications over Bluespec include:

* Combinational logic: No typeclasses, numeric types, provisos, or pattern
  matching.

* Sequential logic: Implicit interfaces, rules always fire every cycle (i.e.,
  no synthesized schedulers), no Action or ActionValue methods, no <-
  assignments. Instead of Action/ActionValue methods, modules have **inputs**
  that rules read to change the module's state. This maps to a more structural
  design style of sequential logic than Bluespec, but with a simple execution
  model that avoids combinational cycles (unlike e.g., Verilog).

* Parametric functions/modules/types with Integer or type parameters. No
  parametric (HM-style) type checking; instead, parametrics are elaborated
  as needed, then each instance is typechecked (similar to C++ templates).

Copyright & License
-------------------

Copyright (C) 2019-2022 by Daniel Sanchez

Minispec is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, version 2.

Setup
-----

**Using a virtual machine (recommended):** Install `vagrant`, then run `vagrant
up` from the minispec root folder in your host. This will provision an Ubuntu
18.04 VM with all dependencies, and build the minispec compiler (`msc`). Then,
run `vagrant ssh` to log into the VM.

The VM also installs Jupyter notebook with the Minispec kernel. Within the VM,
run `jupyter notebook --ip=0.0.0.0` to launch the notebook. You can access it
from the host at http://127.0.0.1:8888 (port forwarding is already configured).

**Dependencies:**

* Bluespec (`bsc`) version 2021.07 (build 4cac6eb).
  *Note: It is important to use a compatible version of `bsc`. The Minispec
  compiler parses `bsc` output, which may change across versions. In practice,
  `bsc` output stays relatively stable across versions, so bsc-2016.07.beta1
  and earlier open-source versions should work well. However, a different
  version may make make error reporting wonky.*

* A C++17 compiler (tested with `gcc 8.2`).

* `scons`

* `xxd` (standard in most Linux distributions). 

* ANTLR4 version 4.7 or above (tested with 4.7.2).
  *This is now automatically downloaded and built (see SConstruct).*

**Build:** Run `scons -j16`. This will produce `msc` (the Minispec compiler).
`msc` is self-contained, you can copy it to your system path and use it as-is
(run `msc -h` to see syntax and options).

Getting started
---------------

The Minispec language reference, at `docs/reference`, is the primary reference
for the Minispec HDL. It describes the syntax and semantics of the language in
detail, and illustrates their use. To build the reference, run:
```cd docs/reference
make
```
The reference uses LaTeX, so you'll need a LaTeX distribution installed in your
system (e.g., texlive).

`examples/` contains several examples showcasing different aspects of Minispec.
If you already know Bluespec, this should suffice to learn the language.

`tests/` has compiler tests on non-working examples, meant to show error
handling capabilities.

Finally, you may find it useful to check the grammar, at `src/Minispec.g4`.

