# Builds a self-contained Windows x64 mod DLL from Windows or Linux.
# Windows: run from an MSYS2 MinGW64 shell.
# Linux: install MinGW-w64.
#
# Examples:
#   make MOD_NAME=tweaks MOD_SRC=tweaks.c
#   make MOD_NAME=tweaks MOD_SRC=tweaks.cpp
#   make CONFIG=release MOD_NAME=tweaks MOD_SRC=tweaks.cpp
#   make clean

MOD_NAME ?= mod
MOD_SRC  ?= main.c
CONFIG   ?= debug

ifeq ($(origin CC), default)
  ifeq ($(OS),Windows_NT)
    CC := gcc
  else
    CC := x86_64-w64-mingw32-gcc
  endif
endif

BUILD_DIR := build/$(CONFIG)
TARGET    := $(BUILD_DIR)/$(MOD_NAME).dll

SRCS := mod_sdk.c mod_unreal.c mod_nuklear.c
OBJS := $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

COMMON_WARNINGS := -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
COMMON_FLAGS    := $(COMMON_WARNINGS) -ffunction-sections -fdata-sections -MMD -MP

CFLAGS := -std=c11 $(COMMON_FLAGS)

ifeq ($(CONFIG),debug)
  CFLAGS += -O0 -g3 -DBUILD_DEBUG
else ifeq ($(CONFIG),release)
  CFLAGS += -O2 -DNDEBUG -DBUILD_RELEASE
else
  $(error CONFIG must be debug or release)
endif

LDFLAGS := -shared -static -static-libgcc -static-libstdc++ \
           -Wl,--gc-sections \
           -Wl,--exclude-all-symbols \
           -Wl,--out-implib,$(BUILD_DIR)/lib$(MOD_NAME).a

LDLIBS := -luser32

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CC) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/mod_nuklear.o: CFLAGS += -w

clean:
	rm -rf build

-include $(DEPS)
