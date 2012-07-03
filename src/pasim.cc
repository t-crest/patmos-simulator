//
//  This file is part of the Patmos Simulator.
//  The Patmos Simulator is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  The Patmos Simulator is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with the Patmos Simulator. If not, see <http://www.gnu.org/licenses/>.
//
//
// Main file of the Patmos Simulator.
//

#include "command-line.h"
#include "data-cache.h"
#include "instruction.h"
#include "method-cache.h"
#include "simulation-core.h"
#include "stack-cache.h"
#include "streams.h"

#include <gelf.h>
#include <libelf.h>

#include <fstream>
#include <iostream>

#include <boost/program_options.hpp>

/// Test whether the file is an elf executable image or a raw binary stream.
/// @param is The input stream to test.
/// @return True, in case the file appears to be an elf executable image, false
/// otherwise.
static bool is_elf(std::istream &is)
{
  char data[4];

  for(unsigned int i = 0; i < 4; i++)
  {
    data[i] = is.get();
  }

  bool result = (data[0] == '\177') && (data[1] == 'E') &&
                (data[2] == 'L') && (data[3] == 'F');

  for(unsigned int i = 0; i < 4; i++)
  {
    is.putback(data[3 - i]);
  }

  return result;
}

/// Read an elf executable image into the simulator's main memory.
/// @param is The input stream to read from.
/// @param m The main memory to load to.
/// @param msize Maximal number of bytes to load.
/// @return The entry point of the elf executable.
static patmos::uword_t readelf(std::istream &is, patmos::memory_t &m,
                               unsigned int msize)
{
  std::vector<char> elfbuf;
  elfbuf.reserve(1 << 20);
// read the whole stream.
  while (!is.eof())
  {
    char buf[128];

    // read into buffer
    is.read(&buf[0], sizeof(buf));

    // check how much was read
    std::streamsize count = is.gcount();
    assert(count <= 128);

    // write into main memory
    for(unsigned int i = 0; i < count; i++)
      elfbuf.push_back(buf[i]);
  }

  // check libelf version
  elf_version(EV_CURRENT);

  // open elf binary
  Elf *elf = elf_memory((char*)&elfbuf[0], elfbuf.size());
  assert(elf);

  // check file kind
  Elf_Kind ek = elf_kind(elf);
  assert(ek == ELF_K_ELF);

  // check class
  int ec = gelf_getclass(elf);
  assert(ec == ELFCLASS32);

  // get elf header
  GElf_Ehdr hdr;
  GElf_Ehdr *tmphdr = gelf_getehdr(elf, &hdr);
  assert(tmphdr);

  // get program headers
  size_t n;
  int ntmp = elf_getphdrnum (elf, &n);
  assert(ntmp == 0);

  for(size_t i = 0; i < n; i++)
  {
    // get program header
    GElf_Phdr phdr;
    GElf_Phdr *phdrtmp = gelf_getphdr(elf, i, &phdr);
    assert(phdrtmp);

    if (phdr.p_type == PT_LOAD)
    {
      // some assertions
      assert(phdr.p_vaddr == phdr.p_paddr);
      assert(phdr.p_filesz <= phdr.p_memsz);
      assert(phdr.p_paddr + phdr.p_memsz <= msize);

      // copy from the buffer into the main memory
      m.write_peek(phdr.p_paddr,
                   reinterpret_cast<patmos::byte_t*>(&elfbuf[phdr.p_offset]),
                   phdr.p_filesz);
    }
  }

  // get entry point
  patmos::uword_t entry = hdr.e_entry;

  elf_end(elf);

  return entry;
}

/// Read a raw binary image into the simulator's main memory.
/// @param is The input stream to read from.
/// @param m The main memory to load to.
/// @param msize Maximal number of bytes to load.
static void readbin(std::istream &is, patmos::memory_t &m, unsigned int msize)
{
  std::streamsize offset = 0;
  while (!is.eof())
  {
    patmos::byte_t buf[128];

    // read into buffer
    is.read(reinterpret_cast<char*>(&buf[0]), sizeof(buf));

    // check how much was read
    std::streamsize count = is.gcount();
    assert((count <= 128) && (offset + count < msize));

    // write into main memory
    m.write_peek(offset, buf, count);

    offset += count;
  }

  // some output
  std::cerr << boost::format("Loaded: %1% bytes\n") % offset;
}

/// Construct a global memory for the simulation.
/// @param time Access time in cycles for memory accesses.
/// @param size The requested size of the memory in bytes.
/// @return An instance of the requested memory.
static patmos::memory_t &create_global_memory(unsigned int time,
                                              unsigned int size)
{
  if (time == 0)
    return *new patmos::ideal_memory_t(size);
  else
    return *new patmos::fixed_delay_memory_t<>(size, time);
}

/// Construct a data cache for the simulation.
/// @param size The requested size of the data cache in bytes.
/// @param gm Global memory accessed on a cache miss.
/// @return An instance of a data cache.
static patmos::data_cache_t &create_data_cache(unsigned int size,
                                               patmos::memory_t &gm)
{
  return *new patmos::ideal_data_cache_t(gm);
}

/// Construct a method cache for the simulation.
/// @param mck The kind of the method cache requested.
/// @param size The requested size of the method cache in bytes.
/// @param gm Global memory accessed on a cache miss.
/// @return An instance of the requested method  cache kind.
static patmos::method_cache_t &create_method_cache(patmos::method_cache_e mck,
                                                 unsigned int size,
                                                 patmos::memory_t &gm)
{
  switch(mck)
  {
    case patmos::MC_IDEAL:
      return *new patmos::ideal_method_cache_t(gm);
    case patmos::MC_LRU:
    {
      // convert size to number of blocks
      unsigned int num_blocks = std::ceil((float)size/
                                   (float)patmos::NUM_METHOD_CACHE_BLOCK_BYTES);

      return *new patmos::lru_method_cache_t<>(gm, num_blocks);
    }
  }

  assert(false);
  abort();
}

/// Construct a stack cache for the simulation.
/// @param sck The kind of the stack cache requested.
/// @param size The requested size of the stack cache in bytes.
/// @param gm Global memory accessed on stack cache fills/spills.
/// @return An instance of the requested stack cache kind.
static patmos::stack_cache_t &create_stack_cache(patmos::stack_cache_e sck,
                                                 unsigned int size,
                                                 patmos::memory_t &gm)
{
  switch(sck)
  {
    case patmos::SC_IDEAL:
      return *new patmos::ideal_stack_cache_t();
    case patmos::SC_BLOCK:
    {
      // convert size to number of blocks
      unsigned int num_blocks = std::ceil((float)size/
                                    (float)patmos::NUM_STACK_CACHE_BLOCK_BYTES);

      return *new patmos::block_stack_cache_t<>(gm, num_blocks,
                                          patmos::NUM_STACK_CACHE_TOTAL_BLOCKS);
    }
  }

  assert(false);
  abort();
}

int main(int argc, char **argv)
{
  // define command-line options
  boost::program_options::options_description generic_options(
    "Generic options:\n for memory/cache sizes the following units are allowed:"
    " k, m, g, or kb, mb, gb");
  generic_options.add_options()
    ("help,h", "produce help message")
    ("maxc,c", boost::program_options::value<unsigned int>()->default_value(std::numeric_limits<unsigned int>::max(), "inf."), "stop simulation after the given number of cycles")
    ("binary,b", boost::program_options::value<std::string>()->default_value("-"), "binary or elf-executable file (stdin: -)")
    ("output,o", boost::program_options::value<std::string>()->default_value("-"), "output execution trace in file (stdout: -)")
    ("debug", "enable step-by-step debug tracing");

  boost::program_options::options_description memory_options("Memory options");
  memory_options.add_options()
    ("gsize,g", boost::program_options::value<patmos::byte_size_t>()->default_value(patmos::NUM_MEMORY_BYTES), "global memory size in bytes")
    ("gtime,G", boost::program_options::value<unsigned int>()->default_value(0), "access delay to global memory in cycles")
    ("lsize,l", boost::program_options::value<patmos::byte_size_t>()->default_value(patmos::NUM_LOCAL_MEMORY_BYTES), "local memory size in bytes");

  boost::program_options::options_description cache_options("Cache options");
  cache_options.add_options()
    ("dcsize,d", boost::program_options::value<patmos::byte_size_t>()->default_value(patmos::NUM_DATA_CACHE_BYTES), "data cache size in bytes")

    ("scsize,s", boost::program_options::value<patmos::byte_size_t>()->default_value(patmos::NUM_STACK_CACHE_BYTES), "stack cache size in bytes")
    ("sckind,S", boost::program_options::value<patmos::stack_cache_e>()->default_value(patmos::SC_IDEAL), "kind of method cache (ideal, block)")

    ("mcsize,m", boost::program_options::value<patmos::byte_size_t>()->default_value(patmos::NUM_METHOD_CACHE_BYTES), "method cache size in bytes")
    ("mckind,M", boost::program_options::value<patmos::method_cache_e>()->default_value(patmos::MC_IDEAL), "kind of method cache (ideal, lru)");

  boost::program_options::positional_options_description pos;
  pos.add("binary", 1);

  boost::program_options::options_description cmdline_options;
  cmdline_options.add(generic_options).add(memory_options).add(cache_options);

  // process command-line options
  boost::program_options::variables_map vm;
  try
  {
    boost::program_options::store(
                          boost::program_options::command_line_parser(argc, argv)
                            .options(cmdline_options).positional(pos).run(), vm);
    boost::program_options::notify(vm);

    // help message
    if (vm.count("help")) {
      std::cout << cmdline_options << "\n";
      return 1;
    }
  }
  catch(boost::program_options::error &e)
  {
    std::cerr << cmdline_options << "\n" << e.what() << "\n\n";
    return 1;
  }

  // get some command-line  options
  std::string binary(vm["binary"].as<std::string>());
  std::string output(vm["output"].as<std::string>());

  unsigned int gsize = vm["gsize"].as<patmos::byte_size_t>().value();
  unsigned int lsize = vm["lsize"].as<patmos::byte_size_t>().value();
  unsigned int dcsize = vm["dcsize"].as<patmos::byte_size_t>().value();
  unsigned int scsize = vm["scsize"].as<patmos::byte_size_t>().value();
  unsigned int mcsize = vm["mcsize"].as<patmos::byte_size_t>().value();

  unsigned int gtime = vm["gtime"].as<unsigned int>();

  patmos::stack_cache_e sck = vm["sckind"].as<patmos::stack_cache_e>();
  patmos::method_cache_e mck = vm["mckind"].as<patmos::method_cache_e>();

  bool debug_enabled = vm.count("debug");
  unsigned int max_cycle = vm["maxc"].as<unsigned int>();

  // setup simulation framework
  patmos::memory_t &gm = create_global_memory(gtime, gsize);
  patmos::stack_cache_t &sc = create_stack_cache(sck, scsize, gm);
  patmos::method_cache_t &mc = create_method_cache(mck, mcsize, gm);
  patmos::data_cache_t &dc = create_data_cache(dcsize, gm);
  patmos::ideal_memory_t lm(lsize);

  patmos::simulator_t s(gm, lm, dc, mc, sc);

  // open streams
  std::istream &in = patmos::get_stream<std::ifstream>(binary, std::cin);
  std::ostream &out = patmos::get_stream<std::ofstream>(output, std::cout);

  // load input program
  patmos::uword_t entry = 0;
  if (is_elf(in))
    entry = readelf(in, gm, gsize);
  else
    readbin(in, gm, gsize);

  // start execution
  try
  {
    s.run(entry, debug_enabled, max_cycle);
    s.print(out);
  }
  catch (patmos::simulation_exception_t e)
  {
    switch(e.get_kind())
    {
      case patmos::simulation_exception_t::CODE_EXCEEDED:
        std::cerr << boost::format("Method cache size exceeded: %1$08x\n")
                  % e.get_info();
        break;
      case patmos::simulation_exception_t::STACK_EXCEEDED:
        std::cerr << boost::format("Stack size exceeded: %1$08x\n") % s.PC;
        break;
      case patmos::simulation_exception_t::UNMAPPED:
        std::cerr << boost::format("Unmapped memory access: %1$08x: %2$08x\n")
                  % s.PC % e.get_info();
        break;
      case patmos::simulation_exception_t::ILLEGAL:
        std::cerr << boost::format("Illegal instruction: %1$08x: %2$08x\n")
                  % s.PC % e.get_info();
        break;
      case patmos::simulation_exception_t::HALT:
        break;
      default:
        std::cerr << "Unknown simulation error.\n";
    }
  }

  // free memory/cache instances
  // note: no need to free the local memory here.
  delete &gm;
  delete &dc;
  delete &mc;
  delete &sc;

  // free streams
  patmos::free_stream(in, std::cin);
  patmos::free_stream(out, std::cout);

  return 0;
}
