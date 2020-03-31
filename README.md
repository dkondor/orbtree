# Generalized order-statistic tree implementation

An [order statistic tree](https://en.wikipedia.org/wiki/Order_statistic_tree) is a binary search tree where the rank of any element can be calculated efficiently (in O(log n) time). This is achieved by taking a balanced binary tree (a red-black tree in this case) and augmenting each node with additional information about the size of its subtree.

A generalized order statistic tree allows the efficient calculation of a partial sum of an arbitrary function, ![f(k,v)](img/fkv.svg) in general depending on the keys and values stored in the tree:
![\sum_{k < K} f(k,v)](img/partialsum.svg)

The special case ![f(k,v) \equiv 1](img/fkv1.svg) gives back a regular order-statistic tree. In the following, I refer to this function as the _weight_ function and the value it returns as the weight of an element. Efficient calculation of the sum of weights can be achieved by storing partial sums in the nodes of a binary tree. In this case, each node stores the partial sum for its subtree.

This repository contains a C++ template implementation of a generalized order statistic tree, implementing functionality similar to [sets](https://en.cppreference.com/w/cpp/container/set), [multisets](https://en.cppreference.com/w/cpp/container/multiset), [maps](https://en.cppreference.com/w/cpp/container/map) and [multimaps](https://en.cppreference.com/w/cpp/container/multimap) from the standard library. Doxygen documentation is available at https://dkondor.github.io/orbtree/.

Example code using this library is available at https://github.com/dkondor/patest_new, implementing testing for preferential attachment in growing networks (according to our paper [Do the Rich Get Richer? An Empirical Analysis of the Bitcoin Transaction Network](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0086197)).


## Usage

This library requires a C++11 compiler (it was tested on g++).

To use this library, put the main header files (orbtree.h, orbtree_base.h, orbtree_node.h, vector_realloc.h, vector_stacked.h) in your source tree and include ``orbtree.h`` in your project. Each container type has four implementations, according to the following two choices:

 - Whether the weight function returns only one value (simple), or returns a vector of values (vector). The latter case can be especially useful if the goal is to evaluate the same function with multiple parameter values.
 - Whether the implementation allocated individual nodes separately and uses pointers internally, or stores nodes in vector-like containers and uses indexes internally to refer to nodes. The latter choice provides a more compact tree with less memeory allocation, and for trees with less than 2^31-1 elements, it allows the use of 32-bit indexes instead of 64-bit pointers -- this can in turn decrease memory use as well.
 
These implementations are provided by the following classes (all part of the ``orbtree`` namespace:

|                                     | Pointer-based storage | Compact storage |
| ----------------------------------- | --------------------- | --------------- |
| Weight function return scalar value | [simple_set](https://dkondor.github.io/orbtree/classorbtree_1_1simple__set.html), [simple_multiset](https://dkondor.github.io/orbtree/classorbtree_1_1simple__multiset.html), [simple_map](https://dkondor.github.io/orbtree/classorbtree_1_1simple__map.html), [simple_multimap](https://dkondor.github.io/orbtree/classorbtree_1_1simple__multimap.html) | [simple_setC](https://dkondor.github.io/orbtree/classorbtree_1_1simple__setC.html), [simple_multisetC](https://dkondor.github.io/orbtree/classorbtree_1_1simple__multisetC.html), [simple_mapC](https://dkondor.github.io/orbtree/classorbtree_1_1simple__mapC.html), [simple_multimapC](https://dkondor.github.io/orbtree/classorbtree_1_1simple__multimapC.html) |
| Weight function returns vector      | [orbset](https://dkondor.github.io/orbtree/classorbtree_1_1orbset.html), [orbmultiset](https://dkondor.github.io/orbtree/classorbtree_1_1orbmultiset.html), [orbmap](https://dkondor.github.io/orbtree/classorbtree_1_1orbmap.html), [orbmultimap](https://dkondor.github.io/orbtree/classorbtree_1_1orbmultimap.html) | [orbsetC](https://dkondor.github.io/orbtree/classorbtree_1_1orbsetC.html), [orbmultisetC](https://dkondor.github.io/orbtree/classorbtree_1_1orbmultisetC.html), [orbmapC](https://dkondor.github.io/orbtree/classorbtree_1_1orbmapC.html), [orbmultimapC](https://dkondor.github.io/orbtree/classorbtree_1_1orbmultimapC.html) |


The main interface for the above is actually provided by the
[orbtree::orbtree](https://dkondor.github.io/orbtree/classorbtree_1_1orbtree.html) and
[orbtree::orbtreemap](https://dkondor.github.io/orbtree/classorbtree_1_1orbtreemap.html)
classes, but it is recommended to not instantiate these directly, but use the wrappers (which are simple ``using`` template definitions with the appropriate template parameter values).

**Note**: a main difference from STL maps is that values cannot be directly set by the reference returned by
[map::at()](https://dkondor.github.io/orbtree/classorbtree_1_1orbtreemap.html#afcd0ad3aaec56efa1bd0f1eb133d119f)
or
[operator \[\]](https://dkondor.github.io/orbtree/classorbtree_1_1orbtreemap.html#ac6589c21a2b662426486e75046edf11f)
or by
[dereferencing iterators](https://dkondor.github.io/orbtree/structorbtree_1_1orbtree_1_1iterator__base.html#a3a4fafd27b6a15b8526f2fbeb6c9b6f8)
. The reason for this is that setting a value can require recomputing the internal function values that depend on it. Use one of the
[map::set_value()](https://dkondor.github.io/orbtree/classorbtree_1_1orbtreemap.html#a203b1dd0e1834e71c38258bda179ae49)
or
[map::update_value()](https://dkondor.github.io/orbtree/classorbtree_1_1orbtreemap.html#a2170e391bc8d51302f9208ef02b0967d)
or
[iterator::set_value()](https://dkondor.github.io/orbtree/structorbtree_1_1orbtree_1_1iterator__base.html#a80aa4ab05496ddf4ce7e88e6d5102175)
functions instead.

What is needed for all cases is to provide a function object that can compute the values associated with the elements stored in the container (whose sum is calculated). For more generality, this function can either return a scalar (to be used be the ``simple_`` variants) or be vector-valued, e.g. to compute results with different parameter values using the same tree.

The expected interface is documented as
[orbtree::NVFunc_Adapter_Simple](https://dkondor.github.io/orbtree/structorbtree_1_1NVFunc__Adapter__Simple.html)
; also, if you would like to use a function that returns a single value, you can use the wrapper
[orbtree::NVFunc_Adapter_Simple](https://dkondor.github.io/orbtree/structorbtree_1_1NVFunc__Adapter__Simple.html)
.

A simple example is the following for a function that adds and doubles the key and value (which are both unsigned integers):

```
// define function to use
struct double_add {
    typedef std::pair<unsigned int, unsigned int> argument_type;
    typedef unsigned int result_type;
    unsigned int operator()(const std::pair<unsigned int, unsigned int>& p) const {
        return 2*(p.first + p.second);
    }
};


// map with the above function
orbtree::simple_map<unsigned int, unsigned int, double_add > map1;

// insert elements
map1.insert(std::make_pair(1U,2U));
map1.insert(std::make_pair(1000U,1234U));
// ...
// calculate sum of function for element with key == 1000
unsigned int res = map1.get_sum(1000U);
```

A more detailed example with a vector function that multiplies the key with the given parameter and a map that actually stores a "histogram", thus weights need to be multiplied by the value of nodes:
```
// function to use
struct mult_hist {
    typedef std::pair<unsigned int, unsigned int> argument_type;
    typedef double ParType;
    typedef double result_type;
    double operator()(const std::pair<unsigned int, unsigned int>& p, double a) const {
        return a*p.first*p.second;
    }
};
```

When declaring the tree, it is possible to use the [orbtree::NVFunc_Adapter_Vec]() adapter and also to give a vector of parameter values to the tree constructor. When calculating weights, the weight function will be called with each parameter value.
```
std::vector<double> parameters{1.0,2.5,5.555555};
orbtree::orbmapC<unsigned int, unsigned int, orbtree::NVFunc_Adapter_Vec<mult_hist> > map2(parameters);
```

Insert elements as normal:
```
map2.insert(std::make_pair(1U,3U);
map2.insert(std::make_pair(10U,1U);
map2.insert(std::make_pair(5U,2U);
```

Calculation of weights now requires an appropriately sized array:
```
std::vector<double> sum1(parameters.size());
std::vector<double> norm(parameters.size());
map2.get_sum(10,tmp.data());
map2.get_norm(norm.data());
```


Note: it is not possible to modify mapped values directly or with iterators, e.g. the following will not compile:
```
map2[1] = 5;
map2[10]++;
auto it = map2.find(5);
it->second = 1;
```
It is possible to use 
[map::set_value()](https://dkondor.github.io/orbtree/classorbtree_1_1orbtreemap.html#a203b1dd0e1834e71c38258bda179ae49)
or
[map::update_value()](https://dkondor.github.io/orbtree/classorbtree_1_1orbtreemap.html#a2170e391bc8d51302f9208ef02b0967d)
to modify the values, e.g.:
```
map2.update_value(1,5);
map2.set_value(11,1);
```
Note that the diffence between the two is that 
[update_value()](https://dkondor.github.io/orbtree/classorbtree_1_1orbtreemap.html#a2170e391bc8d51302f9208ef02b0967d)
requires the key to be already present, i.e. it will throw an exception if called with a key not present in the map. On the other hand, 
[set_value()](https://dkondor.github.io/orbtree/classorbtree_1_1orbtreemap.html#a203b1dd0e1834e71c38258bda179ae49) will insert a new element if the key is not present (if the key is present, it will update the value -- of course, you can use
[insert()](https://dkondor.github.io/orbtree/classorbtree_1_1orbtree.html#ab810d2bd674ee1a7fea099b9a69f6243)
to only insert a new value if the key is not yet present in a map). Also, values can be updated using iterators:
```
auto it = map2.find(5);
it.set_value(1);
```
Note that
[iterator::set_value()](https://dkondor.github.io/orbtree/structorbtree_1_1orbtree_1_1iterator__base.html#a80aa4ab05496ddf4ce7e88e6d5102175) is present for both maps and multimaps, so it can be used to update values in a multimap as well. Read-only access is still possible the usual way, e.g.:
```
unsigned int x = map2[1];
unsigned int y = map2[10];
auto it = map2.find(5);
unsigned int z = it->second;
```
Note that if called with a non-existent key, 
[operator \[\]](https://dkondor.github.io/orbtree/classorbtree_1_1orbtreemap.html#ac6589c21a2b662426486e75046edf11f)
will still insert a new element with the default value, similar to [std::map](https://en.cppreference.com/w/cpp/container/map/operator_at).

## Details

There are two types of implementations (defined in orbtree_node.h) which differ in how they allocate memory. The pointer-based version (using 
[orbtree::NodeAllocatorPtr](https://dkondor.github.io/orbtree/classorbtree_1_1NodeAllocatorPtr.html)
) allocates each node separately and stores pointer to child and parent nodes and is used for the variants
[simple_set](https://dkondor.github.io/orbtree/classorbtree_1_1simple__set.html), [simple_multiset](https://dkondor.github.io/orbtree/classorbtree_1_1simple__multiset.html), [simple_map](https://dkondor.github.io/orbtree/classorbtree_1_1simple__map.html), [simple_multimap](https://dkondor.github.io/orbtree/classorbtree_1_1simple__multimap.html) and
[orbset](https://dkondor.github.io/orbtree/classorbtree_1_1orbset.html),
[orbmultiset](https://dkondor.github.io/orbtree/classorbtree_1_1orbmultiset.html),
[orbmap](https://dkondor.github.io/orbtree/classorbtree_1_1orbmap.html) and
[orbmultimap](https://dkondor.github.io/orbtree/classorbtree_1_1orbmultimap.html).
Also, for the non-simple versions, it dynamically allocates storage for the vector of computed partial sums of weights. The advantage of this is simplicity and that only the exact amount of memory needed for storage is allocated and memory is freed instantly when removing elements. It is also recommended to use this implementation for containers that are expected to store a moderate number of large objects (thus the overhead of storing pointers is small compared to the stored object size).

An alternative implementation (using
[orbtree::NodeAllocatorCompact](https://dkondor.github.io/orbtree/classorbtree_1_1NodeAllocatorCompact.html)
) stores nodes in flat arrays (special vectors) and is used for the variants [simple_setC](https://dkondor.github.io/orbtree/classorbtree_1_1simple__setC.html), [simple_multisetC](https://dkondor.github.io/orbtree/classorbtree_1_1simple__multisetC.html), [simple_mapC](https://dkondor.github.io/orbtree/classorbtree_1_1simple__mapC.html), [simple_multimapC](https://dkondor.github.io/orbtree/classorbtree_1_1simple__multimapC.html) and
[orbsetC](https://dkondor.github.io/orbtree/classorbtree_1_1orbsetC.html),
[orbmultisetC](https://dkondor.github.io/orbtree/classorbtree_1_1orbmultisetC.html),
[orbmapC](https://dkondor.github.io/orbtree/classorbtree_1_1orbmapC.html) and
[orbmultimapC](https://dkondor.github.io/orbtree/classorbtree_1_1orbmultimapC.html).
One main advantage is that instead of storing raw pointers, it is possible to store indexes into this array. On a 64-bit machine, using only 32-bit indexes can save a significant amount of memory, assuming that the number of elements stored does not exceed 2^32. Furthermore, the red-black flag is stored in the parent index, reducing the maximum size to 2^31-1, but decreasing the size of nodes (typically by 1 byte, but up to 4 bytes practically due to padding). In this case, memory is allocated in larger chunks instead of individually for nodes; in some cases, this can increase performance as well. The partial sums of the node weights are stored separately, simplifying node structure further and avoiding dynamic memory allocation for each node.

The main disadvantage is the vector implementation to use. Instead of std::vector (that can be wasteful with memory), two special implementations ([realloc_vector::vector](https://dkondor.github.io/orbtree/classrealloc__vector_1_1vector.html) and [stacked_vector::vector](https://dkondor.github.io/orbtree/classstacked__vector_1_1vector.html)) are used that have linear growth, thus are less likely to cause out-of-memory errors. The first one (realloc_vector::vector) relies on having an efficient implementation of [realloc()](https://en.cppreference.com/w/c/memory/realloc) available and only works for key and value types that are [trivially copyable](https://en.cppreference.com/w/cpp/named_req/TriviallyCopyable) (essentially any type that can be moved to a different memory location without invoking its copy / move constructor). For general types, a second option is used (stacked_vector::vector) that maintains a stack of vectors. This implementation can be significantly slower due to a lot of integer division operations necessary to access elements. This can be sped up by using the [libdivide](https://github.com/ridiculousfish/libdivide) library, that can be enabled by defining USE_LIBDIVIDE.






