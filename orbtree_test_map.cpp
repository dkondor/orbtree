/*
 * orbtree_test.cpp -- test program for order statistic tree using maps
 * 
 * Copyright 2023 Daniel Kondor <kondor.dani@gmail.com>
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
 */


#include <stdio.h>
#include <stdint.h>
#include "read_table.h"
#include "orbtree.h"

int main(int argc, char **argv)
{

#ifdef USE_COMPACT	
	orbtree::rankmultimapC<double, double, uint32_t> rbtree;
#else
	orbtree::rankmultimap<double, double, uint32_t> rbtree;
#endif
	
	bool check_only_end = false;
	if(argc > 1 && argv[1][0] == '-' && argv[1][1] == 'c') check_only_end = true;
	
	read_table2 rt(stdin);
	
	while(rt.read_line()) {
		double x;
		if(!rt.read(x)) break;
		
		bool neg = (x < 0);
		if(neg) x *= -1L;
		double y = x;
		
		if(neg) {
			/* try to find and remove */
			auto it = rbtree.lower_bound(y);
			if(it == rbtree.end() || it->first != y) throw std::runtime_error("key not found!\n");
			rbtree.erase(it);
		}
		else {
			/* add one more key */
			rbtree.insert(std::make_pair(y, y));
		}
		
		if(!check_only_end) {
			/* check tree */
			rbtree.check_tree(0.0);
			
			/* check all ranks (by iterating over all nodes) */
			uint32_t i = 0;
			for(auto it = rbtree.cbegin();it != rbtree.cend();++it,++i) {
				uint32_t r = rbtree.get_sum_node(it);
				if(r != i) throw std::runtime_error("key rank not consistent!\n");
				auto it2 = orbtree::lower_bound_r(rbtree, r);
				if(it2 != it) throw std::runtime_error("rank search result not consistent!\n");
			}
			if(i != rbtree.size()) throw std::runtime_error("inconsistent tree size!\n");
		}
	}
	
	if(check_only_end) {
		/* check tree */
		rbtree.check_tree(0.0);
		
		/* check all ranks (by iterating over all nodes) */
		uint32_t i = 0;
		for(auto it = rbtree.cbegin();it != rbtree.cend();++it,++i) {
			uint32_t r = rbtree.get_sum_node(it);
			if(r != i) throw std::runtime_error("key rank not consistent!\n");
			auto it2 = orbtree::lower_bound_r(rbtree, r);
			if(it2 != it) throw std::runtime_error("rank search result not consistent!\n");
		}
		if(i != rbtree.size()) throw std::runtime_error("inconsistent tree size!\n");
	}
	
	if(rt.get_last_error() != T_EOF) rt.write_error(stderr);
	
	return 0;
}


