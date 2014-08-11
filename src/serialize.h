#ifndef SDP_BOOTSTRAP_SERIALIZE_H_
#define SDP_BOOTSTRAP_SERIALIZE_H_

#include <string>
#include <vector>
#include <sstream>
#include <gmpxx.h>
#include "boost/serialization/serialization.hpp"
#include "boost/serialization/base_object.hpp"
#include "boost/serialization/utility.hpp"
#include "boost/serialization/vector.hpp"
#include "boost/serialization/string.hpp"
#include "boost/serialization/split_free.hpp"
#include "boost/archive/text_iarchive.hpp"
#include "boost/archive/text_oarchive.hpp"
#include "boost/filesystem.hpp"
#include "boost/filesystem/fstream.hpp"
#include "types.h"
#include "Vector.h"
#include "Matrix.h"
#include "BlockDiagonalMatrix.h"

using boost::filesystem::path;
using boost::archive::text_iarchive;
using std::vector;

namespace boost {
  namespace serialization {

    template<class Archive>
    void load(Archive& ar, mpf_class& f, unsigned int version) {
      std::string s;
      ar & s;
      f = mpf_class(s);
    }

    template<class Archive>
    void save(Archive& ar, mpf_class const& f, unsigned int version) {
      std::ostringstream os;
      os.precision(f.get_prec());
      os << f;
      std::string s = os.str();
      ar & s;
    }

    template<class Archive>
    void serialize(Archive& ar, Matrix& m, const unsigned int version) {
      ar & m.rows;
      ar & m.cols;
      ar & m.elements;
    }

    template<class Archive>
    void serialize(Archive& ar, BlockDiagonalMatrix& m, const unsigned int version) {
      ar & m.dim;
      ar & m.blocks;
      ar & m.blockStartIndices;
    }

    template<class Archive>
    void serializeSDPSolverState(Archive& ar, Vector &x, BlockDiagonalMatrix &X, BlockDiagonalMatrix &Y) {
      ar & x;
      ar & X;
      ar & Y;
    }

  } // namespace serialization
} // namespace boost


BOOST_SERIALIZATION_SPLIT_FREE(mpf_class)
BOOST_CLASS_VERSION(mpf_class, 0)
BOOST_CLASS_TRACKING(Matrix,              boost::serialization::track_never)
BOOST_CLASS_TRACKING(BlockDiagonalMatrix, boost::serialization::track_never)

#endif  // SDP_BOOTSTRAP_SERIALIZE_H_