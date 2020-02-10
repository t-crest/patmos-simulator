/*
   Copyright 2012 Technical University of Denmark, DTU Compute.
   All rights reserved.

   This file is part of the Patmos simulator.

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
 */

//
// Method-cache implementations.
//

#include "method-cache.h"

#include "basic-types.h"
#include "endian-conversion.h"
#include "instr-cache.h"
#include "simulation-core.h"
#include "symbol.h"

#include <cassert>
#include <cmath>
#include <map>
#include <ostream>
#include <limits>

#include <boost/format.hpp>
#include "exception.h"


using namespace patmos;


void ideal_method_cache_t::initialize(simulator_t &s, uword_t address)
{
  current_base = address;
}

bool ideal_method_cache_t::fetch(simulator_t &s, uword_t base, uword_t address, word_t iw[2])
{
  Memory.read_peek(s, address, reinterpret_cast<byte_t*>(&iw[0]),
                   sizeof(word_t)*2, true);
  return true;
}

bool ideal_method_cache_t::load_method(simulator_t &s, uword_t address, word_t offset)
{
  current_base = address;
  return true;
}

bool ideal_method_cache_t::is_available(simulator_t &s, uword_t address)
{
  return true;
}

uword_t ideal_method_cache_t::get_active_method_base()
{
  return current_base;
}

void ideal_method_cache_t::tick(simulator_t &s)
{
  // do nothing here
}

void ideal_method_cache_t::print(const simulator_t &s, std::ostream &os)
{
  // nothing to do here either, since the cache has no internal state.
}

void ideal_method_cache_t::print_stats(const simulator_t &s, std::ostream &os,
                                       const stats_options_t& options)
{
  // nothing to do here either, since the cache has no internal state.
}



void lru_method_cache_t::method_info_t::update(uword_t address,
                                               uword_t num_blocks,
                                               uword_t num_bytes)
{
  Address = address;
  Num_blocks = num_blocks;
  Num_bytes = num_bytes;
  reset_utilization();
}

void lru_method_cache_t::method_info_t::reset_utilization() {
  Utilization.clear();
  Utilization.resize(Num_bytes / sizeof(uword_t) + 3);
}

unsigned int lru_method_cache_t::method_info_t::get_utilized_bytes() {
  uword_t utilized_bytes = 0;
  for (unsigned int i = 0; i < Utilization.size(); i++) {
    if (Utilization[i]) {
      utilized_bytes += sizeof(uword_t);
    }
  }
  return utilized_bytes;
}


bool lru_method_cache_t::do_fetch(simulator_t &s, method_info_t &current_method,
                                  uword_t address, word_t iw[2])
{
  if(Phase != IDLE ||
      address < current_method.Address ||
      current_method.Address + current_method.Num_bytes + sizeof(word_t) * NUM_SLOTS * 3 <= address)
  {
    simulation_exception_t::illegal_pc(current_method.Address);
  }

  // get instruction word from the method's instructions
  byte_t *iwp = reinterpret_cast<byte_t*>(&iw[0]);

  // TODO read from Cache buffer, get read position(s) from method_info.

  Memory.read_peek(s, address, iwp, sizeof(word_t)*NUM_SLOTS, true);

  for (unsigned int i = 0; i < NUM_SLOTS; i++) {
    unsigned int word = (address-current_method.Address)/sizeof(word_t) + i;
    if (word >= current_method.Num_bytes/sizeof(word_t)) {
      break;
    }
    current_method.Utilization[word] = true;
  }

  Num_bytes_fetched += sizeof(word_t) * NUM_SLOTS;

  return true;
}

bool lru_method_cache_t::lookup(simulator_t &s, uword_t address)
{
  // check if the address is in the cache
  for(int i = Num_blocks - 1; i >= (int)(Num_blocks - Num_active_methods); i--)
  {
    if (Methods[i].Address == address)
    {
      // update the ordering of the methods to match LRU.

      // store the currently accessed entry
      method_info_t tmp = Methods[i];

      // shift all methods between the location of the currently accessed
      // entry and the previously most recently used entry.
      for(unsigned int j = i; j < Num_blocks - 1; j++)
      {
        Methods[j] = Methods[j + 1];
      }

      // reinsert the current entry at the head of the table
      Methods[Num_blocks - 1] = tmp;

      return true;
    }
  }

  // No entry matches the given address.
  return false;
}

void lru_method_cache_t::update_utilization_stats(method_info_t &method,
                                                  uword_t utilized_bytes)
{

  float utilization = (float)utilized_bytes / (float)method.Num_bytes;

  Method_stats[method.Address].Max_utilization = std::max(utilization,
                              Method_stats[method.Address].Max_utilization);
  Method_stats[method.Address].Min_utilization = std::min(utilization,
                              Method_stats[method.Address].Min_utilization);
}

void lru_method_cache_t::update_evict_stats(method_info_t &method,
                                            uword_t new_method,
                                            eviction_type_e type)
{
  if (type != EVICT_FLUSH) {
    std::pair<unsigned int, unsigned int> &eviction_stats(
                          Method_stats[method.Address].Evictions[new_method]);
    if (type == EVICT_TAG)
    {
      eviction_stats.second++;
      Num_evictions_tag++;
    }
    else if (type == EVICT_CAPACITY)
    {
      eviction_stats.first++;
      Num_evictions_capacity++;
    }
  }

  unsigned int utilized_bytes = method.get_utilized_bytes();

  Num_bytes_utilized += utilized_bytes;

  update_utilization_stats(method, utilized_bytes);
}

void lru_method_cache_t::print_cache_state(simulator_t& s, std::ostream& dout,
                                           size_t active_method) const
{
  dout << "     { ";
  for (unsigned int i = 1; i <= Num_active_methods; i++) {
    if (i > 1 && ((i-1) % 4 == 0)) {
      dout << " |\n";
      dout << "       ";
    } else if (i > 1) {
      dout << " | ";
    }
    if (Num_blocks - i == active_method) {
      dout << "*";
    } else {
      dout << " ";
    }

    std::stringstream ss;
    std::string symbol;
    s.Symbols.print(ss, Methods[Num_blocks - i].Address, true);
    ss >> symbol;

    dout << boost::format("0x%1$08x: %2$-22s")
         % Methods[Num_blocks - i].Address
         % symbol;
  }
  dout << "   }\n";
}

void lru_method_cache_t::print_hit(simulator_t &s, std::ostream& dout,
                                   uword_t address) const
{
  size_t active_method = 0;
  // find active method
  for (unsigned int i = 1; i <= Num_active_methods; i++) {
    if (Methods[Num_blocks - i].Address == address) {
      active_method = Num_blocks - i;
      break;
    }
  }

  dout << boost::format("M$ HIT:  Entry %1$2d: 0x%2$08x %3$-30s Size: %4$6d, Used: %5$6d\n")
       % (Num_blocks - active_method - 1)
       % Methods[active_method].Address
       % s.Symbols.find(Methods[active_method].Address)
       % Methods[active_method].Num_bytes
       % Methods[active_method].get_utilized_bytes();

  print_cache_state(s, dout, active_method);

  dout << "\n";
}

void lru_method_cache_t::print_miss(simulator_t &s, std::ostream& dout,
                                    uword_t address, uword_t evicted_methods,
                                    uword_t evicted_blocks,
                                    uword_t blocks_freed,
                                    bool capacity_miss) const
{
  size_t active_method = 0;
  // find the new active method
  for (unsigned int i = 1; i <= Num_active_methods; i++) {
    if (Methods[Num_blocks - i].Address == address) {
      active_method = Num_blocks - i;
      break;
    }
  }

  dout << boost::format("M$ MISS: Entry %1$2d: 0x%2$08x %3$-30s Size: %4$6d, Used: %5$6d\n")
       % (Num_blocks - active_method - 1)
       % Methods[active_method].Address
       % s.Symbols.find(Methods[active_method].Address)
       % Methods[active_method].Num_bytes
       % Methods[active_method].get_utilized_bytes();
  if (evicted_methods > 0) {
    if (capacity_miss) {
      dout << "         CAPACITY miss ";
    } else {
      dout << "         TAG SIZE miss ";
    }
    dout << boost::format("replaces %1$2d methods of %2$5d bytes, frees %3$5d bytes\n")
         % evicted_methods
         % (evicted_blocks * Num_block_bytes)
         % (blocks_freed * Num_block_bytes);
  } else {
    dout << "         COLD miss\n";
  }

  print_cache_state(s, dout, active_method);

  dout << "\n";
}

bool lru_method_cache_t::read_function_size(simulator_t &s,
                                            word_t function_base,
                                            uword_t *result_size)
{
  // We only peek at the size in the simulation, and load the method
  // together with the size word
  return peek_function_size(s, function_base, result_size);
}

bool lru_method_cache_t::peek_function_size(simulator_t &s,
                                            word_t function_base,
                                            uword_t *result_size)
{
  uword_t num_bytes_big_endian;
  Memory.read_peek(s, function_base - sizeof(uword_t),
      reinterpret_cast<byte_t*>(&num_bytes_big_endian),
      sizeof(uword_t), true);
  // convert method size to native endianess and compute size in
  // blocks
  *result_size = from_big_endian<big_uword_t>(num_bytes_big_endian);
  return true;
}


uword_t lru_method_cache_t::get_num_blocks_for_bytes(uword_t num_bytes)
{
  return ((num_bytes - 1) / Num_block_bytes) + 1;
}

uword_t lru_method_cache_t::get_transfer_start(uword_t address) {
  return address - 4;
}

uword_t lru_method_cache_t::get_transfer_size()
{
  // Memory controller aligns to burst size
  // But we need to transfer the size word as well.
  return Num_method_size + 4;
}

lru_method_cache_t::lru_method_cache_t(memory_t &memory,
                    unsigned int num_blocks,
                    unsigned int num_block_bytes,
                    unsigned int max_active_methods) :
    Memory(memory), Num_blocks(num_blocks),
    Num_block_bytes(num_block_bytes), Phase(IDLE),
    Num_allocate_blocks(0), Num_method_size(0), Num_active_methods(0),
    Num_active_blocks(0), Num_blocks_allocated(0),
    Num_max_blocks_allocated(0), Num_bytes_transferred(0),
    Num_max_bytes_transferred(0), Num_bytes_fetched(0),
    Num_max_active_methods(0),
    Num_hits(0), Num_misses(0), Num_misses_ret(0), Num_evictions_capacity(0),
    Num_evictions_tag(0), Num_stall_cycles(0),
    Num_bytes_utilized(0), Num_blocks_freed(0), Max_blocks_freed(0)
{
  Num_max_methods = max_active_methods ? max_active_methods : num_blocks;
  Methods = new method_info_t[Num_blocks];
  for(unsigned int i = 0; i < Num_blocks; i++)
    Methods[i] = method_info_t();

  Cache = new byte_t[Num_block_bytes * Num_blocks + 4];
}

void lru_method_cache_t::initialize(simulator_t &s, uword_t address)
{
  assert(Num_active_blocks == 0 && Num_active_methods == 0);

  // get 'most-recent' method of the cache
  method_info_t &current_method = Methods[Num_blocks - 1];

  // we assume it is an ordinary function entry with size specification
  // (the word before) and copy it in the cache.
  uword_t num_bytes, num_blocks;
  peek_function_size(s, address, &num_bytes);
  num_blocks = get_num_blocks_for_bytes(num_bytes);

  current_method.update(address, num_blocks, num_bytes);
  Num_active_blocks = num_blocks;

  Num_active_methods = 1;
  Num_max_active_methods = std::max(Num_max_active_methods, 1U);
}

bool lru_method_cache_t::fetch(simulator_t &s, uword_t base, uword_t address, word_t iw[2])
{
  // fetch from 'most-recent' method of the cache
  return do_fetch(s, Methods[Num_blocks - 1], address, iw);
}

bool lru_method_cache_t::load_method(simulator_t &s, uword_t address, word_t offset)
{
  // check status of the method cache
  switch(Phase)
  {
    // a new request has to be started.
    case IDLE:
    {
      assert(Num_allocate_blocks == 0 && Num_method_size == 0);

      if (lookup(s, address))
      {
        // method is in the cache ... done!
        Num_hits++;
        Method_stats[address].Accesses[offset].first++;

        if (s.Dbg_stack.get_stats_options().debug_cache == patmos::DC_ALL &&
            s.Dbg_stack.is_printing())
        {
          print_hit(s, *s.Dbg_stack.get_stats_options().debug_out, address);
        }
        return true;
      }
      else
      {
        // proceed to next phase ... fetch the size from memory.
        // NOTE: the next phase starts immediately.
        Phase = SIZE;
        Num_misses++;
        if (offset != 0)
          Num_misses_ret++;
        Method_stats[address].Accesses[offset].second++;
      }
    }

    // the size of the method has to be fetched from memory.
    case SIZE:
    {
      assert(Num_allocate_blocks == 0 && Num_method_size == 0);

      // get the size of the method that should be loaded
      if (peek_function_size(s, address, &Num_method_size)) {

        Num_allocate_blocks = get_num_blocks_for_bytes(Num_method_size);

        // TODO should we also store how many bytes are actually transferred
        // by the memory? Ask the Memory for the actual transfer size.
        Method_stats[address].Num_method_bytes = Num_method_size;
        Method_stats[address].Num_blocks_allocated = Num_allocate_blocks;

        // check method size against cache size.
        if (Num_allocate_blocks == 0 || Num_allocate_blocks > Num_blocks)
        {
          simulation_exception_t::code_exceeded(address);
        }

        uword_t evicted_blocks = 0;
        uword_t evicted_methods = 0;
        bool capacity_miss = (Num_active_blocks + Num_allocate_blocks > Num_blocks);

        // throw other entries out of the cache if needed
        while (Num_active_blocks + Num_allocate_blocks > Num_blocks ||
                Num_active_methods >= Num_max_methods)
        {
          assert(Num_active_methods > 0);
          method_info_t &method(Methods[Num_blocks - Num_active_methods]);

          // update eviction statistics
          evicted_blocks += method.Num_blocks;
          evicted_methods++;

          // is this a cache miss due to the limited number of tag?
          bool is_tag_capacity_miss = (Num_active_blocks +
                                        Num_allocate_blocks <= Num_blocks);

          update_evict_stats(method, address, is_tag_capacity_miss ? EVICT_TAG
                                                              : EVICT_CAPACITY);

          // evict the method from the cache
          Num_active_blocks -= method.Num_blocks;
          Num_active_methods--;
        }

        uword_t blocks_freed = evicted_blocks > Num_allocate_blocks ?
                               evicted_blocks - Num_allocate_blocks : 0;

        Num_blocks_freed += blocks_freed;
        Max_blocks_freed = std::max(Max_blocks_freed, blocks_freed);

        // update counters
        Num_active_methods++;
        Num_max_active_methods = std::max(Num_max_active_methods,
                                          Num_active_methods);
        Num_active_blocks += Num_allocate_blocks;
        Num_blocks_allocated += Num_allocate_blocks;
        Num_max_blocks_allocated = std::max(Num_max_blocks_allocated,
                                              Num_allocate_blocks);
        Num_bytes_transferred += get_transfer_size();
        Num_max_bytes_transferred = std::max(Num_max_bytes_transferred,
                                              get_transfer_size());

        // shift the remaining blocks
        for(unsigned int j = Num_blocks - Num_active_methods;
            j < Num_blocks - 1; j++)
        {
          Methods[j] = Methods[j + 1];
        }

        // insert the new entry at the head of the table
        Methods[Num_blocks - 1].update(address, Num_allocate_blocks,
                                                Num_method_size);

        if (s.Dbg_stack.get_stats_options().debug_cache != patmos::DC_NONE &&
            s.Dbg_stack.is_printing())
        {
          print_miss(s, *s.Dbg_stack.get_stats_options().debug_out, address,
                     evicted_methods, evicted_blocks, blocks_freed,
                     capacity_miss);
        }

        // proceed to next phase ... the size of the method has been fetched
        // from memory, now transfer the method's instructions.
        // NOTE: the next phase starts immediately.
        Phase = TRANSFER;
      } else {
        // keep waiting until the size has been loaded.
        return false;
      }
    }

    // begin transfer from main memory to the method cache.
    case TRANSFER:
    {
      assert(Num_allocate_blocks != 0 && Num_method_size != 0);

      // TODO implement as actual cache, keep track of where to store
      // methods to in the cache buffer, and keep pointers into the cache in
      // the method_infos.

      if (Memory.read(s, get_transfer_start(address), Cache,
                      get_transfer_size(), true))
      {
        // the transfer is done, go back to IDLE phase
        Num_allocate_blocks = Num_method_size = 0;
        Phase = IDLE;
        return true;
      }
      else
      {
        // keep waiting until the transfer is completed.
        return false;
      }
    }
  }

  assert(false);
  abort();
}

bool lru_method_cache_t::is_available(simulator_t &s, uword_t address)
{
  // check if the address is in the cache
  for(int i = Num_blocks - 1; i >= (int)(Num_blocks - Num_active_methods);
      i--)
  {
    if (Methods[i].Address == address)
    {
      return true;
    }
  }

  return false;
}

uword_t lru_method_cache_t::get_active_method_base()
{
  return Methods[Num_blocks - 1].Address;
}

size_t lru_method_cache_t::get_active_method() const
{
  return Num_blocks - 1;
}

void lru_method_cache_t::tick(simulator_t &s)
{
  // update statistics
  if (Phase != IDLE)
    Num_stall_cycles++;
}

void lru_method_cache_t::print(const simulator_t &s, std::ostream &os)
{
  os << boost::format(" #M: %1$02d #B: %2$02d\n")
      % Num_active_methods % Num_active_blocks;

  for(int i = Num_blocks - 1; i >= (int)(Num_blocks - Num_active_methods);
      i--)
  {
    os << boost::format("   M%1$02d: 0x%2$08x (%3$8d Blk %4$8d b)\n")
        % (Num_blocks - i) % Methods[i].Address % Methods[i].Num_blocks
        % Methods[i].Num_bytes;
  }

  os << '\n';
}

void lru_method_cache_t::print_stats(const simulator_t &s, std::ostream &os,
                                     const stats_options_t& options)
{
  uword_t bytes_utilized = Num_bytes_utilized;
  for(unsigned int j = Num_blocks - Num_active_methods; j < Num_blocks; j++)
  {
    uword_t ub = Methods[j].get_utilized_bytes();

    bytes_utilized += ub;

    update_utilization_stats(Methods[j], ub);
  }

  // per cache miss, we load the size word, but do not store it in the cache
  uword_t bytes_allocated = Num_bytes_transferred - Num_misses * 4;
  // Utilization = Bytes used / bytes allocated in cache
  float utilization = (float)bytes_utilized /
                      (float)(Num_blocks_allocated * Num_block_bytes);
  // Internal fragmentation = Bytes loaded to cache / Bytes allocated in cache
  float fragmentation = 1.0 - (float)bytes_allocated /
                      (float)(Num_blocks_allocated * Num_block_bytes);

  // External fragmentation = Blocks evicted but not allocated / Blocks allocated
  float ext_fragmentation = (float)Num_blocks_freed /
                            (float)Num_blocks_allocated;

  // Ratio of bytes loaded from main memory to bytes fetched from the cache.
  float transfer_ratio = (float)Num_bytes_transferred/(float)Num_bytes_fetched;

  // instruction statistics
  os << boost::format("                              total        max.\n"
                      "   Blocks Allocated    : %1$10d  %2$10d\n"
                      "   Bytes Transferred   : %3$10d  %4$10d\n"
                      "   Bytes Allocated     : %5$10d  %6$10d\n"
                      "   Bytes Used          : %7$10d\n"
                      "   Block Utilization   : %8$10.2f%%\n"
                      "   Int. Fragmentation  : %9$10.2f%%\n"
                      "   Bytes Freed         : %10$10d  %11$10d\n"
                      "   Ext. Fragmentation  : %12$10.2f%%\n"
                      "   Max Methods in Cache: %13$10d\n"
                      "   Cache Hits          : %14$10d  %15$10.2f%%\n"
                      "   Cache Misses        : %16$10d  %17$10.2f%%\n"
                      "   Cache Misses Returns: %18$10d  %19$10.2f%%\n"
                      "   Evictions Capacity  : %20$10d  %21$10.2f%%\n"
                      "   Evictions Tag       : %22$10d  %23$10.2f%%\n"
                      "   Transfer Ratio      : %24$10.3f\n"
                      "   Miss Stall Cycles   : %25$10d  %26$10.2f%%\n\n")
    % Num_blocks_allocated % Num_max_blocks_allocated
    % Num_bytes_transferred % Num_max_bytes_transferred
    % bytes_allocated % (Num_max_bytes_transferred - 4)
    % bytes_utilized % (utilization * 100.0) % (fragmentation * 100.0)
    % (Num_blocks_freed * Num_block_bytes)
    % (Max_blocks_freed * Num_block_bytes)
    % (ext_fragmentation * 100.0)
    % Num_max_active_methods
    % Num_hits % (100.0 * Num_hits / (Num_hits + Num_misses))
    % Num_misses % (100.0 * Num_misses / (Num_hits + Num_misses))
    % Num_misses_ret % (100.0 * Num_misses_ret / Num_misses)
    % Num_evictions_capacity
    % (100.0*Num_evictions_capacity/(Num_evictions_capacity+Num_evictions_tag))
    % Num_evictions_tag
    % (100.0 * Num_evictions_tag / (Num_evictions_capacity + Num_evictions_tag))
    % transfer_ratio
    % Num_stall_cycles % (100.0 * Num_stall_cycles / (float)s.Cycle);

  if (options.short_stats)
    return;

  // print stats per method
  os << "       Method:        #hits     #misses  methodsize      blocks    "
        "min-util    max-util\n";
  for(method_stats_t::iterator i(Method_stats.begin()),
      ie(Method_stats.end()); i != ie; i++)
  {
    unsigned int hits = 0;
    unsigned int misses = 0;
    for(offset_stats_t::iterator j(i->second.Accesses.begin()),
        je(i->second.Accesses.end()); j != je; j++)
    {
      hits += j->second.first;
      misses += j->second.second;
    }

    // Skip all stats entries that are never accessed since the last stats reset
    if (hits+misses == 0) {
      continue;
    }

    os << boost::format("   0x%1$08x:   %2$10d  %3$10d  %4$10d  %5$10d "
                        "%6$10.2f%% %7$10.2f%%    %8%\n")
        % i->first % hits % misses
        % i->second.Num_method_bytes % i->second.Num_blocks_allocated
        % (i->second.Min_utilization * 100.0)
        % (i->second.Max_utilization * 100.0)
        % s.Symbols.find(i->first);

    // print hit/miss statistics per offset
    if (options.hitmiss_stats)
    {
      for(offset_stats_t::iterator j(i->second.Accesses.begin()),
          je(i->second.Accesses.end()); j != je; j++)
      {
        os << boost::format("     0x%1$08x: %2$10d  %3$10d %4%\n")
            % (i->first + j->first)
            % j->second.first
            % j->second.second
            % s.Symbols.find(i->first + j->first);
      }
    }
  }

  // print Eviction statistics
  if (options.hitmiss_stats) {
    os << "\n       Method:    #capacity        #tag          by\n";
    for(method_stats_t::iterator i(Method_stats.begin()), ie(Method_stats.end());
        i != ie; i++)
    {
      // count number of evictions
      unsigned int num_capacity_evictions = 0;
      unsigned int num_tag_evictions = 0;
      for(eviction_stats_t::iterator j(i->second.Evictions.begin()),
          je(i->second.Evictions.end()); j != je; j++)
      {
        num_capacity_evictions += j->second.first;
        num_tag_evictions += j->second.second;
      }

      // print address and name of current method
      os << boost::format("   0x%1$08x:   %2$10d  %3$10d          %4%\n")
          % i->first % num_capacity_evictions % num_tag_evictions
          % s.Symbols.find(i->first);

      // print other methods who evicted this method
      for(eviction_stats_t::iterator j(i->second.Evictions.begin()),
          je(i->second.Evictions.end()); j != je; j++)
      {
        os << boost::format("                 %1$10d  %2$10d  0x%3$08x  %4%\n")
            % j->second.first % j->second.second % j->first
            % s.Symbols.find(j->first);
      }
    }
  }
}

void lru_method_cache_t::reset_stats()
{
  Num_blocks_allocated = 0;
  Num_max_blocks_allocated = 0;
  Num_bytes_transferred = 0;
  Num_max_blocks_allocated = 0;
  Num_bytes_fetched = 0;
  Num_bytes_utilized = 0;
  Num_max_active_methods = 0;
  Num_hits = 0;
  Num_misses = 0;
  Num_misses_ret = 0;
  Num_evictions_capacity = 0;
  Num_evictions_tag = 0;
  Num_stall_cycles = 0;
  Num_blocks_freed = 0;
  Max_blocks_freed = 0;
  Method_stats.clear();
  for(unsigned int j = Num_blocks - Num_active_methods; j < Num_blocks; j++)
  {
    Methods[j].reset_utilization();
  }
}

void lru_method_cache_t::flush_cache()
{
  if (Num_active_methods < 2) return;

  uword_t current_base = Methods[Num_blocks - 1].Address;

  for(unsigned int j = Num_blocks - Num_active_methods; j < Num_blocks - 1; j++)
  {
    update_evict_stats(Methods[j], current_base, EVICT_FLUSH);
  }

  Num_active_methods = 1;
  Num_active_blocks = Methods[Num_blocks - 1].Num_blocks;
}

/// free dynamically allocated cache memory.
lru_method_cache_t::~lru_method_cache_t()
{
  delete [] Methods;
  delete [] Cache;
}



bool fifo_method_cache_t::lookup(simulator_t &s, uword_t address)
{
  return base_t::is_available(s, address);
}

bool fifo_method_cache_t::load_method(simulator_t &s, uword_t address, word_t offset)
{
  // check if the address is in the cache
  bool avail = base_t::load_method(s, address, offset);

  if (avail) {
    // update the active method pointer
    for(int i = base_t::Num_blocks - 1;
        i >= (int)(base_t::Num_blocks - base_t::Num_active_methods); i--)
    {
      if (base_t::Methods[i].Address == address)
      {
        active_method = i;
      }
    }
  }

  return avail;
}

uword_t fifo_method_cache_t::get_active_method_base()
{
  return base_t::Methods[active_method].Address;
}

size_t fifo_method_cache_t::get_active_method() const
{
  return active_method;
}

bool fifo_method_cache_t::fetch(simulator_t &s, uword_t base, uword_t address, word_t iw[2])
{
  // fetch from the currently active method
  return base_t::do_fetch(s, base_t::Methods[active_method], address, iw);
}

void fifo_method_cache_t::flush_cache()
{
  if (Num_active_methods < 2) return;

  // Ensure that the active method is not flushed out.
  method_info_t active = Methods[active_method];
  Methods[active_method] = Methods[Num_blocks - 1];
  Methods[Num_blocks - 1] = active;

  active_method = Num_blocks - 1;

  for(unsigned int j = Num_blocks - Num_active_methods; j < Num_blocks - 1; j++)
  {
    update_evict_stats(Methods[j], active.Address, EVICT_FLUSH);
  }

  Num_active_methods = 1;
  Num_active_blocks = Methods[Num_blocks - 1].Num_blocks;
}

