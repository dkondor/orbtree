# Generalized order-statistic tree implementation

An [order statistic tree](https://en.wikipedia.org/wiki/Order_statistic_tree) is a binary search tree where the rank of any element can be calculated efficiently (in O(log n) time). This is achieved by taking a balanced binary tree (a red-black tree in this case) and augmenting each node with additional information about the size of its subtree.

A generalized order statistic tree allows the efficient calculation of a partial sum of an arbitrary function, ![f(k,v)](img/fkv.svg) in general depending on the keys and values stored in the tree:
![\sum_{k < K} f(k,v)](img/partialsum.svg)

The special case ![f(k,v) \equiv 1](img/fkv1.svg) gives back a regular order-statistic tree.

This repository contains a C++ template implementation of a generalized order statistic tree, implementing functionality similar to [sets](https://en.cppreference.com/w/cpp/container/set), [multisets](https://en.cppreference.com/w/cpp/container/multiset), [maps](https://en.cppreference.com/w/cpp/container/map) and [multimaps](https://en.cppreference.com/w/cpp/container/multimap) from the standard library.


## Usage

This library requires a C++11 compiler (it was tested on g++).

To use this library, put the main header files (orbtree.h, orbtree_base.h, orbtree_node.h, vector_realloc.h, vector_stacked.h) in your source tree and include ``orbtree.h`` in your project. Use one of the following classes according to the needs of your project:
[orbtree::orbset](docs/classorbtree_1_1orbset.html),
[orbtree::orbmultiset](docs/classorbtree_1_1orbmultiset.html),
[orbtree::orbmap](docs/classorbtree_1_1orbmap.html) and
[orbtree::orbmultimap](docs/classorbtree_1_1orbmultimap.html)
provide the functionaility of sets, multisets, maps and multimaps respectively. Alternatively, the variants 
[orbtree::orbsetC](docs/classorbtree_1_1orbsetC.html),
[orbtree::orbmultisetC](docs/classorbtree_1_1orbmultisetC.html),
[orbtree::orbmapC](docs/classorbtree_1_1orbmapC.html) and
[orbtree::orbmultimapC](docs/classorbtree_1_1orbmultimapC.html)
provide the same functionality using a more compact form of storage with less memory use, especially if the expected number of elements is below 2^31-1 (approximately 2 billion elements).

The main interface for the above classes is actually provided by the
[orbtree::orbtree](docs/classorbtree_1_1orbtree.html) and
[orbtree::orbtreemap](docs/classorbtree_1_1orbtreemap.html)
classes, but it is recommended to not instantiate these directly, but use the wrappers (which are simple ``using`` template definitions with the appropriate template parameter values).

**Note**: a main difference from STL maps is that values cannot be directly set by the reference returned by
[map::at()](docs/classorbtree_1_1orbtreemap.html#a66123a9c46f754394dd7cea454c470fe)
or
[operator \[\]](docs/classorbtree_1_1orbtreemap.html#a36010840bfc431a36bb5ef6c9881d314)
or by
[dereferencing iterators](docs/structorbtree_1_1orbtree_1_1iterator__base.html#a3a4fafd27b6a15b8526f2fbeb6c9b6f8)
. The reason for this is that setting a value can require recomputing the internal function values that depend on it. Use one of the
[map::set_value()](docs/classorbtree_1_1orbtreemap.html#aca93218a61534544c32a404f43d02263)
or
[map::update_value()](docs/classorbtree_1_1orbtreemap.html#a17e509af03af8ec20b5733abbc727225)
or
[iterator::set_value()](docs/structorbtree_1_1orbtree_1_1iterator__base.html#a9b2d76ff520060a376d75c6afdc0cbc8)
functions instead.

What is needed for all cases is to provide a function object that can compute the values associated with the elements stored in the container (whose sum is calculated). For more generality, this function is assumed to compute a vector of values (e.g. to be able to run any calculation with different parameters at the same time). The expected interface is documented as
[orbtree::NVFunc_Adapter_Simple](docs/structorbtree_1_1NVFunc__Adapter__Simple.html)
; also, if you would like to use a function that returns a single value, you can use the wrapper
[orbtree::NVFunc_Adapter_Simple](docs/structorbtree_1_1NVFunc__Adapter__Simple.html)
. If you would like to do the calculations with a function that returns a single value and takes one parameter for multiple parameter values, you can use 
[orbtree::NVFunc_Adapter_Vec](docs/structorbtree_1_1NVFunc__Adapter__Vec.html)
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
orbtree::orbmap<unsigned int, unsigned int, orbtree::NVFunc_Adapter_Simple< double_add > > map1;

// insert elements
map1.insert(std::make_pair(1U,2U));
map1.insert(std::make_pair(1000U,1234U));
// ...
// calculate sum of function for element with key == 1000
unsigned int res;
map1.get_sum(1000U,&res);
```

## Details

There are two types of implementations (defined in orbtree_node.h) which differ in how they allocate memory. The simple version (using 
[orbtree::NodeAllocatorPtr](docs/classorbtree_1_1NodeAllocatorPtr.html)
) allocates each node separately and stores pointer to child and parent nodes and is used for the variants
[orbtree::orbset](docs/classorbtree_1_1orbset.html),
[orbtree::orbmultiset](docs/classorbtree_1_1orbmultiset.html),
[orbtree::orbmap](docs/classorbtree_1_1orbmap.html) and
[orbtree::orbmultimap](docs/classorbtree_1_1orbmultimap.html).
Also, it dynamically allocates storage to the vector of computed value / partial sums. The advantage of this is simplicity and that only the exact amount of memory needed for storage is allocated and memory is freed instantly when removing elements. It is also recommended to use this implementation for containers that are expected to store a moderate number of large objects (thus the overhead of storing pointers is small compared to the stored object size).

An alternative implementation (using
[orbtree::NodeAllocatorCompact](docs/classorbtree_1_1NodeAllocatorCompact.html)
) stores nodes in flat arrays (special vectors) and is used for the variants
[orbtree::orbsetC](docs/classorbtree_1_1orbsetC.html),
[orbtree::orbmultisetC](docs/classorbtree_1_1orbmultisetC.html),
[orbtree::orbmapC](docs/classorbtree_1_1orbmapC.html) and
[orbtree::orbmultimapC](docs/classorbtree_1_1orbmultimapC.html).
One main advantage is that instead of storing raw pointers, it is possible to store indexes into this array. On a 64-bit machine, using only 32-bit indexes can save a significant amount of memory, assuming that the number of elements stored does not exceed 2^32. Furthermore, the red-black flag is stored in the parent index, reducing the maximum size to 2^31-1, but further decreasing the size of nodes (typically by 1 byte, but up to 4 bytes practically due to padding). In this case, memory is allocated in larger chunks instead of individually for nodes; in some cases, this can increase performance as well. The partial sum of the values generated by the function from the keys and values are stored separately, simplifying node structure further.

The main disadvantage is the vector implementation to use. Instead of std::vector (that can be wasteful with memory), two special implementations ([realloc_vector::vector](docs/classrealloc__vector_1_1vector.html) and [stacked_vector::vector](docs/classstacked__vector_1_1vector.html)) are used that have linear growth, thus are less likely to cause out-of-memory errors. The first one (realloc_vector::vector) relies on having an efficient implementation of [realloc()](https://en.cppreference.com/w/c/memory/realloc) available and only works for key and value types that are [trivially copyable](https://en.cppreference.com/w/cpp/named_req/TriviallyCopyable) (essentially any type that can be moved to a different memory location without issues). For general types, a second option is used (stacked_vector::vector) that maintains a stack of vectors. This implementation can be significantly slower due to a lot of integer division operations necessary to access elements. This can be sped up by using the [libdivide](https://github.com/ridiculousfish/libdivide) library, that can be enabled by defining USE_LIBDIVIDE.






