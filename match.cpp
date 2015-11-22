//
//  match.cpp
//  deanonymization
//
//  Created by Shuyang Shi on 15/9/20.
//  Copyright © 2015年 Shuyang Shi. All rights reserved.
//

#include "match.hpp"
#include "match_vars.hpp"

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
					i > 2000);
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

bool cmp_deg(int a, int b) {
	return GA->out_deg(a) > GA->out_deg(b);
}

void matcher::gen_ans_pairs() {
	clock_t time_start = clock();
	ans_pairs.clear();
	vector<match_edge> match_edges;
	char * flag_a = new char[MAX_NODES], * flag = new char[MAX_NODES];
	char * fake_flag_a = new char[MAX_NODES], * fake_flag = new char[MAX_NODES];
	memset(flag_a, 0, MAX_NODES);
	memset(flag, 0, MAX_NODES);
	memset(fake_flag_a, 0, MAX_NODES);
	memset(fake_flag, 0, MAX_NODES);

	/*
	 * TODO
	 *	match nodes whose degree > deg_thrsd (deg_thrsd=3)
	 *	deal with nodes left (choose according to neighbor matching):
	 *		for node u left:
	 *			for node v in neighbors(u) and v is matched:
	 *				for r in neighbor(match[v]) and r is not matched:
	 *					weight[r] += simi[u][r]
	 *		match[u] = v, where weight[v] = max(weight[])
	 *
	 *	TODO
	 *		degree info
	 */

	const int deg_thrsd = 3;

	int * match = new int[MAX_NODES];

	for (int i=1; i <= G_a->num_nodes; i++)
		for (int j=1; j <= G->num_nodes; j++)
			match_edges.push_back(match_edge(i, j, sim_nodes[i][j]));
	sort(match_edges.begin(), match_edges.end());
	for (vector <match_edge> :: iterator it=match_edges.begin(); it!=match_edges.end(); it++) {
		if (!fake_flag_a[it->u] && !fake_flag[it->v]) {
			fake_flag_a[it->u] = 1;
			fake_flag[it->v] = 1;
			if (G_a->edges[it->u]->size() > deg_thrsd
					&& !flag_a[it->u] && !flag[it->v]) {
				flag_a[it->u] = 1;
				flag[it->v] = 1;
				match[it->u] = it->v;
				ans_pairs.push_back(*it);
			}
		}
	}

	fprintf(stderr, "First part: %d pairs.\n", ans_pairs.size());

	int TIME = 0;

refine:

	match_edges.clear();
	for (int i=1, t; i<=G_a->num_nodes; i++) {
		if (!flag_a[i]) {
			map <int, double> weight;
			for (vector <int> :: iterator j = G_a->edges[i]->begin(); j != G_a->edges[i]->end(); j++)
				if (flag_a[*j]) {
					for (vector <int> :: iterator k = G->rev_edges[match[*j]]->begin(); k != G->rev_edges[match[*j]]->end(); k++)
						if (!flag[*k])
							weight[*k] += max(sim_nodes[i][*k], 1e-10);
				}
			for (vector <int> :: iterator j = G_a->rev_edges[i]->begin(); j != G_a->rev_edges[i]->end(); j++)
				if (flag_a[*j]) {
					for (vector <int> :: iterator k = G->edges[match[*j]]->begin(); k != G->edges[match[*j]]->end(); k++)
						if (!flag[*k])
							weight[*k] += max(sim_nodes[i][*k], 1e-10);
				}
			for (map <int, double> :: iterator k = weight.begin(); k!=weight.end(); k++)
				match_edges.push_back(match_edge(i, k->first, k->second));
		}
	}
	sort(match_edges.begin(), match_edges.end());
	for (vector <match_edge> :: iterator it=match_edges.begin(); it!=match_edges.end(); it++) {
		if (it->w < 2.9e-20 && TIME < 3){
			TIME ++;
			goto refine;
		}
		if ( !flag_a[it->u] && !flag[it->v]) {
			flag_a[it->u] = 1;
			flag[it->v] = 1;
			match[it->u] = it->v;
			ans_pairs.push_back(*it);
			if (it->w < 1e-8)
				fprintf(stderr, "%d, %d: sim=%g, weight=%g, No. %d\n",
						it->u, it->v, sim_nodes[it->u][it->v],
						it->w, ans_pairs.size());
		}
	}

	for (int i=1; i<=G_a->num_nodes; i++)
		if (!flag_a[i]) {
			for (int j=1; j<=G->num_nodes; j++)
				if (!flag[j]) {
					flag_a[i] = flag[j] = 1;
					match[i] = j;
					ans_pairs.push_back(match_edge(i, j, 0));
					break;
				}
		}

	delete []match;
	delete []flag_a;
	delete []flag;
	delete []fake_flag_a;
	delete []fake_flag;
	fprintf(stderr, "answer pairs generated.\n\t%.2lf seconds.\n",
			(clock()-time_start)*1.0/CLOCKS_PER_SEC);
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


// =================================================================
// matcher::heap

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

