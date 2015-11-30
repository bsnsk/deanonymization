# De-anonymization
By bsnsk (And Jialin L) | Started @ Sept, 2015
***

## Contents
- [Project Description](#desc)
- [Previous Consideration](#prev)
- [Current Status](#curr)
	- [Version 1: DNMC](#dnmc)
	- [Version 2: BTCH](#btch)
- [理解与阐述](#unde)
	- [Baseline Algorithm](#base)
	- [Improvement Algorithm](#impr)
- [Next Step](#next)


## Description <span id="desc"></span>
Social network de-anonymization (provided with an anonymized graph and a crawled graph)

## Previous Consideration <span id="prev"></span>

- 思路
	- 对度数小的点，原匹配算法不使用。利用已匹配的邻居信息判断，并分多次逐渐匹配所有点
- 流程
	1. 原算法匹配，结果中删去涉及到度数 `out_deg + in_deg< thrsd = 6`的点，删去的点用精炼算法
	2. 设置标志 TIME = 0 （用以控制次数）
	3. 按照已匹配邻居通路数，然后按照weight排序然后贪心选取
	4. weight增加的时候增加max(TINY, sim_nodes[u][v])，其中TINY为sim_nodes矩阵中最小的非零值
	5. 将weight从大到小排序，依次处理
		1. 如果该次已经匹配10条边，
		则记录TIME++，并回到第2步，重新计算weight矩阵
		2. 否则，匹配该边并继续
- 效果（略）

## Current Status: Use 'refinement' to match most vertices <span id="curr"></span>

目前分为DNMC和BTCH两个版本。(通过`match.hpp`中的宏定义来控制)

### DNMC: 每次匹配之后动态维护weights矩阵 <span id="dnmc"></span>
- 想法
	- 利用原来的迭代算法，计算出seeds（一定比例）。
		(由该算法的特点可以知道seeds的度数一定较大且正确性较高。)
	- 从seeds出发迭代匹配，每次匹配weights值最大的一对，匹配以后动态更新weights
		- 更新的时候，使用的公式是`weights[x][y] += sim_nodes[x][y] * DECAY(iterno)`
		- 其中，函数`DECAY`是一个不上升函数，自变量是迭代轮数`iterno`
	- 剩余未匹配的少量点用Baseline算法匹配。
- 效果
	- 1 graph with 7500 vertices graph (50% overlap, precision upper bound 66.7%)
		- Baseline: 45.83%
		- `DECAY(i)=1`
			- `perc_thrsd=0.07`, `DECAY(i)=1`: 52.45%. 提升6.62个百分点
			- `perc_thrsd=0.05`, `DECAY(i)=1`: 51.56%. 提升5.73个百分点
			- `perc_thrsd=0.10`, `DECAY(i)=1`: 51.52%. 提升5.69个百分点
		- `DECAY(i) = (1 - 1e-4) ^ i`
			- `perc_thrsd=0.07`, `DECAY(i) = (1 - 1e-4) ^ i`: 52.19%, 提升6.36个百分点
			- `perc_thrsd=0.05`, `DECAY(i) = (1 - 1e-4) ^ i`: 52.41%, 提升6.58个百分点


### BTCH: 每考虑若干对匹配之后重建weights矩阵 <span id="btch"></span>

- 思路
	- 用baseline算法得到占比重`PERC_THRSD`的匹配对
	- 用refine算法，一次`iter_cond`对逐步匹配剩余点对
	- 余下的点对用baseline算法匹配
- 效果
	- Graph: 7500 vertices, 50%overlap (upper bound precision: 66.7%)
		- Baseline: 45.83%
		- `PERC_THRSD=0.05`, `iter_cond=10`: 55.03%
			- 提升了9.20个百分点

### 相关理解与阐述 <span id="unde"></span>
####  Baseline 算法 <span id="base"></span>
该算法在计算的时候未将点对的相似权值归一化，事实上，度数大的点对权值大。
事实上，度数大的点对，匹配的置信度相对也高。

因此，Baseline算法实际上是将邻居相似度和度数置信度统一在一个实数中进行迭代。

#### 改进算法 <span id="impr"></span>
将Baseline得到的相似度矩阵`sim_nodes`作为基础，用Baseline匹配的前5％的点作为已匹配的种子，重新构造`weights`矩阵。
`weights`矩阵利用了匹配的中间结果，利用已经匹配的邻居数量作为权值，求匹配对的加权`sum(sim_nodes)`。

其中，两个版本的区别在于
- DNMC: 动态维护`weights`，用堆维护，每次匹配以后都更新（单线程运行）
- BTCH: 每次处理`NUM_PER_ITER`对候选匹配对之后，重建`weights`矩阵（重建过程可并行）

## Next Step <span id="next"></span>

- 多种匿名方法实验
- 准备整理？
