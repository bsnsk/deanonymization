//
//  main.cpp
//  deanonymization
//
//  Created by Shuyang Shi on 15/9/20.
//  Copyright © 2015年 Shuyang Shi. All rights reserved.
//

#include <cstdio>
#include "graph.hpp"
#include "match.hpp"
#include "data_analysis.hpp"

#define DEBUG 0

int main(int argc, const char * argv[]) {

#ifdef BASELINE
	fprintf(stderr, "De-anonymizing ... version = BASELINE\n");
#else
#ifdef ALPHA_R
	fprintf(stderr, "De-anonymizing ... version = ALPHA-RoleSim++\n");
#else
	fprintf(stderr, "De-anonymizing ... version = RoleSim++\n");
#endif
#endif

	graph *G_a, *G;

	G_a = new graph("data/anonymized.txt");
	G = new graph("data/crawled.txt");

	matcher *M = new matcher(G_a, G);
#if DEBUG
	M->debug_print();
#endif
	M->gen_sim_matrix_simranc();
	M->print_top_simi();

#if DEBUG
	//M->load_matrix();
	M->record_matrix();
	M->gen_ans_pairs_oldway();

	FILE * backup_res = fopen("bak_res.txt", "w");
	M->print(backup_res);
	fclose(backup_res);
	fprintf(stderr, "old ways result generated in `bak_res.txt`.\n");
#endif

#ifndef BASELINE
		M->gen_ans_pairs();
#else
		M->gen_ans_pairs_oldway();
#endif
	M->print(stdout);

    return 0;
}
