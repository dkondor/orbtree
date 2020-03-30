/*  -*- C++ -*-
 * vector_realloc.h
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
 * \class realloc_vector::vector
 * 
 * \brief C++ vector-like container for trivially moveable types, using realloc()
 * for growing memory, with limits on maximum absolute growth.
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
 * This class addresses these issues by requiring the stored type to be
 * trivially copyable, thus realloc() can be used for changing the size. Using
 * the assumption that realloc() is optimized for large memory areas to avoid
 * a copy, this will result in the memory requirement being only the requested
 * new size. This also allows the vector to be grown in smaller chunks that
 * can be useful to avoid getting an out of memory error.
 * 
 * Currently, the requirement is for the type to be trivially copyable
 * (as per std::is_trivially_copyable), but it could be relaxed to any type
 * that can be moved by a simple memmove() (and not calling the destructor
 * explicitly), i.e. any type that allocates dynamic memory as well, as long
 * as it does not store a pointer to itself (and no pointers to it are stored
 * elsewhere as well).
 */

#ifndef VECTOR_REALLOC_H
#define VECTOR_REALLOC_H

#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <stdlib.h>
#include <string.h>


/* constexpr if support only for c++17 or newer */
#if __cplusplus >= 201703L
#define CONSTEXPR constexpr
#else
#define CONSTEXPR
#endif


namespace realloc_vector {

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
class vector {
	protected:
		/* This requirement is too strict; what we actually want is trivially moveable,
		 * i.e. a type that can be moved with memmove(), but the copy constructor could
		 * be nontrivial.
		 * For such types, this assertation will need to be deleted, but this might also
		 * result in undefined behavior depending on the compiler */
		static_assert(std::is_trivially_copyable<T>::value,"Element type must be trivially copyable for realloc_vector::vector!\n");
	
		T* start; /**< \brief pointer to elements */
		size_t p_size; /**< \brief number of elements in vector */
		size_t p_capacity; /**< \brief current capacity of vector */
		size_t max_grow; /**< \brief grow memory by maximum this many elements at a time */
		/// \brief maximum safe capacity to avoid overflow
		static constexpr size_t p_max_capacity = std::numeric_limits<size_t>::max() / sizeof(T);
		
		/// \brief Reallocate memory to the given new size
		bool change_size(size_t new_size) {
			T* tmp = (T*)realloc(start,new_size*sizeof(T));
			if(!tmp) return false;
			start = tmp;
			p_capacity = new_size;
			return true;
		}
		/// \brief Attempt to grow vector either to the given minimum size or
		/// by doubling the current size unless growth would be
		/// larger than max_grow, in which case size is increased by max_grow.
		bool grow_vector(size_t minimum_size = 0) {
			if(minimum_size > p_max_capacity) return false;
			size_t grow_capacity = p_capacity;
			if(!grow_capacity) grow_capacity = 1;
			if(grow_capacity > max_grow) grow_capacity = max_grow;
			if(grow_capacity > p_max_capacity) grow_capacity = p_max_capacity;
			if(p_max_capacity - grow_capacity < p_capacity) grow_capacity = p_max_capacity - p_capacity;
			if(!grow_capacity) return false;
			size_t new_size = p_capacity + grow_capacity;
			if(new_size < minimum_size) new_size = minimum_size;
			return change_size(new_size);
		}
		
		
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
		vector() noexcept : start(nullptr),p_size(0),p_capacity(0),max_grow(131072) { }
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
			if(start) free(start);
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
		/// \brief Get the maximum growth size. Memory is grown by this amount maximally.
		size_t get_max_grow() const { return max_grow; }
		/// \brief Set the maximum growth size. Memory is grown by this amount maximally.
		void set_max_grow(size_t max_grow_) {
			if(max_grow_ == 0) max_grow = 131072;
			else max_grow = max_grow_;
		}
		
		
		/* data access functions
		 * similarly to std::vector, only at() checks bounds, all other versions
		 * result in undefined behavior if an out of bounds is attempted */
		/// \brief Access the underlying array
		T* data() { return start; }
		/// \brief Access the underlying array
		const T* data() const { return start; }
		
		/// \brief Access the ith element. It is undefined behavior to if i >= size()
		T& operator[](size_t i) { return start[i]; }
		/// \brief Access the ith element. It is undefined behavior to if i >= size()
		const T& operator[](size_t i) const { return start[i]; }
		/// \brief Access the ith element with bounds checking, throws an exception if i >= size()
		T& at(size_t i) { if(i < p_size) return start[i]; throw std::out_of_range("vector::at(): index out of range!\n"); }
		/// \brief Access the ith element with bounds checking, throws an exception if i >= size()
		const T& at(size_t i) const { if(i < p_size) return start[i]; throw std::out_of_range("vector::at(): index out of range!\n"); }
		/// \brief Access the first element. It is undefined behavior if this function is called on an empty vector.
		T& front() { return start[0]; }
		/// \brief Access the first element. It is undefined behavior if this function is called on an empty vector.
		const T& front() const { return start[0]; }
		/// \brief Access the last element. It is undefined behavior if this function is called on an empty vector.
		T& back() { return start[p_size - 1]; }
		/// \brief Access the last element. It is undefined behavior if this function is called on an empty vector.
		const T& back() const { return start[p_size - 1]; }
		
		/* reserve memory */
		/// \brief Reserve memory for at least n elements. Returns true if allocation was successfull, false otherwise.
		bool reserve_nothrow(size_t n);
		/// \brief Reserve memory for at least n elements. Throws an exception if memory allocation is not successful.
		void reserve(size_t n) { if(!reserve_nothrow(n)) throw std::bad_alloc(); }
		/// \brief Free up unused memory.
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
		void clear() { while(p_size) start[--p_size].~T(); }
		/// \brief Removes the last element; does not free up memory, use \ref shrink_to_fit() for that.
		void pop_back() { if(p_size) start[--p_size].~T(); }
		
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
		
		/// \brief Iterators are simple pointers to the data
		typedef T* iterator;
		/// \brief Iterators are simple pointers to the data
		typedef const T* const_iterator;
		
	protected:
		bool insert_helper(size_t pos, size_t new_pos);
	
	public:
		iterator begin() { return iterator(start); } ///< \brief Iterator to the beginning
		const_iterator begin() const { return const_iterator(start); } ///< \brief Iterator to the beginning
		const_iterator cbegin() const { return const_iterator(start); } ///< \brief Iterator to the beginning
		iterator end() { return iterator(start + p_size); } ///< \brief Iterator to the end
		const_iterator end() const { return const_iterator(start + p_size); } ///< \brief Iterator to the end
		const_iterator cend() const { return const_iterator(start + p_size); } ///< \brief Iterator to the end
		
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




/* Constructors */
template<class T>
vector<T>::vector(size_t count, const T& value, size_t max_grow_) :
		start(nullptr),p_size(0),p_capacity(0),max_grow(max_grow_) {
	reserve(count);
	if CONSTEXPR(std::is_nothrow_constructible<T>::value) {
		p_size = count;
		for(size_t i = 0; i<count; i++) new(start + i) T(value);
	}
	else for(;p_size < count; ++p_size) new(start + p_size) T(value);
}

template<class T> template<class It, typename std::enable_if< at_least_input_iterator<It>::value, int>::type >
vector<T>::vector(It first, It last, size_t max_grow_) :
		start(nullptr),p_size(0),p_capacity(0),max_grow(max_grow_) {
	for(; first != last; ++first) push_back(*first);
}

template<class T>
vector<T>::vector(const vector<T>& v) : start(nullptr), p_size(0), p_capacity(0), max_grow(v.max_grow) {
	reserve(v.size());
	memcpy(start, v.start, sizeof(T)*(v.size()));
	p_size = v.size();
}

template<class T>
void vector<T>::swap(vector<T>& v) {
	using std::swap;
	swap(start, v.start);
	swap(p_size, v.p_size);
	swap(p_capacity, v.p_capacity);
	swap(max_grow, v.max_grow);
}

template<class T>
vector<T>::vector(vector<T>&& v) : start(nullptr), p_size(0), p_capacity(0), max_grow(v.max_grow) {
	swap(v);
}

template<class T>
vector<T>& vector<T>::operator = (const vector<T>& v) {
	resize(0);
	reserve(v.size());
	memcpy(start, v.start, sizeof(T)*(v.size()));
	p_size = v.size();
	return *this;
}

template<class T>
vector<T>& vector<T>::operator = (vector<T>&& v) {
	resize(0);
	if(start) free(start);
	start = nullptr;
	swap(v);
}

template<class T>
bool vector<T>::reserve_nothrow(size_t n) {
	if(n > p_max_capacity) return false;
	if(n <= p_capacity) return true;
	return change_size(n);
}

template<class T>
void vector<T>::shrink_to_fit(size_t new_capacity) {
	if(new_capacity < p_size) new_capacity = p_size;
	if(new_capacity > p_capacity) return;
	if(!change_size(new_capacity)) throw std::bad_alloc(); /* this should not happen, shrinking memory should always succeed */
}



/* insert /create elements at the end of the vector */
template<class T>
void vector<T>::push_back(const T& x) {
	if(!push_back_nothrow(x)) throw std::bad_alloc();
}
template<class T>
void vector<T>::push_back(T&& x) {
	if(!push_back_nothrow(std::forward<T>(x))) throw std::bad_alloc();
}
template<class T> template<class... Args>
T& vector<T>::emplace_back(Args&&... args) {
	if(!emplace_back_nothrow(std::forward<Args>(args)...)) throw std::bad_alloc();
	return back();
}
template<class T>
bool vector<T>::push_back_nothrow(const T& x) {
	if(p_size == p_capacity) if(!grow_vector()) return false;
	new(start + p_size) T(x); /* copy constructor -- might still throw an exception */
	p_size++;
	return true;
}
template<class T>
bool vector<T>::push_back_nothrow(T&& x) {
	if(p_size == p_capacity) if(!grow_vector()) return false;
	new(start + p_size) T(std::forward<T>(x)); /* move constructor -- might throw an exception */
	p_size++;
	return true;
}
template<class T> template<class... Args>
bool vector<T>::emplace_back_nothrow(Args&&... args) {
	if(p_size == p_capacity) if(!grow_vector()) return false;
	new(start + p_size) T(std::forward<Args>(args)...); /* constructor -- might throw an exception */
	p_size++;
	return true;
}



template<class T>
bool vector<T>::resize_nothrow(size_t count) {
	if(count == p_size) return true;
	if(!count) { clear(); return true; }
	if(count < p_size) {
		do { pop_back(); } while(p_size > count);
		return true;
	}
	/* here count > p_size */
	if(count > p_capacity) if(!grow_vector(count)) return false;
	if CONSTEXPR(std::is_nothrow_constructible<T>::value) {
		for(size_t i = p_size; i<count; i++) new(start + i) T();
		p_size = count;
	}
	else for(; p_size < count; p_size++) new(start + p_size) T();
	return true;
}
template<class T>
bool vector<T>::resize_nothrow(size_t count, const T& x) {
	if(count == p_size) return true;
	if(!count) { clear(); return true; }
	if(count < p_size) {
		do { pop_back(); } while(p_size > count);
		return true;
	}
	/* here count > p_size */
	if(count > p_capacity) if(!grow_vector(count)) return false;
	if CONSTEXPR(std::is_nothrow_copy_constructible<T>::value) {
		for(size_t i = p_size; i<count; i++) new(start + i) T(x);
		p_size = count;
	}
	else for(; p_size < count; p_size++) new(start + p_size) T(x);
	return true;
}


template<class T>
typename vector<T>::iterator vector<T>::erase(vector<T>::const_iterator pos) {
	size_t p2 = pos - start;
	if(p2 >= p_size) throw std::out_of_range("vector::erase(): iterator out of bounds!\n");
	
	size_t remaining = (p_size - p2) - 1;
	start[p2].~T();
	if(remaining) memmove(start + p2, start + p2 + 1, remaining*sizeof(T));
	p_size--;
	return iterator(start + p2);
}
template<class T>
typename vector<T>::iterator vector<T>::erase(vector<T>::const_iterator first, vector<T>::const_iterator last) {
	ssize_t dist = last - first;
	if(!dist) return iterator(first);
	
	size_t p2 = first.data - start;
	size_t p3 = last.data - start;
	if(p2 >= p_size || p3 > p_size) throw std::out_of_range("vector::erase(): iterator out of bounds!\n");
	for(size_t i = p2; i < p3; i++) start[i].~T();
	
	size_t remaining = (p_size - p3) - 1;
	if(remaining) memmove(start + p2, start + p3, remaining*sizeof(T));
	p_size -= (p3 - p2);
	return iterator(start + p2);
}
		


/* move elements to new position from given position */
template<class T>
bool vector<T>::insert_helper(size_t pos, size_t new_pos) {
	if(new_pos > pos) {
		size_t diff = new_pos - pos;
		if(p_size > p_max_capacity - diff || !reserve_nothrow(p_size + diff)) return false;
	}
	size_t remaining = p_size - pos;
	memmove(start + new_pos, start + pos, remaining*sizeof(T));
	return true;
}



template<class T>
bool vector<T>::insert_nothrow(vector<T>::const_iterator pos, vector<T>::iterator& res, const T& x) {
	if(pos == cend()) return push_back_nothrow(x);
	size_t p2 = pos - start;
	if(!insert_helper(p2,p2+1)) return false;
	if CONSTEXPR(std::is_nothrow_copy_constructible<T>::value) new(start + p2) T(x);
	else {
		/* provide fallback if copy throws -- note: this is not possible for trivially
		 * copyable types, but this vector class will work if type is trivially moveable
		 * only that is a weaker criterion, but is not provided by the type_traits library */
		try { new(start + p2) T(x); }
		catch(...) {
			insert_helper(p2+1,p2);
			throw;
		}
	}
	p_size++;
	res = vector<T>::iterator(start + p2);
	return true;
}

template<class T>
bool vector<T>::insert_nothrow(vector<T>::const_iterator pos, vector<T>::iterator& res, T&& x) {
	if(pos == cend()) return push_back_nothrow(std::forward<T>(x));
	size_t p2 = pos - start;
	if(!insert_helper(p2,p2+1)) return false;
	/* note: move constructor of T should never throw an exception */
	new(start + p2) T(std::forward<T>(x));
	p_size++;
	res = vector<T>::iterator(start + p2);
	return true;
}


template<class T>
bool vector<T>::insert_nothrow(vector<T>::const_iterator pos, vector<T>::iterator& res, size_t count, const T& x) {
	size_t p2 = pos - start;
	if(pos == cend()) {
		if(!resize_nothrow(p_size + count, x)) return false;
		res = vector<T>::iterator(start + p2);
		return true;
	}
	
	if(!insert_helper(p2,p2+count)) return false;
	if CONSTEXPR(std::is_nothrow_copy_constructible<T>::value) for(size_t i = 0; i < count; i++) new(start + p2 + i) T(x);
	else {
		/* provide fallback if copy throws -- note: this is not possible for trivially
		 * copyable types, but this vector class will work if type is trivially moveable
		 * only that is a weaker criterion, but is not provided by the type_traits library */
		size_t i = 0;
		try {
			for(; i < count; i++) new(start + p2 + i) T(x);
		}
		catch(...) {
			/* ensure that in the case of an exception, this class
			 * is left in a well-defined state */
			while(i) {
				i--;
				start[p2 + i].~T();
			}
			insert_helper(p2+count,p2);
			throw;
		}
	}
	p_size += count;
	res = vector<T>::iterator(start + p2);
	return true;
}


template<class T> template<class InputIt,
	typename std::enable_if<at_least_input_iterator<InputIt>::value, int>::type >
bool vector<T>::insert_nothrow(vector<T>::const_iterator pos, vector<T>::iterator& res, InputIt first, InputIt last) {
	size_t p2 = pos - start;
	if(first != last) {
		if CONSTEXPR(at_least_forward_iterator<InputIt>::value) {
			ssize_t dist = std::distance(first,last);
			if(dist < 0) return false;
			
			if(pos == cend()) {
				if(!reserve_nothrow(p_size + dist)) return false;
				for(; first != last; ++first) push_back(*first);
				res = vector<T>::iterator(start + p2);
				return true;
			}
			
			if(!insert_helper(p2,p2+dist)) return false;
			if CONSTEXPR(std::is_nothrow_copy_constructible<T>::value) {
				size_t p3 = p2;
				for(; first != last; ++first, p3++) new(start + p3) T(*first);
			}
			else {
				/* provide fallback if copy throws -- note: this is not possible for trivially
				 * copyable types, but this vector class will work if type is trivially moveable
				 * only that is a weaker criterion, but is not provided by the type_traits library */
				size_t p3 = p2;
				try {
					for(; first != last; ++first, p3++) new(start + p3) T(*first);
				}
				catch(...) {
					while(p3 > p2) {
						p3--;
						start[p3].~T();
					}
					insert_helper(p2+dist,p2);
					throw;
				}
			}
			p_size += dist;
		}
		else {
			for(size_t p3 = p2;first != last; ++first, p3++)
				if(!insert_nothrow(vector<T>::iterator(start + p3),res,*first)) return false;
		}
	}
	res = vector<T>::iterator(start + p2);
	return true;
}



} // namespace realloc_vector


#endif

