CC        := x86_64-w64-mingw32-gcc
BUILD_DIR := build

OBJ_DIR_DEBUG    := $(BUILD_DIR)/obj/debug
OBJ_DIR_RELEASE  := $(BUILD_DIR)/obj/release
OBJ_DIR_TEST_UI  := $(BUILD_DIR)/obj/test_ui
OBJ_DIR_TEST_LUA := $(BUILD_DIR)/obj/test_lua

INC            := -I. -Iinclude -Ivendor
CFLAGS_COMMON  := -std=c11 -Wall -Wextra -Wno-unused-function -DWIN32_LEAN_AND_MEAN -DCOBJMACROS $(INC)
LDFLAGS_COMMON := -shared
LIBS_DLL       := -luser32 -lkernel32 -ladvapi32 -lshlwapi -ld3d12 -ld3d11 -ldxgi -ldxguid -luuid -ldbghelp
LIBS_TEST_UI   := -luser32 -lkernel32 -ladvapi32 -lshlwapi -ld3d11 -ldxguid -ldbghelp

CFLAGS_DEBUG  := -g3 -O0
LDFLAGS_DEBUG :=

CFLAGS_RELEASE  := -O2 -DNDEBUG -flto -ffunction-sections -fdata-sections
LDFLAGS_RELEASE := -Wl,--gc-sections -flto

LUA_SRCS := $(filter-out vendor/lua/src/lua.c vendor/lua/src/luac.c,$(wildcard vendor/lua/src/*.c))
SRCS     := $(shell find src -name '*.c') $(LUA_SRCS)

SRCS_TEST_UI :=                     \
  src/arena.c                       \
  src/file.c                        \
  src/debug.c                       \
  src/globals.c                     \
  src/ini.c                         \
  src/input.c                       \
  src/log.c                         \
  src/misc.c                        \
  src/mod_host.c                    \
  src/mod_manager.c                 \
  src/nk_kb_text_shape.c            \
  src/nk_font_runtime.c             \
  src/path.c                        \
  src/profiler.c                    \
  src/scratch.c                     \
  src/sigscan.c                     \
  src/str_utf.c                     \
  src/str.c                         \
  src/string_pool.c                 \
  src/ui_console.c                  \
  src/ui_keybind_capture.c          \
  src/ui_manager.c                  \
  src/ui_mod_manager.c              \
  src/ui_nuklear.c                  \
  src/unreal.c                      \
  src/unreal_prop.c                 \
  src/vendor.c                      \
  src/vendor_kb.c                   \
  src/builtin/cache.c               \
  src/builtin/common.c              \
  src/builtin/details_panel.c       \
  src/builtin/kismet_disassembler.c \
  src/builtin/search.c              \
  src/builtin/uobject_search.c      \
  src/builtin/ufunction_call.c      \
  src/builtin/ufunction_tracer.c    \
  src/builtin/tweaks.c              \
  test/test_ui_overview.c           \
  test/test_ui_style_configurator.c

SRCS_TEST_UI  += $(LUA_SRCS)
SRCS_TEST_LUA := src/lua_runtime.c test/test_lua_runtime.c $(LUA_SRCS)

OBJS_DEBUG    := $(SRCS:%.c=$(OBJ_DIR_DEBUG)/%.o)
OBJS_RELEASE  := $(SRCS:%.c=$(OBJ_DIR_RELEASE)/%.o)
OBJS_TEST_UI  := $(SRCS_TEST_UI:%.c=$(OBJ_DIR_TEST_UI)/%.o)
OBJS_TEST_LUA := $(SRCS_TEST_LUA:%.c=$(OBJ_DIR_TEST_LUA)/%.o)

.PHONY: all debug release test_ui test_lua clean

all: debug

debug: $(OBJS_DEBUG) | $(BUILD_DIR)
	@printf "LINK\t$(BUILD_DIR)/overdub.dll\n"
	@$(CC) -o $(BUILD_DIR)/overdub.dll $(OBJS_DEBUG) $(LDFLAGS_COMMON) $(LDFLAGS_DEBUG) $(LIBS_DLL)

release: $(OBJS_RELEASE) | $(BUILD_DIR)
	@printf "LINK\t$(BUILD_DIR)/overdub.dll\n"
	@$(CC) -o $(BUILD_DIR)/overdub.dll $(OBJS_RELEASE) $(LDFLAGS_COMMON) $(LDFLAGS_RELEASE) $(LIBS_DLL)

test_ui: test/test_ui.c $(OBJS_TEST_UI) | $(BUILD_DIR)
	@printf "LINK\t$(BUILD_DIR)/overdub_test_ui.exe\n"
	@$(CC) $(CFLAGS_COMMON) -Wno-missing-braces -g3 -O0 -DBUILD_TEST_UI -o $(BUILD_DIR)/overdub_test_ui.exe test/test_ui.c $(OBJS_TEST_UI) $(LIBS_TEST_UI)
	@printf "RUN\t$(BUILD_DIR)/overdub_test_ui.exe\n"
	@wine $(BUILD_DIR)/overdub_test_ui.exe

test_lua: $(OBJS_TEST_LUA) | $(BUILD_DIR)
	@printf "LINK\t$(BUILD_DIR)/overdub_test_lua.exe\n"
	@$(CC) $(CFLAGS_COMMON) -g3 -O0 -DBUILD_TEST_LUA -o $(BUILD_DIR)/overdub_test_lua.exe $(OBJS_TEST_LUA)
	@printf "RUN\t$(BUILD_DIR)/overdub_test_lua.exe\n"
	@wine $(BUILD_DIR)/overdub_test_lua.exe

$(BUILD_DIR) $(OBJ_DIR_DEBUG) $(OBJ_DIR_RELEASE) $(OBJ_DIR_TEST) $(OBJ_DIR_TEST_UI) $(OBJ_DIR_TEST_LUA):
	@printf "MKDIR\t$@\n"
	@mkdir -p $@

$(OBJ_DIR_DEBUG)/%.o: %.c
	@mkdir -p $(dir $@)
	@printf "CC\t$@\n"
	@$(CC) $(CFLAGS_COMMON) $(CFLAGS_DEBUG) -DBUILD_DEBUG -MMD -MP -c $< -o $@

$(OBJ_DIR_RELEASE)/%.o: %.c
	@mkdir -p $(dir $@)
	@printf "CC\t$@\n"
	@$(CC) $(CFLAGS_COMMON) $(CFLAGS_RELEASE) -DBUILD_RELEASE -MMD -MP -c $< -o $@

$(OBJ_DIR_TEST_UI)/%.o: %.c
	@mkdir -p $(dir $@)
	@printf "CC\t$@\n"
	@$(CC) $(CFLAGS_COMMON) -g3 -O0 -DBUILD_TEST_UI -MMD -MP -c $< -o $@

$(OBJ_DIR_TEST_LUA)/%.o: %.c
	@mkdir -p $(dir $@)
	@printf "CC\t$@\n"
	@$(CC) $(CFLAGS_COMMON) -g3 -O0 -DBUILD_TEST_LUA -MMD -MP -c $< -o $@

clean:
	@printf "RM\t$(BUILD_DIR)\n"
	@rm -rf $(BUILD_DIR)

-include $(OBJS_DEBUG:.o=.d)
-include $(OBJS_RELEASE:.o=.d)
-include $(OBJS_TEST_UI:.o=.d)
-include $(OBJS_TEST_LUA:.o=.d)
