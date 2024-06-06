/**********************************************************************
 * Copyright (c) 2020-2024
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list_head.h"
#include "vm.h"

/**
 * Ready queue of the system
 */
extern struct list_head processes;

/**
 * Currently running process
 */
extern struct process *current;

/**
 * Page Table Base Register that MMU will walk through for address translation
 */
extern struct pagetable *ptbr;

/**
 * TLB of the system.
 */
extern struct tlb_entry tlb[];


/**
 * The number of mappings for each page frame. Can be used to determine how
 * many processes are using the page frames.
 */
extern unsigned int mapcounts[];


/**
 * lookup_tlb(@vpn, @rw, @pfn)
 *
 * DESCRIPTION
 *   Translate @vpn of the current process through TLB. DO NOT make your own
 *   data structure for TLB, but should use the defined @tlb data structure
 *   to translate. If the requested VPN exists in the TLB and it has the same
 *   rw flag, return true with @pfn is set to its PFN. Otherwise, return false.
 *   The framework calls this function when needed, so do not call
 *   this function manually.
 *
 * RETURN
 *   Return true if the translation is cached in the TLB.
 *   Return false otherwise
 */

// 0x01 => read 1 3
// 0x02 => write 이건 출력하면 안됨
// 0x03 => read write
bool lookup_tlb(unsigned int vpn, unsigned int rw, unsigned int *pfn)
{
	for(int i = 0; i < NR_TLB_ENTRIES; i++) {
		if((tlb[i].valid == true) && (tlb[i].vpn == vpn)) {
			if(tlb[i].rw >= rw) { 
				*pfn = tlb[i].pfn;
				return true;
			}
		}
	}
	return false;
}


/**
 * insert_tlb(@vpn, @rw, @pfn)
 *
 * DESCRIPTION
 *   Insert the mapping from @vpn to @pfn for @rw into the TLB. The framework will
 *   call this function when required, so no need to call this function manually.
 *   Note that if there exists an entry for @vpn already, just update it accordingly
 *   rather than removing it or creating a new entry.
 *   Also, in the current simulator, TLB is big enough to cache all the entries of
 *   the current page table, so don't worry about TLB entry eviction. ;-)
 */
void insert_tlb(unsigned int vpn, unsigned int rw, unsigned int pfn)
{

	// page frame만큼 반복
	for(int i = 0; i < NR_PAGEFRAMES; i++) {
		// 고려 해야 할 점 => tlb에 있을 때는 업데이트, 없을 때는 생성 크기는 신경 x
		if (tlb[i].valid == true && tlb[i].vpn == vpn) {
			tlb[i].pfn = pfn;
			tlb[i].rw = rw;
			break;
		}
		// valid bit가 없다면 
		else if (tlb[i].valid == false) {
			tlb[i].valid = true;
			tlb[i].rw = rw;
			tlb[i].pfn = pfn;
			tlb[i].vpn = vpn;
			tlb[i].private = rw;
			break;
		}
	}
}

/**
 * alloc_page(@vpn, @rw)
 *
 * DESCRIPTION
 *   Allocate a page frame that is not allocated to any process, and map it
 *   to @vpn. When the system has multiple free pages, this function should
 *   allocate the page frame with the **smallest pfn**.
 *   You may construct the page table of the @current process. When the page
 *   is allocated with ACCESS_WRITE flag, the page may be later accessed for writes.
 *   However, the pages populated with ACCESS_READ should not be accessible with
 *   ACCESS_WRITE accesses.
 *
 * RETURN
 *   Return allocated page frame number.
 *   Return -1 if all page frames are allocated.
 */
unsigned int alloc_page(unsigned int vpn, unsigned int rw)
{
	unsigned int pfn = 0; // page frame number
	unsigned int pd_idx = vpn / NR_PTES_PER_PAGE; // page directory's index 
	unsigned int pte_idx = vpn % NR_PTES_PER_PAGE; // smallest pfn index 

	// physical memory 검사
	for (int i = 0; i < NR_PAGEFRAMES; i++, pfn++) {
		// 다른 process에 의해 mapping 됐는지 체크
		if(mapcounts[i] == 0) {
			mapcounts[i]++; // pte랑 mapping됨 

			struct pte_directory *pte_dir = current->pagetable.pdes[pd_idx]; //pte_dir
			// allocate check
			if(pte_dir == NULL) {
				pte_dir = malloc(sizeof(struct pte_directory));
				current->pagetable.pdes[pd_idx] = pte_dir;
			}
			// pte => 이건 pte_dir 하위임
			struct pte *pte = &pte_dir->ptes[pte_idx];
			// set page table entry's value
			pte->valid = true;
			pte->rw = rw; 
			pte->private = rw; 
			pte->pfn = pfn;

			return pfn;
		}

	}
	return -1;
}


/**
 * free_page(@vpn)
 *
 * DESCRIPTION
 *   Deallocate the page from the current processor. Make sure that the fields
 *   for the corresponding PTE (valid, rw, pfn) is set @false or 0.
 *   Also, consider the case when a page is shared by two processes,
 *   and one process is about to free the page. Also, think about TLB as well ;-)
 */
void free_page(unsigned int vpn)
{
	// pd_idx, pte_idx -> alloc 기능에서 정의한 것 처럼, map_cnt는 
	unsigned int pd_idx = vpn / NR_PTES_PER_PAGE;
	unsigned int pte_idx = vpn % NR_PTES_PER_PAGE;
	int map_cnt = 0;
	
	struct pte_directory *pte_dir = current->pagetable.pdes[pd_idx];
	struct pte *pte = &current->pagetable.pdes[pd_idx]->ptes[pte_idx]; // pte 

	// if pte_dir is allocated, exist pte in pte_dir
	if (pte_dir != NULL) {
		// pte's valid check
		if(pte->valid == true) {
			// deallocate
			pte->valid = false;
			pte->rw = 0;
			pte->private = 0;
			//mapcount save
			map_cnt = pte->pfn;
			pte->pfn = 0;
			// count down input vpn's map count
			mapcounts[map_cnt]--;
			
			// tlb도 없애주자 tlb는 cache니까 free를 해주면 같이 없애줘야 함
			for (int i = 0; i < NR_TLB_ENTRIES; i++) {
				if ((tlb[i].valid == true) && (tlb[i].vpn == vpn)) {
					tlb[i].valid = false;
					tlb[i].vpn = 0;
					tlb[i].private = 0;
					tlb[i].pfn = 0;
					tlb[i].rw = 0;
					break;
				}	
			}
		}
	}
}

/**
 * handle_page_fault()
 *
 * DESCRIPTION
 *   Handle the page fault for accessing @vpn for @rw. This function is called
 *   by the framework when the __translate() for @vpn fails. This implies;
 *   0. page directory is invalid
 *   1. pte is invalid
 *   2. pte is not writable but @rw is for write
 *   This function should identify the situation, and do the copy-on-write if
 *   necessary.
 *
 * RETURN
 *   @true on successful fault handling
 *   @false otherwise
 */
bool handle_page_fault(unsigned int vpn, unsigned int rw)
{
	unsigned int pd_idx = vpn / NR_PTES_PER_PAGE;
	unsigned int pte_idx = vpn % NR_PTES_PER_PAGE;
	
	struct pte *pte = &current->pagetable.pdes[pd_idx]->ptes[pte_idx]; // pte declare

	//pte valid bit가 false거나 null이면 false를 주고
	if (pte->valid == false || pte == NULL) {
		return false;
	}
	// 나머지 경우에는 handling을 해주면 되겠네
	else {
		// fork했는데 private이랑 rw가 다르고, rw가 읽는 경우는 자식 프로세스가 읽는 경우니까
		// 그에 맞게 다른 physical memory allocate 해줘야 함
		if ((pte->private != ACCESS_READ) && (pte->rw == ACCESS_READ) && (rw > ACCESS_READ)) {
			// mapcount가 1보다 크면 두개 이상 할당 된 거니까 다른 애로 다시 할당해야 함
			if(mapcounts[pte->pfn] > 1) {
				// 할당하는 함수를 불러와서 새롭게 할당해주고 해당하는 mapcount는 감소해주면 됨
				rw = 0x03;
				mapcounts[pte->pfn]--;
				unsigned int new_pfn = alloc_page(vpn, rw);
				return true;
			}
			// 1보다 작거나 같은 경우는 걍 할당해주면 되겠네
			else {
				pte->rw = 0x03;
				return true;
			}
		}
	}
	return false;
}


/**
 * switch_process()
 *
 * DESCRIPTION
 *   If there is a process with @pid in @processes, switch to the process.
 *   The @current process at the moment should be put into the @processes
 *   list, and @current should be replaced to the requested process.
 *   Make sure that the next process is unlinked from the @processes, and
 *   @ptbr is set properly.
 *
 *   If there is no process with @pid in the @processes list, fork a process
 *   from the @current. This implies the forked child process should have
 *   the identical page table entry 'values' to its parent's (i.e., @current)
 *   page table. 
 *   To implement the copy-on-write feature, you should manipulate the writable
 *   bit in PTE and mapcounts for shared pages. You may use pte->private for 
 *   storing some useful information :-)
 */
void switch_process(unsigned int pid)
{	
	struct process *change;
	bool is_change = false;
	// process linked list 순회
	list_for_each_entry(change, &processes, list) {
		// if first process exist in process's linkded list
		if(change->pid == pid) {
			current = change; // current process는 pid가 같은 다른 process로 변경
			ptbr = &current->pagetable; // ptbr은 current pagetable의 address니까 change
			list_del_init(&change->list);
			is_change = true;
			break; // if process change => loop exit
		}
	}
	// process isn't in processes => fork process
	if(is_change == false) {
		change = malloc(sizeof(struct process));
		INIT_LIST_HEAD(&change->list);
		change->pid = pid;

		// pte_directory 만큼 반복문 why? physical 메모리에 매핑 해야 함
		for (int i = 0; i < NR_PDES_PER_PAGE; i++) {			
			// 이건 pte_dir 주소값 이걸로 첫번째 테이블 녀석 주소값 갖고오자 (0-3)
			struct pte_directory *pte_dir;
			struct pte_directory *temp = malloc(sizeof(struct pte_directory));
			pte_dir = current->pagetable.pdes[i];
			// pte_dir가 안 비어있을 때만 계속 진행
			if(pte_dir != NULL) {
				change->pagetable.pdes[i] = temp;
				// 테이블 디렉토리 밑에 있는 애들 16번 씩 돌아서 pte 갖고오게
				for (int j = 0; j < NR_PTES_PER_PAGE; j++) {
					struct pte *change_pte = &change->pagetable.pdes[i]->ptes[j];
					struct pte *current_pte = &pte_dir->ptes[j]; 
					// valid bit가 true일 때만
					if(current_pte->valid == true) {
						//원래 값이랑 바꿀 값이랑 복사해주고
						memcpy(change_pte, current_pte, sizeof(struct pte));
						mapcounts[current_pte->pfn]++; // physical mem 에 mapping 
						change_pte->pfn = current_pte->pfn; // pfn도 추가

						// rw 일 때는 프로세스들이 w되면 안되고 read bit만
						if(current_pte->rw == 0x03) {
							change_pte->rw = 0x01;
							current_pte->rw = 0x01;
						}
						// 반대는 rw 가능
						else {
							change_pte->rw = current_pte->rw;
						}
					}
				}
			}
		}
		list_add_tail(&current->list, &processes);
		current = change;
		ptbr = &current->pagetable;
	}

	for(int i = 0; i < NR_PAGEFRAMES; i++) {
		tlb[i].valid = false;
	}
}