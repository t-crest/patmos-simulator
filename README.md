# Patmos Simulator

A collection of tools for simulating the Patmos processor.

## Included Tools

#### `paasm`   

A tiny assembler (accepts Patmos instructions, empty lines, and comments in the form of lines 
starting with # -- no symbols/relocation yet)

Usage: `paasm <input assembly> <binary stream>`

#### `padasm`  
A tiny disassembler (accepts a binary stream and prints the decoded instructions).

usage: `padasm <binary stream> <output assembly>`

#### `pasim   `

The Patmos simulator (accepts a binary stream, loads the entire stream into the simulator's 
main memory and begins execution at address zero).

usage: `pasim <binary stream> <trace output>`

## Installation

Prebuilt binaries can be found [here](https://github.com/t-crest/patmos-simulator/releases). 

1. Download the latest release tarball for your machine.
2. Extact the tarball in your T-CREST installation folder as-is (I.e. use the `extract here` option).
3. Ensure your T-CREST installation folder is on your PATH.
4. Done.

To build the simulator from source, see the [developer instructions](#anch-developer) below.

## Memory Configuration

### Overview

#### Memory models

The simulator provides the following memory models:

1. Ideal memory: All memory transfers are executed without any delays.
   Used for single-core if --gtime=0 and --tdelay=0

2. Fixed delay (default): Memory transfers are executed using burst of 
   fixed length and time.
   Used for single-core if --psize=0 and either --gtime or --tdelay is non-zero.

3. Variable bursts: Memory transfers are executed using bursts of arbitrary 
   length.
   Used for single-core if --psize and either --gtime or --tdelay is non-zero.

4. TDM access: Similar to fixed delay, but the memory is accessed using TDM.
   Used for multi-core configurations.

5. Ramulator (optional): Simulates the main memory using the DRAM emulator
  ramulator (single-core only). In order to enable ramulator the USE_RAMULATOR
  option has to be activated when invoking cmake (e.g., -DUSE_RAMULATOR=on).
  Subsequently, two additional command-line options become available --gkind and
  --ramul-config.

#### Requests and Bursts

Memory read or write request are issued by the method cache or instruction
cache, the stack cache, the data cache, bypass loads and stores. Requests
may not be aligned nor limited in size (e.g., the method cache loads
a whole function using one request).

The memory system is responsible for aligning requests and might transfer
the data using multiple bursts.

#### Common Options

The following options are common to all memory models (if applicable):

--gsize=<M>	    The size of the global memory in bytes.

--gtime=<t_B>	    The number of cycles per burst. The exact meaning 
		    depends on the memory model as explained below.

--bsize=<burst>	    The number of bytes per burst. The exact meaning 
		    depends on the memory model as explained below.


--tdelay=<t_DLY>    For every read or non-posted write request, n cycles 
		    are added once. This might include delays over the NOC 
		    and the memory controller (including both send and 
		    receive delays). It is assumed that for sequencial 
		    bursts, the delay for subsequent bursts can be hidden 
		    using pipelining.

--posted=<p>	    If p=0, posted writes are disabled. If p=1, posted 
		    writes are used. If p>1, posted writes are used, and 
		    at most n writes can be queued before the processor 
		    stalls.


### Ideal Memory Model

Options: None


The time to complete any request is always zero, i.e.,

  t_REQ(n) = 0


### Fixed Delay Memory

Options:
  --bsize   burst_size  Bytes transferred per burst
  --gtime   t_B         Cycles per burst (including activation and prefetch)
  --tdelay  t_DLY       Read delay per request in cycles


Request start and end addresses are aligned to burst_size, and only full 
bursts are transferred. A read or non-posted write request consisting of 
n bursts takes 

  t_REQ(n) = t_DLY + n * t_B

to complete. For posted writes, it takes

  t_REQwp(n) = n * t_B 

cycles to complete.


### Variable Bursts Memory

Options:
  --bsize   burst_size  Bytes transferred per page access
  --psize   page_size   Bytes per page
  --gtime   t_B         Minimum cycles per accessed page
  --tdelay  t_DLY       Read delay per request in cycles


The memory is segmented into pages of page_size bytes. For each page of a 
request, a minimum of burst_size bytes is transferred, taking t_B cycles. 
Every additional word is transferred in one cycle.

t_B models the maximum activation and precharge delay for accessing one 
row, and burst_size the number of read or written bytes that can overlap
activation and precharge.

Request start and end addresses are aligned to burst_size (thus, at least
burst_size bytes are transferred per page, taking t_B cycles per page).

Let start_page(s,b) and end_page(s,b) be the first and the last page number 
of a request of b bytes, starting at address s. Then, the number of page 
activations required is 

  Pages(s,b) = end_page(s,b) - start_page(s,b) + 1

and for reads and non-posted writes of b bytes length (after alignment), 
it takes

  t_REQ(s,b) = t_DLY + Pages(s,b) * t_B + (b - Pages(s,b) * burst_size) / 4

and for posted writes

  t_REQwp(b) = Pages(s,b) * t_B + (b - Pages(s,b) * burst_size) / 4

The first term describes the number of cycles required to activate
each page, while the second term describes the number of cycles required 
to transfer the remaining bytes that have not been transferred during
row activation.

Note that if page_size == burst_size, this memory is identical to the
fixed delay memory.

Also note that aligning start and end addresses to burst_size is not strictly
required for full-page bursts and leads to longer transfers than necessary,
but simplifies modelling the timing of corner-cases when a request starts 
or ends very close to the beginning of a page.


### TDM Memory

Options:
  --cores     N           Number of cores
  --cpuid     CPU-ID	  CPU ID, defines phase in TDM round
  --bsize     burst_size  Bytes transferred per slot
  --gtime     t_TDM       Cycles per slot
  --tdelay    t_DLY       Read delay per request in cycles
  --trefresh  t_Refresh   Cycles per round reserved for refresh


On a multi-core setup with N cores, the memory is accessed using TDM
with rounds of length

  t_Round = N * t_TDM + t_Refresh

where t_TDM is the length of one TDM slot (--gtime), and t_Refresh is
the number of cycles required per round to perform memory refreshs 
(--trefresh). Within one TDM slot, burst_size (--bsize) bytes are 
transferred. The NOC and memory controller roundtrip time is t_DLY
(--tdelay).

The TDM slot of a core depends on its CPU-ID (--cpuid). The phase of a
core is

  Phase(CPU-ID) = CPU-ID * t_TDM

Each request is aligned to burst_size. A core must wait t_Wait < t_Round
cycles for its TDM slot, and then starts sending a burst every 
t_Round cycles. The response per round is received after 
t_TDM + t_DLY cycles, which can be pipelined.

Therefore a read or non-posted write request of n bursts takes

  t_REQ(n) = t_Wait + (n-1) * t_Round + t_DLY + t_TDM

cycles, and posted writes of n bursts take

  t_REQwp(n) = t_Wait + (n-1) * t_Round + t_TDM

cycles.

### Ramulator Memory

Ramulator support is optional and needs to be activated using the USE_RAMULATOR
option when invoking cmake (e.g., pass -DUSE_RAMULATOR=on to cmake).

Once activated two additional options become available:
  --gkind         arg    Select the main memory kind. The default value is
        "simple", which activates one of the standard memory configurations (see
        above). Alternative options are "ddr3", "ddr4", "lpddr3", and "lpddr4",
        which activate the simulation of the appropriate DRAM model using
        ramulator.

  --ramul-config  file   Supply the name of a ramulator configuration file,
        which allows to select specific device details (number of channels,
        ranks, speed, ...). Example configuration files are included in the
        directory ramulator/configs.


Note that several of the other memory options are ignored when ramulator is
active, this includes (--gtime, --tdelay, --trefresh, --psize, --posted).
However, the device configuration is (automatically) adapted by ramulator in
order to match the selected burst size (--bsize). This may fail, though, when
the device configuration (provided through --ramul-config) is incompatible.

## <a name="anch-developer"></a>Developer

#### Setup

Requirements: 

* cmake    2.6 or above
* boost    1.46 or above
* C++14    GCC or clang  
* libelf   0.8.13 or above

To build the simulator, run the following commands in the root directory 
(assuming `build` is chosen as the build directory):
```
  mkdir build
  cd build
  cmake ..
  make -j
```

#### Testing

To test the simulator, run the following command in the build directory:
```
  make -j test
```

#### Release Packaging

Requirements:

* GNU-Tar utility available as `tar` on the command line.

To package binaries for release, run the following command in the build directory:
```
  make -j box
```

It will make a tarball with the name `patmos-simulator-*.tar.gz` (where `*` is the build target)
that includes the following:

* A `bin` folder containing the release binaries.
* A YAML file with various metadata about the release and which files are inluded.

### License

   Copyright 2012 Technical University of Denmark, DTU Compute.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

      1. Redistributions of source code must retain the above copyright notice,
         this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY EXPRESS
   OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
   NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   The views and conclusions contained in the software and documentation are
   those of the authors and should not be interpreted as representing official
   policies, either expressed or implied, of the copyright holder.



The source code of ramulator is included in the Patmos repository and licensed
under the GPL-compatible MIT license (aka. Expat). See ramulator/LICENSE for
additional details.

