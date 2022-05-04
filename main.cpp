//James Kerrigan; Assignment 3; Language: C++11; Environment: repl.it
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <map>
#include <bitset>
#include <cstdint>
#include <stdio.h>
#include <string.h>
#include <deque> 
using namespace std;

#define NUMMEMORY 65536 /* maximum number of data words in memory */
#define NUMREGS 8 /* number of machine registers */

#define NOOPINSTRUCTION 0x1c00000

enum class Opcode
{
  ADD = 0b000,
  NAND = 0b001,
  LW = 0b010,
  SW = 0b011,
  BEQ = 0b100,
  HALT = 0b110,
  NOOP = 0b111
};

struct IFIDStruct {
    int instr;
    int pcPlus1;
};

struct IDEXStruct {
    int instr;
    int pcPlus1;
    int readRegA;
    int readRegB;
    int offset;
};

struct EXMEMStruct {
    int instr;
    int branchTarget;
    int aluResult;
    int readRegB;
};

struct MEMWBStruct {
    int instr;
    int writeData;
};

struct WBENDStruct {
    int instr;
    int writeData;
};

struct stateStruct {
    int pc;
    int instrMem[NUMMEMORY];
    int dataMem[NUMMEMORY];
    int reg[NUMREGS];
    int numMemory;
    IFIDStruct IFID;
    IDEXStruct IDEX;
    EXMEMStruct EXMEM;
    MEMWBStruct MEMWB;
    WBENDStruct WBEND;
    int cycles; /* number of cycles run so far */
};

class Simulator
{
  public:
  stateStruct state;
  stateStruct newState;
  uint32_t instruction_bits;
  string file_name;
  bool stall;

  Simulator(string file_name)
  {
    this->file_name = file_name;
  }

  void simulator_start()
  {
    ifstream file(file_name, ios_base::in);

    if(!file.is_open()) 
    {
      cout << "error in opening input file\n";
      exit(1);
    }

    state.pc = 0;  
    state.numMemory = 0;
    state.cycles = 0;
    state.IFID.instr = NOOPINSTRUCTION;
    state.IDEX.instr = NOOPINSTRUCTION;
    state.EXMEM.instr = NOOPINSTRUCTION;
    state.MEMWB.instr = NOOPINSTRUCTION;
    state.WBEND.instr = NOOPINSTRUCTION;

    for(int i = 0; i < NUMREGS; i++)
    {
      state.reg[i] = 0;
    }

    for(int i = 0;file >> instruction_bits; i++)
    {
      state.instrMem[i] = instruction_bits;
      state.dataMem[i] = instruction_bits;
      state.numMemory++;
    }

    //for(int i = 0; i < 100; i++)
    while(true)
    {


      printState(&state);
      
      /* check for halt */
      if (opcode(state.MEMWB.instr) == Opcode::HALT) 
      {
          printf("machine halted\n");
          printf("total of %d cycles executed\n", state.cycles);
          exit(0);
      }


      newState = state;
      newState.cycles++;


      /* --------------------- IF stage --------------------- */

      // struct IFIDStruct
      // int instr;
      // int pcPlus1;

      //Checks if simulation requires stalling; used in conjuction with LW
      stall = stall_check(state.IFID.instr);

      //Prevents IF stage from aquiring new instructions if stall is required
      if(!stall)
      {
        //IF Pipeline registers
        newState.IFID.instr = state.instrMem[state.pc];
        newState.IFID.pcPlus1 = state.pc + 1;
        newState.pc++;
      }

      /* --------------------- ID stage --------------------- */

      // struct IDEXStruct
      // int instr;
      // int pcPlus1;
      // int readRegA;
      // int readRegB;
      // int offset;
      
      //NOOP instruction is injected into the ID stage if stall is required
      if(!stall)
      {
        newState.IDEX.instr = state.IFID.instr;
      }

      else newState.IDEX.instr = NOOPINSTRUCTION;

      //ID Pipeline registers
      newState.IDEX.readRegA = state.reg[reg_a(state.IFID.instr)];
      newState.IDEX.pcPlus1 = state.IFID.pcPlus1;
      newState.IDEX.readRegB = state.reg[reg_b(state.IFID.instr)];
      newState.IDEX.offset = signed_offset(state.IFID.instr);
 

      /* --------------------- EX stage --------------------- */

      // struct EXMEMStruct 
      // int instr;
      // int branchTarget;
      // int aluResult;
      // int readRegB;

      //EX Pipeline registers
      newState.EXMEM.instr = state.IDEX.instr;
      newState.EXMEM.branchTarget = state.IDEX.offset + state.IDEX.pcPlus1;
      newState.EXMEM.readRegB = forward_check(state.IDEX.readRegB, 1);
      
      //ALU operation/register; will check if any data requires forwarding
      newState.EXMEM.aluResult = 
      ALU_operation(state.IDEX.instr, forward_check(state.IDEX.readRegA, 0), forward_check(state.IDEX.readRegB, 1), state.IDEX.offset);

      //Stores word into memory
      if(opcode(state.EXMEM.instr) == Opcode::SW)
      {
        newState.dataMem[state.EXMEM.aluResult ] = state.EXMEM.readRegB;
      }
      

      //Resolves BEQ control Hazards but reducing previous instructions to NOOPs and updating the PC
      //if the branch is taken, otherwise the simulation continues from current PC.
      else if(opcode(state.EXMEM.instr) == Opcode::BEQ && state.EXMEM.aluResult == 1)
      {
        newState.pc = state.EXMEM.branchTarget;
        newState.EXMEM.instr = NOOPINSTRUCTION;
        newState.IDEX.instr = NOOPINSTRUCTION;
        newState.IFID.instr = NOOPINSTRUCTION;
      }


      /* --------------------- MEM stage --------------------- */

      // struct MEMWBStruct
      // int instr;
      // int writeData;

      //MEM Pipeline registers
      newState.MEMWB.instr = state.EXMEM.instr;
      if(opcode(state.EXMEM.instr) == Opcode::LW)
      {
        newState.MEMWB.writeData = state.dataMem[state.EXMEM.aluResult];
      }

      else if(opcode(state.EXMEM.instr) == Opcode::ADD || opcode(state.EXMEM.instr) == Opcode::NAND)
      {
        newState.MEMWB.writeData = state.EXMEM.aluResult;
      }


      //Mem Write Operation
      switch (opcode(state.MEMWB.instr))
      {
        case Opcode::ADD:
        case Opcode::NAND:
        newState.reg[signed_offset(state.MEMWB.instr)] = state.MEMWB.writeData;
        break;

        case Opcode::LW:
        newState.reg[reg_b(state.MEMWB.instr)] = state.MEMWB.writeData;
        break;

        case Opcode::SW:
        case Opcode::BEQ:
        case Opcode::HALT:
        case Opcode::NOOP:
        break;
      }


      /* --------------------- WB stage --------------------- */

      // struct WBENDStruct
      // int instr;
      // int writeData;

      //WB Pipeline registers
      newState.WBEND.instr = state.MEMWB.instr;
      newState.WBEND.writeData = state.MEMWB.writeData;


      //End of current cycle; update state for next cycle
      state = newState;
    }
  }

  int ALU_operation(int instr, int regA, int regB, int offset)
  {
      switch (opcode(instr))
      {
        case Opcode::ADD:
        return regA + regB;
        break;

        case Opcode::NAND:
        {
        int a = regA;
        int b = regB;
        int nand_result = 0;

        int top_bit = -1;
        for(int i=31; i>=0; --i) 
        {
          if(((a>>i)&1) || ((b>>i)&1)) 
          {
              top_bit = i;
              break;
          }
        }

        if(top_bit == -1) 
        {
            nand_result = 1;
        } 

        else 
        {
            for(int i=0; i<=top_bit; ++i) 
            {
                if(!((a>>i)&1) || !((b>>i)&1)) 
                {
                    nand_result |= 1 << i;
                }
            }
        }
    
        return nand_result;
        break;
        }

        case Opcode::LW:
        case Opcode::SW:
        return offset + regA;
        break;
        
        case Opcode::BEQ:
        if(regA == regB)
        {
          return 1;
        }
        break;

        case Opcode::HALT:
        case Opcode::NOOP:
        break;
      }

    return 0; //state.EXMEM.aluResult;
  }

  //Forward checking function; compares register A or register B to other Pipeline dest registers
  //to check for data hazards. If detected, a new value will be returned and substituted into 
  //the ALU for the most current IDEX instruction.
  int forward_check(int reg, int reg_type)
  {
    if(reg_type == 0)
    {
      reg_type = reg_a(state.IDEX.instr);
    }

    else reg_type = reg_b(state.IDEX.instr);

      switch (opcode((state.WBEND.instr)))
      {
        case Opcode::ADD:
        case Opcode::NAND:
        if(signed_offset(state.WBEND.instr) == reg_type)
        {
          reg = state.WBEND.writeData;
        }
        break;

        case Opcode::LW:
        if(reg_b(state.WBEND.instr) == reg_type)
        {
          //reg = state.dataMem[state.MEMWB.writeData];
          reg = state.WBEND.writeData;
        }
        break;

        case Opcode::SW:
        case Opcode::BEQ:
        case Opcode::HALT:
        case Opcode::NOOP:
        break;
      }

      switch (opcode((state.MEMWB.instr)))
      {
        case Opcode::ADD:
        case Opcode::NAND:
        if(signed_offset(state.MEMWB.instr) == reg_type)
        {
          reg = state.MEMWB.writeData;
        }
        break;

        case Opcode::LW:
        if(reg_b(state.MEMWB.instr) == reg_type)
        {
          reg = state.MEMWB.writeData;
          //reg = state.dataMem[state.MEMWB.writeData];
        }
        break;

        case Opcode::SW:
        case Opcode::BEQ:
        case Opcode::HALT:
        case Opcode::NOOP:
        break;
      }

       switch (opcode((state.EXMEM.instr)))
      {
        case Opcode::ADD:
        case Opcode::NAND:
        if(signed_offset(state.EXMEM.instr) == reg_type)
        {
          reg = state.EXMEM.aluResult;
        }
        break;

        case Opcode::LW:
        if(signed_offset(state.EXMEM.instr) == reg_b(state.IDEX.instr))
        {
          reg = state.EXMEM.aluResult;
        }
        break;

        case Opcode::SW:
        case Opcode::BEQ:
        case Opcode::HALT:
        case Opcode::NOOP:
        break;
      }

      return reg;
  }

  //Stall checking function; checks for LW data hazard in the IDEX stage
  bool stall_check(int instr)
  {
    if(opcode(state.IDEX.instr) == Opcode::LW)
    {
      if(reg_b(state.IDEX.instr) == reg_a(instr))
      {
        return true;
      }

      else if (reg_b(state.IDEX.instr) == reg_b(instr))
      {
        return true;
      }
    }
    
    return false;
  }

  void
  printState(stateStruct *statePtr)
  {
      int i;
      printf("\n@@@\nstate before cycle %d starts\n", statePtr->cycles);
      printf("\tpc %d\n", statePtr->pc);

      printf("\tdata memory:\n");
    for (i=0; i<statePtr->numMemory; i++) {
        printf("\t\tdataMem[ %d ] %d\n", i, statePtr->dataMem[i]);
    }
      printf("\tregisters:\n");
    for (i=0; i<NUMREGS; i++) {
        printf("\t\treg[ %d ] %d\n", i, statePtr->reg[i]);
    }
      printf("\tIFID:\n");
    printf("\t\tinstruction ");
    printInstruction(statePtr->IFID.instr);
    printf("\t\tpcPlus1 %d\n", statePtr->IFID.pcPlus1);
      printf("\tIDEX:\n");
    printf("\t\tinstruction ");
    printInstruction(statePtr->IDEX.instr);
    printf("\t\tpcPlus1 %d\n", statePtr->IDEX.pcPlus1);
    printf("\t\treadRegA %d\n", statePtr->IDEX.readRegA);
    printf("\t\treadRegB %d\n", statePtr->IDEX.readRegB);
    printf("\t\toffset %d\n", statePtr->IDEX.offset);
      printf("\tEXMEM:\n");
    printf("\t\tinstruction ");
    printInstruction(statePtr->EXMEM.instr);
    printf("\t\tbranchTarget %d\n", statePtr->EXMEM.branchTarget);
    printf("\t\taluResult %d\n", statePtr->EXMEM.aluResult);
    printf("\t\treadRegB %d\n", statePtr->EXMEM.readRegB);
      printf("\tMEMWB:\n");
    printf("\t\tinstruction ");
    printInstruction(statePtr->MEMWB.instr);
    printf("\t\twriteData %d\n", statePtr->MEMWB.writeData);
      printf("\tWBEND:\n");
    printf("\t\tinstruction ");
    printInstruction(statePtr->WBEND.instr);
    printf("\t\twriteData %d\n", statePtr->WBEND.writeData);
  }

  void
  printInstruction(int instr)
  {
    char opcodeString[10];

    switch (opcode(instr))
    {
      case Opcode::HALT:
      strcpy(opcodeString, "halt");
      break;
      case Opcode::NOOP:
      strcpy(opcodeString, "noop");
      break;
      case Opcode::ADD:
      strcpy(opcodeString, "add");
      break;
      case Opcode::NAND:
      strcpy(opcodeString, "nand");
      break;
      case Opcode::LW:
      strcpy(opcodeString, "lw");
      break;
      case Opcode::SW:
      strcpy(opcodeString, "sw");
      break;
      case Opcode::BEQ:
      strcpy(opcodeString, "beq");
      break;
      default:
      strcpy(opcodeString, "data");
      break;
    }

      printf("%s %d %d %d\n", opcodeString, reg_a(instr), reg_b(instr),
    signed_offset(instr));
  }

  
  int HALT_operation()
  {
    return 0;
  }

  void NOOP_operation()
  {
    //do nothing
  }

  Opcode opcode(int num)
  {
    return static_cast<Opcode>((num >> 22) & 0b111);
  }

  uint reg_a(int num)
  {
    return (num >> 19) & 0b111;
  }

  uint reg_b(int num)
  {
    return (num >> 16) & 0b111;
  }

  uint dest_reg(int num)
  {
    return (num) & 0b111;
  }

  int16_t signed_offset(int num)
  {
    return (num & 0xffff);
  }

};

int main(int argc, char *argv[])
{
  if (argc != 2) 
  {
    printf("error: usage: %s <machine-code-file>\n",
        argv[0]);
    exit(1);
  }

  Simulator simulator(argv[1]);
  //Simulator simulator("test");
  simulator.simulator_start();
  
}