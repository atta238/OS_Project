#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include <kern/conc/sleeplock.h>
#include <kern/proc/user_environment.h>
#include <kern/mem/memory_manager.h>
#include "../conc/kspinlock.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//==============================================
// [1] INITIALIZE KERNEL HEAP:
//==============================================
//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #0 kheap_init [GIVEN]
//Remember to initialize locks (if any)
struct kspinlock lk;
void kheap_init()
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		init_kspinlock(&lk, "kheap");
		initialize_dynamic_allocator(KERNEL_HEAP_START, KERNEL_HEAP_START + DYN_ALLOC_MAX_SIZE);
		set_kheap_strategy(KHP_PLACE_CUSTOMFIT);
		kheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		kheapPageAllocBreak = kheapPageAllocStart;
	}
	//==================================================================================
	//==================================================================================
}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = alloc_page(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE), PERM_WRITEABLE, 1);
	if (ret < 0)
		panic("get_page() in kern: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	unmap_frame(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE));
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===================================
// [1] ALLOCATE SPACE IN KERNEL HEAP:
//===================================
void* kmalloc(unsigned int size)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #1 kmalloc
	#if USE_KHEAP
	//Your code is here
	if(size<=DYN_ALLOC_MAX_BLOCK_SIZE)
	{
		acquire_kspinlock(&lk);
		void * atta_ptr=alloc_block(size);
		release_kspinlock(&lk);
		return atta_ptr;
	}
	uint32 no_pages=ROUNDUP(size,PAGE_SIZE)/PAGE_SIZE;
	uint32 cntr=0;
	uint32 max_cntr=0;
	int type=-1;
	uint32 start_address;
	uint32 address_of_max;
	acquire_kspinlock(&lk);
	for(uint32 i=kheapPageAllocStart;i<kheapPageAllocBreak;)
	{
		uint32 *ptrtable=NULL;
		struct FrameInfo * fi= get_frame_info(ptr_page_directory,i, &ptrtable);
		if(fi == NULL)
		{
			cntr++;
			i+=PAGE_SIZE;
		}
		else
		{
			if(cntr==no_pages)
			{
				type=1;
				start_address=i-(cntr*PAGE_SIZE);
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
				i+=fi->no_of_pages*PAGE_SIZE;
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
	else if(no_pages<=(KERNEL_HEAP_MAX-kheapPageAllocBreak)/PAGE_SIZE)
	{
		//cprintf("BNSHOF UN_USED, size needeed=%d,ADDRESS = %x\n",no_pages,uheapPageAllocBreak);
		act_address=kheapPageAllocBreak;
		type=3;
		kheapPageAllocBreak+=(no_pages*PAGE_SIZE);
	}




	if(type != -1)
	{
		for(int i=0;i<no_pages;i++)
		{
			void * ptr=(void *)(act_address+(i*PAGE_SIZE));
			get_page(ptr);
			if(i==0)
			{
				uint32 *ptrtable=NULL;
				struct FrameInfo* frame_no = get_frame_info(ptr_page_directory, (uint32)ptr, &ptrtable);
				if(frame_no!=NULL)
				{
					frame_no->no_of_pages=no_pages;
				}
			}
		}
		release_kspinlock(&lk);
		return (void*)act_address;
	}

//	cprintf("MAFESH MKAN YA HABEBY");
	release_kspinlock(&lk);
	return NULL;
	#endif

	//Comment the following line
	kpanic_into_prompt("kmalloc() is not implemented yet...!!");
//TODO: [PROJECT'25.BONUS#3] FAST PAGE ALLOCATOR
}

//=================================
// [2] FREE SPACE FROM KERNEL HEAP:
//=================================
void kfree(void* virtual_address)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #2 kfree
	#if USE_KHEAP
	//Your code is here

	uint32 VA = (uint32)virtual_address;
    if (VA >= dynAllocStart && VA < dynAllocEnd)
    {
    	acquire_kspinlock(&lk);
        free_block(virtual_address);
        release_kspinlock(&lk);
        return;
    }
    acquire_kspinlock(&lk);
    if(VA >= kheapPageAllocStart && VA < kheapPageAllocBreak )
    {
		VA=ROUNDDOWN(VA,PAGE_SIZE);
		uint32 *ptrtable=NULL;
		struct FrameInfo* f_info = get_frame_info(ptr_page_directory,VA, &ptrtable);
		if(f_info==NULL || f_info->no_of_pages==0)
		{
			release_kspinlock(&lk);
			return;
		}
		uint32 page_count=f_info->no_of_pages;
		uint32 size=page_count*PAGE_SIZE;
		f_info->no_of_pages=0;
//		cprintf("LAST ADDRESS REMOVED =%x\n", VA + (page_count*PAGE_SIZE));
		for (uint32 i = 0; i < page_count; i++)
		{
			uint32 virtualA=VA+(i*PAGE_SIZE);
			return_page((void*)virtualA);
		}
		if (VA == kheapPageAllocBreak-size)
		{
//			cprintf("D5LT 5LAS\n");
			kheapPageAllocBreak=VA;
			while (kheapPageAllocBreak > kheapPageAllocStart)
			{
				uint32 below_add= kheapPageAllocBreak - PAGE_SIZE;
				uint32 *ptrtable=NULL;
			   if (get_frame_info(ptr_page_directory, below_add, &ptrtable) == NULL)
			   {
				   kheapPageAllocBreak -= PAGE_SIZE;
			   }
			   else
			   {
				   break;
			   }
			}
		}
//		cprintf("VALUE OF BREAK END =%x\n", kheapPageAllocBreak);
		release_kspinlock(&lk);
		return;
    }
    else
    {
    	release_kspinlock(&lk);
    	panic("INVALID ADDRESS");
    }
	#endif
	//Comment the following line
	//panic("kfree() is not implemented yet...!!");
}

//=================================
// [3] FIND VA OF GIVEN PA:
//=================================
unsigned int kheap_virtual_address(unsigned int physical_address)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #3 kheap_virtual_address
	#if USE_KHEAP
	//Your code is here
	if(physical_address==0)
	{
		return 0;
	}
	uint32 VA=0;
	struct FrameInfo* frame_no= to_frame_info(physical_address);
	if(frame_no->VA!=0)
	{
		uint32 offset = PGOFF(physical_address);
		VA=frame_no->VA + offset;
	}

	//		cprintf("VALUE OF PA =%x\n",physical_address);
	//		cprintf("VALUE OF VA =%x\n",VA);

	return VA;
	#endif

	//Comment the following line
	panic("kheap_virtual_address() is not implemented yet...!!");
	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
}

//=================================
// [4] FIND PA OF GIVEN VA:
//=================================
unsigned int kheap_physical_address(unsigned int virtual_address)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #4 kheap_physical_address
	#if USE_KHEAP
	//Your code is here
	if(virtual_address==0)
	{
		return 0;
	}
	uint32 PA=0;
	uint32 *ptr_page_table = NULL;
	struct FrameInfo * frame_no= get_frame_info(ptr_page_directory,virtual_address, &ptr_page_table);

	if (frame_no != NULL)
	{
		uint32 offset = PGOFF(virtual_address);
		PA = to_physical_address(frame_no) + offset;
	}

//		cprintf("VALUE OF VA =%x\n",virtual_address);
//		cprintf("VALUE OF PA =%x\n",PA);

	return PA;
	#endif
	//Comment the following line
	panic("kheap_physical_address() is not implemented yet...!!");
	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
}

//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

extern __inline__ uint32 get_block_size(void *va);

void *krealloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - krealloc
	//Your code is here
	//Comment the following line
	panic("krealloc() is not implemented yet...!!");
}
