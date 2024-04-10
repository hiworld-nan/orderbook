#pragma once

#include "util.h"

static constexpr float EPSILON = (1e-6);
ForceInline bool greator(float lhs, float rhs) { return lhs > rhs + EPSILON; }
ForceInline bool lessThan(float lhs, float rhs) { return lhs + EPSILON < rhs; }
ForceInline bool equal(double x, double y) { return ((x - y) < EPSILON) && ((y - x) < EPSILON); }
