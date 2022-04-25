#
# PG-Strom Makefile
#
PG_CONFIG ?= pg_config

ifndef STROM_BUILD_ROOT
STROM_BUILD_ROOT=..
endif

#
# PG-Strom version
#
PGSTROM_VERSION := 5.0
PGSTROM_RELEASE := devel

#
# Source of PG-Strom host code
#
__XPU_COMMON_OBJS = xpu_basetype.o xpu_numeric.o \
                    xpu_timelib.o xpu_textlib.o xpu_misclib.o

__STROM_OBJS = main.o shmbuf.o extra.o \
               gpu_device.o \
               $(__XPU_COMMON_OBJS) \
               float2.o tinyint.o
STROM_OBJS = $(addprefix $(STROM_BUILD_ROOT)/next/,$(__STROM_OBJS))

#
# Installation Scripts
#
__STROM_SQL = pg_strom--5.0.sql
STROM_SQL = $(addprefix $(STROM_BUILD_ROOT)/sql/,$(__STROM_SQL))

#
# CUDA path and related
#
CUDA_PATH ?= /usr/local/cuda
CUDA_INCLUDE_PATH ?= $(CUDA_PATH)/include
CUDA_BIN_PATH ?= $(CUDA_PATH)/bin
CUDA_LIB_PATH ?= $(CUDA_PATH)/lib64
NVCC := $(CUDA_BIN_PATH)/nvcc
MAXREGCOUNT := 128

#
# GitHash to build
#
ifdef PGSTROM_GITHASH
ifeq ($(PGSTROM_GITHASH),HEAD)
PGSTROM_GITHASH = $(shell git rev-parse HEAD)
endif
else
ifeq ($(shell test -e $(STROM_BUILD_ROOT)/.git/config && echo -n 1),1)
PGSTROM_GITHASH = $(shell git rev-parse HEAD)
ifneq ($(shell git diff | wc -l),0)
PGSTROM_GITHASH_SUFFIX = ::local_changes
endif
else
ifeq ($(shell test -e $(STROM_BUILD_ROOT)/GITHASH && echo -n 1),1)
PGSTROM_GITHASH = $(shell cat $(STROM_BUILD_ROOT)/GITHASH)
else
PGSTROM_GITHASH = HEAD
endif
endif
endif

#
# Flags to build
#
PGSTROM_FLAGS += $(PGSTROM_FLAGS_CUSTOM)
PGSTROM_FLAGS += -D__PGSTROM_MODULE__=1
PGSTROM_FLAGS += "-DPGSTROM_VERSION=\"$(PGSTROM_VERSION)\""
ifeq ($(PGSTROM_DEBUG),1)
PGSTROM_FLAGS += -g -O0 -DPGSTROM_DEBUG_BUILD=1
endif
PGSTROM_FLAGS += -D__STROM_HOST__=1
ifeq ($(shell uname -m),aarch64)
PGSTROM_FLAGS += -DHAVE_FLOAT2 -mfp16-format=ieee
endif
PGSTROM_FLAGS += -DPGSTROM_GITHASH=\"$(PGSTROM_GITHASH)$(PGSTROM_GITHASH_SUFFIX)\"
PGSTROM_FLAGS += -DCUDA_MAXREGCOUNT=$(MAXREGCOUNT)
PGSTROM_FLAGS += -DCMD_GPUINFO_PATH=\"$(shell $(PG_CONFIG) --bindir)/gpuinfo\"
PG_CPPFLAGS := $(PGSTROM_FLAGS) -I $(CUDA_INCLUDE_PATH)
SHLIB_LINK := -L $(CUDA_LIB_PATH) -lcuda

#
# Definition of PG-Strom Extension
#
MODULE_big = pg_strom
MODULEDIR  = pg_strom
OBJS = $(STROM_OBJS)
EXTENSION = pg_strom

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
