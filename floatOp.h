#pragma once

static constexpr float EPSILON = (1e-6);
inline bool greator(float lhs, float rhs) { return lhs > rhs + EPSILON; }
inline bool lessThan(float lhs, float rhs) { return lhs + EPSILON < rhs; }
inline bool equal(double x, double y) { return ((x - y) < EPSILON) && ((y - x) < EPSILON); }
