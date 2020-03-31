/*  -*- C++ -*-
 * orbtree.h -- generalized order statistic red-black tree implementation
 * 	main interface
 * 
 * Copyright 2019 Daniel Kondor <kondor.dani@gmail.com>
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


#ifndef ORBTREE_H
#define ORBTREE_H

#include "orbtree_base.h"
#include <type_traits>
#include <limits>
#include <stdexcept>
#include <vector>
#include <math.h>

/* constexpr if support only for c++17 or newer */
#if __cplusplus >= 201703L
#define CONSTEXPR constexpr
#else
#define CONSTEXPR
#endif


namespace orbtree {
	
	/* main class with public interface */
	/** \brief Generalized order statistic tree main interface.
	 * It is recommended to use the templates \ref orbset, orbsetC,
	 * 	\ref orbmultiset, orbmultisetC, \ref orbmap, orbmapC, \ref
	 * orbmultimap and orbmultimapC to instantiate various versions
	 * instead of directly using this class template */
	/** Generalized order statistic tree main interface. This class
	 * provides common functionality for sets, maps, multisets and
	 * multimaps, depending on the templates provided. Most of the
	 * interface is defined here. Interface should be as similar as
	 * possible to std::set, std::multiset, std::map and std::multimap.
	 * 
	 * @tparam NodeAllocator internal base class to store nodes
	 * @tparam Comapre comparison functor
	 * @tparam NVFunc function that calculates values associated with
	 * 		elements based on key and value
	 * @tparam multi determines if this is a multiset / multimap (keys
	 * 		can be present multiple times) or not
	 * @tparam simple determines if the weight function returns one value
	 * 		(if true) or multiple values (if false). This changes the
	 * 		return value of \ref get_sum()
	 */
	template<class NodeAllocator, class Compare, class NVFunc, bool multi, bool simple = false>
	class orbtree : public orbtree_base<NodeAllocator, Compare, NVFunc, multi> {
		protected:
			typedef typename orbtree_base<NodeAllocator, Compare, NVFunc, multi>::NodeHandle NodeHandle;
			typedef typename orbtree_base<NodeAllocator, Compare, NVFunc, multi>::KeyValue KeyValue;
		
		public:
/* typedefs */
			/// \brief Values stored in this tree. Either the key (for sets)
			///	or an std::pair or orbtree::trivial_pair of key and value
			typedef typename NodeAllocator::KeyValue::ValueType value_type;
			/// Key type of nodes that determines ordering.
			typedef typename NodeAllocator::KeyValue::KeyType key_type;
			/// Type of values associated by nodes (calculated by NVFunc)
			typedef typename NodeAllocator::NVType NVType;
			/* conditionally define mapped_type ?? */
			typedef size_t size_type;
			typedef size_t difference_type; /* note: unsigned difference_type not standard?
						-- since iterators are InputIterators, only unsigned differences are possible */
			
			
			explicit orbtree(const NVFunc& f_ = NVFunc(), const Compare& c_ = Compare())
					: orbtree_base<NodeAllocator, Compare, NVFunc, multi>(f_,c_) {
				if CONSTEXPR (simple) if(this->f.get_nr() != 1) {
					throw std::runtime_error("For simple tree, weight function can only return one component!\n");
				}
			}
			explicit orbtree(NVFunc&& f_,const Compare& c_ = Compare())
					: orbtree_base<NodeAllocator, Compare, NVFunc, multi>(f_,c_) { 
				if CONSTEXPR (simple) if(this->f.get_nr() != 1) {
					throw std::runtime_error("For simple tree, weight function can only return one component!\n");
				}
			}
			template<class T>
			explicit orbtree(const T& t, const Compare& c_ = Compare())
					: orbtree_base<NodeAllocator, Compare, NVFunc, multi>(t,c_) {
				if CONSTEXPR (simple) if(this->f.get_nr() != 1) {
					throw std::runtime_error("For simple tree, weight function can only return one component!\n");
				}
			}
					
			
			
			
			/* iterators -- note: set should only have const iterators */
			/// Iterators
			template<bool is_const>
			struct iterator_base {
				protected:
					typedef typename NodeAllocator::KeyValue::ValueType value_base;
					typedef typename std::conditional<is_const, const orbtree, orbtree>::type orbtree_t;
				public:
					/* typedefs */
					typedef std::bidirectional_iterator_tag iterator_category;
					/* note: value type is always const, value cannot be changed
					 * by dereferincing the iterator since rank function might depend on it
					 * need to call set_value() explicitely */
					/// Type of value pointed to by iterator
					/** 
					 * Either the key (for sets) or an std::pair or
					 * orbtree::trivial_pair of key and value for maps.
					 * 
					 * Note: value cannot be changed directly even for
					 * non-const map iterators. Use the set_value()
					 * member function for that
					 */
					typedef const value_base value_type;
					typedef const value_base* pointer;
					typedef const value_base& reference;
					
					friend class iterator_base<!is_const>;
					friend class orbtree<NodeAllocator,Compare,NVFunc,multi,simple>;
				protected:
					orbtree_t& t; /* has to be const reference for const iterator */
					NodeHandle n;
					iterator_base() = delete;
					/* create iterator (by orbtree class) */
					iterator_base(orbtree_t& t_, NodeHandle n_):t(t_),n(n_) { }
					/* convert const iterator to non-const -- protected, might be needed internally ? */
					template<bool is_const_ = is_const>
					explicit iterator_base(const iterator_base<true>& it, typename std::enable_if<!is_const_>::type* = 0)
						:t(const_cast<orbtree>(it.t)),n(it.n) { }
				public:
					
					/* any iterator can be copied from non-const iterator;
					 * this is not explicit so comparison operators can auto-convert */
					iterator_base(const iterator_base<false>& it):t(it.t),n(it.n) { }
					/* only const iterator can be copied from const iterator */
					template<bool is_const_ = is_const>
					iterator_base(const iterator_base<true>& it, typename std::enable_if<is_const_>::type* = 0 ):t(it.t),n(it.n) { }
					
					/// read-only access to values
					/** 
					 * Note: dereferencing the past-the-end iterator throws an exception.
					 * 
					 * Note: to allow the stored values in a map depend on
					 * the mapped value as well, they cannot be changed 
					 * here, use the \ref set_value() function instead
					 * for that purpose. */
					reference operator * () {
						if(n == NodeAllocator::Invalid) throw std::runtime_error("Attempt to dereference invalid orbtree::iterator!\n");
						return t.get_node(n).get_key_value().keyvalue();
					}
					/// \copydoc operator*()
					pointer operator -> () {
						if(n == NodeAllocator::Invalid) throw std::runtime_error("Attempt to dereference invalid orbtree::iterator!\n");
						return &(t.get_node(n).get_key_value().keyvalue());
					}
					/// change value stored in map or multimap
					template<bool is_const_ = is_const, class KeyValue_ = typename NodeAllocator::KeyValue>
					typename std::enable_if<!is_const_>::type set_value(typename KeyValue_::MappedType&& v) {
						t.update_value(n,std::move(v));
					}
					/// change value stored in map or multimap
					template<bool is_const_ = is_const, class KeyValue_ = typename NodeAllocator::KeyValue>
					typename std::enable_if<!is_const_>::type set_value(typename KeyValue_::MappedType const& v) {
						t.update_value(n,v);
					}
					
					/// convenience function to return the key (for both set and map)
					const key_type& key() const { return t.get_node(n).get_key_value().key(); }
					
					/// compare iterators
					/** Note: comparing iterators from different map / tree
					 * is undefined behavior, it is not explicitely tested */
					template<bool is_const2> bool operator == (const iterator_base<is_const2>& i) const { return n == i.n; }
					/// compare iterators
					/** Note: comparing iterators from different map / tree
					 * is undefined behavior, it is not explicitely tested */
					template<bool is_const2> bool operator != (const iterator_base<is_const2>& i) const { return n != i.n; }
					
					/// increment: move to the next stored node
					iterator_base& operator ++() { n = t.next(n); return *this; }
					/// increment: move to the next stored node
					iterator_base operator ++(int) { iterator_base<is_const> i(*this); n = t.next(n); return i; }
					/// decrement: move to the previous stored node
					iterator_base& operator --() { n = t.previous(n); return *this; }
					/// decrement: move to the previous stored node
					iterator_base operator --(int) { iterator_base<is_const> i(*this); n = t.previous(n); return i; }
			};
			
			/// iterator that does not allow modification
			typedef iterator_base<true> const_iterator;
			/// iteraror that allows the modification of the stored value (for maps)
			typedef typename std::conditional<NodeAllocator::KeyValue::keyonly, iterator_base<true>, iterator_base<false> >::type iterator;
			
			iterator begin() { return iterator(*this,this->first()); } /// get an iterator to the beginning (node with the lowest key value)
			const_iterator begin() const { return const_iterator(*this,this->first()); } /// get an iterator to the beginning (node with the lowest key value)
			const_iterator cbegin() const { return const_iterator(*this,this->first()); } /// get an iterator to the beginning (node with the lowest key value)
			
			iterator end() { return iterator(*this,this->nil()); } /// get the past-the-end iterator
			const_iterator end() const { return const_iterator(*this,this->nil()); } /// get the past-the-end iterator
			const_iterator cend() const { return const_iterator(*this,this->nil()); } /// get the past-the-end iterator
			
			bool empty() const { return this->size1 == 0; } /// check if the tree is empty
			size_t size() const { return this->size1; } /// return the number of elemets
			/// return the maximum possible number of elements
			constexpr size_t max_size() const {
				return NodeAllocator::max_nodes ? NodeAllocator::max_nodes : std::numeric_limits<size_t>::max();
			}
			
/* 2. add / remove */
			//~ void clear(); -- defined in orbtree_base already, no need to add functionality
			
			/* for insert, we need to distinguish between map and multimap
			 * create a return typedef which is either an iterator or a pair */
			/// return type of insert functions
			typedef typename std::conditional<multi, iterator, std::pair<iterator, bool> >::type insert_type;
			
		protected:
			template<bool multi_ = multi>
			typename std::enable_if<multi_,insert_type>::type insert_helper(const value_type& v) {
				return iterator(*this, orbtree_base<NodeAllocator, Compare, NVFunc, multi>::insert(v).first);
			}
			template<bool multi_ = multi>
			typename std::enable_if<!multi_,insert_type>::type insert_helper(const value_type& v) {
				auto x = orbtree_base<NodeAllocator, Compare, NVFunc, multi>::insert(v);
				return std::pair<iterator,bool>(iterator(*this,x.first),x.second);
			}
			template<bool multi_ = multi>
			typename std::enable_if<multi_,insert_type>::type insert_helper(value_type&& v) {
				return iterator(*this, orbtree_base<NodeAllocator, Compare, NVFunc, multi>::insert(std::move(v)).first);
			}
			template<bool multi_ = multi>
			typename std::enable_if<!multi_,insert_type>::type insert_helper(value_type&& v) {
				auto x = orbtree_base<NodeAllocator, Compare, NVFunc, multi>::insert(std::move(v));
				return std::pair<iterator,bool>(iterator(*this,x.first),x.second);
			}
			
			
			template<class... Args, bool multi_ = multi>
			typename std::enable_if<multi_,insert_type>::type emplace_helper(Args&... args) {
				return iterator(*this, orbtree_base<NodeAllocator, Compare, NVFunc, multi>::emplace(std::forward<Args...>(args...)).first);
			}
			template<class... Args, bool multi_ = multi>
			typename std::enable_if<!multi_,insert_type>::type emplace_helper(Args&... args) {
				auto x = orbtree_base<NodeAllocator, Compare, NVFunc, multi>::emplace(std::forward<Args...>(args...));
				return std::pair<iterator,bool>(iterator(*this,x.first),x.second);
			}
			
			
		public:
			
			/** \brief Insert new element
			 * 
			 * For non-multi map/set, the return type is std::pair<iterator,bool>,
			 * where the second element indicates if insert was successful.
			 * If an element with the same key already existed, the insert fails and
			 * false is returned.
			 * 
			 * For multi map/set, inserting always succeeds and the return type is an
			 * iterator to the new element. In this case, a new element is always inserted
			 * after any existing elements with the same key */
			insert_type insert(const value_type& v) { return insert_helper(v); }
			/// \copydoc insert(const value_type& v)
			insert_type insert(value_type&& v) { return insert_helper(std::move(v)); }
			
			
			
			/** \brief Insert new element with hint
			 * 
			 * Caller suggests a position which is before
			 * the element pointed to by the supplied iterator.
			 * 
			 * For non-multi tree (map/set; i.e. duplicates are not allowed):
			 * 	- if the hint points to the correct position (i.e. the new element should go before the element
			 * 		referenced by the hint iterator), then a search is not performed, so the insertion
			 * 		cost is amortized constant
			 * 	- in all other cases, the hint is ignored
			 *
			 * For multi tree (duplicate keys are allowed):
			 * 	- if the key is equal to the key of element referenced by the hint iterator, then the
			 * 		new element is inserted before it
			 * 	- otherwise, the new element is inserted as close as possible  */
			iterator insert(const_iterator hint, const value_type& v) {
				if(hint.n == this->nil()) return iterator(*this, orbtree_base<NodeAllocator, Compare, NVFunc, multi>::insert(v).first);
				else return iterator(*this, orbtree_base<NodeAllocator, Compare, NVFunc, multi>::insert(hint.n,v));
			}
			/// \copydoc iterator insert(const_iterator hint, const value_type& v)
			iterator insert(const_iterator hint, value_type&& v) {
				if(hint.n == this->nil()) return iterator(*this, orbtree_base<NodeAllocator, Compare, NVFunc, multi>::insert(std::move(v)).first);
				else return iterator(*this, orbtree_base<NodeAllocator, Compare, NVFunc, multi>::insert(hint.n,std::move(v)));
			}
			
			
			/// insert all elements in the range [first,last)
			template<class InputIt> void insert(InputIt first, InputIt last) {
				for(;first!=last;++first) orbtree::insert(*first);
			}
			
			/// construct new element in-place
			template<class... Args> insert_type emplace(Args&&... args) { return emplace_helper(std::forward<Args...>(args...)); }
			/// construct new element in-place, using the given hint in a same way as insert() with a hint
			template<class... Args> iterator emplace_hint(const_iterator hint, Args&&... args) {
				if(hint.n == this->nil()) return iterator(*this, orbtree_base<NodeAllocator, Compare, NVFunc, multi>::emplace(std::forward<Args...>(args...)).first);
				else return iterator(*this, orbtree_base<NodeAllocator, Compare, NVFunc, multi>::emplace_hint(hint, std::forward<Args...>(args...)));
			}
			
			 
			//~ iterator erase(iterator pos) { return iterator(*this,orbtree_base<NodeAllocator, Compare, NVFunc, multi>::erase(pos.n)); }
			/// erase element pointed to by the given iterator; returns the element after it (in order)
			iterator erase(const_iterator pos) { return iterator(*this,orbtree_base<NodeAllocator, Compare, NVFunc, multi>::erase(pos.n)); }
			/// erase elements in the range [first,last); returns last
			iterator erase(const_iterator first, const_iterator last) {
				if(first == last) return iterator(*this,first.n);
				iterator x = erase(first);
				while(x != last) x = erase(x);
				return x;
			}
			
			/// erase all elements with the given key; returns the number of elements erased
			size_t erase(const key_type& k) {
				size_t r = 0;
				for(NodeHandle n = orbtree_base<NodeAllocator, Compare, NVFunc, multi>::lower_bound(k);
					n != this->nil() && n != this->Invalid && this->compare_key_equals(n,k);
					n = orbtree_base<NodeAllocator, Compare, NVFunc, multi>::erase(n)) r++;
				return r;
			}
			
			/// count number of elements with key
			size_t count(const key_type& k) const {
				size_t r = 0;
				for(NodeHandle n = orbtree_base<NodeAllocator, Compare, NVFunc, multi>::lower_bound(k);
					n != this->nil() && n != this->Invalid && this->compare_key_equals(n,k); n = this->next(n)) r++;
				return r;
			}
			/// count the number of elements whose key compares equal to k
			template<class K>
			size_t count(const K& k) const {
				size_t r = 0;
				for(NodeHandle n = orbtree_base<NodeAllocator, Compare, NVFunc, multi>::lower_bound(k);
					n != this->nil() && n != this->Invalid && this->compare_key_equals(n,k); n = this->next(n)) r++;
				return r;
			}
			
/* 3. search based on key */
			
			
			/** \brief Find an element with the given key and return an iterator to it.
			 * 
			 * Returns the past-the-end iterator if no such element is found.
			 * 
			 * In case of multimap / multiset, any element with such key can
			 * be returned. Use lower_bound() and upper_bound() if the beginning or
			 * end of a range is required.
			 */
			iterator find(const key_type& k) {
				return iterator(*this,orbtree_base<NodeAllocator, Compare, NVFunc, multi>::find(k));
			}
			/// \copydoc iterator find(const key_type& k)
			/// Returns const iterator for const tree.
			const_iterator find(const key_type& k) const {
				return const_iterator(*this,orbtree_base<NodeAllocator, Compare, NVFunc, multi>::find(k));
			}
			/// \copydoc iterator find(const key_type& k)
			/// Works for any type K that is comparable to the keys.
			template<class K> iterator find(const K& k) {
				return iterator(*this,orbtree_base<NodeAllocator, Compare, NVFunc, multi>::find(k));
			}
			/// \copydoc iterator find(const key_type& k)
			/// Works for any type K that is comparable to the keys.
			///
			/// Returns const iterator for const tree.
			template<class K> const_iterator find(const K& k) const {
				return const_iterator(*this,orbtree_base<NodeAllocator, Compare, NVFunc, multi>::find(k));
			}
			
			
			/// return an iterator to the first element with key not less than k
			iterator lower_bound(const key_type& k) {
				return iterator(*this,orbtree_base<NodeAllocator, Compare, NVFunc, multi>::lower_bound(k));
			}
			/// return an iterator to the first element with key not less than k
			const_iterator lower_bound(const key_type& k) const {
				return const_iterator(*this,orbtree_base<NodeAllocator, Compare, NVFunc, multi>::lower_bound(k));
			}
			/// return an iterator to the first element with key not less than k
			template<class K> iterator lower_bound(const K& k) {
				return iterator(*this,orbtree_base<NodeAllocator, Compare, NVFunc, multi>::lower_bound(k));
			}
			/// return an iterator to the first element with key not less than k
			template<class K> const_iterator lower_bound(const K& k) const {
				return const_iterator(*this,orbtree_base<NodeAllocator, Compare, NVFunc, multi>::lower_bound(k));
			}
			/// return an iterator to the first element with key greater than k
			iterator upper_bound(const key_type& k) {
				return iterator(*this,orbtree_base<NodeAllocator, Compare, NVFunc, multi>::upper_bound(k));
			}
			/// return an iterator to the first element with key greater than k
			const_iterator upper_bound(const key_type& k) const {
				return const_iterator(*this,orbtree_base<NodeAllocator, Compare, NVFunc, multi>::upper_bound(k));
			}
			/// return an iterator to the first element with key greater than k
			template<class K> iterator upper_bound(const K& k) {
				return iterator(*this,orbtree_base<NodeAllocator, Compare, NVFunc, multi>::upper_bound(k));
			}
			/// return an iterator to the first element with key greater than k
			template<class K> const_iterator upper_bound(const K& k) const {
				return const_iterator(*this,orbtree_base<NodeAllocator, Compare, NVFunc, multi>::upper_bound(k));
			}
			
			/// return a pair of iterators corresponding to the range of all elements with key equal to k
			std::pair<iterator,iterator> equal_range(const key_type& k) {
				return std::pair<iterator,iterator>(lower_bound(k),upper_bound(k));
			}
			/// return a pair of iterators corresponding to the range of all elements with key equal to k
			std::pair<const_iterator,const_iterator> equal_range(const key_type& k) const {
				return std::pair<const_iterator,const_iterator>(lower_bound(k),upper_bound(k));
			}
			/// return a pair of iterators corresponding to the range of all elements with keys that compares equal to k
			template<class K> std::pair<iterator,iterator> equal_range(const K& k) {
				return std::pair<iterator,iterator>(lower_bound(k),upper_bound(k));
			}
			/// return a pair of iterators corresponding to the range of all elements with keys that compares equal to k
			template<class K> std::pair<const_iterator,const_iterator> equal_range(const K& k) const {
				return std::pair<const_iterator,const_iterator>(lower_bound(k),upper_bound(k));
			}
			
			/// returns if an element with key equivalent to k exists in this tree
			bool contains(const key_type& k) const {
				return orbtree_base<NodeAllocator, Compare, NVFunc, multi>::find(k) != this->nil();
			}
			/// returns if an element with key equivalent to k exists in this tree
			template<class K> bool contains(const K& k) const {
				return orbtree_base<NodeAllocator, Compare, NVFunc, multi>::find(k) != this->nil();
			}
			
			
		public:
			/** \brief Calculate partial sum of the weights of nodes
			 * that come before the one pointed to by it.
			 * 
			 * Result is returned in res, which must point to an array
			 * large enough (e.g. with at least as many elements as
			 * the components calculated by NVFunc)
			 */
			template<bool simple_ = simple>
			void get_sum_node(const_iterator it, typename std::enable_if<!simple_,NVType*>::type res) const {
				if(it == cend()) this->get_norm_fv(res);
				this->get_sum_fv_node(it.n,res);
			}
			/** \brief Calculate partial sum of nodes
			 * that come before the one pointed to by it.
			 * 
			 * Specialized version for simple containers (i.e. where the
			 * weight function returns only one component) that returns
			 * the result directly instead of using a pointer.
			 */
			template<bool simple_ = simple>
			NVType get_sum_node(typename std::enable_if<simple_,const_iterator>::type it) const {
				NVType res;
				if(it == cend()) this->get_norm_fv(&res);
				this->get_sum_fv_node(it.n,&res);
				return res;
			}
			
			
			/** \brief Calculate partial sum of weights for keys that come before k.
			 * 
			 * Result is returned in res, which must point to an array
			 * large enough (with elements corresponding to the number
			 * of components returned by NVFunc).
			 */
			template<bool simple_ = simple>
			void get_sum(const key_type& k, typename std::enable_if<!simple_,NVType*>::type res) const {
				auto it = lower_bound(k);
				get_sum_node(it,res);
			}
			/** \brief Calculate partial sum of weights for keys that come before k.
			 * 
			 * Specialized version for simple containers (i.e. where the
			 * weight function returns only one component) that returns
			 * the result directly instead of using a pointer.
			 */
			template<bool simple_ = simple>
			NVType get_sum(typename std::enable_if<simple_,const key_type&>::type k) const {
				auto it = lower_bound(k);
				return get_sum_node(it);
			}
			
			/** \brief Calculate partial sum of weights for keys that come before k.
			 * 
			 * Result is returned in res, which must point to an array
			 * large enough (with elements corresponding to the number
			 * of components returned by NVFunc).
			 */
			template<class K, bool simple_ = simple>
			void get_sum(const K& k, typename std::enable_if<!simple_,NVType*>::type res) const {
				auto it = lower_bound(k);
				get_sum_node(it,res);
			}
			/** \brief Calculate partial sum of weights for keys that come before k.
			 * 
			 * Specialized version for simple containers (i.e. where the
			 * weight function returns only one component) that returns
			 * the result directly instead of using a pointer.
			 */
			template<class K, bool simple_ = simple>
			NVType get_sum(typename std::enable_if<simple_,const K&>::type k) const {
				auto it = lower_bound(k);
				return get_sum_node(it);
			}
			
			/// Calculate normalization, i.e. sum of all weights. Equivalent to get_sum(cend(),res).
			template<bool simple_ = simple>
			void get_norm(typename std::enable_if<!simple_,NVType*>::type res) const { this->get_norm_fv(res); }
			/// Calculate normalization, i.e. sum of all weights. Equivalent to get_sum(cend()).
			template<bool simple_ = simple> NVType get_norm(typename std::enable_if<simple_,void*>::type k = 0) const {
				NVType res;
				this->get_norm_fv(&res);
				return res;
			}
			
			
	};
	
	
	/** \brief Adapter for functions that return only one result.
	 * This class also describes the interface expected for the
	 * NVFunc template parameter for the tree / set / map classes.
	 * 
	 * @tparam NVFunc simple function taking one argument and returning the associated value to be stored.
	 * Should define a member type as argument_type to use this adapter.
	 */
	template<class NVFunc> 
	struct NVFunc_Adapter_Simple {
		/// Type of the result of this function.
		typedef typename NVFunc::result_type result_type;
		/// Dimension of result. In case of a simple function, it is one.
		/** Main use case: for a function that has an adjustable parameter,
		 * it can be used as a vector function and results for multiple
		 * parameter combinations can be calculated at the same time.
		 */
		constexpr unsigned int get_nr() const { return 1; }
		/// Calculate the function value associated with one node.
		/**
		 * @param node_value Node's value, i.e. the key in case of a set or multiset,
		 * and a pair of key and value for a map or multimap
		 * 
		 * @param res result is stored here (should be an array of size at
		 * least the number returned by get_nr()
		 */
		void operator ()(const typename NVFunc::argument_type& node_value, result_type* res) const {
			*res = f(node_value);
		}
		NVFunc f;
		NVFunc_Adapter_Simple() { }
		/// Create a new instance storing the given function
		explicit NVFunc_Adapter_Simple(const NVFunc& f_):f(f_) { }
		explicit NVFunc_Adapter_Simple(NVFunc&& f_):f(f_) { }
		template<class T>
		explicit NVFunc_Adapter_Simple(const T& t):f(t) { }
	};
	
	
	/* basic classes */
	
	/* general set and multiset -- needs to be given a node value type and function */
	/* function has to have a defined return type */
	/** \class orbtree::orbset
	 * \brief  General set, i.e. storing a collection of elements without duplicates.
	 * See \ref orbtree::orbtree "orbtree" for description of members.
	 * 
	 * @tparam Key type of elements ("keys") stored in this set.
	 * @tparam NVFunc function calculating the weights associated with stored
	 * elements. See NVFunc_Adapter_Simple for the description of the expected
	 * interface.
	 * @tparam Compare comparison functor for keys.
	 */
	template<class Key, class NVFunc, class Compare = std::less<Key> >
	using orbset = orbtree< NodeAllocatorPtr< KeyOnly<Key>, typename NVFunc::result_type >, Compare, NVFunc, false >;
	
	/** \class orbtree::simple_set
	 * \brief  General set, i.e. storing a collection of elements without duplicates.
	 * Simple version for weight functions that return one component (i.e. scalar functions).
	 * See \ref orbtree::orbtree "orbtree" for description of members.
	 * 
	 * @tparam Key type of elements ("keys") stored in this set.
	 * @tparam NVFunc function object calculating the weights associated with stored
	 * elements. Requires operator() with the key as the only parameter and returning
	 * NVFunc::result_type that should be a public typedef as well.
	 * @tparam Compare comparison functor for keys.
	 */
	template<class Key, class NVFunc, class Compare = std::less<Key> >
	using simple_set = orbtree< NodeAllocatorPtr< KeyOnly<Key>, typename NVFunc::result_type, true >,
		Compare, NVFunc_Adapter_Simple<NVFunc>, false, true >;
	
	
	/** \class orbtree::orbmultiset
	 * \brief  General multiset, i.e. storing a collection of elements allowing duplicates.
	 * See \ref orbtree::orbtree "orbtree" for description of members.
	 * 
	 * @tparam Key type of elements ("keys") stored in this set.
	 * @tparam NVFunc function calculating the values associated with stored
	 * elements. See NVFunc_Adapter_Simple for the description of the expected
	 * interface.
	 * @tparam Compare comparison functor for keys.
	 */
	template<class Key, class NVFunc, class Compare = std::less<Key> >
	using orbmultiset = orbtree< NodeAllocatorPtr< KeyOnly<Key>, typename NVFunc::result_type >, Compare, NVFunc, true >;
	
	/** \class orbtree::simple_multiset
	 * \brief  General multiset, i.e. storing a collection of elements allowing duplicates.
	 * Simple version for weight functions that return one component (i.e. scalar functions).
	 * See \ref orbtree::orbtree "orbtree" for description of members.
	 * 
	 * @tparam Key type of elements ("keys") stored in this set.
	 * @tparam NVFunc function object calculating the weights associated with stored
	 * elements. Requires operator() with the key as the only parameter and returning
	 * NVFunc::result_type that should be a public typedef as well.
	 * @tparam Compare comparison functor for keys.
	 */
	template<class Key, class NVFunc, class Compare = std::less<Key> >
	using simple_multiset = orbtree< NodeAllocatorPtr< KeyOnly<Key>, typename NVFunc::result_type, true >,
		Compare, NVFunc_Adapter_Simple<NVFunc>, true, true >;
	
	
	/** \class orbtree::orbsetC
	 * \brief  Specialized set with compact storage. See \ref orbtree::orbtree "orbtree" for description of members.
	 * 
	 * This is a set, i.e. a collection of elements without duplicates.
	 * See \ref orbtree for description of members. Nodes are stored in a flat array
	 * and the red-black bit is stored as part of node references. Indexing is done
	 * with integers instead of pointers, this can save space especially on 64-bit machines
	 * if 32-bit indices are sufficient. Space for each node is sizeof(Key) +
	 * 3*sizeof(IndexType) + padding (if necessary)
	 * 
	 * @tparam Key Type of elements ("keys") stored in this set.
	 * @tparam NVFunc function calculating the values associated with stored
	 * elements. See NVFunc_Adapter_Simple for the description of the expected
	 * interface.
	 * @tparam IndexType unsigned integral type to use for indexing.
	 * Maximum number of elements is half of the maximum value of this type - 1.
	 * Default is uin32_t, i.e. 32-bit integers, allowing 2^31-1 elements.
	 * @tparam Compare comparison functor for keys.
	 * 
	 * Note: internally, it uses realloc_vector::vector if Key is trivially copyable
	 * (as per [std::is_trivially_copyable](https://en.cppreference.com/w/cpp/types/is_trivially_copyable))
	 * and stacked_vector::vector otherwise. In the latter case, performnace can be
	 * improved by using the [libdivide library](https://github.com/ridiculousfish/libdivide) --
	 * see the documentation of \ref stacked_vector::vector "stacked_vector" for more details.
	 */
	template<class Key, class NVFunc, class IndexType = uint32_t, class Compare = std::less<Key> >
	using orbsetC = orbtree< NodeAllocatorCompact< KeyOnly<Key>, typename NVFunc::result_type, IndexType >, Compare, NVFunc, false >;
	
	/** \class orbtree::simple_setC
	 * \brief  Specialized set with compact storage. 
	 * Simple version for weight functions that return one component (i.e. scalar functions).
	 * See \ref orbtree::orbtree "orbtree" for description of members.
	 * 
	 * This is a set, i.e. a collection of elements without duplicates.
	 * See \ref orbtree for description of members. Nodes are stored in a flat array
	 * and the red-black bit is stored as part of node references. Indexing is done
	 * with integers instead of pointers, this can save space especially on 64-bit machines
	 * if 32-bit indices are sufficient. Space for each node is sizeof(Key) +
	 * 3*sizeof(IndexType) + padding (if necessary)
	 * 
	 * @tparam Key Type of elements ("keys") stored in this set.
	 * @tparam NVFunc function object calculating the weights associated with stored
	 * elements. Requires operator() with the key as the only parameter and returning
	 * NVFunc::result_type that should be a public typedef as well.
	 * @tparam IndexType unsigned integral type to use for indexing.
	 * Maximum number of elements is half of the maximum value of this type - 1.
	 * Default is uin32_t, i.e. 32-bit integers, allowing 2^31-1 elements.
	 * @tparam Compare comparison functor for keys.
	 * 
	 * Note: internally, it uses realloc_vector::vector if Key is trivially copyable
	 * (as per [std::is_trivially_copyable](https://en.cppreference.com/w/cpp/types/is_trivially_copyable))
	 * and stacked_vector::vector otherwise. In the latter case, performnace can be
	 * improved by using the [libdivide library](https://github.com/ridiculousfish/libdivide) --
	 * see the documentation of \ref stacked_vector::vector "stacked_vector" for more details.
	 */
	template<class Key, class NVFunc, class IndexType = uint32_t, class Compare = std::less<Key> >
	using simple_setC = orbtree< NodeAllocatorCompact< KeyOnly<Key>, typename NVFunc::result_type, IndexType >,
		Compare, NVFunc_Adapter_Simple<NVFunc>, false, true >;
	
	
	
	/** \class orbtree::orbmultisetC
	 * \brief  Specialized multiset with compact storage. See \ref orbtree::orbtree "orbtree" for description of members.
	 * 
	 * This is a multiset, i.e. a collection of elements that allows duplicates.
	 * See \ref orbtree for description of members. Nodes are stored in a flat array
	 * and the red-black bit is stored as part of node references. Indexing is done
	 * with integers instead of pointers, this can save space especially on 64-bit machines
	 * if 32-bit indices are sufficient. Space for each node is sizeof(Key) +
	 * 3*sizeof(IndexType) + padding (if necessary)
	 * 
	 * @tparam Key Type of elements ("keys") stored in this set.
	 * @tparam NVFunc function calculating the values associated with stored
	 * elements. See NVFunc_Adapter_Simple for the description of the expected
	 * interface.
	 * @tparam IndexType unsigned integral type to use for indexing.
	 * Maximum number of elements is half of the maximum value of this type - 1.
	 * Default is uin32_t, i.e. 32-bit integers, allowing 2^31-1 elements.
	 * @tparam Compare comparison functor for keys.
	 * 
	 * Note: internally, it uses realloc_vector::vector if Key is trivially copyable
	 * (as per [std::is_trivially_copyable](https://en.cppreference.com/w/cpp/types/is_trivially_copyable))
	 * and stacked_vector::vector otherwise. In the latter case, performnace can be
	 * improved by using the [libdivide library](https://github.com/ridiculousfish/libdivide) --
	 * see the documentation of \ref stacked_vector::vector "stacked_vector" for more details.
	 */
	template<class Key, class NVFunc, class IndexType = uint32_t, class Compare = std::less<Key> >
	using orbmultisetC = orbtree< NodeAllocatorCompact< KeyOnly<Key>, typename NVFunc::result_type, IndexType >, Compare, NVFunc, true >;
	
	/** \class orbtree::simple_multisetC
	 * \brief  Specialized multiset with compact storage.
	 * Simple version for weight functions that return one component (i.e. scalar functions).
	 * See \ref orbtree::orbtree "orbtree" for description of members.
	 * 
	 * This is a multiset, i.e. a collection of elements that allows duplicates.
	 * See \ref orbtree for description of members. Nodes are stored in a flat array
	 * and the red-black bit is stored as part of node references. Indexing is done
	 * with integers instead of pointers, this can save space especially on 64-bit machines
	 * if 32-bit indices are sufficient. Space for each node is sizeof(Key) +
	 * 3*sizeof(IndexType) + padding (if necessary)
	 * 
	 * @tparam Key Type of elements ("keys") stored in this set.
	 * @tparam NVFunc function object calculating the weights associated with stored
	 * elements. Requires operator() with the key as the only parameter and returning
	 * NVFunc::result_type that should be a public typedef as well.
	 * @tparam IndexType unsigned integral type to use for indexing.
	 * Maximum number of elements is half of the maximum value of this type - 1.
	 * Default is uin32_t, i.e. 32-bit integers, allowing 2^31-1 elements.
	 * @tparam Compare comparison functor for keys.
	 * 
	 * Note: internally, it uses realloc_vector::vector if Key is trivially copyable
	 * (as per [std::is_trivially_copyable](https://en.cppreference.com/w/cpp/types/is_trivially_copyable))
	 * and stacked_vector::vector otherwise. In the latter case, performnace can be
	 * improved by using the [libdivide library](https://github.com/ridiculousfish/libdivide) --
	 * see the documentation of \ref stacked_vector::vector "stacked_vector" for more details.
	 */
	template<class Key, class NVFunc, class IndexType = uint32_t, class Compare = std::less<Key> >
	using simple_multisetC = orbtree< NodeAllocatorCompact< KeyOnly<Key>, typename NVFunc::result_type, IndexType >,
		Compare, NVFunc_Adapter_Simple<NVFunc>, true, true >;
		
	
	/* more specialized version: order statistic set / multiset with compact nodes (i.e. calculates ranks, either 32-bit or 64-bit) */
	/// simple function to use for standard order statistic trees; returns 1 for any key, so sum of values can be used to calculate the rank of elements
	template<class KeyValueType, class NVType = uint32_t> struct RankFunc {
		static_assert(std::is_integral<NVType>::value, "rank variable must be integral!\n");
		/// type of argument this function takes; same as the key / value type supplied
		typedef KeyValueType argument_type;
		/// function call operator; returns 1 for any argument
		NVType operator ()(const KeyValueType& k) const { return (NVType)1; }
		typedef NVType result_type;
	};
	
	
	/// adapter for functions that take one parameter from a vector of parameters
	/** @tparam NVFunc Function object to adapt. Should be a function that takes
	 * one extra parameter of type NVFunc::ParType beside the key / value stored in the tree */
	template<class NVFunc> 
	struct NVFunc_Adapter_Vec {
		NVFunc f;
		/// copy of the parameters
		const std::vector<typename NVFunc::ParType> pars;
		/// definition of result for the tree class to use
		typedef typename NVFunc::result_type result_type;
		/// number of component values returned is the same as the number of parameters
		unsigned int get_nr() const { return pars.size(); }
		/// calculate the result of the given function for the given value with each of the parameters
		void operator ()(const typename NVFunc::argument_type& node_value, result_type* res) const {
			for(size_t i=0;i<pars.size();i++) res[i] = f(node_value,pars[i]);
		}
		NVFunc_Adapter_Vec() = delete; /* need parameter vector at least */
		/// this adapter can only be constructed with a parameter vector
		explicit NVFunc_Adapter_Vec(const std::vector<typename NVFunc::ParType>& pars_, const NVFunc& f_ = NVFunc()):f(f_),pars(pars_) { }
		/// this adapter can only be constructed with a parameter vector
		explicit NVFunc_Adapter_Vec(std::vector<typename NVFunc::ParType>&& pars_, const NVFunc& f_ = NVFunc()):f(f_),pars(std::move(pars_)) { }
		/// this adapter can only be constructed with a parameter vector
		explicit NVFunc_Adapter_Vec(const std::vector<typename NVFunc::ParType>& pars_, NVFunc&& f_):f(std::move(f_)),pars(pars_) { }
		/// this adapter can only be constructed with a parameter vector
		explicit NVFunc_Adapter_Vec(std::vector<typename NVFunc::ParType>&& pars_, NVFunc&& f_):f(std::move(f_)),pars(std::move(pars_)) { }
	};
	
	
	/// example function: key^\alpha, i.e. raise the key for a given power
	template<class KeyType> struct NVPower {
		/// main interface takes the exponent as parameter
		double operator () (const KeyType& k, double a) const {
			double x = (double)k;
			return pow(x,a);
		}
		/// result is double
		typedef double result_type;
		/// function argument is the key stored in the tree
		typedef KeyType argument_type;
		/// exponent is double
		typedef double ParType;
	};
	
	/// example function: key^\alpha, i.e. raise the key for a given power; version for a map, where the mapped value is the number of occurrences of the key
	template<class KeyType> struct NVPowerMulti {
		/// now the parameter is assumed to be a pair of the value and the number of occurrences
		double operator () (const KeyType& k, double a) const {
			double x = (double)(k.first);
			double n = (double)(k.second);
			return n*pow(x,a);
		}
		/// result is double
		typedef double result_type;
		/// function argument is the key stored in the tree
		typedef KeyType argument_type;
		/// exponent is double
		typedef double ParType;
	};
	
	template<class KeyType> using NVPower2 = NVFunc_Adapter_Vec<NVPower<KeyType> >;
	template<class KeyType> using NVPowerMulti2 = NVFunc_Adapter_Vec<NVPowerMulti<KeyType> >;
	
	/** \class orbtree::rankset
	 * \brief  Order statistic set, calculates the rank of elements.
	 * See \ref orbtree::orbtree "orbtree" for description of members.
	 * 
	 * @tparam Key type of elements ("keys") stored in this set.
	 * @tparam NVType integer type for rank calculation
	 * @tparam Compare comparison functor for keys.
	 */
	template<class Key, class NVType = uint32_t, class Compare = std::less<Key> >
	using rankset = orbtree< NodeAllocatorPtr< KeyOnly<Key>, NVType, true >, Compare,
			NVFunc_Adapter_Simple<RankFunc<Key, NVType> >, false, true >;
	
	/** \class orbtree::rankmultiset
	 * \brief  Order statistic multiset, calculates the rank of elements.
	 * 
	 * @tparam Key type of elements ("keys") stored in this set.
	 * @tparam NVType integer type for rank calculation
	 * @tparam Compare comparison functor for keys.
	 */
	template<class Key, class NVType = uint32_t, class Compare = std::less<Key> >
	using rankmultiset = orbtree< NodeAllocatorPtr< KeyOnly<Key>, NVType, true >, Compare,
			NVFunc_Adapter_Simple<RankFunc<Key, NVType> >, true, true >;
	
	/** \class orbtree::ranksetC
	 * \brief  Order statistic set, calculates the rank of elements, compact version.
	 * See \ref orbtree::orbtree "orbtree" for description of members.
	 * 
	 * @tparam Key Type of elements ("keys") stored in this set.
	 * @tparam NVType integer type for rank calculation
	 * @tparam Compare comparison functor for keys.
	 * 
	 * Note: internally, it uses realloc_vector::vector if Key is trivially copyable
	 * (as per [std::is_trivially_copyable](https://en.cppreference.com/w/cpp/types/is_trivially_copyable))
	 * and stacked_vector::vector otherwise. In the latter case, performnace can be
	 * improved by using the [libdivide library](https://github.com/ridiculousfish/libdivide) --
	 * see the documentation of \ref stacked_vector::vector "stacked_vector" for more details.
	 */
	template<class Key, class NVType = uint32_t, class IndexType = uint32_t, class Compare = std::less<Key> >
	using ranksetC = orbtree< NodeAllocatorCompact< KeyOnly<Key>, NVType, IndexType >, Compare,
			NVFunc_Adapter_Simple<RankFunc<Key, NVType> >, false, true >;
	
	/** \class orbtree::rankmultisetC
	 * \brief  Order statistic multiset, calculates the rank of elements, compact version.
	 * See \ref orbtree::orbtree "orbtree" for description of members.
	 * 
	 * @tparam Key Type of elements ("keys") stored in this set.
	 * @tparam NVType integer type for rank calculation
	 * @tparam Compare comparison functor for keys.
	 * 
	 * Note: internally, it uses realloc_vector::vector if Key is trivially copyable
	 * (as per [std::is_trivially_copyable](https://en.cppreference.com/w/cpp/types/is_trivially_copyable))
	 * and stacked_vector::vector otherwise. In the latter case, performnace can be
	 * improved by using the [libdivide library](https://github.com/ridiculousfish/libdivide) --
	 * see the documentation of \ref stacked_vector::vector "stacked_vector" for more details.
	 */
	template<class Key, class NVType = uint32_t, class IndexType = uint32_t, class Compare = std::less<Key> >
	using rankmultisetC = orbtree< NodeAllocatorCompact< KeyOnly<Key>, NVType, IndexType >, Compare,
			NVFunc_Adapter_Simple<RankFunc<Key, NVType> >, true, true >;
	
		
	/* map types -- extra step in adding accessor functions (only for map, multimap does not have them)
	 * note: this definition does not explicitely require KeyValue to have value, but the extra functions rely on Value being present
	 * 
	 * note: only const values are supported since modifying the value might modify the rank function as well */
	/** \brief Base class with map-specific function. It is recommended
	 * to use the specializations \ref simple_map, \ref simple_mapC,
	 * \ref simple_multimap, \ref simple_multimapC, \ref orbmap, orbmapC,
	 * \ref orbmultimap and orbmultimapC to define and instantiate this
	 * class instead of this interface directly.
	 * 
	 * Provides implementation for map-specific interface.
	 * 
	 	 * @tparam NodeAllocator internal base class to store nodes
	 * @tparam Comapre comparison functor
	 * @tparam NVFunc function that calculates values associated with
	 * 		elements based on key and value
	 */
	template<class NodeAllocator, class Compare, class NVFunc, bool simple = false>
	class orbtreemap : public orbtree<NodeAllocator, Compare, NVFunc, false, simple> {
		protected:
			typedef typename orbtree<NodeAllocator, Compare, NVFunc, false, simple>::NodeHandle NodeHandle;
			typedef typename orbtree<NodeAllocator, Compare, NVFunc, false, simple>::KeyValue KeyValue;
		
		public:
			/* typedefs */
			typedef typename orbtree<NodeAllocator, Compare, NVFunc, false, simple>::value_type value_type;
			typedef typename orbtree<NodeAllocator, Compare, NVFunc, false, simple>::key_type key_type;
			typedef typename orbtree<NodeAllocator, Compare, NVFunc, false, simple>::size_type size_type;
			typedef typename orbtree<NodeAllocator, Compare, NVFunc, false, simple>::difference_type difference_type;
			typedef typename orbtree<NodeAllocator, Compare, NVFunc, false, simple>::iterator iterator;
			typedef typename orbtree<NodeAllocator, Compare, NVFunc, false, simple>::const_iterator const_iterator;
			/// type of values stored in this map
			typedef typename NodeAllocator::KeyValue::MappedType mapped_type;
			
			explicit orbtreemap(const NVFunc& f_ = NVFunc(), const Compare& c_ = Compare()) :
				orbtree<NodeAllocator, Compare, NVFunc, false, simple>(f_,c_) { }
			explicit orbtreemap(NVFunc&& f_,const Compare& c_ = Compare()) : orbtree<NodeAllocator, Compare, NVFunc, false, simple>(f_,c_) { }
			template <class T>
			explicit orbtreemap(const T& t, const Compare& c_ = Compare()) : orbtree<NodeAllocator, Compare, NVFunc, false, simple>(t,c_) { }
			
			/// Access mapped value for a given key, throws an exception if key is not found.
			/** Cannot be used to modify value since that can require
			 * updating the tree internally. Use the functions \ref set_value() or
			 * \ref update_value() for that purpose instead.
			 */
			const mapped_type& at(const key_type& k) const {
				NodeHandle n = orbtree_base<NodeAllocator, Compare, NVFunc, false>::find(k);
				if(n == this->nil()) throw std::out_of_range("orbtreemap::at(): key not present in map!\n");
				return get_node(n).get_key_value().value();
			}
			/// Access mapped value for a key that compares equal to k, throws an exception if not such key is found
			/** Cannot be used to modify value since that can require
			 * updating the tree internally. Use the functions \ref set_value() or
			 * \ref update_value() for that purpose instead.
			 */
			template<class K> const mapped_type& at(const K& k) const {
				NodeHandle n = orbtree_base<NodeAllocator, Compare, NVFunc, false>::find(k);
				if(n == this->nil()) throw std::out_of_range("orbtreemap::at(): key not present in map!\n");
				return get_node(n).get_key_value().value();
			}
			
			/// Access mapped value for a given key, inserts a new element with the default value if not found.
			/** Cannot be used to modify value since that can require
			 * updating the tree internally. Use the functions \ref set_value() or
			 * \ref update_value() for that purpose instead.
			 */
			const mapped_type& operator[](const key_type& k) {
				NodeHandle n = orbtree_base<NodeAllocator, Compare, NVFunc, false>::find(k);
				if(n == this->nil()) n = orbtree_base<NodeAllocator, Compare, NVFunc, false>::insert(value_type(k,mapped_type()));
				return get_node(n).get_key_value().value();
			}
			
			/** \brief set value associated with the given key or insert new element
			 * 
			 * Returns true if a new element was inserted, false if the
			 * value of an existing element was updated. */
			bool set_value(const key_type& k, const mapped_type& v) {
				std::pair<iterator,bool> tmp = orbtree<NodeAllocator, Compare, NVFunc, false, simple>::insert(value_type(k,v));
				if(tmp.second == false) tmp.first.set_value(v);
				return tmp.second;
			}
			/** \brief set value associated with the given key or insert new element
			 * 
			 * Returns true if a new element was inserted, false if the
			 * value of an existing element was updated. */
			bool set_value(key_type&& k, const mapped_type& v) {
				std::pair<iterator,bool> tmp = orbtree<NodeAllocator, Compare, NVFunc, false, simple>::insert(value_type(std::move(k),v));
				if(tmp.second == false) tmp.first.set_value(v);
				return tmp.second;
			}
			/** \brief set value associated with the given key or insert new element
			 * 
			 * Returns true if a new element was inserted, false if the
			 * value of an existing element was updated. */
			bool set_value(const key_type& k, mapped_type&& v) {
				auto it = orbtree<NodeAllocator, Compare, NVFunc, false, simple>::find(k);
				if(it == orbtree<NodeAllocator, Compare, NVFunc, false, simple>::end()) {
					orbtree<NodeAllocator, Compare, NVFunc, false, simple>::insert(value_type(k,std::move(v)));
					return true;
				}
				it.set_value(std::move(v));
				return false;
			}
			/** \brief set value associated with the given key or insert new element
			 * 
			 * Returns true if a new element was inserted, false if the
			 * value of an existing element was updated. */
			bool set_value(key_type&& k, mapped_type&& v) {
				auto it = orbtree<NodeAllocator, Compare, NVFunc, false, simple>::find(k);
				if(it == orbtree<NodeAllocator, Compare, NVFunc, false, simple>::end()) {
					orbtree<NodeAllocator, Compare, NVFunc, false, simple>::insert(value_type(std::move(k),std::move(v)));
					return true;
				}
				it.set_value(std::move(v));
				return false;
			}
			
			/** \brief update the value of an existing element -- throws exception if the key does not exist */
			void update_value(const key_type& k, const mapped_type& v) {
				NodeHandle n = orbtree_base<NodeAllocator, Compare, NVFunc, false>::find(k);
				if(n == this->nil()) throw std::out_of_range("orbtreemap::at(): key not present in map!\n");
				orbtree_base<NodeAllocator, Compare, NVFunc, false>::update_value(n,v);
			}
			/** \brief update the value of an existing element -- throws exception if the key does not exist */
			void update_value(const key_type& k, mapped_type&& v) {
				NodeHandle n = orbtree_base<NodeAllocator, Compare, NVFunc, false>::find(k);
				if(n == this->nil()) throw std::out_of_range("orbtreemap::at(): key not present in map!\n");
				orbtree_base<NodeAllocator, Compare, NVFunc, false>::update_value(n,std::move(v));
			}
	};
	
	/** \class orbtree::orbmap
	 * \brief General map implementation. See \ref orbtree::orbtree "orbtree"
	 * and \ref orbtreemap for description of members.
	 * 
	 * @tparam Key Key to sort elements by.
	 * @tparam Value Value stored in elements.
	 * @tparam NVFunc function calculating the values associated with stored
	 * elements. See NVFunc_Adapter_Simple for the description of the expected
	 * interface.
	 * @tparam Compare comparison functor for keys.
	 */
	template<class Key, class Value, class NVFunc, class Compare = std::less<Key> >
	using orbmap = orbtreemap< NodeAllocatorPtr< KeyValue<Key,Value>, typename NVFunc::result_type >, Compare, NVFunc >;
	
	/** \class orbtree::simple_map
	 * \brief General map implementation.
	 * Simple version for weight functions that return one component (i.e. scalar functions).
	 * See \ref orbtree::orbtree "orbtree" and \ref orbtreemap for description of members.
	 * 
	 * @tparam Key Key to sort elements by.
	 * @tparam Value Value stored in elements.
	 * @tparam NVFunc function object calculating the weights associated with stored
	 * elements. Requires operator() with the key as the only parameter and returning
	 * NVFunc::result_type that should be a public typedef as well.
	 * @tparam Compare comparison functor for keys.
	 */
	template<class Key, class Value, class NVFunc, class Compare = std::less<Key> >
	using simple_map = orbtreemap< NodeAllocatorPtr< KeyValue<Key,Value>, typename NVFunc::result_type, true >,
		Compare, NVFunc_Adapter_Simple<NVFunc>, true >;
	
	
	/** \class orbtree::orbmultimap
	 * \brief General multimap implementation (i.e. map allowing duplicate keys).
	 * See \ref orbtree::orbtree "orbtree" for description of members.
	 * 
	 * @tparam Key Key to sort elements by.
	 * @tparam Value Value stored in elements.
	 * @tparam NVFunc function calculating the values associated with stored
	 * elements. See NVFunc_Adapter_Simple for the description of the expected
	 * interface.
	 * @tparam Compare comparison functor for keys.
	 */
	template<class Key, class Value, class NVFunc, class Compare = std::less<Key> >
	using orbmultimap = orbtree< NodeAllocatorPtr< KeyValue<Key,Value>, typename NVFunc::result_type >, Compare, NVFunc, true >;
	
	/** \class orbtree::simple_multimap
	 * \brief General multimap implementation (i.e. map allowing duplicate keys).
	 * Simple version for weight functions that return one component (i.e. scalar functions).
	 * See \ref orbtree::orbtree "orbtree" for description of members.
	 * 
	 * @tparam Key Key to sort elements by.
	 * @tparam Value Value stored in elements.
	 * @tparam NVFunc function object calculating the weights associated with stored
	 * elements. Requires operator() with the key as the only parameter and returning
	 * NVFunc::result_type that should be a public typedef as well.
	 * @tparam Compare comparison functor for keys.
	 */
	template<class Key, class Value, class NVFunc, class Compare = std::less<Key> >
	using simple_multimap = orbtree< NodeAllocatorPtr< KeyValue<Key,Value>, typename NVFunc::result_type, true >,
		Compare, NVFunc_Adapter_Simple<NVFunc>, true, true>;
	
	
	/** \class orbtree::orbmapC
	 * \brief Map implementation with compact storage. See \ref orbtree::orbtree "orbtree"
	 * and \ref orbtreemap for description of members.
	 * 
	 * @tparam Key Key to sort elements by.
	 * @tparam Value Value stored in elements.
	 * @tparam NVFunc function calculating the values associated with stored
	 * elements. See NVFunc_Adapter_Simple for the description of the expected
	 * interface.
	 * @tparam IndexType unsigned integral type to use for indexing.
	 * Maximum number of elements is half of the maximum value of this type - 1.
	 * Default is uin32_t, i.e. 32-bit integers, allowing 2^31-1 elements.
	 * @tparam Compare comparison functor for keys.
	 * 
	 * Note: internally, it uses realloc_vector::vector if both Key and Value are trivially copyable
	 * (as per [std::is_trivially_copyable](https://en.cppreference.com/w/cpp/types/is_trivially_copyable))
	 * and stacked_vector::vector otherwise. In the latter case, performnace can be
	 * improved by using the [libdivide library](https://github.com/ridiculousfish/libdivide) --
	 * see the documentation of \ref stacked_vector::vector "stacked_vector" for more details.
	 */
	template<class Key, class Value, class NVFunc, class IndexType = uint32_t, class Compare = std::less<Key> >
	using orbmapC = orbtreemap< NodeAllocatorCompact< KeyValue<Key,Value>, typename NVFunc::result_type, IndexType>, Compare, NVFunc >;
	
	/** \class orbtree::simple_mapC
	 * \brief Map implementation with compact storage.
	 * Simple version for weight functions that return one component (i.e. scalar functions).
	 * See \ref orbtree::orbtree "orbtree" and \ref orbtreemap for description of members.
	 * 
	 * @tparam Key Key to sort elements by.
	 * @tparam Value Value stored in elements.
	 * @tparam NVFunc function object calculating the weights associated with stored
	 * elements. Requires operator() with the key as the only parameter and returning
	 * NVFunc::result_type that should be a public typedef as well.
	 * @tparam IndexType unsigned integral type to use for indexing.
	 * Maximum number of elements is half of the maximum value of this type - 1.
	 * Default is uin32_t, i.e. 32-bit integers, allowing 2^31-1 elements.
	 * @tparam Compare comparison functor for keys.
	 * 
	 * Note: internally, it uses realloc_vector::vector if both Key and Value are trivially copyable
	 * (as per [std::is_trivially_copyable](https://en.cppreference.com/w/cpp/types/is_trivially_copyable))
	 * and stacked_vector::vector otherwise. In the latter case, performnace can be
	 * improved by using the [libdivide library](https://github.com/ridiculousfish/libdivide) --
	 * see the documentation of \ref stacked_vector::vector "stacked_vector" for more details.
	 */
	template<class Key, class Value, class NVFunc, class IndexType = uint32_t, class Compare = std::less<Key> >
	using simple_mapC = orbtreemap< NodeAllocatorCompact< KeyValue<Key,Value>, typename NVFunc::result_type, IndexType>,
		Compare, NVFunc_Adapter_Simple<NVFunc>, true >;
	
	
	/** \class orbtree::orbmultimapC
	 * \brief Multimap implementation with compact storage. See \ref orbtree::orbtree "orbtree" for description of members.
	 * 
	 * @tparam Key Key to sort elements by.
	 * @tparam Value Value stored in elements.
	 * @tparam NVFunc function calculating the values associated with stored
	 * elements. See NVFunc_Adapter_Simple for the description of the expected
	 * interface.
	 * @tparam IndexType unsigned integral type to use for indexing.
	 * Maximum number of elements is half of the maximum value of this type - 1.
	 * Default is uin32_t, i.e. 32-bit integers, allowing 2^31-1 elements.
	 * @tparam Compare comparison functor for keys.
	 * 
	 * Note: internally, it uses realloc_vector::vector if both Key and Value are trivially copyable
	 * (as per [std::is_trivially_copyable](https://en.cppreference.com/w/cpp/types/is_trivially_copyable))
	 * and stacked_vector::vector otherwise. In the latter case, performnace can be
	 * improved by using the [libdivide library](https://github.com/ridiculousfish/libdivide) --
	 * see the documentation of \ref stacked_vector::vector "stacked_vector" for more details.
	 */
	template<class Key, class Value, class NVFunc, class IndexType = uint32_t, class Compare = std::less<Key> >
	using orbmultimapC = orbtree< NodeAllocatorCompact< KeyValue<Key,Value>, typename NVFunc::result_type, IndexType>, Compare, NVFunc, true >;
	
	/** \class orbtree::simple_multimapC
	 * \brief Multimap implementation with compact storage.
	 * Simple version for weight functions that return one component (i.e. scalar functions).
	 * See \ref orbtree::orbtree "orbtree" for description of members.
	 * 
	 * @tparam Key Key to sort elements by.
	 * @tparam Value Value stored in elements.
	 * @tparam NVFunc function object calculating the weights associated with stored
	 * elements. Requires operator() with the key as the only parameter and returning
	 * NVFunc::result_type that should be a public typedef as well.
	 * @tparam IndexType unsigned integral type to use for indexing.
	 * Maximum number of elements is half of the maximum value of this type - 1.
	 * Default is uin32_t, i.e. 32-bit integers, allowing 2^31-1 elements.
	 * @tparam Compare comparison functor for keys.
	 * 
	 * Note: internally, it uses realloc_vector::vector if both Key and Value are trivially copyable
	 * (as per [std::is_trivially_copyable](https://en.cppreference.com/w/cpp/types/is_trivially_copyable))
	 * and stacked_vector::vector otherwise. In the latter case, performnace can be
	 * improved by using the [libdivide library](https://github.com/ridiculousfish/libdivide) --
	 * see the documentation of \ref stacked_vector::vector "stacked_vector" for more details.
	 */
	template<class Key, class Value, class NVFunc, class IndexType = uint32_t, class Compare = std::less<Key> >
	using simple_multimapC = orbtree< NodeAllocatorCompact< KeyValue<Key,Value>, typename NVFunc::result_type, IndexType>,
		Compare, NVFunc_Adapter_Simple<NVFunc>, true, true >;
	
	
	/** \class orbtree::rankmap
	 * \brief  Order statistic map, calculates the rank of elements.
	 * See \ref orbtree::orbtree "orbtree" and \ref orbtreemap for description of members.
	 * 
	 * @tparam Key type of elements ("keys") stored in this set.
	 * @tparam Value Value stored in elements.
	 * @tparam NVType Integer type for rank calculation.
	 * @tparam Compare Comparison functor for keys.
	 */
	template<class Key, class Value, class NVType, class Compare = std::less<Key> >
	using rankmap = orbtreemap< NodeAllocatorPtr< KeyValue<Key,Value>, NVType, true >, Compare,
			NVFunc_Adapter_Simple< RankFunc<NVType,KeyValue<Key,Value> > >, true >;
	
	/** \class orbtree::rankmultimap
	 * \brief  Order statistic multimap, calculates the rank of elements.
	 * See \ref orbtree::orbtree "orbtree" for description of members.
	 * 
	 * @tparam Key type of elements ("keys") stored in this set.
	 * @tparam Value Value stored in elements.
	 * @tparam NVType Integer type for rank calculation.
	 * @tparam Compare Comparison functor for keys.
	 */
	template<class Key, class Value, class NVType, class Compare = std::less<Key> >
	using rankmultimap = orbtree< NodeAllocatorPtr< KeyValue<Key,Value>, NVType, true >, Compare,
			NVFunc_Adapter_Simple< RankFunc<NVType,KeyValue<Key,Value> > >, true, true >;
	
	/** \class orbtree::rankmapC
	 * \brief  Order statistic map with compact storage, calculates the rank of elements.
	 * See \ref orbtree::orbtree "orbtree" and \ref orbtreemap for description of members.
	 * 
	 * @tparam Key Key to sort elements by.
	 * @tparam Value Value stored in elements.
	 * @tparam NVType Integer type for rank calculation.
	 * @tparam IndexType unsigned integral type to use for indexing.
	 * Maximum number of elements is half of the maximum value of this type - 1.
	 * Default is uin32_t, i.e. 32-bit integers, allowing 2^31-1 elements.
	 * @tparam Compare Comparison functor for keys.
	 * 
	 * Note: internally, it uses realloc_vector::vector if both Key and Value are trivially copyable
	 * (as per [std::is_trivially_copyable](https://en.cppreference.com/w/cpp/types/is_trivially_copyable))
	 * and stacked_vector::vector otherwise. In the latter case, performnace can be
	 * improved by using the [libdivide library](https://github.com/ridiculousfish/libdivide) --
	 * see the documentation of \ref stacked_vector::vector "stacked_vector" for more details.
	 */
	template<class Key, class Value, class NVType, class IndexType = uint32_t, class Compare = std::less<Key> >
	using rankmapC = orbtreemap< NodeAllocatorCompact< KeyValue<Key,Value>, NVType, IndexType >, Compare,
			NVFunc_Adapter_Simple< RankFunc<NVType,KeyValue<Key,Value> > >, true >;
	
	/** \class orbtree::rankmultimapC
	 * \brief  Order statistic multimap with compact storage, calculates the rank of elements.
	 * See \ref orbtree::orbtree "orbtree" for description of members.
	 * 
	 * @tparam Key Key to sort elements by.
	 * @tparam Value Value stored in elements.
	 * @tparam NVType Integer type for rank calculation.
	 * @tparam IndexType unsigned integral type to use for indexing.
	 * Maximum number of elements is half of the maximum value of this type - 1.
	 * Default is uin32_t, i.e. 32-bit integers, allowing 2^31-1 elements.
	 * @tparam Compare Comparison functor for keys.
	 * 
	 * Note: internally, it uses realloc_vector::vector if both Key and Value are trivially copyable
	 * (as per [std::is_trivially_copyable](https://en.cppreference.com/w/cpp/types/is_trivially_copyable))
	 * and stacked_vector::vector otherwise. In the latter case, performnace can be
	 * improved by using the [libdivide library](https://github.com/ridiculousfish/libdivide) --
	 * see the documentation of \ref stacked_vector::vector "stacked_vector" for more details.
	 */
	template<class Key, class Value, class NVType, class IndexType = uint32_t, class Compare = std::less<Key> >
	using rankmultimapC = orbtree< NodeAllocatorCompact< KeyValue<Key,Value>, NVType, IndexType >, Compare,
			NVFunc_Adapter_Simple< RankFunc<NVType,KeyValue<Key,Value> > >, true, true >;
	
}

#undef CONSTEXPR

#endif

