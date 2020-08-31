/*
 * boot.S for memos-2
 *
 */
        .globl stack

        .bss
        .align 0x1000
        .comm stack, 0x1000 /* setup 4KB stack area in bss */

        .data


gdt_start:
gdt_null:
        .long 0x0
        .long 0x0
gdt_cs:
        .long 0x0000FFFF
        .long 0x00CF9800
gdt_ds:
        .long 0x0000FFFF
        .long 0x00CF9200
gdt_end:

gdt_descriptor:
        .word gdt_end - gdt_start
        .long gdt_start


idt_begin: 
idt: 
	.zero 256
int_32:
	.long 0x00081000
	.long 0x00108E00
idt_end: 
	.zero 16

idt_info: 
	.word idt_end - idt_begin 
	.long idt_begin

		
        .text
test:
	mov 8(%ebp), %eax       #eax holds pointer to TCB
        push %ebx               #save value of ebx
        mov $1, %ebx 
        mov %ebx, 24(%eax)
        mov 4(%esp), %ebx        #ebx now holds return address into threadFunction
        mov %ebx, 16(%eax)      #put that into TCB's isp
        pop %ebx                #restore ebx
        add $8, %esp
        popfl
        cli
        pusha
        mov %esp, 12(%eax)
        mov %ebp, %esp
        pop %ebp
        mov	 $0x20, %al	# set bit 4 of OCW 2
	out	%al, $0x20	# write to primary PIC command register
        ret



setup_pic: 
	mov $0x11, %al	#ICW1 used for initialization of PIC 
	out %al, $0x20  

	mov $0x20, %al  	#ICW2, used to point where the IVT for IRQs are 
	out %al, $0x21          #PIC1 now starts at 32



	mov $0x01, %al
	out %al,$0x21
	
	in $0x21, %al
	mov $0x1, %al    #Masking every interrupt besides IRQ 0 (Timer Interrupt) using 11111110
	out %al,$0x21
	
setup_pit: 
	mov $0x34, %al
	out %al, $0x43     #tell the PIT which channel we're setting

	mov $11931, %ax
	out %al,$0x40    #send low byte
	mov %ah, %al
	out %al, $0x40    #send high byte
	jmp after
	
	
        .globl _start


_start:
        jmp real_start

        /* Multiboot header -- Safe to place this header in 1st page for GRUB */
        .align 4
        .long 0x1BADB002 /* Multiboot magic number */
        .long 0x00000003 /* Align modules to 4KB, req. mem size */
                         /* See 'info multiboot' for further info */
        .long 0xE4524FFB /* Checksum */

real_start:
        lgdt gdt_descriptor
	jmp $0x08, $gdt_flush


gdt_flush:
        mov $0x10, %ax
        mov %ax, %ds
        mov %ax, %es
        mov %ax, %fs
        mov %ax, %gs
        mov %ax, %ss
	cli
	lidt idt_info
	jmp setup_pic
after:
        /* set up stack */
        movl $stack+0x1000, %esp /* setup 4Kbyte stack */
        movl $stack+0x1000, %ebp

        /* save multiboot parameter, for eventual call to C code */
        pushl %ebx
        call init /* start of C code */


        /* In case we return from the call, we want to suspend the processor */
	call main
        
        cli
        hlt


loop:
        jmp loop


        .globl runThread
        .type runThread, @function
runThread:

        /*Prologue*/
        push %ebp
        mov  %esp, %ebp
        push %edi
        push %ebx

        /*Body*/
        pusha
        mov 8(%ebp), %edi       /* edi holds the TCB*/
        mov 8(%edi), %esp       /*Sets the new stack */
        
        /*Check if its been preempted */
        cmp $0, 24(%edi)
        jne preempted   
        push %edi                       #passes the TCB as a parameter
        sti
        call threadFunction
        cli
        add $4, %esp
        jmp finish
preempted:
        mov 12(%edi), %esp
        popa
        mov 8(%ebp), %eax
        sti
        jmp *16(%eax) 
finish:
        mov %ebp, %ebx
        sub $40, %ebx
        mov %ebx, %esp
        popa
        mov $0, %eax

        /*Epilogue*/
        pop %ebx
        pop %edi
        mov %ebp, %esp
        pop %ebp
        ret
        

        .globl yield
        .type yield, @function
yield:

        mov 4(%esp), %eax       /*now eax points to TCB*/
        push %ebx
        mov 4(%esp), %ebx
        mov %ebx, 16(%eax)      /*moved the return address from yeild to threadFunction into TCB
        pop %ebx 
        add $8, %esp          /*set esp to before function call (just changed from sub to add*/
        pusha                 /* saved the state of all the registers*/
        mov %esp, 12(%eax)    /*saved the top of the stack into TCB*/
        mov %ebp, %esp        /* set esp to the ebp of threadFunction*/
        pop %ebp              /* pops ebp of runThread*/
        ret                   /* returns to runThread*/


		
		
########################################################		
#	Sources: 										   #
#													   #
#	http://www.brokenthorn.com/Resources/OSDevPit.html #
#	http://www.brokenthorn.com/Resources/OSDevPic.html #
#													   #
########################################################		