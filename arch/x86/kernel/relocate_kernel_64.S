/*
 * relocate_kernel.S - put the kernel image in place to boot
 * Copyright (C) 2002-2005 Eric Biederman  <ebiederm@xmission.com>
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

#include <linux/linkage.h>
#include <asm/page_types.h>
#include <asm/kexec.h>
#include <asm/processor-flags.h>
#include <asm/pgtable_types.h>

/*
 * Must be relocatable PIC code callable as a C function
 */

#define PTR(x) (x << 3)
#define PAGE_ATTR (_PAGE_PRESENT | _PAGE_RW | _PAGE_ACCESSED | _PAGE_DIRTY)

/*
 * control_page + KEXEC_CONTROL_CODE_MAX_SIZE
 * ~ control_page + PAGE_SIZE are used as data storage and stack for
 * jumping back
 */
#define DATA(offset)		(KEXEC_CONTROL_CODE_MAX_SIZE+(offset))

/* Minimal CPU state */
#define RSP			DATA(0x0)
#define CR0			DATA(0x8)
#define CR3			DATA(0x10)
#define CR4			DATA(0x18)

/* other data */
#define CP_PA_TABLE_PAGE	DATA(0x20)
#define CP_PA_SWAP_PAGE		DATA(0x28)
#define CP_PA_BACKUP_PAGES_MAP	DATA(0x30)

	.text
	.align PAGE_SIZE
	.code64
	.globl relocate_kernel
relocate_kernel:
	/*
	 * %rdi indirection_page
	 * %rsi page_list
	 * %rdx start address
	 * %rcx preserve_context
	 */

	/* Save the CPU context, used for jumping back */
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	pushf

	movq	PTR(VA_CONTROL_PAGE)(%rsi), %r11
	movq	%rsp, RSP(%r11)
	movq	%cr0, %rax
	movq	%rax, CR0(%r11)
	movq	%cr3, %rax
	movq	%rax, CR3(%r11)
	movq	%cr4, %rax
	movq	%rax, CR4(%r11)

	/* zero out flags, and disable interrupts */
	pushq $0
	popfq

	/*
	 * get physical address of control page now
	 * this is impossible after page table switch
	 */
	movq	PTR(PA_CONTROL_PAGE)(%rsi), %r8

	/* get physical address of page table now too */
	movq	PTR(PA_TABLE_PAGE)(%rsi), %r9

	/* get physical address of swap page now */
	movq	PTR(PA_SWAP_PAGE)(%rsi), %r10

	/* save some information for jumping back */
	movq	%r9, CP_PA_TABLE_PAGE(%r11)
	movq	%r10, CP_PA_SWAP_PAGE(%r11)
	movq	%rdi, CP_PA_BACKUP_PAGES_MAP(%r11)

	/* Switch to the identity mapped page tables */
	movq	%r9, %cr3

	/* setup a new stack at the end of the physical control page */
	lea	PAGE_SIZE(%r8), %rsp

	/* jump to identity mapped page */
	addq	$(identity_mapped - relocate_kernel), %r8
	jmp	*%r8

identity_mapped:
	/* set return address to 0 if not preserving context */
	pushq	$0
	/* store the start address on the stack */
	pushq   %rdx

	/*
	 * Set cr0 to a known state:
	 *  - Paging enabled
	 *  - Alignment check disabled
	 *  - Write protect disabled
	 *  - No task switch
	 *  - Don't do FP software emulation.
	 *  - Proctected mode enabled
	 */
	movq	%cr0, %rax
	andq	$~(X86_CR0_AM | X86_CR0_WP | X86_CR0_TS | X86_CR0_EM), %rax
	orl	$(X86_CR0_PG | X86_CR0_PE), %eax
	movq	%rax, %cr0

	/*
	 * Set cr4 to a known state:
	 *  - physical address extension enabled
	 */
	movq	$X86_CR4_PAE, %rax
	movq	%rax, %cr4

	jmp 1f
1:

	/* Flush the TLB (needed?) */
	movq	%r9, %cr3

	movq	%rcx, %r11
	call	swap_pages

	/*
	 * To be certain of avoiding problems with self-modifying code
	 * I need to execute a serializing instruction here.
	 * So I flush the TLB by reloading %cr3 here, it's handy,
	 * and not processor dependent.
	 */
	movq	%cr3, %rax
	movq	%rax, %cr3

	/*
	 * set all of the registers to known values
	 * leave %rsp alone
	 */

	testq	%r11, %r11
	jnz 1f
	xorq	%rax, %rax
	xorq	%rbx, %rbx
	xorq    %rcx, %rcx
	xorq    %rdx, %rdx
	xorq    %rsi, %rsi
	xorq    %rdi, %rdi
	xorq    %rbp, %rbp
	xorq	%r8,  %r8
	xorq	%r9,  %r9
	xorq	%r10, %r10
	xorq	%r11, %r11
	xorq	%r12, %r12
	xorq	%r13, %r13
	xorq	%r14, %r14
	xorq	%r15, %r15

	ret

1:
	popq	%rdx
	leaq	PAGE_SIZE(%r10), %rsp
	call	*%rdx

	/* get the re-entry point of the peer system */
	movq	0(%rsp), %rbp
	call	1f
1:
	popq	%r8
	subq	$(1b - relocate_kernel), %r8
	movq	CP_PA_SWAP_PAGE(%r8), %r10
	movq	CP_PA_BACKUP_PAGES_MAP(%r8), %rdi
	movq	CP_PA_TABLE_PAGE(%r8), %rax
	movq	%rax, %cr3
	lea	PAGE_SIZE(%r8), %rsp
	call	swap_pages
	movq	$virtual_mapped, %rax
	pushq	%rax
	ret

virtual_mapped:
	movq	RSP(%r8), %rsp
	movq	CR4(%r8), %rax
	movq	%rax, %cr4
	movq	CR3(%r8), %rax
	movq	CR0(%r8), %r8
	movq	%rax, %cr3
	movq	%r8, %cr0
	movq	%rbp, %rax

	popf
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbp
	popq	%rbx
	ret

	/* Do the copies */
swap_pages:
	movq	%rdi, %rcx 	/* Put the page_list in %rcx */
	xorq	%rdi, %rdi
	xorq	%rsi, %rsi
	jmp	1f

0:	/* top, read another word for the indirection page */

	movq	(%rbx), %rcx
	addq	$8,	%rbx
1:
	testq	$0x1,	%rcx  /* is it a destination page? */
	jz	2f
	movq	%rcx,	%rdi
	andq	$0xfffffffffffff000, %rdi
	jmp	0b
2:
	testq	$0x2,	%rcx  /* is it an indirection page? */
	jz	2f
	movq	%rcx,   %rbx
	andq	$0xfffffffffffff000, %rbx
	jmp	0b
2:
	testq	$0x4,	%rcx  /* is it the done indicator? */
	jz	2f
	jmp	3f
2:
	testq	$0x8,	%rcx  /* is it the source indicator? */
	jz	0b	      /* Ignore it otherwise */
	movq	%rcx,   %rsi  /* For ever source page do a copy */
	andq	$0xfffffffffffff000, %rsi

	movq	%rdi, %rdx
	movq	%rsi, %rax

	movq	%r10, %rdi
	movq	$512,   %rcx
	rep ; movsq

	movq	%rax, %rdi
	movq	%rdx, %rsi
	movq	$512,   %rcx
	rep ; movsq

	movq	%rdx, %rdi
	movq	%r10, %rsi
	movq	$512,   %rcx
	rep ; movsq

	lea	PAGE_SIZE(%rax), %rsi
	jmp	0b
3:
	ret

	.globl kexec_control_code_size
.set kexec_control_code_size, . - relocate_kernel
