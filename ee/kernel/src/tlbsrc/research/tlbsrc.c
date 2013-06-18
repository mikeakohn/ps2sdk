#include <kernel.h>
#include <ee_cop0_defs.h>

/* #define Index	$0
#define EntryLo0	$2
#define EntryLo1	$3
#define PageMask	$5
#define Wired		$6
#define EntryHi		$10 */

struct SyscallPatchData{
	unsigned int syscall;
	void *function;
};

static unsigned int unknown;	/* 0x80075330 */

//Function prototypes:
static int PutTLBEntry(unsigned int PageMask, unsigned int EntryHi, unsigned int EntryLo0, unsigned int EntryLo1);
static int SetTLBEntry(unsigned int index, unsigned int PageMask, unsigned int EntryHi, unsigned int EntryLo0, unsigned int EntryLo1);
static int GetTLBEntry(unsigned int index, unsigned int *PageMask, unsigned int *EntryHi, unsigned int *EntryLo0, unsigned int *EntryLo1);
static int ProbeTLBEntry(unsigned int EntryHi, unsigned int *PageMask, unsigned int *EntryLo0, unsigned int *EntryLo1);
static int ExpandScratchPad(unsigned int page);

static const struct SyscallPatchData SyscallPatchData[]={
	{ 0x55, &PutTLBEntry},
	{ 0x56, &SetTLBEntry},
	{ 0x57, &GetTLBEntry},
	{ 0x58, &ProbeTLBEntry},
	{ 0x59, &ExpandScratchPad},
	{ 0x03, &unknown},
};

/* 0x80075000 */
int _start(int syscall){
	unsigned int i;

	for(i=0; i<6; i++){
		if(SyscallPatchData[i].syscall==syscall){
			return((unsigned int)SyscallPatchData[i].function);
		}
	}

	return 0;
}

/* 0x80075038 */
static int PutTLBEntry(unsigned int PageMask, unsigned int EntryHi, unsigned int EntryLo0, unsigned int EntryLo1){
	int result;

	switch(EntryHi>>24){
		case 0x40:
		case 0x30:
		case 0x20:
		case 0x00:
			__asm volatile(	"mtc0 %0, PageMask\n"
					"mtc0 %1, EntryHi\n"
					"mtc0 %2, EntryLo0\n"
					"mtc0 %3, EntryLo1\n"
					"sync.p\n"
					"tlbwr\n"
					"sync.p\n"
					"tlbp\n"
					"sync.p\n"
					"mfc0 %4, Index\n" ::"r"(PageMask),"r"(EntryHi),"r"(EntryLo0),"r"(EntryLo1),"r"(result));
			break;
		case 0x50:
		case 0x10:
			result=-1;
	}

	return result;
}

/* 0x800750c8 */
static int SetTLBEntry(unsigned int index, unsigned int PageMask, unsigned int EntryHi, unsigned int EntryLo0, unsigned int EntryLo1){
	int result;

	if(index<0x30){
		__asm volatile(	"mtc0 %0, Index\n"
				"mtc0 %1, PageMask\n"
				"mtc0 %2, EntryHi\n"
				"mtc0 %3, EntryLo0\n"
				"mtc0 %4, EntryLo1\n"
				"sync.p\n"
				"tlbwi\n"
				"sync.p\n" ::"r"(index),"r"(PageMask),"r"(EntryHi),"r"(EntryLo0),"r"(EntryLo1));

		result=index;
	}
	else result=-1;

	return result;
}

/* 0x80075108 */
static int GetTLBEntry(unsigned int index, unsigned int *PageMask, unsigned int *EntryHi, unsigned int *EntryLo0, unsigned int *EntryLo1){
	int result;

	if(index<0x30){
		__asm volatile(	"mtc0 %0, Index\n"
				"sync.p\n"
				"tlbr\n"
				"sync.p\n"
				"mfc0 $v0, PageMask\n"
				"sw $v0, (%1)\n"
				"mfc0 $v0, EntryHi\n"
				"sw $v0, (%2)\n"
				"mfc0 $v0, EntryLo0\n"
				"sw $v0, (%3)\n"
				"mfc0 $v0, EntryLo1\n"
				"sw $v0, (%4)\n" ::"r"(index),"r"(PageMask),"r"(EntryHi),"r"(EntryLo0),"r"(EntryLo1));

		result=index;
	}
	else result=-1;

	return result;
}

/* 0x80075158 */
static int ProbeTLBEntry(unsigned int EntryHi, unsigned int *PageMask, unsigned int *EntryLo0, unsigned int *EntryLo1){
	int result, index;

	__asm volatile(	"mtc0 %0, EntryHi\n"
			"sync.p\n"
			"tlbp\n"
			"sync.p\n"
			"mfc0 %0, Index\n" ::"r"(EntryHi),"r"(index));

	if(index>=0){
		__asm volatile(	"tlbr\n"
				"sync.p\n"
				"mfc0 $v0, PageMask\n"
				"sw $v0, (%0)\n"
				"mfc0 $v1, EntryLo0\n"
				"sw $v1, (%1)\n"
				"mfc0 $v0, EntryLo1\n"
				"sw $v0, (%2)\n" ::"r"(PageMask),"r"(EntryLo0),"r"(EntryLo1));

		result=index;
	}
	else result=-1;

	return result;
}

/* 0x800751a8 */
static int ExpandScratchPad(unsigned int page){
	int result, index;
	unsigned int PageMask, EntryHi, EntryLo0, EntryLo1;

	if(!(page&0xFFF)){
		if(0xFFFFE<page-1){
			if((index=ProbeTLBEntry(0x70004000, &PageMask, &EntryLo0, &EntryLo1))>=0){
				if(page==0){
					EntryHi=0xE0010000+((index-1)<<13);

					__asm volatile(	"mfc0 $v0, Wired\n"
							"addiu $v0, $v0, 0xFFFF\n"
							"mtc0 $v0, Wired\n"
							"mtc0 %0, Index\n"
							"mtc0 $zero, PageMask\n"
							"mtc0 %1, EntryHi\n"
							"mtc0 $zero, EntryLo0\n"
							"mtc0 $zero, EntryLo1\n"
							"sync.p\n"
							"tlbwi\n"
							"sync.p\n" ::"r"(index),"r"(EntryHi));
				}
				else{
					__asm volatile(	"mfc0 %0, Wired\n"
							"addiu $v0, %0, 1\n"
							"mtc0 $v0, Wired\n" ::"r"(index));
				}
			}

			if(page!=0){
			/*	Not sure why this code saves the EntryLo0 and EntryLo1 values on the stack, and sets a word on the stack to zero, but does not use them:

				0($sp)=0
				4($sp)=$v0=(page+0x1000&0xFFFFF000)>>6|0x1F
				8($sp)=$a0=(page&0xFFFFF000)>>6|0x1F */

				EntryHi=0x70004000;
				EntryLo0=((page+0x1000)&0xFFFFF000)>>6|0x1F;
				EntryLo1=(page&0xFFFFF000)>>6|0x1F;

				__asm volatile(	"mtc0 %0, Index\n"
						"daddu $v1, $zero, $zero\n"
						"mtc0 $v1, PageMask\n"
						"mtc0 %1, EntryHi\n"
						"mtc0 %2, EntryLo0\n"
						"mtc0 %3, EntryLo1\n"
						"sync.p\n"
						"tlbwi\n"
						"sync.p\n" ::"r"(index),"r"(EntryHi),"r"(EntryLo0),"r"(EntryLo1));

				result=index;
			}
			else result=0;
		}
		else result=-1;
	}
	else result=-1;

	return result;
}
