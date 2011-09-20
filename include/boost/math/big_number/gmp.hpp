///////////////////////////////////////////////////////////////////////////////
//  Copyright 2011 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MATH_ER_GMP_BACKEND_HPP
#define BOOST_MATH_ER_GMP_BACKEND_HPP

#include <boost/math/big_number.hpp>
#include <boost/math/special_functions/fpclassify.hpp>
#include <boost/cstdint.hpp>
#include <boost/lexical_cast.hpp>
#include <gmp.h>
#include <cmath>
#include <limits>
#include <climits>

namespace boost{ namespace math{

template <unsigned digits10>
struct gmp_real;

namespace detail{

template <unsigned digits10>
struct gmp_real_imp
{
   typedef mpl::list<long, long long>                 signed_types;
   typedef mpl::list<unsigned long, unsigned long long>   unsigned_types;
   typedef mpl::list<double, long double>            real_types;

   gmp_real_imp(){}

   gmp_real_imp(const gmp_real_imp& o)
   {
      //
      // We have to do an init followed by a set here, otherwise *this may be at
      // a lower precision than o: seems like mpf_init_set copies just enough bits
      // to get the right value, but if it's then used in further calculations
      // things go badly wrong!!
      //
      mpf_init2(m_data, (((digits10 ? digits10 : get_default_precision()) + 1) * 1000L) / 301L);
      mpf_set(m_data, o.m_data);
   }
#ifndef BOOST_NO_RVALUE_REFERENCES
   gmp_real_imp(gmp_real_imp&& o)
   {
      m_data[0] = o.m_data[0];
      o.m_data[0]._mp_d = 0;
   }
#endif
   gmp_real_imp& operator = (const gmp_real_imp& o)
   {
      mpf_set(m_data, o.m_data);
      return *this;
   }
#ifndef BOOST_NO_RVALUE_REFERENCES
   gmp_real_imp& operator = (gmp_real_imp&& o)
   {
      mpf_swap(m_data, o.m_data);
      return *this;
   }
#endif
   gmp_real_imp& operator = (boost::uintmax_t i)
   {
      boost::uintmax_t mask = ((1uLL << std::numeric_limits<unsigned>::digits) - 1);
      unsigned shift = 0;
      mpf_t t;
      mpf_init2(t, (((digits10 ? digits10 : get_default_precision()) + 1) * 1000L) / 301L);
      mpf_set_ui(m_data, 0);
      while(i)
      {
         mpf_set_ui(t, static_cast<unsigned>(i & mask));
         if(shift)
            mpf_mul_2exp(t, t, shift);
         mpf_add(m_data, m_data, t);
         shift += std::numeric_limits<unsigned>::digits;
         i >>= std::numeric_limits<unsigned>::digits;
      }
      mpf_clear(t);
      return *this;
   }
   gmp_real_imp& operator = (boost::intmax_t i)
   {
      bool neg = i < 0;
      *this = static_cast<boost::uintmax_t>(std::abs(i));
      if(neg)
         mpf_neg(m_data, m_data);
      return *this;
   }
   gmp_real_imp& operator = (unsigned long i)
   {
      mpf_set_ui(m_data, i);
      return *this;
   }
   gmp_real_imp& operator = (long i)
   {
      mpf_set_si(m_data, i);
      return *this;
   }
   gmp_real_imp& operator = (double d)
   {
      mpf_set_d(m_data, d);
      return *this;
   }
   gmp_real_imp& operator = (long double a)
   {
      using std::frexp;
      using std::ldexp;
      using std::floor;

      if (a == 0) {
         mpf_set_si(m_data, 0);
         return *this;
      }

      if (a == 1) {
         mpf_set_si(m_data, 1);
         return *this;
      }

      BOOST_ASSERT(!(boost::math::isinf)(a));
      BOOST_ASSERT(!(boost::math::isnan)(a));

      int e;
      long double f, term;
      mpf_init_set_ui(m_data, 0u);

      f = frexp(a, &e);

      static const int shift = std::numeric_limits<int>::digits - 1;

      while(f)
      {
         // extract int sized bits from f:
         f = ldexp(f, shift);
         term = floor(f);
         e -= shift;
         mpf_mul_2exp(m_data, m_data, shift);
         if(term > 0)
            mpf_add_ui(m_data, m_data, static_cast<unsigned>(term));
         else
            mpf_sub_ui(m_data, m_data, static_cast<unsigned>(-term));
         f -= term;
      }
      if(e > 0)
         mpf_mul_2exp(m_data, m_data, e);
      else if(e < 0)
         mpf_div_2exp(m_data, m_data, -e);
      return *this;
   }
   gmp_real_imp& operator = (const char* s)
   {
      mpf_set_str(m_data, s, 10);
      return *this;
   }
   void swap(gmp_real_imp& o)
   {
      mpf_swap(m_data, o.m_data);
   }
   std::string str(unsigned digits, bool scientific)const
   {
      std::string result;
      mp_exp_t e;
      void *(*alloc_func_ptr) (size_t);
      void *(*realloc_func_ptr) (void *, size_t, size_t);
      void (*free_func_ptr) (void *, size_t);
      const char* ps = mpf_get_str (0, &e, 10, digits, m_data);
      std::ptrdiff_t sl = std::strlen(ps);
      if(sl == 0)
         return "0";
      if(*ps == '-')
         --sl; // number of digits excluding sign.
      if(!scientific
         && (sl <= std::numeric_limits<boost::uintmax_t>::digits10 + 1)
         && (e >= sl)
         && (sl <= std::numeric_limits<boost::uintmax_t>::digits10 + 1))
      {
         result = ps;
         result.append(e-sl, '0');
      }
      else
      {
         result = ps;
         if(ps[0] == '-')
            result.insert(2, 1, '.');
         else
            result.insert(1, 1, '.');
         --e;
         if(e)
            result += "e" + lexical_cast<std::string>(e);
      }
      mp_get_memory_functions(&alloc_func_ptr, &realloc_func_ptr, &free_func_ptr);
      (*free_func_ptr)((void*)ps, std::strlen(ps) + 1);
      return result;
   }
   ~gmp_real_imp()
   {
      if(m_data[0]._mp_d)
         mpf_clear(m_data);
   }
   void negate()
   {
      mpf_neg(m_data, m_data);
   }
   int compare(const gmp_real<digits10>& o)const
   {
      return mpf_cmp(m_data, o.m_data);
   }
   int compare(long i)const
   {
      return mpf_cmp_si(m_data, i);
   }
   int compare(unsigned long i)const
   {
      return mpf_cmp_ui(m_data, i);
   }
   template <class V>
   int compare(V v)const
   {
      gmp_real<digits10> d;
      d = v;
      return compare(d);
   }
   mpf_t& data() { return m_data; }
   const mpf_t& data()const { return m_data; }
protected:
   mpf_t m_data;
   static unsigned& get_default_precision()
   {
      static unsigned val = 50;
      return val;
   }
};

} // namespace detail

template <unsigned digits10>
struct gmp_real : public detail::gmp_real_imp<digits10>
{
   gmp_real()
   {
      mpf_init2(this->m_data, ((digits10 + 1) * 1000L) / 301L);
   }
   gmp_real(const gmp_real& o) : detail::gmp_real_imp<digits10>(o) {}
#ifndef BOOST_NO_RVALUE_REFERENCES
   gmp_real(gmp_real&& o) : detail::gmp_real_imp<digits10>(o) {}
#endif
   gmp_real& operator=(const gmp_real& o)
   {
      *static_cast<detail::gmp_real_imp<digits10>*>(this) = static_cast<detail::gmp_real_imp<digits10> const&>(o);
      return *this;
   }
#ifndef BOOST_NO_RVALUE_REFERENCES
   gmp_real& operator=(gmp_real&& o)
   {
      *static_cast<detail::gmp_real_imp<digits10>*>(this) = static_cast<detail::gmp_real_imp<digits10>&&>(o);
      return *this;
   }
#endif
   template <class V>
   gmp_real& operator=(const V& v)
   {
      *static_cast<detail::gmp_real_imp<digits10>*>(this) = v;
      return *this;
   }
};

template <>
struct gmp_real<0> : public detail::gmp_real_imp<0>
{
   gmp_real()
   {
      mpf_init2(this->m_data, ((get_default_precision() + 1) * 1000L) / 301L);
   }
   gmp_real(unsigned digits10)
   {
      mpf_init2(this->m_data, ((digits10 + 1) * 1000L) / 301L);
   }
   gmp_real(const gmp_real& o) : detail::gmp_real_imp<0>(o) {}
#ifndef BOOST_NO_RVALUE_REFERENCES
   gmp_real(gmp_real&& o) : detail::gmp_real_imp<0>(o) {}
#endif
   gmp_real(const gmp_real& o, unsigned digits10) 
   {
      mpf_init2(this->m_data, ((digits10 + 1) * 1000L) / 301L);
      *this = o;
   }

   gmp_real& operator=(const gmp_real& o)
   {
      *static_cast<detail::gmp_real_imp<0>*>(this) = static_cast<detail::gmp_real_imp<0> const&>(o);
      return *this;
   }
#ifndef BOOST_NO_RVALUE_REFERENCES
   gmp_real& operator=(gmp_real&& o)
   {
      *static_cast<detail::gmp_real_imp<0>*>(this) = static_cast<detail::gmp_real_imp<0> &&>(o);
      return *this;
   }
#endif
   template <class V>
   gmp_real& operator=(const V& v)
   {
      *static_cast<detail::gmp_real_imp<0>*>(this) = v;
      return *this;
   }
   static unsigned default_precision()
   {
      return get_default_precision();
   }
   static void default_precision(unsigned v)
   {
      get_default_precision() = v;
   }
   unsigned precision()const
   {
      return mpf_get_prec(this->m_data) * 301L / 1000 - 1;
   }
   void precision(unsigned digits10)
   {
      mpf_set_prec(this->m_data, (digits10 + 1) * 1000L / 301);
   }
};

template <unsigned digits10>
inline void add(gmp_real<digits10>& result, const gmp_real<digits10>& o)
{
   mpf_add(result.data(), result.data(), o.data());
}
template <unsigned digits10>
inline void subtract(gmp_real<digits10>& result, const gmp_real<digits10>& o)
{
   mpf_sub(result.data(), result.data(), o.data());
}
template <unsigned digits10>
inline void multiply(gmp_real<digits10>& result, const gmp_real<digits10>& o)
{
   mpf_mul(result.data(), result.data(), o.data());
}
template <unsigned digits10>
inline void divide(gmp_real<digits10>& result, const gmp_real<digits10>& o)
{
   mpf_div(result.data(), result.data(), o.data());
}
template <unsigned digits10>
inline void add(gmp_real<digits10>& result, unsigned long i)
{
   mpf_add_ui(result.data(), result.data(), i);
}
template <unsigned digits10>
inline void subtract(gmp_real<digits10>& result, unsigned long i)
{
   mpf_sub_ui(result.data(), result.data(), i);
}
template <unsigned digits10>
inline void multiply(gmp_real<digits10>& result, unsigned long i)
{
   mpf_mul_ui(result.data(), result.data(), i);
}
template <unsigned digits10>
inline void divide(gmp_real<digits10>& result, unsigned long i)
{
   mpf_div_ui(result.data(), result.data(), i);
}
template <unsigned digits10>
inline void add(gmp_real<digits10>& result, long i)
{
   if(i > 0)
      mpf_add_ui(result.data(), result.data(), i);
   else
      mpf_sub_ui(result.data(), result.data(), std::abs(i));
}
template <unsigned digits10>
inline void subtract(gmp_real<digits10>& result, long i)
{
   if(i > 0)
      mpf_sub_ui(result.data(), result.data(), i);
   else
      mpf_add_ui(result.data(), result.data(), std::abs(i));
}
template <unsigned digits10>
inline void multiply(gmp_real<digits10>& result, long i)
{
   mpf_mul_ui(result.data(), result.data(), std::abs(i));
   if(i < 0)
      mpf_neg(result.data(), result.data());
}
template <unsigned digits10>
inline void divide(gmp_real<digits10>& result, long i)
{
   mpf_div_ui(result.data(), result.data(), std::abs(i));
   if(i < 0)
      mpf_neg(result.data(), result.data());
}
//
// Specialised 3 arg versions of the basic operators:
//
template <unsigned digits10>
inline void add(gmp_real<digits10>& a, const gmp_real<digits10>& x, const gmp_real<digits10>& y)
{
   mpf_add(a.data(), x.data(), y.data());
}
template <unsigned digits10>
inline void add(gmp_real<digits10>& a, const gmp_real<digits10>& x, unsigned long y)
{
   mpf_add_ui(a.data(), x.data(), y);
}
template <unsigned digits10>
inline void add(gmp_real<digits10>& a, const gmp_real<digits10>& x, long y)
{
   if(y < 0)
      mpf_sub_ui(a.data(), x.data(), -y);
   else
      mpf_add_ui(a.data(), x.data(), y);
}
template <unsigned digits10>
inline void add(gmp_real<digits10>& a, unsigned long x, const gmp_real<digits10>& y)
{
   mpf_add_ui(a.data(), y.data(), x);
}
template <unsigned digits10>
inline void add(gmp_real<digits10>& a, long x, const gmp_real<digits10>& y)
{
   if(x < 0)
   {
      mpf_ui_sub(a.data(), -x, y.data());
      mpf_neg(a.data(), a.data());
   }
   else
      mpf_add_ui(a.data(), y.data(), x);
}
template <unsigned digits10>
inline void subtract(gmp_real<digits10>& a, const gmp_real<digits10>& x, const gmp_real<digits10>& y)
{
   mpf_sub(a.data(), x.data(), y.data());
}
template <unsigned digits10>
inline void subtract(gmp_real<digits10>& a, const gmp_real<digits10>& x, unsigned long y)
{
   mpf_sub_ui(a.data(), x.data(), y);
}
template <unsigned digits10>
inline void subtract(gmp_real<digits10>& a, const gmp_real<digits10>& x, long y)
{
   if(y < 0)
      mpf_add_ui(a.data(), x.data(), -y);
   else
      mpf_sub_ui(a.data(), x.data(), y);
}
template <unsigned digits10>
inline void subtract(gmp_real<digits10>& a, unsigned long x, const gmp_real<digits10>& y)
{
   mpf_ui_sub(a.data(), x, y.data());
}
template <unsigned digits10>
inline void subtract(gmp_real<digits10>& a, long x, const gmp_real<digits10>& y)
{
   if(x < 0)
   {
      mpf_add_ui(a.data(), y.data(), -x);
      mpf_neg(a.data(), a.data());
   }
   else
      mpf_ui_sub(a.data(), x, y.data());
}

template <unsigned digits10>
inline void multiply(gmp_real<digits10>& a, const gmp_real<digits10>& x, const gmp_real<digits10>& y)
{
   mpf_mul(a.data(), x.data(), y.data());
}
template <unsigned digits10>
inline void multiply(gmp_real<digits10>& a, const gmp_real<digits10>& x, unsigned long y)
{
   mpf_mul_ui(a.data(), x.data(), y);
}
template <unsigned digits10>
inline void multiply(gmp_real<digits10>& a, const gmp_real<digits10>& x, long y)
{
   if(y < 0)
   {
      mpf_mul_ui(a.data(), x.data(), -y);
      a.negate();
   }
   else
      mpf_mul_ui(a.data(), x.data(), y);
}
template <unsigned digits10>
inline void multiply(gmp_real<digits10>& a, unsigned long x, const gmp_real<digits10>& y)
{
   mpf_mul_ui(a.data(), y.data(), x);
}
template <unsigned digits10>
inline void multiply(gmp_real<digits10>& a, long x, const gmp_real<digits10>& y)
{
   if(x < 0)
   {
      mpf_mul_ui(a.data(), y.data(), -x);
      mpf_neg(a.data(), a.data());
   }
   else
      mpf_mul_ui(a.data(), y.data(), x);
}

template <unsigned digits10>
inline void divide(gmp_real<digits10>& a, const gmp_real<digits10>& x, const gmp_real<digits10>& y)
{
   mpf_div(a.data(), x.data(), y.data());
}
template <unsigned digits10>
inline void divide(gmp_real<digits10>& a, const gmp_real<digits10>& x, unsigned long y)
{
   mpf_div_ui(a.data(), x.data(), y);
}
template <unsigned digits10>
inline void divide(gmp_real<digits10>& a, const gmp_real<digits10>& x, long y)
{
   if(y < 0)
   {
      mpf_div_ui(a.data(), x.data(), -y);
      a.negate();
   }
   else
      mpf_div_ui(a.data(), x.data(), y);
}
template <unsigned digits10>
inline void divide(gmp_real<digits10>& a, unsigned long x, const gmp_real<digits10>& y)
{
   mpf_ui_div(a.data(), x, y.data());
}
template <unsigned digits10>
inline void divide(gmp_real<digits10>& a, long x, const gmp_real<digits10>& y)
{
   if(x < 0)
   {
      mpf_ui_div(a.data(), -x, y.data());
      mpf_neg(a.data(), a.data());
   }
   else
      mpf_ui_div(a.data(), x, y.data());
}

template <unsigned digits10>
inline bool is_zero(const gmp_real<digits10>& val)
{
   return mpf_sgn(val.data()) == 0;
}
template <unsigned digits10>
inline int get_sign(const gmp_real<digits10>& val)
{
   return mpf_sgn(val.data());
}

template <unsigned digits10>
inline void convert_to(unsigned long* result, const gmp_real<digits10>& val)
{
   *result = mpf_get_ui(val.data());
}
template <unsigned digits10>
inline void convert_to(long* result, const gmp_real<digits10>& val)
{
   *result = mpf_get_si(val.data());
}
template <unsigned digits10>
inline void convert_to(double* result, const gmp_real<digits10>& val)
{
   *result = mpf_get_d(val.data());
}

//
// Native non-member operations:
//
template <unsigned Digits10>
inline void eval_sqrt(gmp_real<Digits10>& result, const gmp_real<Digits10>& val)
{
   mpf_sqrt(result.data(), val.data());
}

template <unsigned Digits10>
inline void eval_abs(gmp_real<Digits10>& result, const gmp_real<Digits10>& val)
{
   mpf_abs(result.data(), val.data());
}

template <unsigned Digits10>
inline void eval_fabs(gmp_real<Digits10>& result, const gmp_real<Digits10>& val)
{
   mpf_abs(result.data(), val.data());
}
template <unsigned Digits10>
inline void eval_ceil(gmp_real<Digits10>& result, const gmp_real<Digits10>& val)
{
   mpf_ceil(result.data(), val.data());
}
template <unsigned Digits10>
inline void eval_floor(gmp_real<Digits10>& result, const gmp_real<Digits10>& val)
{
   mpf_floor(result.data(), val.data());
}
template <unsigned Digits10>
inline void eval_trunc(gmp_real<Digits10>& result, const gmp_real<Digits10>& val)
{
   mpf_trunc(result.data(), val.data());
}
template <unsigned Digits10>
inline void eval_ldexp(gmp_real<Digits10>& result, const gmp_real<Digits10>& val, long e)
{
   if(e > 0)
      mpf_mul_2exp(result.data(), val.data(), e);
   else if(e < 0)
      mpf_div_2exp(result.data(), val.data(), -e);
}
template <unsigned Digits10>
inline void eval_frexp(gmp_real<Digits10>& result, const gmp_real<Digits10>& val, int* e)
{
   long v;
   mpf_get_d_2exp(&v, val.data());
   *e = v;
   eval_ldexp(result, val, -v);
}
template <unsigned Digits10>
inline void eval_frexp(gmp_real<Digits10>& result, const gmp_real<Digits10>& val, long* e)
{
   mpf_get_d_2exp(e, val.data());
   eval_ldexp(result, val, -*e);
}

struct gmp_int
{
   typedef mpl::list<long, long long>                 signed_types;
   typedef mpl::list<unsigned long, unsigned long long>   unsigned_types;
   typedef mpl::list<double, long double>            real_types;

   gmp_int()
   {
      mpz_init(this->m_data);
   }
   gmp_int(const gmp_int& o)
   {
      mpz_init_set(m_data, o.m_data);
   }
   gmp_int& operator = (const gmp_int& o)
   {
      mpz_set(m_data, o.m_data);
      return *this;
   }
   gmp_int& operator = (boost::uintmax_t i)
   {
      boost::uintmax_t mask = ((1uLL << std::numeric_limits<unsigned>::digits) - 1);
      unsigned shift = 0;
      mpz_t t;
      mpz_init(m_data);
      mpz_init(t);
      while(i)
      {
         mpz_set_ui(t, static_cast<unsigned>(i & mask));
         if(shift)
            mpz_mul_2exp(t, t, shift);
         mpz_add(m_data, m_data, t);
         shift += std::numeric_limits<unsigned>::digits;
         i >>= std::numeric_limits<unsigned>::digits;
      }
      mpz_clear(t);
      return *this;
   }
   gmp_int& operator = (boost::intmax_t i)
   {
      bool neg = i < 0;
      *this = static_cast<boost::uintmax_t>(std::abs(i));
      if(neg)
         mpz_neg(m_data, m_data);
      return *this;
   }
   gmp_int& operator = (unsigned long i)
   {
      mpz_set_ui(m_data, i);
      return *this;
   }
   gmp_int& operator = (long i)
   {
      mpz_set_si(m_data, i);
      return *this;
   }
   gmp_int& operator = (double d)
   {
      mpz_set_d(m_data, d);
      return *this;
   }
   gmp_int& operator = (long double a)
   {
      using std::frexp;
      using std::ldexp;
      using std::floor;

      if (a == 0) {
         mpz_set_si(m_data, 0);
         return *this;
      }

      if (a == 1) {
         mpz_set_si(m_data, 1);
         return *this;
      }

      BOOST_ASSERT(!(boost::math::isinf)(a));
      BOOST_ASSERT(!(boost::math::isnan)(a));

      int e;
      long double f, term;
      mpz_init_set_ui(m_data, 0u);

      f = frexp(a, &e);

      static const int shift = std::numeric_limits<int>::digits - 1;

      while(f)
      {
         // extract int sized bits from f:
         f = ldexp(f, shift);
         term = floor(f);
         e -= shift;
         mpz_mul_2exp(m_data, m_data, shift);
         if(term > 0)
            mpz_add_ui(m_data, m_data, static_cast<unsigned>(term));
         else
            mpz_sub_ui(m_data, m_data, static_cast<unsigned>(-term));
         f -= term;
      }
      if(e > 0)
         mpz_mul_2exp(m_data, m_data, e);
      else if(e < 0)
         mpz_div_2exp(m_data, m_data, -e);
      return *this;
   }
   gmp_int& operator = (const char* s)
   {
      mpz_set_str(m_data, s, 10);
      return *this;
   }
   void swap(gmp_int& o)
   {
      mpz_swap(m_data, o.m_data);
   }
   std::string str(unsigned /*digits*/, bool /*scientific*/)const
   {
      void *(*alloc_func_ptr) (size_t);
      void *(*realloc_func_ptr) (void *, size_t, size_t);
      void (*free_func_ptr) (void *, size_t);
      const char* ps = mpz_get_str (0, 10, m_data);
      std::string s = ps;
      mp_get_memory_functions(&alloc_func_ptr, &realloc_func_ptr, &free_func_ptr);
      (*free_func_ptr)((void*)ps, std::strlen(ps) + 1);
      return s;
   }
   ~gmp_int()
   {
      mpz_clear(m_data);
   }
   void negate()
   {
      mpz_neg(m_data, m_data);
   }
   int compare(const gmp_int& o)const
   {
      return mpz_cmp(m_data, o.m_data);
   }
   int compare(long i)const
   {
      return mpz_cmp_si(m_data, i);
   }
   int compare(unsigned long i)const
   {
      return mpz_cmp_ui(m_data, i);
   }
   template <class V>
   int compare(V v)const
   {
      gmp_int d;
      d = v;
      return compare(d);
   }
   mpz_t& data() { return m_data; }
   const mpz_t& data()const { return m_data; }
protected:
   mpz_t m_data;
};

inline void add(gmp_int& t, const gmp_int& o)
{
   mpz_add(t.data(), t.data(), o.data());
}
inline void subtract(gmp_int& t, const gmp_int& o)
{
   mpz_sub(t.data(), t.data(), o.data());
}
inline void multiply(gmp_int& t, const gmp_int& o)
{
   mpz_mul(t.data(), t.data(), o.data());
}
inline void divide(gmp_int& t, const gmp_int& o)
{
   mpz_div(t.data(), t.data(), o.data());
}
inline void modulus(gmp_int& t, const gmp_int& o)
{
   bool neg = mpz_sgn(t.data()) < 0;
   bool neg2 = mpz_sgn(o.data()) < 0;
   mpz_mod(t.data(), t.data(), o.data());
   if(neg)
   {
      if(!neg2)
         t.negate();
      mpz_add(t.data(), t.data(), o.data());
      if(!neg2)
         t.negate();
   }
}
inline void add(gmp_int& t, unsigned long i)
{
   mpz_add_ui(t.data(), t.data(), i);
}
inline void subtract(gmp_int& t, unsigned long i)
{
   mpz_sub_ui(t.data(), t.data(), i);
}
inline void multiply(gmp_int& t, unsigned long i)
{
   mpz_mul_ui(t.data(), t.data(), i);
}
inline void modulus(gmp_int& t, unsigned long i)
{
   bool neg = mpz_sgn(t.data()) < 0;
   mpz_mod_ui(t.data(), t.data(), i);
   if(neg)
   {
      t.negate();
      mpz_add_ui(t.data(), t.data(), i);
      t.negate();
   }
}
inline void divide(gmp_int& t, unsigned long i)
{
   mpz_div_ui(t.data(), t.data(), i);
}
inline void add(gmp_int& t, long i)
{
   if(i > 0)
      mpz_add_ui(t.data(), t.data(), i);
   else
      mpz_sub_ui(t.data(), t.data(), -i);
}
inline void subtract(gmp_int& t, long i)
{
   if(i > 0)
      mpz_sub_ui(t.data(), t.data(), i);
   else
      mpz_add_ui(t.data(), t.data(), -i);
}
inline void multiply(gmp_int& t, long i)
{
   mpz_mul_ui(t.data(), t.data(), std::abs(i));
   if(i < 0)
      mpz_neg(t.data(), t.data());
}
inline void modulus(gmp_int& t, long i)
{
   bool neg = mpz_sgn(t.data()) < 0;
   bool neg2 = i < 0;
   mpz_mod_ui(t.data(), t.data(), std::abs(i));
   if(neg)
   {
      if(!neg2)
      {
         t.negate();
         mpz_add_ui(t.data(), t.data(), std::abs(i));
         t.negate();
      }
      else
      {
         mpz_sub_ui(t.data(), t.data(), std::abs(i));
      }
   }
}
inline void divide(gmp_int& t, long i)
{
   mpz_div_ui(t.data(), t.data(), std::abs(i));
   if(i < 0)
      mpz_neg(t.data(), t.data());
}
template <class UI>
inline void left_shift(gmp_int& t, UI i)
{
   mpz_mul_2exp(t.data(), t.data(), static_cast<unsigned long>(i));
}
template <class UI>
inline void right_shift(gmp_int& t, UI i)
{
   mpz_fdiv_q_2exp(t.data(), t.data(), static_cast<unsigned long>(i));
}
template <class UI>
inline void left_shift(gmp_int& t, const gmp_int& v, UI i)
{
   mpz_mul_2exp(t.data(), v.data(), static_cast<unsigned long>(i));
}
template <class UI>
inline void right_shift(gmp_int& t, const gmp_int& v, UI i)
{
   mpz_fdiv_q_2exp(t.data(), v.data(), static_cast<unsigned long>(i));
}

inline void bitwise_and(gmp_int& result, const gmp_int& v)
{
   mpz_and(result.data(), result.data(), v.data());
}

inline void bitwise_or(gmp_int& result, const gmp_int& v)
{
   mpz_ior(result.data(), result.data(), v.data());
}

inline void bitwise_xor(gmp_int& result, const gmp_int& v)
{
   mpz_xor(result.data(), result.data(), v.data());
}

inline void add(gmp_int& t, const gmp_int& p, const gmp_int& o)
{
   mpz_add(t.data(), p.data(), o.data());
}
inline void subtract(gmp_int& t, const gmp_int& p, const gmp_int& o)
{
   mpz_sub(t.data(), p.data(), o.data());
}
inline void multiply(gmp_int& t, const gmp_int& p, const gmp_int& o)
{
   mpz_mul(t.data(), p.data(), o.data());
}
inline void divide(gmp_int& t, const gmp_int& p, const gmp_int& o)
{
   mpz_div(t.data(), p.data(), o.data());
}
inline void modulus(gmp_int& t, const gmp_int& p, const gmp_int& o)
{
   bool neg = mpz_sgn(p.data()) < 0;
   bool neg2 = mpz_sgn(o.data()) < 0;
   mpz_mod(t.data(), p.data(), o.data());
   if(neg)
   {
      if(!neg2)
         t.negate();
      mpz_add(t.data(), t.data(), o.data());
      if(!neg2)
         t.negate();
   }
}
inline void add(gmp_int& t, const gmp_int& p, unsigned long i)
{
   mpz_add_ui(t.data(), p.data(), i);
}
inline void subtract(gmp_int& t, const gmp_int& p, unsigned long i)
{
   mpz_sub_ui(t.data(), p.data(), i);
}
inline void multiply(gmp_int& t, const gmp_int& p, unsigned long i)
{
   mpz_mul_ui(t.data(), p.data(), i);
}
inline void modulus(gmp_int& t, const gmp_int& p, unsigned long i)
{
   bool neg = mpz_sgn(p.data()) < 0;
   mpz_mod_ui(t.data(), p.data(), i);
   if(neg)
   {
      t.negate();
      mpz_add_ui(t.data(), t.data(), i);
      t.negate();
   }
}
inline void divide(gmp_int& t, const gmp_int& p, unsigned long i)
{
   mpz_div_ui(t.data(), p.data(), i);
}
inline void add(gmp_int& t, const gmp_int& p, long i)
{
   if(i > 0)
      mpz_add_ui(t.data(), p.data(), i);
   else
      mpz_sub_ui(t.data(), p.data(), -i);
}
inline void subtract(gmp_int& t, const gmp_int& p, long i)
{
   if(i > 0)
      mpz_sub_ui(t.data(), p.data(), i);
   else
      mpz_add_ui(t.data(), p.data(), -i);
}
inline void multiply(gmp_int& t, const gmp_int& p, long i)
{
   mpz_mul_ui(t.data(), p.data(), std::abs(i));
   if(i < 0)
      mpz_neg(t.data(), t.data());
}
inline void modulus(gmp_int& t, const gmp_int& p, long i)
{
   bool neg = mpz_sgn(p.data()) < 0;
   bool neg2 = i < 0;
   mpz_mod_ui(t.data(), p.data(), std::abs(i));
   if(neg)
   {
      if(!neg2)
      {
         t.negate();
         mpz_add_ui(t.data(), t.data(), std::abs(i));
         t.negate();
      }
      else
      {
         mpz_sub_ui(t.data(), t.data(), std::abs(i));
      }
   }
}
inline void divide(gmp_int& t, const gmp_int& p, long i)
{
   mpz_div_ui(t.data(), p.data(), std::abs(i));
   if(i < 0)
      mpz_neg(t.data(), t.data());
}
   
inline void bitwise_and(gmp_int& result, const gmp_int& u, const gmp_int& v)
{
   mpz_and(result.data(), u.data(), v.data());
}

inline void bitwise_or(gmp_int& result, const gmp_int& u, const gmp_int& v)
{
   mpz_ior(result.data(), u.data(), v.data());
}

inline void bitwise_xor(gmp_int& result, const gmp_int& u, const gmp_int& v)
{
   mpz_xor(result.data(), u.data(), v.data());
}

inline void complement(gmp_int& result, const gmp_int& u)
{
   mpz_com(result.data(), u.data());
}

inline bool is_zero(const gmp_int& val)
{
   return mpz_sgn(val.data()) == 0;
}
inline int get_sign(const gmp_int& val)
{
   return mpz_sgn(val.data());
}
inline void convert_to(unsigned long* result, const gmp_int& val)
{
   *result = mpz_get_ui(val.data());
}
inline void convert_to(long* result, const gmp_int& val)
{
   *result = mpz_get_si(val.data());
}
inline void convert_to(double* result, const gmp_int& val)
{
   *result = mpz_get_d(val.data());
}

inline void eval_abs(gmp_int& result, const gmp_int& val)
{
   mpz_abs(result.data(), val.data());
}

struct gmp_rational;
void add(gmp_rational& t, const gmp_rational& o);

struct gmp_rational
{
   typedef mpl::list<long, long long>                 signed_types;
   typedef mpl::list<unsigned long, unsigned long long>   unsigned_types;
   typedef mpl::list<double, long double>            real_types;

   gmp_rational()
   {
      mpq_init(this->m_data);
   }
   gmp_rational(const gmp_rational& o)
   {
      mpq_init(m_data);
      mpq_set(m_data, o.m_data);
   }
   gmp_rational& operator = (const gmp_rational& o)
   {
      mpq_set(m_data, o.m_data);
      return *this;
   }
   gmp_rational& operator = (boost::uintmax_t i)
   {
      boost::uintmax_t mask = ((1uLL << std::numeric_limits<unsigned>::digits) - 1);
      unsigned shift = 0;
      mpq_t t;
      mpq_init(m_data);
      mpq_init(t);
      while(i)
      {
         mpq_set_ui(t, static_cast<unsigned>(i & mask), 1);
         if(shift)
            mpq_mul_2exp(t, t, shift);
         mpq_add(m_data, m_data, t);
         shift += std::numeric_limits<unsigned>::digits;
         i >>= std::numeric_limits<unsigned>::digits;
      }
      mpq_clear(t);
      return *this;
   }
   gmp_rational& operator = (boost::intmax_t i)
   {
      bool neg = i < 0;
      *this = static_cast<boost::uintmax_t>(std::abs(i));
      if(neg)
         mpq_neg(m_data, m_data);
      return *this;
   }
   gmp_rational& operator = (unsigned long i)
   {
      mpq_set_ui(m_data, i, 1);
      return *this;
   }
   gmp_rational& operator = (long i)
   {
      mpq_set_si(m_data, i, 1);
      return *this;
   }
   gmp_rational& operator = (double d)
   {
      mpq_set_d(m_data, d);
      return *this;
   }
   gmp_rational& operator = (long double a)
   {
      using std::frexp;
      using std::ldexp;
      using std::floor;
      using big_num_default_ops::add;
      using big_num_default_ops::subtract;

      if (a == 0) {
         mpq_set_si(m_data, 0, 1);
         return *this;
      }

      if (a == 1) {
         mpq_set_si(m_data, 1, 1);
         return *this;
      }

      BOOST_ASSERT(!(boost::math::isinf)(a));
      BOOST_ASSERT(!(boost::math::isnan)(a));

      int e;
      long double f, term;
      mpq_init(m_data);
      mpq_set_ui(m_data, 0u, 1);
      gmp_rational t;

      f = frexp(a, &e);

      static const int shift = std::numeric_limits<int>::digits - 1;

      while(f)
      {
         // extract int sized bits from f:
         f = ldexp(f, shift);
         term = floor(f);
         e -= shift;
         mpq_mul_2exp(m_data, m_data, shift);
         t = static_cast<long>(term);
         add(*this, t);
         f -= term;
      }
      if(e > 0)
         mpq_mul_2exp(m_data, m_data, e);
      else if(e < 0)
         mpq_div_2exp(m_data, m_data, -e);
      return *this;
   }
   gmp_rational& operator = (const char* s)
   {
      mpq_set_str(m_data, s, 10);
      return *this;
   }
   void swap(gmp_rational& o)
   {
      mpq_swap(m_data, o.m_data);
   }
   std::string str(unsigned /*digits*/, bool /*scientific*/)const
   {
      void *(*alloc_func_ptr) (size_t);
      void *(*realloc_func_ptr) (void *, size_t, size_t);
      void (*free_func_ptr) (void *, size_t);
      const char* ps = mpq_get_str (0, 10, m_data);
      std::string s = ps;
      mp_get_memory_functions(&alloc_func_ptr, &realloc_func_ptr, &free_func_ptr);
      (*free_func_ptr)((void*)ps, std::strlen(ps) + 1);
      return s;
   }
   ~gmp_rational()
   {
      mpq_clear(m_data);
   }
   void negate()
   {
      mpq_neg(m_data, m_data);
   }
   int compare(const gmp_rational& o)const
   {
      return mpq_cmp(m_data, o.m_data);
   }
   template <class V>
   int compare(V v)const
   {
      gmp_rational d;
      d = v;
      return compare(d);
   }
   int compare(unsigned long v)
   {
      return mpq_cmp_ui(m_data, v, 1);
   }
   int compare(long v)
   {
      return mpq_cmp_si(m_data, v, 1);
   }
   mpq_t& data() { return m_data; }
   const mpq_t& data()const { return m_data; }
protected:
   mpq_t m_data;
};

inline void add(gmp_rational& t, const gmp_rational& o)
{
   mpq_add(t.data(), t.data(), o.data());
}
inline void subtract(gmp_rational& t, const gmp_rational& o)
{
   mpq_sub(t.data(), t.data(), o.data());
}
inline void multiply(gmp_rational& t, const gmp_rational& o)
{
   mpq_mul(t.data(), t.data(), o.data());
}
inline void divide(gmp_rational& t, const gmp_rational& o)
{
   mpq_div(t.data(), t.data(), o.data());
}
inline void add(gmp_rational& t, const gmp_rational& p, const gmp_rational& o)
{
   mpq_add(t.data(), p.data(), o.data());
}
inline void subtract(gmp_rational& t, const gmp_rational& p, const gmp_rational& o)
{
   mpq_sub(t.data(), p.data(), o.data());
}
inline void multiply(gmp_rational& t, const gmp_rational& p, const gmp_rational& o)
{
   mpq_mul(t.data(), p.data(), o.data());
}
inline void divide(gmp_rational& t, const gmp_rational& p, const gmp_rational& o)
{
   mpq_div(t.data(), p.data(), o.data());
}
   
inline bool is_zero(const gmp_rational& val)
{
   return mpq_sgn(val.data()) == 0;
}
inline int get_sign(const gmp_rational& val)
{
   return mpq_sgn(val.data());
}
inline void convert_to(double* result, const gmp_rational& val)
{
   *result = mpq_get_d(val.data());
}

inline void convert_to(long* result, const gmp_rational& val)
{
   double r;
   convert_to(&r, val);
   *result = r;
}

inline void convert_to(unsigned long* result, const gmp_rational& val)
{
   double r;
   convert_to(&r, val);
   *result = r;
}

inline void eval_abs(gmp_rational& result, const gmp_rational& val)
{
   mpq_abs(result.data(), val.data());
}

template<>
struct is_extended_integer<gmp_int> : public mpl::true_ {};

typedef big_number<gmp_real<50> >    mpf_real_50;
typedef big_number<gmp_real<100> >   mpf_real_100;
typedef big_number<gmp_real<500> >   mpf_real_500;
typedef big_number<gmp_real<1000> >  mpf_real_1000;
typedef big_number<gmp_real<0> >     mpf_real;
typedef big_number<gmp_int >         mpz_int;
typedef big_number<gmp_rational >    mpq_rational;

}}  // namespaces

namespace std{

#ifdef BOOST_NO_NOEXCEPT
#  define noexcept
#endif

//
// numeric_limits [partial] specializations for the types declared in this header:
//
template<unsigned Digits10> 
class numeric_limits<boost::math::big_number<boost::math::gmp_real<Digits10> > >
{
   typedef boost::math::big_number<boost::math::gmp_real<Digits10> > number_type;
public:
   BOOST_STATIC_CONSTEXPR bool is_specialized = true;
   BOOST_STATIC_CONSTEXPR number_type (min)() noexcept
   { 
      initializer.do_nothing();
      static std::pair<bool, number_type> value;
      if(!value.first)
      {
         value.first = true;
         value.second = 1;
         mpf_div_2exp(value.second.backend().data(), value.second.backend().data(), LONG_MAX);
      }
      return value.second;
   }
   BOOST_STATIC_CONSTEXPR number_type (max)() noexcept
   { 
      initializer.do_nothing();
      static std::pair<bool, number_type> value;
      if(!value.first)
      {
         value.first = true;
         value.second = 1;
         mpf_mul_2exp(value.second.backend().data(), value.second.backend().data(), LONG_MAX - 1);
      }
      return value.second;
   }
   BOOST_STATIC_CONSTEXPR number_type lowest() noexcept
   {
      return -(max)();
   }
   BOOST_STATIC_CONSTEXPR int digits = static_cast<int>(((Digits10 + 1) * 1000L) / 301L);
   BOOST_STATIC_CONSTEXPR int digits10 = Digits10;
   // Is this really correct???
   BOOST_STATIC_CONSTEXPR int max_digits10 = Digits10 + 1;
   BOOST_STATIC_CONSTEXPR bool is_signed = true;
   BOOST_STATIC_CONSTEXPR bool is_integer = false;
   BOOST_STATIC_CONSTEXPR bool is_exact = false;
   BOOST_STATIC_CONSTEXPR int radix = 2;
   BOOST_STATIC_CONSTEXPR number_type epsilon() noexcept 
   { 
      initializer.do_nothing();
      static std::pair<bool, number_type> value;
      if(!value.first)
      {
         value.first = true;
         value.second = 1;
         mpf_div_2exp(value.second.backend().data(), value.second.backend().data(), std::numeric_limits<number_type>::digits - 1);
      }
      return value.second;
   }
   // What value should this be????
   BOOST_STATIC_CONSTEXPR number_type round_error() noexcept 
   { 
      // returns epsilon/2
      initializer.do_nothing();
      static std::pair<bool, number_type> value;
      if(!value.first)
      {
         value.first = true;
         value.second = 1;
         mpf_div_2exp(value.second.backend().data(), value.second.backend().data(), digits);
      }
      return value.second;
   }
   BOOST_STATIC_CONSTEXPR long min_exponent = LONG_MIN;
   BOOST_STATIC_CONSTEXPR long min_exponent10 = (LONG_MIN / 1000) * 301L;
   BOOST_STATIC_CONSTEXPR long max_exponent = LONG_MAX;
   BOOST_STATIC_CONSTEXPR long max_exponent10 = (LONG_MAX / 1000) * 301L;
   BOOST_STATIC_CONSTEXPR bool has_infinity = false;
   BOOST_STATIC_CONSTEXPR bool has_quiet_NaN = false;
   BOOST_STATIC_CONSTEXPR bool has_signaling_NaN = false;
   BOOST_STATIC_CONSTEXPR float_denorm_style has_denorm = denorm_absent;
   BOOST_STATIC_CONSTEXPR bool has_denorm_loss = false;
   BOOST_STATIC_CONSTEXPR number_type infinity() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR number_type quiet_NaN() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR number_type signaling_NaN() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR number_type denorm_min() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR bool is_iec559 = false;
   BOOST_STATIC_CONSTEXPR bool is_bounded = true;
   BOOST_STATIC_CONSTEXPR bool is_modulo = false;
   BOOST_STATIC_CONSTEXPR bool traps = true;
   BOOST_STATIC_CONSTEXPR bool tinyness_before = false;
   BOOST_STATIC_CONSTEXPR float_round_style round_style = round_to_nearest;

private:
   struct data_initializer
   {
      data_initializer()
      {
         std::numeric_limits<boost::math::big_number<boost::math::gmp_real<digits10> > >::epsilon();
         std::numeric_limits<boost::math::big_number<boost::math::gmp_real<digits10> > >::round_error();
         (std::numeric_limits<boost::math::big_number<boost::math::gmp_real<digits10> > >::min)();
         (std::numeric_limits<boost::math::big_number<boost::math::gmp_real<digits10> > >::max)();
      }
      void do_nothing()const{}
   };
   static const data_initializer initializer;
};

template<unsigned Digits10> 
const typename numeric_limits<boost::math::big_number<boost::math::gmp_real<Digits10> > >::data_initializer numeric_limits<boost::math::big_number<boost::math::gmp_real<Digits10> > >::initializer;

template<> 
class numeric_limits<boost::math::big_number<boost::math::gmp_real<0> > >
{
   typedef boost::math::big_number<boost::math::gmp_real<0> > number_type;
public:
   BOOST_STATIC_CONSTEXPR bool is_specialized = false;
   BOOST_STATIC_CONSTEXPR number_type (min)() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR number_type (max)() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR number_type lowest() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR int digits = 0;
   BOOST_STATIC_CONSTEXPR int digits10 = 0;
   BOOST_STATIC_CONSTEXPR int max_digits10 = 0;
   BOOST_STATIC_CONSTEXPR bool is_signed = false;
   BOOST_STATIC_CONSTEXPR bool is_integer = false;
   BOOST_STATIC_CONSTEXPR bool is_exact = false;
   BOOST_STATIC_CONSTEXPR int radix = 0;
   BOOST_STATIC_CONSTEXPR number_type epsilon() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR number_type round_error() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR int min_exponent = 0;
   BOOST_STATIC_CONSTEXPR int min_exponent10 = 0;
   BOOST_STATIC_CONSTEXPR int max_exponent = 0;
   BOOST_STATIC_CONSTEXPR int max_exponent10 = 0;
   BOOST_STATIC_CONSTEXPR bool has_infinity = false;
   BOOST_STATIC_CONSTEXPR bool has_quiet_NaN = false;
   BOOST_STATIC_CONSTEXPR bool has_signaling_NaN = false;
   BOOST_STATIC_CONSTEXPR float_denorm_style has_denorm = denorm_absent;
   BOOST_STATIC_CONSTEXPR bool has_denorm_loss = false;
   BOOST_STATIC_CONSTEXPR number_type infinity() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR number_type quiet_NaN() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR number_type signaling_NaN() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR number_type denorm_min() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR bool is_iec559 = false;
   BOOST_STATIC_CONSTEXPR bool is_bounded = false;
   BOOST_STATIC_CONSTEXPR bool is_modulo = false;
   BOOST_STATIC_CONSTEXPR bool traps = false;
   BOOST_STATIC_CONSTEXPR bool tinyness_before = false;
   BOOST_STATIC_CONSTEXPR float_round_style round_style = round_toward_zero;
};

template<> 
class numeric_limits<boost::math::mpz_int >
{
   typedef boost::math::mpz_int number_type;
public:
   BOOST_STATIC_CONSTEXPR bool is_specialized = true;
   //
   // Largest and smallest numbers are bounded only by available memory, set
   // to zero:
   //
   BOOST_STATIC_CONSTEXPR number_type (min)() noexcept
   { 
      return number_type();
   }
   BOOST_STATIC_CONSTEXPR number_type (max)() noexcept 
   { 
      return number_type();
   }
   BOOST_STATIC_CONSTEXPR number_type lowest() noexcept { return (min)(); }
   // Digits are unbounded, use zero for now:
   BOOST_STATIC_CONSTEXPR int digits = 0;
   BOOST_STATIC_CONSTEXPR int digits10 = 0;
   BOOST_STATIC_CONSTEXPR int max_digits10 = 0;
   BOOST_STATIC_CONSTEXPR bool is_signed = true;
   BOOST_STATIC_CONSTEXPR bool is_integer = true;
   BOOST_STATIC_CONSTEXPR bool is_exact = true;
   BOOST_STATIC_CONSTEXPR int radix = 2;
   BOOST_STATIC_CONSTEXPR number_type epsilon() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR number_type round_error() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR int min_exponent = 0;
   BOOST_STATIC_CONSTEXPR int min_exponent10 = 0;
   BOOST_STATIC_CONSTEXPR int max_exponent = 0;
   BOOST_STATIC_CONSTEXPR int max_exponent10 = 0;
   BOOST_STATIC_CONSTEXPR bool has_infinity = false;
   BOOST_STATIC_CONSTEXPR bool has_quiet_NaN = false;
   BOOST_STATIC_CONSTEXPR bool has_signaling_NaN = false;
   BOOST_STATIC_CONSTEXPR float_denorm_style has_denorm = denorm_absent;
   BOOST_STATIC_CONSTEXPR bool has_denorm_loss = false;
   BOOST_STATIC_CONSTEXPR number_type infinity() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR number_type quiet_NaN() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR number_type signaling_NaN() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR number_type denorm_min() noexcept { return number_type(); }
   BOOST_STATIC_CONSTEXPR bool is_iec559 = false;
   BOOST_STATIC_CONSTEXPR bool is_bounded = false;
   BOOST_STATIC_CONSTEXPR bool is_modulo = false;
   BOOST_STATIC_CONSTEXPR bool traps = false;
   BOOST_STATIC_CONSTEXPR bool tinyness_before = false;
   BOOST_STATIC_CONSTEXPR float_round_style round_style = round_toward_zero;
};

#ifdef BOOST_NO_NOEXCEPT
#  undef noexcept
#endif

} // namespace std

#endif
