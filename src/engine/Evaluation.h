#pragma once
#include "../chess/Position.h"
#include "../nnue/NNUE.h"
namespace uc {
NNUE& network();
int evaluate(const Position& p);
int evaluate(const Position& p,const NNUE::Accumulator& acc);
}
