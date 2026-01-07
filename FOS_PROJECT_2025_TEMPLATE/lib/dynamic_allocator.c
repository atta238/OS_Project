/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//==================================
//==================================
// [1] GET PAGE VA:
//==================================
__inline__ uint32 to_page_va(struct PageInfoElement *ptrPageInfo)
{
	if (ptrPageInfo < &pageBlockInfoArr[0] || ptrPageInfo >= &pageBlockInfoArr[DYN_ALLOC_MAX_SIZE/PAGE_SIZE])
			panic("to_page_va called with invalid pageInfoPtr");
	//Get start VA of the page from the corresponding Page Info pointer
	int idxInPageInfoArr = (ptrPageInfo - pageBlockInfoArr);
	return dynAllocStart + (idxInPageInfoArr << PGSHIFT);
}

//==================================
// [2] GET PAGE INFO OF PAGE VA:
//==================================
__inline__ struct PageInfoElement * to_page_info(uint32 va)
{
	int idxInPageInfoArr = (va - dynAllocStart) >> PGSHIFT;
	if (idxInPageInfoArr < 0 || idxInPageInfoArr >= DYN_ALLOC_MAX_SIZE/PAGE_SIZE)
		panic("to_page_info called with invalid pa");
	return &pageBlockInfoArr[idxInPageInfoArr];
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================
bool is_initialized = 0;
void initialize_dynamic_allocator(uint32 daStart, uint32 daEnd)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(daEnd <= daStart + DYN_ALLOC_MAX_SIZE);
		is_initialized = 1;
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #1 initialize_dynamic_allocator
	//Your code is here
	dynAllocStart=daStart;
	dynAllocEnd=daEnd;
	LIST_INIT(&freePagesList);
	uint32 size_of_page_block_arr=(daEnd-daStart)/PAGE_SIZE;
	for (int i=0;i<size_of_page_block_arr;i++)
	{
		pageBlockInfoArr[i].block_size=0;
		pageBlockInfoArr[i].num_of_free_blocks=0;
		pageBlockInfoArr[i].prev_next_info.le_next=NULL;
		pageBlockInfoArr[i].prev_next_info.le_prev=NULL;
		LIST_INSERT_TAIL(&freePagesList,&pageBlockInfoArr[i]);
	}
	int size_of_free_block_list=LOG2_MAX_SIZE-LOG2_MIN_SIZE+1;
	for (int i=0;i<size_of_free_block_list;i++)
	{
		LIST_INIT(&freeBlockLists[i]);
	}
	//Comment the following line
//	panic("initialize_dynamic_allocator() Not implemented yet");

}

//===========================
// [2] GET BLOCK SIZE:
//===========================
__inline__ uint32 get_block_size(void *va)
{
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #2 get_block_size
	//Your code is here
	return to_page_info((uint32)va)->block_size;
	//Comment the following line
	//panic("get_block_size() Not implemented yet");

}

//===========================
// 3) ALLOCATE BLOCK:
//===========================
void *alloc_block(uint32 size)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(size <= DYN_ALLOC_MAX_BLOCK_SIZE);
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #3 alloc_block
	//Your code is here
	if(size==0)
	{
		return NULL;
	}
	if(size<8)
	{
		size=8;
	}
	int act_bits;
	uint16 act_size;
	for(int i=LOG2_MIN_SIZE;i<=LOG2_MAX_SIZE;i++)
	{
		int x=1<<i;
		if(size<=x)
		{
			act_bits=i;
			act_size=x;
			break;
		}
	}
	int idx=(act_bits-LOG2_MIN_SIZE);
	if(!LIST_EMPTY(&freeBlockLists[idx]))
	{
		struct BlockElement * block=LIST_FIRST(&freeBlockLists[idx]);
		LIST_REMOVE(&freeBlockLists[idx],block);
		to_page_info((uint32)block)->num_of_free_blocks--;
// 		cprintf("VALUE OF ADDRESS OF BLOCK1=%x\n",block);
		return block;
	}
	if(!LIST_EMPTY(&freePagesList))
	{
		struct PageInfoElement * page=LIST_FIRST(&freePagesList);
		uint32 va_of_page=to_page_va(page);
		get_page((void*)va_of_page);
		LIST_REMOVE(&freePagesList,page);
		page->block_size=act_size;
		uint16 free_blocks=(PAGE_SIZE/act_size);
		page->num_of_free_blocks=free_blocks-1;
		struct BlockElement * first_block = (struct BlockElement *) va_of_page;
		for(int i=1;i<free_blocks;i++)
		{
			uint32 va_of_block=va_of_page+(i*act_size);
			struct BlockElement * block= (struct BlockElement *) va_of_block;
			LIST_INSERT_TAIL(&freeBlockLists[idx],block);
		}
// 		cprintf("VALUE OF ADDRESS OF BLOCK2=%x\n",first_block);
		return first_block;
	}
	else
	{
		int size_of_free_block_list=LOG2_MAX_SIZE-LOG2_MIN_SIZE+1;
		for (int i=idx+1;i<size_of_free_block_list;i++)
		{
			if(!LIST_EMPTY(&freeBlockLists[i]))
			{
				struct BlockElement * block=LIST_FIRST(&freeBlockLists[i]);
				LIST_REMOVE(&freeBlockLists[i],block);
				to_page_info((uint32)block)->num_of_free_blocks--;
// 				cprintf("VALUE OF ADDRESS OF BLOCK3=%x\n",block);
				return block;
			}
		}
	}
	panic("NO SPACE AVAILABLE");
	//Comment the following line
	//	panic("alloc_block() Not implemented yet");
	//TODO: [PROJECT'25.BONUS#1] DYNAMIC ALLOCATOR - block if no free block
}

//===========================
// [4] FREE BLOCK:
//===========================
 void free_block(void *va)
 {
 	//==================================================================================
 	//DON'T CHANGE THESE LINES==========================================================
 	//==================================================================================
 	{
 		assert((uint32)va >= dynAllocStart && (uint32)va < dynAllocEnd);
 	}
 	//==================================================================================
 	//==================================================================================

 	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #4 free_block
 	//Your code is here
 	uint32 size=get_block_size(va);
 	if(size==0)
 	{
 		return;
 	}
 	uint32 idx=0;
 	uint32 s=size;
 	while(s>1)
 	{
 		s/=2;
 		idx++;
 	}
 	idx-=LOG2_MIN_SIZE;
 	struct BlockElement * block= (struct BlockElement *) va;
 	LIST_INSERT_TAIL(&freeBlockLists[idx],block);
 	to_page_info((uint32)va)->num_of_free_blocks++;
 	if(to_page_info((uint32)va)->num_of_free_blocks==(PAGE_SIZE/size))
 	{
 		to_page_info((uint32)va)->num_of_free_blocks=0;
 		to_page_info((uint32)va)->block_size=0;
 		uint32 va_of_page=to_page_va(to_page_info((uint32)va));
 		uint32 end_address=va_of_page+PAGE_SIZE;
 		struct BlockElement * element;
 		LIST_FOREACH_SAFE(element,&freeBlockLists[idx],BlockElement)
 		{
 			if((uint32)element >=va_of_page && (uint32)element<end_address)
 			{
 				LIST_REMOVE(&freeBlockLists[idx],element);
// 				cprintf("VALUE OF REMOVED BLOCK =%x\n",element);
 			}
 		}
// 		cprintf("VALUE OF RETURNED BLOCK =%x\n",va_of_page);
 		return_page((void*)va_of_page);
 		LIST_INSERT_TAIL(&freePagesList,to_page_info((uint32)va));
 	}
 	//Comment the following line
 //	panic("free_block() Not implemented yet");
 }

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] REALLOCATE BLOCK:
//===========================
void *realloc_block(void* va, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - realloc_block
	//Your code is here
	//Comment the following line
	panic("realloc_block() Not implemented yet");
}
