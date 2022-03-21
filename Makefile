CONFIG_MODULE_SIG = n
TARGET_MODULE := fibdrv2

obj-m := $(TARGET_MODULE).o
fibdrv2-objs := fibdrv.o bignum.o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

GIT_HOOKS := .git/hooks/applied

all: $(GIT_HOOKS) client
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) client out
load:
	sudo insmod $(TARGET_MODULE).ko
unload:
	sudo rmmod $(TARGET_MODULE) || true >/dev/null

client: client.c
	$(CC) -o $@ $^

set_env:
	# This rule must be run as root
	# Suppress address space layout randomization (ASLR)
	sudo sh -c "echo 0 > /proc/sys/kernel/randomize_va_space"
	sudo sh performance.sh
	# For Intel CPU, disable turbo mode
	sudo sh -c "echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo"

it_time: client set_env
	# This rule must be run as root
	echo "iteration" > /sys/kernel/fibonacci/algo
	# Bind the executable to CPU 0.
	taskset -c 0 ./client $(PWD) it_fib.time it_rd.time

fd_time: client set_env
	# This rule must be run as root
	echo "fast-doubling" > /sys/kernel/fibonacci/algo
	# Bind the executable to CPU 0.
	taskset -c 0 ./client $(PWD) fd_fib.time fd_rd.time

fd_clz_time: client set_env
	# This rule must be run as root
	echo "fast-doubling-clz" > /sys/kernel/fibonacci/algo
	# Bind the executable to CPU 0.
	taskset -c 0 ./client $(PWD) fd_clz_fib.time fd_clz_rd.time

plot: fd_time fd_clz_time it_time
	# This rule must be run as root
	gnuplot fibdrv_perf.gp
	gnuplot fibdrv_perf2.gp
	gnuplot rd_perf.gp

plot_clean:
	# This rule must be run as root
	sudo rm -rf *.time *.png

PRINTF = env printf
PASS_COLOR = \e[32;01m
NO_COLOR = \e[0m
pass = $(PRINTF) "$(PASS_COLOR)$1 Passed [-]$(NO_COLOR)\n"

check: all
	$(MAKE) unload
	$(MAKE) load
	sudo ./client > out
	$(MAKE) unload
	@diff -u out scripts/expected.txt && $(call pass)
	@scripts/verify.py
