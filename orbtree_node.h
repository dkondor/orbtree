/*  -*- C++ -*-
 * orbtree_node.h -- generalized order statistic red-black tree implementation
 * 	tree nodes and node allocators
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

#ifndef ORBTREE_NODE_H
#define ORBTREE_NODE_H

#include <functional>
#include <stdexcept>
#include <type_traits>
#include <limits>
#include <utility>
#include <algorithm>


//~ #ifdef USE_STACKED_VECTOR
#include "vector_stacked.h"
//~ template <class T> using compact_vector = stacked_vector::vector<T>;
//~ #else
#include "vector_realloc.h"
//~ template <class T> using compact_vector = realloc_vector::vector<T>;
//~ #endif

/* constexpr if support only for c++17 or newer */
#if __cplusplus >= 201703L
#define CONSTEXPR constexpr
#else
#define CONSTEXPR
#endif


namespace orbtree {
	
	/** \brief trivially copyable pair, similar to std::pair */
	template<class T1, class T2>
	struct trivial_pair {
		T1 first;
		T2 second;
		trivial_pair() = default;
		trivial_pair(const T1& x, const T2& y):first(x),second(y) { }
		trivial_pair(const T1& x, T2&& y):first(x),second(std::move(y)) { }
		trivial_pair(T1&& x, const T2& y):first(std::move(x)),second(y) { }
		trivial_pair(T1&& x, T2&& y):first(std::move(x)),second(std::move(y)) { }
		trivial_pair(const std::pair<T1,T2>& p):first(p.first),second(p.second) { }
		operator std::pair<T1,T2>() const { return std::pair<T1,T2>(first,second); }
		operator trivial_pair<const T1,T2>() const { return trivial_pair<const T1,T2>(first,second); }
	};
	
	/* key and optionally value classes */
	/// \brief wrapper class for key that can go in sets and multisets
	template<class Key> struct KeyOnly {
		protected:
			Key k; /**< \brief actual key; should not change during a node's lifetime -- not const to allow swapping / moving nodes */
		public:
			typedef Key KeyType;
			typedef const Key ValueType;
			
			explicit KeyOnly(const Key& k_):k(k_) { }
			explicit KeyOnly(Key&& k_):k(std::move(k_)) { }
			template<class... T> explicit KeyOnly(T&&... args) : k(std::forward<T...>(args...)) { }
			KeyOnly():k() { }
			/// \brief access key value -- only const access is possible, key should not change
			const Key& key() const { return k; }
			
			ValueType& keyvalue() const { return k; } /**<  \brief universal access to key / key-value pair */
			
			/** \brief helper to extract the key from ValueType -- contrast it with KeyValue */
			static const Key& key(const ValueType& k) { return k; }
			
			static const bool keyonly = true; /**< flag to signal tree if this is key-value pair or just key -- used when defining iterators */
			
			void swap(KeyOnly& k_) {
				using std::swap;
				swap(k,k_.k);
			}
	};
	/// \brief wrapper class for a key-value pair
	template<class Key, class Value, template<class,class> class pair_type = trivial_pair> struct KeyValue {
		protected:
			pair_type<Key, Value> p; /**< \brief key and value, key should not change -- not const to allow swapping / moving nodes */
		public:
			typedef Key KeyType;
			typedef Value MappedType;
			/* note: key is not const to make things simpler -- all accessor functions return const ValueType */
			typedef pair_type<Key, Value> ValueType;
			
			explicit KeyValue(const std::pair<Key,Value>& kv):p(kv) { }
			explicit KeyValue(pair_type<Key,Value>&& kv):p(std::move(kv)) { }
			explicit KeyValue(const pair_type<Key,Value>& kv):p(kv) { }
			KeyValue():p() { }
			/// \brief access key -- only const access is possible, key should not change
			const Key& key() const { return p.first; }
			/// \brief access value
			Value& value() { return p.second; }
			/// \brief access value
			const Value& value() const { return p.second; }
			
			ValueType& keyvalue() { return p; } /**< \brief  universal access to key / key-value pair */
			const ValueType& keyvalue() const { return p; } /**< \brief  universal access to key / key-value pair */
			
			/** \brief helper to extract only the key from ValueType */
			static const Key& key(const ValueType& kv) { return kv.first; }
			
			static const bool keyonly = false; /**< \brief flag to signal tree if this is key-value pair or just key -- used when defining iterators */
			
			void swap(KeyValue& k_) {
				using std::swap;
				swap(p,k_.p);
			}
	};
	
	template<class Key>
	void swap(KeyOnly<Key>& x, KeyOnly<Key>& y) {
		x.swap(y);
	}
	template<class Key,class Value>
	void swap(KeyValue<Key,Value>& x, KeyValue<Key,Value>& y) {
		x.swap(y);
	}
	 
	
	
	
	/** \brief Basic allocator that allocates each node individually, using C++ new / delete and pointers.
	 * 	Can throw exception if out of memory.
	 * 
	 * @tparam KeyValueT Type of stored data, should be either KeyOnly or KeyValue
	 * @tparam NVTypeT Type of extra data stored along in nodes (i.e. the return value of the function whose sum can be calculated).
	 */
	template<class KeyValueT, class NVTypeT, bool simple = false>
	class NodeAllocatorPtr {
		protected: /* everything is protected, red-black tree class inherits from this */
			
			typedef KeyValueT KeyValue;
			typedef NVTypeT NVType;
			
			/** \brief node class
			 * 
			 * All properties are protected and should be accessed via getters / setters.
			 * This way, the tree class doesn't depend on implementation details of nodes. */
			struct Node {
				protected:
					KeyValue kv; /**< \brief key and (optionally) value stored */
					/// \brief partial sum (sum of this node's children's weight + this node's weight)
					typename std::conditional<simple, NVType, NVType*>::type partialsum;
					Node* parent;
					Node* left;
					Node* right;
					bool red;
				
				public:
					/* node functionality */
					Node(const KeyValue& kv_) : kv(kv_), partialsum(0) { }
					Node(KeyValue&& kv_) : kv(std::move(kv_)), partialsum(0) { }
					Node():kv(), partialsum(0) { }
					template<class... T> Node(T&&... args) : kv(std::forward<T...>(args...)), partialsum(0) { }
					/* note: destructor only deletes the partial sum, does not recursively delete the children */
					template<bool simple_ = simple> void do_delete(typename std::enable_if<simple_,void*>::type = 0) { }
					template<bool simple_ = simple> void do_delete(typename std::enable_if<!simple_,void*>::type = 0) {
						if(partialsum) delete[]partialsum;
					}
					~Node() { do_delete(); }
					
					KeyValue& get_key_value() { return kv; }
					const KeyValue& get_key_value() const { return kv; }
					
					bool is_red() const { return red; } ///< \brief test if this node is red
					bool is_black() const { return !red; } ///< \brief test if this node is black
					void set_red() { red = true; } ///< \brief set this node red
					void set_black() { red = false; } ///< \brief set this node black
					
					/* get / set for parent, left and right -- node is considered opaque
					 * by the tree to allow different optimizations here */
					const Node* get_parent() const { return parent; } ///< \brief get handle for parent
					const Node* get_left() const { return left; } ///< \brief get handle for left child
					const Node* get_right() const { return right; } ///< \brief get handle for right child
					void set_parent(const Node* p) { parent = const_cast<Node*>(p); } ///< \brief set handle for parent
					void set_left(const Node* x) { left = const_cast<Node*>(x); } ///< \brief set handle for left child
					void set_right(const Node* x) { right = const_cast<Node*>(x); } ///< \brief set handle for right child
				
					friend class NodeAllocatorPtr<KeyValueT,NVTypeT,simple>;
			};
			
			/** \brief Node handle type to be used by tree implementation.
			 * 
			 * Note: NodeHandle is always const to make it easier to
			 * return it from const nodes; whether a node can be
			 * modified depends if this class is const. */
			typedef const Node* NodeHandle;
			
			/// \brief Root sentinel, allocated automatically, used when freeing the tree.
			Node* root;
			Node* nil; ///< \brief Sentinel for nil.
			
			/** \brief each node has nv_per_node values calculated and stored in it */
			const unsigned int nv_per_node;
			unsigned int get_nv_per_node() const { return nv_per_node; }
			
			NodeAllocatorPtr():root(0),nil(0),nv_per_node(1) {
				root = new_node();
				nil = new_node();
			}
			explicit NodeAllocatorPtr(unsigned int nv_per_node_):root(0),nil(0),nv_per_node(nv_per_node_) {
				if CONSTEXPR (simple) if(nv_per_node != 1)
					throw std::runtime_error("For simple tree, weight function can only return one component!\n");
				root = new_node();
				nil = new_node();
			}
			~NodeAllocatorPtr() { 
				if(root) free_tree_nodes_r(root);
				if(nil) delete nil;
			}
			
			/** \brief get reference to a modifiable node */
			Node& get_node(NodeHandle x) { return *const_cast<Node*>(x); }
			/** \brief get reference to a const node */
			const Node& get_node(const NodeHandle x) const { return *x; }
			
			/** \brief invalid handle */
			constexpr static Node* Invalid = 0; /* null pointer */
			/** \brief maximum number of nodes -- unlimited */
			const static size_t max_nodes = 0; /* unlimited */
			/** \brief get new node 
			 * 
			 * It is the callers responsibility to store
			 * the handle in the tree or otherwise remember and free it later.
			 * Will throw an exception if allocation failed. */
			Node* new_node() { Node* n = new Node(); init_node(n); return n; }
			/// \copydoc new_node()
			Node* new_node(const KeyValue& kv) { Node* n = new Node(kv); init_node(n); return n; }
			/// \copydoc new_node()
			Node* new_node(KeyValue&& kv) { Node* n = new Node(std::move(kv)); init_node(n); return n; }
			/// \copydoc new_node()
			template<class... T>
			Node* new_node(T&&... kv) { Node* n = new Node(std::forward<T>(kv)...); init_node(n); return n; }
			
			/** \brief delete node -- tree is not traversed, the caller must arrange to "cut" out
			 * the node in question (otherwise the referenced nodes will be lost) */
			void free_node(NodeHandle n) { delete (Node*)n; }
			/** \brief clear tree, i.e. free all nodes, but keep root (sentinel) and nil, so that tree can be used again */
			void clear_tree() { 
				if(root) {
					Node* n = root;
					if(n->left && n->left != nil) free_tree_nodes_r((Node*)(n->left));
					if(n->right && n->right != nil) free_tree_nodes_r((Node*)(n->right));
					n->left = 0;
					n->right = 0;
				}
			}
			
			/** \brief free whole tree (starting from root) */
			void free_tree_nodes_r(Node* n) {
				if(n->left && n->left != nil) free_tree_nodes_r((Node*)(n->left));
				if(n->right && n->right != nil) free_tree_nodes_r((Node*)(n->right));
				delete n;
			}
			
			/** \brief safely initialize new node, throw an exception if memory allocation failed */
			template <bool simple_ = simple>
			void init_node(typename std::enable_if<simple_, Node*>::type n) {
				if(n) {
					n->parent = Invalid;
					n->left = Invalid;
					n->right = Invalid;
				}
				if(!n) throw std::runtime_error("NodeAllocatorPtr::init_node(): out of memory!\n");
			}
			/** \brief safely initialize new node, throw an exception if memory allocation failed */
			template <bool simple_ = simple>
			void init_node(typename std::enable_if<!simple_, Node*>::type n) {
				if(n) {
					n->parent = Invalid;
					n->left = Invalid;
					n->right = Invalid;
					n->partialsum = new NVType[nv_per_node];
				}
				if( !n || ! (n->partialsum) ) throw std::runtime_error("NodeAllocatorPtr::init_node(): out of memory!\n");
			}
			
			/// \brief get the value of the partial sum of weights stored in this node -- simple case when the weight function returns only one value 
			template<bool simple_ = simple>
			void get_node_sum(NodeHandle n, typename std::enable_if<simple_,NVType*>::type s) const {
				*s = n->partialsum;
			}
			/// \brief get the value of the partial sum of weights stored in this node -- case when the weight function returns a vector
			template<bool simple_ = simple>
			void get_node_sum(NodeHandle n, typename std::enable_if<!simple_,NVType*>::type s) const {
				for(unsigned int i = 0; i < nv_per_node; i++) s[i] = n->partialsum[i];
			}
			/// \brief set the value of the partial sum stored in this node -- simple case when the weight function returns only one value 
			template<bool simple_ = simple>
			void set_node_sum(NodeHandle n1, typename std::enable_if<simple_,const NVType*>::type s) { 
				Node* n = const_cast<Node*>(n1);
				n->partialsum = *s;
			}
			/// \brief set the value of the partial sum stored in this node -- case when the weight function returns a vector
			template<bool simple_ = simple>
			void set_node_sum(NodeHandle n1, typename std::enable_if<!simple_,const NVType*>::type s) { 
				Node* n = const_cast<Node*>(n1);
				for(unsigned int i = 0; i < nv_per_node; i++) n->partialsum[i] = s[i];
			}
	};
	
	
	/** \brief Alternate node allocator with the aim to use less memory. 
	 * 
	 * Special node allocator that stores values in separate array + stores red/black flag in parent pointer.
	 * Note that memory is not freed automatically when nodes are deleted, use the \ref shrink_size() function
	 * for that purpose.
	 * 
	 * Note: this uses the custom vector implementation either \ref realloc_vector::vector
	 * if key and value are trivially copyable or \ref stacked_vector::vector if not. The
	 * latter version will be significantly slower.
	 * 
	 * @tparam KeyValueT Type of stored data, should be either KeyOnly or KeyValue
	 * @tparam NVTypeT Type of extra data stored along in nodes (i.e. the return value of the function whose sum can be calculated).

	 * Note: the actual requirement for \ref realloc_vector::vector
	 * would be "trivially moveable" (meaning any object that can be moved to a new
	 * memory location without problems, but can still have nontrivial destructor), but
	 * as far as I know, this concept does not exist in C++.
	 */
	template<class KeyValueT, class NVTypeT, class IndexType>
	class NodeAllocatorCompact {
		protected:
			//~ static_assert(std::is_trivially_copyable<KeyValueT>::value,
				//~ "Key and Value must be trivially copyable to use compact flat allocator!\n");
			typedef IndexType NodeHandle;
			typedef KeyValueT KeyValue;
			typedef NVTypeT NVType;
		
		private:
			static constexpr IndexType redbit = 1U << (std::numeric_limits<IndexType>::digits - 1);
			static constexpr IndexType max_nodes = redbit - 1;
			static constexpr IndexType deleted_indicator = (max_nodes | redbit); /* 0xFFFFFFFFh */
			
		protected:
			/// \brief Node object
			struct Node {
				protected:
					/// \brief Key and (optionally) value stored in this node
					KeyValue kv;
					/// \brief Parent node reference, actually an index in a vector; also stores red-black flag.
					IndexType parent;
					IndexType left; ///< \brief Left child.
					IndexType right; ///< \brief Right child.
					/* red-black flag is stored inside parent */
					
				public:
					Node(const KeyValue& kv_) : kv(kv_), parent(NodeAllocatorCompact::Invalid),
						left(NodeAllocatorCompact::Invalid), right(NodeAllocatorCompact::Invalid) { }
					Node(KeyValue&& kv_) : kv(kv_), parent(NodeAllocatorCompact::Invalid),
						left(NodeAllocatorCompact::Invalid), right(NodeAllocatorCompact::Invalid) { }
					template<class... T> Node(T&&... args) : kv(std::forward<T...>(args...)), parent(NodeAllocatorCompact::Invalid),
						left(NodeAllocatorCompact::Invalid), right(NodeAllocatorCompact::Invalid) { }
					Node():kv(), parent(NodeAllocatorCompact::Invalid),
						left(NodeAllocatorCompact::Invalid), right(NodeAllocatorCompact::Invalid) { }
										
					KeyValue& get_key_value() { return kv; }
					const KeyValue& get_key_value() const { return kv; }
					bool is_red() const { return parent & NodeAllocatorCompact::redbit; }
					bool is_black() const { return !is_red(); }
					void set_red() { parent |= NodeAllocatorCompact::redbit; }
					void set_black() { parent &= (~NodeAllocatorCompact::redbit); }
					
					NodeHandle get_parent() const { return parent & (~NodeAllocatorCompact::redbit); }
					NodeHandle get_left() const { return left; }
					NodeHandle get_right() const { return right; }
					void set_parent(NodeHandle p) {
						if(p > NodeAllocatorCompact::max_nodes) throw std::runtime_error("NodeAllocatorCompact::Node::set_parent(): parent ID too large!\n");
						parent = p | (parent & NodeAllocatorCompact::redbit);
					}
					void set_left(NodeHandle x) { left = x; }
					void set_right(NodeHandle x) { right = x; }
					
					/// \brief Set a flag indicating this node has been deleted (but memory has not been freed yet).
					void set_deleted() { parent = NodeAllocatorCompact::deleted_indicator; }
					/// \brief Check if this node is deleted.
					bool is_deleted() const { return (parent == NodeAllocatorCompact::deleted_indicator); }
					
					/* swap contents with other node */
					void swap(Node& n) {
						using std::swap;
						swap(kv,n.kv);
						swap(parent,n.parent);
						swap(left,n.left);
						swap(right,n.right);
					}
					
					friend class NodeAllocatorCompact<KeyValueT,NVTypeT,IndexType>;
			};
			
		private:

#ifdef USE_STACKED_VECTOR
			typedef stacked_vector::vector<Node> node_vector_type;
#else			
			typedef typename std::conditional< std::is_trivially_copyable<KeyValueT>::value,
				realloc_vector::vector<Node>, stacked_vector::vector<Node> >::type node_vector_type;
#endif
			
			realloc_vector::vector<NVType> nvarray; ///< \brief Vector storing the partial sum of function values in nodes.
			node_vector_type nodes; ///< \brief Vector storing the node objects.
			const unsigned int nv_per_node; ///< \brief Number of weight values per node (number of components returned by the weight function).
			size_t n_del; ///< \brief Number of deleted nodes (memory not freed yet, these are stored in-place, forming a linked list).
			NodeHandle deleted_nodes_head; ///< \brief Head of linked list for deleted nodes.

		protected:
			/** \brief invalid handle */
			static constexpr NodeHandle Invalid = max_nodes;
			NodeHandle root; /** \brief Root sentinel. */
			NodeHandle nil; /** \brief Nil sentinel */
			
			NodeAllocatorCompact():nv_per_node(1),n_del(0),deleted_nodes_head(Invalid),root(Invalid),nil(Invalid) { 
				root = new_node();
				nil = new_node();
			}
			explicit NodeAllocatorCompact(unsigned int nv_per_node_):nv_per_node(nv_per_node_),n_del(0),
					deleted_nodes_head(Invalid),root(Invalid),nil(Invalid) {
				root = new_node();
				nil = new_node();
			}
			//~ ~NodeAllocatorCompact() { }
			/* main interface */
			/** \brief get reference to a modifiable node */
			Node& get_node(NodeHandle x) { return nodes[x]; }
			/** \brief get reference to a const node */
			const Node& get_node(NodeHandle x) const { return nodes[x]; }
			
			
		private:
			/** \brief try to get a node from the deleted pool; return true if such a node exists, false otherwise */
			bool try_get_node(NodeHandle& n) {
				if(deleted_nodes_head != Invalid) {
					if(!n_del) throw std::runtime_error("NodeAllocatorCompact::alloc_node(): inconsistent deleted nodes!\n");
					n_del--;
					n = deleted_nodes_head;
					nodes[n].set_parent(Invalid);
					deleted_nodes_head = nodes[deleted_nodes_head].get_left();
					if(deleted_nodes_head != Invalid) nodes[deleted_nodes_head].set_right(Invalid);
					return true;
				}
				else return false;
			}
			
			/** \brief Move the partial sum of a node to a new location */
			void move_nv(IndexType x, IndexType y) {
				size_t xbase = ((size_t)x)*nv_per_node;
				size_t ybase = ((size_t)y)*nv_per_node;
				for(unsigned int i=0;i<nv_per_node;i++) nvarray[xbase + i] = nvarray[ybase + i];
			}
			
			/** \brief Move node from position y to x, updating parent and child relationships.
			 * 
			 * y must be a valid node, and x must have been cut out of the tree before (or be a "deleted" node)
			 * 
			 * after the call, y is an invalid (duplicate) node */
			void move_node(NodeHandle x, NodeHandle y) {
				nodes[x] = std::move(nodes[y]); /* no need to swap (i.e. preserve values in x) */
				move_nv(x,y); /* move rank function as well */
				/* note: destructor is not called for y (memory is not freed) */
				if(y == root || y == nil) throw std::runtime_error("NodeAllocatorCompact::move_node(): root or nil is moved!\n");
				/* update parent and children if needed */
				if(nodes[x].get_parent() != Invalid && nodes[x].get_parent() != nil) {
					if(nodes[nodes[x].get_parent()].get_left() == y) nodes[nodes[x].get_parent()].set_left(x);
					else {
						if(nodes[nodes[x].get_parent()].get_right() == y) nodes[nodes[x].get_parent()].set_right(x);
						else throw std::runtime_error("NodeAllocatorCompact::move_node(): inconsistent tree detected!\n");
					}
				}
				else throw std::runtime_error("NodeAllocatorCompact::move_node(): node with no parent!\n");
				if(nodes[x].get_left() != Invalid && nodes[x].get_left() != nil) {
					if(nodes[nodes[x].get_left()].get_parent() != y)
						throw std::runtime_error("NodeAllocatorCompact::move_node(): inconsistent tree detected!\n");
					nodes[nodes[x].get_left()].set_parent(x);
				}
				if(nodes[x].get_right() != Invalid && nodes[x].get_right() != nil) {
					if(nodes[nodes[x].get_right()].get_parent() != y)
						throw std::runtime_error("NodeAllocatorCompact::move_node(): inconsistent tree detected!\n");
					nodes[nodes[x].get_right()].set_parent(x);
				}
			}
			
			/** \brief shrink memory used to current size */
			void shrink_memory(IndexType new_capacity = 0) {
				nodes.shrink_to_fit(new_capacity);
				nvarray.shrink_to_fit(((size_t)new_capacity)*nv_per_node);
			}
			
		protected:
			/** \brief get new node -- note that it is the callers responsibility to store the handle in the tree */
			NodeHandle new_node() {
				NodeHandle n = nodes.size();
				/* use previously deleted node, if available */
				if(try_get_node(n)) nodes[n] = Node();
				else {
					/* create new node */
					if(n == max_nodes) throw std::runtime_error("NodeAllocatorFlat::new_node(): reached maximum number of nodes!\n");
					nodes.emplace_back();
					nvarray.resize(((size_t)(n+1))*nv_per_node,NVType());
				}
				return n;
			}
			/** \brief get new node -- note that it is the callers responsibility to store the handle in the tree */
			NodeHandle new_node(const KeyValue& kv) {
				NodeHandle n = nodes.size();
				if(try_get_node(n)) nodes[n] = Node(kv);
				else {
					if(n == max_nodes) throw std::runtime_error("NodeAllocatorFlat::new_node(): reached maximum number of nodes!\n");
					nodes.emplace_back(kv);
					nvarray.resize(((size_t)(n+1))*nv_per_node,NVType());
				}
				return n;
			}
			/** \brief get new node -- note that it is the callers responsibility to store the handle in the tree */
			NodeHandle new_node(KeyValue&& kv) {
				NodeHandle n = nodes.size();
				if(try_get_node(n)) nodes[n] = Node(std::forward<KeyValue>(kv));
				else {
					if(n == max_nodes) throw std::runtime_error("NodeAllocatorFlat::new_node(): reached maximum number of nodes!\n");
					nodes.emplace_back(std::forward<KeyValue>(kv));
					nvarray.resize(((size_t)(n+1))*nv_per_node,NVType());
				}
				return n;
			}
			/** \brief get new node -- note that it is the callers responsibility to store the handle in the tree */		
			template<class... T> NodeHandle new_node(T&&... kv) {
				NodeHandle n = nodes.size();
				if(try_get_node(n)) nodes[n] = Node(std::forward<T>(kv)...);
				else {
					if(n == max_nodes) throw std::runtime_error("NodeAllocatorFlat::new_node(): reached maximum number of nodes!\n");
					nodes.emplace_back(std::forward<T>(kv)...);
					nvarray.resize(((size_t)(n+1))*nv_per_node,NVType());
				}
				return n;
			}
		
			/** \brief delete node -- tree is not traversed, the caller must arrange to "cut" out the node in question */
			void free_node(NodeHandle n) { 
				/* store deleted nodes in a linked list */
				nodes[n].set_deleted();
				nodes[n].set_right(Invalid);
				nodes[n].set_left(deleted_nodes_head);
				if(deleted_nodes_head != Invalid) nodes[deleted_nodes_head].set_right(n);
				deleted_nodes_head = n;
				n_del++;
			}
			/** \brief clear tree, to be reused */
			void clear_tree() {
				nodes.resize(2);
				nvarray.resize(2);
				root = 0;
				nil = 1;
				deleted_nodes_head = Invalid;
				n_del = 0;
				nodes[root] = Node();
				nodes[nil] = Node();
				shrink_memory(4);
			}
			
			
			/** \brief each node has nv_per_node values calculated and stored in it */
			unsigned int get_nv_per_node() const { return nv_per_node; }
			
			/// \brief get the partial sum stored in node n
			void get_node_sum(NodeHandle n, NVType* s) const {
				size_t base = ((size_t)n)*nv_per_node;
				for(unsigned int i=0;i<nv_per_node;i++) s[i] = nvarray[base+i];
			}
			/// \brief set the partial sum stored in node n
			void set_node_sum(NodeHandle n, const NVType* s) {
				size_t base = ((size_t)n)*nv_per_node;
				for(unsigned int i=0;i<nv_per_node;i++) nvarray[base+i] = s[i];
			}
		
		public:
			
			/** \brief get the current capacity of the underlying storage */
			size_t capacity() const { return nodes.capacity(); }
			/** \brief get the number of deleted nodes */
			size_t deleted_nodes() const { return n_del; }
			
			/// \brief Free up memory taken up by deleted nodes by rearranging storage.
			void shrink_to_fit() {
				while(deleted_nodes_head != Invalid) {
					/* remove nodes at the end */
					NodeHandle size = nodes.size();
					if(size == 0) throw std::runtime_error("NodeAllocatorCompact::shrink_size(): ran out of nodes!\n");
					size--;
					if(nodes[size].is_deleted()) {
						/* if last node was deleted, update the list of deleted nodes */
						NodeHandle left = nodes[size].get_left();
						NodeHandle right = nodes[size].get_right();
						if(left != Invalid) nodes[left].set_right(right);
						if(right != Invalid) nodes[right].set_left(left);
						if(deleted_nodes_head == size) {
							deleted_nodes_head = left; // in this case, right == Invalid
							if(deleted_nodes_head == Invalid && n_del > 1)
								throw std::runtime_error("NodeAllocatorCompact::shrink_size(): inconsistent deleted nodes!\n");
						}
					}
					else {
						/* otherwise move last node into the place of a deleted node */
						NodeHandle n = deleted_nodes_head;
						if(n == size)
							throw std::runtime_error("NodeAllocatorCompact::shrink_size(): inconsistent deleted nodes!\n");
						deleted_nodes_head = nodes[deleted_nodes_head].get_left();
						nodes[deleted_nodes_head].set_right(Invalid);
						move_node(n,size);
					}
					nodes.pop_back();
					nvarray.resize(((size_t)size)*nv_per_node);
					if(!n_del) throw std::runtime_error("NodeAllocatorCompact::shrink_size(): inconsistent deleted nodes!\n");
					n_del--;
				}
				shrink_memory();
			}
			
			/// \brief Reserve storage for at least the requested number of elements.
			/// It can throw an exception on failure to allocate memory.
			void reserve(size_t size) {
				nvarray.reserve(size);
				nodes.reserve(size);
			}
	};
}



#endif

