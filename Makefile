# we assume that the utilities from RISC-V cross-compiler (i.e., riscv64-unknown-elf-gcc and etc.)
# are in your system PATH. To check if your environment satisfies this requirement, simple use 
# `which` command as follows:
# $ which riscv64-unknown-elf-gcc
# if you have an output path, your environment satisfy our requirement.

# ---------------------	macros --------------------------
CROSS_PREFIX 	:= riscv64-unknown-elf-
CC 				:= $(CROSS_PREFIX)gcc
AR 				:= $(CROSS_PREFIX)ar
RANLIB        	:= $(CROSS_PREFIX)ranlib

SRC_DIR        	:= .
OBJ_DIR 		:= obj
SPROJS_INCLUDE 	:= -I.  

HOSTFS_ROOT := hostfs_root
ifneq (,)
  march := -march=
  is_32bit := $(findstring 32,$(march))
  mabi := -mabi=$(if $(is_32bit),ilp32,lp64)
endif

CFLAGS        := -Wall -Werror -gdwarf-3 -fno-builtin -nostdlib -D__NO_INLINE__ -mcmodel=medany -g -Og -std=gnu99 -Wno-unused -Wno-attributes -fno-delete-null-pointer-checks -fno-PIE $(march) -fno-omit-frame-pointer
COMPILE       	:= $(CC) -MMD -MP $(CFLAGS) $(SPROJS_INCLUDE)

#---------------------	utils -----------------------
UTIL_CPPS 	:= util/*.c

UTIL_CPPS  := $(wildcard $(UTIL_CPPS))
UTIL_OBJS  :=  $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(UTIL_CPPS)))


UTIL_LIB   := $(OBJ_DIR)/util.a

#---------------------	kernel -----------------------
KERNEL_LDS  	:= kernel/kernel.lds
KERNEL_CPPS 	:= \
	kernel/*.c \
	kernel/machine/*.c \
	kernel/util/*.c

KERNEL_ASMS 	:= \
	kernel/*.S \
	kernel/machine/*.S \
	kernel/util/*.S

KERNEL_CPPS  	:= $(wildcard $(KERNEL_CPPS))
KERNEL_ASMS  	:= $(wildcard $(KERNEL_ASMS))
KERNEL_OBJS  	:=  $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(KERNEL_CPPS)))
KERNEL_OBJS  	+=  $(addprefix $(OBJ_DIR)/, $(patsubst %.S,%.o,$(KERNEL_ASMS)))

KERNEL_TARGET = $(OBJ_DIR)/riscv-pke


#---------------------	spike interface library -----------------------
SPIKE_INF_CPPS 	:= spike_interface/*.c

SPIKE_INF_CPPS  := $(wildcard $(SPIKE_INF_CPPS))
SPIKE_INF_OBJS 	:=  $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(SPIKE_INF_CPPS)))


SPIKE_INF_LIB   := $(OBJ_DIR)/spike_interface.a


#---------------------	user   -----------------------
USER_CPPS 		:= user/app_shell.c user/user_lib.c

USER_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_CPPS)))

USER_TARGET 	:= $(HOSTFS_ROOT)/bin/shell

USER_E_CPPS 		:= user/app_ls.c user/user_lib.c

USER_E_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_E_CPPS)))

USER_E_TARGET 	:= $(HOSTFS_ROOT)/bin/ls

USER_M_CPPS 		:= user/app_mkdir.c user/user_lib.c

USER_M_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_M_CPPS)))

USER_M_TARGET 	:= $(HOSTFS_ROOT)/bin/mkdir

USER_T_CPPS 		:= user/app_touch.c user/user_lib.c

USER_T_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_T_CPPS)))

USER_T_TARGET 	:= $(HOSTFS_ROOT)/bin/touch

USER_C_CPPS 		:= user/app_cat.c user/user_lib.c

USER_C_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_C_CPPS)))

USER_C_TARGET 	:= $(HOSTFS_ROOT)/bin/cat

USER_O_CPPS 		:= user/app_echo.c user/user_lib.c

USER_O_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_O_CPPS)))

USER_O_TARGET 	:= $(HOSTFS_ROOT)/bin/echo

USER_EL_CPPS 		:= user/app_errorline.c user/user_lib.c

USER_EL_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_EL_CPPS)))

USER_EL_TARGET 	:= $(HOSTFS_ROOT)/bin/error_line

USER_SEM_CPPS 		:= user/app_semaphore.c user/user_lib.c

USER_SEM_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_SEM_CPPS)))

USER_SEM_TARGET 	:= $(HOSTFS_ROOT)/bin/sem

USER_PF_CPPS 		:= user/app_sum_sequence.c user/user_lib.c

USER_PF_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_PF_CPPS)))

USER_PF_TARGET 	:= $(HOSTFS_ROOT)/bin/page_fault

USER_BT_CPPS 		:= user/app_print_backtrace.c user/user_lib.c

USER_BT_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_BT_CPPS)))

USER_BT_TARGET 	:= $(HOSTFS_ROOT)/bin/backtrace

USER_HM_CPPS 		:= user/app_singlepageheap.c user/user_lib.c

USER_HM_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_HM_CPPS)))

USER_HM_TARGET 	:= $(HOSTFS_ROOT)/bin/heap

USER_CW_CPPS 		:= user/app_cow.c user/user_lib.c

USER_CW_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_CW_CPPS)))

USER_CW_TARGET 	:= $(HOSTFS_ROOT)/bin/cow
#------------------------targets------------------------
$(OBJ_DIR):
	@-mkdir -p $(OBJ_DIR)	
	@-mkdir -p $(dir $(UTIL_OBJS))
	@-mkdir -p $(dir $(SPIKE_INF_OBJS))
	@-mkdir -p $(dir $(KERNEL_OBJS))
	@-mkdir -p $(dir $(USER_OBJS))
	@-mkdir -p $(dir $(USER_E_OBJS))
	@-mkdir -p $(dir $(USER_M_OBJS))
	@-mkdir -p $(dir $(USER_T_OBJS))
	@-mkdir -p $(dir $(USER_C_OBJS))
	@-mkdir -p $(dir $(USER_O_OBJS))
	@-mkdir -p $(dir $(USER_EL_OBJS))
	@-mkdir -p $(dir $(USER_PF_OBJS))
	@-mkdir -p $(dir $(USER_BT_OBJS))
	@-mkdir -p $(dir $(USER_SEM_OBJS))
	@-mkdir -p $(dir $(USER_HM_OBJS))
	@-mkdir -p $(dir $(USER_CW_OBJS))

	
$(OBJ_DIR)/%.o : %.c
	@echo "compiling" $<
	@$(COMPILE) -c $< -o $@

$(OBJ_DIR)/%.o : %.S
	@echo "compiling" $<
	@$(COMPILE) -c $< -o $@

$(UTIL_LIB): $(OBJ_DIR) $(UTIL_OBJS)
	@echo "linking " $@	...	
	@$(AR) -rcs $@ $(UTIL_OBJS) 
	@echo "Util lib has been build into" \"$@\"
	
$(SPIKE_INF_LIB): $(OBJ_DIR) $(UTIL_OBJS) $(SPIKE_INF_OBJS)
	@echo "linking " $@	...	
	@$(AR) -rcs $@ $(SPIKE_INF_OBJS) $(UTIL_OBJS)
	@echo "Spike lib has been build into" \"$@\"

$(KERNEL_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(SPIKE_INF_LIB) $(KERNEL_OBJS) $(KERNEL_LDS)
	@echo "linking" $@ ...
	@$(COMPILE) $(KERNEL_OBJS) $(UTIL_LIB) $(SPIKE_INF_LIB) -o $@ -T $(KERNEL_LDS)
	@echo "PKE core has been built into" \"$@\"

$(USER_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"
	@cp $@ $(OBJ_DIR)

$(USER_E_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_E_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_E_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_M_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_M_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_M_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_T_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_T_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_T_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_C_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_C_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_C_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_O_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_O_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_O_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_EL_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_EL_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_EL_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_PF_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_PF_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_PF_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_SEM_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_SEM_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_SEM_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_BT_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_BT_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_BT_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_HM_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_HM_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_HM_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_CW_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_CW_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_CW_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

-include $(wildcard $(OBJ_DIR)/*/*.d)
-include $(wildcard $(OBJ_DIR)/*/*/*.d)

.DEFAULT_GOAL := $(all)

all: $(KERNEL_TARGET) $(USER_TARGET) $(USER_E_TARGET) $(USER_M_TARGET) $(USER_T_TARGET) $(USER_C_TARGET) $(USER_O_TARGET) $(USER_EL_TARGET) $(USER_PF_TARGET) $(USER_SEM_TARGET) $(USER_BT_TARGET) ${USER_HM_TARGET} ${USER_CW_TARGET}
.PHONY:all

run: $(KERNEL_TARGET) $(USER_TARGET) $(USER_E_TARGET) $(USER_M_TARGET) $(USER_T_TARGET) $(USER_C_TARGET) $(USER_O_TARGET) $(USER_EL_TARGET) $(USER_PF_TARGET) $(USER_SEM_TARGET) $(USER_BT_TARGET) ${USER_HM_TARGET} ${USER_CW_TARGET}
	@echo "********************HUST PKE********************"
	spike $(KERNEL_TARGET) /bin/shell

# need openocd!
gdb:$(KERNEL_TARGET) $(USER_TARGET)
	spike --rbb-port=9824 -H $(KERNEL_TARGET) $(USER_TARGET) &
	@sleep 1
	openocd -f ./.spike.cfg &
	@sleep 1
	riscv64-unknown-elf-gdb -command=./.gdbinit

# clean gdb. need openocd!
gdb_clean:
	@-kill -9 $$(lsof -i:9824 -t)
	@-kill -9 $$(lsof -i:3333 -t)
	@sleep 1

objdump:
	riscv64-unknown-elf-objdump -d $(KERNEL_TARGET) > $(OBJ_DIR)/kernel_dump
	riscv64-unknown-elf-objdump -d $(USER_TARGET) > $(OBJ_DIR)/user_dump

cscope:
	find ./ -name "*.c" > cscope.files
	find ./ -name "*.h" >> cscope.files
	find ./ -name "*.S" >> cscope.files
	find ./ -name "*.lds" >> cscope.files
	cscope -bqk

format:
	@python ./format.py ./

clean:
	rm -fr ${OBJ_DIR} ${HOSTFS_ROOT}/bin
