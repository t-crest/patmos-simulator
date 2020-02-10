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
// Read binary files.
//

#include "loader.h"
#include "streams.h"
#include "symbol.h"
#include "endian-conversion.h"

#include <unistd.h>

#include <gelf.h>
#include <libelf.h>

#include <iostream>
#include <cassert>

#include <boost/format.hpp>

using namespace patmos;


loader_t::loader_t(std::istream &is, bool elf)
: Is_ELF(elf)
{
  buf.reserve(1 << 20);

  // read the whole stream.
  while (!is.eof())
  {
    char ibuf[128];

    // read into buffer
    is.read(&ibuf[0], sizeof(ibuf));

    // check how much was read
    std::streamsize count = is.gcount();
    assert(count <= 128);

    // write into main memory
    for(unsigned int i = 0; i < count; i++)
      buf.push_back(ibuf[i]);
  }
}

uword_t loader_t::read_word(uword_t offset)
{
  char *tmp = (char*)&buf[offset];
  uword_t value = *(reinterpret_cast<uword_t*>(tmp));
  return value;
}

elf_loader_t::elf_loader_t(std::istream &is)
: loader_t(is, true)
{
  // check libelf version
  elf_version(EV_CURRENT);

  // open elf binary
  elf = elf_memory((char*)&buf[0], buf.size());
  assert(elf);

  // check file kind
  Elf_Kind ek = elf_kind(elf);
  if (ek != ELF_K_ELF) {
    std::cout << "readelf: ELF file must be of kind ELF.\n";
    exit(1);
  }

  // get elf header
  GElf_Ehdr hdr;
  GElf_Ehdr *tmphdr = gelf_getehdr(elf, &hdr);
  assert(tmphdr);

  if (hdr.e_machine != 0xBEEB) {
    std::cout << "readelf: unsupported architecture: ELF file is not a Patmos ELF file.\n";
    exit(1);
  }

  // check class
  int ec = gelf_getclass(elf);
  if (ec != ELFCLASS32) {
    std::cout << "readelf: unsupported architecture: ELF file is not a 32bit Patmos ELF file.\n";
    exit(1);
  }

  // get entry point
  entry = hdr.e_entry;
}

elf_loader_t::~elf_loader_t()
{
  elf_end(elf);
}


void elf_loader_t::load_symbols(symbol_map_t &symbols, section_list_t &text)
{
  // get sections
  size_t n;
  int ntmp = elf_getshdrnum(elf, &n);
  assert(ntmp == 0);

  // read symbol information
  for(size_t i = 0; i < n; i++)
  {
    Elf_Scn *sec =  elf_getscn (elf, i);
    assert(sec);

    // get section header
    GElf_Shdr shdr;
    gelf_getshdr(sec, &shdr);
    GElf_Shdr *shdrtmp = gelf_getshdr(sec, &shdr);
    assert(shdrtmp);

    if (shdr.sh_type == SHT_SYMTAB)
    {
      int num_entries = shdr.sh_size/shdr.sh_entsize;
      Elf_Data *data = elf_getdata(sec, NULL);
      assert(data);

      for(int j = 0; j != num_entries; j++)
      {
        GElf_Sym sym;
        GElf_Sym *tmpsym = gelf_getsym(data, j, &sym);
        assert(tmpsym);
        char *name = elf_strptr(elf, shdr.sh_link, sym.st_name);
        assert(name);

        // construct a symbol and store it for later use, i.e., the symbol map
        // is queried during simulation to find symbol names associated with
        // addresses.
        patmos::symbol_info_t sym_info(sym.st_value, sym.st_size,
                                       (GELF_ST_TYPE(sym.st_info) == STT_FUNC),
                                       name);
        symbols.add(sym_info);
      }
    }

    if (shdr.sh_flags & SHF_EXECINSTR) {
      text.push_back(section_info_t(shdr.sh_offset, shdr.sh_addr, shdr.sh_size));
    }
  }

  // ensure that the symbol map is sorted.
  symbols.sort();
}

void elf_loader_t::load_to_memory(simulator_t &s, memory_t &m)
{
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
      // copy from the buffer into the main memory
      m.write_peek(s, phdr.p_paddr,
                   reinterpret_cast<patmos::byte_t*>(&buf[phdr.p_offset]),
                   phdr.p_filesz);
      // Zero-initialize rest of segment
      byte_t zero = 0;
      for (unsigned int off = phdr.p_filesz; off < phdr.p_memsz; off++) {
        m.write_peek(s, phdr.p_paddr + off, &zero, 1);
      }
    }
  }
}



void bin_loader_t::load_symbols(symbol_map_t &sym, section_list_t &text)
{
  text.push_back(section_info_t(0, 0, buf.size()));
}

void bin_loader_t::load_to_memory(simulator_t &s, memory_t &m)
{
  m.write_peek(s, 0, reinterpret_cast<patmos::byte_t*>(&buf[0]), buf.size());
}


/// Test whether the file is an elf executable image or a raw binary stream.
/// @param is The input stream to test.
/// @return True, in case the file appears to be an elf executable image, false
/// otherwise.
bool patmos::is_elf(std::istream &is)
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


loader_t *patmos::create_loader(std::istream &is)
{
  // load input program
  if (is_elf(is)) {
    return new elf_loader_t(is);
  } else {
    return new bin_loader_t(is);
  }
}
