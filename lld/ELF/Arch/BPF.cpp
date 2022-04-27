//===- BPF.cpp ------------------------------------------------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "InputFiles.h"
#include "Symbols.h"
#include "Target.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm/Object/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;

namespace lld {
namespace elf {

namespace {
class BPF final : public TargetInfo {
public:
  BPF();
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  RelType getDynRel(RelType type) const override;
  int64_t getImplicitAddend(const uint8_t *buf, RelType type) const override;
  void relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const override;
};
} // namespace

BPF::BPF() {
  noneRel = R_BPF_NONE;
  relativeRel = R_BPF_64_RELATIVE;
  symbolicRel = R_BPF_64_64;
}

RelExpr BPF::getRelExpr(RelType type, const Symbol &s,
                        const uint8_t *loc) const {
  switch (type) {
    case R_BPF_64_32:
      return R_PC;
    case R_BPF_64_64:
    case R_BPF_64_ABS64:
      return R_ABS;
    default:
      error(getErrorLocation(loc) + "unrecognized reloc " + toString(type));
  }
  return R_NONE;
}

RelType BPF::getDynRel(RelType type) const {
  switch (type) {
    case R_BPF_64_ABS64:
        // R_BPF_64_ABS64 is symbolic like R_BPF_64_64, which is set as our
        // symbolicRel in the constructor. Return R_BPF_64_64 here so that if
        // the symbol isn't preemptible, we emit a _RELATIVE relocation instead
        // and skip emitting the symbol.
        //
        // See https://github.com/solana-labs/llvm-project/blob/6b6aef5dbacef31a3c7b3a54f7f1ba54cafc7077/lld/ELF/Relocations.cpp#L1179
        return R_BPF_64_64;
    default:
        return type;
  }
}

int64_t BPF::getImplicitAddend(const uint8_t *buf, RelType type) const {
  return 0;
}

void BPF::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  switch (rel.type) {
    case R_BPF_64_32: {
      // Relocation of a symbol
      write32le(loc + 4, ((val - 8) / 8) & 0xFFFFFFFF);
      break;
    }
    case R_BPF_64_64: {
      // Relocation of a lddw instruction
      // 64 bit address is divided into the imm of this and the following
      // instructions, lower 32 first.
      write32le(loc + 4, val & 0xFFFFFFFF);
      write32le(loc + 8 + 4, val >> 32);
      break;
    }
    case R_BPF_64_ABS64: {
      // The relocation type is used for normal 64-bit data. The
      // actual to-be-relocated data is stored at r_offset and the
      // read/write data bitsize is 64 (8 bytes).
      write64le(loc, val);
      break;
    }
    default:
      error(getErrorLocation(loc) + "unrecognized reloc " + toString(rel.type));
  }
}

TargetInfo *getBPFTargetInfo() {
  static BPF target;
  return &target;
}

} // namespace elf
} // namespace lld
