/*
 * vector_test.cpp -- simple tests for vector class implementation
 * 
 * Copyright 2020 Daniel Kondor <kondor.dani@gmail.com>
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


#ifdef USE_STACKED
#include "vector_stacked.h"
#define realloc_vector stacked_vector
#else
#include "vector_realloc.h"
#endif

#include <stdio.h>
#include <random>
#include <vector>
#include <algorithm>



/* function to compare two vectors for checking results */
template<class Vec1, class Vec2>
bool cmp_vec(const Vec1& v1, const Vec2& v2, bool throw_on_error = true) {
	static_assert(std::is_same<typename Vec1::value_type, typename Vec2::value_type>::value,
		"cmp_vec() needs vectors with same stored types!\n");
	bool ret = true;
	if(v1.size() != v2.size()) {
		fprintf(stderr,"cmp_vec: size differs!\n");
		ret = false;
	}
	for(size_t i=0;i<v1.size() && ret;i++) if(v1[i] != v2[i]) {
		fprintf(stderr,"cmp_vec: element %lu differs!\n",i);
		ret = false;
	}
	if(!ret && throw_on_error) throw std::runtime_error("cmp_vec(): vectors differ!\n");
	return ret;
}


/* custom quicksort
 * recursive function using [begin,end] range */
template<class It>
void quicksort_r(It begin, It end) {
	using std::swap;
	It begin0 = begin;
	ssize_t dist = end - begin;
	if(dist == 0) return;
	if(dist == 1) {
		if(*end < *begin) swap(*end,*begin);
		return;
	}
	ssize_t pivot = dist / 2;
	swap(begin[pivot],*end);
	It p = begin;
	for(;begin<end;begin++) {
		if(*begin < *end) {
			swap(*begin,*p);
			p++;
		}
	}
	swap(*end,*p);
	if(p > begin0) quicksort_r(begin0,p-1);
	if(end > p) quicksort_r(p+1,end);
}

/* interface function using [begin,end) range */
template<class It>
void quicksort(It begin, It end) {
	if(end > begin) quicksort_r(begin,end-1);
}



int main(int argc, char **argv)
{
	size_t size = 1000;
	int mod = 1000;
	size_t insert_size = 1;
	
	for(int i=1;i<argc;i++) if(argv[i][0] == '-') switch(argv[i][1]) {
		case 's':
			size = strtoul(argv[i+1],0,10);
			i++;
			break;
		case 'm':
			mod = atoi(argv[i+1]);
			i++;
			break;
		case 'i':
			insert_size = strtoul(argv[i+1],0,10);
			i++;
			break;
		default:
			fprintf(stderr,"Unknown argument: %s!\n",argv[i]);
			break;
	}
	else fprintf(stderr,"Unknown argument: %s!\n",argv[i]);
	
	/* create two vectors with random elements */
	std::vector<int> v1(size);
	realloc_vector::vector<int> v2(size);
	realloc_vector::vector<int> v3;
	
	std::mt19937 rg;
	std::uniform_int_distribution<int> rd(1,mod);
	for(size_t i=0;i<size;i++) {
		int x = rd(rg);
		v1[i] = x;
		v2[i] = x;
		v3.push_back(x);
	}
	
	cmp_vec(v1,v2);
	cmp_vec(v1,v3);
	cmp_vec(v2,v2);
	
	/* test iterators with std::sort */
	
	std::vector<int> v12(v1);
	realloc_vector::vector<int> v22(v2.begin(),v2.end());
	
	cmp_vec(v12,v22);
	cmp_vec(v1,v22);
	
	std::sort(v12.begin(),v12.end());
	std::sort(v22.begin(),v22.end());
	/*
	cmp_vec(v12,v22);
	
	v12 = v1;
	v22 = v2;
	
	cmp_vec(v12,v22);
	
	quicksort(v12.begin(),v12.end());
	quicksort(v22.begin(),v22.end());
	
	cmp_vec(v12,v22);
	*/
	
	/* test positional insert and erase -- note that these are O(n^2), should be for lower size 
	for(size_t i=0;i<size;i++) {
		int x = rd(rg);
		auto it1 = std::lower_bound(v12.begin(),v12.end(),x);
		ssize_t d1 = it1 - v12.begin();
		v12.insert(it1,x);
		auto it2 = std::lower_bound(v22.begin(),v22.end(),x);
		ssize_t d2 = it2 - v22.begin();
		v22.insert(it2,x);
		if(d1 != d2) throw std::runtime_error("insert position differs!\n");
	}
	cmp_vec(v12,v22);
	
	for(size_t i=0;i<size;i++) {
		int x = rd(rg);
		auto it1 = std::lower_bound(v12.begin(),v12.end(),x);
		ssize_t d1 = it1 - v12.begin();
		if(it1 != v12.end()) v12.erase(it1);
		auto it2 = std::lower_bound(v22.begin(),v22.end(),x);
		ssize_t d2 = it2 - v22.begin();
		if(it2 != v22.end()) v22.erase(it2);
		if(d1 != d2) throw std::runtime_error("insert position differs!\n");
	}
	cmp_vec(v12,v22);
	*/
	
	std::vector<int> insert_vec(insert_size);
	
	for(size_t j = 0; j < size;) {
		size_t i;
		for(i = 0; i < insert_size && j < size; i++, j++)
			insert_vec[i] = rd(rg);
		std::uniform_int_distribution<size_t> rd2(0,v12.size()-1);
		size_t insert_pos = rd2(rg);
		v12.insert(v12.begin() + insert_pos,insert_vec.begin(),insert_vec.begin() + i);
		v22.insert(v22.begin() + insert_pos,insert_vec.begin(),insert_vec.begin() + i);
	}
	
	cmp_vec(v12,v22);
	
	return 0;
}

