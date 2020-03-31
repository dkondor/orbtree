/*  -*- C++ -*-
 * vector_stacked.h
 * 
 * Copyright 2018,2020 Daniel Kondor <kondor.dani@gmail.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of the  nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * 
 */


/**
 * \class stacked_vector::vector
 * 
 * \brief C++ vector-like container that internally maintains a "stack"
 * of vectors instead of having one large resizeable storage.
 * 
 * @tparam T Type stored in this vector.
 * @tparam vector_type Container type to use for storing pointers to
 * the individual arrays
 * 
 * Main motivation for this class are the following perceived issues with std::vector:
 * 
 * 1. An std::vector always grows by a fixed factor (typically 2, i.e. doubling
 * 	capacity on most implementations). This is required to avoid O(n^2) copies
 * 	of elements if growing a vector requires copying all elements (see below).
 * 	Thus, if a vector is full with N elements, memory requirement jumps up to
 * 	2*N, which might end up wasting memory space.
 * 
 * 2. Since the stored type in std::vector is not necessarily trivially copyable
 * 	(cannot be moved with memcopy() / memmove()), any change in capacity needs
 * 	to be performed in three steps: 1\. allocate new memory; 2\. copy elements;
 * 	3\. free previous memory. This way, it is not possible to use any optimizations
 * 	offered by realloc() (e.g. using mremap() on Linux for large allocations).
 * 	Growing a vector of size N then actually requires allocating memory in total
 * 	for 3*N elements, and using 2*N elements of it for the duration of the grow.
 * 
 * This class addresses these issues by storing multiple arrays of fixed size
 * instead of one large allocation. These array are stored in a "stack", thus
 * they can be dynamically added or removed. Growing the vector then results
 * in fixed memory allocations and there is no need to copy / move elements.
 * 
 * The downside is that indexing is more complicated, i.e. it includes an extra
 * division and an extra memory lookup operation, thus it can be significantly
 * slower than using a regular vector.
 * 
 * This can be mitigated by optimizing divide operations with the use of the
 * [libdivide library](https://github.com/ridiculousfish/libdivide). To do this,
 * download libdivide.h and place it in the same directory and define
 * USE_LIBDIVIDE (e.g. by the by adding -DUSE_LIBDIVIDE command line argument
 * if compiling with g++).
 * 
 */




#ifndef VECTOR_STACKED_H
#define VECTOR_STACKED_H

#include <limits>
#include <new>
#include <stdexcept>
#include <vector>
#include <type_traits>
#include <iterator>
#include <stdlib.h>
#include <string.h>


/* constexpr if support only for c++17 or newer */
#if __cplusplus >= 201703L
#define CONSTEXPR constexpr
#else
#define CONSTEXPR
#endif

#ifdef USE_LIBDIVIDE
#include "libdivide.h"
namespace stacked_vector {
	
	/// \brief Optional support for the libdivide library to speed up vector element
	/// access. Get it from  https://github.com/ridiculousfish/libdivide and enable
	/// with the USE_LIBDIVIDE define (e.g. by adding -DUSE_LIBDIVIDE command line
	/// option when compiling with g++).
	struct divider {
		size_t n;
		libdivide::divider<size_t> div;
		
		explicit divider(size_t x):n(x),div(x) { }
		
		bool operator == (size_t x) const { return n == x; }
		bool operator != (size_t x) const { return n != x; }
		bool operator < (size_t x) const { return n < x; }
		bool operator <= (size_t x) const { return n <= x; }
		bool operator > (size_t x) const { return n > x; }
		bool operator >= (size_t x) const { return n >= x; }
	};
	
	size_t operator / (size_t x, const divider& d) { return x / d.div; }
	size_t& operator /= (size_t& x, const divider& d) { x /= d.div; return x; }
	size_t operator * (size_t x, const divider& d) { return x * d.n; }
	size_t& operator *= (size_t& x, const divider& d) { x *= d.n; return x; }
	
	size_t operator + (size_t x, const divider& d) { return x + d.n; }
	size_t& operator += (size_t& x, const divider& d) { x += d.n; return x; }
	size_t operator - (size_t x, const divider& d) { return x - d.n; }
	size_t& operator -= (size_t& x, const divider& d) { x -= d.n; return x; }
	
	bool operator == (size_t x, const divider& d) { return d == x; }
	bool operator != (size_t x, const divider& d) { return d != x; }
	bool operator > (size_t x, const divider& d) { return d < x; }
	bool operator >= (size_t x, const divider& d) { return d <= x; }
	bool operator < (size_t x, const divider& d) { return d > x; }
	bool operator <= (size_t x, const divider& d) { return d >= x; }
	
}
#endif


namespace stacked_vector {

		/** \brief helper to distinguish iterators */
		template<class It>
		struct at_least_forward_iterator {
			constexpr static bool value =
				std::is_same< typename std::iterator_traits<It>::iterator_category, typename std::forward_iterator_tag >::value ||
				std::is_same< typename std::iterator_traits<It>::iterator_category, typename std::bidirectional_iterator_tag >::value ||
				std::is_same< typename std::iterator_traits<It>::iterator_category, typename std::random_access_iterator_tag >::value
#if __cplusplus >= 202000L
				|| std::is_same< typename std::iterator_traits<It>::iterator_category, typename std::contiguous_iterator_tag >::value
#endif
				;
		};
		/** \brief helper to distinguish iterators */
		template<class It>
		struct at_least_input_iterator {
			constexpr static bool value = at_least_forward_iterator<It>::value || 
				std::is_same< typename std::iterator_traits<It>::iterator_category, typename std::input_iterator_tag >::value;
		};
		

template <class T>
using std_vector_wrapper = std::vector<T>;

template <class T, template<class> class vector_type = std_vector_wrapper>
class vector {
	protected:
		vector_type<T*> stack;
		size_t p_size; /**< \brief number of elements in vector */
		size_t p_capacity; /**< \brief current capacity of vector */
		size_t p_stack_size; /**< \brief size of one array in the stack */
		
#ifdef USE_LIBDIVIDE
		divider max_grow;
#else
		size_t max_grow; /**< \brief grow memory by maximum this many elements at a time, i.e. maximum value for p_stack_size. Does not change, but not const to allow for swapping / copying / moving vectors with different value. */
#endif
		/// \brief maximum safe capacity to avoid overflow
		static constexpr size_t p_max_capacity = std::numeric_limits<size_t>::max() / sizeof(T);
				
		/// \brief Helper function to copy or move values to new array,
		/// selecting copy or move based on SFINAE
		template<class U = T>
		bool copy_or_move(U* target, size_t new_size,
				typename std::enable_if< std::is_nothrow_move_constructible<U>::value >::type* = 0) {
			size_t i;
			for(i=0;i<p_size;i++) {
				if(i<new_size) new(target + i) U(std::move(stack[0][i]));
				stack[0][i].~U();
			}
			return true;
		}
		template<class U = T>
		bool copy_or_move(U* target, size_t new_size,
				typename std::enable_if< ! std::is_nothrow_move_constructible<U>::value >::type* = 0) {
			size_t copy_size = p_size < new_size ? p_size : new_size;
			U* tmp = std::uninitialized_copy(stack[0], stack[0] + copy_size, target);
			if(tmp != target + copy_size) return false;
			for(size_t i=0;i<p_size;i++) stack[0][i].~U();
		}
		
		/// \brief Reallocate memory in the first stack element to the
		/// given new size, moving / copying elements to the new location.
		/// new_size must be > 0, but can be less than p_size
		bool change_size(size_t new_size) {
			if(new_size > max_grow) return false;
			if(new_size == p_stack_size) return true;
			T* new_array = (T*)malloc(sizeof(T)*new_size);
			if(!new_array) return false;
			if(stack.size() == 0) {
				try {
					stack.push_back(new_array);
				}
				catch(std::bad_alloc& x) {
					free(new_array);
					return false;
				}
			}
			else {
				if(!copy_or_move<T>(new_array,new_size)) {
					free(new_array);
					return false;
				}
				free(stack[0]);
				stack[0] = new_array;
			}
			p_capacity = new_size;
			p_stack_size = new_size;
			if(new_size < p_size) p_size = new_size;
			return true;
		}
		/// \brief Attempt to grow vector either to the given minimum size
		/// or using the default strategy, i.e. doubling size until the
		/// first element in the stack is full and after allocating
		/// max_grow elements at a time.
		bool grow_vector(size_t minimum_size = 0) {
			size_t new_size = minimum_size;
			if(!new_size) {
				if(p_stack_size == max_grow) return grow_one();
				new_size = p_stack_size*2UL;
				if(!new_size) new_size = 1UL;
			}
			if(new_size > max_grow) 
#ifdef USE_LIBDIVIDE
				new_size = max_grow.n;
#else
				new_size = max_grow;
#endif		
			if(!change_size(new_size)) return false;
			while(minimum_size > p_capacity) if(!grow_one()) return false;
			return true;
		}
		
		/// \brief Add one element to the stack
		bool grow_one() {
			// maximum number of elements in stack such that p_capacity will not overflow
			size_t max_stack_size = std::numeric_limits<size_t>::max() / p_stack_size;
			if(stack.size() >= max_stack_size) return false;
			T* new_array = (T*)malloc(sizeof(T)*p_stack_size);
			if(!new_array) return false;
			try {
				stack.push_back(new_array);
			}
			catch(std::bad_alloc& x) {
				free(new_array);
				return false;
			}
			p_capacity += p_stack_size;
			return true;
		}
		
		/// Helper to get internal indices (which stack + position in stack) based on item index.
		std::pair<size_t,size_t> get_indices(size_t i) const {
#ifdef USE_LIBDIVIDE
			size_t i1 = i / max_grow;
			size_t i2 = (i - i1*max_grow);
#else
			size_t i1 = i / max_grow;
			size_t i2 = i % max_grow;
#endif
			return std::pair<size_t,size_t>(i1,i2);
		}
		
		/// \brief Helper to get memory address of element at index. Does not check if index is
		/// in range, it is undefined behavior to call this function with i >= \ref size().
		T* get_addr(size_t i) { auto x = get_indices(i); return stack[x.first] + x.second; }		
		/// \brief Helper to get memory address of element at index. Does not check if index is
		/// in range, it is undefined behavior to call this function with i >= \ref size().
		const T* get_addr(size_t i) const { auto x = get_indices(i); return stack[x.first] + x.second; }		
		/// \brief Helper to get reference of element at index. Does not check if index is
		/// in range, it is undefined behavior to call this function with i >= \ref size().
		T& get_ref(size_t i) { auto x = get_indices(i); return stack[x.first][x.second]; }		
		/// \brief Helper to get reference of element at index. Does not check if index is
		/// in range, it is undefined behavior to call this function with i >= \ref size().
		const T& get_ref(size_t i) const { auto x = get_indices(i); return stack[x.first][x.second]; }
	
	public:
		/* types */
		typedef T value_type;
		typedef size_t size_type;
		typedef ssize_t difference_type;
		typedef T& reference;
		typedef const T& const_reference;
		typedef T* pointer;
		typedef const T* const_pointer; 
		
		/* Constructors */
		
		/** \brief default constructor, creates empty vector, maximum growth is 128k elements */
		vector() noexcept : p_size(0),p_capacity(0),p_stack_size(0),max_grow(131072) { }
		/** \brief constructor to create vector of given size and potentially set maximum growth size */
		explicit vector(size_t count, const T& value = T(), size_t max_grow_ = 131072);
		/** \brief contructor from iterators and optionally setting maximum growth size */
		template<class It, typename std::enable_if<at_least_input_iterator<It>::value, int>::type = 0>
		vector(It first, It last, size_t max_grow_ = 131072);
		/** \brief copy and move constructors */
		vector(const vector& v);
		vector(vector&& v);
		/* 5. assignment */
		vector& operator = (const vector& v);
		vector& operator = (vector&& v);
		/* 6. swap */
		void swap(vector& v);
		
		/* Destructor */
		~vector() {
			/* call destructors of existing elements -- these are not allowed to throw an exception! */
			if(p_size) resize(0);
			for(auto x : stack) free(x);
		}
		
		
		/* Basic access to members */
		
		/// \brief Current size, i.e. the number of elements stored in this vector.
		size_t size() const { return p_size; }
		/// \brief Current capacity, i.e. the number of elements that can be stored without allocating more memory.
		size_t capacity() const { return p_capacity; }
		/// \brief Maximum size to avoid overflow when calculating memory size.
		size_t max_size() const { return p_max_capacity; }
		/// \brief Maximum capacity to avoid overflow when calculating memory size.
		size_t max_capacity() const { return p_max_capacity; }
		/// \brief Returns true if the vector is empty.
		bool empty() const { return p_size == 0; }
		/// \brief Get the maximum growth size. Separate arrays are allocated by this amount.
		size_t get_stack_array_size() const {
#ifdef USE_LIBDIVIDE
			return max_grow.n;
#else
			return max_grow;
#endif
		}
		
		
		
		/* data access functions
		 * similarly to std::vector, only at() checks bounds, all other versions
		 * result in undefined behavior if an out of bounds is attempted */
		
		/// \brief Access the ith element. It is undefined behavior to if i >= size()
		T& operator[](size_t i) { return get_ref(i); }
		/// \brief Access the ith element. It is undefined behavior to if i >= size()
		const T& operator[](size_t i) const { return get_ref(i); }
		/// \brief Access the ith element with bounds checking, throws an exception if i >= size()
		T& at(size_t i) { if(i < p_size) return get_ref(i); throw std::out_of_range("vector::at(): index out of range!\n"); }
		/// \brief Access the ith element with bounds checking, throws an exception if i >= size()
		const T& at(size_t i) const { if(i < p_size) return get_ref(i); throw std::out_of_range("vector::at(): index out of range!\n"); }
		/// \brief Access the first element. It is undefined behavior if this function is called on an empty vector.
		T& front() { return stack[0][0]; }
		/// \brief Access the first element. It is undefined behavior if this function is called on an empty vector.
		const T& front() const { return stack[0][0]; }
		/// \brief Access the last element. It is undefined behavior if this function is called on an empty vector.
		T& back() { return get_ref(p_size-1); }
		/// \brief Access the last element. It is undefined behavior if this function is called on an empty vector.
		const T& back() const { return get_ref(p_size-1); }
		
		/* reserve memory */
		/// \brief Reserve memory for at least n elements. Returns true if allocation was successfull, false otherwise.
		bool reserve_nothrow(size_t n);
		/// \brief Reserve memory for at least n elements. Throws an exception if memory allocation is not successful.
		void reserve(size_t n) { if(!reserve_nothrow(n)) throw std::bad_alloc(); }
		/// \brief Free up unused memory, keeping at least the given new_capacity (if new_capacity > \ref size()).
		void shrink_to_fit(size_t new_capacity = 0);
		
		/* insert /create elements at the end of the vector
		 * versions that throw an exception if out of memory */
		/// \brief Insert element at the end of the vector. Can throw an exception if memory allocation fails.
		void push_back(const T& x);
		/// \brief Insert element at the end of the vector. Can throw an exception if memory allocation fails.
		void push_back(T&& x);
		/// \brief Construct an element at the end of the vector. Can throw an exception if memory allocation fails.
		template<class... Args> T& emplace_back(Args&&... args);
		/* versions that return false if out of memory -- note: any exceptions
		 * from T's (copy / move) constructor are propagated, i.e. still can
		 * throw an exception if those throw */
		/// \brief Insert element at the end of the vector. Does not throw exception, return value indicates if insert was successful.
		bool push_back_nothrow(const T& x);
		/// \brief Insert element at the end of the vector. Does not throw exception, return value indicates if insert was successful.
		bool push_back_nothrow(T&& x);
		/// \brief Construct an element at the end of the vector. Does not throw exception, return value indicates if insert was successful.
		template<class... Args> bool emplace_back_nothrow(Args&&... args);
		
		/* remove elements -- T's destructor should not throw exception! */
		/// \brief Removes all elements; does not free up memory, use \ref shrink_to_fit() for that.
		void clear() { while(p_size) pop_back(); }
		/// \brief Removes the last element; does not free up memory, use \ref shrink_to_fit() for that.
		void pop_back() {
			if(p_size) {
				--p_size;
				get_ref(p_size).~T();
			}
		}
		
		/// \brief Resize vector. If new size is larger than current size, new elements are default constructed.
		/// Does not throw exception on memory allocation error, return value indicates if resize was successful.
		bool resize_nothrow(size_t count);
		/// \brief Resize vector. If new size is larger than current size, new elements are inserted as copies of x.
		/// Does not throw exception on memory allocation error, return value indicates if resize was successful.
		bool resize_nothrow(size_t count, const T& x);
		/// \brief Resize vector. If new size is larger than current size, new elements are default constructed.
		/// Can throw an exception on memory allocation error.
		void resize(size_t count) { if(!resize_nothrow(count)) throw std::bad_alloc(); }
		/// \brief Resize vector. If new size is larger than current size, new elements are inserted as copies of x.
		/// Can throw exception on memory allocation error.
		void resize(size_t count, const T& x) { if(!resize_nothrow(count,x)) throw std::bad_alloc(); }
	
	protected:	
		/// \brief Iterators store a reference to this class and a position
		template<bool is_const>
		class iterator_base {
			public:
				typedef std::random_access_iterator_tag iterator_category;
				typedef T value_type;
				typedef T* pointer;
				typedef T& reference;
				typedef ssize_t difference_type;
				/*
      typedef typename _Iterator::iterator_category iterator_category;
      typedef typename _Iterator::value_type        value_type;
      typedef typename _Iterator::difference_type   difference_type;
      typedef typename _Iterator::pointer           pointer;
      typedef typename _Iterator::reference         reference;
    */
    
				friend class iterator_base<!is_const>;
				friend class vector<T,vector_type>;
			
			protected:
				typedef typename std::conditional<is_const, const vector, vector>::type vector_type1;
				vector_type1* v;
				size_t pos;
				
				/* make non-const iterator from const iterator -- only possible from within the vector 
				template<bool is_const_ = is_const>
				iterator_base(const iterator_base<true>& it, typename std::enable_if<is_const_>::type* = 0 ):v(it.v),pos(it.pos) { } */
								
			public:
				iterator_base():v(0),pos(0) { }
				explicit iterator_base(vector_type1& v_, size_t pos_ = 0):v(&v_),pos(pos_) { }
				explicit iterator_base(vector_type1* v_, size_t pos_ = 0):v(v_),pos(pos_) { }
				
				/* any iterator can be copied from non-const iterator;
				 * this is not explicit so comparison operators can auto-convert */
				iterator_base(const iterator_base<false>& it):v(it.v),pos(it.pos) { }
				/* only const iterator can be copied from const iterator */
				template<bool is_const_ = is_const>
				iterator_base(const iterator_base<true>& it, typename std::enable_if<is_const_>::type* = 0 ):v(it.v),pos(it.pos) { }
				
				
				
				/// access to values
				reference operator * () {
					return (*v)[pos];
				}
				/// read-only access values
				pointer operator -> () {
					return &((*v)[pos]);
				}
				/// compare iterators
				/** Note: comparing iterators from different vector
				 * is undefined behavior, it is not explicitely tested */
				template<bool is_const2> bool operator == (const iterator_base<is_const2>& i) const { return pos == i.pos; }
				/// compare iterators
				/** Note: comparing iterators from different map / tree
				 * is undefined behavior, it is not explicitely tested */
				template<bool is_const2> bool operator != (const iterator_base<is_const2>& i) const { return pos != i.pos; }
				
				/// compare iterators
				template<bool is_const2> bool operator < (const iterator_base<is_const2>& i) const { return pos < i.pos; }
				/// compare iterators
				template<bool is_const2> bool operator > (const iterator_base<is_const2>& i) const { return pos > i.pos; }
				/// compare iterators
				template<bool is_const2> bool operator <= (const iterator_base<is_const2>& i) const { return pos <= i.pos; }
				/// compare iterators
				template<bool is_const2> bool operator >= (const iterator_base<is_const2>& i) const { return pos >= i.pos; }
				
				/// increment: move to the next stored node
				iterator_base& operator ++() { ++pos; return *this; }
				/// increment: move to the next stored node
				iterator_base operator ++(int) { iterator_base<is_const> i(*this); ++pos; return i; }
				/// decrement: move to the previous stored node
				iterator_base& operator --() { --pos; return *this; }
				/// decrement: move to the previous stored node
				iterator_base operator --(int) { iterator_base<is_const> i(*this); --pos; return i; }
				
				/// move forward by the given number of steps
				iterator_base& operator +=(ssize_t i) { pos += i; return *this; }
				/// move backward
				iterator_base& operator -=(ssize_t i) { pos -= i; return *this; }
				iterator_base operator +(ssize_t i) const { iterator_base<is_const> it(*this); it += i; return it; }
				iterator_base operator -(ssize_t i) const { iterator_base<is_const> it(*this); it -= i; return it; }
				
				/// Calculate the difference between two iterators. This has undefined behavior if the subtraction overflows.
				template<bool is_const2> ssize_t operator - (const iterator_base<is_const2>& i) const {
					return ((ssize_t)pos) - ((ssize_t)(i.pos));
				}
				
				typename std::conditional<is_const, const reference, reference>::type operator [] (ssize_t i) { return (*v)[pos+i]; }
		};
		
		/// \brief Helper for the insert functions -- moves elements starting from pos to new_pos,
		/// allocating memory if necessary. The caller must ensure that new_pos >= pos and p_size > pos
		bool insert_helper(size_t pos, size_t new_pos);
		
		/// \brief Helper to create an iterator based on a position.
		iterator_base<false> make_iterator(size_t pos) { return iterator_base<false>(*this,pos); }
		/// \brief Helper to make a non-const copy of a const iterator -- only possible if this class is not const.
		iterator_base<false> make_iterator(iterator_base<true> pos) { return iterator_base<false>(*this,pos.pos); }
		/// \brief Helper to create an iterator based on a position.
		iterator_base<true> make_iterator(size_t pos) const { return iterator_base<true>(*this,pos); }
	
	public:
		typedef iterator_base<false> iterator;
		typedef iterator_base<true> const_iterator;
		
		iterator begin() { return iterator(*this,0); } ///< \brief Iterator to the beginning
		const_iterator begin() const { return const_iterator(*this,0); } ///< \brief Iterator to the beginning
		const_iterator cbegin() const { return const_iterator(*this,0); } ///< \brief Iterator to the beginning
		iterator end() { return iterator(*this,p_size); } ///< \brief Iterator to the end
		const_iterator end() const { return const_iterator(*this,p_size); } ///< \brief Iterator to the end
		const_iterator cend() const { return const_iterator(*this,p_size); } ///< \brief Iterator to the end
		
		iterator erase(const_iterator pos); ///< \brief Erase element at the given position
		iterator erase(const_iterator first, const_iterator last); ///< \brief Erase elements in the given range
		
		/* inserts that do not throw exception when out of memory,
		 * instead they return whether the insert was successful
		 * the res iterator is updated to point to the inserted element
		 * if the insert is successful, otherwise it is not changed */
		 
		/** \brief Inserts a copy of x at the given position. Does not throw an
		 * exception on memory allocation failure, the return value indicates if
		 * it was successful. The res iterator is updated to point to the inserted
		 * element if successful, otherwise it is not changed. */
		bool insert_nothrow(const_iterator pos, iterator& res, const T& x);
		/** \brief Inserts x at the given position. Does not throw an
		 * exception on memory allocation failure, the return value indicates if
		 * it was successful. The res iterator is updated to point to the inserted
		 * element if successful, otherwise it is not changed. */
		bool insert_nothrow(const_iterator pos, iterator& res, T&& x);
		/** \brief Inserts count copies of x at the given position. Does not throw an
		 * exception on memory allocation failure, the return value indicates if
		 * it was successful. The res iterator is updated to point to the inserted
		 * element if successful, otherwise it is not changed. */
		bool insert_nothrow(const_iterator pos, iterator& res, size_t count, const T& x);
		/** \brief Inserts the elements in the range [first,last) at pos. Does not throw an
		 * exception on memory allocation failure, the return value indicates if
		 * it was successful. The res iterator is updated to point to the inserted
		 * element if successful, otherwise it is not changed. */
		template<class InputIt, typename std::enable_if<at_least_input_iterator<InputIt>::value, int>::type = 0>
		bool insert_nothrow(const_iterator pos, iterator& res, InputIt first, InputIt last);

		/// \brief Inserts a copy of x at pos. Can throw an exception if out of memory.
		iterator insert(const_iterator pos, const T& x) {
			iterator res;
			if(insert_nothrow(pos,res,x)) return res;
			else throw std::bad_alloc();
		}
		/// \brief Inserts x at pos. Can throw an exception if out of memory.
		iterator insert(const_iterator pos, T&& x) {
			iterator res;
			if(insert_nothrow(pos,res,std::forward<T>(x))) return res;
			else throw std::bad_alloc();
		}
		/// \brief Inserts count copies of x at pos. Can throw an exception if out of memory.
		iterator insert(const_iterator pos, size_t count, const T& x) {
			iterator res;
			if(insert_nothrow(pos,res,count,x)) return res;
			else throw std::bad_alloc();
		}
		/// \brief Inserts the elements in the range [first,last) at pos. Can throw an exception if out of memory.
		template<class InputIt, typename std::enable_if<at_least_input_iterator<InputIt>::value, int>::type = 0>
		iterator insert(const_iterator pos, InputIt first, InputIt last) {
			iterator res;
			if(insert_nothrow(pos,res,first,last)) return res;
			else throw std::bad_alloc();
		}
};


template <class T, template<class> class vector_type>
auto operator + (ssize_t i, const typename vector<T,vector_type>::iterator& it) -> typename vector<T,vector_type>::iterator {
	typename vector<T,vector_type>::iterator it2(it);
	it2 += i;
	return it2;
}

template <class T, template<class> class vector_type>
auto operator + (ssize_t i, const typename vector<T,vector_type>::const_iterator& it) -> typename vector<T,vector_type>::const_iterator {
	typename vector<T,vector_type>::const_iterator it2(it);
	it2 += i;
	return it2;
}



/* Constructors */
template <class T, template<class> class vt>
vector<T,vt>::vector(size_t count, const T& value, size_t max_grow_) :
		p_size(0),p_capacity(0),p_stack_size(0),max_grow(max_grow_) {
	reserve(count);
	for(;p_size < count; ++p_size) new(get_addr(p_size)) T(value);
}

template <class T, template<class> class vt> template<class It,
	typename std::enable_if< at_least_input_iterator<It>::value, int>::type >
vector<T,vt>::vector(It first, It last, size_t max_grow_) :
		 p_size(0),p_capacity(0),p_stack_size(0),max_grow(max_grow_) {
	for(; first != last; ++first) push_back(*first);
}

template <class T, template<class> class vt>
vector<T,vt>::vector(const vector<T,vt>& v) : p_size(0),p_capacity(0),p_stack_size(0),max_grow(v.max_grow) {
	reserve(v.size());
	for(;p_size < v.size(); ++p_size) new(get_addr(p_size)) T(v[p_size]);
}

template <class T, template<class> class vt>
void vector<T,vt>::swap(vector<T,vt>& v) {
	using std::swap;
	swap(stack, v.stack);
	swap(p_size, v.p_size);
	swap(p_capacity, v.p_capacity);
	swap(p_stack_size, v.p_stack_size);
	swap(max_grow, v.max_grow);
}

template <class T, template<class> class vt>
vector<T,vt>::vector(vector<T,vt>&& v) : p_size(0),p_capacity(0),p_stack_size(0),max_grow(v.max_grow) {
	swap(v);
}

template <class T, template<class> class vt>
vector<T,vt>& vector<T,vt>::operator = (const vector<T,vt>& v) {
	resize(0);
	shrink_to_fit(v.size());
	reserve(v.size());
	for(;p_size < v.size(); ++p_size) new(get_addr(p_size)) T(v[p_size]);
	return *this;
}

template <class T, template<class> class vt>
vector<T,vt>& vector<T,vt>::operator = (vector<T,vt>&& v) {
	resize(0);
	for(auto x : stack) free(x);
	stack.resize(0);
	swap(v);
}

template <class T, template<class> class vt>
bool vector<T,vt>::reserve_nothrow(size_t n) {
	if(n > p_max_capacity) return false;
	if(n <= p_capacity) return true;
	return grow_vector(n);
}

template <class T, template<class> class vt>
void vector<T,vt>::shrink_to_fit(size_t new_capacity) {
	if(new_capacity < p_size) new_capacity = p_size;
	if(new_capacity > p_capacity) return;
	if(new_capacity > max_grow) {
#ifdef USE_LIBDIVIDE
		size_t tmp = new_capacity / max_grow;
		if(new_capacity - tmp*max_grow) new_capacity += max_grow;
#else
		if(new_capacity % max_grow) new_capacity += max_grow;
#endif
		while(p_capacity > new_capacity) {
			free(stack.back());
			stack.pop_back();
			p_capacity -= max_grow;
		}
	}
	else {
		size_t new_stack_size = (new_capacity > 0)?1:0;
		for(size_t i = new_stack_size; i < stack.size(); i++) free(stack[i]);
		stack.resize(new_stack_size);
		if(new_capacity) change_size(new_capacity);
	}
	stack.shrink_to_fit();
}


/* insert /create elements at the end of the vector */
template <class T, template<class> class vt>
void vector<T,vt>::push_back(const T& x) {
	if(!push_back_nothrow(x)) throw std::bad_alloc();
}
template <class T, template<class> class vt>
void vector<T,vt>::push_back(T&& x) {
	if(!push_back_nothrow(std::forward<T>(x))) throw std::bad_alloc();
}
template<class T, template<class> class vt> template<class... Args>
T& vector<T,vt>::emplace_back(Args&&... args) {
	if(!emplace_back_nothrow(std::forward<Args>(args)...)) throw std::bad_alloc();
	return back();
}
template <class T, template<class> class vt>
bool vector<T,vt>::push_back_nothrow(const T& x) {
	if(p_size == p_capacity) if(!grow_vector()) return false;
	new(get_addr(p_size)) T(x); /* copy constructor -- might still throw an exception */
	p_size++;
	return true;
}
template <class T, template<class> class vt>
bool vector<T,vt>::push_back_nothrow(T&& x) {
	if(p_size == p_capacity) if(!grow_vector()) return false;
	new(get_addr(p_size)) T(std::forward<T>(x)); /* move constructor -- might throw an exception */
	p_size++;
	return true;
}
template<class T, template<class> class vt> template<class... Args>
bool vector<T,vt>::emplace_back_nothrow(Args&&... args) {
	if(p_size == p_capacity) if(!grow_vector()) return false;
	new(get_addr(p_size)) T(std::forward<Args>(args)...); /* constructor -- might throw an exception */
	p_size++;
	return true;
}



template <class T, template<class> class vt>
bool vector<T,vt>::resize_nothrow(size_t count) {
	if(count == p_size) return true;
	if(!count) { clear(); return true; }
	if(count < p_size) {
		do { pop_back(); } while(p_size > count);
		return true;
	}
	/* here count > p_size */
	if(count > p_capacity) if(!grow_vector(count)) return false;
	for(; p_size < count; p_size++) new(get_addr(p_size)) T();
	return true;
}
template <class T, template<class> class vt>
bool vector<T,vt>::resize_nothrow(size_t count, const T& x) {
	if(count == p_size) return true;
	if(!count) { clear(); return true; }
	if(count < p_size) {
		do { pop_back(); } while(p_size > count);
		return true;
	}
	/* here count > p_size */
	if(count > p_capacity) if(!grow_vector(count)) return false;
	for(; p_size < count; p_size++) new(get_addr(p_size)) T(x);
	return true;
}


template <class T, template<class> class vt>
typename vector<T,vt>::iterator vector<T,vt>::erase(vector<T,vt>::const_iterator pos) {
	if(pos.pos >= p_size) throw std::out_of_range("vector::erase(): iterator out of bounds!\n");
	for(size_t p2 = pos.pos; p2 + 1 < p_size; p2++) get_ref(p2) = std::move(get_ref(p2+1));
	p_size--;
	get_ref(p_size).~T();
	return make_iterator(pos.pos);
}
template <class T, template<class> class vt>
typename vector<T,vt>::iterator vector<T,vt>::erase(vector<T,vt>::const_iterator first, vector<T,vt>::const_iterator last) {
	if(first.pos >= p_size) throw std::out_of_range("vector::erase(): iterator out of bounds!\n");
	ssize_t dist = last - first;
	if(!dist) return make_iterator(first);
	if(dist < 0) throw std::out_of_range("vector::erase(): invalid iterators!\n");
	for(size_t p2 = first.pos; p2 + dist < p_size; p2++) get_ref(p2) = std::move(get_ref(p2+dist));
	
	size_t new_size = p_size - dist;
	for(;p_size > new_size; --p_size) get_ref(p_size-1).~T();
	return make_iterator(first);
}
		


/* move elements to new position from given position
 * requires that new_pos >= pos and p_size > pos */
template <class T, template<class> class vt>
bool vector<T,vt>::insert_helper(size_t pos, size_t new_pos) {
	size_t diff = new_pos - pos;
	if(p_size > p_max_capacity - diff || !reserve_nothrow(p_size + diff)) return false;
	size_t remaining = p_size - pos;
	
	for(size_t i = 0; i < diff; i++) {
		size_t j = p_size + i;
		new(get_addr(j)) T(std::move(get_ref( p_size - diff + i )));
	}
	for(size_t i = p_size - diff; i > pos; i--) get_ref(i - 1UL + diff) = std::move(get_ref(i - 1UL));
	p_size += diff;
	return true;
}



template <class T, template<class> class vt>
bool vector<T,vt>::insert_nothrow(vector<T,vt>::const_iterator pos, vector<T,vt>::iterator& res, const T& x) {
	res = make_iterator(pos);
	if(pos == cend()) return push_back_nothrow(x);
	if(!insert_helper(pos.pos,pos.pos+1)) return false;
	get_ref(pos.pos) = x;
	return true;
}

template <class T, template<class> class vt>
bool vector<T,vt>::insert_nothrow(vector<T,vt>::const_iterator pos, vector<T,vt>::iterator& res, T&& x) {
	res = make_iterator(pos);
	if(pos == cend()) return push_back_nothrow(std::forward<T>(x));
	if(!insert_helper(pos.pos,pos.pos+1)) return false;
	get_ref(pos.pos) = std::move(x);
	return true;
}


template <class T, template<class> class vt>
bool vector<T,vt>::insert_nothrow(vector<T,vt>::const_iterator pos, vector<T,vt>::iterator& res, size_t count, const T& x) {
	res = make_iterator(pos);
	if(pos == cend()) return resize_nothrow(p_size + count, x);
	if(!insert_helper(pos.pos,pos.pos+count)) return false;
	for(size_t i = 0; i < count; i++) get_ref(pos.pos + i) = x;
	return true;
}


template <class T, template<class> class vt> template<class InputIt,
	typename std::enable_if<at_least_input_iterator<InputIt>::value, int>::type >
bool vector<T,vt>::insert_nothrow(vector<T,vt>::const_iterator pos, vector<T,vt>::iterator& res, InputIt first, InputIt last) {
	res = make_iterator(pos);
	if(first != last) {
		if CONSTEXPR(at_least_forward_iterator<InputIt>::value) {
			ssize_t dist = std::distance(first,last);
			if(dist < 0) return false;
			
			if(pos == cend()) {
				if(!reserve_nothrow(p_size + dist)) return false;
				for(; first != last; ++first) push_back(*first);
				return true;
			}
			
			if(!insert_helper(pos.pos,pos.pos+dist)) return false;
			for(size_t i = pos.pos; first != last; ++first, i++) get_ref(i) = *first;
		}
		else {
			vector<T,vt>::iterator tmp;
			for(size_t i = pos.pos; first != last; ++first, i++)
				if(!insert_nothrow(vector<T,vt>::iterator(*this,i),tmp,*first)) return false;
		}
	}
	return true;
}



} // namespace stacked_vector

#undef CONSTEXPR

#endif


