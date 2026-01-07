#include <inc/lib.h>

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//==============================================
// [1] INITIALIZE USER HEAP:
//==============================================
int __firstTimeFlag = 1;

int UHEAP_ARR[NUM_OF_UHEAP_PAGES]; // shayel el no. of pages
void uheap_init()
{
	if(__firstTimeFlag)
	{
		initialize_dynamic_allocator(USER_HEAP_START, USER_HEAP_START + DYN_ALLOC_MAX_SIZE);
		uheapPlaceStrategy = sys_get_uheap_strategy();
		uheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		uheapPageAllocBreak = uheapPageAllocStart;
		memset(UHEAP_ARR, 0, sizeof(UHEAP_ARR));
		__firstTimeFlag = 0;
	}
}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = __sys_allocate_page(ROUNDDOWN(va, PAGE_SIZE), PERM_USER|PERM_WRITEABLE|PERM_UHPAGE);
	if (ret < 0)
		panic("get_page() in user: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	int ret = __sys_unmap_frame(ROUNDDOWN((uint32)va, PAGE_SIZE));
	if (ret < 0)
		panic("return_page() in user: failed to return a page to the kernel");
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=================================
// [1] ALLOCATE SPACE IN USER HEAP:
//=================================
void* malloc(uint32 size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size == 0) return NULL ;
	//==============================================================
	//TODO: [PROJECT'25.IM#2] USER HEAP - #1 malloc
#if USE_KHEAP
	//Your code is here
	if(size<=DYN_ALLOC_MAX_BLOCK_SIZE)
	{
		return alloc_block(size);
	}
	uint32 no_pages=ROUNDUP(size,PAGE_SIZE)/PAGE_SIZE;
	uint32 cntr=0;
	uint32 max_cntr=0;
	int type=-1;
	uint32 start_address;
	uint32 address_of_max;
	for(uint32 i=uheapPageAllocStart;i<uheapPageAllocBreak;)
	{
		if(UHEAP_ARR[(i-USER_HEAP_START)/PAGE_SIZE]==0)
		{
			cntr++;
			i+=PAGE_SIZE;
		}
		else
		{
			if(cntr==no_pages)
			{
				start_address=i-(cntr*PAGE_SIZE);
				type=1;
				break;
			}
			else
			{
				if(cntr>max_cntr && cntr>no_pages)
				{
					max_cntr=cntr;
					type=2;
					address_of_max=i-(max_cntr*PAGE_SIZE);
				}
				cntr=0;
				i+=(UHEAP_ARR[(i-USER_HEAP_START)/PAGE_SIZE]*PAGE_SIZE);
			}
		}
	}



	uint32 act_address;
	if(type == 1)
	{
		//cprintf("BNSHOF EXACT, size needeed=%d,ADDRESS = %x\n",no_pages,start_address);
		act_address=start_address;
	}
	else if(type == 2)
	{
		//cprintf("BNSHOF WORST, size needeed=%d,size_founded=%d,ADDRESS = %x\n",no_pages,max_cntr,address_of_max);
		act_address=address_of_max;
	}
	else if(no_pages<=(USER_HEAP_MAX-uheapPageAllocBreak)/PAGE_SIZE)
	{
		//cprintf("BNSHOF UN_USED, size needeed=%d,ADDRESS = %x\n",no_pages,uheapPageAllocBreak);
		act_address=uheapPageAllocBreak;
		type=3;
		uheapPageAllocBreak+=(no_pages*PAGE_SIZE);
	}





	if(type != -1)
	{
		UHEAP_ARR[(act_address-USER_HEAP_START)/PAGE_SIZE]=no_pages;
		sys_allocate_user_mem(act_address,size);
		return (void*)act_address;
	}
	return NULL;
#endif
	//Comment the following line
	panic("malloc() is not implemented yet...!!");
}

//=================================
// [2] FREE SPACE FROM USER HEAP:
//=================================
void free(void* virtual_address)
{
	//TODO: [PROJECT'25.IM#2] USER HEAP - #3 free
#if USE_KHEAP
	//Your code is here
	uint32 VA = (uint32)virtual_address;
	if (VA >= dynAllocStart && VA < dynAllocEnd)
	{
		free_block(virtual_address);
		return;
	}
	if(VA >= uheapPageAllocStart && VA < uheapPageAllocBreak )
	{
		VA=ROUNDDOWN(VA,PAGE_SIZE);
		uint32 size=UHEAP_ARR[(VA-USER_HEAP_START)/PAGE_SIZE]*PAGE_SIZE;
		if(size==0)
		{
			return;
		}
		UHEAP_ARR[(VA-USER_HEAP_START)/PAGE_SIZE]=0;
		//cprintf("FREED VA = %x , with size =%d\n",VA,size/PAGE_SIZE);
		if (VA == uheapPageAllocBreak-size)
		{
			uheapPageAllocBreak=VA;
			uint32 a5r_wa7ed;
			int size=-1;
			uint32 below_add=uheapPageAllocBreak;
			while (below_add > uheapPageAllocStart)
			{
				below_add-=PAGE_SIZE;
			   if(UHEAP_ARR[(below_add-USER_HEAP_START)/PAGE_SIZE] != 0)
			   {
				   a5r_wa7ed=below_add;
				   size=UHEAP_ARR[(below_add-USER_HEAP_START)/PAGE_SIZE]*PAGE_SIZE;
				   break;
			   }
			}
			if(size == -1)
			{
				uheapPageAllocBreak=uheapPageAllocStart;
			}
			else
			{

				uheapPageAllocBreak=a5r_wa7ed+size;
			}
		}
		sys_free_user_mem(VA,size);
	}
	else
	{
		panic("INVALID ADDRESS");
	}
#endif
	//Comment the following line
	//panic("free() is not implemented yet...!!");
}

//=================================
// [3] ALLOCATE SHARED VARIABLE:
//=================================
void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size == 0) return NULL ;
	//==============================================================

	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #2 smalloc
#if USE_KHEAP
	//Your code is here
	uint32 no_pages=ROUNDUP(size,PAGE_SIZE)/PAGE_SIZE;
	uint32 cntr=0;
	uint32 max_cntr=0;
	int type=-1;
	uint32 start_address;
	uint32 address_of_max;
	for(uint32 i=uheapPageAllocStart;i<uheapPageAllocBreak;)
	{
		if(UHEAP_ARR[(i-USER_HEAP_START)/PAGE_SIZE]==0)
		{
			cntr++;
			i+=PAGE_SIZE;
		}
		else
		{
			if(cntr==no_pages)
			{
				start_address=i-(cntr*PAGE_SIZE);
				type=1;
				break;
			}
			else
			{
				if(cntr>max_cntr && cntr>no_pages)
				{
					max_cntr=cntr;
					type=2;
					address_of_max=i-(max_cntr*PAGE_SIZE);
				}
				cntr=0;
				i+=(UHEAP_ARR[(i-USER_HEAP_START)/PAGE_SIZE]*PAGE_SIZE);
			}
		}
	}



	uint32 act_address;
	if(type == 1)
	{
		//cprintf("BNSHOF EXACT, size needeed=%d,ADDRESS = %x\n",no_pages,start_address);
		act_address=start_address;
	}
	else if(type == 2)
	{
		//cprintf("BNSHOF WORST, size needeed=%d,size_founded=%d,ADDRESS = %x\n",no_pages,max_cntr,address_of_max);
		act_address=address_of_max;
	}
	else if(no_pages<=(USER_HEAP_MAX-uheapPageAllocBreak)/PAGE_SIZE)
	{
		//cprintf("BNSHOF UN_USED, size needeed=%d,ADDRESS = %x\n",no_pages,uheapPageAllocBreak);
		act_address=uheapPageAllocBreak;
		type=3;
		uheapPageAllocBreak+=(no_pages*PAGE_SIZE);
	}





	if(type!=-1)
	{
		UHEAP_ARR[(act_address-USER_HEAP_START)/PAGE_SIZE]=no_pages;
		int ret=sys_create_shared_object(sharedVarName,size,isWritable,(void*)act_address);
		if(ret == E_NO_SHARE || ret == E_SHARED_MEM_EXISTS)
		{
			uheapPageAllocBreak-=(no_pages*PAGE_SIZE);
			return NULL;
		}
		return (void*)act_address;
	}
	return NULL;
#endif
	//Comment the following line
	panic("smalloc() is not implemented yet...!!");
}

//========================================
// [4] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================

	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #4 sget
#if USE_KHEAP
	//Your code is here
	int size=sys_size_of_shared_object(ownerEnvID,sharedVarName);
	if(size==E_SHARED_MEM_NOT_EXISTS)
	{
		return NULL;
	}
	uint32 no_pages=ROUNDUP(size,PAGE_SIZE)/PAGE_SIZE;
	uint32 cntr=0;
	uint32 max_cntr=0;
	int type=-1;
	uint32 start_address;
	uint32 address_of_max;
	for(uint32 i=uheapPageAllocStart;i<uheapPageAllocBreak;)
	{
		if(UHEAP_ARR[(i-USER_HEAP_START)/PAGE_SIZE]==0)
		{
			cntr++;
			i+=PAGE_SIZE;
		}
		else
		{
			if(cntr==no_pages)
			{
				start_address=i-(cntr*PAGE_SIZE);
				type=1;
				break;
			}
			else
			{
				if(cntr>max_cntr && cntr>no_pages)
				{
					max_cntr=cntr;
					type=2;
					address_of_max=i-(max_cntr*PAGE_SIZE);
				}
				cntr=0;
				i+=(UHEAP_ARR[(i-USER_HEAP_START)/PAGE_SIZE]*PAGE_SIZE);
			}
		}
	}



	uint32 act_address;
	if(type == 1)
	{
		//cprintf("BNSHOF EXACT, size needeed=%d,ADDRESS = %x\n",no_pages,start_address);
		act_address=start_address;
	}
	else if(type == 2)
	{
		//cprintf("BNSHOF WORST, size needeed=%d,size_founded=%d,ADDRESS = %x\n",no_pages,max_cntr,address_of_max);
		act_address=address_of_max;
	}
	else if(no_pages<=(USER_HEAP_MAX-uheapPageAllocBreak)/PAGE_SIZE)
	{
		//cprintf("BNSHOF UN_USED, size needeed=%d,ADDRESS = %x\n",no_pages,uheapPageAllocBreak);
		act_address=uheapPageAllocBreak;
		type=3;
		uheapPageAllocBreak+=(no_pages*PAGE_SIZE);
	}





	if(type!=-1)
	{
		UHEAP_ARR[(act_address-USER_HEAP_START)/PAGE_SIZE]=no_pages;
		int ret=sys_get_shared_object(ownerEnvID,sharedVarName,(void*)act_address);
		if(ret==E_SHARED_MEM_NOT_EXISTS )
		{
			uheapPageAllocBreak-=(no_pages*PAGE_SIZE);
			return NULL;
		}
		return (void*)act_address;
	}
	return NULL;


#endif
	//Comment the following line
	panic("sget() is not implemented yet...!!");
}


//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//


//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================
	panic("realloc() is not implemented yet...!!");
}


//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_delete_shared_object(...); which switches to the kernel mode,
//	calls delete_shared_object(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the delete_shared_object() function is empty, make sure to implement it.
void sfree(void* virtual_address)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - sfree
	//Your code is here
	//Comment the following line
	panic("sfree() is not implemented yet...!!");

	//	1) you should find the ID of the shared variable at the given address
	//	2) you need to call sys_freeSharedObject()
}


//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//
