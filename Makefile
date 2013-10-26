# Variables
CROSS_COMPILE=arm-none-eabi-
QEMU_STM32 ?= ../qemu_stm32/arm-softmmu/qemu-system-arm

ARCH=CM3
VENDOR=ST
PLAT=STM32F10x

# Build options
USE_ASM_OPTI_FUNC=YES
USE_SEMIHOST=YES

# PATH
LIBDIR = .
CMSIS_LIB=$(LIBDIR)/libraries/CMSIS/$(ARCH)
STM32_LIB=$(LIBDIR)/libraries/STM32F10x_StdPeriph_Driver

# Sources
CMSIS_PLAT_SRC = $(CMSIS_LIB)/DeviceSupport/$(VENDOR)/$(PLAT)

CMSIS_SRCS = \
		$(CMSIS_LIB)/CoreSupport/core_cm3.c \
		$(CMSIS_PLAT_SRC)/system_stm32f10x.c \
		$(CMSIS_PLAT_SRC)/startup/gcc_ride7/startup_stm32f10x_md.s

STM32_SRCS = \
		$(STM32_LIB)/src/stm32f10x_rcc.c \
		$(STM32_LIB)/src/stm32f10x_gpio.c \
		$(STM32_LIB)/src/stm32f10x_usart.c \
		$(STM32_LIB)/src/stm32f10x_exti.c \
		$(STM32_LIB)/src/misc.c

SRCS= \
		$(CMSIS_SRCS) \
		$(STM32_SRCS) \
		context_switch.s \
		syscall.c \
		stm32_p103.c \
		kernel.c \
		str_util.c \
		task.c \
		path_server.c \
		serial.c \
		shell.c

HEADERS = \
		syscall.h \
		str_util.h \
		serial.h \
		shell.h \
		task.h \
		path_server.h \
		common_define.h

# Flags
ifeq ($(USE_ASM_OPTI_FUNC),YES)
	SRCS+=memcpy.s

	CFLAGS=-DUSE_ASM_OPTI_FUNC
endif

ifeq ($(USE_SEMIHOST),YES)
	SMH_ROOT=./rtenv_root
	CFLAGS+=-DUSE_SEMIHOST
	QEMU_SMH_PARAM_SUFFIX=-semihosting -chroot $(SMH_ROOT)
	QEMU_SMH_PARAM_PREFIX=sudo
endif

all: main.bin

main.bin: $(SRCS) $(HEADERS) $(SMH_ROOT)
	$(CROSS_COMPILE)gcc \
		-Wl,-Tmain.ld -nostartfiles \
		-I . \
		-I$(LIBDIR)/libraries/CMSIS/CM3/CoreSupport \
		-I$(LIBDIR)/libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x \
		-I$(CMSIS_LIB)/CM3/DeviceSupport/ST/STM32F10x \
		-I$(LIBDIR)/libraries/STM32F10x_StdPeriph_Driver/inc \
		-fno-common -O0 -fno-builtin-fork -Wall -std=c99 -pedantic \
		-gdwarf-2 -g3 \
		-mcpu=cortex-m3 -mthumb \
		-o main.elf \
		\
		$(CFLAGS) \
		$(SRCS)
	$(CROSS_COMPILE)objcopy -Obinary main.elf main.bin
	$(CROSS_COMPILE)objdump -S main.elf > main.list

$(SMH_ROOT):
	if [ ! -d "$(SMH_ROOT)" ] ; then mkdir $(SMH_ROOT) ; fi

qemu: main.bin $(QEMU_STM32)
	$(QEMU_SMH_PARAM_PREFIX) $(QEMU_STM32) -M stm32-p103 \
		-kernel main.bin -monitor null $(QEMU_SMH_PARAM_SUFFIX)

qemudbg: main.bin $(QEMU_STM32)
	$(QEMU_SMH_PARAM_PREFIX) $(QEMU_STM32) -M stm32-p103 \
		-gdb tcp::3333 -S \
		-kernel main.bin -monitor null $(QEMU_SMH_PARAM_SUFFIX)

qemu_remote: main.bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 -kernel main.bin -vnc :1

qemudbg_remote: main.bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 \
		-gdb tcp::3333 -S \
		-kernel main.bin \
		-vnc :1

qemu_remote_bg: main.bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 \
		-kernel main.bin \
		-vnc :1 &

qemudbg_remote_bg: main.bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 \
		-gdb tcp::3333 -S \
		-kernel main.bin \
		-vnc :1 &

emu: main.bin
	bash emulate.sh main.bin

qemuauto: main.bin gdbscript
	bash emulate.sh main.bin &
	sleep 1
	$(CROSS_COMPILE)gdb -x gdbscript &
	sleep 5

qemuauto_remote: main.bin gdbscript
	bash emulate_remote.sh main.bin &
	sleep 1
	$(CROSS_COMPILE)gdb -x gdbscript &
	sleep 5

clean:
	rm -f *.elf *.bin *.list
