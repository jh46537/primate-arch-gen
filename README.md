# Primate Build Process

### Requires:

 - Clang >= 14
 - LLVM requirements
 - 32-bit libc

### Checkout branch 'primate'

        git checkout primate

### Required env vars

        export CC=/usr/bin/clang
        export CXX=/usr/bin/clang++
        export PRIMATE_COMPILER_ROOT=<path to this repo>

### CMake with proper targets. This will make the backend with RISCV and Primate useful for comparisons.

        cmake -S llvm -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLLVM_ENABLE_PROJECTS='clang;libc;libcxx;libcxxabi' -DLLVM_TARGETS_TO_BUILD='Primate;RISCV' -DLLVM_BUILD_TESTS=False -DCMAKE_INSTALL_PREFIX="${PRIMATE_COMPILER_ROOT}/build" -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=Primate

### Ninja to actually compile

<<<<<<< HEAD
        ninja -C ${LLVM_ROOT}/build
=======
        ninja -C {PRIMATE_COMPILER_ROOT}/build
>>>>>>> f78e03c95ba1 ([ArchGen] Update readme with instructions.)
    
Note: may get some odd compile fails. This is potentially caused by OOM. just retry the build again.
Note: if you fail due to some inline asm related to fsqrt or similar, ensure you are using clang

### Finally you can compile cpp into primate insts:

<<<<<<< HEAD
        export PATH="${LLVM_ROOT}/build/bin:$PATH"
        export LD_LIBRARY_PATH="${LLVM_ROOT}/build/lib:$LD_LIBRARY_PATH"
        clang++ -std=c++20 --target=primate32-linux-gnu -march=pr32i -mno-relax -c -o <output-file> <cpp>
=======
        export PATH="${PRIMATE_COMPILER_ROOT}/build/bin:$PATH"``
        export LD_LIBRARY_PATH="${PRIMATE_COMPILER_ROOT}/build/lib:$LD_LIBRARY_PATH"``
        clang++ -std=c++20 --target=primate32-linux-gnu -march=pr32i -c -o <output-file> <cpp>``
>>>>>>> f78e03c95ba1 ([ArchGen] Update readme with instructions.)

By default Primate comes with a basic setup containing a Memory unit, IO unit, and 2 lanes of green functional units. 

### Generating a new architecture

Once you have run primate's archgen phase, you should be left with a `primate.cfg`, and a `BFU_list.txt`. 
In order to compile code for the generated architecture you need to run `archgen2tablegen.py <path to BFU_list.txt> <path to primate.cfg>`.
This will create a directory `primate-compiler-gen` with the tablegen files the compiler requires. 
Then run `cpyTablegen.sh`, re-run `ninja`, and you'll be able to compile!

### Useful commands:

dump the compile results:

        llvm-objdump –-arch-name=primate32 -d -t <obj-file> > debug.dsm

### Giving back

Primate compiler has some quirks that require ironing out. If you run into a backend crash is probably best to submit your IR, and primate config files as an issue on the project instead of attempting to debug.

Another thing of interest is op-fusion. If you think there is an important scalar operation that is not currently fused, open an issue. 

# The LLVM Compiler Infrastructure

[![OpenSSF Scorecard](https://api.securityscorecards.dev/projects/github.com/llvm/llvm-project/badge)](https://securityscorecards.dev/viewer/?uri=github.com/llvm/llvm-project)
[![OpenSSF Best Practices](https://www.bestpractices.dev/projects/8273/badge)](https://www.bestpractices.dev/projects/8273)
[![libc++](https://github.com/llvm/llvm-project/actions/workflows/libcxx-build-and-test.yaml/badge.svg?branch=main&event=schedule)](https://github.com/llvm/llvm-project/actions/workflows/libcxx-build-and-test.yaml?query=event%3Aschedule)

Welcome to the LLVM project!

This repository contains the source code for LLVM, a toolkit for the
construction of highly optimized compilers, optimizers, and run-time
environments.

The LLVM project has multiple components. The core of the project is
itself called "LLVM". This contains all of the tools, libraries, and header
files needed to process intermediate representations and convert them into
object files. Tools include an assembler, disassembler, bitcode analyzer, and
bitcode optimizer.

C-like languages use the [Clang](http://clang.llvm.org/) frontend. This
component compiles C, C++, Objective-C, and Objective-C++ code into LLVM bitcode
-- and from there into object files, using LLVM.

Other components include:
the [libc++ C++ standard library](https://libcxx.llvm.org),
the [LLD linker](https://lld.llvm.org), and more.

## Getting the Source Code and Building LLVM

Consult the
[Getting Started with LLVM](https://llvm.org/docs/GettingStarted.html#getting-the-source-code-and-building-llvm)
page for information on building and running LLVM.

For information on how to contribute to the LLVM project, please take a look at
the [Contributing to LLVM](https://llvm.org/docs/Contributing.html) guide.

## Getting in touch

Join the [LLVM Discourse forums](https://discourse.llvm.org/), [Discord
chat](https://discord.gg/xS7Z362),
[LLVM Office Hours](https://llvm.org/docs/GettingInvolved.html#office-hours) or
[Regular sync-ups](https://llvm.org/docs/GettingInvolved.html#online-sync-ups).

The LLVM project has adopted a [code of conduct](https://llvm.org/docs/CodeOfConduct.html) for
participants to all modes of communication within the project.
