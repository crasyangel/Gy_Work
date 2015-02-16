#ifndef _NGOD_GW_BISORTT_H_
#define _NGOD_GW_BISORTT_H_

#ifdef __cplusplus
extern "C" {
#endif

//********************** Include Files ***************************************
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#include "rocme_porting_typedef.h"
#include "vod_ext_debug.h"

//********************** struct typedef *************************************

/*二叉查找树的二叉链表结点结构定义 */
typedef  struct BiTNode    /* 结点结构 */
{
    INT32_T data[2];    /* 结点数据 */
    struct BiTNode *lchild, *rchild; /* 左右孩子指针 */
} BiTNode, *BiTree;


//********************** Global Functions *************************************


//中序遍历打印树,打印出来的将是排好序的结果
void DisplayBST(const BiTree T);

//后序遍历删除树
void DeleteBST(BiTree *T);

//在二叉排序树中查找某个值是否在某个区间内
bool SearchBST_Key(const BiTree T, const INT32_T key);

//创建QAMname区间组成的二叉排序树
//这里的buffer是这样的[[23001,23010],[23027,23036],[24821,24830],[24847,24856]]
bool CreateQamNameBiTree(BiTree *T, CHAR_T *qambuf);


#ifdef __cplusplus
}
#endif

#endif  /*_NGOD_GW_BISORTT_H_*/

