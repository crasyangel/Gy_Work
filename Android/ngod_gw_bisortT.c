#ifndef _NGOD_GW_BISORTT_C_
#define _NGOD_GW_BISORTT_C_

#include "ngod_gw_bisortT.h"


/********************** Local define *****************************************/
//����ѭ����ӡ��Ϣ
//#define PRINT_ALWAYS_MSG
#ifdef PRINT_ALWAYS_MSG
	#define VOD_EXT_ALWAYS(x) VOD_EXT_INFO(x)
#else
	#define VOD_EXT_ALWAYS(x)
#endif


/*����
Data_Cmp��DisplayBST��SearchBST_Data��SearchBST_Parent��Delete_Node
InsertBST��DeleteBST��SearchBST_Key��strtok_str�Լ�CreateQamNameBiTree
���Ƕ�������������������صĺ���
���㷨���ڶ�����������������Ԫ����������ص�����˵���
�˲��ֿ��Զ�����ȥ��Ϊ������ģ��

ע��CreateQamNameBiTree�еķָ������Ը���ʵ���������
�����buffer��������[[23001,23010],[23027,23036],[24821,24830],[24847,24856]]
�����õ�"],[" ��һ������ַ�����Ϊ����ָ���
���Զ�windows�µ�strtok�����˵���
����ʹ���˾�̬�ֲ��������������̰߳�ȫ��
���̰߳汾���޸�UNIX�µ�strtok_r������������ֱ��ʹ��strsep
��������Android 4.2 �µ�strsep���ڶ��ַ�����ȷ
*/

/*
�Ƚ���������Ĵ�С�����ؽ������
���data1=data2������0
���data1>data2������1
���data1<data2������-1
���data1����data2������2
���data2����data1������-2

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
	//���غϵ������������������ȵ����
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

//���������ӡ��,��ӡ�����Ľ����ź���Ľ��
void DisplayBST(const BiTree T)
{ 
	if(T->lchild)
	{
    	DisplayBST(T->lchild); //��ӡ������
    }
    
    if(T) /* �ڵ㲻Ϊ�գ����ӡ */ 
    {
		PrintBST("[%d,%d]", T->data[0], T->data[1]);
    }
    
	if(T->rchild)
	{
    	DisplayBST(T->rchild);//��ӡ������
    }
}

//�ڶ����������в�������ڵ�
static INT32_T SearchBST_Data(const BiTree T, const INT32_T* data, const BiTree p, BiTNode **f) 
{  
    if (!T)    /*  ���Ҳ��ɹ� �����ظ��ڵ�*/
    { 
        *f = p;  
        return -1; 
    } 
    /*  
     		���ҳɹ� �����ص�ǰ�ڵ�
     		-2��ʾ�ɽڵ��data������data����ʲôҲ������
     		2˵����data�����ɽڵ��data
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
        return SearchBST_Data(T->lchild, data, T, f);  /*  ���������м������� */
    }
    else if (1 == Data_Cmp(data, T->data)) 
    {
        return SearchBST_Data(T->rchild, data, T, f);  /*  ���������м������� */
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
    
    /* ����������ֻ���ؽ�����������
    		��ɾ�����Ҷ��Ҳ�ߴ˷�֧*/
    if((*f)->rchild==NULL) 
    {
		SearchBST_Parent(T, *f, &q);
		if(q)
		{
	    	if(*f == q->lchild) //����
	    	{
	    		q->lchild = (*f)->lchild; free(*f);*f=NULL;
	    	}
	    	else if(*f == q->rchild)//�Һ���
	    	{
	    		q->rchild = (*f)->lchild; free(*f);*f=NULL;
	    	}
    	}
    	else //���ڵ�Ϊ�գ��Ǹ��ڵ�
    	{
			q=*f; *f=(*f)->lchild; free(q);q=NULL;
    	}
    	
    }
    else if((*f)->lchild==NULL) /* ֻ���ؽ����������� */
    {
		SearchBST_Parent(T, *f, &q);
		if(q)
		{
	    	if(*f == q->lchild) //����
	    	{
	    		q->lchild = (*f)->rchild; free(*f);*f=NULL;
	    	}
	    	else if(*f == q->rchild)//�Һ���
	    	{
	    		q->rchild = (*f)->rchild; free(*f);*f=NULL;
	    	}
    	}
    	else //���ڵ�Ϊ�գ��Ǹ��ڵ�
    	{
        	q=*f; *f=(*f)->rchild; free(q);q=NULL;
        }
    }
    else /* �������������� */
    {
    	//f��������Ϊ�գ����̾���������������Сֵ
        q=*f; s=(*f)->rchild; 
         /*
         	ת�ң������ߵ���ͷ���Ҵ�ɾ���ĺ�̣�
         	�˽ڵ����Ǵ��ڵ�ǰ�ڵ����С�ڵ�
        	 */
        while(s->lchild)
        {
            q=s;
            s=s->lchild;
        }
        /*  
        	sָ��ɾ����ֱ�Ӻ�̣�q���丸�ڵ�
        	s��������
        	����ɾ����̵�ֵȡ����ɾ����ֵ
        	Ȼ��ɾ����ɾ�����
        	*/
        memcpy((*f)->data, s->data, sizeof((*f)->data)); 
        if(q!=*f)
        {
         	/*���q������f ����s��q���������� �ؽ�q�������� */ 
            q->lchild=s->rchild;
        }
        else
        {
        	/*���q����f ����s��q(f)���������� �ؽ�q�������� */ 
            q->rchild=s->rchild;
        }
        free(s);
        s=NULL;
    }
}


//�ڶ����������в���ڵ�
static void InsertBST(BiTree *T, const INT32_T* data) 
{  
    BiTNode *f = NULL;
    BiTNode *s = NULL;
    INT32_T seach_ret = SearchBST_Data(*T, data, NULL, &f);
    
    if (-1 == seach_ret) /* ���Ҳ��ɹ�������ڵ� */
    {
        s = (BiTNode *)malloc(sizeof(BiTNode));
        memcpy(s->data, data, sizeof(s->data));  
        s->lchild = s->rchild = NULL;  
        if (!f) 
        {
			VOD_EXT_ALWAYS(("insert [%d,%d] as root!!!\n", s->data[0], s->data[1]));
            *T = s;            /*  ����sΪ�µĸ���� */
		}
        else if (-1 == Data_Cmp(data, f->data)) 
        {
			VOD_EXT_ALWAYS(("insert [%d,%d] as left child of [%d,%d]!!!\n",
					s->data[0], s->data[1], f->data[0], f->data[1]));
            f->lchild = s;    /*  ����sΪ���� */
		}
        else if (1 == Data_Cmp(data, f->data)) 
        {
			VOD_EXT_ALWAYS(("insert [%d,%d] as right child of [%d,%d]!!!\n",
					s->data[0], s->data[1], f->data[0], f->data[1]));
            f->rchild = s;  /*  ����sΪ�Һ��� */
        }
    } 
    else if(1 == seach_ret)
    {
    	//��Ҫ��ɾ���ɽڵ㣬�ٲ�����data
        if (f) 
        {
			VOD_EXT_ALWAYS(("need to delete [%d,%d] before insert [%d,%d]\n", 
					f->data[0], f->data[1], data[0], data[1]));
            Delete_Node(*T, &f);
            //�ݹ���룬��ֱֹ�Ӳ�����ͻ
            InsertBST(T, data);
		}
		else
		{
			VOD_EXT_ERRO(("insert [%d,%d] error!!!\n", data[0], data[1]));
		}
    }
	else if(0 == seach_ret)
    {
    	/*  �������йؼ�����ͬ�Ľ�㣬���ٲ��� */
       	VOD_EXT_ALWAYS(("no need to insert [%d,%d], already have [%d,%d]!!!\n"
       				, data[0], data[1], f->data[0], f->data[1]));  
    }
}

void DeleteBST(BiTree *T)//�������ɾ����
{
	if((*T)->lchild)
	{
    	DeleteBST(&(*T)->lchild); //ɾ��������
    }
    
	if((*T)->rchild)
	{
    	DeleteBST(&(*T)->rchild);//ɾ��������
    }
    
    if(*T) /* �ڵ㲻Ϊ�գ���ɾ�� */ 
    {
		//PrintBST("[%d,%d]", (*T)->data[0], (*T)->data[1]);
        free(*T);
        *T = NULL;
    }
}

//�ڶ����������в���ĳ��ֵ�Ƿ���ĳ��������
bool SearchBST_Key(const BiTree T, const INT32_T key) 
{  
    if (!T)    /*  ���Ҳ��ɹ� ������false*/
    { 
        return false; 
    }
    else if (key >= T->data[0] && key <= T->data[1]) /*  ���ҳɹ� ������true*/
    { 
		VOD_EXT_WARN(("!!!!!!!!! the %d is in [%d,%d] !!!!!!!!!\n", key, T->data[0], T->data[1]));
		return true; 
    } 
    else if (key < T->data[0]) 
    {
        return SearchBST_Key(T->lchild, key);  /*  ���������м������� */
    }
    else if (key > T->data[1]) 
    {
        return SearchBST_Key(T->rchild, key);  /*  ���������м������� */
    }
    
	return true; 
}

//΢��strtok_rʵ�ָĶ�
//�����ö���ַ���Ϊһ����Ϸָ���
static char*  strtok_str(char* string_org,const char* demial)
{
	static unsigned char* last; //����ָ���ʣ��Ĳ���
	unsigned char* str;         //���ص��ַ���
	const unsigned char* ctrl = (const unsigned char*)demial;//�ָ��ַ�

	//�ѷָ��ַ��ŵ�һ���������С�
	//����32����ΪASCII�ַ��������0~255����
	//Ҳ��˵������255����3λ��
	//Ҳ���ǳ���8һ������32�е�һ������
	const int rownum = strlen(demial);
	const int size_type = sizeof(unsigned char);

    //�й̶��Ķ�̬��ά����
    //�������ڴ棬����point_size * row��ʾ���row����ָ��
    unsigned char *map_tmp = (unsigned char *) malloc(size_type * rownum * 32);
    unsigned char (*map)[32] = (unsigned char(*)[32])map_tmp;

	//unsigned char map[][32];
	int count = 0;
	int row = 0;

	//��mapȫ����Ϊ0��֮������Ĳ�������0�Ķ�Ϊ0
	for(row = 0; row < rownum; row++)
	{
		for (count =0; count <32; count++)
		{
			map[row][count] = 0;
		}
	}

	//��ƥ���ַ��������
	//������㷨�ǰ�ƥ���ַ�����3λ���൱�ڳ���8������ֵ ����(����)
	//ƥ���ַ���7���õ���3λ���ó��Ľ�����ǰ�1���Ƶ�λ����
	//�������λ����7��Ҳ��������ʾ�����ֵ��128

	for(row = 0; row < rownum; row++)
	{
		do
		{
			map[row][*ctrl >> 3] |= (1 << (*ctrl & 7));
		} while (*ctrl++);
		ctrl -= rownum;
	}

	//ԭʼ�ַ����Ƿ�Ϊ�գ����Ϊ�ձ�ʾ�ڶ��λ�ȡʣ���ַ��ķָ����֡�
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
	//�Թ� �ͷƥ���ַ����ظ�ƥ���ַ���ע�ⲿ��ƥ������
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

	//������Ҫɨ����ַ���
	string_org = (char*)str;

	bool exitflag = false;
	//��ʼɨ��
	for (;*str && !exitflag; str++)
	{
		row = 0;
		while ( map[row][*str >> 3] & (1 << (*str & 7)))
	    {
	    	str++;
			row++;
			if(rownum == row)
			{
				*(str-rownum) = '\0';//���ҵ�ʱ����ƥ���ַ���Ϊ0�����Ұ�strָ����һλ��
				exitflag = true;
				break; //�˳�ѭ��
			}
	    }
	}
    str--; //����ѭ�����ɾ��һ��

	//�����ʱ����
    free(map_tmp);
    map_tmp = NULL;
    
	last =str; // ��ʣ���ַ�����ָ�뱣�浽��̬����last�С�
	if (string_org == (char*)str)
	{
		return NULL; //û���ҵ���Ҳ����û���ƶ�ָ���λ�ã�����NULL
	}
	else
	{
		return string_org; //�ҵ��ˣ�����֮ǰ�ַ�����ͷָ��
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

	p = strstr(qambuf, "[["); //���俪ʼ
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
		p = strstr(q, "[["); //���俪ʼ
	}
	VOD_EXT_DBUG(("saved %d qam_name region!!!\n", qam_num));

	if(0 == qam_num)
	{
		return false;
	}

	return true;
}


#endif  /*_NGOD_GW_BISORTT_C_*/
