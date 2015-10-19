//
//  match.cpp
//  deanonymization
//
//  Created by Shuyang Shi on 15/9/20.
//  Copyright © 2015年 Shuyang Shi. All rights reserved.
//

#include "match.hpp"

int int_abs(int x){
	return x < 0 ? -x : x;
}

matcher::matcher(class graph * g_a, class graph * g): G_a(g_a), G(g) {
	for (int i=1; i<=g_a->num_nodes; i++)
		for (int j=1; j<=g->num_nodes; j++){
			sim_nodes[i][j] = 1;
		}
}

void matcher::debug_print() {
	FILE *ou = fopen("debug.info", "w");
	fprintf(ou, "G_a: %d nodes,\t G:%d nodes.\n",
			G_a->num_nodes, G->num_nodes);
	int sum[2], S=0;
	sum[0]=sum[1]=0;
	for (int i=1; i<=G_a->num_nodes; i++){
		graph::subgraph *subg = G_a->extract_subgraph(i);
		sum[0] += subg->num_nodes_seq[0];
		sum[1] += subg->num_nodes_seq[1];
		S += subg->num_nodes_seq[0] * subg->num_nodes_seq[0];
		S += subg->num_nodes_seq[1] * subg->num_nodes_seq[1];
	}
	fprintf(ou, "Average neighbor number (G_a): %d %d\t#%d\n",
			sum[0] / G_a->num_nodes, sum[1] / G->num_nodes, S);
	fclose(ou);
}

double matcher::calc_sim_nodes(int u, int v, int level) {
#ifdef USE_ONLY_NEIGHBORS
	level = 1;
#endif

	graph::subgraph * subg_a = G_a->extract_subgraph(u);
	graph::subgraph * subg = G->extract_subgraph(v);

	double w = 0; // sum(sum_per_level(sim_nodes))
#if AVERAGE_EACH_CALC
	int sum = 0;
#endif
	vector <match_edge> match_edges;
	set <int> flag_a;
	set <int> flag;
	for (int i=0; i < level; i++) {
#if AVERAGE_EACH_CALC
		sum += min(subg_a->nodes_per_level[i].size(),
				subg->nodes_per_level[i].size());
#endif
		for (vector<int> :: iterator j = subg_a->nodes_per_level[i].begin(); j!=subg_a->nodes_per_level[i].end(); j++)
			for (vector<int> :: iterator k = subg->nodes_per_level[i].begin(); k!=subg->nodes_per_level[i].end(); k++)
				match_edges.push_back(match_edge(*j, *k, last_round[*j][*k]));
	}
	sort(match_edges.begin(), match_edges.end());
	for (vector<match_edge> :: iterator it=match_edges.begin(); it!=match_edges.end(); it++) {
		if (flag_a.find(it->u) == flag_a.end() && flag.find(it->v) == flag.end()) {
			flag_a.insert(it->u);
			flag.insert(it->v);
			w += it->w;
		}
	}
#if AVERAGE_EACH_CALC
	w /= sum;
#endif
//	if (u%5000==0 && v%5000 == 0)
//		fprintf(stderr, "\t#sim_nodes(%d, %d) = %g#", u, v, w);
	return sim_nodes[u][v] = w ;
}

#if MULTITHREAD
void * calc_sim_nodes_pthread(void * args) {
	int * t = ((int**)args)[0];
	((class matcher *)(((int**)args)[1]))->calc_sim_nodes(t[0], t[1], t[2]);
	return NULL;
}
#endif

void matcher::calc_sim_nodes_wrapper(int i, int j, bool flag){
#if MULTITHREAD
	int ** t = new int* [2];
	t[0] = new int[3];
	t[0][0] = i, t[0][1] = j, t[0][2] = (flag ? 2 : 1);
	t[1] = (int*)this;
	thpool_add_work(thpool, (calc_sim_nodes_pthread), (void *)t);
#else
	if (flag)
		calc_sim_nodes(i, j, 2);
	else
		calc_sim_nodes(i, j, 1);
#endif
}

void matcher::record_matrix() {
	FILE *ou = fopen("matrix.txt", "w");
	fprintf(ou, "%d %d\n", G_a->num_nodes, G->num_nodes);
	for (int i=1; i<=G_a->num_nodes; i++){
		for (int j=1; j<=G->num_nodes; j++)
			fprintf(ou, "%g\t", sim_nodes[i][j]);
		fprintf(ou, "\n");
	}
	fclose(ou);
	fprintf(stderr, "matrix saved in `matrix.txt`.\n");
}

void matcher::load_matrix() {
	FILE *in = fopen("matrix.txt", "r");
	fscanf(in, "%d%d", &(G_a->num_nodes), &(G->num_nodes));
	for (int i = 1; i <= G_a->num_nodes; i++)
		for (int j = 1; j <= G->num_nodes; j++)
			fscanf(in, "%lf", sim_nodes[i]+j);
	fclose(in);
	fprintf(stderr, "Matrix read from `matrix.txt`\n");
}

void matcher::gen_random_walk(class graph *G, int start, int maxlen, int delta, FILE * ou) {
	for (int nxt, idx, cur = start; maxlen > 0; maxlen--, cur = nxt){
		if (! G->edges[cur]->size())
			break;
		idx = rand() % (G->edges[cur]->size());
		nxt = (*(G->edges[cur]))[idx];
		fprintf(ou, "%d ", cur + delta);
		if (nxt == start)
			break;
	}
	fprintf(ou, "\n");fflush(ou);
}

void matcher::gen_passage() {
	FILE * psg = fopen("passage.txt", "w");
	// TODO
#define GAMMA 30
#define MAXLEN 40
	srand(time(0));
	for (int i=1; i<=G_a->num_nodes; i++)
		for (int j=0; j<GAMMA; j++)
			gen_random_walk(G_a, i, MAXLEN, 0, psg);
	for (int i=1; i<=G->num_nodes; i++)
		for (int j=0; j<GAMMA; j++)
			gen_random_walk(G, i, MAXLEN, G_a->num_nodes, psg);
	fclose(psg);
	fprintf(stderr, "passage generated.\n");
}

void matcher::gen_sim_matrix_word2vec() {
	// generate passage
	gen_passage();

	// train node vectors
	system("./word2vec-read-only/word2vec -train passage.txt -output vectors.txt -cbow 0 -size 300 -window 5 -negative 5 -hs 1 -sample 1e-5 -threads 3 -binary 0 -iter 15");

	// read node vectors
	FILE *in = fopen("vectors.txt", "r");
	int tot_nodes = G_a->num_nodes + G->num_nodes;
	double ** vectors = new double * [tot_nodes+1];
	for (int i=1; i<=tot_nodes; i++)
		vectors[i] = new double [300];
	for (int i=0, j; i<tot_nodes; i++){
		fscanf(in, "%d", &j);
		for (int k=0; k<300; k++)
			fscanf(in, "%lf", vectors[j]+k);
	}
	fclose(in);

	// generate matrix
	for (int i=1; i<=G_a->num_nodes; i++)
		for (int j=1; j<=G->num_nodes; j++)
			sim_nodes[i][j] = cos_analogy(vectors[i], vectors[j+G_a->num_nodes], 300);
}

double cos_analogy(double *a, double *b, int len) {
	double re=0, sa=0, sb=0;
	for (int i=0; i<len; i++){
		re += a[i] * b[i];
		sa += a[i] * a[i];
		sb += b[i] * b[i];
	}
	return fabs(re) / sqrt(sa * sb);
}

void matcher::gen_sim_matrix_simranc() {
	clock_t time_start = clock();
	int cT;

#if MULTITHREAD
	thpool = thpool_init(THREAD_POOL_SIZE);
	for (int i=1; i<=G_a->num_nodes; i++)
		G_a->extract_subgraph(i);
	for (int i=1; i<=G->num_nodes; i++)
		G->extract_subgraph(i);
#endif

	for (cT=0; cT++ < MAX_ROUNDS; ) {

		fprintf(stderr, "Processing round %d ... \r", cT);

		// update node similarities for every pair of nodes
		memcpy(last_round, sim_nodes, sizeof(sim_nodes));

		for (int i=1; i<=G_a->num_nodes; i++)
			for (int j=1; j<=G->num_nodes; j++){
				calc_sim_nodes_wrapper(i, j);
			}
		int tmpCNT = 0;
		/*
		for (int i=1; i<=G_a->num_nodes; i++)
			for (int j=1; j<=G->num_nodes; j++){
				sim_pairs[tmpCNT++] = node_pair(i, j, &sim_nodes);
			}
		sort(sim_pairs, sim_pairs + tmpCNT);
		for (int i=0; i < tmpCNT; i++)
			calc_sim_nodes_wrapper(sim_pairs[i].u, sim_pairs[i].v,
					i < (tmpCNT >> (cT-1)));
		*/

#if MULTITHREAD
		thpool_wait(thpool);
#endif

		// normalization
		double max_ele_nodes = -1;
		for (int i=1; i<=G_a->num_nodes; i++)
			for (int j=1; j<=G->num_nodes; j++){
				double tmp = sim_nodes[i][j];
				max_ele_nodes = max(max_ele_nodes, tmp);
			}
		for (int i=1; i<=G_a->num_nodes; i++)
			for (int j=1; j<=G->num_nodes; j++){
				sim_nodes[i][j] /= max_ele_nodes;
			}
		fprintf(stderr, "Round %d processed (%d pairs updated)\n",
				cT, (tmpCNT >> (cT-1)));
	}

	fprintf(stderr, "matcher info\n\t%d rounds, %.2lf seconds.\n",
			MAX_ROUNDS, (clock()-time_start)*1.0/CLOCKS_PER_SEC);
}

matcher::heap::heap(class matcher *o) {
	len = 0;
	owner = o;
}

matcher::heap::heap(int n, int m, class matcher *o) {
	owner = o;
	len = 0;
	for (int i=1; i<=n; i++)
		for (int j=1; j<=m; j++){
			heap_pos[i][j] = ++len;
			nodes[len] = heap_node(i, j);
		}
	for (int i=len/2; i>0; i--)
		heap_down(i);
}

void matcher::heap::push(int u, int v) {
	int pos = heap_pos[u][v];
	if (pos <= len && nodes[pos].u == u && nodes[pos].v == v)
		return;
	nodes[++len] = heap_node(u, v);
	heap_pos[u][v] = len;
	heap_up(len);
}

void matcher::heap::heap_down(int x) {
	for (int ma; heap_lc(x) <= len; x = ma){
		ma = heap_rc(x) <= len
			? (heap_v(heap_lc(x)) > heap_v(heap_rc(x))
				? heap_lc(x) : heap_rc(x))
			: heap_lc(x);
		if (heap_v(x) >= heap_v(ma))
			break;
		swap(nodes[x], nodes[ma]);
		swap(heap_p(x), heap_p(ma));
	}
}

void matcher::heap::heap_up(int x){
	for (; heap_fa(x)>0 && heap_v(x) > heap_v(heap_fa(x)); ){
		swap(nodes[x], nodes[heap_fa(x)]);
		swap(heap_p(x), heap_p(heap_fa(x)));
	}
}

void matcher::heap::pop(){
	if (len <= 0)
		return;
	swap(nodes[1], nodes[len]);
	swap(heap_p(1), heap_p(len));
	len--;
	heap_down(1);
}

void matcher::gen_ans_pairs_oldway() {
	ans_pairs.clear();
	vector<match_edge> match_edges;
	char * flag_a = new char[MAX_NODES], * flag = new char[MAX_NODES];
	memset(flag_a, 0, MAX_NODES);
	memset(flag, 0, MAX_NODES);
	for (int i=1; i <= G_a->num_nodes; i++)
		for (int j=1; j <= G->num_nodes; j++)
			match_edges.push_back(match_edge(i, j, sim_nodes[i][j]));
	sort(match_edges.begin(), match_edges.end());
	for (vector <match_edge> :: iterator it=match_edges.begin(); it!=match_edges.end(); it++) {
		//printf("\t %d -- %d : %g\n", it->u, it->v, it->w);
		if (!flag_a[it->u] && !flag[it->v]) {
			flag_a[it->u] = 1;
			flag[it->v] = 1;
			ans_pairs.push_back(*it);
		}
	}
	delete []flag_a;
	delete []flag;
}

void matcher::gen_ans_pairs() {
	clock_t time_start = clock();
	ans_pairs.clear();
	vector<match_edge> match_edges;
	char * flag_a = new char[MAX_NODES], * flag = new char[MAX_NODES];
	memset(flag_a, 0, MAX_NODES);
	memset(flag, 0, MAX_NODES);

	H = new heap(G_a->num_nodes, G->num_nodes, this);

	vector <int> nbs_a;
	vector <int> nbs;

	for (int u, v; H->len > 0; ) {

		// get heap.top and check conflicts
		u = H->nodes[1].u, v = H->nodes[1].v;
		if (flag_a[u] || flag[v])
			goto next_round;

		// add as an answer pair
		ans_pairs.push_back(match_edge(u, v, sim_nodes[u][v]));
		flag_a[u] = 1;
		flag[v] = 1;

		// add neighbors to heap H
		nbs_a.clear(), nbs.clear();
		for (vector <int>::iterator i=G_a->edges[u]->begin(); i!=G_a->edges[u]->end(); i++)
			if (!flag_a[*i])
				nbs_a.push_back(*i);
		for (vector <int>::iterator i=G_a->rev_edges[u]->begin(); i!=G_a->rev_edges[u]->end(); i++)
			if (!flag_a[*i])
				nbs_a.push_back(*i);
		for (vector <int>::iterator j=G->edges[v]->begin(); j!=G->edges[v]->end(); j++)
			if (!flag[*j])
				nbs.push_back(*j);
		for (vector <int>::iterator j=G->rev_edges[v]->begin(); j!=G->rev_edges[v]->end(); j++)
			if (!flag[*j])
				nbs.push_back(*j);
		for (vector <int> :: iterator i=nbs_a.begin(); i!=nbs_a.end(); i++)
			for (vector <int> :: iterator j=nbs.begin(); j!=nbs.end(); j++){
				sim_nodes[*i][*j] *= 1.05;
				if (sim_nodes[*i][*j] > 1)
					sim_nodes[*i][*j] = 1;
				H->heap_down(H->heap_pos[*i][*j]);
				H->heap_up(H->heap_pos[*i][*j]);
			}
next_round:
		H->pop();
	}

	delete H;
	delete []flag_a;
	delete []flag;
	fprintf(stderr, "answer pairs generated.\n\t%.2lf seconds.\n",
			(clock()-time_start)*1.0/CLOCKS_PER_SEC);
}

void matcher::gen_ans_pairs_iter() {
	clock_t time_start = clock();
	int cU=0;

	ans_pairs.clear();
	vector<match_edge> match_edges;
	char * flag_a = new char[MAX_NODES], * flag = new char[MAX_NODES];
	memset(flag_a, 0, MAX_NODES);
	memset(flag, 0, MAX_NODES);

	memcpy(last_round, sim_nodes, sizeof(sim_nodes));
	H = new heap(G_a->num_nodes, G->num_nodes, this);

	for (int u, v; H->len > 0; ) {
		vector <int> nbs_a;
		vector <int> nbs;

		// get heap.top and check conflicts
		u = H->nodes[1].u, v = H->nodes[1].v;
		if (flag_a[u] || flag[v])
			goto next_round;

		// add as an answer pair
		ans_pairs.push_back(match_edge(u, v, sim_nodes[u][v]));
		flag_a[u] = 1;
		flag[v] = 1;
		last_round[u][v] = sim_nodes[u][v] = 1;

		// update matrix
		nbs_a.clear(), nbs.clear();
		for (vector <int>::iterator i=G_a->rev_edges[u]->begin(); i!=G_a->rev_edges[u]->end(); i++)
			if (!flag_a[*i])
				nbs_a.push_back(*i);
		for (vector <int>::iterator j=G->rev_edges[v]->begin(); j!=G->rev_edges[v]->end(); j++)
			if (!flag[*j])
				nbs.push_back(*j);
		for (vector <int> :: iterator i=nbs_a.begin(); i!=nbs_a.end(); i++)
			for (vector <int> :: iterator j=nbs.begin(); j!=nbs.end(); j++){
				cU ++;
				calc_sim_nodes_wrapper(*i, *j, 0);
			}
#if MULTITHREAD
		thpool_wait(thpool);
#endif

		// apply changes to matrix
		for (vector <int> :: iterator i=nbs_a.begin(); i!=nbs_a.end(); i++)
			for (vector <int> :: iterator j=nbs.begin(); j!=nbs.end(); j++)
				last_round[*i][*j] = sim_nodes[*i][*j];
next_round:
		H->pop();
	}

	delete []flag_a;
	delete []flag;
	delete H;
	H = 0;

	fprintf(stderr, "answer pairs generated.\n\t%d updates, %.2lf seconds.\n",
			cU, (clock()-time_start)*1.0/CLOCKS_PER_SEC);
}

void matcher::print(FILE *ou) {
	for (vector<match_edge> :: iterator it=ans_pairs.begin(); it!=ans_pairs.end(); ++it) {
#if PRINT_SIMI
		fprintf(ou, "%d %d %g\n", it->u, it->v, it->w);
#else
		fprintf(ou, "%d %d\n", it->u, it->v);
#endif

	}
}
