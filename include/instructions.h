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
// Definition of simulation functions for every Patmos instruction.
//

#ifndef PATMOS_INSTRUCTIONS_H
#define PATMOS_INSTRUCTIONS_H

#include "endian-conversion.h"
#include "data-cache.h"
#include "instruction.h"
#include "memory.h"
#include "instr-cache.h"
#include "simulation-core.h"
#include "stack-cache.h"
#include "symbol.h"
#include "excunit.h"

#include <ostream>
#include <boost/format.hpp>


namespace patmos
{
  template<typename T>
  T &i_mk()
  {
    static T t;
    return t;
  }

  /// A NOP instruction, which does really nothing, except incrementing the PC.
  class i_nop_t : public instruction_t
  {
  public:
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      os << "nop";
    }

    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print_operands(const simulator_t &s, std::ostream &os, 
	               const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    { }

    /// Returns false, nop is not a flow control instruction
    virtual bool is_flow_control() const
    {
      return false;
    }

    virtual unsigned get_delay_slots(const instruction_data_t &ops) const
    {
      return 0;
    }

    virtual unsigned get_intr_delay_slots(const instruction_data_t &ops) const
    {
      return 0;
    }
    
    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the MW pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
    }
  };

  /// Abstract base class of predicated instructions, i.e., all instructions
  /// that actually do something.
  class i_pred_t : public i_nop_t
  {
  protected:
    /// Read a GPR register at the EX stage.
    /// @param s The parent simulator.
    /// @param op The register operand.
    /// @return The register value, considering by-passing from the EX, and MW
    /// stages.
    static inline word_t read_GPR_EX(const simulator_t &s, GPR_op_t op)
    {
      // Note: Usually we would need forwarding from MW here, but since we
      // execute MW before EX stages and there is no WB stage, data is already
      // commited to the register file and DR stage already got the correct
      // value.
      // Also note that we read out EX bypass from MW, since the intruction
      // writing the bypass has already moved on in this cycle.
      //
      // If we modifiy the simulator to 5 stages or change the order of
      // execution, we need a bypass from MW to EX (but any EX bypass must have
      // precendence!), which must be fetched from WB stage.

      return s.Pipeline[SMW][1].GPR_EX_Rd.get(
             s.Pipeline[SMW][0].GPR_EX_Rd.get(
               op)).get();
    }

    /// Write the result of the EX stage into the bypass.
    static inline void store_GPR_EX_result(simulator_t &s,
                                           instruction_data_t &ops,
                                           GPR_e reg, word_t value)
    {
      if (ops.DR_Pred && reg != r0)
      {
        // store the result by writing it into a by-pass
        ops.GPR_EX_Rd.set(reg, value);
      }
    }

    /// Write the result of the MW stage into the bypass and to the register.
    static inline void store_GPR_MW_result(simulator_t &s,
                                           instruction_data_t &ops,
                                           GPR_e reg, word_t value)
    {
      if (ops.DR_Pred && reg != r0)
      {
        // Store to register file
        s.GPR.set(reg, value);
        // Resetting the EX bypass is not necessary, this instruction will be
        // dropped after this stage.
        //ops.GPR_EX_Rd.reset();
      }
    }

    /// Check if the instruction is a predicated instruction
    static inline bool hasPred(const instruction_data_t &ops) {
      return ops.Pred != p0;
    }
    
    /// Print a predicate to an output stream.
    /// @param os The output stream to print to.
    /// @param pred The predicate.
    /// @param bare If true, print the operand without parentheses,
    ///        also the default operand (always true).
    static inline void printPred(std::ostream &os, const PRR_e pred,
                                 bool bare=false)
    {
      int preg = pred & (NUM_PRR-1);
      bit_t pflag = (pred >> 3) & 1;

      if (bare) {
        if (pflag) os << "!";
        os << "p" << preg;
        return;
      }

      // hide the default predicate operand
      if (preg != p0 || pflag == true) {
        os << boost::format("(%1%p%2%) ") % ((pflag)?"!":" ") % preg;
      } else {
        // omit default predicate
        //os << "(!pN) ";
        os <<   "      ";
      }
    }

    static inline void printGPReg(std::ostream &os, const char* sep, 
          GPR_e reg, word_t val) 
    {
      // Not interested in r0, but this messes up the output format
      // if (reg == r0 && val == 0) return;
      os << boost::format("%1%r%2$-2d = %3$08x") % sep % reg % val;
    }
    
    static inline void printGPReg(std::ostream &os, const char* sep, 
          GPR_e reg, GPR_op_t op, const simulator_t &s) 
    {
      // Note: we are already after EX_commit, so EX bypasses already 
      // contain the new values, but we want the inputs.
      printGPReg(os, sep, reg, read_GPR_EX(s, op));
    }
        
    static inline void printSPReg(std::ostream &os, const char* sep, 
          SPR_e reg, word_t val) 
    {
      os << boost::format("%1%s%2$-2d = %3$08x") % sep % reg % val;
    }
    
    static inline void printPPReg(std::ostream &os, const char* sep,
          PRR_e reg, bool val)
    {
      os << boost::format("%1%p%2% = %3$1u") % sep % reg % val;
    }
    
    static inline void printPPReg(std::ostream &os, const char* sep,
          PRR_e reg, const simulator_t &s)
    {
      printPPReg(os, sep, reg, s.PRR.get(reg).get());
    }
    
    static inline void printSymbol(std::ostream &os, const char* name,
           word_t val,
           const symbol_map_t &symbols)
    {
      std::string sym = symbols.find(val);
      if (!sym.empty()) {
        os << name << ": " << sym;
      }      
    }
           
    
  public:
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      assert(false);
    }
  };

  /// Base class for ALUi or ALUl instructions.
  class i_aluil_t : public i_pred_t
  {
    uint64_t cnt_short_imm;
    uint64_t cnt_short_loads;
    uint64_t cnt_nops;
    uint64_t cnt_inplace_imm5;
    uint64_t cnt_imm5_loads;
    uint64_t cnt_inplace_imm4;
    uint64_t cnt_imm4_loads;
    uint64_t cnt_zero;
  public:
    i_aluil_t() { reset_stats(); }
    
    virtual GPR_e get_dst_reg(const instruction_data_t &ops) const { 
      return ops.OPS.ALUil.Rd;
    }

    virtual GPR_e get_src1_reg(const instruction_data_t &ops) const { 
      return ops.OPS.ALUil.Rs1;
    }
    
    /// Compute the result of an ALUi or ALUl instruction.
    /// @param value1 The value of the first operand.
    /// @param value2 The value of the second operand.
    virtual word_t compute(word_t value1, word_t value2) const = 0;

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.ALUil.Rs1);
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      // compute the result of the ALU instruction
      ops.EX_result = compute(read_GPR_EX(s, ops.DR_Rs1), ops.OPS.ALUil.Imm2);

      store_GPR_EX_result(s, ops, ops.OPS.ALUil.Rd, ops.EX_result);
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the MW pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      store_GPR_MW_result(s, ops, ops.OPS.ALUil.Rd, ops.EX_result);
    }

    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print_operands(const simulator_t &s, std::ostream &os, 
		       const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      // skip NOPs
      if (ops.OPS.ALUil.Rd == r0 && ops.OPS.ALUil.Rs1 == r0 && 
          ops.EX_result == 0 && ops.OPS.ALUil.Imm2 == 0) {
        os << "nop";
        return;
      }
      
      printGPReg(os, "out: ", ops.OPS.ALUil.Rd,  ops.EX_result);
      printGPReg(os, " in: ", ops.OPS.ALUil.Rs1, ops.DR_Rs1, s);

      printSymbol(os, " imm", ops.OPS.ALUil.Imm2, symbols);
    }
    
    virtual void reset_stats() {
      cnt_short_imm = 0;
      cnt_short_loads = 0;
      cnt_nops = 0;
      cnt_imm5_loads = 0;
      cnt_inplace_imm5 = 0;
      cnt_imm4_loads = 0;
      cnt_inplace_imm4 = 0;
      cnt_zero = 0;
    }
    
    virtual void collect_stats(const simulator_t &s, 
                               const instruction_data_t &ops) {
      if (ops.OPS.ALUil.Rd == r0 && 
          ops.OPS.ALUil.Rs1 == r0 &&
          ops.OPS.ALUil.Imm2 == 0) 
      {
        ++cnt_nops;
      }
      else if (!hasPred(ops) && 
               ops.OPS.ALUil.Rs1 == r0 && ops.OPS.ALUil.Imm2 < (1<<4)) 
      {
        ++cnt_imm4_loads;
      }
      else if (!hasPred(ops) && 
               ops.OPS.ALUil.Rs1 == ops.OPS.ALUil.Rd &&
               ops.OPS.ALUil.Imm2 < (1<<4)) 
      {
        ++cnt_inplace_imm4;
      }
      else if (!hasPred(ops) && 
               ops.OPS.ALUil.Rs1 == r0 && ops.OPS.ALUil.Imm2 < (1<<5)) 
      {
        ++cnt_imm5_loads;
      }
      else if (!hasPred(ops) && 
               ops.OPS.ALUil.Rs1 == ops.OPS.ALUil.Rd &&
               ops.OPS.ALUil.Imm2 < (1<<5)) 
      {
        ++cnt_inplace_imm5;
      }
      else if (ops.OPS.ALUil.Rs1 == r0 && ops.OPS.ALUil.Imm2 < (1<<12)) {
        ++cnt_short_loads;
      }
      else if (!hasPred(ops) && 
               ops.OPS.ALUil.Imm2 == 0) {
        ++cnt_zero;
      }
      else if (ops.OPS.ALUil.Imm2 < (1<<12)) {
        ++cnt_short_imm;
      }
    }
    
    virtual void print_stats(const simulator_t &s, std::ostream &os,
                             const symbol_map_t &symbols) const {
      bool printed = false;
      if (cnt_nops) {
        os << "NOPs: " << cnt_nops;
        printed = true;
      }
      if (cnt_inplace_imm5 || cnt_imm5_loads || 
          cnt_inplace_imm4 || cnt_imm4_loads ||
          cnt_short_loads || cnt_zero || cnt_short_imm) 
      {
        if (printed) os << ", ";
        os <<   "Inplace Imm4: " << cnt_inplace_imm4;
        os << ", Load Imm4: " << cnt_imm4_loads;
        os << ", Inplace Imm5: " << cnt_inplace_imm5;
        os << ", Load Imm5: " << cnt_imm5_loads;
        os << ", Short Load Imm: " << cnt_short_loads;
        os << ", Zero Imm: " << cnt_zero;
        os << ", Short Imm: " << cnt_short_imm;
      }
    }
  };

#define ALUil_INSTR(name, expr) \
  class i_ ## name ## _t : public i_aluil_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      printPred(os, ops.Pred); \
      os << boost::format("%1% r%2% = r%3%, %4%") % #name \
          % ops.OPS.ALUil.Rd % ops.OPS.ALUil.Rs1  % ops.OPS.ALUil.Imm2; \
      symbols.print(os, ops.OPS.ALUil.Imm2); \
    } \
    virtual word_t compute(word_t value1, word_t value2) const \
    { \
      return expr; \
    } \
  };

  ALUil_INSTR(addil , value1          +  value2                  )
  ALUil_INSTR(subil , value1          -  value2                  )
  ALUil_INSTR(xoril , value1          ^  value2                  )
  ALUil_INSTR(slil  , value1          << (value2 & 0x1F)         )
  ALUil_INSTR(sril  , (uword_t)value1 >> (uword_t)(value2 & 0x1F))
  ALUil_INSTR(srail , value1          >> (value2 & 0x1F)         )
  ALUil_INSTR(oril  , value1          |  value2                  )
  ALUil_INSTR(andil , value1          &  value2                  )

  ALUil_INSTR(norl   , ~(value1        |  value2)                 )
  ALUil_INSTR(shaddl , (value1 << 1)   +  value2                  )
  ALUil_INSTR(shadd2l, (value1 << 2)   +  value2                  )

  /// Base class for ALUr instructions.
  class i_alur_t : public i_pred_t
  {
  private:
    uint64_t cnt_inplace;
  public:
    i_alur_t() { reset_stats(); }
    
    virtual GPR_e get_dst_reg(const instruction_data_t &ops) const { 
      return ops.OPS.ALUr.Rd;
    }

    virtual GPR_e get_src1_reg(const instruction_data_t &ops) const { 
      return ops.OPS.ALUr.Rs1;
    }
    
    virtual GPR_e get_src2_reg(const instruction_data_t &ops) const { 
      return ops.OPS.ALUr.Rs2;
    }
    
    /// Compute the result of an ALUr instruction.
    /// @param value1 The value of the first operand.
    /// @param value2 The value of the second operand.
    virtual word_t compute(word_t value1, word_t value2) const = 0;

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.ALUr.Rs1);
      ops.DR_Rs2 = s.GPR.get(ops.OPS.ALUr.Rs2);
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      // compute the result of the ALU instruction
      ops.EX_result = compute(read_GPR_EX(s, ops.DR_Rs1),
                              read_GPR_EX(s, ops.DR_Rs2));

      store_GPR_EX_result(s, ops, ops.OPS.ALUr.Rd, ops.EX_result);
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the MW pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      store_GPR_MW_result(s, ops, ops.OPS.ALUr.Rd, ops.EX_result);
    }

    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print_operands(const simulator_t &s, std::ostream &os, 
		       const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printGPReg(os, "out: ", ops.OPS.ALUr.Rd , ops.EX_result);
      printGPReg(os, " in: ", ops.OPS.ALUr.Rs1, ops.DR_Rs1, s);
      printGPReg(os, ", "   , ops.OPS.ALUr.Rs2, ops.DR_Rs2, s);
    }
    
    virtual void reset_stats() {
      cnt_inplace = 0;
    }
    
    virtual void collect_stats(const simulator_t &s, 
                               const instruction_data_t &ops) 
    {
      if (!hasPred(ops) &&
          (ops.OPS.ALUr.Rd == ops.OPS.ALUr.Rs1 || 
           ops.OPS.ALUr.Rd == ops.OPS.ALUr.Rs2))
      {
        cnt_inplace++;
      }
    }

    virtual void print_stats(const simulator_t &s, std::ostream &os,
                             const symbol_map_t &symbols) const 
    {
      if (cnt_inplace) {
        os << "Inplace: " << cnt_inplace;
      }
    }
  };

#define ALUr_INSTR(name, expr) \
  class i_ ## name ## _t : public i_alur_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      printPred(os, ops.Pred); \
      os << boost::format("%1% r%2% = r%3%, r%4%") % #name \
          % ops.OPS.ALUr.Rd % ops.OPS.ALUr.Rs1 % ops.OPS.ALUr.Rs2; \
    } \
    virtual word_t compute(word_t value1, word_t value2) const \
    { \
      return expr; \
    } \
  };

  ALUr_INSTR(add   , value1          +  value2                  )
  ALUr_INSTR(sub   , value1          -  value2                  )
  ALUr_INSTR(xor   , value1          ^  value2                  )
  ALUr_INSTR(sl    , value1          << (value2 & 0x1F)         )
  ALUr_INSTR(sr    , (uword_t)value1 >> (uword_t)(value2 & 0x1F))
  ALUr_INSTR(sra   , value1          >> (value2 & 0x1F)         )
  ALUr_INSTR(or    , value1          |  value2                  )
  ALUr_INSTR(and   , value1          &  value2                  )

  ALUr_INSTR(nor   , ~(value1        |  value2)                 )
  ALUr_INSTR(shadd , (value1 << 1)   +  value2                  )
  ALUr_INSTR(shadd2, (value1 << 2)   +  value2                  )

  /// Base class for ALUm instructions.
  class i_alum_t : public i_pred_t
  {
  public:
    virtual GPR_e get_src1_reg(const instruction_data_t &ops) const { 
      return ops.OPS.ALUm.Rs1;
    }
    
    virtual GPR_e get_src2_reg(const instruction_data_t &ops) const { 
      return ops.OPS.ALUm.Rs2;
    }
    
    /// Compute the result of an ALUr instruction.
    /// @param value1 The value of the first operand.
    /// @param value2 The value of the second operand.
    virtual dword_t compute(word_t value1, word_t value2) const = 0;

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.ALUm.Rs1);
      ops.DR_Rs2 = s.GPR.get(ops.OPS.ALUm.Rs2);
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      // TODO send the operands to a separate MUL unit, simulate some delay

      // compute the result of the ALU instruction
      dword_t result = compute(read_GPR_EX(s, ops.DR_Rs1),
                               read_GPR_EX(s, ops.DR_Rs2));

      ops.EX_mull = (word_t)result;
      ops.EX_mulh = result >> sizeof(word_t)*8;
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the MW pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        s.SPR.set(sl, ops.EX_mull);
        s.SPR.set(sh, ops.EX_mulh);
      }
    }

    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print_operands(const simulator_t &s, std::ostream &os, 
		       const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      // TODO result should actually not be available yet?
      printSPReg(os, "out: ", sl, ops.EX_mull);
      printSPReg(os, ", "   , sh, ops.EX_mulh);
      printGPReg(os, " in: ", ops.OPS.ALUm.Rs1, ops.DR_Rs1, s);
      printGPReg(os, ", "   , ops.OPS.ALUm.Rs2, ops.DR_Rs2, s);
    }

    virtual unsigned get_intr_delay_slots(const instruction_data_t &ops) const {
      return 1;
    }
  };

#define ALUm_INSTR(name, type, stype) \
  class i_ ## name ## _t : public i_alum_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      printPred(os, ops.Pred); \
      os << boost::format("%1% r%2%, r%3%") % #name \
          % ops.OPS.ALUm.Rs1 % ops.OPS.ALUm.Rs2; \
    } \
    virtual dword_t compute(word_t value1, word_t value2) const \
    { \
      return ((type)(stype)value1) * ((type)(stype)value2); \
    } \
  };

  ALUm_INSTR(mul , dword_t, word_t)
  ALUm_INSTR(mulu, udword_t, uword_t)

  /// Base class for ALUc instructions.
  class i_aluc_t : public i_pred_t
  {
  protected:
    uint64_t cnt_cmp_r0;
    uint64_t cnt_cmp_zero;
    uint64_t cnt_cmp_short_negimm;
    uint64_t cnt_cmp_short_imm;
    uint64_t cnt_cmp_short_uimm;
  public:
    i_aluc_t() { reset_stats(); }
    
    virtual GPR_e get_src1_reg(const instruction_data_t &ops) const { 
      return ops.OPS.ALUc.Rs1;
    }
    
    virtual GPR_e get_src2_reg(const instruction_data_t &ops) const { 
      return ops.OPS.ALUc.Rs2;
    }
    
    /// Compute the result of an ALUc instruction.
    /// @param value1 The value of the first operand.
    /// @param value2 The value of the second operand.
    virtual bit_t compute(word_t value1, word_t value2) const = 0;

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.ALUc.Rs1);
      ops.DR_Rs2 = s.GPR.get(ops.OPS.ALUc.Rs2);
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        // compute the result of the comparison instruction
        bit_t result = compute(read_GPR_EX(s, ops.DR_Rs1),
                               read_GPR_EX(s, ops.DR_Rs2));

        // store the result by writing it into the register file.
        s.PRR.set(ops.OPS.ALUc.Pd, result);
      }
    }

    // MW inherited from NOP
    
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print_operands(const simulator_t &s, std::ostream &os, 
		       const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printPPReg(os, "out: ", ops.OPS.ALUc.Pd, s);
      printGPReg(os, ", in: ", ops.OPS.ALUc.Rs1, ops.DR_Rs1, s);
      printGPReg(os, ", "  , ops.OPS.ALUc.Rs2, ops.DR_Rs2, s);
    }    
    
    virtual void reset_stats() {
      cnt_cmp_r0 = 0;
      cnt_cmp_zero = 0;
      cnt_cmp_short_imm = 0;
      cnt_cmp_short_negimm = 0;
      cnt_cmp_short_uimm = 0;
    }
    
    virtual void collect_stats(const simulator_t &s, 
                               const instruction_data_t &ops) 
    {
      if (hasPred(ops)) {
        return;
      }
      if (ops.OPS.ALUc.Rs1 == r0 || 
          ops.OPS.ALUc.Rs2 == r0) 
      {
        cnt_cmp_r0++;
      } else {
        word_t value = std::min(s.GPR.get(ops.OPS.ALUc.Rs1).get(),
                                s.GPR.get(ops.OPS.ALUc.Rs2).get());
        if (value == 0) {
          cnt_cmp_zero++;
        } else if (value < 0 && -value < (1<<5)) {
          cnt_cmp_short_negimm++;
        } else if (value > 0 && value < (1<<5)) {
          cnt_cmp_short_imm++;
        } else if (value > 0 && value < (1<<6)) {
          cnt_cmp_short_uimm++;
        }
      }
    }
    
    virtual void print_stats(const simulator_t &s, std::ostream &os,
                             const symbol_map_t &symbols) const {
      os << boost::format("r0: %d, zero: %d, negimm: %d, short imm: %d, uimm: %d") 
         % cnt_cmp_r0 % cnt_cmp_zero % cnt_cmp_short_negimm % cnt_cmp_short_imm % cnt_cmp_short_uimm;
    }
  };

#define ALUc_INSTR(name, operator) \
  class i_ ## name ## _t : public i_aluc_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      printPred(os, ops.Pred); \
      os << boost::format("%1% p%2% = r%3%, r%4%") % #name \
          % ops.OPS.ALUc.Pd % ops.OPS.ALUc.Rs1 % ops.OPS.ALUc.Rs2; \
    } \
    virtual bit_t compute(word_t value1, word_t value2) const \
    { \
      return value1 operator value2; \
    } \
  };

  ALUc_INSTR(cmpeq , ==)
  ALUc_INSTR(cmpneq, !=)
  ALUc_INSTR(cmplt , <)
  ALUc_INSTR(cmple , <=)

#define ALUcu_INSTR(name, operator) \
  class i_ ## name ## _t : public i_aluc_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      printPred(os, ops.Pred); \
      os << boost::format("%1% p%2% = r%3%, r%4%") % #name \
          % ops.OPS.ALUc.Pd % ops.OPS.ALUc.Rs1 % ops.OPS.ALUc.Rs2; \
    } \
    virtual bit_t compute(word_t value1, word_t value2) const \
    { \
      return ((uword_t)value1) operator ((uword_t)value2); \
    } \
  };

  ALUcu_INSTR(cmpult, <)
  ALUcu_INSTR(cmpule, <=)

  /// Base class for ALUci instructions.
  class i_aluci_t : public i_aluc_t
  {
  public:
    virtual GPR_e get_src1_reg(const instruction_data_t &ops) const { 
      return ops.OPS.ALUci.Rs1;
    }
    
    virtual GPR_e get_src2_reg(const instruction_data_t &ops) const { 
      return r0;
    }
    

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.ALUci.Rs1);
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        // compute the result of the comparison instruction
        bit_t result = compute(read_GPR_EX(s, ops.DR_Rs1),
                               ops.OPS.ALUci.Imm);

        // store the result by writing it into the register file.
        s.PRR.set(ops.OPS.ALUci.Pd, result);
      }
    }

    // MW inherited from NOP
    
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print_operands(const simulator_t &s, std::ostream &os, 
		       const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printPPReg(os, "out: ", ops.OPS.ALUci.Pd, s);
      printGPReg(os, ", in: ", ops.OPS.ALUci.Rs1, ops.DR_Rs1, s);
      printSymbol(os, " imm", ops.OPS.ALUci.Imm, symbols);
    }    
    
    virtual void collect_stats(const simulator_t &s, 
                               const instruction_data_t &ops) 
    {
      if (hasPred(ops)) {
        return;
      }
      if (ops.OPS.ALUci.Rs1 == r0) {
        cnt_cmp_r0++;
      } else {
        word_t value = ops.OPS.ALUci.Imm;
        
        if (value == 0) {
          cnt_cmp_zero++;
        } else if (value < 0 && -value < (1<<5)) {
          cnt_cmp_short_negimm++;
        } else if (value > 0 && value < (1<<5)) {
          cnt_cmp_short_imm++;
        } else if (value > 0 && value < (1<<6)) {
          cnt_cmp_short_uimm++;
        }
      }
    }
  };

#define ALUci_INSTR(name, operator) \
  class i_ ## name ## _t : public i_aluci_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      printPred(os, ops.Pred); \
      os << boost::format("%1% p%2% = r%3%, %4%") % #name \
          % ops.OPS.ALUci.Pd % ops.OPS.ALUci.Rs1 % ops.OPS.ALUci.Imm; \
    } \
    virtual bit_t compute(word_t value1, word_t value2) const \
    { \
      return value1 operator value2; \
    } \
  };

  ALUci_INSTR(cmpieq , ==)
  ALUci_INSTR(cmpineq, !=)
  ALUci_INSTR(cmpilt , <)
  ALUci_INSTR(cmpile , <=)

#define ALUciu_INSTR(name, operator) \
  class i_ ## name ## _t : public i_aluci_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      printPred(os, ops.Pred); \
      os << boost::format("%1% p%2% = r%3%, %4%") % #name \
          % ops.OPS.ALUci.Pd % ops.OPS.ALUci.Rs1 % ops.OPS.ALUci.Imm; \
    } \
    virtual bit_t compute(word_t value1, word_t value2) const \
    { \
      return ((uword_t)value1) operator ((uword_t)value2); \
    } \
  };

  ALUciu_INSTR(cmpiult, <)
  ALUciu_INSTR(cmpiule, <=)

  class i_btest_t : public i_aluc_t
  {
  public:
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printPred(os, ops.Pred);
      os << boost::format("btest p%1% = r%2%, r%3%")
          % ops.OPS.ALUc.Pd % ops.OPS.ALUc.Rs1 % ops.OPS.ALUc.Rs2;
    }

    virtual bit_t compute(word_t value1, word_t value2) const
    {
      return (((uword_t)value1) & (1 << ((uword_t)value2))) != 0;
    }
  };

  class i_btesti_t : public i_aluci_t
  {
  public:
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printPred(os, ops.Pred);
      os << boost::format("btesti p%1% = r%2%, %3%")
          % ops.OPS.ALUci.Pd % ops.OPS.ALUci.Rs1 % ops.OPS.ALUci.Imm;
    }

    virtual bit_t compute(word_t value1, word_t value2) const
    {
      return (((uword_t)value1) & (1 << ((uword_t)value2))) != 0;
    }
  };

  /// Base class for ALUp instructions.
  class i_alup_t : public i_pred_t
  {
  public:
    /// Compute the result of an ALUp instruction.
    /// @param value1 The value of the first operand.
    /// @param value2 The value of the second operand.
    virtual bit_t compute(word_t value1, word_t value2) const = 0;

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Ps1 = s.PRR.get(ops.OPS.ALUp.Ps1).get();
      ops.DR_Ps2 = s.PRR.get(ops.OPS.ALUp.Ps2).get();
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        // compute the result of the ALU instruction
        bit_t result = compute(ops.DR_Ps1, ops.DR_Ps2);

        // store the result by writing it into the register file.
        s.PRR.set(ops.OPS.ALUp.Pd, result);
      }
    }

    // MW inherited from NOP.
    
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print_operands(const simulator_t &s, std::ostream &os, 
		       const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printPPReg(os, "out: ", ops.OPS.ALUp.Pd, s);
      printPPReg(os, ", in: ", ops.OPS.ALUp.Ps1, ops.DR_Ps1);
      printPPReg(os, ", "  , ops.OPS.ALUp.Ps2, ops.DR_Ps2);
    }
  };

#define ALUp_INSTR(name, lval, operator) \
  class i_ ## name ## _t : public i_alup_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      printPred(os, ops.Pred); \
      os << boost::format("%1% p%2% = ") % #name % ops.OPS.ALUp.Pd; \
      printPred(os, ops.OPS.ALUp.Ps1, true); \
      os << ", "; \
      printPred(os, ops.OPS.ALUp.Ps2, true); \
    } \
    virtual bit_t compute(word_t value1, word_t value2) const \
    { \
      return lval == ((value1 operator value2) & 0x1); \
    } \
  };

  ALUp_INSTR(por , 1, |)
  ALUp_INSTR(pand, 1, &)
  ALUp_INSTR(pxor, 1, ^)

  /// Base class for ALUb instructions.
  class i_alub_t : public i_pred_t
  {
  public:
    virtual GPR_e get_dst_reg(const instruction_data_t &ops) const { 
      return ops.OPS.ALUb.Rd;
    }

    virtual GPR_e get_src1_reg(const instruction_data_t &ops) const { 
      return ops.OPS.ALUb.Rs1;
    }
    
    /// Compute the result of an ALUp instruction.
    /// @param value1 The value of the first operand.
    /// @param value2 The value of the second operand.
    virtual word_t compute(word_t value1, word_t value2, bit_t value3) const = 0;

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.ALUb.Rs1);
      ops.DR_Ps1 = s.PRR.get(ops.OPS.ALUb.Ps).get();
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        // compute the result of the instruction
        ops.EX_result = compute(read_GPR_EX(s, ops.DR_Rs1),
                                ops.OPS.ALUb.Imm,
                                ops.DR_Ps1);

        store_GPR_EX_result(s, ops, ops.OPS.ALUb.Rd, ops.EX_result);
      }
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the MW pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      store_GPR_MW_result(s, ops, ops.OPS.ALUb.Rd, ops.EX_result);
    }
    
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print_operands(const simulator_t &s, std::ostream &os, 
                                const instruction_data_t &ops,
                                const symbol_map_t &symbols) const
    {
      printGPReg(os, "out: ", ops.OPS.ALUb.Rd, ops.EX_result);
      printGPReg(os, ", in: ", ops.OPS.ALUb.Rs1, ops.DR_Rs1, s);
      printSymbol(os, " imm", ops.OPS.ALUb.Imm, symbols);
      printPPReg(os, ", "  , ops.OPS.ALUb.Ps, ops.DR_Ps1);
    }
  };

  class i_bcopy_t : public i_alub_t
  {
  public:
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printPred(os, ops.Pred);
      os << boost::format("bcopy r%1% = r%2%, %3%, %4%")
          % ops.OPS.ALUb.Rd % ops.OPS.ALUb.Rs1 % ops.OPS.ALUb.Imm % ops.OPS.ALUb.Ps;
    }
    virtual word_t compute(word_t value1, word_t value2, bit_t value3) const
    {
      return ((value1 & ~(1 << value2)) | ((word_t)value3 << value2));
    }
  };


  /// Move a value from a general purpose register to a special purpose
  /// register.
  class i_spct_t : public i_pred_t
  {
  private:
    uint64_t cnt_nopred;
  public:
    i_spct_t() { reset_stats(); }
    
    virtual GPR_e get_src1_reg(const instruction_data_t &ops) const { 
      return ops.OPS.SPCt.Rs1;
    }
        
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printPred(os, ops.Pred);
      os << boost::format("mts s%1% = r%2%")
                          % ops.OPS.SPCt.Sd % ops.OPS.SPCt.Rs1;
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.SPCt.Rs1);
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred)
      {
        uword_t result = read_GPR_EX(s, ops.DR_Rs1);

        // store the result by writing it into the special purpose register file
        // or resetting the predicate registers
        if (ops.OPS.SPCt.Sd == 0)
        {
          // p0 is always 1, so skip it
          for(unsigned int i = 1; i < NUM_PRR; i++) {
            s.PRR.set ((PRR_e)i, ((result >> i) & 1) == 1);
          }
        }
        else
          s.SPR.set(ops.OPS.SPCt.Sd, result);
      }
    }

    // MW inherited from NOP

    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.

    virtual void print_operands(const simulator_t &s, std::ostream &os, 
		       const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printGPReg(os, "in: ", ops.OPS.SPCt.Rs1, ops.DR_Rs1, s);
    }
    
    virtual void reset_stats() {
      cnt_nopred = 0;
    }
    
    virtual void collect_stats(const simulator_t &s, 
                               const instruction_data_t &ops) 
    {
      if (!hasPred(ops))
      {
        cnt_nopred++;
      }
    }

    virtual void print_stats(const simulator_t &s, std::ostream &os,
                             const symbol_map_t &symbols) const 
    {
      if (cnt_nopred) {
        os << "Nopred: " << cnt_nopred;
      }
    }

  };

  /// Move a value from a special purpose register to a general purpose
  /// register.
  class i_spcf_t : public i_pred_t
  {
    uint64_t cnt_nopred;
    uint64_t cnt_accesses[8];
  public:
    i_spcf_t() { reset_stats(); }
    
    virtual GPR_e get_dst_reg(const instruction_data_t &ops) const { 
      return ops.OPS.SPCf.Rd;
    }

    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printPred(os, ops.Pred);
      os << boost::format("mfs r%1% = s%2%")
                          % ops.OPS.SPCf.Rd % ops.OPS.SPCf.Ss;
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      // Read a value from the special register file or all predicate registers.
      // Note that the stage simulation order implies forwarding from EX to DR.
      if (ops.OPS.SPCf.Ss == 0)
      {
        ops.DR_Ss = 0;
        for(unsigned int i = 0; i < NUM_PRR; i++)
          ops.DR_Ss |= (s.PRR.get((PRR_e)i).get() << i);
      }
      else
        ops.DR_Ss = s.SPR.get(ops.OPS.SPCf.Ss).get();
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      // read the special purpose register, without forwarding
      ops.EX_result = ops.DR_Ss;

      store_GPR_EX_result(s, ops, ops.OPS.SPCf.Rd, ops.EX_result);
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the MW pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      store_GPR_MW_result(s, ops, ops.OPS.SPCf.Rd, ops.EX_result);
    }

    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.

    virtual void print_operands(const simulator_t &s, std::ostream &os, 
		       const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printGPReg(os, "out: ", ops.OPS.SPCf.Rd, ops.EX_result);
    }    
    
    virtual void reset_stats() {
      cnt_nopred = 0;
      for (int i = 0; i < 8; i++) {
        cnt_accesses[i] = 0;
      }
    }

    virtual void collect_stats(const simulator_t &s, 
                               const instruction_data_t &ops) 
    {
      if (!hasPred(ops)) {
        cnt_nopred++;
      }
      if (ops.OPS.SPCf.Ss < 8) {
        cnt_accesses[ops.OPS.SPCf.Ss]++;
      }
    }
    
    virtual void print_stats(const simulator_t &s, std::ostream &os,
                             const symbol_map_t &symbols) const 
    {
      if (cnt_nopred) {
        os << "Nopred: " << cnt_nopred;
      }
      for (int i = 0; i < 8; i++) {
        if (i || cnt_nopred) os << ", ";
        os << boost::format("s%d: %d") % i % cnt_accesses[i];
      }
    }
    
  };

  /// Base class for memory load instructions.
  class i_ldt_t : public i_pred_t
  {
  private:
    uint64_t cnt_zero_offset;
    uint64_t cnt_imm5_offset;
    uint64_t cnt_inplace_imm5;
  public:
    i_ldt_t() { reset_stats(); }
    
    virtual bool is_load() const { return true; }

    virtual GPR_e get_dst_reg(const instruction_data_t &ops) const { 
      return ops.OPS.LDT.Rd;
    }

    virtual GPR_e get_src1_reg(const instruction_data_t &ops) const { 
      return ops.OPS.LDT.Ra;
    }

    /// load the value from memory.
    /// @param s The Patmos simulator executing the instruction.
    /// @param address The address of the memory access.
    /// @param value If the function returns true, the loaded value is returned
    /// here.
    /// @return True if the value was loaded, false if the value is not yet
    /// available and stalling is needed.
    virtual bool load(simulator_t &s, word_t address, word_t &value) const = 0;

    /// peek into the memory to read the value without delay.
    /// @param s The Patmos simulator executing the instruction.
    /// @param address The address of the memory access.
    /// @return The read value.
    virtual word_t peek(simulator_t &s, word_t address) const = 0;
    
    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.LDT.Ra);
      ops.MW_Discard = 0;
    }

    // EX implemented by sub classes

    /// Pipeline function to simulate the behavior of the instruction in
    /// the MW pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred && !ops.MW_Discard)
      {
        // load from memory
        word_t result;
        bool is_available = load(s, ops.EX_Address, result);

        // the value is already available?
        if (is_available)
        {
          
          // TODO this is a very quick hack for now, should be moved to a 
          //      Debug class (see stores).
          if (!s.Debug_mem_address.empty() &&
              s.Debug_mem_address.count(ops.EX_Address))
          {
            std::cerr << "*** Load: " << result 
                      << " = [0x" << std::hex << ops.EX_Address << std::dec << " ";
            s.Symbols.print(std::cerr, ops.EX_Address);
            std::cerr << "] (PC: 0x" 
                      << std::hex << s.PC << std::dec << ", cycle: " << s.Cycle << ")\n";
          }
          
          store_GPR_MW_result(s, ops, ops.OPS.LDT.Rd, result);
          ops.MW_Discard = 1;
        }
        else
        {
          // stall and wait for the memory/cache
          s.pipeline_stall(SMW);
        }
      }
    }

    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    virtual void print_operands(const simulator_t &s, std::ostream &os, 
		       const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      // This might return an old value if there is a store in MW in progress,
      // but this is much better than nothing.
      // TODO this is nasty, that we have to cast const away, peek should be const.
      word_t result = peek(const_cast<simulator_t &>(s), ops.EX_Address);
      
      printGPReg(os, "peek: ", ops.OPS.LDT.Rd, result);
      printGPReg(os, " in: ", ops.OPS.LDT.Ra, ops.DR_Rs1, s);
      os << boost::format(" addr: %1$08x ") % ops.EX_Address;
      symbols.print(os, ops.EX_Address);
    }
    
    virtual void reset_stats() {
      cnt_zero_offset = 0;
      cnt_imm5_offset = 0;
      cnt_inplace_imm5 = 0;
    }
    
    virtual void collect_stats(const simulator_t &s, 
                               const instruction_data_t &ops) 
    {
      if (hasPred(ops)) {
        return;
      }
      if (ops.OPS.LDT.Ra == r0 && ops.OPS.LDT.Imm < (1<<5)) {
        ++cnt_imm5_offset;
      }
      else if (ops.OPS.LDT.Rd == ops.OPS.LDT.Ra && ops.OPS.LDT.Imm < (1<<5)) {
        ++cnt_inplace_imm5;
      }
      else if (ops.OPS.LDT.Imm == 0) {
        ++cnt_zero_offset;
      }
    }
    
    virtual void print_stats(const simulator_t &s, std::ostream &os,
                             const symbol_map_t &symbols) const {
      os << boost::format("imm5: %d, inplace imm5: %d, zero: %d") 
         % cnt_imm5_offset % cnt_inplace_imm5 % cnt_zero_offset;
    }
  };

#define LD_INSTR(name, base, atype, ctype) \
  class i_ ## name ## _t : public i_ldt_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      printPred(os, ops.Pred); \
      os << boost::format("%1% r%2% = [r%3% + %4%]") % #name \
          % ops.OPS.LDT.Rd % ops.OPS.LDT.Ra % ops.OPS.LDT.Imm; \
      symbols.print(os, ops.EX_Address); \
    } \
    virtual void EX(simulator_t &s, instruction_data_t &ops) const \
    { \
      ops.EX_Address = read_GPR_EX(s, ops.DR_Rs1) + ops.OPS.LDT.Imm*sizeof(atype); \
    } \
    virtual bool load(simulator_t &s, word_t address, word_t &value) const \
    { \
      atype tmp=0; \
      if ((address & (sizeof(atype) - 1)) != 0) \
        simulation_exception_t::unaligned(address); \
      bool is_available = base.read_fixed(s, address, tmp, false);    \
      value = (ctype)from_big_endian<big_ ## atype>(tmp); \
      return is_available; \
    } \
    virtual word_t peek(simulator_t &s, word_t address) const \
    { \
      atype tmp=0; \
      base.peek_fixed(s, address, tmp, false);                 \
      return (ctype)from_big_endian<big_ ## atype>(tmp); \
    } \
  };

  LD_INSTR(lws , s.Stack_cache, word_t, word_t)
  LD_INSTR(lhs , s.Stack_cache, hword_t, word_t)
  LD_INSTR(lbs , s.Stack_cache, byte_t, word_t)
  LD_INSTR(lwus, s.Stack_cache, uword_t, uword_t)
  LD_INSTR(lhus, s.Stack_cache, uhword_t, uword_t)
  LD_INSTR(lbus, s.Stack_cache, ubyte_t, uword_t)

  LD_INSTR(lwl , s.Local_memory, word_t, word_t)
  LD_INSTR(lhl , s.Local_memory, hword_t, word_t)
  LD_INSTR(lbl , s.Local_memory, byte_t, word_t)
  LD_INSTR(lwul, s.Local_memory, uword_t, uword_t)
  LD_INSTR(lhul, s.Local_memory, uhword_t, uword_t)
  LD_INSTR(lbul, s.Local_memory, ubyte_t, uword_t)

  LD_INSTR(lwc , s.Data_cache, word_t, word_t)
  LD_INSTR(lhc , s.Data_cache, hword_t, word_t)
  LD_INSTR(lbc , s.Data_cache, byte_t, word_t)
  LD_INSTR(lwuc, s.Data_cache, uword_t, uword_t)
  LD_INSTR(lhuc, s.Data_cache, uhword_t, uword_t)
  LD_INSTR(lbuc, s.Data_cache, ubyte_t, uword_t)

  LD_INSTR(lwm , s.Memory, word_t, word_t)
  LD_INSTR(lhm , s.Memory, hword_t, word_t)
  LD_INSTR(lbm , s.Memory, byte_t, word_t)
  LD_INSTR(lwum, s.Memory, uword_t, uword_t)
  LD_INSTR(lhum, s.Memory, uhword_t, uword_t)
  LD_INSTR(lbum, s.Memory, ubyte_t, uword_t)

  /// Base class for memory store instructions.
  class i_stt_t : public i_pred_t
  {
  private:
    uint64_t cnt_zero_offset;
    uint64_t cnt_imm5_offset;
  public:
    i_stt_t() { reset_stats(); }

    virtual bool is_store() { return true; }
    
    virtual GPR_e get_src1_reg(const instruction_data_t &ops) const { 
      return ops.OPS.STT.Ra;
    }
    
    virtual GPR_e get_src2_reg(const instruction_data_t &ops) const { 
      return ops.OPS.STT.Rs1;
    }

    /// Store the value to memory.
    /// @param s The Patmos simulator executing the instruction.
    /// @param address The address of the memory access.
    /// @param value The value to be stored.
    /// @return True when the value was finally written to the memory, false
    /// if the instruction has to stall.
    virtual bool store(simulator_t &s, word_t address, word_t value) const = 0;

    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.STT.Ra);
      ops.DR_Rs2 = s.GPR.get(ops.OPS.STT.Rs1);
      ops.MW_Discard = 0;
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the MW pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      if (ops.DR_Pred && !ops.MW_Discard)
      {
        // store to memory
        if (!store(s, ops.EX_Address, ops.EX_Rs))
        {
          // we need to stall in order to ensure that the value was actually
          // propagated down to the memory
          s.pipeline_stall(SMW);
        } else {
          
          // TODO this is a very quick hack for now, this should be moved to a
          // s.Debug class, which properly prints out (or not) all the debug related
          // stuff (including traces, RTC events, ..), like
          // s.Debug.print_store(ops.EX_Address, ops.Ex_Rs);
          if (!s.Debug_mem_address.empty() &&
              s.Debug_mem_address.count(ops.EX_Address))
          {
            std::cerr << "*** Store: [0x" << std::hex << ops.EX_Address << std::dec << " ";
            s.Symbols.print(std::cerr, ops.EX_Address);
            std::cerr << "] = " << ops.EX_Rs
            << " (PC: 0x" <<std::hex << s.PC << std::dec << ", cycle: " << s.Cycle << ")\n";
          }
          
          ops.MW_Discard = 1;
        }
      }
    }

    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print_operands(const simulator_t &s, std::ostream &os, 
		       const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printGPReg(os, "in: " , ops.OPS.STT.Ra, ops.DR_Rs1, s);
      printGPReg(os, ", "   , ops.OPS.STT.Rs1, ops.EX_Rs);
      os << boost::format(" addr: %1$08x ") % ops.EX_Address;
      symbols.print(os, ops.EX_Address);
    }
    
    virtual void reset_stats() {
      cnt_zero_offset = 0;
      cnt_imm5_offset = 0;
    }
    
    virtual void collect_stats(const simulator_t &s, 
                               const instruction_data_t &ops) 
    {
      if (hasPred(ops)) {
        return;
      }
      if (ops.OPS.STT.Ra == r0 && ops.OPS.STT.Imm2 < (1<<5)) {
        ++cnt_imm5_offset;
      }
      else if (ops.OPS.STT.Imm2 == 0) {
        ++cnt_zero_offset;
      }
    }
    
    virtual void print_stats(const simulator_t &s, std::ostream &os,
                             const symbol_map_t &symbols) const {
      os << boost::format("imm5: %d, zero: %d") 
         % cnt_imm5_offset % cnt_zero_offset;
    }
  };

#define ST_INSTR(name, base, type) \
  class i_ ## name ## _t : public i_stt_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      printPred(os, ops.Pred); \
      os << boost::format("%1% [r%2% + %3%] = r%4%") % #name \
          % ops.OPS.STT.Ra % ops.OPS.STT.Imm2 % ops.OPS.STT.Rs1; \
      symbols.print(os, ops.EX_Address); \
    } \
    virtual void EX(simulator_t &s, instruction_data_t &ops) const \
    { \
      ops.EX_Address = read_GPR_EX(s, ops.DR_Rs1) + ops.OPS.STT.Imm2*sizeof(type); \
      ops.EX_Rs = read_GPR_EX(s, ops.DR_Rs2); \
    } \
    virtual bool store(simulator_t &s, word_t address, word_t value) const \
    { \
      type big_value = to_big_endian<big_ ## type>((type)value); \
      if ((address & (sizeof(type) - 1)) != 0) \
        simulation_exception_t::unaligned(address); \
      return base.write_fixed(s, address, big_value); \
    } \
  };

  ST_INSTR(sws, s.Stack_cache, word_t)
  ST_INSTR(shs, s.Stack_cache, hword_t)
  ST_INSTR(sbs, s.Stack_cache, byte_t)

  ST_INSTR(swl, s.Local_memory, word_t)
  ST_INSTR(shl, s.Local_memory, hword_t)
  ST_INSTR(sbl, s.Local_memory, byte_t)

  ST_INSTR(swc, s.Data_cache, word_t)
  ST_INSTR(shc, s.Data_cache, hword_t)
  ST_INSTR(sbc, s.Data_cache, byte_t)

  ST_INSTR(swm, s.Memory, word_t)
  ST_INSTR(shm, s.Memory, hword_t)
  ST_INSTR(sbm, s.Memory, byte_t)


  class i_stc_t : public i_pred_t
  {
  protected:
    virtual word_t EX_cache(simulator_t &s, uword_t size,
                            uword_t &stack_spill, uword_t &stack_top) const = 0;
    
    virtual bool MW_cache(simulator_t &s, uword_t size, word_t delta,
                          uword_t stack_spill, uword_t stack_top) const = 0;

    virtual uword_t read_size_EX(simulator_t &s, 
                                 instruction_data_t &ops) const = 0;
    
  public:
    
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      // Get the size argument
      ops.EX_Rs = read_size_EX(s, ops);
      
      // Pointers that will be set to the new stack addresses
      uword_t stack_spill = ops.DR_Ss;
      uword_t stack_top = ops.DR_St;
      
      ops.EX_result = EX_cache(s, ops.EX_Rs, stack_spill, stack_top);

      // Update the special registers already in EX.
      ops.EX_Ss = stack_spill;
      ops.EX_St = stack_top;
      s.SPR.set(ss, stack_spill);
      s.SPR.set(st, stack_top);      
    }
    
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      // Early exit if this instruction is disabled or already finished.
      if (!ops.DR_Pred || ops.MW_Discard) 
        return;
      
      if(!MW_cache(s, ops.EX_Rs, ops.EX_result, ops.EX_Ss, ops.EX_St)) {
        s.pipeline_stall(SMW);
      }
      else {
        ops.MW_Discard = 1;
      }
    }
    
  };
  
  class i_stci_t : public i_stc_t 
  {
  private:
    uint64_t cnt_imm8_offset;
  protected:
    virtual uword_t read_size_EX(simulator_t &s, instruction_data_t &ops) const
    {
      return ops.OPS.STCi.Imm * sizeof(word_t);
    }
    
  public:
    i_stci_t() { reset_stats(); }
    
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Ss = s.SPR.get(ss).get();
      ops.DR_St = s.SPR.get(st).get();
      ops.MW_Discard = 0;
    }
    
    virtual void print_operands(const simulator_t &s, std::ostream &os,
                                const instruction_data_t &ops,
                                const symbol_map_t &symbols) const
    {
      printSPReg(os, "out: ", ss, ops.EX_Ss);
      printSPReg(os, ", "   , st, ops.EX_St);
      printSPReg(os, " in: ", ss, ops.DR_Ss);
      printSPReg(os, ", "   , st, ops.DR_St);
      os << ", size: " << s.Stack_cache.size();
    }
    
    virtual void reset_stats() {
      cnt_imm8_offset = 0;
    }
    
    virtual void collect_stats(const simulator_t &s, 
                               const instruction_data_t &ops) {
      if (!hasPred(ops) && ops.OPS.STCi.Imm < (1<<8)) {
        ++cnt_imm8_offset;
      }
    }
    
    virtual void print_stats(const simulator_t &s, std::ostream &os,
                             const symbol_map_t &symbols) const {
      os << boost::format("imm8: %d") 
         % cnt_imm8_offset;
    }
    
  };

  class i_stcr_t : public i_stc_t 
  {
  protected:
    virtual uword_t read_size_EX(simulator_t &s, instruction_data_t &ops) const
    {
      return read_GPR_EX(s, ops.DR_Rs1);
    }

  public:
    virtual GPR_e get_src1_reg(const instruction_data_t &ops) const { 
      return ops.OPS.STCr.Rs;
    }
    
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Ss = s.SPR.get(ss).get();
      ops.DR_St = s.SPR.get(st).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.STCr.Rs);
      ops.MW_Discard = 0;
    }
    
    virtual void print_operands(const simulator_t &s, std::ostream &os,
                       const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printSPReg(os, "out: ", ss, ops.EX_Ss);
      printSPReg(os, ", "   , st, ops.EX_St);
      printGPReg(os, " in: ", ops.OPS.STCr.Rs, ops.EX_Rs);
      printSPReg(os, ", "   , ss, ops.DR_Ss);
      printSPReg(os, ", "   , st, ops.DR_St);
      os << ", size: " << s.Stack_cache.size();
    }
  };
  
#define STCi_INSTR(name, function) \
  class i_ ## name ## _t : public i_stci_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      printPred(os, ops.Pred); \
      os << #name << " " << ops.OPS.STCi.Imm; \
    } \
    virtual word_t EX_cache(simulator_t &s, uword_t size, \
                            uword_t &stack_spill, uword_t &stack_top) const \
    { \
      return s.Stack_cache.prepare_ ## function(s, size, stack_spill, stack_top); \
    } \
    virtual bool MW_cache(simulator_t &s, uword_t size, word_t delta, \
                          uword_t stack_spill, uword_t stack_top) const \
    { \
      return s.Stack_cache.function(s, size, delta, stack_spill, stack_top); \
    } \
  };

#define STCr_INSTR(name, function) \
  class i_ ## name ## _t : public i_stcr_t \
  { \
  public:\
    virtual void print(std::ostream &os, const instruction_data_t &ops, \
                       const symbol_map_t &symbols) const \
    { \
      printPred(os, ops.Pred); \
      os << #name << " r" << ops.OPS.STCr.Rs; \
    } \
    virtual word_t EX_cache(simulator_t &s, uword_t size, \
                            uword_t &stack_spill, uword_t &stack_top) const \
    { \
      return s.Stack_cache.prepare_ ## function(s, size, stack_spill, stack_top); \
    } \
    virtual bool MW_cache(simulator_t &s, uword_t size, word_t delta, \
                          uword_t stack_spill, uword_t stack_top) const \
    { \
      return s.Stack_cache.function(s, size, delta, stack_spill, stack_top); \
    } \
  };

  STCi_INSTR(sres, reserve)
  STCi_INSTR(sens, ensure)
  STCi_INSTR(sfree, free)
  STCi_INSTR(sspill, spill)

  STCr_INSTR(sensr, ensure)
  STCr_INSTR(sspillr, spill)

  /// Base class for branch, call, and return instructions.
  class i_cfl_t : public i_pred_t
  {
  protected:
    /// Get the return offset from the current base and PC
    uword_t get_offset(uword_t base, uword_t pc) const {
      return pc - base;
    }
    
    /// Get the PC from base and offset
    uword_t get_PC(uword_t base, uword_t offset) const {
      return base + offset;
    }
    
    /// Store the method base address and offset to the respective special
    /// purpose registers.
    /// @param s The Patmos simulator executing the instruction.
    /// @param pred The predicate under which the instruction is executed.
    /// @param base The base address of the current method.
    /// @param pc The current program counter.
    void store_return_address(simulator_t &s, instruction_data_t &ops,
                              bit_t pred, uword_t base, uword_t pc, 
                              word_t address, Pipeline_t stage, 
                              bool is_interrupt) const
    {
      if (pred && !s.is_stalling(stage))
      {
        assert(base <= pc);
        
        // store return info to special registers
        s.SPR.set(is_interrupt ? sxb : srb, base);
        s.SPR.set(is_interrupt ? sxo : sro, get_offset(base, pc));
      }
    }

    /// Perform a function branch/call/return.
    /// Fetch the function into the method cache, stall the pipeline, and set
    /// the program counter.
    /// Has to be executed in the MW stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param pred The predicate under which the instruction is executed.
    /// @param base The base address of the target method.
    /// @param address The target address.
    /// @return returns true when fetching is finished.
    bool fetch_and_dispatch(simulator_t &s, instruction_data_t &ops,
                            bit_t pred, word_t base, word_t address) const
    {
      if (pred)
      {
        // check if the target method is in the cache, otherwise stall until
        // it is loaded.
        if (!s.Instr_cache.load_method(s, base, address - base))
        {
          // stall the pipeline
          s.pipeline_stall(SMW);
        }
        // We could still be stalling due to IF, wait for it to finish
        else if (!s.is_stalling(SMW))
        {
          // set the program counter and base
          s.BASE = base;
          s.nPC = address;
          return true;
        }        
      }
      return false;
    }

    /// Perform a function branch or call.
    /// The function is assumed to be in the method cache, thus simply set the
    /// program counter.
    /// Has to be executed in the EX or MW stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param pred The predicate under which the instruction is executed.
    /// @param base The base address of the target method.
    /// @param address The target address.
    /// @return returns true when fetching is finished.
    bool dispatch(simulator_t &s, instruction_data_t &ops, bit_t pred,
                  word_t base, word_t address) const
    {
      if (pred)
      {
        // assure that the target method is in the cache.
        assert(s.is_stalling(SMW) || s.Instr_cache.is_available(s, base));

        // set the program counter and base
        s.BASE = base;
        s.nPC = address;
        return true;
      }
      return false;
    }
    
    bool push_dbgstack(simulator_t &s, instruction_data_t &ops, bit_t pred,
                       uword_t base, uword_t address, uword_t callee) const 
    {
      // Enter the debug stack just once, and before we actually do the update
      // to avoid any issues with timing and stalling.
      // We need to wait until any IF-stage stalling is complete, so that the
      // debug stack reads out the correct stack pointer values if they are 
      // modified in the delay slot.
      if (pred && !ops.MW_Initialized && !s.is_stalling(SMW)) {
        ops.MW_Initialized = true;
        
        s.Dbg_stack.push(base, get_offset(base, address), callee);
        s.Profiling.enter(callee, s.Cycle);
        
        return true;
      }
      return false;
    }
                        
    bool pop_dbgstack(simulator_t &s, instruction_data_t &ops, bit_t pred,
                      uword_t base, uword_t offset) const
    {
      if (pred && !ops.MW_Initialized && !s.is_stalling(SMW)) {
        ops.MW_Initialized = true;
        
        s.Profiling.leave(s.Cycle);
        s.Dbg_stack.pop(base, offset);
        
        return true;
      }
      return false;
    }
                        
  public:
    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.MW_Initialized = false;
    }

    // EX, MW implemented by sub-classes

    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print_operands(const simulator_t &s, std::ostream &os, 
		       const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printSymbol(os, "PC", ops.EX_Address, symbols);
    }

    /// Returns true because these are all flow control instruction
    virtual bool is_flow_control() const
    {
      return true;
    }
  };

  class i_call_t : public i_cfl_t
  {
  public:
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printPred(os, ops.Pred);
      os << "call" << (ops.OPS.CFLi.D ? " " : "nd ") << ops.OPS.CFLi.UImm;
      symbols.print(os, ops.OPS.CFLi.UImm * sizeof(word_t));
    }
    
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      ops.EX_Address = ops.OPS.CFLi.UImm*sizeof(word_t);
    }
    
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      uword_t ret_pc = ops.OPS.CFLi.D ? s.nPC : s.Pipeline[SEX][0].Address;
      store_return_address(s, ops, ops.DR_Pred, s.BASE, ret_pc,
                           ops.EX_Address, SMW, false);
      
      push_dbgstack(s, ops, ops.DR_Pred, s.BASE, ret_pc, ops.EX_Address);
      
      fetch_and_dispatch(s, ops, ops.DR_Pred, ops.EX_Address, ops.EX_Address);
      if (!ops.OPS.CFLi.D && ops.DR_Pred && !s.is_stalling(SMW))
      {
        s.pipeline_flush(SMW);
      }
    }
    
    virtual bool is_call() const {
      return true;
    }
    
    virtual unsigned get_delay_slots(const instruction_data_t &ops) const {
      return ops.OPS.CFLi.D ? 3 : 0;
    }

    virtual unsigned get_intr_delay_slots(const instruction_data_t &ops) const {
      return 3;
    }
  };

  class i_br_t : public i_cfl_t
  {
  private:
    uint64_t cnt_imm8_jump;
  public:
    i_br_t() { reset_stats(); }
    
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printPred(os, ops.Pred);
      os << "br" << (ops.OPS.CFLi.D ? " " : "nd ") << ops.OPS.CFLi.Imm;
      symbols.print(os, ops.Address + ops.OPS.CFLi.Imm * sizeof(word_t));
    }
    
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      ops.EX_Address = ops.Address + ops.OPS.CFLi.Imm*sizeof(word_t);
      dispatch(s, ops, ops.DR_Pred, s.BASE, ops.EX_Address);
      if (!ops.OPS.CFLi.D && ops.DR_Pred)
      {
        s.pipeline_flush(SEX);
      }
    }
    
    virtual unsigned get_delay_slots(const instruction_data_t &ops) const {
      return ops.OPS.CFLi.D ? 2 : 0;
    }

    virtual unsigned get_intr_delay_slots(const instruction_data_t &ops) const {
      return 2;
    }
    
    virtual void reset_stats() {
      cnt_imm8_jump = 0;
    }
    
    virtual void collect_stats(const simulator_t &s, 
                               const instruction_data_t &ops) {
      if (!hasPred(ops) && ops.OPS.CFLi.UImm < (1<<8)) {
        ++cnt_imm8_jump;
      }
    }
    
    virtual void print_stats(const simulator_t &s, std::ostream &os,
                             const symbol_map_t &symbols) const {
      os << boost::format("imm8 jump: %d") 
         % cnt_imm8_jump;
    }    
  };

  class i_brcf_t : public i_cfl_t
  {
  private:
    uint64_t cnt_imm8_jump;
  public:
    i_brcf_t() { reset_stats(); }
    
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                        const symbol_map_t &symbols) const
    {
      printPred(os, ops.Pred);
      os << "brcf" << (ops.OPS.CFLi.D ? " " : "nd ") << ops.OPS.CFLi.Imm;
      symbols.print(os, ops.OPS.CFLi.UImm * sizeof(word_t));
    }
    
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      ops.EX_Base    = ops.OPS.CFLi.UImm*sizeof(word_t);
      ops.EX_Offset  = 0;
      ops.EX_Address = get_PC(ops.EX_Base, ops.EX_Offset);
    }
    
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      // brcf to address 0? interpret this as a halt.
      if (ops.DR_Pred && ops.EX_Base == 0)
      {
        s.halt();
      }
      else
      {
        fetch_and_dispatch(s, ops, ops.DR_Pred, ops.EX_Address, ops.EX_Address);
        if (!ops.OPS.CFLi.D && ops.DR_Pred)
        {
          s.pipeline_flush(SMW);
        }
      }
    }
    
    virtual unsigned get_delay_slots(const instruction_data_t &ops) const {
      return ops.OPS.CFLi.D ? 3 : 0;
    }

    virtual unsigned get_intr_delay_slots(const instruction_data_t &ops) const {
      return 3;
    }
    
    virtual void reset_stats() {
      cnt_imm8_jump = 0;
    }
    
    virtual void collect_stats(const simulator_t &s, 
                               const instruction_data_t &ops) {
      if (!hasPred(ops) && ops.OPS.CFLi.UImm < (1<<8)) {
        ++cnt_imm8_jump;
      }
    }
    
    virtual void print_stats(const simulator_t &s, std::ostream &os,
                             const symbol_map_t &symbols) const {
      os << boost::format("imm8 jump: %d") 
         % cnt_imm8_jump;
    }        
  };

  class i_trap_t : public i_cfl_t {
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printPred(os, ops.Pred);
      os << "trap " << ops.OPS.CFLi.UImm;
    }

    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.MW_Initialized = false;
    }
    
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      exception_e exc = (exception_e)ops.OPS.CFLi.UImm;
      exception_t isr;
      
      // determine if the exception should be triggered by the trap instruction
      ops.EX_result = ops.DR_Pred && s.Exception_handler.trap(exc, isr);
      
      ops.EX_Base    = isr.Address;
      ops.EX_Offset  = 0;
      ops.EX_Address = get_PC(ops.EX_Base, ops.EX_Offset);
    }
    
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      store_return_address(s, ops, ops.EX_result, s.BASE, 
                           s.Pipeline[SEX][0].Address, ops.EX_Address, 
                           SMW, true);
      
      push_dbgstack(s, ops, ops.EX_result, s.BASE, 
                    s.Pipeline[SEX][0].Address, ops.EX_Address);

      fetch_and_dispatch(s, ops, ops.EX_result, ops.EX_Address, ops.EX_Address);
      
      if (ops.EX_result && !s.is_stalling(SMW))
      {
        s.pipeline_flush(SMW);
      }
    }
        
    virtual unsigned get_delay_slots(const instruction_data_t &ops) const {
      return 0;
    }

    virtual unsigned get_intr_delay_slots(const instruction_data_t &ops) const {
      return 3;
    }
  };
  
  class i_intr_t : public i_cfl_t
  {
  public:
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printPred(os, ops.Pred);
      os << "intr " << ops.OPS.CFLi.UImm;
      symbols.print(os, ops.OPS.CFLi.UImm * sizeof(word_t));
    }

    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      ops.EX_Address = ops.OPS.CFLi.UImm;
      ops.MW_Initialized = false;
    }
    
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      store_return_address(s, ops, ops.DR_Pred, s.BASE, ops.Address, ops.EX_Address, 
                           SMW, true);
      
      push_dbgstack(s, ops, ops.DR_Pred, s.BASE, ops.Address, ops.EX_Address);

      fetch_and_dispatch(s, ops, ops.DR_Pred, ops.EX_Address, ops.EX_Address);
      
      if (ops.DR_Pred && !s.is_stalling(SMW))
      {
        s.pipeline_flush(SMW);
      }
    }
    
    virtual unsigned get_delay_slots(const instruction_data_t &ops) const {
      return 0;
    }

    virtual unsigned get_intr_delay_slots(const instruction_data_t &ops) const {
      return 3;
    }
  };

  class i_halt_t : public i_cfl_t
  {
  public:
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printPred(os, ops.Pred);
      os << "halt";
    }

    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      simulation_exception_t::halt(s.GPR.get(GPR_EXIT_CODE_INDEX).get());
    }
    
    virtual unsigned get_delay_slots(const instruction_data_t &ops) const {
      return 3;
    }

    virtual unsigned get_intr_delay_slots(const instruction_data_t &ops) const {
      return 3;
    }
  };

  
  /// Control flow instructions with implicit register operands.
  class i_cflri_t : public i_cfl_t
  {
  public:
    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.MW_Initialized = false;
    }
  };

  /// Control flow instructions with a single register operand.
  class i_cflrs_t : public i_cfl_t
  {
  public:
    virtual GPR_e get_src1_reg(const instruction_data_t &ops) const { 
      return ops.OPS.CFLrs.Rs;
    }
    
    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.CFLrs.Rs);
      ops.MW_Initialized = false;
    }

    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print_operands(const simulator_t &s, std::ostream &os,
                                const instruction_data_t &ops,
                                const symbol_map_t &symbols) const
    {
      printGPReg(os, "in: ", ops.OPS.CFLrs.Rs, ops.EX_Address);
      os << " ";
      symbols.print(os, ops.EX_Address);
    }
  };

  /// Control flow instructions with two register operands.
  class i_cflrt_t : public i_cfl_t
  {
  public:
    virtual GPR_e get_src1_reg(const instruction_data_t &ops) const { 
      return ops.OPS.CFLrt.Rs1;
    }
    
    virtual GPR_e get_src2_reg(const instruction_data_t &ops) const { 
      return ops.OPS.CFLrt.Rs2;
    }
    
    /// Pipeline function to simulate the behavior of the instruction in
    /// the DR pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void DR(simulator_t &s, instruction_data_t &ops) const
    {
      ops.DR_Pred = s.PRR.get(ops.Pred).get();
      ops.DR_Rs1 = s.GPR.get(ops.OPS.CFLrt.Rs1);
      ops.DR_Rs2 = s.GPR.get(ops.OPS.CFLrt.Rs2);
      ops.MW_Initialized = false;
    }

    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print_operands(const simulator_t &s, std::ostream &os,
                                const instruction_data_t &ops,
                                const symbol_map_t &symbols) const
    {
      printGPReg(os, "in: ", ops.OPS.CFLrt.Rs1, ops.EX_Base);
      printGPReg(os, ", "  , ops.OPS.CFLrt.Rs2, ops.EX_Offset);
      os << boost::format(" addr: %1$08x ") % ops.EX_Address;
      symbols.print(os, ops.EX_Address);
    }
  };

  class i_callr_t : public i_cflrs_t
  {
  public:
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printPred(os, ops.Pred);
      os << "callr" << (ops.OPS.CFLi.D ? " r" : "nd r") << ops.OPS.CFLrs.Rs;
    }
    
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      ops.EX_Address = read_GPR_EX(s, ops.DR_Rs1);
    }
    
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      uword_t ret_pc = ops.OPS.CFLrs.D ? s.nPC : s.Pipeline[SEX][0].Address;
      store_return_address(s, ops, ops.DR_Pred, s.BASE, ret_pc,
                           ops.EX_Address, SMW, false);
      
      push_dbgstack(s, ops, ops.DR_Pred, s.BASE, ret_pc, ops.EX_Address);
      
      fetch_and_dispatch(s, ops, ops.DR_Pred, ops.EX_Address, ops.EX_Address);
      
      if (!ops.OPS.CFLrs.D && ops.DR_Pred && !s.is_stalling(SMW))
      {
        s.pipeline_flush(SMW);
      }
    }
    
    virtual bool is_call() const { 
      return true;
    }
    
    virtual unsigned get_delay_slots(const instruction_data_t &ops) const {
      return ops.OPS.CFLrs.D ? 3 : 0;
    }

    virtual unsigned get_intr_delay_slots(const instruction_data_t &ops) const {
      return 3;
    }
  };

  class i_brr_t : public i_cflrs_t
  {
  public:
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printPred(os, ops.Pred);
      os << "brr" << (ops.OPS.CFLi.D ? " r" : "nd r") << ops.OPS.CFLrs.Rs;
    }
    
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      ops.EX_Address = read_GPR_EX(s, ops.DR_Rs1);
      dispatch(s, ops, ops.DR_Pred, s.BASE, ops.EX_Address);
      if (!ops.OPS.CFLrs.D && ops.DR_Pred)
      {
        s.pipeline_flush(SEX);
      }
    }
    
    virtual unsigned get_delay_slots(const instruction_data_t &ops) const {
      return ops.OPS.CFLrs.D ? 2 : 0;
    }

    virtual unsigned get_intr_delay_slots(const instruction_data_t &ops) const {
      return 2;
    }
  };

  class i_brcfr_t : public i_cflrt_t
  {
  public:
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printPred(os, ops.Pred);
      os << "brcfr" << (ops.OPS.CFLi.D ? " r" : "nd r") << ops.OPS.CFLrt.Rs1 << ", r" << ops.OPS.CFLrt.Rs2;
    }
    
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      ops.EX_Base   = read_GPR_EX(s, ops.DR_Rs1);
      ops.EX_Offset = read_GPR_EX(s, ops.DR_Rs2);;
      ops.EX_Address = get_PC(ops.EX_Base, ops.EX_Offset);
    }
    
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      // brcf to address 0? interpret this as a halt.
      if (ops.DR_Pred && ops.EX_Base == 0)
      {
        s.halt();
      }
      else
      {
        fetch_and_dispatch(s, ops, ops.DR_Pred, ops.EX_Base, ops.EX_Address);
        if (!ops.OPS.CFLrt.D && ops.DR_Pred)
        {
          s.pipeline_flush(SMW);
        }
      }
    }
    
    virtual unsigned get_delay_slots(const instruction_data_t &ops) const {
      return ops.OPS.CFLrt.D ? 3 : 0;
    }

    virtual unsigned get_intr_delay_slots(const instruction_data_t &ops) const {
      return 3;
    }
  };


  /// An instruction for returning from function calls.
  class i_ret_t : public i_cflri_t
  {
  public:
    virtual bool is_return() const { return true; }
    
    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printPred(os, ops.Pred);
      os << "ret" << (ops.OPS.CFLi.D ? "" : "nd");
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the EX pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      ops.EX_Base   = s.SPR.get(srb).get();
      ops.EX_Offset = s.SPR.get(sro).get();
      ops.EX_Address = get_PC(ops.EX_Base, ops.EX_Offset);
    }

    /// Pipeline function to simulate the behavior of the instruction in
    /// the MW pipeline stage.
    /// @param s The Patmos simulator executing the instruction.
    /// @param ops The operands of the instruction.
    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      // returning to address 0? interpret this as a halt.
      if (ops.DR_Pred && ops.EX_Base == 0)
      {
        s.halt();
      }
      else if (ops.DR_Pred)
      {
        pop_dbgstack(s, ops, ops.DR_Pred, ops.EX_Base, ops.EX_Offset);
        
        fetch_and_dispatch(s, ops, ops.DR_Pred, ops.EX_Base, ops.EX_Address);

        if (!ops.OPS.CFLri.D && ops.DR_Pred)
        {
          s.pipeline_flush(SMW);
        }
      }
    }

    /// Print the instruction to an output stream.
    /// @param os The output stream to print to.
    /// @param ops The operands of the instruction.
    /// @param symbols A mapping of addresses to symbols.
    virtual void print_operands(const simulator_t &s, std::ostream &os,
                                const instruction_data_t &ops,
                                const symbol_map_t &symbols) const
    {
      printSPReg(os, "in: ", srb, ops.EX_Base);
      printSPReg(os, ", "  , sro, ops.EX_Offset);
      os << boost::format(" addr: %1$08x ") % ops.EX_Address;
      symbols.print(os, ops.EX_Address);
    }
    
    virtual unsigned get_delay_slots(const instruction_data_t &ops) const {
      return ops.OPS.CFLri.D ? 3 : 0;
    }

    virtual unsigned get_intr_delay_slots(const instruction_data_t &ops) const {
      return 3;
    }
  };
  
  class i_xret_t : public i_cflri_t {
  public:
    virtual bool is_return() const { return true; }

    virtual void print(std::ostream &os, const instruction_data_t &ops,
                       const symbol_map_t &symbols) const
    {
      printPred(os, ops.Pred);
      os << "xret" << (ops.OPS.CFLi.D ? "" : "nd");
    }

    virtual void EX(simulator_t &s, instruction_data_t &ops) const
    {
      ops.EX_Base   = s.SPR.get(sxb).get();
      ops.EX_Offset = s.SPR.get(sxo).get();
      ops.EX_Address = get_PC(ops.EX_Base, ops.EX_Offset);
    }

    virtual void MW(simulator_t &s, instruction_data_t &ops) const
    {
      // returning to address 0? interpret this as an error
      if (ops.DR_Pred && ops.EX_Address == 0)
      {
        simulation_exception_t::illegal_pc("Returning from xret to address 0!");
      }
      else if (ops.DR_Pred)
      {
        // We re-enable interrupts *once*, and *before* we start fetching from
        // M$, so that we can abort fetching if there is another interrupt pending
        // TODO this is nasty that we use pop_dbgstack to check if this is the first call of MW
        if (pop_dbgstack(s, ops, ops.DR_Pred, ops.EX_Base, ops.EX_Offset)) {          
          s.Exception_handler.resume();
        }

        // TODO maybe do not even fetch return function if there is an 
        // interrupt pending
        fetch_and_dispatch(s, ops, ops.DR_Pred, ops.EX_Base, ops.EX_Address);

        if (!ops.OPS.CFLri.D && ops.DR_Pred)
        {
          s.pipeline_flush(SMW);
        }
      }
    }

    virtual void print_operands(const simulator_t &s, std::ostream &os,
                                const instruction_data_t &ops,
                                const symbol_map_t &symbols) const
    {
      printSPReg(os, "in: ", sxb, ops.EX_Base);
      printSPReg(os, ", "  , sxo, ops.EX_Offset);
      os << boost::format(" addr: %1$08x ") % ops.EX_Address;
      symbols.print(os, ops.EX_Address);
    }
    
    virtual unsigned get_delay_slots(const instruction_data_t &ops) const {
      return ops.OPS.CFLri.D ? 3 : 0;
    }

    virtual unsigned get_intr_delay_slots(const instruction_data_t &ops) const {
      return 3;
    }
  };
}

#endif // PATMOS_INSTRUCTIONS_H

