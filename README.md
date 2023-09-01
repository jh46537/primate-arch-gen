# Primate Build Process

### Requires:

 - Clang >= 14
 - LLVM requirements
 - 32-bit libc

### Checkout branch 'primate'

        git checkout primate

### Some useful env vars

        export CC=/usr/bin/clang
        export CXX=/usr/bin/clang++
        export LLVM_ROOT=<path to this repo>

### CMake with proper targets. This will make the backend with RISCV and Primate useful for comparisons.

        cmake -S llvm -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLLVM_ENABLE_PROJECTS='clang;libc;libcxx;libcxxabi' -DLLVM_TARGETS_TO_BUILD='Primate;RISCV' -DLLVM_BUILD_TESTS=False -DCMAKE_INSTALL_PREFIX="${LLVM_ROOT}/build" -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=Primate

### Ninja to actually compile

        ninja -C {LLVM_ROOT}/build
    
Note: may get some odd compile fails. This is potentially caused by OOM. just retry the build again.
Note: if you fail due to some inline asm related to fsqrt or similar, ensure you are using clang

### Finally you can compile cpp into primate insts:

        export PATH="${LLVM_ROOT}/build/bin:$PATH"``
        export LD_LIBRARY_PATH="${LLVM_ROOT}/build/lib:$LD_LIBRARY_PATH"``
        clang++ -std=c++20 --target=primate32-linux-gnu -march=pr32i -c -o <output-file> <cpp>``

### Useful commands:

dump the compile results:

        llvm-objdump –arch-name=primate32 -d -t <obj-file> > debug.dsm

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
