/*
Copyright 2011, D. E. Shaw Research.
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

////////////////////////////////////////////////////////////////////////////////
//
// ThreadFunction.hpp
//
// Copyright (C) 2011 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 09/14/2011
//
// Helper interface used for thread entry function
//
////////////////////////////////////////////////////////////////////////////////

#ifndef ThreadFunction_hpp_737004071327947834
#define ThreadFunction_hpp_737004071327947834

BEGIN_NAMESPACE_DESCORE

struct IThreadFunction
{
    virtual ~IThreadFunction () {}
    virtual void startThread () = 0;
};

//----------------------------------
// Thread arguments
//----------------------------------
template <typename A1>
struct ThreadArgs1
{
    ThreadArgs1 (const A1 &_a1) : a1(_a1) {}
    A1 a1;
};
template <typename A1, typename A2>
struct ThreadArgs2 : public ThreadArgs1<A1>
{
    ThreadArgs2 (const A1 &_a1, const A2 &_a2) : ThreadArgs1<A1>(_a1), a2(_a2) {}
    A2 a2;
};
template <typename A1, typename A2, typename A3>
struct ThreadArgs3 : public ThreadArgs2<A1, A2>
{
    ThreadArgs3 (const A1 &_a1, const A2 &_a2, const A3 &_a3) : ThreadArgs2<A1,A2>(_a1, _a2), a3(_a3) {}
    A3 a3;
};
template <typename A1, typename A2, typename A3, typename A4>
struct ThreadArgs4 : public ThreadArgs3<A1, A2, A3>
{
    ThreadArgs4 (const A1 &_a1, const A2 &_a2, const A3 &_a3, const A4 &_a4) : ThreadArgs3<A1,A2,A3>(_a1, _a2, _a3), a4(_a4) {}
    A4 a4;
};
template <typename A1, typename A2, typename A3, typename A4, typename A5>
struct ThreadArgs5 : public ThreadArgs4<A1, A2, A3, A4>
{
    ThreadArgs5 (const A1 &_a1, const A2 &_a2, const A3 &_a3, const A4 &_a4, const A5 &_a5) : ThreadArgs4<A1,A2,A3,A4>(_a1, _a2, _a3, _a4), a5(_a5) {}
    A5 a5;
};
template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
struct ThreadArgs6 : public ThreadArgs5<A1, A2, A3, A4, A5>
{
    ThreadArgs6 (const A1 &_a1, const A2 &_a2, const A3 &_a3, const A4 &_a4, const A5 &_a5, const A6 &_a6) : ThreadArgs5<A1,A2,A3,A4,A5>(_a1, _a2, _a3, _a4, _a5), a6(_a6) {}
    A6 a6;
};
template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
struct ThreadArgs7 : public ThreadArgs6<A1, A2, A3, A4, A5, A6>
{
    ThreadArgs7 (const A1 &_a1, const A2 &_a2, const A3 &_a3, const A4 &_a4, const A5 &_a5, const A6 &_a6, const A7 &_a7) : ThreadArgs6<A1,A2,A3,A4,A5,A6>(_a1, _a2, _a3, _a4, _a5, _a6), a7(_a7) {}
    A7 a7;
};
template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
struct ThreadArgs8 : public ThreadArgs7<A1, A2, A3, A4, A5, A6, A7>
{
    ThreadArgs8 (const A1 &_a1, const A2 &_a2, const A3 &_a3, const A4 &_a4, const A5 &_a5, const A6 &_a6, const A7 &_a7, const A8 &_a8) : ThreadArgs7<A1,A2,A3,A4,A5,A6,A7>(_a1, _a2, _a3, _a4, _a5, _a6, _a7), a8(_a8) {}
    A8 a8;
};

//----------------------------------
// Non-member function entry points
//----------------------------------
struct ThreadFunction0 : public IThreadFunction
{
    typedef void (*fn_t) ();
    ThreadFunction0 (fn_t _fn) : fn(_fn) {}
    virtual void startThread () { fn(); }
    fn_t fn;
};
template <typename A1, typename X1>
struct ThreadFunction1 : public IThreadFunction
{
    typedef void (*fn_t) (X1);
    ThreadFunction1 (fn_t _fn, const A1 &a1) : fn(_fn), args(a1) {}
    virtual void startThread () { fn(args.a1); }
    fn_t fn;
    ThreadArgs1<A1> args;
};
template <typename A1, typename A2, typename X1, typename X2>
struct ThreadFunction2 : public IThreadFunction
{
    typedef void (*fn_t) (X1, X2);
    ThreadFunction2 (fn_t _fn, const A1 &a1, const A2 &a2) : fn(_fn), args(a1, a2) {}
    virtual void startThread () { fn(args.a1, args.a2); }
    fn_t fn;
    ThreadArgs2<A1, A2> args;
};
template <typename A1, typename A2, typename A3, typename X1, typename X2, typename X3>
struct ThreadFunction3 : public IThreadFunction
{
    typedef void (*fn_t) (X1, X2, X3);
    ThreadFunction3 (fn_t _fn, const A1 &a1, const A2 &a2, const A3 &a3) : fn(_fn), args(a1, a2, a3) {}
    virtual void startThread () { fn(args.a1, args.a2, args.a3); }
    fn_t fn;
    ThreadArgs3<A1, A2, A3> args;
};
template <typename A1, typename A2, typename A3, typename A4, typename X1, typename X2, typename X3, typename X4>
struct ThreadFunction4 : public IThreadFunction
{
    typedef void (*fn_t) (X1, X2, X3, X4);
    ThreadFunction4 (fn_t _fn, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4) : fn(_fn), args(a1, a2, a3, a4) {}
    virtual void startThread () { fn(args.a1, args.a2, args.a3, args.a4); }
    fn_t fn;
    ThreadArgs4<A1, A2, A3, A4> args;
};
template <typename A1, typename A2, typename A3, typename A4, typename A5, typename X1, typename X2, typename X3, typename X4, typename X5>
struct ThreadFunction5 : public IThreadFunction
{
    typedef void (*fn_t) (X1, X2, X3, X4, X5);
    ThreadFunction5 (fn_t _fn, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5) : fn(_fn), args(a1, a2, a3, a4, a5) {}
    virtual void startThread () { fn(args.a1, args.a2, args.a3, args.a4, args.a5); }
    fn_t fn;
    ThreadArgs5<A1, A2, A3, A4, A5> args;
};
template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename X1, typename X2, typename X3, typename X4, typename X5, typename X6>
struct ThreadFunction6 : public IThreadFunction
{
    typedef void (*fn_t) (X1, X2, X3, X4, X5, X6);
    ThreadFunction6 (fn_t _fn, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6) : fn(_fn), args(a1, a2, a3, a4, a5, a6) {}
    virtual void startThread () { fn(args.a1, args.a2, args.a3, args.a4, args.a5, args.a6); }
    fn_t fn;
    ThreadArgs6<A1, A2, A3, A4, A5, A6> args;
};
template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename X1, typename X2, typename X3, typename X4, typename X5, typename X6, typename X7>
struct ThreadFunction7 : public IThreadFunction
{
    typedef void (*fn_t) (X1, X2, X3, X4, X5, X6, X7);
    ThreadFunction7 (fn_t _fn, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6, const A7 &a7) : fn(_fn), args(a1, a2, a3, a4, a5, a6, a7) {}
    virtual void startThread () { fn(args.a1, args.a2, args.a3, args.a4, args.a5, args.a6, args.a7); }
    fn_t fn;
    ThreadArgs7<A1, A2, A3, A4, A5, A6, A7> args;
};
template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename X1, typename X2, typename X3, typename X4, typename X5, typename X6, typename X7, typename X8>
struct ThreadFunction8 : public IThreadFunction
{
    typedef void (*fn_t) (X1, X2, X3, X4, X5, X6, X7, X8);
    ThreadFunction8 (fn_t _fn, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6, const A7 &a7, const A8 &a8) : fn(_fn), args(a1, a2, a3, a4, a5, a6, a7, a8) {}
    virtual void startThread () { fn(args.a1, args.a2, args.a3, args.a4, args.a5, args.a6, args.a7, args.a8); }
    fn_t fn;
    ThreadArgs8<A1, A2, A3, A4, A5, A6, A7, A8> args;
};

//----------------------------------
// Member-function entry points
//----------------------------------
template <typename T>
struct ThreadMemberFunction0 : public IThreadFunction
{
    typedef void (T::*fn_t) ();
    ThreadMemberFunction0 (T *_obj, fn_t _fn) : obj(_obj), fn(_fn) {}
    virtual void startThread () { (obj->*fn)(); }
    T *obj;
    fn_t fn;
};
template <typename T, typename A1, typename X1>
struct ThreadMemberFunction1 : public IThreadFunction
{
    typedef void (T::*fn_t) (X1);
    ThreadMemberFunction1 (T *_obj, fn_t _fn, const A1 &a1) : obj(_obj), fn(_fn), args(a1) {}
    virtual void startThread () { (obj->*fn)(args.a1); }
    T *obj;
    fn_t fn;
    ThreadArgs1<A1> args;
};
template <typename T, typename A1, typename A2, typename X1, typename X2>
struct ThreadMemberFunction2 : public IThreadFunction
{
    typedef void (T::*fn_t) (X1, X2);
    ThreadMemberFunction2 (T *_obj, fn_t _fn, const A1 &a1, const A2 &a2) : obj(_obj), fn(_fn), args(a1, a2) {}
    virtual void startThread () { (obj->*fn)(args.a1, args.a2); }
    T *obj;
    fn_t fn;
    ThreadArgs2<A1, A2> args;
};
template <typename T, typename A1, typename A2, typename A3, typename X1, typename X2, typename X3>
struct ThreadMemberFunction3 : public IThreadFunction
{
    typedef void (T::*fn_t) (X1, X2, X3);
    ThreadMemberFunction3 (T *_obj, fn_t _fn, const A1 &a1, const A2 &a2, const A3 &a3) : obj(_obj), fn(_fn), args(a1, a2, a3) {}
    virtual void startThread () { (obj->*fn)(args.a1, args.a2, args.a3); }
    T *obj;
    fn_t fn;
    ThreadArgs3<A1, A2, A3> args;
};
template <typename T, typename A1, typename A2, typename A3, typename A4, typename X1, typename X2, typename X3, typename X4>
struct ThreadMemberFunction4 : public IThreadFunction
{
    typedef void (T::*fn_t) (X1, X2, X3, X4);
    ThreadMemberFunction4 (T *_obj, fn_t _fn, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4) : obj(_obj), fn(_fn), args(a1, a2, a3, a4) {}
    virtual void startThread () { (obj->*fn)(args.a1, args.a2, args.a3, args.a4); }
    T *obj;
    fn_t fn;
    ThreadArgs4<A1, A2, A3, A4> args;
};
template <typename T, typename A1, typename A2, typename A3, typename A4, typename A5, typename X1, typename X2, typename X3, typename X4, typename X5>
struct ThreadMemberFunction5 : public IThreadFunction
{
    typedef void (T::*fn_t) (X1, X2, X3, X4, X5);
    ThreadMemberFunction5 (T *_obj, fn_t _fn, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5) : obj(_obj), fn(_fn), args(a1, a2, a3, a4, a5) {}
    virtual void startThread () { (obj->*fn)(args.a1, args.a2, args.a3, args.a4, args.a5); }
    T *obj;
    fn_t fn;
    ThreadArgs5<A1, A2, A3, A4, A5> args;
};
template <typename T, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename X1, typename X2, typename X3, typename X4, typename X5, typename X6>
struct ThreadMemberFunction6 : public IThreadFunction
{
    typedef void (T::*fn_t) (X1, X2, X3, X4, X5, X6);
    ThreadMemberFunction6 (T *_obj, fn_t _fn, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6) : obj(_obj), fn(_fn), args(a1, a2, a3, a4, a5, a6) {}
    virtual void startThread () { (obj->*fn)(args.a1, args.a2, args.a3, args.a4, args.a5, args.a6); }
    T *obj;
    fn_t fn;
    ThreadArgs6<A1, A2, A3, A4, A5, A6> args;
};
template <typename T, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename X1, typename X2, typename X3, typename X4, typename X5, typename X6, typename X7>
struct ThreadMemberFunction7 : public IThreadFunction
{
    typedef void (T::*fn_t) (X1, X2, X3, X4, X5, X6, X7);
    ThreadMemberFunction7 (T *_obj, fn_t _fn, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6, const A7 &a7) : obj(_obj), fn(_fn), args(a1, a2, a3, a4, a5, a6, a7) {}
    virtual void startThread () { (obj->*fn)(args.a1, args.a2, args.a3, args.a4, args.a5, args.a6, args.a7); }
    T *obj;
    fn_t fn;
    ThreadArgs7<A1, A2, A3, A4, A5, A6, A7> args;
};
template <typename T, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename X1, typename X2, typename X3, typename X4, typename X5, typename X6, typename X7, typename X8>
struct ThreadMemberFunction8 : public IThreadFunction
{
    typedef void (T::*fn_t) (X1, X2, X3, X4, X5, X6, X7, X8);
    ThreadMemberFunction8 (T *_obj, fn_t _fn, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6, const A7 &a7, const A8 &a8) : obj(_obj), fn(_fn), args(a1, a2, a3, a4, a5, a6, a7, a8) {}
    virtual void startThread () { (obj->*fn)(args.a1, args.a2, args.a3, args.a4, args.a5, args.a6, args.a7, args.a8); }
    T *obj;
    fn_t fn;
    ThreadArgs8<A1, A2, A3, A4, A5, A6, A7, A8> args;
};

END_NAMESPACE_DESCORE

#endif // #ifndef ThreadFunction_hpp_737004071327947834
