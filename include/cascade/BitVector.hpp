/*
Copyright 2007, D. E. Shaw Research.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions, and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions, and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of D. E. Shaw Research nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

//////////////////////////////////////////////////////////////////////
//
// BitVector.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 05/15/2007
//
// This file implements the following:
//
// 1) Support for efficient arbitrary-width bit vectors
//
//      e.g. an unsigned bit vector of size 43 is bitvec<43>; a signed bit vector
//           of size 43 is bitvec<-43>.  
//
//      By convention, the typedef for bitvec<N> is uN (e.g. 43) and the typedef 
//      for bitvec<-N> is sN (e.g. s43).  These can be typedefed manually or 
//      en-masse via
//
//         #include "bv_types.hpp"
//
// 2) Support for efficient bit index operator:
//
//      u18 val18 = 0x557;
//      val18[15] = 1;
//      bit b = val18[12];
//
// 3) Support for efficient bit slice operator:
//
//      u23 val23 = val29(26,4);
//      val29(26,4) = val23;
//
// 4) Bit vector references:
//
//      uint32 data;
//      bitvecref<17> v17(&data);
//
// 5) Concatenation of bitvectors, bits, bit slices and bit vector references:
//
//      (v17, v23(9,6)) = (v8, v9[7], v47(33, 22));
//
// 6) Reduction operators:
//
//      bit b1 = reduce_and(v17);
//      bit b2 = reduce_or(v29(18,6));
//      bit b3 = reduce_xor(v8, v7);
//
// 7) String conversions:
//
//      str(v23);
//      str_bits(v17);
//      fromString("0x1eb", v9);
//      fromBitString("1001010", v7);
//    
//////////////////////////////////////////////////////////////////////

#ifndef Cascade_BitVector_hpp_
#define Cascade_BitVector_hpp_

#include <descore/StringBuffer.hpp>
#include <descore/utils.hpp>

/////////////////////////////////////////////////////////////////
//
// Code coverage for bitvector unit test
//
/////////////////////////////////////////////////////////////////
#ifdef _BV_COVERAGE
#include <descore/Coverage.hpp>
#define bv_CoverageItemBool CoverageItemBool
#define bv_CoverageItemValues CoverageItemValues
#define bv_coverAssert coverAssert
#define bv_coverLine coverLine
#define bv_coverBool coverBool
#define bv_if(exp) coverBool(exp) if (exp)
#define bv_coverRange coverRange
#define bv_coverValues coverValues
#define bv_coverCross coverCross
#define bv_coverCrossPredicated coverCrossPredicated
#define bv_returnCoveredBool returnCoveredBool
#define bv_returnCoveredBit(expr) returnCoveredValues(expr, 0, 1)
BEGIN_COVERAGE("ut_bitvector");
#else
#define bv_CoverageItemBool(...)
#define bv_CoverageItemValues(...)
#define bv_coverAssert(...)
#define bv_coverValues(...)
#define bv_coverRange(...)
#define bv_coverLine()
#define bv_coverBool(...)
#define bv_if(exp) if (exp)
#define bv_coverCross(...)
#define bv_coverCrossPredicated(...)
#define bv_returnCoveredBool(expr) return expr;
#define bv_returnCoveredBit(expr) return expr;
#endif

// Various inlining macros
#ifdef _UT_CASCADE
    #ifdef _BV_COVERAGE
        #define BV_INLINE
        #define BV_FORCEINLINE
        #define BV_FORCEINLINE_GLOBAL static
    #else
        #define BV_INLINE             inline
        #define BV_FORCEINLINE        inline
        #define BV_FORCEINLINE_GLOBAL inline
    #endif
#else
    #ifdef _DEBUG
        #define BV_INLINE             inline
        #define BV_FORCEINLINE        inline
        #define BV_FORCEINLINE_GLOBAL inline
    #else
        #define BV_INLINE             inline
        #define BV_FORCEINLINE        ALWAYS_INLINE
        #define BV_FORCEINLINE_GLOBAL ALWAYS_INLINE
    #endif
#endif


/////////////////////////////////////////////////////////////////
//
// Disable assertions in Release builds so that the BV_FORCEINLINE 
// optimizations will work.
//
/////////////////////////////////////////////////////////////////
#ifdef _DEBUG
#define bv_assert assert
#else
#define bv_assert(...)
#endif

////////////////////////////////////////////////////////////////////////
//
// Types and macros
//
////////////////////////////////////////////////////////////////////////

// Macro for use in functions where u_t is a k-bit unsigned integer,
// and bits <= k.  Returns the bitmask with the low 'bits' bits set.
// Do not use if 'bits' can be zero as it won't work when k = 32.
#define BITVEC_MASK(bits) (u_t(-1) >> (k - (bits)))

// Templated bitvector type (size is positive for unsigned, negative for signed)
template <int size> struct bitvec;

// Single-bit type
typedef bitvec<1> bit;

// Pseudo-bit type for when we don't need the dynamic range checking because the
// value is guaranteed to be 0/1 and/or we t to deal with an integer type 
// directly to avoid problems with multiple levels of implicit casting.
typedef byte _bit;

// Type declarations
template <typename Bits>  struct BV_ConstBits;
template <typename Bits>  struct BV_Bits;
template <int size>       struct bitvec;
template <int size>       struct bitvec_small;
template <int size>       struct bitvec_large;
                          struct BV_BitRef;
template <int kp, int kv> struct BV_BitSlice;
template <typename High, typename Low> struct BV_Compound;

////////////////////////////////////////////////////////////////////////
//
// Helper template to determine the C type used to implement small
// bitvectors and the size of the type in bits.
//
////////////////////////////////////////////////////////////////////////
template <int n> struct bv_basic_traits
{ 
    static const int usize = ((n > 32) || (n < -32)) ? 64 : ((n > 16) || (n < -16)) ? 32 : ((n > 8) || (n < -8)) ? 16 : 8;
    typedef typename bv_basic_traits<(n > 0) ? usize : -usize>::bv_t bv_t;
};

template<> struct bv_basic_traits<-8>  { typedef int8   bv_t; static const int usize = 8; };
template<> struct bv_basic_traits<8>   { typedef uint8  bv_t; static const int usize = 8; };
template<> struct bv_basic_traits<-16> { typedef int16  bv_t; static const int usize = 16; };
template<> struct bv_basic_traits<16>  { typedef uint16 bv_t; static const int usize = 16; };
template<> struct bv_basic_traits<-32> { typedef int32  bv_t; static const int usize = 32; };
template<> struct bv_basic_traits<32>  { typedef uint32 bv_t; static const int usize = 32; };
template<> struct bv_basic_traits<-64> { typedef int64  bv_t; static const int usize = 64; };
template<> struct bv_basic_traits<64>  { typedef uint64 bv_t; static const int usize = 64; };

template <int n> struct bv_traits
{
    // Absolute width of bitvector
    static const int width = (n > 0) ? n : -n;

    // Primitive word type used to store value
    typedef typename bv_basic_traits<n>::bv_t bv_t;

    // Unsigned type of same size as bv_t
    typedef typename bv_basic_traits<width>::bv_t u_t;

    // Size in bits of bv_t (8/16/32/64)
    static const int usize = bv_basic_traits<n>::usize;

    // usize * sign(n)
    static const int isize = (n > 0) ? usize : -usize;

    // Length of array of bv_t required to hold bitvector value
    static const int arraylen = (width + bv_basic_traits<n>::usize - 1) / bv_basic_traits<n>::usize;

    // Bitmask of bitvector bits in highest word in array
    static const u_t mask = (u_t) (u_t(-1) >> ((usize-1) - ((width-1) & (usize-1))));

    // Most significant bit of bitvector
    static const u_t msb = ((u_t) 1) << ((width-1) & (usize-1));

    // Maximum legal value for highest word in array                                                                                  
    static const bv_t maxval = (n < 0) ? (bv_t) (mask >> 1) : mask;

    // Minimum legal value for highest word in array
    static const bv_t minval = (n < 0) ? (bv_t(-1) << ((width-1) & (usize-1))) : 0;
};

/////////////////////////////////////////////////////////////////
//
// Bit assignment operations (=, |=, &=, ^=)
//
// k = 8, 16, 32 or 64.
//
/////////////////////////////////////////////////////////////////

enum EBV_OP
{
    BV_OP_ASSIGN,
    BV_OP_AND,
    BV_OP_OR,
    BV_OP_XOR
};

template <int k, EBV_OP op> struct BV_OpAssign;

// Specializations to do the actual assignment operation
template <int k>
struct BV_OpAssign<k, BV_OP_ASSIGN>
{
    typedef typename bv_traits<k>::u_t u_t;

    static BV_FORCEINLINE void assign (u_t *dst, u_t src, u_t mask = u_t(-1))
    {
        *dst ^= (*dst ^ src) & mask;
    }
};

template <int k>
struct BV_OpAssign<k, BV_OP_AND>
{
    typedef typename bv_traits<k>::u_t u_t;

    static BV_FORCEINLINE void assign (u_t *dst, u_t src, u_t mask = u_t(-1))
    {
        *dst ^= (*dst ^ (*dst & src)) & mask;
    }
};

template <int k>
struct BV_OpAssign<k, BV_OP_OR>
{
    typedef typename bv_traits<k>::u_t u_t;

    static BV_FORCEINLINE void assign (u_t *dst, u_t src, u_t mask = u_t(-1))
    {
        *dst |= src & mask;
    }
};

template <int k>
struct BV_OpAssign<k, BV_OP_XOR>
{
    typedef typename bv_traits<k>::u_t u_t;

    static BV_FORCEINLINE void assign (u_t *dst, u_t src, u_t mask = u_t(-1))
    {
        *dst ^= src & mask;
    }
};

// Helper structure used to tell assign() functions what to do
template <EBV_OP op> struct BV_EOpAssign {};

// Definition to make the code a bit more concise.  e.g.
// template <EBV_OP op> void assign (..., BV_OP)
// {
//     other_assign(..., (BV_OP) 0);
// }
#define BV_OP     BV_EOpAssign<op> *
#define BV_ASSIGN (BV_EOpAssign<BV_OP_ASSIGN> *) 0

////////////////////////////////////////////////////////////////////////
//
// Bit assignment helper function
//
// Fast copy of arbitrary source bits to arbitrary destination bits
// using a templated primitive type (uint8/uint16/uint32/uint64).
// k = 8, 16, 32 or 64.
//
////////////////////////////////////////////////////////////////////////

template <int k, EBV_OP op>
struct BV_Assign
{
    typedef typename bv_traits<k>::u_t u_t;
    typedef BV_OpAssign<k, op> bv_op;

    // Arbitrary bitcpy
    static BV_FORCEINLINE void assign (u_t *dst, int dst_low, const u_t *src, int src_low, int len);

    //----------------------------------
    // assign() can handle everything and is extremely efficient when forceinlined
    // with constants propagated, but not all compilers support the forceinline
    // construct so manually provide specialization for common cases to improve 
    // their performance with these compilers.
    //----------------------------------

    // Copy to a single u_t.
    static BV_FORCEINLINE void assignToWord (u_t *dst, const u_t *src, int src_low, int len);

    // Copy from a single u_t.
    static BV_FORCEINLINE void assignFromWord (u_t *dst, int dst_low, u_t src, int len);
};

template <int k, EBV_OP op>
BV_FORCEINLINE void BV_Assign<k,op>::assign (u_t *dst, int dst_low, const u_t *src, int src_low, int len)
{
    const int dst_high = dst_low + len;
    const int dst_low_modk = dst_low & (k-1);
    src += src_low / k;
    dst += dst_low / k;
    if (dst_low_modk + len <= k)
    {
        // destination is contained in a single k-bit word
        const int dst_high_modk = (dst_high - 1) & (k-1);
        const u_t mask = (u_t(-1) << dst_low_modk) & (u_t(-1) >> ((k-1) - dst_high_modk));

        bv_if ((src_low - dst_low) & (k-1))
        {
            // unaligned
            const int src_low_modk = src_low & (k-1);
            const int offset = dst_low_modk - src_low_modk;

            // If we're left-shifting src, then it's also in a single word
            if (offset >= 0)
            {
                bv_coverLine();
                bv_op::assign(dst, *src << offset, mask);
            }

            // Otherwise need to check if src crosses a word boundary
            else if (src_low_modk + len <= k)
            {
                bv_coverLine();
                bv_op::assign(dst, *src >> -offset, mask);
            }
            else
            {
                bv_coverLine();
                bv_op::assign(dst, (*src >> -offset) | (src[1] << (k + offset)), mask);
            }
        }
        else // aligned (easy)
        {
            bv_coverLine();
            bv_op::assign(dst, *src, mask);
        }
    }
    else if ((src_low - dst_low) & (k-1)) // unaligned
    {
        // 1. copy bits until the destination is k-bit aligned
        bv_if (dst_low_modk)
        {
            const u_t mask = u_t(-1) << dst_low_modk;
            const int src_low_modk = src_low & (k-1);

            // If the source bits straddle two words, need to do some extra shifting
            const int offset = dst_low_modk - src_low_modk;
            bv_if (offset < 0)
            {
                bv_op::assign(dst, (*src >> -offset) | (src[1] << (k + offset)), mask);
                src++;
            }
            else
                bv_op::assign(dst, *src << offset, mask);

            // Update state
            const int count = k - dst_low_modk;
            src_low += count;
            dst_low += count;
            dst++;
        }

        // 2. copy k bits at a time until there are fewer than k bits remaining
        //    Cheat by neglecting to update src_low since we will only need its
        //    value mod k.
        const int offset = src_low & (k-1);
        bv_coverBool(dst_high >= dst_low + k);
        for ( ; dst_high >= dst_low + k ; dst_low += k, src++, dst++)
            bv_op::assign(dst, (*src >> offset) | (src[1] << (k - offset)));

        // 3. copy the remaining bits (if any)
        bv_if (dst_low < dst_high)
        {
            const int dst_high_modk = dst_high & (k-1);
            bv_assert(dst_high_modk);
            const u_t mask = BITVEC_MASK(dst_high_modk);

            // If the source bits straddle two words, need to do some extra shifting
            bv_if (dst_high_modk > k - offset)
                bv_op::assign(dst, (*src >> offset) | (src[1] << (k - offset)), mask);
            else
                bv_op::assign(dst, *src >> offset, mask);
        }
    }
    else // aligned
    {
        // 1. copy bits until the source and destination are k-bit aligned
        bv_if (dst_low_modk)
        {
            const u_t mask = u_t(-1) << dst_low_modk;
            bv_op::assign(dst, *src, mask);
            dst++;
            src++;
            dst_low += k - dst_low_modk;
        }

        // 2. copy k bits at a time until there are fewer than k bits remaining
        bv_coverBool(dst_high >= dst_low + k);
        for ( ; dst_high >= dst_low + k ; dst_low += k)
            bv_op::assign(dst++, *src++);

        // 3. copy the remaining bits (if any)
        bv_if (dst_low < dst_high)
        {
            const int dst_high_modk = dst_high & (k-1);
            bv_assert(dst_high_modk);
            const u_t mask = BITVEC_MASK(dst_high_modk);
            bv_op::assign(dst, *src, mask);
        }
    }
}

template <int k, EBV_OP op> 
BV_FORCEINLINE void BV_Assign<k,op>::assignToWord (u_t *dst, const u_t *src, int src_low, int len)
{
    bv_assert(len && len <= k);

    // Compute the bitmask for the last word in the copy.
    const u_t final_mask = BITVEC_MASK(len);

    // If src_low is a multiple of k then it's just a copy (*dst = src[i]).
    // Otherwise it's a shift copy (*dst = (src[i] >> X) | (src[i+1] << Y)).
    src += src_low / k;
    const uint16 low_modk = src_low & (k-1);
    bv_if (low_modk)
    {
        const int shl = k - low_modk;

        // There are (k - low_modk) bits in src[0], and we need
        // len bits.  It's a single shift if src[0] has enough bits.
        bv_if (low_modk + len <= k)
            bv_op::assign(dst, src[0] >> low_modk, final_mask);
        else
            bv_op::assign(dst, (src[0] >> low_modk) | (src[1] << shl), final_mask);
    }
    else
        bv_op::assign(dst, *src, final_mask);
}

template <int k, EBV_OP op>
BV_FORCEINLINE void BV_Assign<k,op>::assignFromWord (u_t *dst, int dst_low, u_t src, int len)
{
    bv_assert(len <= k);

    // Code to set the slice [dst_low+len-1:low] using k bit arithmetic.  This is 
    // more complicated than assignToWord() because of the need to preserve existing
    // bits before and after the slice.  
    //
    // If low is a multiple of k then it's a masked assignment.
    //
    // If low is not a multiple of k then there are two cases:
    // 
    // (1) If the entire slice is contained in a single k bit word then it's a 
    //     single masked assignment where the mask of preserved bits includes 
    //     bits both above and below the slice.  
    //
    // (2) Otherwise the slice spans two words.  In this last case the first 
    //     word needs a masked assignment, and the second word needs a masked 
    //     assignment with a single shift from the source.  
    //
    dst += dst_low / k;
    const uint16 low_modk = dst_low & (k-1);
    bv_if (low_modk)
    {
        // Bitmask of bits to set in the first word
        const u_t lowmask = u_t(-1) << low_modk;
        const uint16 high1_modk = (dst_low + len) & (k-1);
        bv_if (low_modk + len <= k)
        {
            // Everything is contained in a single dst word
            const u_t mask = lowmask & BITVEC_MASK(low_modk + len);
            bv_op::assign(dst, src << low_modk, mask);

            bv_coverBool(low_modk + len == k);
        }
        else
        {
            // Need to copy to two words.  Start with the first (k - low_modk) bits.
            bv_op::assign(dst, src << low_modk, lowmask);

            // Now there are (len - k + low_modk) = high1_modk bits left.
            bv_assert(high1_modk);
            const int shr = k - low_modk;
            const u_t mask = BITVEC_MASK(high1_modk);
            bv_op::assign(&dst[1], src >> shr, mask);
        }
    }
    else
    {
        bv_assert(len);
        const u_t mask = BITVEC_MASK(len);
        bv_op::assign(dst, src, mask);
    }
}

////////////////////////////////////////////////////////////////////////
//
// Bit comparison helper function
//
// Fast comparison of arbitrary rhs bits to arbitrary lhs bits
// using a templated primitive type (uint8/uint16/uint32/uint64).
// k = 8, 16, 32 or 64.
//
////////////////////////////////////////////////////////////////////////

#define BV_CHECK_EQUAL(exp) \
{ \
    bool cmp = (exp); \
    bv_coverBool(cmp); \
    equal &= cmp; \
}

template <int k>
struct BV_Compare
{
    typedef typename bv_traits<k>::u_t u_t;
    static BV_FORCEINLINE bool compare (const u_t *lhs, int lhs_low, const u_t *rhs, int rhs_low, int len);

    //----------------------------------
    // compare() can handle everything and is extremely efficient when forceinlined
    // with constants propagated, but not all compilers support the forceinlined
    // construct so manually provide specialization for common cases to improve 
    // their performance with these compilers.
    //----------------------------------

    // Compare to a single u_t.
    static BV_FORCEINLINE bool compareToWord (u_t *lhs, int dst_low, u_t rhs, int len);
};

template <int k>
BV_FORCEINLINE bool BV_Compare<k>::compare (const u_t *lhs, int lhs_low, const u_t *rhs, int rhs_low, int len)
{
    typedef typename bv_traits<k>::bv_t u_t;

    bool equal = true;

    const int lhs_high = lhs_low + len;
    const int lhs_low_modk = lhs_low & (k-1);
    rhs += rhs_low / k;
    lhs += lhs_low / k;
    if (lhs_low_modk + len <= k)
    {
        // lhs is contained in a single k-bit word
        const int lhs_high_modk = (lhs_high - 1) & (k-1);
        const u_t mask = (u_t(-1) << lhs_low_modk) & (u_t(-1) >> ((k-1) - lhs_high_modk));

        if ((rhs_low - lhs_low) & (k-1))
        {
            // unaligned
            const int rhs_low_modk = rhs_low & (k-1);
            const int offset = lhs_low_modk - rhs_low_modk;

            // If we're left-shifting rhs, then it's also in a single word
            if (offset >= 0)
                BV_CHECK_EQUAL(((*lhs ^ u_t(*rhs << offset)) & mask) == 0)

                // Otherwise need to check if rhs crosses a word boundary
            else if (rhs_low_modk + len <= k)
                BV_CHECK_EQUAL(((*lhs ^ (*rhs >> -offset)) & mask) == 0)
            else
                BV_CHECK_EQUAL(((*lhs ^ ((*rhs >> -offset) | (rhs[1] << (k + offset)))) & mask) == 0)
        }

        // aligned (easy)
        else
            equal &= (((*lhs ^ *rhs) & mask) == 0);
    }
    else if ((rhs_low - lhs_low) & (k-1)) // unaligned
    {

        // 1. compare bits until the destination is k-bit aligned
        bv_CoverageItemBool(c1, lhs_low_modk);
        if (lhs_low_modk)
        {
            const u_t mask = u_t(-1) << lhs_low_modk;
            const int rhs_low_modk = rhs_low & (k-1);

            // If the source bits straddle two words, need to do some extra shifting
            const int offset = lhs_low_modk - rhs_low_modk;
            if (offset < 0)
            {
                BV_CHECK_EQUAL(((*lhs ^ ((*rhs >> -offset) | (rhs[1] << (k + offset)))) & mask) == 0)
                rhs++;
            }
            else 
                BV_CHECK_EQUAL(((*lhs ^ (*rhs << offset)) & mask) == 0)

            const int count = k - lhs_low_modk;
            rhs_low += count;
            lhs_low += count;
            lhs++;
        }

        // 2. compare k bits at a time until there are fewer than k bits remaining
        //    Cheat by neglecting to update rhs_low since we will only need its
        //    value mod k.
        const int offset = rhs_low & (k-1);
        bv_CoverageItemBool(c2, lhs_high >= lhs_low + k);
        for ( ; lhs_high >= lhs_low + k ; lhs_low += k, rhs++, lhs++)
            BV_CHECK_EQUAL(*lhs == ((*rhs >> offset) | u_t(rhs[1] << (k - offset))))

        // 3. compare the remaining bits (if any)
        bv_CoverageItemBool(c3, lhs_low < lhs_high);
        bv_coverCrossPredicated(c2 || (c1 && c3), c1, c2, c3);
        if (lhs_low < lhs_high)
        {
            const int lhs_high_modk = lhs_high & (k-1);
            const u_t mask = BITVEC_MASK(lhs_high_modk);
            const int rhs_low_modk = rhs_low & (k-1);

            // If the source bits straddle two words, need to do some extra shifting
            if (lhs_high_modk > k - rhs_low_modk)
                BV_CHECK_EQUAL(((*lhs ^ ((*rhs >> rhs_low_modk) | (rhs[1] << (k - rhs_low_modk)))) & mask) == 0)
            else
                BV_CHECK_EQUAL(((*lhs ^ (*rhs >> rhs_low_modk)) & mask) == 0)
        }
    }
    else
    {
        // 1. compare bits until the source and destination are k-bit aligned
        bv_CoverageItemBool(c1, lhs_low & (k-1));
        if (lhs_low & (k-1))
        {
            const u_t mask = u_t(-1) << (lhs_low & (k-1));
            BV_CHECK_EQUAL(((*lhs ^ *rhs) & mask) == 0)

            lhs++;
            rhs++;
            lhs_low = (lhs_low + (k-1)) & ~(k-1);
        }

        // 2. compare k bits at a time until there are fewer than k bits remaining
        bv_CoverageItemBool(c2, lhs_high >= lhs_low + k);
        for ( ; lhs_high >= lhs_low + k ; lhs_low += k, lhs++, rhs++)
            equal &= (*lhs == *rhs);

        // 3. compare the remaining bits (if any)
        bv_CoverageItemBool(c3, lhs_low < lhs_high);
        bv_coverCrossPredicated(c2 || (c1 && c3), c1, c2, c3);;
        if (lhs_low < lhs_high)
        {
            const u_t mask = BITVEC_MASK(lhs_high & (k-1));
            BV_CHECK_EQUAL(((*lhs ^ *rhs) & mask) == 0)
        }
    }
    return equal;
}

template <int k> 
BV_FORCEINLINE bool BV_Compare<k>::compareToWord (u_t *lhs, int lhs_low, u_t rhs, int len)
{
    bv_assert(len && len <= k);
    bool equal = true;

    // If lhs_low is a multiple of k then it's just a comparison (rhs == lhs[i]).  
    // Otherwise it's a shift comparisons (rhs == (lhs[i] >> X) | (lhs[i+1] << Y)).
    lhs += lhs_low / k;
    const uint16 low_modk = lhs_low & (k-1);
    const u_t mask = BITVEC_MASK(len);
    bv_if (low_modk)
    {
        const int shl = k - low_modk;

        // There are shl bits in lhs[i] that are available to be compared, 
        // and we need to compare len bits.  Do a single shift if there are 
        // enough bits in lhs[i] and a double shift otherwise.
        if (shl < len)
            BV_CHECK_EQUAL(((((lhs[0] >> low_modk) | (lhs[1] << shl)) ^ rhs) & mask) == 0)
        else
            BV_CHECK_EQUAL((((lhs[0] >> low_modk) ^ rhs) & mask) == 0)
    }
    else
    {
        BV_CHECK_EQUAL(((lhs[0] ^ rhs) & mask) == 0)
    }
    return equal;
}

////////////////////////////////////////////////////////////////////////
//
// BV_Bits 
//
// Helper structure used to wrap bitref, bitslice and compound bitvectors
// to implement const-ness correctly, and also to reduce the number of
// combinations when defining concatenations.
//
////////////////////////////////////////////////////////////////////////

template <typename T> struct BV_Bits;

template <typename T>
struct BV_ConstBits : public T
{
    //----------------------------------
    // Every possible constructor
    //----------------------------------

    template <typename T1, typename T2> BV_FORCEINLINE BV_ConstBits (T1 t1, T2 t2) : T(t1, t2) {}
    template <typename T1, typename T2, typename T3> BV_FORCEINLINE BV_ConstBits (T1 t1, T2 t2, T3 t3) : T(t1, t2, t3) {}

    // The compound bit vector constructor proxies need to be declared explicitly
    // in order to get the references and const-ness right (with only the 
    // above generic proxies, the compiler will drop references and create 
    // copies of bitvector objects).
    template <int s1, int s2> BV_FORCEINLINE BV_ConstBits (const bitvec<s1> &high, const bitvec<s2> &low) : T(high, low) { bv_coverLine(); }
    template <int s, typename T1> BV_FORCEINLINE BV_ConstBits (const bitvec<s> &high, const BV_ConstBits<T1> &low) : T(high, low) { bv_coverLine(); }
    template <typename T1, int s> BV_FORCEINLINE BV_ConstBits (const BV_ConstBits<T1> &high, const bitvec<s> &low) : T(high, low) { bv_coverLine(); }
    template <typename T1, typename T2> BV_FORCEINLINE BV_ConstBits (const BV_ConstBits<T1> &high, const BV_ConstBits<T2> &low) : T(high, low) { bv_coverLine(); }

protected:
    template <int s1, int s2> BV_FORCEINLINE BV_ConstBits (bitvec<s1> &high, bitvec<s2> &low) : T(high, low) {}
    template <int s, typename T1> BV_FORCEINLINE BV_ConstBits (bitvec<s> &high, const BV_Bits<T1> &low) : T(high, low) {}
    template <typename T1, int s> BV_FORCEINLINE BV_ConstBits (const BV_Bits<T1> &high, bitvec<s> &low) : T(high, low) {}
    template <typename T1, typename T2> BV_FORCEINLINE BV_ConstBits (const BV_Bits<T1> &high, const BV_Bits<T2> &low) : T(high, low) {}
};

template <typename T>
struct BV_Bits : public BV_ConstBits<T>
{
    //----------------------------------
    // Every possible constructor
    //----------------------------------

    template <typename T1, typename T2> BV_FORCEINLINE BV_Bits (T1 t1, T2 t2) : BV_ConstBits<T>(t1, t2) {}
    template <typename T1, typename T2, typename T3> BV_FORCEINLINE BV_Bits (T1 t1, T2 t2, T3 t3) : BV_ConstBits<T>(t1, t2, t3) {}

    // The compound bit vector constructor proxies need to be declared explicitly
    // in order to get the references and const-ness right (with only the 
    // above generic proxies, the compiler will drop references and create 
    // copies of bitvector objects).
    template <int s1, int s2> BV_FORCEINLINE BV_Bits (bitvec<s1> &high, bitvec<s2> &low) : BV_ConstBits<T>(high, low) { bv_coverLine(); }
    template <int s, typename T1> BV_FORCEINLINE BV_Bits (bitvec<s> &high, const BV_Bits<T1> &low) : BV_ConstBits<T>(high, low) { bv_coverLine(); }
    template <typename T1, int s> BV_FORCEINLINE BV_Bits (const BV_Bits<T1> &high, bitvec<s> &low) : BV_ConstBits<T>(high, low) { bv_coverLine(); }
    template <typename T1, typename T2> BV_FORCEINLINE BV_Bits (const BV_Bits<T1> &high, const BV_Bits<T2> &low) : BV_ConstBits<T>(high, low) { bv_coverLine(); }

    //----------------------------------
    // Assignment operators
    //----------------------------------

    // Required BV_Bits assignment operator
    BV_FORCEINLINE const BV_ConstBits<T> &operator= (const BV_Bits<T> &rhs)
    { 
        bv_coverLine(); 
        this->assign(rhs, BV_ASSIGN); 
        return *this; 
    }

    // Everything else
    template <typename T2>
    BV_FORCEINLINE const BV_ConstBits<T> &operator= (const T2 &rhs)
    { 
        bv_coverLine(); 
        this->assign(rhs, BV_ASSIGN); 
        return *this; 
    }

    //----------------------------------
    // Logical assignment operators
    //----------------------------------

    template <typename T1> 
    BV_FORCEINLINE BV_ConstBits<T> &operator&= (const T1 &rhs)
    { 
        bv_coverLine(); 
        this->assign(rhs, (BV_EOpAssign<BV_OP_AND> *) 0); 
        return *this; 
    }
    template <typename T1> 
    BV_FORCEINLINE BV_ConstBits<T> &operator|= (const T1 &rhs)
    { 
        bv_coverLine(); 
        this->assign(rhs, (BV_EOpAssign<BV_OP_OR> *) 0); 
        return *this; 
    }
    template <typename T1> 
    BV_FORCEINLINE BV_ConstBits<T> &operator^= (const T1 &rhs)
    { 
        bv_coverLine(); 
        this->assign(rhs, (BV_EOpAssign<BV_OP_XOR> *) 0); 
        return *this; 
    }
};

////////////////////////////////////////////////////////////////////////
//
// BV_BitRef - used to implement x[i] for bit vectors.
//
////////////////////////////////////////////////////////////////////////

#define BITVEC_VALIDATE_BIT(b) bv_assert(!(b&~1), "Bit value (%d) out of range", b)

struct BV_BitRef
{
    //----------------------------------
    // Types and constants
    //----------------------------------
    static const int width = 1;

    //----------------------------------
    // Construction
    //----------------------------------
    BV_FORCEINLINE BV_BitRef (uint8 *_data, int index) : data(_data + (index/8)), offset(index & 7) 
    {
    }

    //----------------------------------
    // Conversion to bit
    //----------------------------------
    BV_FORCEINLINE operator _bit () const
    {
        bv_coverLine();
        return (*data >> offset) & 1; 
    }

    //----------------------------------
    // Assignment
    //----------------------------------

    // Assignment from constant (also handles assignment from a bitref
    template <EBV_OP op>
    BV_FORCEINLINE void assign (_bit b, BV_OP)
    {
        bv_CoverageItemBool(item_b, b);
        bv_CoverageItemValues(item_op, op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);
        bv_coverCross(item_b, item_op);

        BITVEC_VALIDATE_BIT(b);
        BV_OpAssign<8, op>::assign(data, b << offset, 1 << offset);
    }

    // Assignment from bit vector
    template <int _size, EBV_OP op>
    BV_FORCEINLINE void assign (const bitvec<_size> &b, BV_OP)
    {
        bv_CoverageItemBool(item_b, b);
        bv_CoverageItemValues(item_op, op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);
        bv_coverCross(item_b, item_op);

        STATIC_ASSERT(_size == 1 || _size == -1);
        BV_OpAssign<8, op>::assign(data, b.val << offset, 1 << offset);
    }

private: 
    // Assignment operator should never be called directly; BV_Bits should
    // always call assign.
    const BV_BitRef &operator= (const BV_BitRef &rhs);
    template <typename T> T &operator= (const T &rhs);

    // Assignment from bit slice is not allowed
    template <int kp, int kv, EBV_OP op>
    void assign (const BV_BitSlice<kp, kv> &b, BV_OP);

    // Assignment from compound is not allowed
    template <typename High, typename Low, typename T>
    void assign (const BV_Compound<High, Low> &, T *);

private:
    uint8 *data;
    int offset;
};

////////////////////////////////////////////////////////////////////////
//
// BV_BitSlice - used to implement x(m,n) for bit vectors and ports
//
// Sample usage:
//
// u23 val23 = val29(26,4);
// val29(26,4) = val23;
//
// Note: Bit slices are inherently unsigned.
//
// kv = value type width   = 8, 16, 32, 64
// kp = pointer type width = 8, 16, 32, 64
//
// Bit slices are also used to implement bitvecref.
//
////////////////////////////////////////////////////////////////////////

// Helper macros
#ifdef _DEBUG
#define BITSLICE_VALIDATE_INDEX(i) bv_assert((i) < width, "Bit index (%d) out of bounds for bitvector of size %d", (i), width)
#else
#define BITSLICE_VALIDATE_INDEX(i)
#endif

template <int kp, int kv = kp>
struct BV_BitSlice
{ 
    //----------------------------------
    // Types and constants
    //----------------------------------
    typedef typename bv_traits<kp>::bv_t p_t; // Pointer type
    typedef typename bv_traits<kv>::bv_t v_t; // Value type

    //----------------------------------
    // Construction
    //----------------------------------
    BV_FORCEINLINE BV_BitSlice (p_t *_data, uint16 _high, uint16 _low) : data(_data), low(_low), width(_high - _low + 1)
    {
        bv_assert(_low <= _high, "Invalid bitslice: high bit index (%d) is smaller than low bit index (%d)", _high, _low);
    }

    //----------------------------------
    // Conversion to u_t
    //----------------------------------
    BV_FORCEINLINE operator v_t () const
    {
        bv_coverValues(kv, 8, 16, 32, 64);
        bv_assert(width <= 64, "Cannot convert bitslice larger than 64 bits to an integer");

        v_t val = 0;
        bv_if (kp == kv)
            BV_Assign<kp, BV_OP_ASSIGN>::assignToWord((p_t *) &val, data, low, width);
        else
            BV_Assign<8, BV_OP_ASSIGN>::assign((byte *) &val, 0, (const byte *) data, low, width);
        return val;
    }

    //----------------------------------
    // Assignment
    //----------------------------------

    // Assignment from v_t (also handles assignment from a bitref)
    template <EBV_OP op>
    BV_FORCEINLINE void assign (v_t val, BV_OP)
    {
        bv_coverValues(op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);
        bv_coverValues(kv, 8, 16, 32, 64);
        bv_assert(width <= 64, "Cannot assign to bitslice larger than 64 bits from an integer");

        // validate that the input is not too large
        bv_assert((v_t(val << (bv_traits<kv>::width - width)) >> (bv_traits<kv>::width - width)) == val,
            "Source value 0x%" PRIx64 " out of range for bitslice of width %d", (uint64) val, width);

        bv_if (kp == bv_traits<kv>::usize)
            BV_Assign<kp, op>::assignFromWord(data, low, val, width);
        else
            BV_Assign<8, op>::assign((byte *) data, low, (const byte *) &val, 0, width);
    }

    // Assignment from a small bit vector
    template <int _size, EBV_OP op> 
    BV_FORCEINLINE void assign (const bitvec_small<_size> &vec, BV_OP)
    {
        bv_coverValues(op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);
        bv_CoverageItemValues(item_k, kp, 8, 16, 32, 64);
        bv_CoverageItemBool(item_size, _size > 0);
        bv_coverCross(item_k, item_size);

        bv_assert(vec.width == width, 
            "Size mismatch: Cannot assign to bitslice of size %d from bitvector of size %d", 
            width, vec.width);

        bv_if (kp == bv_traits<_size>::usize)
            BV_Assign<kp, op>::assignFromWord(data, low, vec.val, width);
        else
            BV_Assign<8, op>::assign((byte *) data, low, (const byte *) &vec.val, 0, width);
    }

    // Assignment from a large bit vector
    template <int _size, EBV_OP op> 
    BV_FORCEINLINE void assign (const bitvec_large<_size> &vec, BV_OP)
    {
        bv_coverValues(op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);

        STATIC_ASSERT(kv == 64 || kv == -64);

        bv_assert(vec.width == width, 
            "Size mismatch: Cannot assign to bitslice of size %d from bitvector of size %d", 
            width, vec.width);
        typedef typename bv_traits<kp>::u_t u_t;
        BV_Assign<kp,op>::assign(data, low, (const u_t *) vec.val, 0, width);
    }

    // Assignment from bitslice
    template <int kp2, int kv2, EBV_OP op>
    BV_FORCEINLINE void assign (const BV_BitSlice<kp2, kv2> &rhs, BV_OP)
    {
        bv_coverValues(op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);
        bv_CoverageItemValues(item_k, kp, 8, 16, 32, 64);
        bv_CoverageItemValues(item_k2, kp2, 8, 16, 32, 64);
        bv_coverCross(item_k, item_k2);

        bv_assert(width == rhs.width, 
            "Size mismatch: Cannot assign to bitslice of size %d from bitslice of size %d",
            width, rhs.width);
        static const int k = (kp == kp2) ? kp : 8;
        typedef typename bv_traits<k>::u_t u_t;
        BV_Assign<k,op>::assign((u_t *) data, low, (const u_t *) rhs.data, rhs.low, width);
    }

    // Assignment from compound bit vector
    template <typename High, typename Low, EBV_OP op>
    BV_FORCEINLINE void assign (const BV_Compound<High, Low> &val, BV_OP)
    {
        bv_coverValues(op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);
        bv_assert(width == val.width, 
            "Size mismatch: Cannot assign to bitslice of size %d from compound bitvector of size %d",
            width, val.width);
        bv_assign(*this, 0, val, 0, width, (BV_OP) 0);
    }

    //----------------------------------
    // Comparison
    //----------------------------------

    // Comparison operators
    template <typename T> 
    BV_FORCEINLINE bool operator== (const T &rhs) const
    { 
        bv_returnCoveredBool(equals(rhs));
    }
    template <typename T> 
    BV_FORCEINLINE bool operator!= (const T &rhs) const
    { 
        bv_returnCoveredBool(!equals(rhs)); 
    }

    // Compare to u_t (also handles comparison to bitref)
    BV_FORCEINLINE bool equals (v_t val) const
    {
        bv_assert(width <= 64, "Cannot compare bitslice larger than 64 bits to an integer");

        bv_if (kp == bv_traits<kv>::usize)
        {
            bv_returnCoveredBool(BV_Compare<kp>::compareToWord(data, low, val, width));
        }
        else
            bv_returnCoveredBool(BV_Compare<8>::compare((const byte *) data, low, (const byte *) &val, 0, width));
    }

    // Compare to small bit vector
    template <int s>
    BV_FORCEINLINE bool equals (const bitvec_small<s> &rhs) const
    {
        bv_CoverageItemValues(item_k, kp, 8, 16, 32, 64);
        bv_CoverageItemBool(item_s, s > 0);
        bv_coverCross(item_k, item_s);

        bv_assert(rhs.width == width, 
            "Size mismatch: Cannot compare bitslice of size %d to bitvector of size %d", 
            width, rhs.width);

        bv_if (kp == bv_traits<s>::usize)
        {
            bv_returnCoveredBool(BV_Compare<kp>::compareToWord(data, low, rhs.val, width));
        }
        else
            bv_returnCoveredBool(BV_Compare<8>::compare((const byte *) data, low, (const byte *) &rhs.val, 0, width));
    }

    // Compare to large bit vector
    template <int s>
    BV_FORCEINLINE bool equals (const bitvec_large<s> &rhs) const
    {
        STATIC_ASSERT(kv == 64);

        bv_assert(rhs.width == width, 
            "Size mismatch: Cannot compare bitslice of size %d to bitvector of size %d", 
            width, rhs.width);
        typedef typename bv_traits<kp>::u_t u_t;
        bv_returnCoveredBool(BV_Compare<kp>::compare(data, low, (const u_t *) &rhs.val, 0, width));
    }

    // Compare to bit slice
    template <int kp2, int kv2>
    BV_FORCEINLINE bool equals (const BV_BitSlice<kp2, kv2> &rhs) const
    {
        bv_CoverageItemValues(item_k, kp, 8, 16, 32, 64);
        bv_CoverageItemValues(item_k2, kp2, 8, 16, 32, 64);
        bv_coverCross(item_k, item_k2);

        bv_assert(width == rhs.width, 
            "Size mismatch: Cannot compare bitslice of size %d to bitslice of size %d", 
            width, rhs.width);
        static const int k = (kp == kp2) ? kp : 8;
        typedef typename bv_traits<k>::u_t u_t;
        bv_returnCoveredBool(BV_Compare<k>::compare((const u_t *) data, low, (const u_t *) rhs.data, rhs.low, width));
    }

    // Compare to compound
    template <typename High, typename Low>
    BV_FORCEINLINE bool equals (const BV_Compound<High, Low> &rhs) const
    {
        bv_assert(width == rhs.width, 
            "Size mismatch: Cannot compare bitslice of size %d to compound bitvector of size %d",
            width, rhs.width);
        bv_returnCoveredBool(bv_compare(*this, 0, rhs, 0, width));
    }
    
    //----------------------------------
    // Bitref and bitslice operators
    //----------------------------------
    BV_FORCEINLINE BV_ConstBits<BV_BitRef> operator[] (int index) const
    { 
        bv_coverLine(); 
        BITSLICE_VALIDATE_INDEX(index); 
        return BV_ConstBits<BV_BitRef>((byte *) data, index + low);
    }
    BV_FORCEINLINE BV_Bits<BV_BitRef> operator[] (int index)
    {
        bv_coverLine();
        BITSLICE_VALIDATE_INDEX(index); 
        return BV_Bits<BV_BitRef>((byte *) data, index + low);
    }
    BV_FORCEINLINE BV_ConstBits<BV_BitSlice<kp, kv> > operator() (int msb, int lsb) const
    {
        bv_coverLine();
        BITSLICE_VALIDATE_INDEX(msb); 
        return BV_ConstBits<BV_BitSlice<kp, kv> >(data, msb + low, lsb + low);
    } 
    BV_FORCEINLINE BV_Bits<BV_BitSlice<kp, kv> > operator() (int msb, int lsb)
    {
        bv_coverLine();
        BITSLICE_VALIDATE_INDEX(msb); 
        return BV_Bits<BV_BitSlice<kp, kv> >(data, msb + low, lsb + low);
    }

    //----------------------------------
    // Other operators
    //----------------------------------

    // Reduction
    BV_FORCEINLINE bool operator! () const
    {
        bv_returnCoveredBool(!bv_reduce_or(*this));
    }

private: 
    // Assignment operator should never be called directly; BV_Bits or
    // bitvecref should always call assign.
    const BV_BitSlice<kv, kp> &operator= (const BV_BitSlice<kv, kp> &rhs);
    template <typename T> T &operator= (const T &rhs);

public: // Make these public so that they're accessible to compound assignments
    p_t *data;
    uint16 low;
    uint16 width;
};

////////////////////////////////////////////////////////////////////////
//
// bitvecref
//
// Allows arbitrary data to be treated as an bitvector.
//
////////////////////////////////////////////////////////////////////////

// Constants for specifying a non-default pointer size
enum bitvecref_ptr
{
    BITVECREF_8BIT_PTR = 8,
    BITVECREF_16BIT_PTR = 16,
    BITVECREF_32BIT_PTR = 32,
    BITVECREF_64BIT_PTR = 64
};

// Use void * for the construction pointer when the internal pointer is byte *,
// since then any data type is safe.  Otherwise, only allow the internal pointer
// type (signed or unsigned) in the constructor.
template <int k> struct bv_bitvecref_traits
{
    STATIC_ASSERT(k>0);
    typedef typename bv_traits<-k>::bv_t s_t;
    typedef typename bv_traits<k>::bv_t  u_t;
};

template <>
struct bv_bitvecref_traits<8>
{
    typedef void s_t;
    typedef byte u_t;
};

// Take care of both masking and sign extension
template <int _size, bool is_signed = (_size < 0), 
          bool required = (bv_traits<_size>::width != bv_traits<_size>::usize)>
struct BV_ClipValue
{
    typedef typename bv_traits<_size>::bv_t bv_t;
    static BV_FORCEINLINE bv_t clip (bv_t val)
    {
        return val;
    }

};

template <int _size>
struct BV_ClipValue<_size, false, true>
{
    typedef typename bv_traits<_size>::bv_t bv_t;
    static BV_FORCEINLINE bv_t clip (bv_t val)
    {
        return val & bv_traits<_size>::mask;
    }
};

template <int _size>
struct BV_ClipValue<_size, true, true>
{
    typedef typename bv_traits<_size>::bv_t bv_t;
    static BV_FORCEINLINE bv_t clip (bv_t val)
    {
        return (val & bv_traits<_size>::mask) | -(val & (bv_t) bv_traits<_size>::msb);
    }
};

// bitvecref
template <int _size, bitvecref_ptr kp = BITVECREF_8BIT_PTR>
struct bitvecref : public BV_Bits<BV_BitSlice<kp, bv_traits<_size>::isize> >
{
    typedef bv_traits<_size> traits;
    typedef BV_ClipValue<_size> clip;
    typedef typename bv_traits<kp>::bv_t p_t;
    typedef typename bv_traits<_size>::bv_t v_t;
    typedef typename bv_bitvecref_traits<kp>::s_t cs_t;
    typedef typename bv_bitvecref_traits<kp>::u_t cu_t;

    //----------------------------------
    // Constructions
    //----------------------------------
    BV_FORCEINLINE bitvecref (cs_t *data, int offset = 0)
        : BV_Bits<BV_BitSlice<kp, traits::isize> >((p_t *) data, offset + traits::width - 1, offset)
    {
        bv_coverLine();
    }
    BV_FORCEINLINE bitvecref (cu_t *data, int offset = 0)
        : BV_Bits<BV_BitSlice<kp, traits::isize> >((p_t *) data, offset + traits::width - 1, offset)
    {
        bv_coverLine();
    }

    //----------------------------------
    // Assignment
    //----------------------------------
    BV_FORCEINLINE const bitvecref &operator= (const bitvecref &rhs)
    {
        bv_coverLine();
        this->assign(rhs, BV_ASSIGN);
        return *this;
    }

    // Return the RHS instead of *this so that multiple bitvectors can
    // be assigned from an integer, e.g. v12 = v47 = v81 = 0;
    template <typename T>
    BV_FORCEINLINE const T &operator= (const T &rhs)
    {
        bv_coverLine();
        this->assign(rhs, BV_ASSIGN);
        return rhs;
    }

    //----------------------------------
    // Logical assignment operators
    //----------------------------------
    template <typename T1> 
    BV_FORCEINLINE bitvecref &operator&= (const T1 &rhs)
    { 
        bv_coverLine(); 
        this->assign(rhs, (BV_EOpAssign<BV_OP_AND> *) 0); 
        return *this; 
    }
    template <typename T1> 
    BV_FORCEINLINE bitvecref &operator|= (const T1 &rhs)
    { 
        bv_coverLine(); 
        this->assign(rhs, (BV_EOpAssign<BV_OP_OR> *) 0); 
        return *this; 
    }
    template <typename T1> 
    BV_FORCEINLINE bitvecref &operator^= (const T1 &rhs)
    { 
        bv_coverLine(); 
        this->assign(rhs, (BV_EOpAssign<BV_OP_XOR> *) 0); 
        return *this; 
    }

    //----------------------------------
    // Standard non-const operators
    //----------------------------------
    BV_FORCEINLINE bitvecref &operator++ ()
    {
        bv_coverLine();
        *this = clip::clip(((v_t) *this) + 1);
        return *this;
    }
    BV_FORCEINLINE v_t operator++ (int)
    {
        bv_coverLine();
        v_t ret = *this;
        *this = clip::clip(((v_t) *this) + 1);
        return ret;
    }
    BV_FORCEINLINE bitvecref &operator-- ()
    {
        bv_coverLine();
        *this = clip::clip(((v_t) *this) - 1);
        return *this;
    }
    BV_FORCEINLINE v_t operator-- (int)
    {
        bv_coverLine();
        v_t ret = *this;
        *this = clip::clip(((v_t) *this) - 1);
        return ret;
    }

#define BITVECREF_ASSIGNOP(op) \
    BV_FORCEINLINE bitvecref operator op##= (v_t rhs) \
    { \
        bv_coverLine(); \
        *this = clip::clip(((v_t) *this) op rhs); \
        return *this; \
    }

    BITVECREF_ASSIGNOP(+)
    BITVECREF_ASSIGNOP(-)
    BITVECREF_ASSIGNOP(*)
    BITVECREF_ASSIGNOP(/)
    BITVECREF_ASSIGNOP(%)
    BITVECREF_ASSIGNOP(<<)
    BITVECREF_ASSIGNOP(>>)
};

template <int _size, bitvecref_ptr kp = BITVECREF_8BIT_PTR>
struct const_bitvecref : public BV_ConstBits<BV_BitSlice<kp, bv_traits<_size>::isize> >
{
    typedef bv_traits<_size> traits;
    typedef typename bv_traits<kp>::bv_t p_t;
    typedef typename bv_bitvecref_traits<kp>::s_t cs_t;
    typedef typename bv_bitvecref_traits<kp>::u_t cu_t;

    //----------------------------------
    // Constructions
    //----------------------------------
    BV_FORCEINLINE const_bitvecref (const cs_t *data, int offset = 0)
        : BV_ConstBits<BV_BitSlice<kp, traits::isize> >((p_t *) data, offset + traits::width - 1, offset)
    {
        bv_coverLine();
    }
    BV_FORCEINLINE const_bitvecref (const cu_t *data, int offset = 0)
        : BV_ConstBits<BV_BitSlice<kp, traits::isize> >((p_t *) data, offset + traits::width - 1, offset)
    {
        bv_coverLine();
    }
};

////////////////////////////////////////////////////////////////////////
//
// Signed/unsigned bit vectors <= 64 bits wide
//
// typedef bitvec<-8>  s8;
// typedef bitvec<8>   u8;
// typedef bitvec<-16> s16;
// typedef bitvec<16>  u16;
// typedef bitvec<-32> s32;
// typedef bitvec<32>  u32;
// typedef bitvec<-64> s64;
// typedef bitvec<64>  u64;
//
// etc...
//
////////////////////////////////////////////////////////////////////////

// Use some template magic to eliminate "comparison will always be true" warnings
// and also make run-time assertion messages as friendly as possible.
template <int size>
struct bv_min
{
    static BV_FORCEINLINE void validate (int64 val)
    {
        static const int64 minval = (~int64(0) << (size-1));
        bv_assert(val >= minval, "Value (%" PRId64 ") is less than minimum bitvector value (%" PRId64 ")", val, minval);
    }
};

template<> struct bv_min<8> { static BV_FORCEINLINE void validate (int64) {} };
template<> struct bv_min<16> { static BV_FORCEINLINE void validate (int64) {} };
template<> struct bv_min<32> { static BV_FORCEINLINE void validate (int64) {} };
template<> struct bv_min<64> { static BV_FORCEINLINE void validate (int64) {} };

template <int size>
struct bv_max_unsigned
{
    static BV_FORCEINLINE void validate (uint64 val)
    {
        static const uint64 maxval = (((uint64) 1) << size) - 1;
        bv_assert(val <= maxval, "Value (0x%" PRIx64 ") exceeds maximum bitvector value (0x%" PRIx64 ")", val, maxval);
    }
};

template<> struct bv_max_unsigned<8> { static BV_FORCEINLINE void validate (uint64) {} };
template<> struct bv_max_unsigned<16> { static BV_FORCEINLINE void validate (uint64) {} };
template<> struct bv_max_unsigned<32> { static BV_FORCEINLINE void validate (uint64) {} };
template<> struct bv_max_unsigned<64> { static BV_FORCEINLINE void validate (uint64) {} };

template <int size>
struct bv_max_signed
{
    static BV_FORCEINLINE void validate (int64 val)
    {
        static const int64 maxval = ((((int64) 1) << (size-1)) - 1);
        bv_assert(val <= maxval, "Value (0x%" PRIx64 ") exceeds maximum bitvector value (0x%" PRIx64 ")", val, maxval);
    }
};

template<> struct bv_max_signed<8> { static BV_FORCEINLINE void validate (int64) {} };
template<> struct bv_max_signed<16> { static BV_FORCEINLINE void validate (int64) {} };
template<> struct bv_max_signed<32> { static BV_FORCEINLINE void validate (int64) {} };
template<> struct bv_max_signed<64> { static BV_FORCEINLINE void validate (int64) {} };

template <int size, typename minval_t>
struct BV_ValidateMinval
{
    static BV_FORCEINLINE void validate (minval_t val) 
    { 
        bv_min<-size>::validate(val); 
    }
};

template<int size> struct BV_ValidateMinval<size, uint64> 
{ 
    static BV_FORCEINLINE void validate (uint64) {} 
};

template <int size, typename maxval_t> struct BV_ValidateMaxval;

template <int size>
struct BV_ValidateMaxval<size, uint64>
{
    static BV_FORCEINLINE void validate (uint64 val) 
    { 
        bv_max_unsigned<size>::validate(val); 
    }
};

template <int size>
struct BV_ValidateMaxval<size, int64>
{
    static BV_FORCEINLINE void validate (int64 val) 
    { 
        bv_max_signed<-size>::validate(val); 
    }
};

// Use templates for sign extension to avoid "unary minus applied to unsigned type" warnings
template <int _size, bool required = (_size < 0) && 
                                     ((-_size) & 63) != 0 &&
                                     _size != -8 && _size != -16 && _size != -32>
struct SignExtend
{
    typedef typename bv_traits<_size>::bv_t bv_t;
    static BV_FORCEINLINE bv_t sign_extend (bv_t val)
    {
        return val;
    }
};

template <int _size>
struct SignExtend<_size, true>
{
    typedef typename bv_traits<_size>::bv_t bv_t;
    static BV_FORCEINLINE bv_t sign_extend (bv_t val)
    {
        const int usize = bv_traits<_size>::usize;
        const int shift = usize - (bv_traits<_size>::width % usize);
        return bv_t(val << shift) >> shift;
    }
};

// Helper macros
#ifdef _DEBUG
#define BITVEC_VALIDATE_MIN(v)   BV_ValidateMinval<_size, typename bv_traits<(_size>0)?64:-64>::bv_t>::validate(v)
#define BITVEC_VALIDATE_MAX(v)   BV_ValidateMaxval<_size, typename bv_traits<(_size>0)?64:-64>::bv_t>::validate(v)
#define BITVEC_VALIDATE(v)       BITVEC_VALIDATE_MIN(v); BITVEC_VALIDATE_MAX(v)
#define BITVEC_VALIDATE_INDEX(i) bv_assert((i) < traits::width, "Bit index (%d) out of bounds for bitvector of size %d", (i), traits::width)
#else
#define BITVEC_VALIDATE(v)
#define BITVEC_VALIDATE_INDEX(i)
#endif

template <int _size>
struct bitvec_small
{
    //----------------------------------
    // Typedefs and constants
    //----------------------------------
    typedef bv_traits<_size> traits;
    typedef typename traits::bv_t bv_t;
    typedef typename traits::u_t u_t;
    typedef SignExtend<_size> se;
    typedef BV_ClipValue<_size> clip;
    static const int width = traits::width;

    //----------------------------------
    // Data
    //----------------------------------
    bv_t val;

    //----------------------------------
    // Constructor
    //----------------------------------
    BV_FORCEINLINE bitvec_small () : val(0)
    { 
        bv_coverLine(); 
    }

    //----------------------------------
    // Integer conversion
    //----------------------------------
    BV_FORCEINLINE operator bv_t () const
    { 
        bv_coverLine(); 

        // Sign extend negative bitvectors only to handle the case where the
        // msb was set directly via a bitref/bitslice.
        bv_t ret = se::sign_extend(val);
        BITVEC_VALIDATE(ret); 
        return ret;
    }

    //----------------------------------
    // Assignment operators
    //----------------------------------

    // Assignment from integer (also handles assignment from a bitref)
    template <EBV_OP op>
    BV_FORCEINLINE void assign (bv_t rhs, BV_OP)
    { 
        bv_coverValues(op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);
        BITVEC_VALIDATE(rhs);
        BV_OpAssign<traits::usize, op>::assign((u_t *) &val, se::sign_extend(rhs));
    }

    // Assignment from bit vector (must be small)
    template <int s2, EBV_OP op> 
    BV_FORCEINLINE void assign (const bitvec_small<s2> &rhs, BV_OP)
    { 
        bv_coverValues(op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);
        bv_coverBool(s2 < 0);
        bv_coverBool(width < rhs.width);
        BV_OpAssign<traits::usize, op>::assign((u_t *) &val, (bv_t) rhs);
        BITVEC_VALIDATE(val);
    }

    // Assignment from bit slice
    template <int kp, int kv, EBV_OP op> 
    BV_FORCEINLINE void assign (const BV_BitSlice<kp, kv> &rhs, BV_OP)
    {
        bv_coverValues(op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);
        bv_CoverageItemValues(item_k, kp, 8, 16, 32, 64);
        bv_CoverageItemBool(item_size, _size > 0);
        bv_coverCross(item_k, item_size);

        bv_assert(width == rhs.width, 
            "Size mismatch: Cannot convert bitslice of size %d to bitvector of size %d", 
            rhs.width, width);

        typedef typename bv_traits<kp>::u_t uu_t;
        bv_if (bv_traits<_size>::usize == kp)
            BV_Assign<kp, op>::assignToWord((uu_t *) &val, (const uu_t *) rhs.data, rhs.low, width);
        else
            BV_Assign<8, op>::assign((byte *) &val, 0, (const byte *) rhs.data, rhs.low, width);
        val = se::sign_extend(val);
        BITVEC_VALIDATE(val);
    }

    //----------------------------------
    // bv_assign helper function
    //----------------------------------
    BV_FORCEINLINE void sign_extend ()
    {
        val = se::sign_extend(val);
        BITVEC_VALIDATE(val);
    }

    //----------------------------------
    // Comparison operators
    //----------------------------------
    BV_FORCEINLINE bool equals (bv_t rhs) const
    {
        bv_returnCoveredBool((bv_t) *this == rhs);
    }
    BV_FORCEINLINE bool equals (bitvec_small<_size> rhs) const
    {
        bv_returnCoveredBool((bv_t) *this == (bv_t) rhs);
    }

    //----------------------------------
    // Standard operators
    //----------------------------------
    BV_FORCEINLINE void operator++ ()
    {
        bv_coverLine();
        val = clip::clip(val + 1); 
    }
    BV_FORCEINLINE void operator-- ()
    {
        bv_coverLine();
        val = clip::clip(val - 1);
    }
    BV_FORCEINLINE void operator+= (bv_t rhs)
    {
        bv_coverLine();
        val = clip::clip(val + rhs);
    }
    BV_FORCEINLINE void operator-= (bv_t rhs)
    {
        bv_coverLine();
        val = clip::clip(val - rhs);
    }
    BV_FORCEINLINE void operator*= (bv_t rhs)
    {
        bv_coverLine();
        val = clip::clip(se::sign_extend(val) * rhs);
    }
    BV_FORCEINLINE void operator/= (bv_t rhs)
    {
        bv_coverLine();
        val = se::sign_extend(val) / rhs; 
    }
    BV_FORCEINLINE void operator%= (bv_t rhs)
    {
        bv_coverLine();
        val = se::sign_extend(val) % rhs; 
    }
    BV_FORCEINLINE void operator<<= (bv_t rhs)
    {
        bv_coverLine();
        val = clip::clip(val << rhs);
    }
    BV_FORCEINLINE void operator>>= (bv_t rhs)
    {
        bv_coverLine();
        val = se::sign_extend(val) >> rhs; 
    }
    BV_FORCEINLINE bitvec<_size> operator~ () const
    { 
        bv_coverBool(_size > 0);
        bv_coverAssert(_size & (_size - 1));
        return (_size > 0) ? (val ^ traits::mask) : ~val; 
    }
};

////////////////////////////////////////////////////////////////////////
//
// Signed/unsigned bit vectors > 64 bits wide
//
// typedef bitvec<-96> s96;
// typedef bitvec<96>  u96;
//
// etc...
//
////////////////////////////////////////////////////////////////////////

template <int _size>
struct bitvec_large
{
    //----------------------------------
    // Typedefs and constants
    //----------------------------------
    typedef bv_traits<_size> traits;
    typedef typename traits::bv_t bv_t;
    static const int width = traits::width;
    typedef SignExtend<_size> se;
    typedef BV_ClipValue<_size> clip;

    //----------------------------------
    // Data
    //----------------------------------
    bv_t val[traits::arraylen];

    //----------------------------------
    // Constructors
    //----------------------------------
    BV_FORCEINLINE bitvec_large ()
    {
        bv_coverLine();
        initialize(0);
    }
    BV_FORCEINLINE void construct (const bv_t *v, int n)
    {
        bv_assert((unsigned) n <= (unsigned) traits::arraylen);
        for (int i = 0 ; i < n ; i++)
            val[i] = v[i];
        initialize(n);
    }

private:
    BV_FORCEINLINE void initialize (int n)
    {
        bv_if (n < traits::arraylen)
        {
            bv_t extend = (n && val[n-1] < 0) ? ~bv_t(0) : 0;
            bv_coverBool(extend);
            for ( ; n < traits::arraylen ; n++)
                val[n] = extend;
        }
        else
            bv_assert((val[n-1] >= traits::minval) && (val[n-1] <= traits::maxval), "Initialization value out of bounds");
    }

public:

    //----------------------------------
    // Assignment operators
    //----------------------------------

    // Assignment from integer (also handles assignment from a bit ref)
    template <EBV_OP op>
    BV_FORCEINLINE void assign (bv_t rhs, BV_OP)
    { 
        bv_coverValues(op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);
        BV_OpAssign<64,op>::assign((uint64 *) val, rhs);
        initialize(1);
    }

    // Assignment from a bit vector (must be same size)
    template <int s2, EBV_OP op> 
    BV_FORCEINLINE void assign (const bitvec<s2> &rhs, BV_OP)
    { 
        STATIC_ASSERT(width == bv_traits<s2>::width);
        bv_coverValues(op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);
        BV_Assign<64, op>::assign((uint64 *) val, 0, (uint64 *) rhs.val, 0, width);
        bv_t *high = &val[traits::arraylen-1];
        *high = se::sign_extend(*high);
    }

    // Assignment from a bit slice
    template <int kp, EBV_OP op> 
    BV_FORCEINLINE void assign (const BV_BitSlice<kp, 64> &rhs, BV_OP)
    {
        bv_coverValues(op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);
        bv_coverBool(_size > 0);

        bv_assert(width == rhs.width, 
            "Size mismatch: Cannot convert bitslice of size %d to bitvector of size %d", 
            rhs.width, width);

        static const int k = (kp == 64) ? 64 : 8;
        typedef typename bv_traits<k>::u_t uu_t;
        BV_Assign<k,op>::assign((uu_t *) val, 0, (const uu_t *) rhs.data, rhs.low, width);
        bv_t *high = &val[traits::arraylen-1];
        *high = se::sign_extend(*high);
    }

    //----------------------------------
    // bv_assign helper function
    //----------------------------------
    BV_FORCEINLINE void sign_extend ()
    {
        bv_t *high = &val[traits::arraylen-1];
        *high = se::sign_extend(*high);
    }

    //----------------------------------
    // Comparison operators
    //----------------------------------

    // Note: use 'if (val != 0)' in place of 'if (val)' for large bitvectors.
    BV_FORCEINLINE bool equals (const bv_t &rhs) const
    {
        bool equal = true;
        BV_CHECK_EQUAL(rhs == val[0])

        bv_t high = (rhs < 0) ? ~bv_t(0) : 0;
        bv_coverBool(high);
        for (int i = 1 ; i < traits::arraylen - 1 ; i++)
            BV_CHECK_EQUAL(val[i] == high)
        BV_CHECK_EQUAL((val[traits::arraylen - 1] & traits::mask) == 
            (high & traits::mask));
        return equal;
    }
    BV_FORCEINLINE bool equals (const bitvec_large<_size> &rhs) const
    {
        bool equal = true;
        for (int i = 0 ; i < traits::arraylen - 1 ; i++)
            BV_CHECK_EQUAL(val[i] == rhs.val[i])
        BV_CHECK_EQUAL((val[traits::arraylen - 1] & traits::mask) == 
                       (rhs.val[traits::arraylen - 1] & traits::mask));
        return equal;
    }

    //----------------------------------
    // Standard operators
    //----------------------------------
    BV_FORCEINLINE bitvec<_size> operator~ () const
    {
        bitvec<_size> ret;
        for (int i = 0 ; i < (width/64) ; i++)
            ret.val[i] = ~val[i];
        bv_coverBool(width & 63);
        bv_if (width & 63)
            ret.val[width/64] = val[width/64] ^ (0x7fffffffffffffffLL >> (63 - (width & 63)));
        return ret;
    }
    BV_FORCEINLINE bitvec<_size> operator| (const bitvec<_size> &rhs) const
    {
        bv_coverLine();
        bitvec<_size> ret;
        for (int i = 0 ; i < traits::arraylen ; i++)
            ret.val[i] = val[i] | rhs.val[i];
        return ret;
    }
    BV_FORCEINLINE bitvec<_size> operator& (const bitvec<_size> &rhs) const
    {
        bv_coverLine();
        bitvec<_size> ret;
        for (int i = 0 ; i < traits::arraylen ; i++)
            ret.val[i] = val[i] & rhs.val[i];
        return ret;
    }
    BV_FORCEINLINE bitvec<_size> operator^ (const bitvec<_size> &rhs) const
    {
        bv_coverLine();
        bitvec<_size> ret;
        for (int i = 0 ; i < traits::arraylen ; i++)
            ret.val[i] = val[i] ^ rhs.val[i];
        return ret;
    }
    BV_FORCEINLINE bool operator! () const
    {
        bool equal = true;
        for (int i = 0 ; i < traits::arraylen ; i++)
            BV_CHECK_EQUAL(!val[i])
        return equal;
    }
    BV_FORCEINLINE bitvec<_size> operator<< (int count) const
    {
        bv_coverLine();
        bitvec<_size> ret;
        shl(count, *this, ret);
        return ret;
    }
    BV_FORCEINLINE bitvec<_size> operator>> (int count) const
    {
        bv_coverLine();
        bitvec<_size> ret;
        shr(count, *this, ret);
        return ret;
    }
    BV_FORCEINLINE void operator<<= (int count)
    {
        bv_coverLine();
        shl(count, *this, *this);
    }
    BV_FORCEINLINE void operator>>= (int count)
    {
        bv_coverLine();
        shr(count, *this, *this);
    }

private:
    static BV_FORCEINLINE void shl (int count, const bitvec_large<_size> &src, bitvec_large<_size> &dst)
    {
        int i = traits::arraylen - 1;
        int shl = count & 63;
        count >>= 6;
        bv_if (shl)
        {
            int shr = 64 - shl;
            for ( ; i > count ; i--)
                dst.val[i] = (src.val[i-count] << shl) | (((uint64) src.val[i-count-1]) >> shr);
            dst.val[i--] = src.val[0] << shl;
        }
        else
        {
            for ( ; i >= count ; i--)
                dst.val[i] = src.val[i-count];
        }
        for ( ; i >= 0 ; i--)
            dst.val[i] = 0;
        dst.val[traits::arraylen-1] = clip::clip(dst.val[traits::arraylen-1]);
    }

    static BV_FORCEINLINE void shr (int count, const bitvec_large<_size> &src, bitvec_large<_size> &dst)
    {
        int i = 0;
        int shr = count & 63;
        count >>= 6;
        int upper = traits::arraylen - count - 1;
        bv_if (shr)
        {
            int shl = 64 - shr;
            for ( ; i < upper ; i++)
                dst.val[i] = (((uint64) src.val[i+count]) >> shr) | (src.val[i+count+1] << shl);
            dst.val[i++] = se::sign_extend(src.val[traits::arraylen-1]) >> shr;
        }
        else
        {
            for ( ; i <= upper ; i++)
                dst.val[i] = src.val[i+count];
        }
        bv_t fill = (src.val[traits::arraylen-1] < 0) ? -1 : 0;
        bv_coverBool(fill);
        for ( ; i < (int) traits::arraylen ; i++)
            dst.val[i] = fill;
    }
};

////////////////////////////////////////////////////////////////////////
//
// Actual bitvec type inherits from either bitvec_small or bitvec_large
//
////////////////////////////////////////////////////////////////////////

template <int size, bool large = (size < -64) || (size > 64)>
struct bitvec_type;

template <int size>
struct bitvec_type<size, true>
{
    // Use the bitvec_large implementation for size > 64
    typedef bitvec_large<size> bitvec_t;

    // Read ports of this type without modification
    typedef bitvec_large<size> port_read_t;
};

template <int size>
struct bitvec_type<size, false>
{
    STATIC_ASSERT(size != 0);

    // Use the bitvec_small implementation for size <= 64
    typedef bitvec_small<size> bitvec_t;

    // Read ports of this type as integers
    typedef typename bitvec_small<size>::bv_t port_read_t;
};

template <int _size>
struct bitvec : public bitvec_type<_size>::bitvec_t
{
    typedef typename bitvec_type<_size>::bitvec_t bitvec_t;
    typedef bv_traits<_size> traits;
    typedef typename traits::bv_t bv_t;
    static const int width = traits::width;

    //----------------------------------
    // Constructors
    //----------------------------------
    BV_FORCEINLINE bitvec ()
    {
        bv_coverLine();
    }
    BV_FORCEINLINE bitvec (bv_t v1)
    {
        assign(v1, BV_ASSIGN);
        bv_coverLine();
    }
    template <typename T> 
    BV_FORCEINLINE bitvec (const BV_ConstBits<T> &rhs)
    {
        assign(rhs, BV_ASSIGN);
        bv_coverLine();
    }
    BV_FORCEINLINE bitvec (bv_t v1, bv_t v2)
    {
        bv_t v[2] = { v2, v1 };
        this->construct(v, 2);
        bv_coverLine();
    }
    BV_FORCEINLINE bitvec (bv_t v1, bv_t v2, bv_t v3)
    {
        bv_t v[3] = { v3, v2, v1 };
        this->construct(v, 3);
        bv_coverLine();
    }
    BV_FORCEINLINE bitvec (bv_t v1, bv_t v2, bv_t v3, bv_t v4)
    {
        bv_t v[4] = { v4, v3, v2, v1 };
        this->construct(v, 4);
        bv_coverLine();
    }
    BV_FORCEINLINE bitvec (bv_t v1, bv_t v2, bv_t v3, bv_t v4, bv_t v5)
    {
        bv_t v[5] = { v5, v4, v3, v2, v1 };
        this->construct(v, 5);
        bv_coverLine();
    }
    BV_FORCEINLINE bitvec (bv_t v1, bv_t v2, bv_t v3, bv_t v4, bv_t v5, bv_t v6)
    {
        bv_t v[6] = { v6, v5, v4, v3, v2, v1 };
        this->construct(v, 6);
        bv_coverLine();
    }
    BV_FORCEINLINE bitvec (bv_t v1, bv_t v2, bv_t v3, bv_t v4, bv_t v5, bv_t v6, bv_t v7)
    {
        bv_t v[7] = { v7, v6, v5, v4, v3, v2, v1 };
        this->construct(v, 7);
        bv_coverLine();
    }
    BV_FORCEINLINE bitvec (bv_t v1, bv_t v2, bv_t v3, bv_t v4, bv_t v5, bv_t v6, bv_t v7, bv_t v8)
    {
        bv_t v[8] = { v8, v7, v6, v5, v4, v3, v2, v1 };
        this->construct(v, 8);
        bv_coverLine();
    }
    BV_FORCEINLINE bitvec (const bv_t *v, int n)
    {
        this->construct(v, n);
        bv_coverLine();
    }
    template <int s2>
    BV_FORCEINLINE bitvec (const bitvec<s2> &rhs)
    {
        bv_coverLine();
        *this = rhs;
    }

    //----------------------------------
    // Assignment
    //----------------------------------

    // Return the RHS instead of *this so that multiple bitvectors can
    // be assigned from an integer, e.g. v12 = v47 = v81 = 0;
    template <typename T>
    BV_FORCEINLINE const T &operator= (const T &rhs)
    {
        bv_coverLine();
        assign(rhs, BV_ASSIGN);
        return rhs;
    }

    using bitvec_t::assign;

    // Assignment from compound bit vector
    template <typename High, typename Low, EBV_OP op> 
    BV_FORCEINLINE void assign (const BV_Compound<High, Low> &rhs, BV_OP)
    {
        bv_coverValues(op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);
        bv_coverBool(_size > 0);
        bv_assert(width == rhs.width, 
            "Size mismatch: cannot convert compound bitvector of size %d to bitvector of size %d",
            rhs.width, width);
        bv_assign(*this, 0, rhs, 0, width, (BV_OP) 0);
        bitvec_t::sign_extend();
    }

    //----------------------------------
    // Logical assignment operators
    //----------------------------------
    template <typename T1> 
    BV_FORCEINLINE bitvec<_size> &operator&= (const T1 &rhs)
    { 
        bv_coverLine(); 
        assign(rhs, (BV_EOpAssign<BV_OP_AND> *) 0); 
        return *this; 
    }
    template <typename T1> 
    BV_FORCEINLINE bitvec<_size> &operator|= (const T1 &rhs)
    { 
        bv_coverLine(); 
        assign(rhs, (BV_EOpAssign<BV_OP_OR> *) 0); 
        return *this; 
    }
    template <typename T1> 
    BV_FORCEINLINE bitvec<_size> &operator^= (const T1 &rhs)
    { 
        bv_coverLine(); 
        assign(rhs, (BV_EOpAssign<BV_OP_XOR> *) 0); 
        return *this; 
    }

    //----------------------------------
    // Comparison
    //----------------------------------
    template <typename T> 
    BV_FORCEINLINE bool operator== (const T &rhs) const
    {
        bv_returnCoveredBool(equals(rhs));
    }
    template <typename T> 
    BV_FORCEINLINE bool operator!= (const T &rhs) const
    {
        bv_returnCoveredBool(!equals(rhs));
    }

    using bitvec_t::equals;

    template <int kp, int kv> 
    BV_FORCEINLINE bool equals (const BV_BitSlice<kp, kv> &rhs) const
    { 
        bv_returnCoveredBool(rhs.equals(*this));
    }
    template <typename High, typename Low> 
    BV_FORCEINLINE bool equals (const BV_Compound<High, Low> &rhs) const
    { 
        bv_returnCoveredBool(rhs.equals(*this)); 
    }

    //----------------------------------
    // Bitref and bitslice operators
    //----------------------------------
    BV_FORCEINLINE BV_ConstBits<BV_BitRef> operator[] (int index) const
    { 
        bv_coverLine(); 
        BITVEC_VALIDATE_INDEX(index); 
        return BV_ConstBits<BV_BitRef>((byte *) &this->val, index);
    }
    BV_FORCEINLINE BV_Bits<BV_BitRef> operator[] (int index)
    {
        bv_coverLine();
        BITVEC_VALIDATE_INDEX(index); 
        return BV_Bits<BV_BitRef>((byte *) &this->val, index);
    }
    BV_FORCEINLINE BV_ConstBits<BV_BitSlice<traits::usize> > operator() (int high, int low) const
    {
        bv_coverLine();
        BITVEC_VALIDATE_INDEX(high); 
        return BV_ConstBits<BV_BitSlice<traits::usize> >((typename traits::u_t *) &this->val, high, low);
    } 
    BV_FORCEINLINE BV_Bits<BV_BitSlice<traits::usize> > operator() (int high, int low)
    {
        bv_coverLine();
        BITVEC_VALIDATE_INDEX(high); 
        return BV_Bits<BV_BitSlice<traits::usize> >((typename traits::u_t *) &this->val, high, low);
    }

    //----------------------------------
    // Standard non-const operators
    //----------------------------------
    BV_FORCEINLINE bitvec<_size> &operator++ ()
    {
        bitvec_t::operator++();
        return *this;
    }
    BV_FORCEINLINE bitvec<_size> operator++ (int)
    {
        bv_coverLine();
        bitvec<_size> ret = *this;
        bitvec_t::operator++();
        return ret;
    }
    BV_FORCEINLINE bitvec<_size> &operator-- ()
    {
        bitvec_t::operator--();
        return *this;
    }
    BV_FORCEINLINE bitvec<_size> operator-- (int)
    {
        bv_coverLine();
        bitvec<_size> ret = *this;
        bitvec_t::operator--();
        return ret;
    }

#define BITVEC_ASSIGNOP(op) \
    BV_FORCEINLINE bitvec<_size> operator op (bv_t rhs) \
    { \
        bitvec_t::operator op (rhs); \
        return *this; \
    }

    BITVEC_ASSIGNOP(+=)
    BITVEC_ASSIGNOP(-=)
    BITVEC_ASSIGNOP(*=)
    BITVEC_ASSIGNOP(/=)
    BITVEC_ASSIGNOP(%=)
    BITVEC_ASSIGNOP(<<=)
    BITVEC_ASSIGNOP(>>=)
};

////////////////////////////////////////////////////////////////////////
//
// Compound bit vectors
//
////////////////////////////////////////////////////////////////////////

template <typename High, typename Low>
struct BV_Compound
{
    //----------------------------------
    // Construction
    //----------------------------------
    BV_FORCEINLINE BV_Compound (High _high, Low _low) : high(_high), low(_low), width(high.width + low.width) {}

    //----------------------------------
    // Conversion to uint64
    //----------------------------------
    BV_FORCEINLINE operator uint64 () const
    {
        bv_coverLine();
        bv_assert(width <= 64, "Cannot convert compound bitvector larger than 64 bits to an integer");
        bitvec<64> ret = 0;
        bv_assign(ret, 0, *this, 0, width, (BV_EOpAssign<BV_OP_ASSIGN> *) 0);
        return ret.val;
    }

    //----------------------------------
    // Assignment
    //----------------------------------

    // Assignment from uint64 (also handles bitref)
    template <EBV_OP op>
    BV_FORCEINLINE void assign (uint64 rhs, BV_OP)
    {
        bv_coverValues(op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);
        bv_assert(width <= 64, "Cannot assign to compound bitvector larger than 64 bits from an integer");

        // validate that the input is not too large
        bv_assert(rhs <= (~uint64(0) >> (64 - width)),
            "Source value 0x%" PRIx64 " too large for compound bitvector of size %d", (uint64) rhs, width);

        bv_assign(*this, 0, (const bitvec<64> &) rhs, 0, width, (BV_OP) 0);
    }

    // Assignment from bitvec
    template <int s, EBV_OP op> 
    BV_FORCEINLINE void assign (const bitvec<s> &rhs, BV_OP)
    {
        bv_coverValues(op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);
        bv_assert(width == bv_traits<s>::width,
            "Size mismatch: cannot assign %d bits to compound bitvector of size %d",
            bv_traits<s>::width, width);
        bv_assign(*this, 0, rhs, 0, width, (BV_OP) 0);
    }

    // Assignment from bitslice
    template <int kp, int kv, EBV_OP op> 
    BV_FORCEINLINE void assign (const BV_BitSlice<kp, kv> &rhs, BV_OP)
    {
        bv_coverValues(op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);
        bv_coverValues(kp, 8, 16, 32, 64);
        bv_assert(width == rhs.width,
            "Size mismatch: cannot assign %d bits to compound bitvector of size %d",
            rhs.width, width);
        bv_assign(*this, 0, rhs, 0, width, (BV_OP) 0);
    }

    // Assignment from compound bitvector
    template <typename High2, typename Low2, EBV_OP op> 
    BV_FORCEINLINE void assign (const BV_Compound<High2, Low2> &rhs, BV_OP)
    {
        bv_coverValues(op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);
        bv_assert(width == rhs.width,
            "Size mismatch: cannot assign %d bits to compound bitvector of size %d",
            rhs.width, width);
        bv_assign(*this, 0, rhs, 0, width, (BV_OP) 0);
    }

    //----------------------------------
    // Comparison
    //----------------------------------

    template <typename T> 
    BV_FORCEINLINE bool operator== (const T &rhs) const
    { 
        bv_returnCoveredBool(equals(rhs)); 
    }
    template <typename T> 
    BV_FORCEINLINE bool operator!= (const T &rhs) const
    { 
        bv_returnCoveredBool(!equals(rhs)); 
    }

    // Comparison to integer (also handles bitref)
    BV_FORCEINLINE bool equals (uint64 val) const
    {
        bv_assert(width <= 64, "Cannot compare compound bitvector larger than 64 bits to an integer");
        return bv_compare(*this, 0, (const bitvec<64> &) val, 0, width);
    }

    // Comparison to bitvec
    template <int s>
    BV_FORCEINLINE bool equals (const bitvec<s> &rhs) const
    {
        bv_assert(width == rhs.width, 
            "Size mismatch: Cannot compare compound bitvector of size %d to %d bits", 
            width, rhs.width);
        bv_returnCoveredBool(bv_compare(*this, 0, rhs, 0, width));
    }

    // Comparison to bit slice
    template <int kp, int kv>
    BV_FORCEINLINE bool equals (const BV_BitSlice<kp, kv> &rhs) const
    {
        bv_assert(width == rhs.width, 
            "Size mismatch: Cannot compare compound bitvector of size %d to %d bits", 
            width, rhs.width);
        bv_returnCoveredBool(bv_compare(*this, 0, rhs, 0, width));
    }

    // Comparison to compound
    template <typename High2, typename Low2>
    BV_FORCEINLINE bool equals (const BV_Compound<High2,Low2> &rhs) const
    {
        bv_assert(width == rhs.width, 
            "Size mismatch: Cannot compare compound bitvector of size %d to %d bits", 
            width, rhs.width);
        bv_returnCoveredBool(bv_compare(*this, 0, rhs, 0, width));
    }

    // Reduction
    BV_FORCEINLINE bool operator! () const
    {
        bv_returnCoveredBool(!bv_reduce_or(*this));
    }

public:
    High high;
    Low low;
    int width;

private: 
    // Assignment operator should never be called directly; it should always
    // be called by BV_Bits via assignBits.
    const BV_Compound<High, Low> &operator= (const BV_Compound<High, Low> &rhs);
    template <typename T> T &operator= (const T &rhs);
};

/////////////////////////////////////////////////////////////////
//
// Concatenation
//
/////////////////////////////////////////////////////////////////

//----------------------------------
// Non-const compound
//----------------------------------

// (bitvec, bitvec)
template <int s1, int s2>
BV_FORCEINLINE BV_Bits<BV_Compound<bitvec<s1> &, bitvec<s2> &> > 
operator, (bitvec<s1> &high, bitvec<s2> &low)
{
    bv_coverLine();
    return BV_Bits<BV_Compound<bitvec<s1> &, bitvec<s2> &> >(high, low);
}

// (bitvec, BV_Bits)
template <int s1, typename T2>
BV_FORCEINLINE BV_Bits<BV_Compound<bitvec<s1> &, T2> > 
operator, (bitvec<s1> &high, const BV_Bits<T2> &low)
{
    bv_coverLine();
    return BV_Bits<BV_Compound<bitvec<s1> &, T2> >(high, low);
}

// (BV_Bits, bitvec)
template <typename T1, int s2>
BV_FORCEINLINE BV_Bits<BV_Compound<T1, bitvec<s2> &> > 
operator, (const BV_Bits<T1> &high, bitvec<s2> &low)
{
    bv_coverLine();
    return BV_Bits<BV_Compound<T1, bitvec<s2> &> >(high, low);
}

// (BV_Bits, BV_Bits)
template <typename T1, typename T2> 
BV_FORCEINLINE BV_Bits<BV_Compound<T1, T2> > 
operator, (const BV_Bits<T1> &high, const BV_Bits<T2> &low)
{
    bv_coverLine();
    return BV_Bits<BV_Compound<T1, T2> >(high, low);
}

//----------------------------------
// Const compound
//----------------------------------

// (bitvec, bitvec)
template <int s1, int s2>
BV_FORCEINLINE BV_ConstBits<BV_Compound<const bitvec<s1> &, const bitvec<s2> &> > 
operator, (const bitvec<s1> &high, const bitvec<s2> &low)
{
    bv_coverLine();
    return BV_ConstBits<BV_Compound<const bitvec<s1> &, const bitvec<s2> &> >(high, low);
}

// (bitvec, BV_ConstBits)
template <int s1, typename T2>
BV_FORCEINLINE BV_ConstBits<BV_Compound<const bitvec<s1> &, T2> > 
operator, (const bitvec<s1> &high, const BV_ConstBits<T2> &low)
{
    bv_coverLine();
    return BV_ConstBits<BV_Compound<const bitvec<s1> &, T2> >(high, low);
}

// (BV_ConstBits, bitvec)
template <typename T1, int s2>
BV_FORCEINLINE BV_ConstBits<BV_Compound<T1, const bitvec<s2> &> > 
operator, (const BV_ConstBits<T1> &high, const bitvec<s2> &low)
{
    bv_coverLine();
    return BV_ConstBits<BV_Compound<T1, const bitvec<s2> &> >(high, low);
}

// (BV_ConstBits, BV_ConstBits)
template <typename T1, typename T2> 
BV_FORCEINLINE BV_ConstBits<BV_Compound<T1, T2> > 
operator, (const BV_ConstBits<T1> &high, const BV_ConstBits<T2> &low)
{
    bv_coverLine();
    return BV_ConstBits<BV_Compound<T1, T2> >(high, low);
}

/////////////////////////////////////////////////////////////////
//
// Compound assignment
//
/////////////////////////////////////////////////////////////////

//----------------------------------
// Assignment to bitvec
//----------------------------------

template <int s1, int s2, EBV_OP op>
BV_FORCEINLINE void bv_assign (bitvec<s1> &dst, int dst_low, const bitvec<s2> &src, int src_low, int len, BV_OP)
{
    bv_coverValues(op, BV_OP_ASSIGN, BV_OP_AND, BV_OP_OR, BV_OP_XOR);
    static const int k = (bv_traits<s1>::usize == bv_traits<s2>::usize) ? bv_traits<s1>::usize : 8;
    bv_coverValues(k, 8, 16, 32, 64);
    typedef typename bv_traits<k>::u_t u_t;
    BV_Assign<k, op>::assign((u_t *) &dst, dst_low, (const u_t *) &src, src_low, len);
    dst.sign_extend();
}

template <int s, EBV_OP op>
BV_FORCEINLINE void bv_assign (bitvec<s> &dst, int dst_low, const BV_BitRef &src, int /* src_low */, int /* len */, BV_OP)
{
    bv_coverLine();
    dst[dst_low].assign(src, (BV_OP) 0);
    dst.sign_extend();
}

template <int s, int kp, int kv, EBV_OP op>
BV_FORCEINLINE void bv_assign (bitvec<s> &dst, int dst_low, const BV_BitSlice<kp, kv> &src, int src_low, int len, BV_OP)
{
    static const int k2 = (bv_traits<s>::usize == kp) ? kp : 8;
    bv_coverValues(k2, 8, 16, 32, 64);
    typedef typename bv_traits<k2>::u_t u_t;
    BV_Assign<k2, op>::assign((u_t *) &dst, dst_low, (const u_t *) src.data, src_low + src.low, len);
    dst.sign_extend();
}

template <int s, typename High, typename Low, EBV_OP op>
BV_INLINE void bv_assign (bitvec<s> &dst, int dst_low, const BV_Compound<High, Low> &src, int src_low, int len, BV_OP)
{
    const int split = src.low.width;
    const int lower_len = split - src_low;
    bv_if (lower_len <= 0)
        bv_assign(dst, dst_low, src.high, -lower_len, len, (BV_OP) 0);
    else
    {
        const int upper_len = len - lower_len;
        bv_if (upper_len <= 0)
            bv_assign(dst, dst_low, src.low, src_low, len, (BV_OP) 0);
        else
        {
            bv_assign(dst, dst_low, src.low, src_low, lower_len, (BV_OP) 0);
            bv_assign(dst, dst_low + lower_len, src.high, 0, upper_len, (BV_OP) 0);
        }
    }
    dst.sign_extend();
}

//----------------------------------
// Assignment to bitref
//----------------------------------

template <int s, EBV_OP op>
BV_FORCEINLINE void bv_assign (BV_BitRef &dst, int /* dst_low */, const bitvec<s> &src, int src_low, int /* len */, BV_OP)
{
    bv_coverLine();
    dst.assign(src[src_low], (BV_OP) 0);
}

template <EBV_OP op>
BV_FORCEINLINE void bv_assign (BV_BitRef &dst, int /* dst_low */, const BV_BitRef &src, int /* src_low */, int /* len */, BV_OP)
{
    bv_coverLine();
    dst.assign(src, (BV_OP) 0);
}

template <int kp, int kv, EBV_OP op>
BV_FORCEINLINE void bv_assign (BV_BitRef &dst, int /* dst_low */, const BV_BitSlice<kp, kv> &src, int src_low, int /* len */, BV_OP)
{
    bv_coverValues(kp, 8, 16, 32, 64);
    src_low += src.low;
    dst.assign((src.data[src_low / kp] >> (src_low & (kp-1))) & 1, (BV_OP) 0);
}

template <typename High, typename Low, EBV_OP op>
BV_INLINE void bv_assign (BV_BitRef &dst, int /* dst_low */, const BV_Compound<High, Low> &src, int src_low, int /* len */, BV_OP)
{
    const int upper_src_low = src_low - src.low.width;
    bv_if (upper_src_low >= 0)
        bv_assign(dst, 0, src.high, upper_src_low, 1, (BV_OP) 0);
    else
        bv_assign(dst, 0, src.low, src_low, 1, (BV_OP) 0);
}

//----------------------------------
// Assignment to bitslice
//----------------------------------

template <int kp, int kv, int s, EBV_OP op>
BV_FORCEINLINE void bv_assign (BV_BitSlice<kp, kv> &dst, int dst_low, const bitvec<s> &src, int src_low, int len, BV_OP)
{
    static const int k2 = (bv_traits<s>::usize == kp) ? kp : 8;
    bv_coverValues(k2, 8, 16, 32, 64);
    typedef typename bv_traits<k2>::u_t u_t;
    BV_Assign<k2, op>::assign((u_t *) dst.data, dst_low + dst.low, (const u_t *) &src, src_low, len);
}

template <int kp, int kv, EBV_OP op>
BV_FORCEINLINE void bv_assign (BV_BitSlice<kp, kv> &dst, int dst_low, const BV_BitRef &src, int /* src_low */, int /* len */, BV_OP)
{
    dst_low += dst.low;
    const int offset = dst_low & (kp-1);
    typedef typename bv_traits<kp>::u_t u_t;
    BV_OpAssign<kp, op>::assign(&dst.data[dst_low/kp], ((u_t) (_bit) src) << offset, ((u_t) 1) << offset);
}

template <int kp1, int kv1, int kp2, int kv2, EBV_OP op>
BV_FORCEINLINE void bv_assign (BV_BitSlice<kp1, kv1> &dst, int dst_low, const BV_BitSlice<kp2, kv2> &src, int src_low, int len, BV_OP)
{
    static const int k3 = (kp1 == kp2) ? kp1 : 8;
    bv_coverValues(k3, 8, 16, 32, 64);
    typedef typename bv_traits<k3>::u_t u_t;
    BV_Assign<k3, op>::assign((u_t *) dst.data, dst_low + dst.low, (const u_t *) src.data, src_low + src.low, len);
}

template <int kp, int kv, typename High, typename Low, EBV_OP op>
BV_INLINE void bv_assign (BV_BitSlice<kp, kv> &dst, int dst_low, const BV_Compound<High, Low> &src, int src_low, int len, BV_OP)
{
    const int split = src.low.width;
    const int lower_len = split - src_low;
    bv_if (lower_len <= 0)
        bv_assign(dst, dst_low, src.high, -lower_len, len, (BV_OP) 0);
    else
    {
        const int upper_len = len - lower_len;
        bv_if (upper_len <= 0)
            bv_assign(dst, dst_low, src.low, src_low, len, (BV_OP) 0);
        else
        {
            bv_assign(dst, dst_low, src.low, src_low, lower_len, (BV_OP) 0);
            bv_assign(dst, dst_low + lower_len, src.high, 0, upper_len, (BV_OP) 0);
        }
    }
}

//----------------------------------
// Assignment to compound
//----------------------------------

template <typename High, typename Low, typename Src, EBV_OP op>
BV_INLINE void bv_assign (BV_Compound<High, Low> &dst, int /* dst_low */, const Src &src, int src_low, int /* len */, BV_OP)
{
    bv_coverLine();
    const int lower_len = dst.low.width;
    bv_assign(dst.low, 0, src, src_low, lower_len, (BV_OP) 0);
    bv_assign(dst.high, 0, src, src_low + lower_len, dst.high.width, (BV_OP) 0);
}

/////////////////////////////////////////////////////////////////
//
// Compound comparison
//
/////////////////////////////////////////////////////////////////

//----------------------------------
// Comparison to bitvec
//----------------------------------

template <int s1, int s2>
BV_FORCEINLINE bool bv_compare (const bitvec<s1> &lhs, int lhs_low, const bitvec<s2> &rhs, int rhs_low, int len)
{
    static const int k = (bv_traits<s1>::usize == bv_traits<s2>::usize) ? bv_traits<s1>::usize : 8;
    bv_coverValues(k, 8, 16, 32, 64);
    typedef typename bv_traits<k>::u_t u_t;
    bv_returnCoveredBool(BV_Compare<k>::compare((const u_t *) &lhs, lhs_low, (const u_t *) &rhs, rhs_low, len));
}

template <int s>
BV_FORCEINLINE bool bv_compare (const bitvec<s> &lhs, int lhs_low, const BV_BitRef &rhs, int /* rhs_low */, int /* len */)
{
    bv_returnCoveredBool((_bit) lhs[lhs_low] == (_bit) rhs);
}

template <int s, int kp, int kv>
BV_FORCEINLINE bool bv_compare (const bitvec<s> &lhs, int lhs_low, const BV_BitSlice<kp, kv> &rhs, int rhs_low, int len)
{
    static const int k2 = (bv_traits<s>::usize < kp) ? bv_traits<s>::usize : kp;
    bv_coverValues(k2, 8, 16, 32, 64);
    typedef typename bv_traits<k2>::u_t u_t;
    bv_returnCoveredBool(BV_Compare<k2>::compare((const u_t *) &lhs, lhs_low, (const u_t *) rhs.data, rhs_low + rhs.low, len));
}

template <int s, typename High, typename Low>
BV_INLINE bool bv_compare (const bitvec<s> &lhs, int lhs_low, const BV_Compound<High, Low> &rhs, int rhs_low, int len)
{
    const int split = rhs.low.width;
    const int lower_len = split - rhs_low;
    if (lower_len <= 0)
        bv_returnCoveredBool(bv_compare(lhs, lhs_low, rhs.high, -lower_len, len));
    const int upper_len = len - lower_len;
    if (upper_len <= 0)
        bv_returnCoveredBool(bv_compare(lhs, lhs_low, rhs.low, rhs_low, len));
    bv_returnCoveredBool(bv_compare(lhs, lhs_low, rhs.low, rhs_low, lower_len) &&
        bv_compare(lhs, lhs_low + lower_len, rhs.high, 0, upper_len));
}

//----------------------------------
// Comparison to bitref
//----------------------------------

template <int s>
BV_FORCEINLINE bool bv_compare (const BV_BitRef &lhs, int /* lhs_low */, const bitvec<s> &rhs, int rhs_low, int /* len */)
{
    bv_returnCoveredBool((_bit) lhs == (_bit) rhs[rhs_low]);
}

BV_FORCEINLINE_GLOBAL bool bv_compare (const BV_BitRef &lhs, int /* lhs_low */, const BV_BitRef &rhs, int /* rhs_low */, int /* len */)
{
    bv_returnCoveredBool((_bit) lhs == (_bit) rhs);
}

template <int kp, int kv>
BV_FORCEINLINE bool bv_compare (const BV_BitRef &lhs, int /* lhs_low */, const BV_BitSlice<kp, kv> &rhs, int rhs_low, int /* len */)
{
    bv_coverValues(kp, 8, 16, 32, 64);
    rhs_low += rhs.low;
    bv_returnCoveredBool((_bit) lhs == ((rhs.data[rhs_low / kp] >> (rhs_low & (kp-1))) & 1));
}

template <typename T1, typename T2>
BV_INLINE bool bv_compare (const BV_BitRef &lhs, int /* lhs_low */, const BV_Compound<T1, T2> &rhs, int rhs_low, int /* len */)
{   
    const int upper_rhs_low = rhs_low - rhs.low.width;
    bv_if (upper_rhs_low >= 0)
        bv_returnCoveredBool(bv_compare(lhs, 0, rhs.high, upper_rhs_low, 1))
    else
        bv_returnCoveredBool(bv_compare(lhs, 0, rhs.low, rhs_low, 1));
}

//----------------------------------
// Comparison to bitslice
//----------------------------------

template <int kp, int kv, int s>
BV_FORCEINLINE bool bv_compare (const BV_BitSlice<kp, kv> &lhs, int lhs_low, const bitvec<s> &rhs, int rhs_low, int len)
{
    static const int k2 = (bv_traits<s>::usize == kp) ? kp : 8;
    bv_coverValues(k2, 8, 16, 32, 64);
    typedef typename bv_traits<k2>::u_t u_t;
    bv_returnCoveredBool(BV_Compare<k2>::compare((const u_t *) lhs.data, lhs_low + lhs.low, (const u_t *) &rhs, rhs_low, len));
}

template <int kp, int kv>
BV_FORCEINLINE bool bv_compare (const BV_BitSlice<kp, kv> &lhs, int lhs_low, const BV_BitRef &rhs, int /* rhs_low */, int /* len */)
{
    lhs_low += lhs.low;
    bv_returnCoveredBool((_bit) rhs == ((lhs.data[lhs_low/kp] >> (lhs_low & (kp-1))) & 1));
}

template <int kp1, int kv1, int kp2, int kv2>
BV_FORCEINLINE bool bv_compare (const BV_BitSlice<kp1, kv1> &lhs, int lhs_low, const BV_BitSlice<kp2, kv2> &rhs, int rhs_low, int len)
{
    static const int k3 = (kp1 < kp2) ? kp1 : kp2;
    bv_coverValues(k3, 8, 16, 32, 64);
    typedef typename bv_traits<k3>::u_t u_t;
    bv_returnCoveredBool(BV_Compare<k3>::compare((const u_t *) lhs.data, lhs_low + lhs.low, (const u_t *) rhs.data, rhs_low + rhs.low, len));
}

template <int kp, int kv, typename High, typename Low>
BV_INLINE bool bv_compare (const BV_BitSlice<kp, kv> &lhs, int lhs_low, const BV_Compound<High, Low> &rhs, int rhs_low, int len)
{
    const int split = rhs.low.width;
    const int lower_len = split - rhs_low;
    if (lower_len <= 0)
        bv_returnCoveredBool(bv_compare(lhs, lhs_low, rhs.high, -lower_len, len));
    const int upper_len = len - lower_len;
    if (upper_len <= 0)
        bv_returnCoveredBool(bv_compare(lhs, lhs_low, rhs.low, rhs_low, len));
    bv_returnCoveredBool(bv_compare(lhs, lhs_low, rhs.low, rhs_low, lower_len) &&
        bv_compare(lhs, lhs_low + lower_len, rhs.high, 0, upper_len));
}

//----------------------------------
// Comparison to compound
//----------------------------------

template <typename High, typename Low, typename Rhs>
BV_INLINE bool bv_compare (const BV_Compound<High, Low> &lhs, int /* lhs_low */, const Rhs &rhs, int rhs_low, int /* len */)
{
    bv_coverLine();
    const int lower_len = lhs.low.width;
    bv_returnCoveredBool(bv_compare(lhs.low, 0, rhs, rhs_low, lower_len) &&
        bv_compare(lhs.high, 0, rhs, rhs_low + lower_len, lhs.high.width));
}

////////////////////////////////////////////////////////////////////////
//
// Reduction operators
//
////////////////////////////////////////////////////////////////////////

//----------------------------------
// Reductions on bitvectors
//----------------------------------

template <int s>
BV_FORCEINLINE _bit bv_reduce_or (const bitvec<s> &val)
{
    bool equal = true;
    const typename bv_traits<s>::u_t *src = (typename bv_traits<s>::u_t *) &val;
    int i;
    bv_coverBool(bv_traits<s>::arraylen - 1);
    for (i = 0 ; i < bv_traits<s>::arraylen - 1 ; i++)
        BV_CHECK_EQUAL(src[i] == 0)
    BV_CHECK_EQUAL((src[i] & bv_traits<s>::mask) == 0)
    return equal ? 0 : 1;
}

template <int s>
BV_FORCEINLINE _bit bv_reduce_and (const bitvec<s> &val)
{
    bool equal = true;
    const typename bv_traits<s>::u_t *src = (typename bv_traits<s>::u_t *) &val;
    int i;
    bv_coverBool(bv_traits<s>::arraylen - 1);
    for (i = 0 ; i < bv_traits<s>::arraylen - 1 ; i++)
        BV_CHECK_EQUAL(src[i] == (~(typename bv_traits<s>::u_t) 0))
    BV_CHECK_EQUAL((src[i] & bv_traits<s>::mask) == bv_traits<s>::mask)
    return equal ? 1 : 0;
}

BV_FORCEINLINE_GLOBAL _bit bv_reduce_xor (uint8 val)
{
    val ^= (val >> 4);
    val ^= (val >> 2);
    val ^= (val >> 1);
    bv_returnCoveredBit(val & 1);
}

BV_FORCEINLINE_GLOBAL _bit bv_reduce_xor (uint16 val)
{
    bv_returnCoveredBit(bv_reduce_xor((uint8) (val ^ (val >> 8))));
}

BV_FORCEINLINE_GLOBAL _bit bv_reduce_xor (uint32 val)
{
    bv_returnCoveredBit(bv_reduce_xor((uint16) (val ^ (val >> 16))));
}

BV_FORCEINLINE_GLOBAL _bit bv_reduce_xor (uint64 val)
{
    bv_returnCoveredBit(bv_reduce_xor((uint32) (val ^ (val >> 32))));
}

template <int s>
BV_FORCEINLINE _bit bv_reduce_xor (const bitvec<s> &val)
{
    const typename bv_traits<s>::u_t *src = (typename bv_traits<s>::u_t *) &val;
    _bit ret = 0;
    int i;
    bv_coverBool(bv_traits<s>::arraylen - 1);
    for (i = 0 ; i < bv_traits<s>::arraylen - 1 ; i++)
        ret ^= bv_reduce_xor(src[i]);
    bv_returnCoveredBit(ret ^ bv_reduce_xor((typename bv_traits<s>::u_t) (src[i] & bv_traits<s>::mask)));
}

//----------------------------------
// Reductions on bitrefs
//----------------------------------

BV_FORCEINLINE_GLOBAL _bit bv_reduce_or (const BV_BitRef &val)
{ 
    bv_returnCoveredBit(val); 
}

BV_FORCEINLINE_GLOBAL _bit bv_reduce_and (const BV_BitRef &val)
{ 
    bv_returnCoveredBit(val); 
}

BV_FORCEINLINE_GLOBAL _bit bv_reduce_xor (const BV_BitRef &val)
{ 
    bv_returnCoveredBit(val); 
}

//----------------------------------
// Reductions on bitslices
//----------------------------------

template <int kp, int kv>
BV_FORCEINLINE _bit bv_reduce_or (const BV_BitSlice<kp, kv> &val)
{
    static const int k = kp;
    typedef typename bv_traits<k>::u_t u_t;
    const int low_modk = val.low & (k-1);
    int i = val.low / k;
    int len = val.width;

    // Special case for low_modk+len < k
    int upper = len + low_modk - k;
    if (upper <= 0)
        bv_returnCoveredBit(((u_t) ((val.data[i] >> low_modk) << (low_modk - upper))) ? 1 : 0);

    if (val.data[i++] >> low_modk)
    {
        bv_coverLine();
        return 1;
    }
    bv_coverBool(upper >= k);
    for (len = upper ; len >= k ; len -= k)
    {
        bv_coverBool(val.data[i]);
        if (val.data[i++])
            return 1;
    }
    bv_returnCoveredBit((len && ((u_t) (val.data[i] << (k - len)))) ? 1 : 0);
}

template <int kp, int kv>
BV_FORCEINLINE _bit bv_reduce_and (const BV_BitSlice<kp, kv> &val)
{
    static const int k = kp;
    typedef typename bv_traits<k>::u_t u_t;
    const int low_modk = val.low & (k-1);
    int i = val.low / k;
    int len = val.width;

    // Special case for low_modk+len < k
    int upper = len + low_modk - k;
    if (upper <= 0)
        bv_returnCoveredBit(((u_t) ((~val.data[i] >> low_modk) << (low_modk - upper))) ? 0 : 1);

    if ((u_t) (~val.data[i++] >> low_modk))
    {
        bv_coverLine();
        return 0;
    }
    bv_coverBool(upper >= k);
    for (len = upper ; len >= k ; len -= k)
    {
        bv_coverBool(val.data[i] != u_t(-1));
        if (val.data[i++] != u_t(-1))
            return 0;
    }
    bv_returnCoveredBit((len && ((u_t) (~val.data[i] << (k - len)))) ? 0 : 1);
}

template <int kp, int kv>
BV_FORCEINLINE _bit bv_reduce_xor (const BV_BitSlice<kp, kv> &val)
{
    static const int k = kp;
    typedef typename bv_traits<k>::u_t u_t;
    const int low_modk = val.low & (k-1);
    int i = val.low / k;
    int len = val.width;

    // Special case for low_modk+len < k
    int upper = len + low_modk - k;
    if (upper <= 0)
        bv_returnCoveredBit(bv_reduce_xor((u_t) ((val.data[i] >> low_modk) << (low_modk - upper))));

    _bit ret = bv_reduce_xor((u_t) (val.data[i++] >> low_modk));
    bv_coverBool(upper >= k);
    for (len = upper ; len >= k ; len -= k)
        ret ^= bv_reduce_xor(val.data[i++]);
    bv_if (len)
        ret ^= bv_reduce_xor((u_t) (val.data[i] << (k - len)));
    bv_returnCoveredBit(ret);
}

//----------------------------------
// Reductions on compounds
//----------------------------------

template <typename High, typename Low>
BV_INLINE _bit bv_reduce_or (const BV_Compound<High, Low> &val)
{
    bv_returnCoveredBit(bv_reduce_or(val.high) || bv_reduce_or(val.low));
}

template <typename High, typename Low>
BV_INLINE _bit bv_reduce_and (const BV_Compound<High, Low> &val)
{
    bv_returnCoveredBit(bv_reduce_and(val.high) && bv_reduce_and(val.low));
}

template <typename High, typename Low>
BV_INLINE _bit bv_reduce_xor (const BV_Compound<High, Low> &val)
{
    bv_returnCoveredBit(bv_reduce_xor(val.high) ^ bv_reduce_xor(val.low));
}

//----------------------------------
// Actual reduction helper structs
//----------------------------------

// reduce_or
struct BV_ReduceOr
{
    BV_FORCEINLINE BV_ReduceOr (_bit init) : val(init) {}

    template <typename T>
    BV_FORCEINLINE BV_ReduceOr &operator, (const T &rhs)
    {
        bv_if (!val)
            val |= bv_reduce_or(rhs);
        return *this;
    }

    _bit val;
};

#define reduce_or(x, ...) bit((BV_ReduceOr(bv_reduce_or(x)), ##__VA_ARGS__).val)

// reduce_and
struct BV_ReduceAnd
{
    BV_FORCEINLINE BV_ReduceAnd (_bit init) : val(init) {}

    template <typename T>
    BV_FORCEINLINE BV_ReduceAnd &operator, (const T &rhs)
    {
        bv_if (val)
            val &= bv_reduce_and(rhs);
        return *this;
    }

    _bit val;
};

#define reduce_and(x, ...) bit((BV_ReduceAnd(bv_reduce_and(x)), ##__VA_ARGS__).val)

// reduce_xor
struct BV_ReduceXor
{
    BV_FORCEINLINE BV_ReduceXor (_bit init) : val(init) {}

    template <typename T>
    BV_FORCEINLINE BV_ReduceXor &operator, (const T &rhs)
    {
        bv_coverBool(val);
        val ^= bv_reduce_xor(rhs);
        return *this;
    }

    _bit val;
};

#define reduce_xor(x, ...) bit((BV_ReduceXor(bv_reduce_xor(x)), ##__VA_ARGS__).val)

/////////////////////////////////////////////////////////////////
//
// popcount()
//
/////////////////////////////////////////////////////////////////

// bitvector
template <int s>
BV_FORCEINLINE int popcount (const bitvec<s> &val)
{
    const typename bv_traits<s>::u_t *src = (typename bv_traits<s>::u_t *) &val;
    int ret = 0;
    for (int i = 0 ; i < bv_traits<s>::arraylen ; i++)
        ret += popcount(src[i]);
    return ret;
}

// bitref
BV_FORCEINLINE_GLOBAL int popcount (const BV_BitRef &val)
{
    bv_returnCoveredBit(val);
}

// bitslice
template <int kp, int kv>
BV_FORCEINLINE int popcount (const BV_BitSlice<kp, kv> &val)
{
    static const int k = kp;
    typedef typename bv_traits<k>::u_t u_t;
    const int low_modk = val.low & (k-1);
    int i = val.low / k;
    int len = val.width;

    // Special case for low_modk+len < k
    int upper = len + low_modk - k;
    bv_if (upper <= 0)
        return popcount((u_t) ((val.data[i] >> low_modk) << (low_modk - upper)));

    int ret = popcount((u_t) (val.data[i++] >> low_modk));
    bv_coverBool(upper >= k);
    for (len = upper ; len >= k ; len -= k)
        ret += popcount(val.data[i++]);
    bv_if (len)
        ret += popcount((u_t) (val.data[i] << (k - len)));
    return ret;
}

// compound
template <typename High, typename Low>
BV_INLINE int popcount (const BV_Compound<High, Low> &val)
{
    return popcount(val.high) + popcount(val.low);
}

/////////////////////////////////////////////////////////////////
//
// lsb()
//
/////////////////////////////////////////////////////////////////

// bitvector
template <int s>
BV_FORCEINLINE int lsb (const bitvec<s> &val)
{
    const typename bv_traits<s>::u_t *src = (typename bv_traits<s>::u_t *) &val;
    for (int i = 0 ; i < bv_traits<s>::arraylen ; i++)
    {
        if (src[i])
            return i * bv_traits<s>::usize + lsb(src[i]);
    }
    return bv_traits<s>::width;
}

// bitref
BV_FORCEINLINE_GLOBAL int lsb (const BV_BitRef &val)
{
    bv_returnCoveredBit(1 - val);
}

// bitslice
template <int kp, int kv>
BV_FORCEINLINE _bit lsb (const BV_BitSlice<kp, kv> &val)
{
    static const int k = kp;
    typedef typename bv_traits<k>::u_t u_t;
    const int low_modk = val.low & (k-1);
    int i = val.low / k;
    int len = val.width;

    // Special case for low_modk+len < k
    int upper = len + low_modk - k;
    bv_if (upper <= 0)
        return lsb(u_t((val.data[i] << -upper) >> (low_modk - upper)));
    
    u_t temp = val.data[i++] >> low_modk;
    bv_if (temp)
        return lsb(temp);
    bv_coverBool(upper >= k);
    int ret = k - low_modk;
    for (len = upper ; len >= k ; len -= k, ret += k, i++)
    {
        bv_if (val.data[i])
            return ret + lsb(val.data[i]);
    }
    return ret + lsb(u_t((val.data[i] << (k - len)) >> (k - len)));
}

// compound
template <typename High, typename Low>
BV_INLINE int lsb (const BV_Compound<High, Low> &val)
{
    int ret = lsb(val.low);
    bv_if (ret == val.low.width)
        ret += lsb(val.high);
    return ret;
}

/////////////////////////////////////////////////////////////////
//
// Conversion to/from string
//
/////////////////////////////////////////////////////////////////

string bv_str_hex_internal (const byte *data, int len);
string bv_str_bits (const byte *data, int shift, int len);
void bv_fromStringInternal (byte *data, int len, const char *s);
void bv_fromBitStringInternal (byte *data, int len, const char *s);

//----------------------------------
// Conversion to hex string
//----------------------------------

template <typename T>
string bv_str_hex (const T &val)
{
    bv_coverLine();
    byte *data = new byte[(val.width + 7)/8];
    BV_BitSlice<8> slice(data, val.width - 1, 0);
    slice.assign(val, BV_ASSIGN);
    string ret = bv_str_hex_internal(data, val.width);
    delete[] data;
    return ret;
}

template <int size>
string str (const bitvec<size> &val)
{
    bv_coverLine();
    return bv_str_hex_internal((const byte *) &val, val.width);
}

template <typename T>
string str (const BV_Bits<T> &val)
{
    bv_coverLine();
    return bv_str_hex(val);
}

template <typename T>
string str (const BV_ConstBits<T> &val)
{
    bv_coverLine();
    return bv_str_hex(val);
}

template <int _size, bitvecref_ptr kp>
string str (const bitvecref<_size, kp> &val)
{
    bv_coverLine();
    return bv_str_hex(val);
}

template <int _size, bitvecref_ptr kp>
string str (const const_bitvecref<_size, kp> &val)
{
    bv_coverLine();
    return bv_str_hex(val);
}

//----------------------------------
// Conversion to binary string
//----------------------------------

template <int size>
string str_bits (const bitvec<size> &val)
{
    bv_coverLine();
    return bv_str_bits((const byte *) &val, 0, val.width);
}

BV_FORCEINLINE_GLOBAL string str_bits (const BV_BitRef &val)
{
    bv_coverBool(val);
    return val ? "1" : "0";
}

template <int kp, int kv>
string str_bits (const BV_BitSlice<kp, kv> &val)
{
    bv_coverLine();
    return bv_str_bits(((const byte *) val.data) + (val.low / 8), val.low & 7, val.width);
}

template <typename High, typename Low>
string str_bits (const BV_Compound<High, Low> &val)
{
    bv_coverLine();
    return str_bits(val.high) + str_bits(val.low);
}

//----------------------------------
// Conversion from hex string
//----------------------------------

template <typename T>
void bv_fromString (T &val, const string &s)
{
    bv_coverLine();
    byte *data = new byte[(val.width + 7)/8];
    bv_fromStringInternal(data, val.width, *s);
    BV_BitSlice<8> slice(data, val.width - 1, 0);
    val = slice;
    delete[] data;
}

template <int size>
void fromString (bitvec<size> &val, const string &s)
{
    bv_coverLine();
    val = 0;
    bv_fromStringInternal((byte *) &val, val.width, *s);
    val.sign_extend();
}

BV_FORCEINLINE_GLOBAL void fromString (BV_Bits<BV_BitRef> val, const string &s)
{
    byte data;
    bv_fromStringInternal(&data, 1, *s);
    val.assign(data, BV_ASSIGN);
    bv_coverBool(data);
}

template <typename T>
void fromString (BV_Bits<T> val, const string &s)
{
    bv_coverLine();
    bv_fromString(val, s);
}

template <int _size, bitvecref_ptr kp>
void fromString (bitvecref<_size, kp> val, const string &s)
{
    bv_coverLine();
    bv_fromString(val, s);
}

//----------------------------------
// Conversion from binary string
//----------------------------------

template <typename T>
void bv_fromBitString (T &val, const string &s)
{
    bv_coverLine();
    byte *data = new byte[(val.width + 7)/8];
    bv_fromBitStringInternal(data, val.width, *s);
    BV_BitSlice<8> slice(data, val.width - 1, 0);
    val.assign(slice, BV_ASSIGN);
    delete[] data;
}

template <int size>
void fromBitString (bitvec<size> &val, const string &s)
{
    bv_coverLine();
    val = 0;
    bv_fromBitStringInternal((byte *) &val, val.width, *s);
    val.sign_extend();
}

BV_FORCEINLINE_GLOBAL void fromBitString (BV_BitRef val, const string &s)
{
    byte data;
    bv_fromBitStringInternal(&data, 1, *s);
    val.assign(data, BV_ASSIGN);
    bv_coverBool(data);
}

template <int kp, int kv>
void fromBitString (BV_BitSlice<kp, kv> val, const string &s)
{
    bv_coverLine();
    return bv_fromBitString(val, s);
}

template <typename High, typename Low>
void fromBitString (BV_Compound<High, Low> val, const string &s)
{
    bv_coverLine();
    return bv_fromBitString(val, s);
}

/////////////////////////////////////////////////////////////////
//
// Port traits
//
/////////////////////////////////////////////////////////////////
#ifdef DECLARE_PORT_SIZE
BEGIN_NAMESPACE_CASCADE

// Size in bits
template <int n>
struct PortSizeInBits<bitvec<n> >
{
    static const uint16 sizeInBits = (n > 0) ? n : -n;
    static const bool exact = true;
};

// Read small bitvector ports as integers
template <int n>
struct PortValueType<bitvec<n> >
{
    typedef bitvec<n> value_t;
    typedef typename bitvec_type<n>::port_read_t read_t;
};

END_NAMESPACE_CASCADE
#endif

/////////////////////////////////////////////////////////////////
//
// Typedefs
//
/////////////////////////////////////////////////////////////////
#include "bv_types.hpp"

/////////////////////////////////////////////////////////////////
//
// Clean up some macros
//
/////////////////////////////////////////////////////////////////
#undef BV_OP
#undef BV_ASSIGN

#ifdef _BV_COVERAGE
END_COVERAGE();
#endif

#endif

