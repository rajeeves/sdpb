#pragma once

#include <El.hpp>
#include <ostream>

// Job for calculating submatrix Q_IJ = P_I^T P_J modulo given prime
// I and J are contiguous ranges, e.g. I=[0,3), J=[6,9)
struct Blas_Job
{
  struct Cost
  {
    size_t elements;
    size_t blas_calls;
    Cost();
    Cost(size_t elements, size_t blas_calls);
    bool operator<(const Cost &rhs) const;
    Cost operator+(Cost const &other) const;
    Cost &operator+=(const Cost &other);
    friend std::ostream &operator<<(std::ostream &os, const Cost &cost);
  };

  const size_t prime_index;
  const El::Range<El::Int> I;
  const El::Range<El::Int> J;

  Blas_Job(size_t prime_index, const El::Range<El::Int> &I,
           const El::Range<El::Int> &J);

  [[nodiscard]] Cost cost() const;
};
