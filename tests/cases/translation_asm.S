#define WORD 0x12345678
.text
.globl translation_asm
translation_asm:
	mov $WORD, %eax
	ret
.pushsection .data
	.long WORD
.popsection
