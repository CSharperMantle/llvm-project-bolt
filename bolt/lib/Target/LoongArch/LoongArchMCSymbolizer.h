//===- bolt/Target/LoongArch/LoongArchMCSymbolizer.h -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef BOLT_TARGET_LOONGARCH_LOONGARCHMCSYMBOLIZER_H
#define BOLT_TARGET_LOONGARCH_LOONGARCHMCSYMBOLIZER_H

#include "bolt/Core/BinaryFunction.h"
#include "llvm/MC/MCDisassembler/MCSymbolizer.h"
#include <optional>

namespace llvm {
namespace bolt {

class LoongArchMCSymbolizer : public MCSymbolizer {
protected:
  BinaryFunction &Function;

  /// Modify relocation \p Rel based on type of the relocation and the
  /// instruction it was applied to. Return the new relocation info, or
  /// std::nullopt if the relocation should be ignored, e.g. in the case the
  /// instruction was modified by the linker.
  std::optional<Relocation> adjustRelocation(const Relocation &Rel,
                                             const MCInst &Inst) const;

public:
  LoongArchMCSymbolizer(BinaryFunction &Function)
      : MCSymbolizer(*Function.getBinaryContext().Ctx, nullptr),
        Function(Function) {}

  LoongArchMCSymbolizer(const LoongArchMCSymbolizer &) = delete;
  LoongArchMCSymbolizer &operator=(const LoongArchMCSymbolizer &) = delete;
  ~LoongArchMCSymbolizer() override;

  bool tryAddingSymbolicOperand(MCInst &Inst, raw_ostream &CStream,
                                int64_t Value, uint64_t Address, bool IsBranch,
                                uint64_t Offset, uint64_t OpSize,
                                uint64_t InstSize) override;

  void tryAddingPcLoadReferenceComment(raw_ostream &CStream, int64_t Value,
                                       uint64_t Address) override;
};

} // namespace bolt
} // namespace llvm

#endif
