/*  -*- C++ -*-
 * orbtree_base.h -- generalized order statistic red-black tree implementation
 * 	main definition of tree and functionality
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

#ifndef ORBTREE_BASE_H
#define ORBTREE_BASE_H
#include "orbtree_node.h"

namespace orbtree {
	
	template<class NVFunc> class NVFunc_wrapper {
		protected:
			NVFunc f;
			explicit NVFunc_wrapper(const NVFunc& f_):f(f_) { }
			explicit NVFunc_wrapper(NVFunc&& f_):f(std::move(f_)) { }
			template<class T>
			explicit NVFunc_wrapper(const T& t):f(t) { }
	};
	
	/** \brief base class for both map and set -- should not be used directly
	 * 
	 * @tparam NodeAllocator Class taking care of allocating and freeing
	 * nodes, should be NodeAllocatorPtr or NodeAllocatorCompact from \ref orbtree_node.h
	 * @tparam Compare Comparison functor
	 * @tparam NVFunc Function that calculates each node's "value" -- it is assumed to be a
	 * vector-valued function, so these values are stored and manipulated in
	 * arrays. It should have a function get_nr() that return the number of
	 * values to use. See NVFunc_Adapter_Simple and NVFunc_Adapter_Vec for examples.
	 * @tparam multi Whether multiple nodes with the same key are allowed.
	 */
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	class orbtree_base : public NVFunc_wrapper<NVFunc>, NodeAllocator {
		protected:
			/// \brief node objects
			typedef typename NodeAllocator::Node Node;
			typedef typename NodeAllocator::KeyValue KeyValue;
			typedef typename NodeAllocator::KeyValue::KeyType KeyType;
			typedef typename NodeAllocator::KeyValue::ValueType ValueType;
			/// \brief handle to refer nodes to (pointer or integer)
			typedef typename NodeAllocator::NodeHandle NodeHandle;
			using NodeAllocator::Invalid;
		
		public:
			/// \brief Type the function NVFunc returns
			typedef typename NodeAllocator::NVType NVType;
		
		protected:
			/// \brief function to add NVType values; checks for overflow and throws exception in the case of integral types
			void NVAdd(NVType* x, const NVType* y) const; /* x = x + y */
			/// \brief function to subtract NVType values; checks for overflow and throws exception in the case of integral types
			void NVSubtract(NVType* x, const NVType* y) const; /* x = x - y */
		
			/** \brief keep track of the number of inserted elements */
			size_t size1;
			NVFunc& f;
			Compare c;
			
			void create_sentinels() {
				Node& rootn = get_node(root());
				rootn.set_parent(nil());
				rootn.set_left(nil());
				rootn.set_right(nil());
				rootn.set_black();
				Node& niln = get_node(nil());
				niln.set_parent(nil());
				niln.set_left(nil());
				niln.set_right(nil());
				niln.set_black(); /* nil should always be black */
				size1 = 0;
			}
			
			/// \brief convenience function that returns node object for a node handle
			Node& get_node(NodeHandle n) { return NodeAllocator::get_node(n); }
			/// \brief convenience function that returns node object for a node handle
			const Node& get_node(NodeHandle n) const { return NodeAllocator::get_node(n); }
			/// \brief handle of root sentinel
			NodeHandle root() const { return NodeAllocator::root; }
			/// \brief handle of nil sentinel
			NodeHandle nil() const { return NodeAllocator::nil; }
			
			
		//~ public:
			orbtree_base() { create_sentinels(); }
			explicit orbtree_base(const NVFunc& f_, const Compare& c_) : NVFunc_wrapper<NVFunc>(f_),
				NodeAllocator(NVFunc_wrapper<NVFunc>::f.get_nr()), f(NVFunc_wrapper<NVFunc>::f), c(c_) { create_sentinels(); }
			explicit orbtree_base(NVFunc&& f_, const Compare& c_) : NVFunc_wrapper<NVFunc>(std::move(f_)),
				NodeAllocator(NVFunc_wrapper<NVFunc>::f.get_nr()), f(NVFunc_wrapper<NVFunc>::f), c(c_) { create_sentinels(); }
			template<class T>
			explicit orbtree_base(const T& t, const Compare& c_) : NVFunc_wrapper<NVFunc>(t),
				NodeAllocator(NVFunc_wrapper<NVFunc>::f.get_nr()), f(NVFunc_wrapper<NVFunc>::f), c(c_) { create_sentinels(); }
			
			/* copy / move constructor -- not implemented yet */
			
			
			/* interface for finding elements / inserting
			 * returns position of new node (if inserted) or node with the same key (for non-multi map/set)
			 * and bool flag indicating if insert happened */
			/** \brief Insert new element
			 * 
			 * Return type is always std::pair<iterator,bool>,
			 * where the first element is a handle to the newly inserted
			 * node and the second element indicates if insert was successful.
			 * 
			 * For multi map/set, insert always succeeds, to the second
			 * element is always true. In this case, a new element is always
			 * inserted after any existing elements with the same key.
			 * 
			 * For a non-multi map/set, insert fails if an element with
			 * the same key already exists. In this case, a handle to the
			 * element with the same key is returned along with false in the
			 * second element. */
			std::pair<NodeHandle,bool> insert(ValueType&& kv);
			/** \copydoc insert(ValueType&& kv) */
			std::pair<NodeHandle,bool> insert(const ValueType& kv);
			/** \brief Insert new element with hint
			 * 
			 * \copydetails insert(ValueType&& kv)
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
			 * 	- otherwise, the new element is inserted as close as possible
			 */
			NodeHandle insert(NodeHandle hint, ValueType&& kv);
			/** \copydoc insert(NodeHandle hint, ValueType&& kv) */
			NodeHandle insert(NodeHandle hint, const ValueType& kv);
			/** \brief Construct new element in place
			 * 
			 * \copydetails insert(ValueType&& kv)
			 */
			template<class... T> std::pair<NodeHandle,bool> emplace(T&&... t);
			/** \brief Construct new element in place with hint
			 * 
			 * \copydetails insert(NodeHandle hint, ValueType&& kv)
			 */
			template<class... T> NodeHandle emplace_hint(NodeHandle hint, T&&... t);
			
			/** \brief helper for insert -- find the location where to insert
	 
			 * note: in a multi map/set, the inserted element will come after any already
			 * existing elements with the same key
			 * 
			 * Return true if insert is possible, false if element already 
			 * exists and this is a non-multi map/set.
			 */
			bool insert_search(const KeyType& k, NodeHandle& n, bool& insert_left) const;
			/** \copydoc insert_search(const KeyType& k, NodeHandle& n, bool& insert_left) const
			 * 
			 * Try to insert as close to hint as possible. Note: if hint is bad, it is ignored.
			 */
			bool insert_search_hint(NodeHandle hint, const KeyType& k, NodeHandle& n, bool& insert_left) const;
			/** \brief helper function to do the real work for insert 
			 * 
			 * insert n1 as the left / right child of n
			 * 
			 * n1 must already contain the proper key / value, but can be uninitialized otherwise
			 * 
			 * note: this function does not check for the correct relationship between the keys of n and n1,
			 * that is the caller's responsibility */
			void insert_helper(NodeHandle n, NodeHandle n1, bool insert_left);
			
			/** \brief find any node with the given key
			 * 
			 *  K should be comparable to KeyType
			 * 
			 * returns nil if not found (to make it easier with iterators) */
			template<class K> auto find(const K& key) const -> NodeHandle;
			/// \brief find the first node not less than the given key -- returns nil if none found
			template<class K> auto lower_bound(const K& key) const -> NodeHandle;
			/// \brief find the first node larger than the given key -- returns nil if none found
			template<class K> auto upper_bound(const K& key) const -> NodeHandle;
			
			/// \brief convenience function to get the key of a node
			const KeyType& get_node_key(NodeHandle n) const { return get_node(n).get_key_value().key(); }
			
			/// \brief helper for erase to compare elements
			bool compare_key_equals(NodeHandle n, const KeyType& k) const {
				return (!c(get_node_key(n),k)) && (!c(k,get_node_key(n)));
			}
			
			/** \brief get the value of NVFunc for the given node
			 * 
			 * should not be called with nil, root or Invalid
			 * 
			 * note that rank function can depend on a node's value as well;
			 * care need to be taken to update rank if a node's value changes! */
			void get_node_grvalue(NodeHandle n, NVType* res) const { f(get_node(n).get_key_value().keyvalue(), res); }
			
			/// \brief get first node (or nil)
			NodeHandle first() const;
			/// \brief get last node (or nil)
			NodeHandle last() const;
			/// \brief get node after n (or nil) -- note: next(nil) == nil
			NodeHandle next(NodeHandle n) const;
			/// \brief get node before n (or nil) -- note: previous(nil) == last() to make it easier to implement end() iterator
			NodeHandle previous(NodeHandle n) const;
			
			/// \brief remove the given node -- return the next node (i.e. next(n) before deleting n)
			NodeHandle erase(NodeHandle n);
			
			/// \brief convenience helper to get node object that is the left child of the given node handle
			Node& get_left(NodeHandle n) { return get_node(get_node(n).get_left()); }
			/// \brief convenience helper to get node object that is the left child of the given node handle
			const Node& get_left(NodeHandle n) const { return get_node(get_node(n).get_left()); }
			/// \brief convenience helper to get node object that is the right child of the given node handle
			Node& get_right(NodeHandle n) { return get_node(get_node(n).get_right()); }
			/// \brief convenience helper to get node object that is the right child of the given node handle
			const Node& get_right(NodeHandle n) const { return get_node(get_node(n).get_right()); }
			/// \brief convenience helper to get node object that is the parent of the given node handle
			Node& get_parent(NodeHandle n) { return get_node(get_node(n).get_parent()); }
			/// \brief convenience helper to get node object that is the parent of the given node handle
			const Node& get_parent(NodeHandle n) const { return get_node(get_node(n).get_parent()); }
			/// \brief convenience helper to get the handle of a node's sibling (i.e. parent's other child)
			NodeHandle get_sibling_handle(NodeHandle n) const {
				if(n == get_parent(n).get_left()) return get_parent(n).get_right();
				else return get_parent(n).get_left();
			}
			/// \brief convenience helper to get the node object of a node's sibling (i.e. parent's other child)
			Node& get_sibling(NodeHandle n) { return get_node(get_sibling_handle(n)); }
			/// \brief convenience helper to get the node object of a node's sibling (i.e. parent's other child)
			const Node& get_sibling(NodeHandle n) const { return get_node(get_sibling_handle(n)); }
			/// \brief check which side child is n (i.e. returns true if n is the left child of its parent) -- requires that n != root
			bool is_left_side(NodeHandle n) const { return (get_parent(n).get_left() == n); }
		
			/// \brief update the sum only inside n
			void update_sum(NodeHandle n);
			/// \brief update the sum recursively up the tree
			void update_sum_r(NodeHandle n) { for(;n != root();n = get_node(n).get_parent()) update_sum(n); }
			
			/** \brief update value in a node -- only if this is a map;
			 * update sum recursively based on it as well */
			template<class KeyValue_ = KeyValue>
			void update_value(NodeHandle n, typename KeyValue_::MappedType const& v) {
				get_node(n).get_key_value().value() = v;
				update_sum_r(n);
			}
			/** \brief update value in a node -- only if this is a map;
			 * update sum recursively based on it as well */
			template<class KeyValue_ = KeyValue>
			void update_value(NodeHandle n, typename KeyValue_::MappedType&& v) {
				get_node(n).get_key_value().value() = std::move(v);
				update_sum_r(n);
			}
			
			/** \brief left rotate
			 * 
			 * right child of x takes its place, x becomes its left child
			 * left child of x's original right child becomes x's right child
			 */
			void rotate_left(NodeHandle x);
			/** \brief right rotate
			 * 
			 * left child of x takes its place, y becomes its right child
			 * right child of x's original left child becomes x's left child */
			void rotate_right(NodeHandle x);
			/** \brief rotate by parent of x (x takes parent's place), this calls either
			 * rotate_left() or rotate_right() with the parent of x */
			void rotate_parent(NodeHandle x);
			
		public:
			/// \brief erase all nodes
			void clear() {
				NodeAllocator::clear_tree(); /* free up all nodes (except root and nil sentinels) */
				create_sentinels(); /* reset sentinels */
			}
			
			/** \brief get the generalized rank for a key, i.e. the sum of NVFunc for all nodes with node.key < k */
			template<class K> void get_sum_fv(const K& k, NVType* res) const;
			/** \brief get the generalized rank for a given node, i.e. the sum of NVFunv for all nodes before it in order */
			void get_sum_fv_node(NodeHandle x, NVType* res) const;
			/** \brief get the normalization factor, i.e. the sum of all keys */
			void get_norm_fv(NVType* res) const;
			
			/** \brief check that the tree is valid
			 * 
			 * Checks that binary tree and red-black tree properties are OK,
			 * throws exception on error. Also
			 * checks that rank function values (partial sums) are consistent as well if epsilon >= 0
			 * (epsilon is the tolerance for rounding errors if NVType is not integral) */
			void check_tree(double epsilon = -1.0) const;
		protected:
			/// \brief recursive helper for \ref check_tree(double)
			void check_tree_r(double epsilon, NodeHandle x, size_t black_count, size_t& previous_black_count) const;
	};
	
	
	template<class NodeAllocator, class Compare, class NVFunc, bool multi> template<class K>
	auto orbtree_base<NodeAllocator,Compare,NVFunc,multi>::find(const K& key) const -> NodeHandle {
		if(root() == Invalid) return nil();
		NodeHandle n = get_node(root()).get_right();
		if(n == Invalid || n == nil()) return nil();
		while(true) {
			const KeyType& k1 = get_node_key(n);
			if(c(key,k1)) {
				/* key < k1 */
				n = get_node(n).get_left();
			}
			else {
				if(c(k1,key)) n = get_node(n).get_right(); /* key > k1 */
				else return n; /* key == k1 */
			}
			if(n == nil()) return n; /* end of search, not found */
		}
	}
	
	template<class NodeAllocator, class Compare, class NVFunc, bool multi> template<class K>
	auto orbtree_base<NodeAllocator,Compare,NVFunc,multi>::lower_bound(const K& key) const -> NodeHandle {
		if(root() == Invalid) return nil();
		NodeHandle n = get_node(root()).get_right();
		if(n == Invalid || n == nil()) return nil();
		NodeHandle last = nil(); /* guess of the result node */
		while(true) {
			const KeyType& k1 = get_node_key(n);
			if(c(k1,key)) {
				/* key > k1 */
				n = get_node(n).get_right();
			}
			else { /* key <= k1, potential candidate */
				last = n;
				n = get_node(n).get_left();
			}
			if(n == nil()) return last; /* end of search */
		}
	}
	
	template<class NodeAllocator, class Compare, class NVFunc, bool multi> template<class K>
	auto orbtree_base<NodeAllocator,Compare,NVFunc,multi>::upper_bound(const K& key) const -> NodeHandle {
		if(root() == Invalid) return nil();
		NodeHandle n = get_node(root()).get_right();
		if(n == Invalid || n == nil()) return nil();
		NodeHandle last = nil(); /* guess of the result node */
		while(true) {
			const KeyType& k1 = get_node_key(n);
			if(c(key,k1)) {
				/* key < k1, potential candidate */
				last = n;
				n = get_node(n).left;
			}
			else {
				/* key >= k1, has to go right */
				n = get_node(n).right;
			}
			if(n == nil()) return last; /* end of search */
		}
	}

	template<class NodeAllocator, class Compare, class NVFunc, bool multi> template<class K>
	void orbtree_base<NodeAllocator,Compare,NVFunc,multi>::get_sum_fv(const K& k, NVType* res) const {
		for(unsigned int i=0; i < f.get_nr(); i++) res[i] = NVType();
		NVType tmp[f.get_nr()];
		if(root() == Invalid) return;
		//~ const Node& r = get_node(root());
		NodeHandle n = get_node(root()).get_right();
		if(n == Invalid || n == nil()) return;
		while(true) { /* recursive search starting from the root to find all nodes with key < k */
			const KeyType& k1 = get_node_key(n);
			if(c(k1,k)) {
				/* k1 < key, we have to add the sum from the left subtree + n and continue to the right */ 
				NodeHandle l = get_node(n).get_left();
				if(l != nil()) {
					this->get_node_sum(l,tmp);
					NVAdd(res,tmp);
				}
				get_node_grvalue(n,tmp);
				NVAdd(res,tmp);
				
				n = get_node(n).get_right();
				if(n == nil()) break;
			}
			else {
				/* k1 >= key, we have to continue toward the left */
				n = get_node(n).get_left();
				if(n == nil()) break;
			}
		}
	
	}
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	void orbtree_base<NodeAllocator,Compare,NVFunc,multi>::get_sum_fv_node(NodeHandle x, NVType* res) const {
		for(unsigned int i=0; i < f.get_nr(); i++) res[i] = NVType();
		NVType tmp[f.get_nr()];
		if(x == this->Invalid || x == nil() || x == root()) return;
		/* 1. add sum from x's left to the sum
		 * 2. go up one level
		 * 3. if x is right child, add parent's value + parent's left child's value
		 * 4. repeat until root is reached */
		NodeHandle l = get_node(x).get_left();
		if(l != nil()) {
			this->get_node_sum(l,tmp);
			NVAdd(res,tmp);
		}
		
		NodeHandle p = get_node(x).get_parent();
		
		while(p != root()) {
			if(x == get_node(p).get_right()) {
				l = get_node(p).get_left();
				if(l != nil()) {
					this->get_node_sum(l,tmp);
					NVAdd(res,tmp);
				}
				get_node_grvalue(p,tmp);
				NVAdd(res,tmp);
			}
			x = p;
			p = get_node(x).get_parent();
		}
	}
	
	
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	void orbtree_base<NodeAllocator,Compare,NVFunc,multi>::get_norm_fv(NVType* res) const {
		if(root() != Invalid) {
			NodeHandle n = get_node(root()).get_right();
			if(n != Invalid && n != nil()) {
				this->get_node_sum(n,res);
				return;
			}
		}
		for(unsigned int i=0; i < f.get_nr(); i++) res[i] = NVType(); /* return all zeroes for an empty tree */
	}
	
	
	/* first, last, next, previous */
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	auto orbtree_base<NodeAllocator,Compare,NVFunc,multi>::first() const -> NodeHandle {
		if(root() == Invalid) return nil();
		NodeHandle n = get_node(root()).get_right();
		if(n == Invalid || n == nil()) return nil();
		/* keep going left until it's possible */
		while(get_node(n).get_left() != nil()) n = get_node(n).get_left();
		return n;
	}
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	auto orbtree_base<NodeAllocator,Compare,NVFunc,multi>::last() const -> NodeHandle {
		if(root() == Invalid) return nil();
		NodeHandle n = get_node(root()).get_right();
		if(n == Invalid || n == nil()) return nil();
		/* keep going right until it's possible */
		while(get_node(n).get_right() != nil()) n = get_node(n).get_right();
		return n;
	}
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	auto orbtree_base<NodeAllocator,Compare,NVFunc,multi>::next(NodeHandle n) const -> NodeHandle {
		if(n == nil()) return n; /* incrementing nil is no-op */
		/* try going right */
		if(get_node(n).get_right() != nil()) {
			n = get_node(n).get_right();
			/* and then keep going left until possible */
			while(get_node(n).get_left() != nil()) n = get_node(n).get_left();
			return n;
		}
		/* if not possible to go right, go up until n is a right child */
		while(true) {
			NodeHandle p = get_node(n).get_parent();
			if(p == root()) return nil(); /* end of search, no next node fonud */
			if(get_node(p).get_left() == n) return p; /* found a suitable parent */
			n = p; /* keep going */
		}
	}
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	auto orbtree_base<NodeAllocator,Compare,NVFunc,multi>::previous(NodeHandle n) const -> NodeHandle {
		if(n == nil()) return last(); /* decrementing nil results in the last valid node -- this is done to make implementing the end() iterator easier */
		/* try going left */
		if(get_node(n).get_left() != nil()) {
			n = get_node(n).get_left();
			/* keep going right if possible */
			while(get_node(n).get_right() != nil()) n = get_node(n).get_right();
			return n;
		}
		/* if not possible to go left, go up until n is a left child */
		while(true) {
			NodeHandle p = get_node(n).get_parent();
			if(p == root()) return nil(); /* already at the beginning */
			if(get_node(p).get_right() == n) return p; /* found a suitable parent */
			n = p; /* otherwise keep going */
		}
	}
	
	
	/* rotations */
	//~ protected:
			/* update the sum only inside n */
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	void orbtree_base<NodeAllocator,Compare,NVFunc,multi>::update_sum(NodeHandle n) {
		/* sum(n) = sum(n.left) + sum(n.right) + f(n.key) */
		NVType sum[f.get_nr()];
		NVType tmp[f.get_nr()];
		get_node_grvalue(n,sum);
		if(get_node(n).get_left() != nil()) {
			this->get_node_sum(get_node(n).get_left(),tmp);
			NVAdd(sum,tmp);
		}
		if(get_node(n).get_right() != nil()) {
			this->get_node_sum(get_node(n).get_right(),tmp);
			NVAdd(sum,tmp);
		}
		this->set_node_sum(n,sum);
	}
	
	
	/* left rotate:
	 * right child of x takes its place, x becomes its left child
	 * left child of x's original right child becomes x's right child
	 */
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	void orbtree_base<NodeAllocator,Compare,NVFunc,multi>::rotate_left(NodeHandle x) {
		NodeHandle y = get_node(x).get_right(); /* we assume that x != nil and y != nil */
		get_node(x).set_right( get_node(y).get_left() ); /* x.right = y.left; -- this can be nil */
		if(get_node(x).get_right() != nil()) get_right(x).set_parent(x); /* here we have to check -- don't want to modify parent of nil */
		get_node(y).set_parent( get_node(x).get_parent() );
		
		if( x == get_parent(x).get_right() ) get_parent(x).set_right(y); /* note: parent of x can be root (sentinel) node (if x is the "real" root of the tree */
		else get_parent(x).set_left(y);
		get_node(y).set_left(x);
		get_node(x).set_parent(y);
		
		update_sum(x); /* only these two need to be updated -- upstream of y the values stay the same */
		update_sum(y);
	}
	
	/* right rotate.
	 * left child of x takes its place, x becomes its right child
	 * right child of x's original left child becomes x's left child */
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	void orbtree_base<NodeAllocator,Compare,NVFunc,multi>::rotate_right(NodeHandle x) {
		NodeHandle y = get_node(x).get_left(); /* we assume that x != nil and y != nil */
		get_node(x).set_left( get_node(y).get_right() );
		if(get_node(x).get_left() != nil()) get_left(x).set_parent(x);
		get_node(y).set_parent( get_node(x).get_parent() );
		
		if( x == get_parent(x).get_right() ) get_parent(x).set_right(y);
		else get_parent(x).set_left(y);
		get_node(y).set_right(x);
		get_node(x).set_parent(y);
		
		update_sum(x);
		update_sum(y);
	}
	
	/* helper: rotate by n's parent -- either left or right, depending on n's position
	 * caller must ensure that n and its parent are not nil or root */
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	void orbtree_base<NodeAllocator,Compare,NVFunc,multi>::rotate_parent(NodeHandle n) {
		if(n == get_parent(n).get_left()) rotate_right(get_node(n).get_parent());
		else rotate_left(get_node(n).get_parent());
	}
	
	
	/* helper for insert -- find the location where to insert
	 * note: in a multi map/set, the inserted element will come after any already
	 * existing elements with the same key */
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	bool orbtree_base<NodeAllocator,Compare,NVFunc,multi>::insert_search(const KeyType& k, NodeHandle& n, bool& insert_left) const {
		n = root();
		if(n == Invalid) throw std::runtime_error("orbtree_base::insert_search(): root is Invalid!\n");
		if(get_node(n).get_right() == nil()) {
			/* empty tree, insert as the dummy root node's right child */
			insert_left = false;
			return true;
		}
		n = get_node(n).get_right(); /* nonempty tree, start with the real root */
		while(true) {
			const KeyType& k1 = get_node_key(n);
			if(c(k,k1)) {
				/* should go to the left */
				if(get_node(n).get_left() == nil()) { insert_left = true; return true; }
				n = get_node(n).get_left();
			}
			else {
				/* should go to the right */
				/* first check if key exists (if not multimap / multiset) */
				if(!multi) if(!c(k1,k)) return false;
				if(get_node(n).get_right() == nil()) { insert_left = false; return true; }
				n = get_node(n).get_right();
			}
		}
	}
	
	
	/* helper function to do the real work for insert -- insert n1 as the left / right child of n
	 * n1 must already contain the proper key / value, but can be uninitialized otherwise
	 * note: this function does not check for the correct relationship between the keys of n and n1,
	 * that is the caller's responsibility */
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	void orbtree_base<NodeAllocator,Compare,NVFunc,multi>::insert_helper(NodeHandle n, NodeHandle n1, bool insert_left) {
		if(insert_left) get_node(n).set_left(n1);
		else get_node(n).set_right(n1);
		get_node(n1).set_parent(n);
		get_node(n1).set_left(nil());
		get_node(n1).set_right(nil());
		get_node(n1).set_red(); /* all new nodes are red */
		NVType sum_add[f.get_nr()];
		get_node_grvalue(n1,sum_add); /* calculate the new value */
		this->set_node_sum(n1,sum_add);
		/* update sum up the tree from n */
		for(NodeHandle n2 = n; n2 != root(); n2 = get_node(n2).get_parent()) {
			NVType tmp[f.get_nr()];
			this->get_node_sum(n2,tmp);
			NVAdd(tmp,sum_add);
			this->set_node_sum(n2,tmp);
		}
			
		while(true) {
			if(n == root()) return; /* nothing to do if we just added one node to an empty tree */
			/* here, n1 is always red */
			/* has to fix red-black tree properties */
			/* only possible problem is if n is red */
			if(get_node(n).is_black()) return;
			
			/* if n is red, it can have a black other child (only after the first step though) */
			
			/* if n is the real root, we can fix the tree by coloring it black */
			if(get_node(n).get_parent() == root()) { get_node(n).set_black(); return; }
			
			/* now, n has a valid, black parent and potentially a sibling */
			if(get_sibling(n).is_red()) {
				/* easier case, we can color n and its sibling black and n's parent red
				 * note: nil is always black, so this will only be true if n has a real sibling */
				get_sibling(n).set_black();
				get_node(n).set_black();
				get_parent(n).set_red();
				/* iteratoion has to continue to check if n's grandparent was red or black */
				n1 = get_node(n).get_parent();
				n = get_node(n1).get_parent();
			}
			else {
				/* n has either a black sibling or no sibling -- here, we have to do rotations,
				 * which depends on whether n and n1 are on the same side */
				if(is_left_side(n1) != is_left_side(n)) {
					/* different side, we have to rotate around n first */
					rotate_parent(n1);
					NodeHandle tmp = n1;
					n1 = n;
					n = tmp;
				}
				/* now n and n1 are on the same side, we have to rotate around n's parent */
				get_node(n).set_black();
				get_parent(n).set_red();
				rotate_parent(n);
				return; /* in this case, we are done, the "top" node (in n's parent's place) is now black */
			}
		}
	}
		
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	auto orbtree_base<NodeAllocator,Compare,NVFunc,multi>::insert(ValueType&& kv) -> std::pair<NodeHandle,bool> {
		bool insert_left;
		NodeHandle n = root();
		
		if(!insert_search(KeyValue::key(kv),n,insert_left)) return std::pair<NodeHandle,bool>(n,false);
		
		/* found a place to insert */
		NodeHandle n1 = this->new_node(std::move(kv));
		insert_helper(n,n1,insert_left);
		size1++;
		return std::pair<NodeHandle,bool>(n1,true);
	}
	
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	auto orbtree_base<NodeAllocator,Compare,NVFunc,multi>::insert(const ValueType& kv) -> std::pair<NodeHandle,bool> {
		bool insert_left;
		NodeHandle n = root();
		if(!insert_search(KeyValue::key(kv),n,insert_left)) return std::pair<NodeHandle,bool>(n,false);
		
		/* found a place to insert */
		NodeHandle n1 = this->new_node(kv);
		insert_helper(n,n1,insert_left);
		size1++;
		return std::pair<NodeHandle,bool>(n1,true);
	}
	
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	bool orbtree_base<NodeAllocator,Compare,NVFunc,multi>::insert_search_hint(NodeHandle hint, const KeyType& k, NodeHandle& n, bool& insert_left) const {
		if(c(k,get_node_key(hint))) {
			/* k should go before hint, it might be a valid hint 
			 * have to compare with previous value as well */
			NodeHandle p = previous(hint);
			if(!c(k,get_node_key(p))) {
				/* p <= k < hint, might be good */
				if(!multi) if(!c(get_node_key(p),k)) return p; /* p == k, cannot insert */
				/* insert between p and hint
				 * note: either p does not have a right child or hint does not have a left child */
				if(get_node(hint).get_left() == nil()) { n = hint; insert_left = true; return true; }
				else {
					if(get_node(p).get_right() == nil()) { n = p; insert_left = false; return true; }
					else throw std::runtime_error("orbtree_base::insert(): inconsistent tree detected!\n");
				}
			}
			/* here, hint is not good, but new element should go before it
			 * in this case, normal insert works in all cases (for multi,
			 * new element should be inserted after all elements with the same key) */
			return insert_search(k,n,insert_left).first;
		}
		/* here, either hint is not good, or k == hint */
		if(!c(get_node_key(hint),k)) {
			if(!multi) { n = hint; return false; } /* hint == k, cannot insert */
			if(get_node(hint).get_left() == nil()) { n = hint; insert_left = true; return true; }
			else {
				NodeHandle p = previous(hint);
				if(get_node(p).get_right() != nil()) std::runtime_error("orbtree_base::insert(): inconsistent tree detected!\n");
				n = p;
				insert_left = false;
				return true;
			}
		}
		/* new element should go after hint, should be the first if elements with the same key already exist */
		n = lower_bound(k);
		if(n != nil()) { /* insert before n */
			if(get_node(n).get_left() == nil()) { insert_left = true; return true; }
			else {
				NodeHandle p = previous(n);
				if(get_node(p).get_right() != nil()) std::runtime_error("orbtree_base::insert(): inconsistent tree detected!\n");
				n = p;
				insert_left = false;
				return true;
			}
		}
		else {
			n = last(); /* note: this is inefficient, as this case will require going through all levels twice (last() is O(logN) ) */
			insert_left = false;
			return true;
		}
	}
	
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	auto orbtree_base<NodeAllocator,Compare,NVFunc,multi>::insert(NodeHandle hint, const ValueType& kv) -> NodeHandle {
		/* insert with a hint -- hint must be a valid node! */
		NodeHandle n;
		bool insert_left;
		if(!insert_search_hint(hint,KeyValue::key(kv),n,insert_left)) return n; /* false means element with same key already exists in a non-multi tree */
		
		NodeHandle n1 = this->new_node(kv);
		insert_helper(n,n1,insert_left);
		size1++;
		return n1;
	}
	
	
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	auto orbtree_base<NodeAllocator,Compare,NVFunc,multi>::insert(NodeHandle hint, ValueType&& kv) -> NodeHandle {
		/* insert with a hint -- hint must be a valid node! */
		NodeHandle n;
		bool insert_left;
		if(!insert_search_hint(hint,KeyValue::key(kv),n,insert_left)) return n; /* false means element with same key already exists in a non-multi tree */
		
		NodeHandle n1 = this->new_node(std::move(kv));
		insert_helper(n,n1,insert_left);
		size1++;
		return n1;
	}
	
	
	template<class NodeAllocator, class Compare, class NVFunc, bool multi> template<class... T>
	auto orbtree_base<NodeAllocator,Compare,NVFunc,multi>::emplace(T&&... t) -> std::pair<NodeHandle,bool> {
		/* create a new node first, so constructor is only called once */
		NodeHandle n1 = new_node(std::forward(t...));
		NodeHandle n = root();
		bool insert_left;
		if(!insert_search(get_node_key(n1),n,insert_left)) {
			free_node(n1,n1);
			return std::pair<NodeHandle,bool>(n,false);
		}
		insert_helper(n,n1,insert_left);
		size1++;
		return std::pair<NodeHandle,bool>(n1,true);
	}
	
	template<class NodeAllocator, class Compare, class NVFunc, bool multi> template<class... T>
	auto orbtree_base<NodeAllocator,Compare,NVFunc,multi>::emplace_hint(NodeHandle hint, T&&... t) -> NodeHandle {
		/* create a new node first, so constructor is only called once */
		NodeHandle n1 = new_node(std::forward(t...));
		NodeHandle n;
		bool insert_left;
		if(!insert_search_hint(hint,get_node_key(n1),n,insert_left)) {
			/* false means element with same key already exists in a non-multi tree
			 * need to delete node and return its position */
			free_node(n1,n1);
			return n;
		}
		insert_helper(n,n1,insert_left);
		size1++;
		return n1;
	}
	
	
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	auto orbtree_base<NodeAllocator,Compare,NVFunc,multi>::erase(NodeHandle n) -> NodeHandle {
		/* delete node n, return a handle to its successor in the tree */
		NodeHandle x = next(n);
		/* if n has one child, we can delete n, replace it with one of its children 
		 * if n has two children, then x has one child (we go right from n and keep going left as long as it's possible)
		 * 	in this case, we can cut out x, and then move it in place of n */
		NodeHandle del = n;
		if(get_node(n).get_left() != nil() && get_node(n).get_right() != nil()) del = x;
		
		/* delete del, replace it with its only child */
		NodeHandle c = get_node(del).get_left();
		if(c == nil()) c = get_node(del).get_right();
		NodeHandle p = get_node(del).get_parent();
		get_node(c).set_parent( p ); /* c can be nil here, its parent will be set anyway */
		if(get_node(p).get_left() == del) get_node(p).set_left(c);
		else get_node(p).set_right(c);
		
		/* subtract the rank function values up from del */
		NVType x2[f.get_nr()];
		get_node_grvalue(del,x2);
		for(NodeHandle p2 = p; p2 != root(); p2 = get_node(p2).get_parent()) {
			NVType y[f.get_nr()];
			this->get_node_sum(p2,y);
			NVSubtract(y,x2);
			this->set_node_sum(p2,y);
		}
		
		/* cut out del, we need to fix the tree properties */
		/* cases: 
		 * 	1. del is black, c is red -> color c black
		 * 	2. del is red, c is black -> no need to do anything
		 *  3. del is black, c is nil -> one less black on del's path, need to fix it
		 */
		if(get_node(del).is_black()) {
			if(c != nil()) {
				get_node(c).set_black();
			}
			else {
				/* here, del is black -- in this case, it must have a valid sibling */
				while(true) {
					NodeHandle s = get_node(p).get_left();
					if(s == nil() || s == c) s = get_node(p).get_right();
					if(s == nil() || s == c) throw std::runtime_error("orbtree_base::erase(): found black node with no sibling!\n");
					
					if(get_node(s).is_red()) {
						/* p is black */
						/* recolor p as red, s as black, do a rotation on p and continue from there */
						get_node(p).set_red();
						get_node(s).set_black();
						rotate_parent(s);
						continue; /* continue with the same p, which is now red and its other child is black */
					}
					/* here, s is black, p can be black or red */
					if(get_left(s).is_black() && get_right(s).is_black()) {
						/* two black children (or nil) */
						/* we can color s red and fix things on upper level */
						get_node(s).set_red();
						/* if parent is red, we can fix the tree by coloring it black and s red */
						if(get_node(p).is_red()) {
							get_node(p).set_black();
							break;
						}
						/* if p was black, we have to continue one level up */
						c = p;
						p = get_node(p).get_parent();
						if(p == root()) break;
						continue;
					}
					/* here, s is black and at least one child of it is red
					 * if it is on the other side as s, then we need to rotate s,
					 * so that the red child is on the same side */
					if(s == get_node(p).get_right() && get_right(s).is_black()) {
						/* s is right child, but left child of s is red, rotate right */
						get_node(s).set_red();
						get_left(s).set_black();
						rotate_right(s);
						continue; /* re-assign s at the top of the loop -- p did not change */
					}
					/* previous condition reversed */
					if(s == get_node(p).get_left() && get_left(s).is_black()) {
						/* s is left child, but right child of s is red, rotate left */
						get_node(s).set_red();
						get_right(s).set_black();
						rotate_left(s);
						continue;
					}
					
					/* here s is black and has a red child on the same side as s
					 * in this case, the tree can be fixed by rotating s into p's position */
					if(get_node(p).is_red()) get_node(s).set_red();
					get_node(p).set_black();
					if(s == get_node(p).get_right()) get_right(s).set_black();
					else get_left(s).set_black();
					rotate_parent(s);
					break;
				}
				
				
			} /* else (c is nil) */
		} /* if del is black -- need to fix the tree */
		
		if(del != n) {
			/* x (n's successor) was deleted, have to put x in n's place
			 * in this case, x cannot be nil */
			get_node(x).set_left( get_node(n).get_left() );
			get_node(x).set_right( get_node(n).get_right() );
			get_node(x).set_parent( get_node(n).get_parent() );
			if(get_node(n).is_black()) get_node(x).set_black();
			else get_node(x).set_red();
			if(get_parent(n).get_left() == n) get_parent(n).set_left(x);
			else {
				if(get_parent(n).get_right() == n) get_parent(n).set_right(x);
				else throw std::runtime_error("orbtree_base::erase(): inconsistent tree detected!\n");
			}
			if(get_node(x).get_left() != nil()) get_left(x).set_parent(x);
			if(get_node(x).get_right() != nil()) get_right(x).set_parent(x);
			
			/* fix sums */
			update_sum_r(x);
		}
		/* delete node of n -- need to give x as well as its value can potentially change */
		this->free_node(n);
		if(!size1) throw std::runtime_error("size1 is zero in orbtree_base::erase!\n");
		size1--;
		return x; /* return the successor -- it can be nil if n was the largest node */
	}
	
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	void orbtree_base<NodeAllocator,Compare,NVFunc,multi>::NVAdd(NVType* x, const NVType* y) const {
		unsigned int nr = f.get_nr();
		
		if(std::is_integral<NVType>::value) {
			/* for integer types -- check for overflow */
			NVType max = std::numeric_limits<NVType>::max();
			NVType min = std::numeric_limits<NVType>::min();
			for(unsigned int i = 0;i<nr;i++) {
				if(y[i] > 0) {
					if(max - y[i] < x[i]) throw std::runtime_error("orbtree_base::NVAdd(): overflow!\n");
				}
				else {
					if(min - y[i] > x[i]) throw std::runtime_error("orbtree_base::NVAdd(): underflow!\n");
				}
				x[i] += y[i];
			}
		}
		else {
			/* TODO: overflow / rounding check for floats? */
			for(unsigned int i=0;i<nr;i++) x[i] += y[i];
		}
	}	
	
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	void orbtree_base<NodeAllocator,Compare,NVFunc,multi>::NVSubtract(NVType* x, const NVType* y) const {
		unsigned int nr = f.get_nr();
		
		if(std::is_integral<NVType>::value) {
			/* for integer types -- check for overflow */
			NVType max = std::numeric_limits<NVType>::max();
			NVType min = std::numeric_limits<NVType>::min();
			for(unsigned int i = 0;i<nr;i++) {
				if(y[i] > 0) {
					if(min + y[i] > x[i]) throw std::runtime_error("orbtree_base::NVSubtract(): underflow!\n");
				}
				else {
					if(max + y[i] < x[i]) throw std::runtime_error("orbtree_base::NVSubtract(): overflow!\n");
				}
				x[i] -= y[i];
			}
		}
		else {
			/* TODO: overflow / rounding check for floats? */
			for(unsigned int i=0;i<nr;i++) x[i] -= y[i];
		}
	}
	
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	void orbtree_base<NodeAllocator,Compare,NVFunc,multi>::check_tree(double epsilon) const {
		//~ if(size1 + 2 != this->size) throw std::runtime_error("orbtree_base::check_tree(): inconsistent tree size!\n");
		const Node& r = get_node(root());
		if(r.get_left() != nil() || r.get_parent() != nil()) throw std::runtime_error("orbtree_base::check_tree(): root is invalid!\n");
		NodeHandle x = r.get_right();
		if(x == nil()) return; /* empty tree nothing more to do */
		if(x == this->Invalid) throw std::runtime_error("orbtree_base::check_tree(): invalid node handle found!\n");
		if(get_node(x).get_parent() != root()) throw std::runtime_error("orbtree_base::check_tree(): inconsistent root node!\n");
		
		size_t previous_black_count = (size_t)-1;
		check_tree_r(epsilon,x,0,previous_black_count);
		
	}
	
	/* recursive helper function to check tree 
	 * this function checks:
	 * 	-- if node x has any children, then they are consistent (i.e. have x as parent, keys are ordered properly)
	 *  -- for red x, both children have to be black or nil
	 *  -- if nil is reached, black_count has to be the same as previous_black_count
	 *  -- if epsilon >= 0.0, then than the rank function value stored in x is equal to the sum of its children + x's value
	 *  -- recurses into both children, increasing black_count if x is black
	 * as tree height for N elements is maximum 2*log_2(N), this will not result in stack overflow
	 * 	(e.g. N <~ 2^40 on a machine with few hundred GB RAM, thus tree height <~ 80, which is reasonable for recursion depth)
	 */
	template<class NodeAllocator, class Compare, class NVFunc, bool multi>
	void orbtree_base<NodeAllocator,Compare,NVFunc,multi>::check_tree_r(double epsilon, NodeHandle x, size_t black_count, size_t& previous_black_count) const {
		NodeHandle l = get_node(x).get_left();
		NodeHandle r = get_node(x).get_right();
		
		{ /* scope for sum and tmp -- no need to keep them over the recursion */
			NVType sum[f.get_nr()];
			NVType tmp[f.get_nr()];
			
			if(epsilon >= 0.0) get_node_grvalue(x,sum);
			
			if(l != nil()) {
				/* check if x is l's parent */
				if(get_node(l).get_parent() != x) throw std::runtime_error("orbtree_base::check_tree(): inconsistent node!\n");
				/* check that red node does not have red child */
				if(get_node(x).is_red() && get_node(l).is_red()) throw std::runtime_error("orbtree_base::check_tree(): inconsistent node (red parent with red child)!\n");
				/* check that l's key is strictly smaller than x's */
				if( ! c(get_node_key(l),get_node_key(x)) ) {
					if( !multi || c(get_node_key(x),get_node_key(l)) ) throw std::runtime_error("orbtree_base::check_tree(): inconsistent ordering!\n");
				}
				/* add l's partial sum to x's value */
				if(epsilon >= 0.0) {
					this->get_node_sum(l,tmp);
					NVAdd(sum,tmp);
				}
			}
			
			if(r != nil()) {
				/* check if x is r's parent */
				if(get_node(r).get_parent() != x) throw std::runtime_error("orbtree_base::check_tree(): inconsistent node!\n");
				/* check that red node does not have red child */
				if(get_node(x).is_red() && get_node(r).is_red()) throw std::runtime_error("orbtree_base::check_tree(): inconsistent node (red parent with red child)!\n");
				/* check that r's key is larger than x's or equal to it (if multiset / multimap) */
				if( c(get_node_key(r),get_node_key(x)) ) throw std::runtime_error("orbtree_base::check_tree(): inconsistent ordering!\n");
				if( !multi && ! c(get_node_key(x),get_node_key(r)) ) throw std::runtime_error("orbtree_base::check_tree(): non-unique key found!\n");
				
				/* add r's partial sum to x's value */
				if(epsilon >= 0.0) {
					this->get_node_sum(r,tmp);
					NVAdd(sum,tmp);
				}
			}
			
			if(epsilon >= 0.0) {
				/* check that the partial sum stored in x is consistent */
				this->get_node_sum(x,tmp);
				
				/* if NVType is integral, we want exact match -- otherwise, we use epsilon for comparison */
				if(std::is_integral<NVType>::value) { for(unsigned int i=0;i<f.get_nr();i++) if(tmp[i] != sum[i])
						throw std::runtime_error("orbtree_base::check_tree(): partial sums are inconsistent!\n"); }
				else for(unsigned int i=0;i<f.get_nr();i++) if(fabs(tmp[i]-sum[i]) > epsilon)
					throw std::runtime_error("orbtree_base::check_tree(): partial sums are inconsistent!\n");
			}
		}
		
		/* increase black count for recursion */
		if(get_node(x).is_black()) black_count++;
		if(r == nil() || l == nil()) {
			/* l or r is nil, check if black_count is same as previously */
			if(previous_black_count == (size_t)-1) previous_black_count = black_count;
			else if(previous_black_count != black_count) throw std::runtime_error("orbtree_base::check_tree(): inconsistent node (black conut differs)!\n");
		}
		
		/* recurse into both children (if not nil) */
		if(l != nil()) check_tree_r(epsilon,l,black_count,previous_black_count);
		if(r != nil()) check_tree_r(epsilon,r,black_count,previous_black_count);		
	}
	

} // namespace orbtree

#endif

