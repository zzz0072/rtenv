	.syntax unified
	.cpu cortex-m3
	.fpu softvfp
	.thumb
#include "syscall_def.h"

.global fork
fork:
	push {r7}
	mov r7, SYS_CALL_FORK
	svc 0
	nop
	pop {r7}
	bx lr
.global getpid
getpid:
	push {r7}
	mov r7, SYS_CALL_GETPID
	svc 0
	nop
	pop {r7}
	bx lr
.global write
write:
	push {r7}
	mov r7, SYS_CALL_WRITE
	svc 0
	nop
	pop {r7}
	bx lr
.global read
read:
	push {r7}
	mov r7, SYS_CALL_READ
	svc 0
	nop
	pop {r7}
	bx lr
.global interrupt_wait
interrupt_wait:
	push {r7}
	mov r7, SYS_CALL_WAIT_INTR
	svc 0
	nop
	pop {r7}
	bx lr
.global getpriority
getpriority:
	push {r7}
	mov r7, SYS_CALL_GETPRIORITY
	svc 0
	nop
	pop {r7}
	bx lr
.global setpriority
setpriority:
	push {r7}
	mov r7, SYS_CALL_SETPRIORITY
	svc 0
	nop
	pop {r7}
	bx lr
.global mknod
mknod:
	push {r7}
	mov r7, SYS_CALL_MK_NODE
	svc 0
	nop
	pop {r7}
	bx lr
.global sleep
sleep:
	push {r7}
	mov r7, SYS_CALL_SLEEP
	svc 0
	nop
	pop {r7}
	bx lr
