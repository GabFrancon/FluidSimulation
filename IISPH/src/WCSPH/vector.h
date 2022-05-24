// ----------------------------------------------------------------------------
// Vector.hpp
//
//  Created on: 22 Jan 2021
//      Author: Kiwon Um
//        Mail: kiwon.um@telecom-paris.fr
//
// Description: Vector class
//
// Copyright 2021 Kiwon Um
//
// The copyright to the computer program(s) herein is the property of Kiwon Um,
// Telecom Paris, France. The program(s) may be used and/or copied only with
// the written permission of Kiwon Um or in accordance with the terms and
// conditions stipulated in the agreement/contract under which the program(s)
// have been supplied.
// ----------------------------------------------------------------------------

#ifndef _VECTOR_H_
#define _VECTOR_H_

// std
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>


typedef float Real;
typedef long int Index;

inline Real square(const Real a) { return a*a; }
inline Real cube(const Real a) { return a*a*a; }
inline Real clamp(const Real v, const Real vmin, const Real vmax)
{
  if(v<vmin) return vmin;
  if(v>vmax) return vmax;
  return v;
}

template<typename T>
class Vector2 {
public:
  enum { D = 2 };

  typedef T ValueT;

  union {
    struct { T x; T y; };
    struct { T i; T j; };
    T v[D];
  };

  explicit Vector2(const T &value=0) : x(value), y(value) {}
  Vector2(const T &a, const T &b) : x(a), y(b) {}

  // assignment operators
  Vector2& operator+=(const Vector2 &r) { x+=r.x; y+=r.y; return *this; }
  Vector2& operator-=(const Vector2 &r) { x-=r.x; y-=r.y; return *this; }
  Vector2& operator*=(const Vector2 &r) { x*=r.x; y*=r.y; return *this; }
  Vector2& operator/=(const Vector2 &r) { x/=r.x; y/=r.y; return *this; }

  Vector2& operator+=(const T *r) { x+=r[0]; y+=r[1]; return *this; }
  Vector2& operator-=(const T *r) { x-=r[0]; y-=r[1]; return *this; }
  Vector2& operator*=(const T *r) { x*=r[0]; y*=r[1]; return *this; }
  Vector2& operator/=(const T *r) { x/=r[0]; y/=r[1]; return *this; }

  Vector2& operator+=(const T s) { x+=s; y+=s; return *this; }
  Vector2& operator-=(const T s) { x-=s; y-=s; return *this; }
  Vector2& operator*=(const T s) { x*=s; y*=s; return *this; }
  Vector2& operator/=(const T s) {
    const T d=static_cast<T>(1)/s; return operator*=(d);
  }

  // unary operators
  Vector2 operator+() const { return *this; }
  Vector2 operator-() const { return Vector2(-x, -y); }

  // binary operators
  Vector2 operator+(const Vector2 &r) const { return Vector2(*this)+=r; }
  Vector2 operator-(const Vector2 &r) const { return Vector2(*this)-=r; }
  Vector2 operator*(const Vector2 &r) const { return Vector2(*this)*=r; }
  Vector2 operator/(const Vector2 &r) const { return Vector2(*this)/=r; }

  Vector2 operator+(const T *r) const { return Vector2(*this)+=r; }
  Vector2 operator-(const T *r) const { return Vector2(*this)-=r; }
  Vector2 operator*(const T *r) const { return Vector2(*this)*=r; }
  Vector2 operator/(const T *r) const { return Vector2(*this)/=r; }

  Vector2 operator+(const T s) const { return Vector2(*this)+=s; }
  Vector2 operator-(const T s) const { return Vector2(*this)-=s; }
  Vector2 operator*(const T s) const { return Vector2(*this)*=s; }
  Vector2 operator/(const T s) const { return Vector2(*this)/=s; }

  // comparison operators
  bool operator==(const Vector2 &r) const { return ((x==r.x) && (y==r.y)); }
  bool operator!=(const Vector2 &r) const { return !((*this)==r); }
  bool operator<(const Vector2 &r) const { return (x!=r.x) ? x<r.x : y<r.y; }

  // cast operator
  template<typename T2>
  operator Vector2<T2>() const
  {
    return Vector2<T2>(static_cast<T2>(x), static_cast<T2>(y));
  }

  const T& operator[](const Index i) const { assert(i<D); return v[i]; }
  T& operator[](const Index i)
  {
    return const_cast<T &>(static_cast<const Vector2 &>(*this)[i]);
  }

  // special calculative functions

  Vector2& lowerValues(const Vector2 &r)
  {
    x = std::min(x, r.x); y = std::min(y, r.y); return *this;
  }
  Vector2& upperValues(const Vector2 &r)
  {
    x = std::max(x, r.x); y = std::max(y, r.y); return *this;
  }

  template<typename T2>
  Vector2& increase(const Index di, const T2 d)
  {
    v[di] += static_cast<T>(d); return (*this);
  }
  template<typename T2>
  Vector2 increased(const Index di, const T2 d) const
  {
    return Vector2(*this).increase(di, d);
  }

  Vector2& normalize() { return (x==0 && y==0) ? (*this) : (*this)/=length(); }
  Vector2 normalized() const { return Vector2(*this).normalize(); }

  T dotProduct(const Vector2 &r) const { return x*r.x + y*r.y; }
  T crossProduct(const Vector2 &r) const { return x*r.y - y*r.x; }

  T length() const { return std::sqrt(lengthSquare()); }
  T lengthSquare() const { return x*x + y*y; }
  T distanceTo(const Vector2 &t) const { return (*this-t).length(); }
  T distanceSquareTo(const Vector2 &t) const
  {
    return (*this-t).lengthSquare();
  }

  T mulAll() const { return x*y; }
  T sumAll() const { return x+y; }

  Index minorAxis() const { return (std::fabs(y)<std::fabs(x)) ? 1 : 0; }
  Index majorAxis() const { return (std::fabs(y)>std::fabs(x)) ? 1 : 0; }

  T minValue() const { return std::min(x, y); }
  T maxValue() const { return std::max(x, y); }
  T minAbsValue() const { return std::min(std::fabs(x), std::fabs(y)); }
  T maxAbsValue() const { return std::max(std::fabs(x), std::fabs(y)); }

  Vector2& rotate(const Real radian) { return *this = rotated(radian); }
  Vector2 rotated(const Real radian) const
  {
    const Real cost=std::cos(radian);
    const Real sint=std::sin(radian);
    return Vector2(cost*x-sint*y, sint*x+cost*y);
  }
  Vector2& rotate90() { return *this = rotated90(); }
  Vector2 rotated90() const { return Vector2(-y, x); }

  Vector2& reflect(const Vector2 &n) { return *this = reflected(n); }
  Vector2 reflected(const Vector2 &n) const
  {
    return *this - 2.0*(this->dotProduct(n))*n;
  }

  Vector2& mirror(const Vector2 &n) { return *this = mirrored(n); }
  Vector2 mirrored(const Vector2 &n) const
  {
    return -(*this) + 2.0*(this->dotProduct(n))*n;
  }

  Vector2& project(const Vector2 &n) { return *this = projected(n); }
  Vector2 projected(const Vector2 &n) const
  {
    return n * (this->dotProduct(n));
  }

  Vector2& reject(const Vector2 &n) { return *this = rejected(n); }
  Vector2 rejected(const Vector2 &n) const { return *this - projected(n); }

  friend std::istream& operator>>(std::istream &in, Vector2 &vec)
  {
    return (in >> vec.x >> vec.y);
  }
  friend std::ostream& operator<<(std::ostream &out, const Vector2 &vec)
  {
    return (out << vec.x << " " << vec.y);
  }
};
typedef Vector2<Real> Vec2f;
inline const Vec2f operator*(const Real s, const Vec2f &r) { return r*s; }

#endif  /* _VECTOR_H_ */
