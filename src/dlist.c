/***************************************************************************************
****************************************************************************************
* file    : app_list.c
* brief   : 用于实现双向链表库
* emai    : lvkun@tcl.com
* Copyright (c) 2014-13-06 by TCL  - tcl Project Team . All Rights Reserved.
*
* history :
* version		Name       		Date			Description

****************************************************************************************
****************************************************************************************/



/* ------ Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <dirent.h>
#include <fcntl.h>
#include <malloc.h>
#include "dlist.h"
/* ------ Private macros ------------------------------------------------------------*/



/* ------ Private typedef -----------------------------------------------------------*/




/* ------ Private variables ---------------------------------------------------------*/




/* ------ Private functions ---------------------------------------------------------*/




/* ------ External variables --------------------------------------------------------*/


/*
* File:   dlist.c：通用双向链表函数实现
* Author: ecorefeng
* Created on 2010年8月13日
*/

/*
*分配内存
*/
//........malloc  node..................
static DListNode *dlist_alloc()
{
	return (DListNode *)calloc(1, sizeof(DListNode));
}

/*
*功能：实现一个DList结构体的初始化
*参数：void
*返回：DList结构体
*/
//..........create head..............
DList *dlist_create()
{
	DList  *dlist = (DList *)calloc(1, sizeof(DList));

	if(dlist == NULL)
	{
		return NULL;
	}
	dlist->head  = (DListNode *)calloc(1, sizeof(DListNode));
	if(dlist->head == NULL)
	{
		free(dlist);
		dlist = NULL;

		return NULL;
	}
	dlist->tail = (DListNode *)calloc(1, sizeof(DListNode));
	if(NULL == dlist->tail)
	{
		free(dlist->head);
		free(dlist);
		dlist = NULL;

		return NULL;
	}
	dlist->head->next =dlist->tail;
	dlist->tail->prev = dlist->head;

	return dlist;
}

/*
*功能：长度
*参数：dlist:指定链表
*返回：大小
*/
int dlist_len(DList *dlist)
{
	return_val_if_fail(dlist != NULL, 0);

	int len = 0;
	DListNode *dlistNode = NULL;

	dlistNode = dlist->head->next;
	while(NULL != dlistNode->data)
	{
		dlistNode = dlistNode->next;
		len = len + 1;
	}

	return len;
}

/*
*功能：添加元素
*参数：dlist:指定链表  index: 添加的位置 data：加入的数据的指针
*返回：Ret
*/
Ret dlist_add(DList *dlist, int index, void *data)
{
	return_val_if_fail(dlist != NULL&&data != NULL&&index >= 0, RET_FAULT);

	int len = 0;
	DListNode *node = NULL;
	DListNode *dlistPreNode = NULL,*dlistNextNode = NULL;

	len = dlist_len(dlist);
	return_val_if_fail(index <= len, RET_FAULT);
	node = dlist_alloc();
	if(node == NULL)
	{
		return RET_OOM;
	}
	node->data = data;
	if(0 == index)
	{
		dlistPreNode = dlist->head;
	}
	else
	{
		dlistPreNode = dlist_get(dlist,index-1);
	}
	dlistNextNode = dlistPreNode->next;
	dlistPreNode->next = node;
	node->prev = dlistPreNode;
	node->next = dlistNextNode;
	dlistNextNode->prev = node;

	return RET_OK;
}

/*
*功能：在链表尾添加元素
*参数：
*返回：Ret
*/
Ret dlist_attach(DList *dlist,void *data)
{
	return_val_if_fail(dlist != NULL&&data != NULL, RET_FAULT);

	DListNode *node = NULL;
	DListNode *dlistPreNode = NULL,*dlistNextNode = NULL;

	node = dlist_alloc();
	if(node == NULL)
	{
		return RET_FAULT;
	}
	node->data = data;
	dlistPreNode = dlist->tail->prev;
	dlistNextNode = dlist->tail;
	dlistPreNode->next = node;
	node->prev = dlistPreNode;
	node->next = dlistNextNode;
	dlistNextNode->prev = node;

	return RET_OK;
}

/*
*功能：得到节点
*参数：dlist:要操作的链表指针地址 index:位置
*/
//..........get node.......
DListNode *dlist_get(DList *dlist, int index)
{
	return_val_if_fail(dlist != NULL && index >= 0, NULL);

	int n = 0,len = 0;
	DListNode *dlistnode = NULL;

	len = dlist_len(dlist);
	return_val_if_fail(index < len, NULL);
	dlistnode = dlist->head->next;
	while (n < index)
	{
		dlistnode = dlistnode->next;
		n = n + 1;
	}

	return dlistnode;
}

/*
*功能：删除指定位置的值
*参数：参数：dlist:指定链表 index：位置
*/
//.........del.............
Ret dlist_delete(DList *dlist, int index)
{
	int len = 0;
	DListNode *dlistNode = NULL;
	DListNode *dlistPreNode = NULL,*dlistNextNode = NULL;

	return_val_if_fail(dlist != NULL && index >=0, RET_FAULT);

	len = dlist_len(dlist);
	return_val_if_fail(index < len, RET_FAULT);
	dlistNode = dlist_get(dlist, index);
	if(NULL == dlistNode)
	{
		return RET_FAULT;
	}
	dlistPreNode = dlistNode->prev;
	dlistNextNode = dlistNode->next;
	dlistPreNode->next = dlistNextNode;
	dlistNextNode->prev = dlistPreNode;
	if(NULL != dlistNode->data)
	{
		free(dlistNode->data);
	}
	free(dlistNode);
	dlistNode =NULL;

	return RET_OK;
}



Ret dlist_remove(DList *dlist, int index)
{
	int len = 0;
	DListNode *dlistNode = NULL;
	DListNode *dlistPreNode = NULL,*dlistNextNode = NULL;

	return_val_if_fail(dlist != NULL && index >=0, RET_FAULT);

	len = dlist_len(dlist);
	return_val_if_fail(index < len, RET_FAULT);
	dlistNode = dlist_get(dlist, index);
	if(NULL == dlistNode)
	{
		return RET_FAULT;
	}
	dlistPreNode = dlistNode->prev;
	dlistNextNode = dlistNode->next;
	dlistPreNode->next = dlistNextNode;
	dlistNextNode->prev = dlistPreNode;
	free(dlistNode);
	dlistNode =NULL;

	return RET_OK;
}



/*
*功能：释放指定链表内存
*参数：dlist:指定链表
*返回：Ret
*/
//..........free list .............
Ret dlist_destroy(DList *dlist)
{
	return_val_if_fail(dlist != NULL, RET_FAULT);

	DListNode *cursor = NULL,*nextNode = NULL;

	cursor = dlist->head->next;
	while(NULL != cursor->data)
	{
		nextNode = cursor->next;
		if(NULL != cursor->data)
		{
			free(cursor->data);
		}
		free(cursor);
		cursor = nextNode;
	}
	free(dlist->head);
	free(dlist->tail);
	free(dlist);

	return RET_OK;
}
