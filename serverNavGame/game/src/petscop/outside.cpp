#include "petscop/outside.hpp"

#include <ctime>

namespace petscop {

long long nowSeconds() { return static_cast<long long>(std::time(nullptr)); }

int hoursBetween(long long from, long long to, int cap) {
  if (from <= 0 || to <= from) return 0;

  const long long hours = (to - from) / 3600;
  if (hours <= 0) return 0;
  if (cap > 0 && hours > cap) return cap;
  return static_cast<int>(hours);
}

}  // namespace petscop
