/*
 * fault_handler.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include "trap.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <kern/cpu/cpu.h>
#include <kern/disk/pagefile_manager.h>
#include <kern/mem/memory_manager.h>
#include <kern/mem/kheap.h>

//2014 Test Free(): Set it to bypass the PAGE FAULT on an instruction with this length and continue executing the next one
// 0 means don't bypass the PAGE FAULT
uint8 bypassInstrLength = 0;

//===============================
// REPLACEMENT STRATEGIES
//===============================
//2020
void setPageReplacmentAlgorithmLRU(int LRU_TYPE)
{
	assert(LRU_TYPE == PG_REP_LRU_TIME_APPROX || LRU_TYPE == PG_REP_LRU_LISTS_APPROX);
	_PageRepAlgoType = LRU_TYPE ;
}
void setPageReplacmentAlgorithmCLOCK(){_PageRepAlgoType = PG_REP_CLOCK;}
void setPageReplacmentAlgorithmFIFO(){_PageRepAlgoType = PG_REP_FIFO;}
void setPageReplacmentAlgorithmModifiedCLOCK(){_PageRepAlgoType = PG_REP_MODIFIEDCLOCK;}
/*2018*/ void setPageReplacmentAlgorithmDynamicLocal(){_PageRepAlgoType = PG_REP_DYNAMIC_LOCAL;}
/*2021*/ void setPageReplacmentAlgorithmNchanceCLOCK(int PageWSMaxSweeps){_PageRepAlgoType = PG_REP_NchanceCLOCK;  page_WS_max_sweeps = PageWSMaxSweeps;}
/*2024*/ void setFASTNchanceCLOCK(bool fast){ FASTNchanceCLOCK = fast; };
/*2025*/ void setPageReplacmentAlgorithmOPTIMAL(){ _PageRepAlgoType = PG_REP_OPTIMAL; };

//2020
uint32 isPageReplacmentAlgorithmLRU(int LRU_TYPE){return _PageRepAlgoType == LRU_TYPE ? 1 : 0;}
uint32 isPageReplacmentAlgorithmCLOCK(){if(_PageRepAlgoType == PG_REP_CLOCK) return 1; return 0;}
uint32 isPageReplacmentAlgorithmFIFO(){if(_PageRepAlgoType == PG_REP_FIFO) return 1; return 0;}
uint32 isPageReplacmentAlgorithmModifiedCLOCK(){if(_PageRepAlgoType == PG_REP_MODIFIEDCLOCK) return 1; return 0;}
/*2018*/ uint32 isPageReplacmentAlgorithmDynamicLocal(){if(_PageRepAlgoType == PG_REP_DYNAMIC_LOCAL) return 1; return 0;}
/*2021*/ uint32 isPageReplacmentAlgorithmNchanceCLOCK(){if(_PageRepAlgoType == PG_REP_NchanceCLOCK) return 1; return 0;}
/*2021*/ uint32 isPageReplacmentAlgorithmOPTIMAL(){if(_PageRepAlgoType == PG_REP_OPTIMAL) return 1; return 0;}

//===============================
// PAGE BUFFERING
//===============================
void enableModifiedBuffer(uint32 enableIt){_EnableModifiedBuffer = enableIt;}
uint8 isModifiedBufferEnabled(){  return _EnableModifiedBuffer ; }

void enableBuffering(uint32 enableIt){_EnableBuffering = enableIt;}
uint8 isBufferingEnabled(){  return _EnableBuffering ; }

void setModifiedBufferLength(uint32 length) { _ModifiedBufferLength = length;}
uint32 getModifiedBufferLength() { return _ModifiedBufferLength;}

//===============================
// FAULT HANDLERS
//===============================

//==================
// [0] INIT HANDLER:
//==================
void fault_handler_init()
{
	//setPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX);
	//setPageReplacmentAlgorithmOPTIMAL();
	setPageReplacmentAlgorithmCLOCK();
	//setPageReplacmentAlgorithmModifiedCLOCK();
	enableBuffering(0);
	enableModifiedBuffer(0) ;
	setModifiedBufferLength(1000);
}
//==================
// [1] MAIN HANDLER:
//==================
/*2022*/
uint32 last_eip = 0;
uint32 before_last_eip = 0;
uint32 last_fault_va = 0;
uint32 before_last_fault_va = 0;
int8 num_repeated_fault  = 0;
extern uint32 sys_calculate_free_frames() ;

struct Env* last_faulted_env = NULL;
void fault_handler(struct Trapframe *tf)
{
	/******************************************************/
	// Read processor's CR2 register to find the faulting address
	uint32 fault_va = rcr2();
	//cprintf("************Faulted VA = %x************\n", fault_va);
	//	print_trapframe(tf);
	/******************************************************/

	//If same fault va for 3 times, then panic
	//UPDATE: 3 FAULTS MUST come from the same environment (or the kernel)
	struct Env* cur_env = get_cpu_proc();
	if (last_fault_va == fault_va && last_faulted_env == cur_env)
	{
		num_repeated_fault++ ;
		if (num_repeated_fault == 3)
		{
			print_trapframe(tf);
			panic("Failed to handle fault! fault @ at va = %x from eip = %x causes va (%x) to be faulted for 3 successive times\n", before_last_fault_va, before_last_eip, fault_va);
		}
	}
	else
	{
		before_last_fault_va = last_fault_va;
		before_last_eip = last_eip;
		num_repeated_fault = 0;
	}
	last_eip = (uint32)tf->tf_eip;
	last_fault_va = fault_va ;
	last_faulted_env = cur_env;
	/******************************************************/
	//2017: Check stack overflow for Kernel
	int userTrap = 0;
	if ((tf->tf_cs & 3) == 3) {
		userTrap = 1;
	}
	if (!userTrap)
	{
		struct cpu* c = mycpu();
		//cprintf("trap from KERNEL\n");
		if (cur_env && fault_va >= (uint32)cur_env->kstack && fault_va < (uint32)cur_env->kstack + PAGE_SIZE)
			panic("User Kernel Stack: overflow exception!");
		else if (fault_va >= (uint32)c->stack && fault_va < (uint32)c->stack + PAGE_SIZE)
			panic("Sched Kernel Stack of CPU #%d: overflow exception!", c - CPUS);
#if USE_KHEAP
		if (fault_va >= KERNEL_HEAP_MAX)
			panic("Kernel: heap overflow exception!");
#endif
	}
	//2017: Check stack underflow for User
	else
	{
		//cprintf("trap from USER\n");
		if (fault_va >= USTACKTOP && fault_va < USER_TOP)
			panic("User: stack underflow exception!");
	}

	//get a pointer to the environment that caused the fault at runtime
	//cprintf("curenv = %x\n", curenv);
	struct Env* faulted_env = cur_env;
	if (faulted_env == NULL)
	{
		cprintf("\nFaulted VA = %x\n", fault_va);
		print_trapframe(tf);
		panic("faulted env == NULL!");
	}
	//check the faulted address, is it a table or not ?
	//If the directory entry of the faulted address is NOT PRESENT then
	if ( (faulted_env->env_page_directory[PDX(fault_va)] & PERM_PRESENT) != PERM_PRESENT)
	{
		faulted_env->tableFaultsCounter ++ ;
		table_fault_handler(faulted_env, fault_va);
	}
	else
	{
		if (userTrap)
		{
			/*============================================================================================*/
			//TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #2 Check for invalid pointers
			//(e.g. pointing to unmarked user heap page, kernel or wrong access rights),
			#if USE_KHEAP
			//your code is here
			// case 1
			if(CHECK_IF_KERNEL_ADDRESS(fault_va))
			{
				env_exit();
			}
			else
			{
				uint32 *ptr_pt;
				uint32 exist=get_page_table(faulted_env->env_page_directory,fault_va,&ptr_pt);
				if(exist==TABLE_NOT_EXIST)
				{
					panic("no page table");
				}
				else
				{
					uint32 permissions=ptr_pt[PTX(fault_va)];

					if((fault_va>=USER_HEAP_START&&fault_va<USER_HEAP_MAX)&&!(permissions&PERM_UHPAGE))
					{
						env_exit();
					}
					else if((permissions&PERM_PRESENT)&&(!(permissions&PERM_WRITEABLE)))
					{
						env_exit();
					}
				}
			}
			#endif



			/*============================================================================================*/
		}

		/*2022: Check if fault due to Access Rights */
		unsigned int * t;
		get_page_table(faulted_env->env_page_directory,fault_va,&t);
		if (t == NULL)
		{
		   panic("Page table not found for fault_va = %x", fault_va);
		}
		else
		{
			uint32 perms =t[PTX(fault_va)] & 0xFFF;
			if (perms & PERM_PRESENT)
			{
				panic("Page @va=%x is exist! page fault due to violation of ACCESS RIGHTS\n", fault_va) ;
			}
		}
		/*============================================================================================*/


		// we have normal page fault =============================================================
		faulted_env->pageFaultsCounter ++ ;

//				cprintf("[%08s] user PAGE fault va %08x\n", faulted_env->prog_name, fault_va);
//				cprintf("\nPage working set BEFORE fault handler...\n");
//				env_page_ws_print(faulted_env);
		//int ffb = sys_calculate_free_frames();

		if(isBufferingEnabled())
		{
			__page_fault_handler_with_buffering(faulted_env, fault_va);
		}
		else
		{
			page_fault_handler(faulted_env, fault_va);
		}

		//		cprintf("\nPage working set AFTER fault handler...\n");
		//		env_page_ws_print(faulted_env);
		//		int ffa = sys_calculate_free_frames();
		//		cprintf("fault handling @%x: difference in free frames (after - before = %d)\n", fault_va, ffa - ffb);
	}

	/*************************************************************/
	//Refresh the TLB cache
	tlbflush();
	/*************************************************************/
}


//=========================
// [2] TABLE FAULT HANDLER:
//=========================
void table_fault_handler(struct Env * curenv, uint32 fault_va)
{
	//panic("table_fault_handler() is not implemented yet...!!");
	//Check if it's a stack page
	uint32* ptr_table;
#if USE_KHEAP
	{
		ptr_table = create_page_table(curenv->env_page_directory, (uint32)fault_va);
	}
#else
	{
		__static_cpt(curenv->env_page_directory, (uint32)fault_va, &ptr_table);
	}
#endif
}

//=========================
// [3] PAGE FAULT HANDLER:
//=========================
/* Calculate the number of page faults according th the OPTIMAL replacement strategy
 * Given:
 * 	1. Initial Working Set List (that the process started with)
 * 	2. Max Working Set Size
 * 	3. Page References List (contains the stream of referenced VAs till the process finished)
 *
 * 	IMPORTANT: This function SHOULD NOT change any of the given lists
 */
int get_optimal_num_faults(struct WS_List *initWorkingSet, int maxWSSize, struct PageRef_List *pageReferences)
{

#if USE_KHEAP

    int num_of_faults = 0;

    struct WS_List OPTWS;
    LIST_INIT(&OPTWS);

    //cprintf("[OPT] Start building initial OPTWS\n");
    //b3ml el optws
	struct WorkingSetElement *elem;
    {LIST_FOREACH_SAFE(elem, initWorkingSet , WorkingSetElement)
    {
        struct WorkingSetElement *elemOPT = kmalloc(sizeof(struct WorkingSetElement));
        elemOPT->virtual_address = elem->virtual_address;
        LIST_INSERT_TAIL(&OPTWS, elemOPT);

        //cprintf("[OPT]   Insert init WS VA=%x\n", elemOPT->virtual_address);
    }}

    struct PageRefElement *moveRef = LIST_FIRST(pageReferences);

    while(moveRef != NULL)
    {
        int found = 0;
        struct WorkingSetElement *wselem;

        // Search in WS lw el elem bta3 el ref fe el ws kda hit -> b7rk el ptr w akml
        {LIST_FOREACH_SAFE(wselem, &(OPTWS), WorkingSetElement)
        {
            if (wselem->virtual_address == moveRef->virtual_address)
            {
                found = 1;
                //cprintf("[OPT]   Hit! VA %x already in WS\n", moveRef->virtual_address);
                break;
            }
        }}

        if(found)
        {
            moveRef = LIST_NEXT(moveRef);
            continue;
        }

        // kda miss n5tar wsVic aw fi mkan n insert
        num_of_faults++;
        //cprintf("[OPT]   MISS → Page Fault! Total faults=%d\n", num_of_faults);

        //   n5tar wsVic
        if(LIST_SIZE(&(OPTWS)) == maxWSSize)
        {
            //cprintf("[OPT]   WS Full → Need to ewsVict a page\n");

            //int pagesToFind = maxWSSize;

                struct WorkingSetElement *wsVic;
				struct WorkingSetElement *victim = NULL;
                int maxdist = -1;
               {LIST_FOREACH_SAFE(wsVic, &(OPTWS), WorkingSetElement)
                {

					int dist = 0;
                    int found = 0;
                    struct PageRefElement *refwsVic = LIST_NEXT(moveRef);
					while(refwsVic != NULL){

						dist++;
						if( refwsVic->virtual_address == wsVic->virtual_address)
						{
						    wsVic->dist_for_opt = dist;
						    found = 1;
							break;
							//pagesToFind--;
						}
                      refwsVic = LIST_NEXT(refwsVic);
					}
					if(!found){
						//wsVic->dist_for_opt = -1;
						   victim = wsVic;
							break;
					}
					else{
						if(wsVic->dist_for_opt > maxdist){
							maxdist = wsVic->dist_for_opt;
							victim = wsVic;
						}
					}
                }}
			/*
				struct WorkingSetElement *victim = NULL;
                int maxdist = -1;
                {LIST_FOREACH_SAFE(wsVic, &(OPTWS), WorkingSetElement){

						if(wsVic->dist_for_opt==-1)
						{
							victim = wsVic;
							break;
						}

						if(wsVic->dist_for_opt > maxdist){
							maxdist = wsVic->dist_for_opt;
							victim = wsVic;
						}


				}}
*/
			    if (victim != NULL)
				{
					 victim->virtual_address = moveRef->virtual_address;
					 victim->dist_for_opt = -1;
				}



        }
        else
        {
            //feh makan f el ws
            struct WorkingSetElement *elemToAdd = kmalloc(sizeof(struct WorkingSetElement));
            elemToAdd->virtual_address = moveRef->virtual_address;
            LIST_INSERT_TAIL(&(OPTWS), elemToAdd);

            //cprintf("[OPT]   Added to WS (no ewsViction): %x\n", elemToAdd->virtual_address);
        }

        moveRef = LIST_NEXT(moveRef);
    }

	        struct WorkingSetElement *wsVic;
            {LIST_FOREACH_SAFE(wsVic, &(OPTWS), WorkingSetElement)
            {
                    LIST_REMOVE(&(OPTWS), wsVic);
                    kfree(wsVic);

            }}


    return num_of_faults;
#endif
    panic("USE_KHEAP IS 0");
}


void bn3ml_placement(struct Env * faulted_env, uint32 fault_va)
{
	 alloc_page(faulted_env->env_page_directory, fault_va, PERM_USER | PERM_WRITEABLE, 1);
	// hna bashof elpage 3la eldisk wla la
	int pageOnDisk = pf_read_env_page(faulted_env,(void*)fault_va);
	if(pageOnDisk == E_PAGE_NOT_EXIST_IN_PF)
	{
		//law mesh mawgoda w mesh stack or heap -> exit
	  if( !((fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX) || (fault_va > USTACKBOTTOM && fault_va <= USTACKTOP)))
	  {
		unmap_frame(faulted_env->env_page_directory,fault_va);
		env_exit();
	  }
	}
}


void page_fault_handler(struct Env * faulted_env, uint32 fault_va)
{
#if USE_KHEAP
	if (isPageReplacmentAlgorithmOPTIMAL())
	{
		//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #1 Optimal Reference Stream
		//Your code is here
		uint32 * ptr_table=NULL;
		uint32 va_page = ROUNDDOWN(fault_va, PAGE_SIZE);
		struct FrameInfo * cv=get_frame_info(faulted_env->env_page_directory,va_page,&ptr_table);
		bool found=0;
		if(cv==NULL)
		{
			bn3ml_placement(faulted_env,va_page);
		}
		else
		{
			pt_set_page_permissions(faulted_env->env_page_directory,va_page,PERM_PRESENT,0);
			struct WorkingSetElement * bndor = NULL;
			LIST_FOREACH_SAFE(bndor,&faulted_env->Active_WS,WorkingSetElement)
			{
				if(bndor->virtual_address == va_page)
				{
					found=1;
					break;
				}
			}
		}
		if(!found)
		{
			if(LIST_SIZE(&(faulted_env->Active_WS))==faulted_env->page_WS_max_size)
			{
				struct WorkingSetElement * bndor2 = NULL;
				LIST_FOREACH_SAFE(bndor2,&faulted_env->Active_WS,WorkingSetElement)
				{
					pt_set_page_permissions(faulted_env->env_page_directory,bndor2->virtual_address,0,PERM_PRESENT);
					LIST_REMOVE(&faulted_env->Active_WS,bndor2);
					kfree((void*)bndor2);
				}
			}
			struct WorkingSetElement *hn7to=env_page_ws_list_create_element(faulted_env,va_page);
			LIST_INSERT_TAIL(&faulted_env->Active_WS,hn7to);
		}
		struct PageRefElement * saf7a_gdeda= (struct PageRefElement*)kmalloc(sizeof(struct PageRefElement));
		saf7a_gdeda->virtual_address=va_page;
		LIST_INSERT_TAIL(&faulted_env->referenceStreamList,saf7a_gdeda);
		//Comment the following line
		//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
	}
	else
	{
		//env_page_ws_print(faulted_env);
		struct WorkingSetElement *victimWSElement = NULL;
		uint32 wsSize = LIST_SIZE(&(faulted_env->page_WS_list));
		if(wsSize < (faulted_env->page_WS_max_size))
		{
			//TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #3 placement
			//Your code is here
			uint32 va_page = ROUNDDOWN(fault_va, PAGE_SIZE);
			bn3ml_placement(faulted_env,va_page);
			struct WorkingSetElement* elgded_mno = env_page_ws_list_create_element(faulted_env, va_page);
			LIST_INSERT_TAIL(&(faulted_env->page_WS_list), elgded_mno);
			if (LIST_SIZE(&(faulted_env->page_WS_list)) == faulted_env->page_WS_max_size)
			{
				faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
			}
			else
			{
				faulted_env->page_last_WS_element = NULL;
			}
			//Comment the following line
//			panic("page_fault_handler().PLACEMENT is not implemented yet...!!");
		}
		else
		{
			if (isPageReplacmentAlgorithmCLOCK())
			{
				//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #3 Clock Replacement
				//Your code is here
				//found1=1;
				uint32 va_page = ROUNDDOWN(fault_va, PAGE_SIZE);
				struct WorkingSetElement * bndor;
				while(1)
				{
					bndor = LIST_FIRST(&faulted_env->page_WS_list);
					int habiba_perm=pt_get_page_permissions(faulted_env->env_page_directory,bndor->virtual_address);
					if(	!(habiba_perm & PERM_USED))
					{
						if((habiba_perm & PERM_MODIFIED))
						{
							uint32 * ptr_table=NULL;
							struct FrameInfo * ptr=get_frame_info(faulted_env->env_page_directory,bndor->virtual_address,&ptr_table);
							pf_update_env_page(faulted_env,bndor->virtual_address,ptr);
						}
						env_page_ws_invalidate(faulted_env,bndor->virtual_address);
//						unmap_frame(faulted_env->env_page_directory,bndor->virtual_address);
//						LIST_REMOVE(&faulted_env->page_WS_list,bndor);
//						kfree((void*)bndor);
						bn3ml_placement(faulted_env,va_page);
						//bndor->virtual_address = va_page;
						struct WorkingSetElement* elgded_mno = env_page_ws_list_create_element(faulted_env, va_page);
						LIST_INSERT_TAIL(&(faulted_env->page_WS_list), elgded_mno);
						break;
					}
					else
					{
						pt_set_page_permissions(faulted_env->env_page_directory,bndor->virtual_address,0,PERM_USED);
						//struct WorkingSetElement * nextcur=LIST_NEXT(bndor);
						faulted_env->page_last_WS_element = LIST_NEXT(bndor);
						LIST_REMOVE(&faulted_env->page_WS_list,bndor);
						LIST_INSERT_TAIL(&faulted_env->page_WS_list,bndor);
					}
				}
//				else
//				{
//					pt_set_page_permissions(faulted_env->env_page_directory,va_page,PERM_USED,0);
//				}
				//Comment the following line
				//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
				//env_page_ws_print(faulted_env);
			}
			else if (isPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX))
			{
				//TODO: [PROJECT'25.IM#6] FAULT HANDLER II - #2 LRU Aging Replacement
				//Your code is here
				uint32 va_page = ROUNDDOWN(fault_va, PAGE_SIZE);
				struct WorkingSetElement * bndor;
				struct WorkingSetElement * hn5rgo=LIST_FIRST(&faulted_env->page_WS_list);
				LIST_FOREACH_SAFE(bndor,&faulted_env->page_WS_list,WorkingSetElement)
				{
					if(hn5rgo->time_stamp > bndor->time_stamp)
					{
						hn5rgo=bndor;
					}
				}
				int salma_perm=pt_get_page_permissions(faulted_env->env_page_directory,hn5rgo->virtual_address);
				if((salma_perm & PERM_MODIFIED))
				{
					uint32 * ptr_table=NULL;
					struct FrameInfo * ptr=get_frame_info(faulted_env->env_page_directory,hn5rgo->virtual_address,&ptr_table);
					pf_update_env_page(faulted_env,hn5rgo->virtual_address,ptr);
				}
				//env_page_ws_invalidate(faulted_env,hn5rgo->virtual_address);
				unmap_frame(faulted_env->env_page_directory,hn5rgo->virtual_address);
				LIST_REMOVE(&faulted_env->page_WS_list,hn5rgo);
				kfree((void*)hn5rgo);
				bn3ml_placement(faulted_env,va_page);
				//victim->virtual_address=va_page;
				struct WorkingSetElement* newElement = env_page_ws_list_create_element(faulted_env, va_page);
				LIST_INSERT_TAIL(&faulted_env->page_WS_list,newElement);
				//Comment the following line
				//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
			}
			else if (isPageReplacmentAlgorithmModifiedCLOCK())
			{
				//TODO: [PROJECT'25.IM#6] FAULT HANDLER II - #3 Modified Clock Replacement
				//Your code is here
//				env_page_ws_print(faulted_env);
				uint32 va_page = ROUNDDOWN(fault_va, PAGE_SIZE);
				struct WorkingSetElement * hn5rgo = NULL;
				struct WorkingSetElement * salma = faulted_env->page_last_WS_element;
				struct WorkingSetElement * ely_ba3dooo= NULL;
				int perm_hn5rgo;
				while (hn5rgo == NULL)
				{
					for(int i = 0; i < LIST_SIZE(&(faulted_env->page_WS_list)); i++)
					{
						//env_page_ws_print(faulted_env);
						int salma_perm = pt_get_page_permissions(faulted_env->env_page_directory, salma->virtual_address);
						if(!(salma_perm & PERM_USED) && !(salma_perm & PERM_MODIFIED))
						{
							perm_hn5rgo=salma_perm;
							hn5rgo = salma;
							break;
						}
						ely_ba3dooo = LIST_NEXT(salma);
						faulted_env->page_last_WS_element = ely_ba3dooo;
						LIST_REMOVE(&(faulted_env->page_WS_list), salma);
						LIST_INSERT_TAIL(&(faulted_env->page_WS_list), salma);
						salma = ely_ba3dooo;
					}
					if(hn5rgo != NULL)
					{
						break;
					}
					for(int i = 0; i < LIST_SIZE(&(faulted_env->page_WS_list)); i++)
					{
						//env_page_ws_print(faulted_env);
						int salma_perm = pt_get_page_permissions(faulted_env->env_page_directory, salma->virtual_address);
						if(!(salma_perm & PERM_USED))
						{
							perm_hn5rgo=salma_perm;
							hn5rgo = salma;
							break;
						}
						else
						{
							pt_set_page_permissions(faulted_env->env_page_directory, salma->virtual_address, 0, PERM_USED);
						}
						ely_ba3dooo = LIST_NEXT(salma);
						faulted_env->page_last_WS_element = ely_ba3dooo;
						LIST_REMOVE(&(faulted_env->page_WS_list), salma);
						LIST_INSERT_TAIL(&(faulted_env->page_WS_list), salma);
						salma = ely_ba3dooo;
						//faulted_env->page_last_WS_element=el;
					}
				}
				//faulted_env->page_last_WS_element = LIST_NEXT(victim);
				if(perm_hn5rgo & PERM_MODIFIED)
				{
					uint32 * ptr_table=NULL;
					struct FrameInfo * ptr=get_frame_info(faulted_env->env_page_directory,hn5rgo->virtual_address,&ptr_table);
					pf_update_env_page(faulted_env,hn5rgo->virtual_address,ptr);
				}
				env_page_ws_invalidate(faulted_env,hn5rgo->virtual_address);
//				unmap_frame(faulted_env->env_page_directory,hn5rgo->virtual_address);
//				LIST_REMOVE(&faulted_env->page_WS_list,hn5rgo);
//				kfree((void*)hn5rgo);
				bn3ml_placement(faulted_env,va_page);
				//bndor->virtual_address = va_page;
				struct WorkingSetElement* newElement = env_page_ws_list_create_element(faulted_env, va_page);
				LIST_INSERT_TAIL(&(faulted_env->page_WS_list), newElement);
				//env_page_ws_print(faulted_env);
				//Comment the following line
				//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
			}
		}
	}
#endif
}

void __page_fault_handler_with_buffering(struct Env * curenv, uint32 fault_va)
{
	panic("this function is not required...!!");
}



