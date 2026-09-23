#pragma once
#include "../chess/Position.h"
#include "../nnue/NNUE.h"
#include <ostream>

namespace uc {

NNUE& network();

int classicalEval(const Position& p);

enum class EvalMode { NNUE, Classical };
void setEvalMode(EvalMode mode);
EvalMode evalMode();

// Human-readable breakdown of classical terms (White POV then STM-relative total).
void printClassicalEval(const Position& p, std::ostream& os);

int evaluate(const Position& p);
int evaluate(const Position& p, const NNUE::Accumulator& acc);

}  // namespace uc
