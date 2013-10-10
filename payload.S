@ bootrom dumper payload
@ (c) pod2g october 2010
@ (c) x56   october 2013

@constants -----------------------------------
.pool

@ return to start at offset:
.set	s5l8930_ret_addr,		0x7ef	@ A4
.set	s5l892x_ret_addr,		0x8b7	@ iPod 3G, iPhone 3GS

@ bootrom version:
.set	s5l8930_bootrom,		0x00342e34
.set	addr_bootrom_version,		0x288

.set	loadaddr,			0x84000000
.set	maxsize,			0x24000
.set	dumpaddr,			0x0
.set	dumpto,				0x84000000
.set	dumpsize,			0x10000

.text
@main code -----------------------------------
.code 16
_start: .global _start
	B	entry_point
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP

entry_point:
	@ copy bootrom to loadaddr for dumping
	LDR	R0,	=dumpto
	LDR	R1,	=dumpaddr
	LDR	R2,	=dumpsize
	BL	memcpy
	
	@ detect bootrom version
	LDR	R0,	=addr_bootrom_version
	LDR	R2,	[R0]
	LDR	R0,	=s5l8930_bootrom
	CMP	R0,	R2
	BNE	_not_s5l8930
	LDR	R3,	=s5l8930_ret_addr
	B	_return
_not_s5l8930:
	LDR	R3,	=s5l892x_ret_addr

_return:
	@ set up return values	
	LDR	R0,	=loadaddr
	LDR	R1,	=maxsize
	MOV	R2,	#0
	
	BLX	R3
@-----------------------------------------------------
memcpy:

_memcpy_loop:
	LDRB	R3,	[R1]
	STRB	R3,	[R0]
	ADD	R0,	#1
	ADD	R1,	#1
	SUB	R2,	#1
	CMP	R2,	#0
	BNE	_memcpy_loop

	BX	LR
@-----------------------------------------------------
.end

