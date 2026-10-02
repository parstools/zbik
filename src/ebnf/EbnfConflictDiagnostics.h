#pragma once

#include <string>

#include "EbnfToBnf.h"
#include "lr/Action.h"
#include "lr/LRkDfa.h"

namespace zbik {

// Describes both reductions and the items responsible for a competing shift,
// mapping every generated BNF rule back to its source EBNF alternative.
[[nodiscard]] std::string dumpEbnfConflict(const EbnfGrammarSpec &specification, const EbnfConversionResult &conversion,
                                           const LRkDfa &dfa, const Conflict &conflict);

} // namespace zbik
