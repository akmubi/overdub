# Builds a self-contained Windows x64 mod DLL from Windows or Linux.
# Windows: run from an MSYS2 MinGW64 shell.
# Linux: install MinGW-w64.
#
# Examples:
#   make MOD_NAME=tweaks MOD_SRC=tweaks.c
#   make MOD_NAME=tweaks MOD_SRC=tweaks.cpp
#   make CONFIG=release MOD_NAME=tweaks MOD_SRC=tweaks.cpp
#   make OVERDUB_DIR=/path/to/overdub MOD_NAME=tweaks MOD_SRC=tweaks.c
#   make clean

MOD_NAME ?= mod
MOD_SRC  ?= main.c
CONFIG   ?= debug

# A packaged template keeps the SDK in this directory. The in-repository
# template falls back to the parent Overdub source tree.
OVERDUB_DIR         ?= $(if $(wildcard include/mod.h),.,..)
OVERDUB_INCLUDE_DIR ?= $(OVERDUB_DIR)/include
OVERDUB_VENDOR_DIR  ?= $(OVERDUB_DIR)/vendor
OVERDUB_HEADER      ?= $(OVERDUB_INCLUDE_DIR)/mod.h
OVERDUB_LIB         ?= $(OVERDUB_DIR)/build/overdub.lib
OVERDUB_MAKEFILE    ?= $(OVERDUB_DIR)/Makefile
OVERDUB_PROJECT     ?= $(OVERDUB_DIR)/overdub.vcxproj

ifeq ($(origin CC), default)
  ifeq ($(OS),Windows_NT)
    CC := gcc
  else
    CC := x86_64-w64-mingw32-gcc
  endif
endif

ifeq ($(origin CXX), default)
  ifeq ($(OS),Windows_NT)
    CXX := g++
  else
    CXX := x86_64-w64-mingw32-g++
  endif
endif

BUILD_DIR := build/$(CONFIG)
TARGET    := $(BUILD_DIR)/$(MOD_NAME).dll

SRCS := $(MOD_SRC)
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(filter %.c,$(SRCS))) \
        $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(filter %.cpp,$(SRCS)))
DEPS := $(OBJS:.o=.d)

COMMON_WARNINGS := -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
COMMON_FLAGS    := $(COMMON_WARNINGS) -I"$(OVERDUB_INCLUDE_DIR)" -I"$(OVERDUB_VENDOR_DIR)" -ffunction-sections -fdata-sections -MMD -MP

CFLAGS   := -std=c11 $(COMMON_FLAGS)
CXXFLAGS := -std=c++20 $(COMMON_FLAGS)

ifeq ($(CONFIG),debug)
  CFLAGS   += -O0 -g3 -DBUILD_DEBUG
  CXXFLAGS += -O0 -g3 -DBUILD_DEBUG
else ifeq ($(CONFIG),release)
  CFLAGS   += -O2 -DNDEBUG -DBUILD_RELEASE
  CXXFLAGS += -O2 -DNDEBUG -DBUILD_RELEASE
else
  $(error CONFIG must be debug or release)
endif

LDFLAGS := -shared -static -static-libgcc -static-libstdc++ \
           -Wl,--gc-sections \
           -Wl,--exclude-all-symbols \
           -Wl,--out-implib,$(BUILD_DIR)/lib$(MOD_NAME).a

LDLIBS := "$(OVERDUB_LIB)"
LINKER := $(if $(filter %.cpp,$(SRCS)),$(CXX),$(CC))

.PHONY: all check-overdub clean

all: $(TARGET)

check-overdub:
	@if [ ! -f "$(OVERDUB_HEADER)" ]; then \
		printf "ERROR\t%s\n" "$(OVERDUB_HEADER) is missing. Set OVERDUB_DIR to the Overdub source directory."; \
		exit 1; \
	fi
	@if [ ! -f "$(OVERDUB_LIB)" ]; then \
		if [ ! -f "$(OVERDUB_MAKEFILE)" ] || [ ! -f "$(OVERDUB_PROJECT)" ]; then \
			printf "ERROR\t%s\n" "$(OVERDUB_LIB) is missing and $(OVERDUB_DIR) is not an Overdub source tree. Cannot build Overdub automatically."; \
			exit 1; \
		fi; \
		printf "BUILD\t%s\n" "$(OVERDUB_LIB) is missing; building Overdub ($(CONFIG))."; \
		$(MAKE) -C "$(OVERDUB_DIR)" "$(CONFIG)" || exit 1; \
	fi
	@if [ ! -f "$(OVERDUB_LIB)" ]; then \
		printf "ERROR\t%s\n" "The Overdub build completed without producing $(OVERDUB_LIB)."; \
		exit 1; \
	fi

$(TARGET): $(OBJS) | check-overdub
	@mkdir -p $(@D)
	$(LINKER) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

$(OBJS): | check-overdub

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf build/debug build/release

-include $(DEPS)
