#pragma once
#include <stdio.h>
#include <stdlib.h>

namespace cos6502 
{
using Byte = unsigned char;
using Word = unsigned short;

using u32 = unsigned int;
using s32 = signed int;

struct Mem;
struct CPU;
}

struct cos6502::Mem {
    static constexpr u32 MAX_MEM = 1024 * 64;
    Byte Data[MAX_MEM];

    void Initialise() {
        for (u32 i = 0; i < MAX_MEM; ++i)
            Data[i] = 0;
    }

    Byte operator[](u32 address) const {
        return Data[address];
    }

    Byte& operator[](u32 address) {
        return Data[address];
    }

};

struct cos6502::CPU {
    Word PC;        // program counter
    Byte SP;        // stack pointer

    Byte A, X, Y;   // registers

    Byte C : 1;     // status flags
    Byte Z : 1;
    Byte I : 1;
    Byte D : 1;
    Byte B : 1;
    Byte V : 1;
    Byte N : 1;

    void Reset(Word pc, Mem& memory) {
        PC = pc;
        SP = 0xFF;
        C = Z = I = D = B = V = N = 0;
        A = X = Y = 0;
        memory.Initialise();
    }

    Byte FetchByte(s32& cycles, Mem const& memory) {
        Byte data = memory[PC++];
        --cycles;
        return data;
    }

    Word FetchWord(s32& cycles, Mem const& memory) {
        // 6502 is little endian;
        Word data = memory[PC++];
        data |= (memory[PC++] << 8);
        cycles -= 2;
        return data;
    }

    Byte ReadByte(s32& cycles, Word addr, Mem const& memory) {
        Byte data = memory[addr];
        --cycles;
        return data;
    }

    Word ReadWord(s32& cycles, Word addr, Mem const& memory) {
        Byte loByte = ReadByte(cycles, addr, memory);
        Byte hiByte = ReadByte(cycles, addr + 1, memory);
        return loByte | (hiByte << 8);
    }

    void WriteByte(Byte value, s32& cycles, Word addr, Mem& memory) {
        memory[addr] = value;
        --cycles;
    }

    void WriteWord(Word value, s32& cycles, u32 address, Mem& memory) {
        memory.Data[address] = value & 0xFF;
        memory.Data[address + 1] = (value >> 8);
        cycles -= 2;
    }

    Word SPToAddress() const {
        return 0x0100 + SP;
    }

    void PushPCToStack(s32& cycles, Mem& memory) {
        WriteWord(PC, cycles, SPToAddress(), memory);
        SP -= 2;
    }

    Word PopWordFromStack(s32& cycles, Mem& memory) {
        SP += 2;
        Word addr = ReadWord(cycles, SPToAddress(), memory);
        --cycles;
        return addr;
    }

    void LoadRegisterSetStatus(Byte reg) {
        Z = (reg == 0);
        N = (reg & 0b10000000) > 0;
    }

    static constexpr Byte
        // LDA
        INS_LDA_IM = 0xA9,
        INS_LDA_ZP = 0xA5,
        INS_LDA_ZPX = 0xB5,
        INS_LDA_ABS = 0xAD,
        INS_LDA_ABSX = 0xBD,
        INS_LDA_ABSY = 0xB9,
        INS_LDA_INDX = 0xA1,
        INS_LDA_INDY = 0xB1,
        // LDX
        INS_LDX_IM = 0xA2,
        INS_LDX_ZP = 0xA6,
        INS_LDX_ZPY = 0xB6,
        INS_LDX_ABS = 0xAE,
        INS_LDX_ABSY = 0xBE,
        // LDY
        INS_LDY_IM = 0xA0,
        INS_LDY_ZP = 0xA4,
        INS_LDY_ZPX = 0xB4,
        INS_LDY_ABS = 0xAC,
        INS_LDY_ABSX = 0xBC,
        // STA
        INS_STA_ZP = 0x85,
        INS_STA_ZPX = 0x95,
        INS_STA_ABS = 0x8D,
        INS_STA_ABSX = 0x9D,
        INS_STA_ABSY = 0x99,
        INS_STA_INDX = 0x81,
        INS_STA_INDY = 0x91,
        // STX
        INS_STX_ZP = 0x86,
        INS_STX_ZPY = 0x96,
        INS_STX_ABS = 0x8E,
        // STY
        INS_STY_ZP = 0x84,
        INS_STY_ZPX = 0x94,
        INS_STY_ABS = 0x8C,
        // Jumps And Calls
        INS_JSR = 0x20,
        INS_RTS = 0x60
        ;

    s32 Execute(s32 cycles, Mem& memory);

    Word AddrZeroPage(s32& cycles, Mem const& memory) {
        Byte zeroPageAddr = FetchByte(cycles, memory);
        return zeroPageAddr;
    }

    Word AddrZeroPageXY(s32& cycles, Byte regXY, Mem const& memory) {
        Byte addr = FetchByte(cycles, memory);
        addr += regXY;
        --cycles;
        return addr;
    }

    Word AddrAbsolute(s32& cycles, Mem const& memory) {
        Word addr = FetchWord(cycles, memory);
        return addr;
    }

    Word AddrAbsoluteXY(s32& cycles, Byte regXY, Mem const& memory) {
        Word absAddr = FetchWord(cycles, memory);
        Word addr = absAddr + regXY;
        if (addr - absAddr >= 0xFF) // crosses page boundary
            --cycles;
        return addr;
    }

    Word AddrAbsoluteXY_5(s32& cycles, Byte regXY, Mem const& memory) {
        Word absAddr = FetchWord(cycles, memory);
        Word addr = absAddr + regXY;
        --cycles;
        return addr;
    }

    Word AddrIndirectX(s32& cycles, Mem const& memory) {
        Byte zpAddr = FetchByte(cycles, memory);
        zpAddr += X;
        --cycles;
        Word effectiveAddr = ReadWord(cycles, zpAddr, memory);
        return effectiveAddr;
    }

    Word AddrIndirectY(s32& cycles, Mem const& memory) {
        Byte zpAddr = FetchByte(cycles, memory);
        Word effectiveAddr = ReadWord(cycles, zpAddr, memory);
        Word effectiveAddrY = effectiveAddr + Y;
        if (effectiveAddrY - effectiveAddr >= 0xFF) // crosses page boundary
            --cycles;
        return effectiveAddrY;
    }

    Word AddrIndirectY_6(s32& cycles, Mem const& memory) {
        Byte zpAddr = FetchByte(cycles, memory);
        Word effectiveAddr = ReadWord(cycles, zpAddr, memory);
        Word effectiveAddrY = effectiveAddr + Y;
        --cycles;
        return effectiveAddrY;
    }

};
