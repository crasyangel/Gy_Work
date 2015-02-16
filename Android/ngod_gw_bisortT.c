#ifndef _NGOD_GW_BISORTT_C_
#define _NGOD_GW_BISORTT_C_

#include "ngod_gw_bisortT.h"


/********************** Local define *****************************************/
//屏蔽循环打印消息
//#define PRINT_ALWAYS_MSG
#ifdef PRINT_ALWAYS_MSG
	#define VOD_EXT_ALWAYS(x) VOD_EXT_INFO(x)
#else
	#define VOD_EXT_ALWAYS(x)
#endif


/*以下
Data_Cmp、DisplayBST、SearchBST_Data、SearchBST_Parent、Delete_Node
InsertBST、DeleteBST、SearchBST_Key、strtok_str以及CreateQamNameBiTree
都是二叉区间搜索排序树相关的函数
该算法基于二叉搜索树，并根据元素是区间的特点进行了调整
此部分可以独立出去作为单独的模块

注意CreateQamNameBiTree中的分隔符可以根据实际情况调整
这里的buffer是这样的[[23001,23010],[23027,23036],[24821,24830],[24847,24856]]
这里用的"],[" 是一个组合字符串作为整体分隔符
所以对windows下的strtok进行了调整
由于使用了静态局部变量，并不是线程安全的
多线程版本请修改UNIX下的strtok_r，本来还可以直接使用strsep
但经测试Android 4.2 下的strsep对于多字符不正确
*/

/*
比较两个区间的大小，返回结果如下
如果data1=data2，返回0
如果data1>data2，返回1
如果data1<data2，返回-1
如果data1包含data2，返回2
如果data2包含data1，返回-2

*/
static INT32_T Data_Cmp(const INT32_T* data1, const INT32_T* data2)
{
	if(data1[0]>data2[1])
	{
		return 1;
	}
	else if (data1[1]<data2[0])
	{
		return -1;
	}
	else if (data1[0] == data2[0] && data1[1] == data2[1])
	{
		return 0;
	}
	//有重合的情况，不可能再有相等的情况
	else if(data1[0]<data2[0])
	{
		if(data1[1]<data2[1]) 
		{
			return -1;
		}
		else if(data1[1]>=data2[1])
		{
			return 2;
		}
	}
	else if (data1[0]>data2[0])
	{
		if(data1[1]>data2[1])
		{
			return 1;
		}
		else if(data1[1]<=data2[1]) 
		{
			return -2;
		}
	}
	else if (data1[0] == data2[0])
	{
		if(data1[1] > data2[1])
		{
			return 2;
		}
		else if(data1[1] < data2[1])
		{
			return -2;
		}
	}
	
	return 0;
}

static void PrintBST(const CHAR_T* format, ...)
{
	static int count = 0;
	va_list args;
	va_start(args, format);

	vprintf(format, args);
	printf("\t\t");
	count++;
	if(count >= 5)
	{
		printf("\n");
		count = 0;
	}
	
	va_end(args);
}

//中序遍历打印树,打印出来的将是排好序的结果
void DisplayBST(const BiTree T)
{ 
	if(T->lchild)
	{
    	DisplayBST(T->lchild); //打印左子树
    }
    
    if(T) /* 节点不为空，则打印 */ 
    {
		PrintBST("[%d,%d]", T->data[0], T->data[1]);
    }
    
	if(T->rchild)
	{
    	DisplayBST(T->rchild);//打印右子树
    }
}

//在二叉排序树中查找区间节点
static INT32_T SearchBST_Data(const BiTree T, const INT32_T* data, const BiTree p, BiTNode **f) 
{  
    if (!T)    /*  查找不成功 ，返回父节点*/
    { 
        *f = p;  
        return -1; 
    } 
    /*  
     		查找成功 ，返回当前节点
     		-2表示旧节点的data包含新data，则什么也不用做
     		2说明新data包含旧节点的data
   	 */
    else if(2 == Data_Cmp(data, T->data))
    {
        *f = T;  
        return 1; 
    }
    else if (0 == Data_Cmp(data, T->data) || -2 == Data_Cmp(data, T->data))
    { 
        *f = T; 
		return 0; 
    } 
    else if (-1 == Data_Cmp(data, T->data)) 
    {
        return SearchBST_Data(T->lchild, data, T, f);  /*  在左子树中继续查找 */
    }
    else if (1 == Data_Cmp(data, T->data)) 
    {
        return SearchBST_Data(T->rchild, data, T, f);  /*  在右子树中继续查找 */
    }
    
	return 0; 
}

static void SearchBST_Parent(const BiTree T, const BiTNode *f, BiTNode **p) 
{
	if(!f)
	{
		return;
	}
	
	if(T)
	{
		if(f == T->lchild)
		{
			*p = T;
			return;
		}
		else if (f == T->rchild)
		{
			*p = T;
			return;
		}
		else
		{
			SearchBST_Parent(T->lchild, f, p);
			SearchBST_Parent(T->rchild, f, p);
		}
	}
}

static void Delete_Node(BiTree T, BiTNode **f)
{
    BiTree q = NULL;
    BiTree s = NULL;

    if(!f)
    {
    	return;
    }
    
    /* 右子树空则只需重接它的左子树
    		待删结点是叶子也走此分支*/
    if((*f)->rchild==NULL) 
    {
		SearchBST_Parent(T, *f, &q);
		if(q)
		{
	    	if(*f == q->lchild) //左孩子
	    	{
	    		q->lchild = (*f)->lchild; free(*f);*f=NULL;
	    	}
	    	else if(*f == q->rchild)//右孩子
	    	{
	    		q->rchild = (*f)->lchild; free(*f);*f=NULL;
	    	}
    	}
    	else //父节点为空，是根节点
    	{
			q=*f; *f=(*f)->lchild; free(q);q=NULL;
    	}
    	
    }
    else if((*f)->lchild==NULL) /* 只需重接它的右子树 */
    {
		SearchBST_Parent(T, *f, &q);
		if(q)
		{
	    	if(*f == q->lchild) //左孩子
	    	{
	    		q->lchild = (*f)->rchild; free(*f);*f=NULL;
	    	}
	    	else if(*f == q->rchild)//右孩子
	    	{
	    		q->rchild = (*f)->rchild; free(*f);*f=NULL;
	    	}
    	}
    	else //父节点为空，是根节点
    	{
        	q=*f; *f=(*f)->rchild; free(q);q=NULL;
        }
    }
    else /* 左右子树均不空 */
    {
    	//f右子树不为空，则后继就是其右子树的最小值
        q=*f; s=(*f)->rchild; 
         /*
         	转右，向左走到尽头（找待删结点的后继）
         	此节点则是大于当前节点的最小节点
        	 */
        while(s->lchild)
        {
            q=s;
            s=s->lchild;
        }
        /*  
        	s指向被删结点的直接后继，q是其父节点
        	s无左子树
        	将被删结点后继的值取代被删结点的值
        	然后删除被删结点后继
        	*/
        memcpy((*f)->data, s->data, sizeof((*f)->data)); 
        if(q!=*f)
        {
         	/*如果q不等于f ，则s是q的左子树， 重接q的左子树 */ 
            q->lchild=s->rchild;
        }
        else
        {
        	/*如果q等于f ，则s是q(f)的右子树， 重接q的右子树 */ 
            q->rchild=s->rchild;
        }
        free(s);
        s=NULL;
    }
}


//在二叉排序树中插入节点
static void InsertBST(BiTree *T, const INT32_T* data) 
{  
    BiTNode *f = NULL;
    BiTNode *s = NULL;
    INT32_T seach_ret = SearchBST_Data(*T, data, NULL, &f);
    
    if (-1 == seach_ret) /* 查找不成功，插入节点 */
    {
        s = (BiTNode *)malloc(sizeof(BiTNode));
        memcpy(s->data, data, sizeof(s->data));  
        s->lchild = s->rchild = NULL;  
        if (!f) 
        {
			VOD_EXT_ALWAYS(("insert [%d,%d] as root!!!\n", s->data[0], s->data[1]));
            *T = s;            /*  插入s为新的根结点 */
		}
        else if (-1 == Data_Cmp(data, f->data)) 
        {
			VOD_EXT_ALWAYS(("insert [%d,%d] as left child of [%d,%d]!!!\n",
					s->data[0], s->data[1], f->data[0], f->data[1]));
            f->lchild = s;    /*  插入s为左孩子 */
		}
        else if (1 == Data_Cmp(data, f->data)) 
        {
			VOD_EXT_ALWAYS(("insert [%d,%d] as right child of [%d,%d]!!!\n",
					s->data[0], s->data[1], f->data[0], f->data[1]));
            f->rchild = s;  /*  插入s为右孩子 */
        }
    } 
    else if(1 == seach_ret)
    {
    	//需要先删除旧节点，再插入新data
        if (f) 
        {
			VOD_EXT_ALWAYS(("need to delete [%d,%d] before insert [%d,%d]\n", 
					f->data[0], f->data[1], data[0], data[1]));
            Delete_Node(*T, &f);
            //递归插入，防止直接插入会冲突
            InsertBST(T, data);
		}
		else
		{
			VOD_EXT_ERRO(("insert [%d,%d] error!!!\n", data[0], data[1]));
		}
    }
	else if(0 == seach_ret)
    {
    	/*  树中已有关键字相同的结点，不再插入 */
       	VOD_EXT_ALWAYS(("no need to insert [%d,%d], already have [%d,%d]!!!\n"
       				, data[0], data[1], f->data[0], f->data[1]));  
    }
}

void DeleteBST(BiTree *T)//后序遍历删除树
{
	if((*T)->lchild)
	{
    	DeleteBST(&(*T)->lchild); //删除左子树
    }
    
	if((*T)->rchild)
	{
    	DeleteBST(&(*T)->rchild);//删除右子树
    }
    
    if(*T) /* 节点不为空，则删除 */ 
    {
		//PrintBST("[%d,%d]", (*T)->data[0], (*T)->data[1]);
        free(*T);
        *T = NULL;
    }
}

//在二叉排序树中查找某个值是否在某个区间内
bool SearchBST_Key(const BiTree T, const INT32_T key) 
{  
    if (!T)    /*  查找不成功 ，返回false*/
    { 
        return false; 
    }
    else if (key >= T->data[0] && key <= T->data[1]) /*  查找成功 ，返回true*/
    { 
		VOD_EXT_WARN(("!!!!!!!!! the %d is in [%d,%d] !!!!!!!!!\n", key, T->data[0], T->data[1]));
		return true; 
    } 
    else if (key < T->data[0]) 
    {
        return SearchBST_Key(T->lchild, key);  /*  在左子树中继续查找 */
    }
    else if (key > T->data[1]) 
    {
        return SearchBST_Key(T->rchild, key);  /*  在右子树中继续查找 */
    }
    
	return true; 
}

//微软strtok_r实现改动
//可以用多个字符作为一个组合分隔符
static char*  strtok_str(char* string_org,const char* demial)
{
	static unsigned char* last; //保存分隔后剩余的部分
	unsigned char* str;         //返回的字符串
	const unsigned char* ctrl = (const unsigned char*)demial;//分隔字符

	//把分隔字符放到一个索引表中。
	//定义32是因为ASCII字符表最多是0~255个，
	//也是说用最大的255右移3位，
	//也就是除以8一定会是32中的一个数。
	const int rownum = strlen(demial);
	const int size_type = sizeof(unsigned char);

    //列固定的动态二维数组
    //先申请内存，其中point_size * row表示存放row个行指针
    unsigned char *map_tmp = (unsigned char *) malloc(size_type * rownum * 32);
    unsigned char (*map)[32] = (unsigned char(*)[32])map_tmp;

	//unsigned char map[][32];
	int count = 0;
	int row = 0;

	//把map全部清为0，之后相与的操作，与0的都为0
	for(row = 0; row < rownum; row++)
	{
		for (count =0; count <32; count++)
		{
			map[row][count] = 0;
		}
	}

	//把匹配字符放入表中
	//放入的算法是把匹配字符右移3位，相当于除以8，的数值 并上(加上)
	//匹配字符与7，得到低3位，得出的结果，是把1左移的位数。
	//最大左移位数是7，也就是所表示的最大值是128

	for(row = 0; row < rownum; row++)
	{
		do
		{
			map[row][*ctrl >> 3] |= (1 << (*ctrl & 7));
		} while (*ctrl++);
		ctrl -= rownum;
	}

	//原始字符串是否为空，如果为空表示第二次获取剩余字符的分隔部分。
	if (string_org)
	{
		str = (unsigned char*)string_org;
	}
	else
	{
		str = last;
	}

	int  match = 0;
	row = 0;
	//略过 最开头匹配字符和重复匹配字符，注意部分匹配的情况
	while ((map[row][*str >> 3] & (1 << (*str & 7)))  && *str)
	{
		str++;
		match++;
		row++;
		if(rownum == row)
		{
			row = 0;
		}
	}
	str -= match%rownum;

	//重置需要扫描的字符串
	string_org = (char*)str;

	bool exitflag = false;
	//开始扫描
	for (;*str && !exitflag; str++)
	{
		row = 0;
		while ( map[row][*str >> 3] & (1 << (*str & 7)))
	    {
	    	str++;
			row++;
			if(rownum == row)
			{
				*(str-rownum) = '\0';//当找到时，把匹配字符填为0，并且把str指向下一位。
				exitflag = true;
				break; //退出循环
			}
	    }
	}
    str--; //跳出循环后多删了一次

	//清空临时数组
    free(map_tmp);
    map_tmp = NULL;
    
	last =str; // 把剩余字符串的指针保存到静态变量last中。
	if (string_org == (char*)str)
	{
		return NULL; //没有找到，也就是没有移动指针的位置，返回NULL
	}
	else
	{
		return string_org; //找到了，返回之前字符串的头指针
	}
}


bool CreateQamNameBiTree(BiTree *T, CHAR_T *qambuf)
{
	CHAR_T *p = NULL;
	CHAR_T *q = NULL;
	CHAR_T *token = NULL;
	INT32_T data_qam[2];
	INT32_T qam_num = 0;
	
	if(!qambuf)
	{
		VOD_EXT_ERRO(("you transfer a null buf!!\n"));
		return false;
	}

	p = strstr(qambuf, "[["); //区间开始
	while(p)
	{
	    q = p+strlen("[[");
		token = strtok_str(q, "],[");

		while(token != NULL)
		{
			sscanf(token, "%d,%d", &data_qam[0], &data_qam[1]);
			InsertBST(T, data_qam);
			qam_num++;
			if(strstr(token, "]]"))
			{
				VOD_EXT_DBUG(("reach the end descriptor!!\n"));
				break;
			}
			token = strtok_str(NULL, "],[");
		}
		p = strstr(q, "[["); //区间开始
	}
	VOD_EXT_DBUG(("saved %d qam_name region!!!\n", qam_num));

	if(0 == qam_num)
	{
		return false;
	}

	return true;
}


#endif  /*_NGOD_GW_BISORTT_C_*/
