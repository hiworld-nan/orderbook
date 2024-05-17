#pragma once

#include "util.h"

static constexpr float EPSILON = (1e-6);
ForceInline HintHot bool greator(float lhs, float rhs) { return lhs > rhs + EPSILON; }
ForceInline HintHot bool lessThan(float lhs, float rhs) { return lhs + EPSILON < rhs; }
ForceInline HintHot bool equal(double x, double y) { return ((x - y) < EPSILON) && ((y - x) < EPSILON); }
