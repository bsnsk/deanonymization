//
//  match.hpp
//  deanonymization
//
//  Created by Shuyang Shi on 15/9/20.
//  Copyright © 2015年 Shuyang Shi. All rights reserved.
//

#ifndef match_hpp
#define match_hpp

#include "graph.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <map>
#include <time.h>
#include <algorithm>

// whether to print the similarity values
#define PRINT_SIMI 0

// MAX rounds of updating each pair of nodes
#define MAX_ROUNDS 6

// whether use multithread
#define MULTITHREAD 1

#if MULTITHREAD

#include "thpool.hpp"

// size of the thread pool
#define THREAD_POOL_SIZE 12

#endif

using namespace std;

int int_abs(int);

#if MULTITHREAD
static threadpool thpool;
#endif

class matcher {
private:
	
	// graphs given
	class graph * G_a;
	class graph * G;

	typedef double all_node_pairs[MAX_NODES][MAX_NODES]; // x_a, x

	// matrix for sim_nodes
	all_node_pairs sim_nodes;
	all_node_pairs last_round;

	// sorted sim_nodes
	struct node_pair {
		int u, v;
		all_node_pairs * sims;
		node_pair(int U=0, int V=0): u(U), v(V) {}
		bool operator < (const struct node_pair &b) const {
			return sims[u][v] > sims[b.u][b.v];
		}
	} sim_pairs[MAX_NODES * MAX_NODES];

	// match_edge structure
	struct match_edge {
		int u, v;
		double w;
		match_edge(int _u=0, int _v=0, double _w=0): u(_u), v(_v), w(_w) {}
		bool operator < (const match_edge & b) const {
			return w > b.w;
		}
	};
	
	// answer sequence
	vector<match_edge> ans_pairs;

	// heap used when generating answer pairs
	struct heap {
		struct heap_node {
			int u, v;
			heap_node(int _=0, int __=0): u(_), v(__) {}
		};

#define heap_fa(x) ((x)>>1)
#define heap_lc(x) ((x)<<1)
#define heap_rc(x) (((x)<<1)+1)
#define heap_v(x) (owner->sim_nodes[nodes[x].u][nodes[x].v])
#define heap_p(x) (heap_pos[nodes[x].u][nodes[x].v])
		struct matcher *owner;
		int len;
		struct heap_node nodes[MAX_NODES*MAX_NODES*2];
		all_node_pairs heap_pos;

		heap(int n, int m, struct matcher *o);
		void heap_down(int x);
		void heap_up(int x);
		void pop();

	} * H;

public:

	// initialize
	matcher(class graph *g_a, class graph *g);
	
	// calculate sim_nodes
	double calc_sim_nodes(int u, int v); // u from G_a, v from G
	void calc_sim_nodes_wrapper(int u, int v);

	// match and generate answer pairs
	void match();
	void gen_ans_pairs();
	
	// print answer pairs
	void print(FILE *ou);
};

#if MULTITHREAD
void * calc_sim_nodes_pthread(void * args);
#endif

#endif /* match_hpp */
