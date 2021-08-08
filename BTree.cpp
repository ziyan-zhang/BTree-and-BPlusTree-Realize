#include "BTree.h"
#include "struct.h"
#include <stdio.h>
#include <stdlib.h>

#define F_OK 0 /* Test for existence. */

#ifdef WIN32
	#include <io.h>
#endif

#ifdef LINUX
	#include <unistd.h>
#endif

btree_node* BTree::btree_node_new()
{
	btree_node* node = (btree_node *)malloc(sizeof(btree_node));
	if(NULL == node) {
		return NULL;
	}

	for(int i = 0; i < 2 * M -1; i++) {
		node->k[i] = 0;  //关键字的全为0, 由于后面给number of keys赋了0, 所以这里并不会引起错误访问
	}

	for(int i = 0; i < 2 * M; i++) {
		node->p[i] = NULL;  //p应该是pointer的意思, 指的是孩子节点(命名并不好, 容易跟parent混淆)
	}

	node->num = 0;  //应该指的number of keys = 0, 这样, 就算给key赋了初值0, 但是并访问不到
	node->is_leaf = true;     //默认为叶子 

	return node;
}

//TODO: 这个功能跟上一个btree_node_new是不是完全重复了. 属于过度封装了吧. 
btree_node* BTree::btree_create()
{
	btree_node *node = btree_node_new();
	if(NULL == node) {
		return NULL;
	}
	return node;
}

int BTree::btree_split_child(btree_node *parent, int pos, btree_node *child)
{
	//第1/2步, 依据要分裂的, 被装满的子节点child建立新节点new_child
	btree_node *new_child = btree_node_new();
	if(NULL == new_child) {  //这里返回NULL应该是没有内存了?
		return -1;
	}
	// 新节点的is_leaf与child相同，key的个数为M-1
	new_child->is_leaf = child->is_leaf;
	new_child->num = M - 1;

	// 将child后半部分的key拷贝给新节点
	// 位于正中间的(第M个)key的下标为M - 1, 因此拷贝从M下标开始, 到2M - 2下标(第M - 1, 也即末个key)结束, 共M - 1个key

	for(int i = 0; i < M - 1; i++) {
		new_child->k[i] = child->k[i+M];
	}

	// 如果child不是叶子，还需要把指针拷过去，指针比节点多1
	if(false == new_child->is_leaf) {
		for(int i = 0; i < M; i++) {
			new_child->p[i] = child->p[i+M];
		}
	}

	child->num = M - 1;  //孩子最后剩M-1个,因为是从满孩子节点执行分裂的.

	// 第2 / 2步, p_node的关键字和孩子节点分别右移, 并装入新的.
	// child的中间节点需要插入parent的pos处，更新parent的key和pointer
	for(int i = parent->num; i > pos; i--) {
		parent->p[i+1] = parent->p[i];
	}

	parent->p[pos+1] = new_child;  //新key的索引pos, 新child的是pos+1

	for(int i = parent->num - 1; i >= pos; i--) {
		parent->k[i+1] = parent->k[i];
	}
	parent->k[pos] = child->k[M-1];

	parent->num += 1;
	return 0;
}

// 执行该操作时，node->num < 2M-1. (插入从顶层开始, 如果顶层不满, 走进这里; 如果顶层满, 分裂后再走进这里)
void BTree::btree_insert_nonfull(btree_node *node, int target)
{
	if(1 == node->is_leaf) {
		// 如果在叶子中找到，直接插入
		int pos = node->num;
		while(pos >= 1 && target < node->k[pos-1]) {
			node->k[pos] = node->k[pos-1];
			pos--;
		}

		node->k[pos] = target;
		node->num += 1;
		btree_node_num+=1;  //TODO: 这个btree_node_num跟踪的是哪个值?
		
	} else {
		// 沿着查找路径下降
		int pos = node->num;
		while(pos > 0 && target < node->k[pos-1]) {
			pos--;
		}

		if(2 * M -1 == node->p[pos]->num) {
			// 如果路径上有满节点则分裂
			btree_split_child(node, pos, node->p[pos]);
			if(target > node->k[pos]) {
				pos++;
			}
		}

		btree_insert_nonfull(node->p[pos], target);
	}
}

//插入入口
//相对于python版实现, 这里:
//TODO: 没有事先判断key是否重复
//TODO: 没有设置新节点的number of keys, 不知道初始化的时候有没有定义, 回头看一下
//TODO: 这里不是void, 而是把root节点返回了. 
btree_node* BTree::btree_insert(btree_node *root, int target)
{
	if(NULL == root) {
		return NULL;
	}

	// 对根节点的特殊处理，如果根是满的，唯一使得树增高的情形
	// 先申请一个新的
	if(2 * M - 1 == root->num) {
		btree_node* node = btree_node_new();
		if(NULL == node) {
			return root;
		}

		node->is_leaf = 0;
		node->p[0] = root;
		btree_split_child(node, 0, root);
		btree_insert_nonfull(node, target);
		return node;
	} else {
		btree_insert_nonfull(root, target);    
		return root;
	}
}

// 将y，root->k[pos], z合并到y节点，并释放z节点，y,z各有M-1个节点
void BTree::btree_merge_child(btree_node *root, int pos, btree_node *y, btree_node *z)
{
	// 将z中节点拷贝到y的后半部分
	y->num = 2 * M - 1;
	for(int i = M; i < 2 * M - 1; i++) {
		y->k[i] = z->k[i-M];
	}
	y->k[M-1] = root->k[pos];// k[pos]下降为y的中间节点

	// 如果z非叶子，需要拷贝pointer
	if(false == z->is_leaf) {
		for(int i = M; i < 2 * M; i++) {
			y->p[i] = z->p[i-M];
		}
	}

	// k[pos]下降到y中，更新key和pointer
	for(int j = pos + 1; j < root->num; j++) {
		root->k[j-1] = root->k[j];
		root->p[j] = root->p[j+1];
	}

	root->num -= 1;
	free(z);  //TODO: free(z)是释放内存?
}

/************  删除分析   **************
*
在删除B树节点时，为了避免回溯，当遇到需要合并的节点时就立即执行合并，B树的删除算法如下：从root向叶子节点按照search规律遍历：
（1）  如果target在叶节点x中，则直接从x中删除target，情况（2）和（3）会保证当再叶子节点找到target时，肯定能借节点或合并成功而不会引起父节点的关键字个数少于t-1。
（2）  如果target在分支节点x中：
（a）  如果x的左分支节点y至少包含t个关键字，则找出y的最右的关键字prev，并替换target，并在y中递归删除prev。
（b）  如果x的右分支节点z至少包含t个关键字，则找出z的最左的关键字next，并替换target，并在z中递归删除next。
（c）  否则，如果y和z都只有t-1个关键字，则将targe与z合并到y中，使得y有2t-1个关键字，再从y中递归删除target。
（3）  如果关键字不在分支节点x中，则必然在x的某个分支节点p[i]中，如果p[i]节点只有t-1个关键字。
（a）  如果p[i-1]拥有至少t个关键字，则将x的某个关键字降至p[i]中，将p[i-1]的最大节点上升至x中。
（b）  如果p[i+1]拥有至少t个关键字，则将x个某个关键字降至p[i]中，将p[i+1]的最小关键字上升至x个。
（c）  如果p[i-1]与p[i+1]都拥有t-1个关键字，则将p[i]与其中一个兄弟合并，将x的一个关键字降至合并的节点中，成为中间关键字。
* 
*/

// 删除入口
btree_node* BTree::btree_delete(btree_node* root, int target)
{
	// 特殊处理，当根只有两个子女，切两个子女的关键字个数都为M-1时，合并根与两个子女
	// 这是唯一能降低树高的情形
	if(1 == root->num) {  //这种情况下要改根节点
		btree_node *y = root->p[0];
		btree_node *z = root->p[1];
		if(NULL != y && NULL != z &&  //这里应该是 与判断 从前向后走, 当y, z为空时直接返回0, 不会再继续往下找其num属性了, 就不会因此而发生错误. 
			M - 1 == y->num && M - 1 == z->num) {
				btree_merge_child(root, 0, y, z);
				free(root);
				btree_delete_nonone(y, target);
				return y;
		} else {  //两个节点都为空(有一个key. 要么两个孩子都为空(没孩子); 要么两个孩子都不为空. 不可能只有一个孩子.); 或者两个孩子的关键字有一个大于M-1(左右子树至少有一个够借, 树不用坍塌)
			btree_delete_nonone(root, target);
			return root;
		}
	} else {  //这种情况下不用改根节点
		btree_delete_nonone(root, target);	
		return root;
	}
}

// root至少有个t个关键字，保证不会回溯
void BTree::btree_delete_nonone(btree_node *root, int target)
{
	if(true == root->is_leaf) {
		// 如果在叶子节点，直接删除
		int i = 0;
		while(i < root->num && target > root->k[i]) i++;
		if(target == root->k[i]) {
			for(int j = i + 1; j < 2 * M - 1; j++) {
				root->k[j-1] = root->k[j];
			}
			root->num -= 1;
			
			btree_node_num-=1;
			
		} else {
			printf("target not found\n");
		}
	} else {  //root不是叶子结点
		int i = 0;
		btree_node *y = NULL, *z = NULL;
		while(i < root->num && target > root->k[i]) i++;
		if(i < root->num && target == root->k[i]) {
			// 如果在分支节点找到target
			y = root->p[i];
			z = root->p[i+1];
			if(y->num > M - 1) {
				// 如果左分支关键字多于M-1，则找到左分支的最右节点prev，替换target
				// 并在左分支中递归删除prev,情况2（a)
				int pre = btree_search_predecessor(y);
				root->k[i] = pre;
				btree_delete_nonone(y, pre);
			} else if(z->num > M - 1) {
				// 如果右分支关键字多于M-1，则找到右分支的最左节点next，替换target
				// 并在右分支中递归删除next,情况2(b)
				int next = btree_search_successor(z);
				root->k[i] = next;
				btree_delete_nonone(z, next);
			} else {
				// 两个分支节点数都为M-1，则合并至y，并在y中递归删除target,情况2(c)
				btree_merge_child(root, i, y, z);
				btree_delete(y, target);
			}
		} else {
			// 在分支没有找到，肯定在分支的子节点(p[i])中
			y = root->p[i];
			if(i < root->num) {  //如果i不是最后一个孩子节点, 也即i孩子在最后一个关键字的左边, 或更左的位置
				z = root->p[i+1];
			}
			btree_node *p = NULL;
			if(i > 0) {
				p = root->p[i-1];  //如果i不是第一个孩子节点(下标为0), 那么定义一个前节点指针p. 
			}

			if(y->num == M - 1) {  //这三种情况是: 如果确定了要删除的节点在y分支上, 先满足其关键字大于m-1, 防止向上级节点回溯, 出现(不拆了树重建的情况下)无法操作的情况, 
				//因为不一级级保证节点数大于m-1, 上级也没法被借, 或者借了还要往更上级借关键字, 还牵涉到孩子的情况, 导致应该是无解的局面, 除非拆树重建. 
				if(i > 0 && p->num > M - 1) {
					// 左邻接节点关键字个数大于M-1(左兄弟够借)
					//情况3(a)
					btree_shift_to_right_child(root, i-1, p, y);
				} else if(i < root->num && z->num > M - 1) {
					// 右邻接节点关键字个数大于M-1(右兄弟够借)
					// 情况3(b)
					btree_shift_to_left_child(root, i, y, z);
				} else if(i > 0) {
					// 情况3（c)
					//i > 0, 左兄弟存在. 跟左兄弟合并
					btree_merge_child(root, i-1, p, y);
					y = p;  //TODO: 这里y换成左兄弟用途是什么?
				} else {
					// 情况3(c)
					//i=0, 只能跟右兄弟合并. 
					btree_merge_child(root, i, y, z);
				}
				btree_delete_nonone(y, target);  //这个时候查找已经下降了一层, 从y中删除. 转回到 "如果在分支节点找到target" 的情况
			} else {
				btree_delete_nonone(y, target);
			}
		}

	}
}

//寻找rightmost，以root为根的最大关键字
int BTree::btree_search_predecessor(btree_node *root)  //predecessor前驱, 前驱是左兄弟最后面
{
	btree_node *y = root;
	while(false == y->is_leaf) {
		y = y->p[y->num];
	}
	return y->k[y->num-1];
}

// 寻找leftmost，以root为根的最小关键字
int BTree::btree_search_successor(btree_node *root)  //successor后继, 后继是右兄弟最前面
{
	btree_node *z = root;
	while(false == z->is_leaf) {
		z = z->p[0];
	}
	return z->k[0];
}

// z向y借节点，将root->k[pos]下降至z，将y的最大关键字上升至root的pos处
void BTree::btree_shift_to_right_child(btree_node *root, int pos, 
	btree_node *y, btree_node *z)
{
	z->num += 1;
	for(int i = z->num -1; i > 0; i--) {
		z->k[i] = z->k[i-1];
	}
	z->k[0]= root->k[pos];
	root->k[pos] = y->k[y->num-1];

	if(false == z->is_leaf) {
		for(int i = z->num; i > 0; i--) {
			z->p[i] = z->p[i-1];
		}
		z->p[0] = y->p[y->num];
	}

	y->num -= 1;
}

// y向借节点，将root->k[pos]下降至y，将z的最小关键字上升至root的pos处
void BTree::btree_shift_to_left_child(btree_node *root, int pos,
	btree_node *y, btree_node *z)
{
	y->num += 1;
	y->k[y->num-1] = root->k[pos];
	root->k[pos] = z->k[0];

	for(int j = 1; j < z->num; j++) {  //这个作者的思路是, 哪些位置的值被移动, 就索引哪些位置
		z->k[j-1] = z->k[j];  //对应的, 等式左边是到达的位置的索引, 右边是出发的位置的索引, 也就是j本身. 
	}

	if(false == z->is_leaf) {
		y->p[y->num] = z->p[0];
		for(int j = 1; j <= z->num; j++) {
			z->p[j-1] = z->p[j];
		}
	} 

	z->num -= 1;
}

void BTree::btree_inorder_print(btree_node *root) 
{
	if(NULL != root) {
		btree_inorder_print(root->p[0]);
		for(int i = 0; i < root->num; i++) {
			printf("%d ", root->k[i]);
			btree_inorder_print(root->p[i+1]);
		}
	}
}

void BTree::btree_level_display(btree_node *root) 
{
	// just for simplicty, can't exceed 200 nodes in the tree
	btree_node *queue[200] = {NULL};
	int front = 0;
	int rear = 0;

	queue[rear++] = root;

	while(front < rear) {
		btree_node *node = queue[front++];

		printf("[");
		for(int i = 0; i < node->num; i++) {
			printf("%d ", node->k[i]);
		}
		printf("]");

		for(int i = 0; i <= node->num; i++) {
			if(NULL != node->p[i]) {
				queue[rear++] = node->p[i];               
			}
		}
	}
	printf("\n");
}

void BTree::Save(btree_node *root) 
{
	/*
	storage_struct ss;
	
	// malloc len space
	ss.len = btree_node_num;
	ss.snode = (storage_node *)malloc(sizeof(storage_node)*ss.len);
	
	ss.snode[0].bnode = *root;
	for(int i=1;i<ss.len;i++)
	{
		btree_node *node = btree_node_new();
		if(NULL == node) {
			return;
		}
	}
	
//	fwrite(&ss,sizeof(ss),1,pfile);
*/
}

BTree::BTree(void)
{	
	// 先判断文件是否存在
 	// windows下，是io.h文件，linux下是 unistd.h文件 
  	// int access(const char *pathname, int mode);
   	if(-1==access("define.Bdb",F_OK))
    {
	   	// 不存在 ,创建 
	//   	pfile = fopen("bstree.b","w");
   		roots = btree_create();
	}
 	else
  	{
//	   	pfile = fopen("bstree.b","r+");
	   	roots = btree_create();
//	   	fread(roots,sizeof(roots),1,pfile);
   	}
	
}


BTree::~BTree(void)
{
//	fclose(pfile); 
}

