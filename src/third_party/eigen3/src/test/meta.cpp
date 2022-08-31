// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "main.h"

template<typename From, typename To>
bool check_is_convertible(const From&, const To&)
{
  return internal::is_convertible<From,To>::value;
}

struct FooReturnType {
  typedef int ReturnType;
};

struct MyInterface {
  virtual void func() = 0;
  virtual ~MyInterface() {}
};
struct MyImpl : public MyInterface {
  void func() {}
};

EIGEN_DECLARE_TEST(meta)
{
  VERIFY(( internal::is_same<float,float>::value));
  VERIFY((!internal::is_same<float,double>::value));
  VERIFY((!internal::is_same<float,float&>::value));
  VERIFY((!internal::is_same<float,const float&>::value));

  VERIFY(( internal::is_same<float,internal::remove_all_t<const float&> >::value));
  VERIFY(( internal::is_same<float,internal::remove_all_t<const float*> >::value));
  VERIFY(( internal::is_same<float,internal::remove_all_t<const float*&> >::value));
  VERIFY(( internal::is_same<float,internal::remove_all_t<float**> >::value));
  VERIFY(( internal::is_same<float,internal::remove_all_t<float**&> >::value));
  VERIFY(( internal::is_same<float,internal::remove_all_t<float* const *&> >::value));
  VERIFY(( internal::is_same<float,internal::remove_all_t<float* const> >::value));

  // test add_const_on_value_type
  VERIFY(( internal::is_same< internal::add_const_on_value_type_t<float&>, float const& >::value));
  VERIFY(( internal::is_same< internal::add_const_on_value_type_t<float*>, float const* >::value));

  VERIFY(( internal::is_same< internal::add_const_on_value_type_t<float>, const float >::value));
  VERIFY(( internal::is_same< internal::add_const_on_value_type_t<const float>, const float >::value));

  VERIFY(( internal::is_same< internal::add_const_on_value_type_t<const float* const>, const float* const>::value));
  VERIFY(( internal::is_same< internal::add_const_on_value_type_t<float* const>, const float* const>::value));

  // is_convertible
  STATIC_CHECK(( internal::is_convertible<float,double>::value ));
  STATIC_CHECK(( internal::is_convertible<int,double>::value ));
  STATIC_CHECK(( internal::is_convertible<int, short>::value ));
  STATIC_CHECK(( internal::is_convertible<short, int>::value ));
  STATIC_CHECK(( internal::is_convertible<double,int>::value ));
  STATIC_CHECK(( internal::is_convertible<double,std::complex<double> >::value ));
  STATIC_CHECK((!internal::is_convertible<std::complex<double>,double>::value ));
  STATIC_CHECK(( internal::is_convertible<Array33f,Matrix3f>::value ));
  STATIC_CHECK(( internal::is_convertible<Matrix3f&,Matrix3f>::value ));
  STATIC_CHECK(( internal::is_convertible<Matrix3f&,Matrix3f&>::value ));
  STATIC_CHECK(( internal::is_convertible<Matrix3f&,const Matrix3f&>::value ));
  STATIC_CHECK(( internal::is_convertible<const Matrix3f&,Matrix3f>::value ));
  STATIC_CHECK(( internal::is_convertible<const Matrix3f&,const Matrix3f&>::value ));
  STATIC_CHECK((!internal::is_convertible<const Matrix3f&,Matrix3f&>::value ));
  STATIC_CHECK((!internal::is_convertible<const Matrix3f,Matrix3f&>::value ));
  STATIC_CHECK(!( internal::is_convertible<Matrix3f,Matrix3f&>::value ));

  STATIC_CHECK(!( internal::is_convertible<int,int&>::value ));
  STATIC_CHECK(( internal::is_convertible<const int,const int& >::value ));

  //STATIC_CHECK((!internal::is_convertible<Matrix3f,Matrix3d>::value )); //does not even compile because the conversion is prevented by a static assertion
  STATIC_CHECK((!internal::is_convertible<Array33f,int>::value ));
  STATIC_CHECK((!internal::is_convertible<MatrixXf,float>::value ));
  {
    float f = 0.0f;
    MatrixXf A, B;
    VectorXf a, b;
    VERIFY(( check_is_convertible(a.dot(b), f) ));
    VERIFY(( check_is_convertible(a.transpose()*b, f) ));
    VERIFY((!check_is_convertible(A*B, f) ));
    VERIFY(( check_is_convertible(A*B, A) ));
  }

  #if (EIGEN_COMP_GNUC  && EIGEN_COMP_GNUC  <=  99) \
   || (EIGEN_COMP_CLANG && EIGEN_COMP_CLANG <= 909) \
   || (EIGEN_COMP_MSVC  && EIGEN_COMP_MSVC  <=1914)
  // See http://eigen.tuxfamily.org/bz/show_bug.cgi?id=1752,
  // basically, a fix in the c++ standard breaks our c++98 implementation
  // of is_convertible for abstract classes.
  // So the following tests are expected to fail with recent compilers.

  STATIC_CHECK(( !internal::is_convertible<MyInterface, MyImpl>::value ));
  STATIC_CHECK(( !internal::is_convertible<MyImpl, MyInterface>::value ));
  STATIC_CHECK((  internal::is_convertible<MyImpl, const MyInterface&>::value ));

  #endif

  {
    int i = 0;
    VERIFY(( check_is_convertible(fix<3>(), i) ));
    VERIFY((!check_is_convertible(i, fix<DynamicIndex>()) ));
  }


  VERIFY((  internal::has_ReturnType<FooReturnType>::value ));
  VERIFY((  internal::has_ReturnType<ScalarBinaryOpTraits<int,int> >::value ));
  VERIFY(( !internal::has_ReturnType<MatrixXf>::value ));
  VERIFY(( !internal::has_ReturnType<int>::value ));

  VERIFY(internal::meta_sqrt<1>::ret == 1);
  #define VERIFY_META_SQRT(X) VERIFY(internal::meta_sqrt<X>::ret == int(std::sqrt(double(X))))
  VERIFY_META_SQRT(2);
  VERIFY_META_SQRT(3);
  VERIFY_META_SQRT(4);
  VERIFY_META_SQRT(5);
  VERIFY_META_SQRT(6);
  VERIFY_META_SQRT(8);
  VERIFY_META_SQRT(9);
  VERIFY_META_SQRT(15);
  VERIFY_META_SQRT(16);
  VERIFY_META_SQRT(17);
  VERIFY_META_SQRT(255);
  VERIFY_META_SQRT(256);
  VERIFY_META_SQRT(257);
  VERIFY_META_SQRT(1023);
  VERIFY_META_SQRT(1024);
  VERIFY_META_SQRT(1025);
}
