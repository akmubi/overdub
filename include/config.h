#ifndef CONFIG_H
#define CONFIG_H

/* global permanent arena */
#define CONFIG_PERM_ARENA_SIZE (64 * GB)

/* log */
#define CONFIG_LOG_FILE_NAME "overdub.log"
#define CONFIG_LOG_LEVEL     LOG_LEVEL_DEBUG

/* scratch */
#define CONFIG_SCRATCH_RESERVE_SIZE (4 * GB)
#define CONFIG_SCRATCH_COMMIT_SIZE  (64 * KB)

/* Nuklear */
#define CONFIG_NK_ARENA_SIZE          (128 * MB) // dedicated arena reserve size
#define CONFIG_NK_INTERNAL_STATE_SIZE (8 * MB)
#define CONFIG_NK_CMD_BUF_SIZE        (1 * MB) // command buffer size
#define CONFIG_NK_FONT_BODY_SIZE      (14.0f)
#define CONFIG_NK_FONT_TITLE_SIZE     (18.0f)

/* Nuklear (D3D12 backend) */
#define CONFIG_NK_D3D12_MAX_VERTICES     (2 * MB)   // max vertices per frame snapshot
#define CONFIG_NK_D3D12_MAX_INDICES      (512 * KB) // max indices per frame snapshot
#define CONFIG_NK_D3D12_MAX_DRAW_CMDS    (1024)     // max draw commands per frame snapshot
#define CONFIG_NK_D3D12_MAX_BACK_BUFFERS (4)        // num of swap chain frames in flight

#define CONFIG_NK_OVERLAY_DEFAULT_TOGGLE_KEY STR_LIT("`")
#define CONFIG_NK_CONSOLE_DEFAULT_TOGGLE_KEY STR_LIT("F1")

/* console (Nuklear) */
#define CONFIG_NK_CONSOLE_MAX_LINES     (4 * KB)
#define CONFIG_NK_CONSOLE_MAX_LINE_LEN  (1 * KB)
#define CONFIG_NK_CONSOLE_MAX_INPUT_LEN (1 * KB)
#define CONFIG_NK_CONSOLE_MAX_HISTORY   (256)
#define CONFIG_NK_CONSOLE_MAX_COMMANDS  (256)

#define CONFIG_NK_CONSOLE_MIN_HEIGHT           (100.0f)
#define CONFIG_NK_CONSOLE_DEFAULT_HEIGHT_RATIO (0.4f) // 40% of viewport
#define CONFIG_NK_CONSOLE_BG_ALPHA             (128)  // 0-255
#define CONFIG_NK_CONSOLE_HANDLE_HEIGHT        (8.0f) // drag handle at top
#define CONFIG_NK_CONSOLE_INPUT_HEIGHT         (28.0f)
#define CONFIG_NK_CONSOLE_LINE_HEIGHT          (CONFIG_NK_FONT_BODY_SIZE + 0.5f)
#define CONFIG_NK_CONSOLE_PADDING              (4.0f)
#define CONFIG_UI_CONSOLE_WINDOW_NAME          "Console"

/* manager */
#define CONFIG_MOD_MANAGER_MAX_MODS          (128)
#define CONFIG_MOD_CFG_MAX_STR_LEN           (1024)
#define CONFIG_MOD_MANAGER_PERM_ARENA_SIZE   (64 * MB)
#define CONFIG_MOD_CODE_ARENA_SIZE           (1 * GB)
#define CONFIG_MOD_MAX_HOOKS                 (256)
#define CONFIG_MOD_MAX_LISTENERS             (8)
#define CONFIG_MOD_MAX_ARENAS                (256)
#define CONFIG_MOD_MAX_COMMANDS              (1024)
#define CONFIG_MOD_MANAGER_CONFIG_PATH       STR_LIT("overdub-config.ini")
#define CONFIG_MOD_MANAGER_MOD_DIR_NAME      STR_LIT("mods")
#define CONFIG_MOD_MANIFEST_FILE_NAME        STR_LIT("mod.ini")
#define CONFIG_MOD_CONFIG_FILE_NAME          STR_LIT("config.ini")
#define CONFIG_MOD_MANAGER_ERROR_MSG_MAX_LEN (1024)
#define CONFIG_MOD_ASSET_BASE_PRIORITY       (200)

/* UObject array create/delete listeners */
#define CONFIG_UOBJECT_ARRAY_MAX_LISTENERS (64)

/* uobject cache */
#define CONFIG_UOB_NAME_CACHE_ARENA_RESERVE (64 * MB)
#define CONFIG_UOB_NAME_CACHE_ARENA_COMMIT  (64 * KB)
#define CONFIG_UOB_NAME_CACHE_STR_RESERVE   (128 * MB)
#define CONFIG_UOB_NAME_CACHE_STR_COMMIT    (64 * KB)
#define CONFIG_UOB_NAME_CACHE_STR_MIN_CHUNK (4 * KB)
#define CONFIG_UOB_NAME_CACHE_MIN_CAP       (16)
#define CONFIG_UOB_NAME_CACHE_MAX_LOAD_NUM  (7)
#define CONFIG_UOB_NAME_CACHE_MAX_LOAD_DEN  (10)

/* uobject search */
#define CONFIG_UOB_SEARCH_ARENA_RESERVE (64 * MB)
#define CONFIG_UOB_SEARCH_ARENA_COMMIT  (64 * KB)
#define CONFIG_UOB_SEARCH_MIN_CAP       (16)
#define CONFIG_UOB_SEARCH_MAX_LOAD_NUM  (7)
#define CONFIG_UOB_SEARCH_MAX_LOAD_DEN  (10)

#endif /* CONFIG_H */
