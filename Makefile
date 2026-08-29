CC        := x86_64-w64-mingw32-gcc
BUILD_DIR := build

OBJ_DIR_DEBUG   := $(BUILD_DIR)/obj/debug
OBJ_DIR_RELEASE := $(BUILD_DIR)/obj/release

INC            := -I. -Iinclude -Ivendor
CFLAGS_COMMON  := -std=c11 -Wall -Wextra -Wno-unused-function -mcx16 -DWIN32_LEAN_AND_MEAN -DCOBJMACROS $(INC)
LDFLAGS_COMMON := -shared
LIBS_DLL       := -luser32 -lkernel32 -ladvapi32 -lshlwapi -ld3d12 -ld3d11 -ldxgi -ldxguid -luuid -ldbghelp

CFLAGS_DEBUG  := -g3 -O0
LDFLAGS_DEBUG :=

CFLAGS_RELEASE  := -O2 -DNDEBUG -flto -ffunction-sections -fdata-sections
LDFLAGS_RELEASE := -Wl,--gc-sections -flto

LUA_SRCS := $(filter-out vendor/lua/src/lua.c vendor/lua/src/luac.c,$(wildcard vendor/lua/src/*.c))
SRCS     := $(shell find src -name '*.c') $(LUA_SRCS)

OBJS_DEBUG   := $(SRCS:%.c=$(OBJ_DIR_DEBUG)/%.o)
OBJS_RELEASE := $(SRCS:%.c=$(OBJ_DIR_RELEASE)/%.o)

.PHONY: all debug release clean

all: debug

debug: $(OBJS_DEBUG) | $(BUILD_DIR)
	@printf "LINK\t$(BUILD_DIR)/overdub.dll\n"
	@$(CC) -o $(BUILD_DIR)/overdub.dll $(OBJS_DEBUG) $(LDFLAGS_COMMON) $(LDFLAGS_DEBUG) -Wl,--out-implib,$(BUILD_DIR)/overdub.lib $(LIBS_DLL)

release: $(OBJS_RELEASE) | $(BUILD_DIR)
	@printf "LINK\t$(BUILD_DIR)/overdub.dll\n"
	@$(CC) -o $(BUILD_DIR)/overdub.dll $(OBJS_RELEASE) $(LDFLAGS_COMMON) $(LDFLAGS_RELEASE) -Wl,--out-implib,$(BUILD_DIR)/overdub.lib $(LIBS_DLL)

$(BUILD_DIR) $(OBJ_DIR_DEBUG) $(OBJ_DIR_RELEASE):
	@printf "MKDIR\t$@\n"
	@mkdir -p $@

$(OBJ_DIR_DEBUG)/%.o: %.c
	@mkdir -p $(dir $@)
	@printf "CC\t$@\n"
	@$(CC) $(CFLAGS_COMMON) $(CFLAGS_DEBUG) -DBUILD_DEBUG -DOVERDUB_BUILD_DLL -MMD -MP -c $< -o $@

$(OBJ_DIR_RELEASE)/%.o: %.c
	@mkdir -p $(dir $@)
	@printf "CC\t$@\n"
	@$(CC) $(CFLAGS_COMMON) $(CFLAGS_RELEASE) -DBUILD_RELEASE -DOVERDUB_BUILD_DLL -MMD -MP -c $< -o $@

clean:
	@printf "RM\t$(BUILD_DIR)\n"
	@rm -rf $(BUILD_DIR)

-include $(OBJS_DEBUG:.o=.d)
-include $(OBJS_RELEASE:.o=.d)
