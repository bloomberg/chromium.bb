// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008 Benoit Jacob <jacob.benoit.1@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "main.h"

template<typename MatrixType> void matrixVisitor(const MatrixType& p)
{
  typedef typename MatrixType::Scalar Scalar;

  Index rows = p.rows();
  Index cols = p.cols();

  // construct a random matrix where all coefficients are different
  MatrixType m;
  m = MatrixType::Random(rows, cols);
  for(Index i = 0; i < m.size(); i++)
    for(Index i2 = 0; i2 < i; i2++)
      while(numext::equal_strict(m(i), m(i2))) // yes, strict equality
        m(i) = internal::random<Scalar>();
  
  Scalar minc = Scalar(1000), maxc = Scalar(-1000);
  Index minrow=0,mincol=0,maxrow=0,maxcol=0;
  for(Index j = 0; j < cols; j++)
  for(Index i = 0; i < rows; i++)
  {
    if(m(i,j) < minc)
    {
      minc = m(i,j);
      minrow = i;
      mincol = j;
    }
    if(m(i,j) > maxc)
    {
      maxc = m(i,j);
      maxrow = i;
      maxcol = j;
    }
  }
  Index eigen_minrow, eigen_mincol, eigen_maxrow, eigen_maxcol;
  Scalar eigen_minc, eigen_maxc;
  eigen_minc = m.minCoeff(&eigen_minrow,&eigen_mincol);
  eigen_maxc = m.maxCoeff(&eigen_maxrow,&eigen_maxcol);
  VERIFY(minrow == eigen_minrow);
  VERIFY(maxrow == eigen_maxrow);
  VERIFY(mincol == eigen_mincol);
  VERIFY(maxcol == eigen_maxcol);
  VERIFY_IS_APPROX(minc, eigen_minc);
  VERIFY_IS_APPROX(maxc, eigen_maxc);
  VERIFY_IS_APPROX(minc, m.minCoeff());
  VERIFY_IS_APPROX(maxc, m.maxCoeff());

  eigen_maxc = (m.adjoint()*m).maxCoeff(&eigen_maxrow,&eigen_maxcol);
  Index maxrow2=0,maxcol2=0;
  eigen_maxc = (m.adjoint()*m).eval().maxCoeff(&maxrow2,&maxcol2);
  VERIFY(maxrow2 == eigen_maxrow);
  VERIFY(maxcol2 == eigen_maxcol);

  if (!NumTraits<Scalar>::IsInteger && m.size() > 2) {
    // Test NaN propagation by replacing an element with NaN.
    bool stop = false;
    for (Index j = 0; j < cols && !stop; ++j) {
      for (Index i = 0; i < rows && !stop; ++i) {
        if (!(j == mincol && i == minrow) &&
            !(j == maxcol && i == maxrow)) {
          m(i,j) = NumTraits<Scalar>::quiet_NaN();
          stop = true;
          break;
        }
      }
    }

    eigen_minc = m.template minCoeff<PropagateNumbers>(&eigen_minrow, &eigen_mincol);
    eigen_maxc = m.template maxCoeff<PropagateNumbers>(&eigen_maxrow, &eigen_maxcol);
    VERIFY(minrow == eigen_minrow);
    VERIFY(maxrow == eigen_maxrow);
    VERIFY(mincol == eigen_mincol);
    VERIFY(maxcol == eigen_maxcol);
    VERIFY_IS_APPROX(minc, eigen_minc);
    VERIFY_IS_APPROX(maxc, eigen_maxc);
    VERIFY_IS_APPROX(minc, m.template minCoeff<PropagateNumbers>());
    VERIFY_IS_APPROX(maxc, m.template maxCoeff<PropagateNumbers>());

    eigen_minc = m.template minCoeff<PropagateNaN>(&eigen_minrow, &eigen_mincol);
    eigen_maxc = m.template maxCoeff<PropagateNaN>(&eigen_maxrow, &eigen_maxcol);
    VERIFY(minrow != eigen_minrow || mincol != eigen_mincol);
    VERIFY(maxrow != eigen_maxrow || maxcol != eigen_maxcol);
    VERIFY((numext::isnan)(eigen_minc));
    VERIFY((numext::isnan)(eigen_maxc));
  }

}

template<typename VectorType> void vectorVisitor(const VectorType& w)
{
  typedef typename VectorType::Scalar Scalar;

  Index size = w.size();

  // construct a random vector where all coefficients are different
  VectorType v;
  v = VectorType::Random(size);
  for(Index i = 0; i < size; i++)
    for(Index i2 = 0; i2 < i; i2++)
      while(v(i) == v(i2)) // yes, ==
        v(i) = internal::random<Scalar>();
  
  Scalar minc = v(0), maxc = v(0);
  Index minidx=0, maxidx=0;
  for(Index i = 0; i < size; i++)
  {
    if(v(i) < minc)
    {
      minc = v(i);
      minidx = i;
    }
    if(v(i) > maxc)
    {
      maxc = v(i);
      maxidx = i;
    }
  }
  Index eigen_minidx, eigen_maxidx;
  Scalar eigen_minc, eigen_maxc;
  eigen_minc = v.minCoeff(&eigen_minidx);
  eigen_maxc = v.maxCoeff(&eigen_maxidx);
  VERIFY(minidx == eigen_minidx);
  VERIFY(maxidx == eigen_maxidx);
  VERIFY_IS_APPROX(minc, eigen_minc);
  VERIFY_IS_APPROX(maxc, eigen_maxc);
  VERIFY_IS_APPROX(minc, v.minCoeff());
  VERIFY_IS_APPROX(maxc, v.maxCoeff());
  
  Index idx0 = internal::random<Index>(0,size-1);
  Index idx1 = eigen_minidx;
  Index idx2 = eigen_maxidx;
  VectorType v1(v), v2(v);
  v1(idx0) = v1(idx1);
  v2(idx0) = v2(idx2);
  v1.minCoeff(&eigen_minidx);
  v2.maxCoeff(&eigen_maxidx);
  VERIFY(eigen_minidx == (std::min)(idx0,idx1));
  VERIFY(eigen_maxidx == (std::min)(idx0,idx2));

  if (!NumTraits<Scalar>::IsInteger && size > 2) {
    // Test NaN propagation by replacing an element with NaN.
    for (Index i = 0; i < size; ++i) {
      if (i != minidx && i != maxidx) {
        v(i) = NumTraits<Scalar>::quiet_NaN();
        break;
      }
    }
    eigen_minc = v.template minCoeff<PropagateNumbers>(&eigen_minidx);
    eigen_maxc = v.template maxCoeff<PropagateNumbers>(&eigen_maxidx);
    VERIFY(minidx == eigen_minidx);
    VERIFY(maxidx == eigen_maxidx);
    VERIFY_IS_APPROX(minc, eigen_minc);
    VERIFY_IS_APPROX(maxc, eigen_maxc);
    VERIFY_IS_APPROX(minc, v.template minCoeff<PropagateNumbers>());
    VERIFY_IS_APPROX(maxc, v.template maxCoeff<PropagateNumbers>());

    eigen_minc = v.template minCoeff<PropagateNaN>(&eigen_minidx);
    eigen_maxc = v.template maxCoeff<PropagateNaN>(&eigen_maxidx);
    VERIFY(minidx != eigen_minidx);
    VERIFY(maxidx != eigen_maxidx);
    VERIFY((numext::isnan)(eigen_minc));
    VERIFY((numext::isnan)(eigen_maxc));
  }
}

template<typename T, bool Vectorizable>
struct TrackedVisitor {
  void init(T v, int i, int j) { return this->operator()(v,i,j); }
  void operator()(T v, int i, int j) {
    EIGEN_UNUSED_VARIABLE(v)
    visited.push_back({i, j});
    vectorized = false;
  }
  
  template<typename Packet>
  void packet(Packet p, int i, int j) {
    EIGEN_UNUSED_VARIABLE(p)  
    visited.push_back({i, j});
    vectorized = true;
  }
  std::vector<std::pair<int,int>> visited;
  bool vectorized;
};

namespace Eigen {
namespace internal {

template<typename T, bool Vectorizable>
struct functor_traits<TrackedVisitor<T, Vectorizable> > {
  enum { PacketAccess = Vectorizable, Cost = 1 };
};

}  // namespace internal
}  // namespace Eigen

void checkOptimalTraversal() {
  
  // Unrolled - ColMajor.
  {
    Eigen::Matrix4f X = Eigen::Matrix4f::Random();
    TrackedVisitor<double, false> visitor;
    X.visit(visitor);
    int count = 0;
    for (int j=0; j<X.cols(); ++j) {
      for (int i=0; i<X.rows(); ++i) {
        VERIFY_IS_EQUAL(visitor.visited[count].first, i);
        VERIFY_IS_EQUAL(visitor.visited[count].second, j);
        ++count;
      }
    }
  }
  
  // Unrolled - RowMajor.
  using Matrix4fRowMajor = Eigen::Matrix<float, 4, 4, Eigen::RowMajor>;
  {
    Matrix4fRowMajor X = Matrix4fRowMajor::Random();
    TrackedVisitor<double, false> visitor;
    X.visit(visitor);
    int count = 0;
    for (int i=0; i<X.rows(); ++i) {
      for (int j=0; j<X.cols(); ++j) {
        VERIFY_IS_EQUAL(visitor.visited[count].first, i);
        VERIFY_IS_EQUAL(visitor.visited[count].second, j);
        ++count;
      }
    }
  }
  
  // Not unrolled - ColMajor
  {
    Eigen::MatrixXf X = Eigen::MatrixXf::Random(4, 4);
    TrackedVisitor<double, false> visitor;
    X.visit(visitor);
    int count = 0;
    for (int j=0; j<X.cols(); ++j) {
      for (int i=0; i<X.rows(); ++i) {
        VERIFY_IS_EQUAL(visitor.visited[count].first, i);
        VERIFY_IS_EQUAL(visitor.visited[count].second, j);
        ++count;
      }
    }
  }
  
  // Not unrolled - RowMajor.
  using MatrixXfRowMajor = Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
  {
    MatrixXfRowMajor X = MatrixXfRowMajor::Random(4, 4);
    TrackedVisitor<double, false> visitor;
    X.visit(visitor);
    int count = 0;
    for (int i=0; i<X.rows(); ++i) {
      for (int j=0; j<X.cols(); ++j) {
        VERIFY_IS_EQUAL(visitor.visited[count].first, i);
        VERIFY_IS_EQUAL(visitor.visited[count].second, j);
        ++count;
      }
    }
  }
  
  // Vectorized - ColMajor
  {
    // Ensure rows/cols is larger than packet size.
    constexpr int PacketSize = Eigen::internal::packet_traits<float>::size;
    Eigen::MatrixXf X = Eigen::MatrixXf::Random(4 * PacketSize, 4 * PacketSize);
    TrackedVisitor<double, true> visitor;
    X.visit(visitor);
    int previ = -1;
    int prevj = 0;
    for (const auto& p : visitor.visited) {
      int i = p.first;
      int j = p.second;
      VERIFY(
        (j == prevj && i == previ + 1)             // Advance single element
        || (j == prevj && i == previ + PacketSize) // Advance packet
        || (j == prevj + 1 && i == 0)              // Advance column
      );
      previ = i;
      prevj = j;
    }
    if (Eigen::internal::packet_traits<float>::Vectorizable) {
      VERIFY(visitor.vectorized);
    }
  }
  
  // Vectorized - RowMajor.
  {
    // Ensure rows/cols is larger than packet size.
    constexpr int PacketSize = Eigen::internal::packet_traits<float>::size;
    MatrixXfRowMajor X = MatrixXfRowMajor::Random(4 * PacketSize, 4 * PacketSize);
    TrackedVisitor<double, true> visitor;
    X.visit(visitor);
    int previ = 0;
    int prevj = -1;
    for (const auto& p : visitor.visited) {
      int i = p.first;
      int j = p.second;
      VERIFY(
        (i == previ && j == prevj + 1)             // Advance single element
        || (i == previ && j == prevj + PacketSize) // Advance packet
        || (i == previ + 1 && j == 0)              // Advance row
      );
      previ = i;
      prevj = j;
    }
    if (Eigen::internal::packet_traits<float>::Vectorizable) {
      VERIFY(visitor.vectorized);
    }
  }
  
}

EIGEN_DECLARE_TEST(visitor)
{
  for(int i = 0; i < g_repeat; i++) {
    CALL_SUBTEST_1( matrixVisitor(Matrix<float, 1, 1>()) );
    CALL_SUBTEST_2( matrixVisitor(Matrix2f()) );
    CALL_SUBTEST_3( matrixVisitor(Matrix4d()) );
    CALL_SUBTEST_4( matrixVisitor(MatrixXd(8, 12)) );
    CALL_SUBTEST_5( matrixVisitor(Matrix<double,Dynamic,Dynamic,RowMajor>(20, 20)) );
    CALL_SUBTEST_6( matrixVisitor(MatrixXi(8, 12)) );
  }
  for(int i = 0; i < g_repeat; i++) {
    CALL_SUBTEST_7( vectorVisitor(Vector4f()) );
    CALL_SUBTEST_7( vectorVisitor(Matrix<int,12,1>()) );
    CALL_SUBTEST_8( vectorVisitor(VectorXd(10)) );
    CALL_SUBTEST_9( vectorVisitor(RowVectorXd(10)) );
    CALL_SUBTEST_10( vectorVisitor(VectorXf(33)) );
  }
  CALL_SUBTEST_11(checkOptimalTraversal());
}
