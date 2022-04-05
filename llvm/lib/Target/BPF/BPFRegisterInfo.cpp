//===-- BPFRegisterInfo.cpp - BPF Register Information ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the BPF implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "BPFRegisterInfo.h"
#include "BPF.h"
#include "BPFSubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/Support/ErrorHandling.h"

#define GET_REGINFO_TARGET_DESC
#include "BPFGenRegisterInfo.inc"
using namespace llvm;

unsigned BPFRegisterInfo::FrameLength = 512;

BPFRegisterInfo::BPFRegisterInfo()
    : BPFGenRegisterInfo(BPF::R0) {}

const MCPhysReg *
BPFRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  return CSR_SaveList;
}

BitVector BPFRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  markSuperRegs(Reserved, BPF::W10); // [W|R]10 is read only frame pointer
  markSuperRegs(Reserved, BPF::W11); // [W|R]11 is pseudo stack pointer
  return Reserved;
}

static void WarnSize(int Offset, MachineFunction &MF, DebugLoc& DL)
{
  static Function *OldMF = nullptr;
  if (&(MF.getFunction()) == OldMF) {
    return;
  }
  OldMF = &(MF.getFunction());
  int MaxOffset = -1 * BPFRegisterInfo::FrameLength;
  if (Offset <= MaxOffset) {
    if (MF.getSubtarget<BPFSubtarget>().isSolana()) {
      dbgs() << "Error:";
      if (DL) {
        dbgs() << " ";
        DL.print(dbgs());
      }
      dbgs() << " Function " << MF.getFunction().getName()
             << " Stack offset of " << -Offset << " exceeded max offset of "
             << -MaxOffset << " by " << MaxOffset - Offset
             << " bytes, please minimize large stack variables\n";
    } else {
      DiagnosticInfoUnsupported DiagStackSize(
          MF.getFunction(),
          "BPF stack limit of 512 bytes is exceeded. "
          "Please move large on stack variables into BPF per-cpu array map.\n",
          DL, DiagnosticSeverity::DS_Error);
      MF.getFunction().getContext().diagnose(DiagStackSize);
    }
  }
}

void BPFRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                          int SPAdj, unsigned FIOperandNum,
                                          RegScavenger *RS) const {
  assert(SPAdj == 0 && "Unexpected");

  unsigned i = 0;
  MachineInstr &MI = *II;
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  DebugLoc DL = MI.getDebugLoc();

  if (!DL)
    /* try harder to get some debug loc */
    for (auto &I : MBB)
      if (I.getDebugLoc()) {
        DL = I.getDebugLoc().getFnDebugLoc();
        break;
      }

  while (!MI.getOperand(i).isFI()) {
    ++i;
    assert(i < MI.getNumOperands() && "Instr doesn't have FrameIndex operand!");
  }

  Register FrameReg = getFrameRegister(MF);
  int FrameIndex = MI.getOperand(i).getIndex();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();

  if (MI.getOpcode() == BPF::MOV_rr) {
    int Offset = MF.getFrameInfo().getObjectOffset(FrameIndex);

    if (!MF.getSubtarget<BPFSubtarget>().getHasDynamicFrames()) {
      WarnSize(Offset, MF, DL);
    }
    MI.getOperand(i).ChangeToRegister(FrameReg, false);
    Register reg = MI.getOperand(i - 1).getReg();
    BuildMI(MBB, ++II, DL, TII.get(BPF::ADD_ri), reg)
        .addReg(reg)
        .addImm(Offset);
    return;
  }

  int Offset = MF.getFrameInfo().getObjectOffset(FrameIndex) +
               MI.getOperand(i + 1).getImm();

  if (!isInt<32>(Offset))
    llvm_unreachable("bug in frame offset");

  if (!MF.getSubtarget<BPFSubtarget>().getHasDynamicFrames()) {
    WarnSize(Offset, MF, DL);
  }

  if (MI.getOpcode() == BPF::FI_ri) {
    // architecture does not really support FI_ri, replace it with
    //    MOV_rr <target_reg>, frame_reg
    //    ADD_ri <target_reg>, imm
    Register reg = MI.getOperand(i - 1).getReg();

    BuildMI(MBB, ++II, DL, TII.get(BPF::MOV_rr), reg)
        .addReg(FrameReg);
    BuildMI(MBB, II, DL, TII.get(BPF::ADD_ri), reg)
        .addReg(reg)
        .addImm(Offset);

    // Remove FI_ri instruction
    MI.eraseFromParent();
  } else {
    MI.getOperand(i).ChangeToRegister(FrameReg, false);
    MI.getOperand(i + 1).ChangeToImmediate(Offset);
  }
}

Register BPFRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return BPF::R10;
}
