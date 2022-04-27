//===-- BPFELFObjectWriter.cpp - BPF ELF Writer ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/BPFMCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"
#include <cstdint>

using namespace llvm;

namespace {

class BPFELFObjectWriter : public MCELFObjectTargetWriter {
public:
  BPFELFObjectWriter(uint8_t OSABI, bool isSolana, bool relocAbs64);
  ~BPFELFObjectWriter() override = default;

protected:
  unsigned getRelocType(MCContext &Ctx, const MCValue &Target,
                        const MCFixup &Fixup, bool IsPCRel) const override;

  bool needsRelocateWithSymbol(const MCSymbol &Sym,
                               unsigned Type) const override;
private:
  bool isSolana;
  bool relocAbs64;
};

} // end anonymous namespace

// Avoid section relocations because the BPF backend can only handle
// section relocations with values (offset into the section containing
// the symbol being relocated).  Forcing a relocation with a symbol
// will result in the symbol's index being used in the .o file instead.
bool BPFELFObjectWriter::needsRelocateWithSymbol(const MCSymbol &Sym,
                                                 unsigned Type) const {
  return isSolana;
}

BPFELFObjectWriter::BPFELFObjectWriter(uint8_t OSABI, bool isSolana,
                                       bool relocAbs64)
    : MCELFObjectTargetWriter(/*Is64Bit*/ true, OSABI, ELF::EM_BPF,
                              /*HasRelocationAddend*/ false),
      isSolana(isSolana), relocAbs64(relocAbs64) {}

unsigned BPFELFObjectWriter::getRelocType(MCContext &Ctx, const MCValue &Target,
                                          const MCFixup &Fixup,
                                          bool IsPCRel) const {
  // determine the type of the relocation
  switch (Fixup.getKind()) {
  default:
    llvm_unreachable("invalid fixup kind!");
  case FK_SecRel_8:
    // LD_imm64 instruction.
    return ELF::R_BPF_64_64;
  case FK_PCRel_4:
    // CALL instruction.
    return ELF::R_BPF_64_32;
  case FK_Data_8:
    return (isSolana && !relocAbs64) ? ELF::R_BPF_64_64 : ELF::R_BPF_64_ABS64;
  case FK_Data_4:
    if (const MCSymbolRefExpr *A = Target.getSymA()) {
      const MCSymbol &Sym = A->getSymbol();

      if (Sym.isDefined()) {
        MCSection &Section = Sym.getSection();
        const MCSectionELF *SectionELF = dyn_cast<MCSectionELF>(&Section);
        assert(SectionELF && "Null section for reloc symbol");

        unsigned Flags = SectionELF->getFlags();

        if (Sym.isTemporary()) {
          // .BTF.ext generates FK_Data_4 relocations for
          // insn offset by creating temporary labels.
          // The reloc symbol should be in text section.
          // Use a different relocation to instruct ExecutionEngine
          // RuntimeDyld not to do relocation for it, yet still to
          // allow lld to do proper adjustment when merging sections.
          if ((Flags & ELF::SHF_ALLOC) && (Flags & ELF::SHF_EXECINSTR))
            return ELF::R_BPF_64_NODYLD32;
        } else {
          // .BTF generates FK_Data_4 relocations for variable
          // offset in DataSec kind.
          // The reloc symbol should be in data section.
          if ((Flags & ELF::SHF_ALLOC) && (Flags & ELF::SHF_WRITE))
            return ELF::R_BPF_64_NODYLD32;
        }
      }
    }
    return isSolana ? ELF::R_BPF_64_32 : ELF::R_BPF_64_ABS32;
  }
}

std::unique_ptr<MCObjectTargetWriter>
llvm::createBPFELFObjectWriter(uint8_t OSABI, bool isSolana) {
  return std::make_unique<BPFELFObjectWriter>(OSABI, isSolana);
}
