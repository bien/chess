 #ifndef GENARRAY_H_
 #define GENARRAY_H_

#include <array>
 
 template<uint64_t... args> 
 struct ArrayHolder {
 	static const uint64_t data[sizeof...(args)];
 };

 template<uint64_t... args>
 const uint64_t ArrayHolder<args...>::data[sizeof...(args)] = { args ... };

 template<size_t N, template<size_t> class F, uint64_t... args>
 struct generate_array_impl {
 	typedef typename generate_array_impl<N-1, F, F<N>::value, args...>::result result;
 };

 template<template<size_t> class F, uint64_t... args>
 struct generate_array_impl<0, F, args...> {
 	typedef ArrayHolder<F<0>::value, args...> result;
 };

 template<size_t N, template<size_t> class F>
 struct generate_array {
 	typedef typename generate_array_impl<N-1, F>::result result;
 };
 
 #endif
 