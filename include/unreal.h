#ifndef UE_TYPES_H
#define UE_TYPES_H

#include "arena.h"
#include "str.h"
#include "types.h"

#define FNAME_MAX_BLOCK_BITS    13
#define FNAME_BLOCK_OFFSET_BITS 16
#define FNAME_MAX_BLOCKS        (1 << FNAME_MAX_BLOCK_BITS)
#define FNAME_ENTRY_NAME_SIZE   1024
#define FNAME_POOL_SHARD_BITS   8
#define FNAME_POOL_MAX_SHARDS   (1 << FNAME_POOL_SHARD_BITS)

#define FNAME_NO_NUMBER_INTERNAL        (0)
#define FNAME_INTERNAL_TO_EXTERNAL(NUM) ((NUM) - 1)
#define FNAME_EXTERNAL_TO_INTERNAL(NUM) ((NUM) + 1)

#define FIO_STATUS_ERR_MSG_LEN 128

#define UOBJECT_ARRAY_NUM_ELEMS_PER_CHUNK (64 * 1024)

#define FUNC_FLAG_NONE                     0x00000000
#define FUNC_FLAG_FINAL                    0x00000001
#define FUNC_FLAG_REQUIRED_API             0x00000002
#define FUNC_FLAG_BLUEPRINT_AUTHORITY_ONLY 0x00000004
#define FUNC_FLAG_BLUEPRINT_COSMETIC       0x00000008
#define FUNC_FLAG_NET                      0x00000040
#define FUNC_FLAG_NET_RELIABLE             0x00000080
#define FUNC_FLAG_NET_REQUEST              0x00000100
#define FUNC_FLAG_EXEC                     0x00000200
#define FUNC_FLAG_NATIVE                   0x00000400
#define FUNC_FLAG_EVENT                    0x00000800
#define FUNC_FLAG_NET_RESPONSE             0x00001000
#define FUNC_FLAG_STATIC                   0x00002000
#define FUNC_FLAG_NET_MULTICAST            0x00004000
#define FUNC_FLAG_UBERGRAPH_FUNCTION       0x00008000
#define FUNC_FLAG_MULTICAST_DELEGATE       0x00010000
#define FUNC_FLAG_PUBLIC                   0x00020000
#define FUNC_FLAG_PRIVATE                  0x00040000
#define FUNC_FLAG_PROTECTED                0x00080000
#define FUNC_FLAG_DELEGATE                 0x00100000
#define FUNC_FLAG_NET_SERVER               0x00200000
#define FUNC_FLAG_HAS_OUT_PARMS            0x00400000
#define FUNC_FLAG_HAS_DEFAULTS             0x00800000
#define FUNC_FLAG_NET_CLIENT               0x01000000
#define FUNC_FLAG_DLL_IMPORT               0x02000000
#define FUNC_FLAG_BLUEPRINT_CALLABLE       0x04000000
#define FUNC_FLAG_BLUEPRINT_EVENT          0x08000000
#define FUNC_FLAG_BLUEPRINT_PURE           0x10000000
#define FUNC_FLAG_EDITOR_ONLY              0x20000000
#define FUNC_FLAG_CONST                    0x40000000
#define FUNC_FLAG_NET_VALIDATE             0x80000000
#define FUNC_FLAG_ALL                      0xFFFFFFFF

typedef uint32_t estruct_flags_t;
#define ESF_NO_FLAGS                      0x00000000
#define ESF_NATIVE                        0x00000001
#define ESF_IDENTICAL_NATIVE              0x00000002
#define ESF_HAS_INSTANCED_REF             0x00000004
#define ESF_NO_EXPORT                     0x00000008
#define ESF_ATOMIC                        0x00000010
#define ESF_IMMUTABLE                     0x00000020
#define ESF_ADD_STRUCT_REF_OBJS           0x00000040
#define ESF_REQUIRED_API                  0x00000200
#define ESF_NET_SERIALIZE_NATIVE          0x00000400
#define ESF_SERIALIZE_NATIVE              0x00000800
#define ESF_COPY_NATIVE                   0x00001000
#define ESF_IS_POD                        0x00002000
#define ESF_NO_DESTRUCTOR                 0x00004000
#define ESF_ZERO_CONSTRUCTOR              0x00008000
#define ESF_EXPORT_TEXT_ITEM_NATIVE       0x00010000
#define ESF_IMPORT_TEXT_ITEM_NATIVE       0x00020000
#define ESF_POST_SERIALIZE_NATIVE         0x00040000
#define ESF_SERIALIZE_FROM_MISMATCHED_TAG 0x00080000
#define ESF_NET_DELTA_SERIALIZE_NATIVE    0x00100000
#define ESF_POST_SCRIPT_CONSTRUCT         0x00200000
#define ESF_NET_SHARED_SERIALIZATION      0x00400000
#define ESF_TRASHED                       0x00800000

typedef uint64_t eprop_flags_t;
#define CPF_NONE                              0
#define CPF_EDIT                              0x0000000000000001
#define CPF_CONST_PARM                        0x0000000000000002
#define CPF_BLUEPRINT_VISIBLE                 0x0000000000000004
#define CPF_EXPORT_OBJECT                     0x0000000000000008
#define CPF_BLUEPRINT_READ_ONLY               0x0000000000000010
#define CPF_NET                               0x0000000000000020
#define CPF_EDIT_FIXED_SIZE                   0x0000000000000040
#define CPF_PARM                              0x0000000000000080
#define CPF_OUT_PARM                          0x0000000000000100
#define CPF_ZERO_CONSTRUCTOR                  0x0000000000000200
#define CPF_RETURN_PARM                       0x0000000000000400
#define CPF_DISABLE_EDIT_ON_TEMPLATE          0x0000000000000800
#define CPF_TRANSIENT                         0x0000000000002000
#define CPF_CONFIG                            0x0000000000004000
#define CPF_DISABLE_EDIT_ON_INSTANCE          0x0000000000010000
#define CPF_EDIT_CONST                        0x0000000000020000
#define CPF_GLOBAL_CONFIG                     0x0000000000040000
#define CPF_INSTANCED_REFERENCE               0x0000000000080000
#define CPF_DUPLICATE_TRANSIENT               0x0000000000200000
#define CPF_SAVE_GAME                         0x0000000001000000
#define CPF_NO_CLEAR                          0x0000000002000000
#define CPF_REFERENCE_PARM                    0x0000000008000000
#define CPF_BLUEPRINT_ASSIGNABLE              0x0000000010000000
#define CPF_DEPRECATED                        0x0000000020000000
#define CPF_IS_PLAIN_OLD_DATA                 0x0000000040000000
#define CPF_REP_SKIP                          0x0000000080000000
#define CPF_REP_NOTIFY                        0x0000000100000000
#define CPF_INTERP                            0x0000000200000000
#define CPF_NON_TRANSACTIONAL                 0x0000000400000000
#define CPF_EDITOR_ONLY                       0x0000000800000000
#define CPF_NO_DESTRUCTOR                     0x0000001000000000
#define CPF_AUTO_WEAK                         0x0000004000000000
#define CPF_CONTAINS_INSTANCED_REFERENCE      0x0000008000000000
#define CPF_ASSET_REGISTRY_SEARCHABLE         0x0000010000000000
#define CPF_SIMPLE_DISPLAY                    0x0000020000000000
#define CPF_ADVANCED_DISPLAY                  0x0000040000000000
#define CPF_PROTECTED                         0x0000080000000000
#define CPF_BLUEPRINT_CALLABLE                0x0000100000000000
#define CPF_BLUEPRINT_AUTHORITY_ONLY          0x0000200000000000
#define CPF_TEXT_EXPORT_TRANSIENT             0x0000400000000000
#define CPF_NON_PIE_DUPLICATE_TRANSIENT       0x0000800000000000
#define CPF_EXPOSE_ON_SPAWN                   0x0001000000000000
#define CPF_PERSISTENT_INSTANCE               0x0002000000000000
#define CPF_UOBJECT_WRAPPER                   0x0004000000000000
#define CPF_HAS_GET_VALUE_TYPE_HASH           0x0008000000000000
#define CPF_NATIVE_ACCESS_SPECIFIER_PUBLIC    0x0010000000000000
#define CPF_NATIVE_ACCESS_SPECIFIER_PROTECTED 0x0020000000000000
#define CPF_NATIVE_ACCESS_SPECIFIER_PRIVATE   0x0040000000000000
#define CPF_SKIP_SERIALIZATION                0x0080000000000000

typedef uint32_t eobj_flags_t;
enum {
  RF_NO_FLAGS                       = 0X00000000,
  RF_PUBLIC                         = 0X00000001,
  RF_STANDALONE                     = 0X00000002,
  RF_MARK_AS_NATIVE                 = 0X00000004,
  RF_TRANSACTIONAL                  = 0X00000008,
  RF_CLASS_DEFAULT_OBJECT           = 0X00000010,
  RF_ARCHETYPE_OBJECT               = 0X00000020,
  RF_TRANSIENT                      = 0X00000040,
  RF_MARK_AS_ROOT_SET               = 0X00000080,
  RF_TAG_GARBAGE_TEMP               = 0X00000100,
  RF_NEED_INITIALIZATION            = 0X00000200,
  RF_NEED_LOAD                      = 0X00000400,
  RF_KEEP_FOR_COOKER                = 0X00000800,
  RF_NEED_POST_LOAD                 = 0X00001000,
  RF_NEED_POST_LOAD_SUBOBJECTS      = 0X00002000,
  RF_NEWER_VERSION_EXISTS           = 0X00004000,
  RF_BEGIN_DESTROYED                = 0X00008000,
  RF_FINISH_DESTROYED               = 0X00010000,
  RF_BEING_REGENERATED              = 0X00020000,
  RF_DEFAULT_SUB_OBJECT             = 0X00040000,
  RF_WAS_LOADED                     = 0X00080000,
  RF_TEXT_EXPORT_TRANSIENT          = 0X00100000,
  RF_LOAD_COMPLETED                 = 0X00200000,
  RF_INHERITABLE_COMPONENT_TEMPLATE = 0X00400000,
  RF_DUPLICATE_TRANSIENT            = 0X00800000,
  RF_STRONG_REF_ON_FRAME            = 0X01000000,
  RF_NON_PIE_DUPLICATE_TRANSIENT    = 0X02000000,
  RF_DYNAMIC                        = 0X04000000,
  RF_WILL_BE_LOADED                 = 0X08000000,
  RF_HAS_EXTERNAL_PACKAGE           = 0X10000000,
};

typedef enum {
  EX_LOCAL_VARIABLE                = 0x00,
  EX_INSTANCE_VARIABLE             = 0x01,
  EX_DEFAULT_VARIABLE              = 0x02,
  EX_RETURN                        = 0x04,
  EX_JUMP                          = 0x06,
  EX_JUMP_IF_NOT                   = 0x07,
  EX_ASSERT                        = 0x09,
  EX_NOTHING                       = 0x0B,
  EX_LET                           = 0x0F,
  EX_CLASS_CONTEXT                 = 0x12,
  EX_META_CAST                     = 0x13,
  EX_LET_BOOL                      = 0x14,
  EX_END_PARM_VALUE                = 0x15,
  EX_END_FUNCTION_PARMS            = 0x16,
  EX_SELF                          = 0x17,
  EX_SKIP                          = 0x18,
  EX_CONTEXT                       = 0x19,
  EX_CONTEXT_FAIL_SILENT           = 0x1A,
  EX_VIRTUAL_FUNCTION              = 0x1B,
  EX_FINAL_FUNCTION                = 0x1C,
  EX_INT_CONST                     = 0x1D,
  EX_FLOAT_CONST                   = 0x1E,
  EX_STRING_CONST                  = 0x1F,
  EX_OBJECT_CONST                  = 0x20,
  EX_NAME_CONST                    = 0x21,
  EX_ROTATION_CONST                = 0x22,
  EX_VECTOR_CONST                  = 0x23,
  EX_BYTE_CONST                    = 0x24,
  EX_INT_ZERO                      = 0x25,
  EX_INT_ONE                       = 0x26,
  EX_TRUE                          = 0x27,
  EX_FALSE                         = 0x28,
  EX_TEXT_CONST                    = 0x29,
  EX_NO_OBJECT                     = 0x2A,
  EX_TRANSFORM_CONST               = 0x2B,
  EX_INT_CONST_BYTE                = 0x2C,
  EX_NO_INTERFACE                  = 0x2D,
  EX_DYNAMIC_CAST                  = 0x2E,
  EX_STRUCT_CONST                  = 0x2F,
  EX_END_STRUCT_CONST              = 0x30,
  EX_SET_ARRAY                     = 0x31,
  EX_END_ARRAY                     = 0x32,
  EX_PROPERTY_CONST                = 0x33,
  EX_UNICODE_STRING_CONST          = 0x34,
  EX_INT64_CONST                   = 0x35,
  EX_UINT64_CONST                  = 0x36,
  EX_PRIMITIVE_CAST                = 0x38,
  EX_SET_SET                       = 0x39,
  EX_END_SET                       = 0x3A,
  EX_SET_MAP                       = 0x3B,
  EX_END_MAP                       = 0x3C,
  EX_SET_CONST                     = 0x3D,
  EX_END_SET_CONST                 = 0x3E,
  EX_MAP_CONST                     = 0x3F,
  EX_END_MAP_CONST                 = 0x40,
  EX_STRUCT_MEMBER_CONTEXT         = 0x42,
  EX_LET_MULTICAST_DELEGATE        = 0x43,
  EX_LET_DELEGATE                  = 0x44,
  EX_LOCAL_VIRTUAL_FUNCTION        = 0x45,
  EX_LOCAL_FINAL_FUNCTION          = 0x46,
  EX_LOCAL_OUT_VARIABLE            = 0x48,
  EX_DEPRECATED_OP_4A              = 0x4A,
  EX_INSTANCE_DELEGATE             = 0x4B,
  EX_PUSH_EXECUTION_FLOW           = 0x4C,
  EX_POP_EXECUTION_FLOW            = 0x4D,
  EX_COMPUTED_JUMP                 = 0x4E,
  EX_POP_EXECUTION_FLOW_IF_NOT     = 0x4F,
  EX_BREAKPOINT                    = 0x50,
  EX_INTERFACE_CONTEXT             = 0x51,
  EX_OBJ_TO_INTERFACE_CAST         = 0x52,
  EX_END_OF_SCRIPT                 = 0x53,
  EX_CROSS_INTERFACE_CAST          = 0x54,
  EX_INTERFACE_TO_OBJ_CAST         = 0x55,
  EX_WIRE_TRACEPOINT               = 0x5A,
  EX_SKIP_OFFSET_CONST             = 0x5B,
  EX_ADD_MULTICAST_DELEGATE        = 0x5C,
  EX_CLEAR_MULTICAST_DELEGATE      = 0x5D,
  EX_TRACEPOINT                    = 0x5E,
  EX_LET_OBJ                       = 0x5F,
  EX_LET_WEAK_OBJ_PTR              = 0x60,
  EX_BIND_DELEGATE                 = 0x61,
  EX_REMOVE_MULTICAST_DELEGATE     = 0x62,
  EX_CALL_MULTICAST_DELEGATE       = 0x63,
  EX_LET_VALUE_ON_PERSISTENT_FRAME = 0x64,
  EX_ARRAY_CONST                   = 0x65,
  EX_END_ARRAY_CONST               = 0x66,
  EX_SOFT_OBJECT_CONST             = 0x67,
  EX_CALL_MATH                     = 0x68,
  EX_SWITCH_VALUE                  = 0x69,
  EX_INSTRUMENTATION_EVENT         = 0x6A,
  EX_ARRAY_GET_BY_REF              = 0x6B,
  EX_CLASS_SPARSE_DATA_VARIABLE    = 0x6C,
  EX_FIELD_PATH_CONST              = 0x6D,
  EX_MAX                           = 0x100,
} expr_token_t;

#define FSTRING(WS)      \
  {                      \
    .data = (WS),        \
    .len  = COUNTOF(WS), \
    .max  = COUNTOF(WS), \
  }

#define TARRAY(T) \
  struct {        \
    T      *data; \
    int32_t num;  \
    int32_t max;  \
  }
#define TPAIR(KT, VT) \
  struct {            \
    KT key;           \
    VT value;         \
  }

#define TSHARED_PTR(T) \
  struct {             \
    T    *obj;         \
    void *ref_ctrl;    \
  }

#define TSPARSE_SLOT(T)      \
  union {                    \
    T elem_data;             \
    struct {                 \
      int32_t prev_free_idx; \
      int32_t next_free_idx; \
    };                       \
  }

#define TSPARSE(T)                \
  struct {                        \
    TARRAY(TSPARSE_SLOT(T)) data; \
    tbit_array_t alloc_flags;     \
    int32_t      first_free_idx;  \
    int32_t      num_free_idx;    \
  }

#define TSET_ELEM(T)      \
  struct {                \
    T       val;          \
    int32_t hash_next_id; \
    int32_t hash_idx;     \
  }

#define TSET(T)                  \
  struct {                       \
    TSPARSE(TSET_ELEM(T)) elems; \
    hash_allocator_t hash;       \
    int32_t          hash_size;  \
  }

typedef struct bit_array_allocator_s bit_array_allocator_t;
struct bit_array_allocator_s {
  uint32_t inline_data[4];
  void    *secondary_data;
};
STATIC_ASSERT(sizeof(bit_array_allocator_t) == 24, "size mismatch");

typedef struct tbit_array_s tbit_array_t;
struct tbit_array_s {
  bit_array_allocator_t allocator;
  int32_t               num_bits;
  int32_t               max_bits;
};
STATIC_ASSERT(sizeof(tbit_array_t) == 32, "size mismatch");

typedef struct hash_allocator_s hash_allocator_t;
struct hash_allocator_s {
  int32_t inline_data[1];
  void   *secondary_data;
};
STATIC_ASSERT(sizeof(hash_allocator_t) == 16, "size mismatch");

typedef uint32_t einput_event_t;
enum {
  IE_PRESSED      = 0,
  IE_RELEASED     = 1,
  IE_REPEAT       = 2,
  IE_DOUBLE_CLICK = 3,
  IE_AXIS         = 4,
  IE_MAX,
};

typedef struct fstring_s fstring_t;
struct fstring_s {
  uint16_t *data;
  int32_t   len;
  int32_t   max;
};
STATIC_ASSERT(sizeof(fstring_t) == 16, "size mismatch");

typedef uint32_t etext_flags_t;
enum {
  ETEXT_TRANSIENT           = (1 << 0),
  ETEXT_CULTURE_INVARIANT   = (1 << 1),
  ETEXT_IMMUTABLE           = (1 << 2),
  ETEXT_INITIAL_EXPR_SYNCED = (1 << 3),
};

typedef struct ftext_data_s ftext_data_t;
struct ftext_data_s {
  void     *vftable;
  fstring_t local_str;
  fstring_t display_str;
};
STATIC_ASSERT(offsetof(ftext_data_t, local_str) == 0x08, "invalid offset");
STATIC_ASSERT(offsetof(ftext_data_t, display_str) == 0x18, "invalid offset");

typedef struct ftext_s ftext_t;
struct ftext_s {
  TSHARED_PTR(ftext_data_t) text_data;
  etext_flags_t flags;
};
STATIC_ASSERT(sizeof(ftext_t) == 24, "size mismatch");
STATIC_ASSERT(offsetof(ftext_t, flags) == 0x10, "invalid offset");

typedef struct fname_s fname_t;
struct fname_s {
  uint32_t cmp_idx;
  uint32_t num;
};
STATIC_ASSERT(sizeof(fname_t) == 8, "size mismatch");

typedef struct fscript_name_s fscript_name_t;
struct fscript_name_s {
  uint32_t cmp_idx;
  uint32_t display_idx;
  uint32_t num;
};
STATIC_ASSERT(sizeof(fscript_name_t) == 12, "size mismatch");

typedef struct fkey_s fkey_t;
struct fkey_s {
  fname_t name;
  TSHARED_PTR(void) details;
};
STATIC_ASSERT(sizeof(fkey_t) == 24, "size mismatch");

typedef struct finput_key_event_args_s finput_key_event_args_t;
struct finput_key_event_args_s {
  void          *viewport;
  int32_t        controller_id;
  fkey_t         key;
  einput_event_t event;
  float          amount_depressed;
  bool           is_touch_event;
};
STATIC_ASSERT(offsetof(finput_key_event_args_t, key) == 0x10, "invalid offset");
STATIC_ASSERT(offsetof(finput_key_event_args_t, event) == 0x28, "invalid offset");
STATIC_ASSERT(offsetof(finput_key_event_args_t, amount_depressed) == 0x2C, "invalid offset");
STATIC_ASSERT(offsetof(finput_key_event_args_t, is_touch_event) == 0x30, "invalid offset");

typedef struct fvector2d_s fvector2d_t;
struct fvector2d_s {
  float x;
  float y;
};
STATIC_ASSERT(sizeof(fvector2d_t) == 8, "size mismatch");

typedef struct fmodifier_keys_state_s fmodifier_keys_state_t;
struct fmodifier_keys_state_s {
  uint16_t is_left_shift_down : 1;
  uint16_t is_right_shift_down : 1;
  uint16_t is_left_control_down : 1;
  uint16_t is_right_control_down : 1;
  uint16_t is_left_alt_down : 1;
  uint16_t is_right_alt_down : 1;
  uint16_t is_left_command_down : 1;
  uint16_t is_right_command_down : 1;
  uint16_t are_caps_locked : 1;
};
STATIC_ASSERT(sizeof(fmodifier_keys_state_t) == 2, "size mismatch");

typedef uint8_t egesture_event_t;
enum {
  GESTURE_NONE       = 0,
  GESTURE_SCROLL     = 1,
  GESTURE_MAGNIFY    = 2,
  GESTURE_SWIPE      = 3,
  GESTURE_ROTATE     = 4,
  GESTURE_LONG_PRESS = 5,
  GESTURE_COUNT      = 6,
};

typedef struct finput_event_s finput_event_t;
struct finput_event_s {
  void                  *vftable;
  fmodifier_keys_state_t modifier_keys;
  bool                   is_repeat;
  uint8_t                _pad0[1];
  uint32_t               user_idx;
  void                  *event_path;
};
STATIC_ASSERT(sizeof(finput_event_t) == 0x18, "size mismatch");
STATIC_ASSERT(offsetof(finput_event_t, modifier_keys) == 0x08, "invalid offset");
STATIC_ASSERT(offsetof(finput_event_t, is_repeat) == 0x0A, "invalid offset");
STATIC_ASSERT(offsetof(finput_event_t, user_idx) == 0x0C, "invalid offset");
STATIC_ASSERT(offsetof(finput_event_t, event_path) == 0x10, "invalid offset");

typedef struct fkey_event_s fkey_event_t;
struct fkey_event_s {
  finput_event_t base;
  fkey_t         key;
  uint32_t       character_code;
  uint32_t       key_code;
};
STATIC_ASSERT(sizeof(fkey_event_t) == 0x38, "size mismatch");
STATIC_ASSERT(offsetof(fkey_event_t, key) == 0x18, "invalid offset");
STATIC_ASSERT(offsetof(fkey_event_t, character_code) == 0x30, "invalid offset");
STATIC_ASSERT(offsetof(fkey_event_t, key_code) == 0x34, "invalid offset");

typedef struct fanalog_input_event_s fanalog_input_event_t;
struct fanalog_input_event_s {
  fkey_event_t base;
  float        analog_value;
  uint8_t      _pad0[4];
};
STATIC_ASSERT(sizeof(fanalog_input_event_t) == 0x40, "size mismatch");
STATIC_ASSERT(offsetof(fanalog_input_event_t, analog_value) == 0x38, "invalid offset");

typedef struct fcharacter_event_s fcharacter_event_t;
struct fcharacter_event_s {
  finput_event_t base;
  uint16_t       character;
  uint8_t        _pad0[6];
};
STATIC_ASSERT(sizeof(fcharacter_event_t) == 0x20, "size mismatch");
STATIC_ASSERT(offsetof(fcharacter_event_t, character) == 0x18, "invalid offset");

typedef struct fpointer_event_s fpointer_event_t;
struct fpointer_event_s {
  finput_event_t   base;
  fvector2d_t      screen_space_pos;
  fvector2d_t      last_screen_space_pos;
  fvector2d_t      cursor_delta;
  void            *pressed_buttons;
  fkey_t           effecting_button;
  uint32_t         pointer_idx;
  uint32_t         touchpad_idx;
  float            force;
  bool             is_touch_event;
  egesture_event_t gesture_type;
  uint8_t          _pad0[2];
  fvector2d_t      wheel_or_gesture_delta;
  bool             is_direction_inverted_from_device;
  bool             is_touch_force_changed;
  bool             is_touch_first_move;
  uint8_t          _pad1[5];
};
STATIC_ASSERT(sizeof(fpointer_event_t) == 0x70, "size mismatch");
STATIC_ASSERT(offsetof(fpointer_event_t, screen_space_pos) == 0x18, "invalid offset");
STATIC_ASSERT(offsetof(fpointer_event_t, last_screen_space_pos) == 0x20, "invalid offset");
STATIC_ASSERT(offsetof(fpointer_event_t, cursor_delta) == 0x28, "invalid offset");
STATIC_ASSERT(offsetof(fpointer_event_t, pressed_buttons) == 0x30, "invalid offset");
STATIC_ASSERT(offsetof(fpointer_event_t, effecting_button) == 0x38, "invalid offset");
STATIC_ASSERT(offsetof(fpointer_event_t, pointer_idx) == 0x50, "invalid offset");
STATIC_ASSERT(offsetof(fpointer_event_t, touchpad_idx) == 0x54, "invalid offset");
STATIC_ASSERT(offsetof(fpointer_event_t, force) == 0x58, "invalid offset");
STATIC_ASSERT(offsetof(fpointer_event_t, is_touch_event) == 0x5C, "invalid offset");
STATIC_ASSERT(offsetof(fpointer_event_t, gesture_type) == 0x5D, "invalid offset");
STATIC_ASSERT(offsetof(fpointer_event_t, wheel_or_gesture_delta) == 0x60, "invalid offset");
STATIC_ASSERT(offsetof(fpointer_event_t, is_direction_inverted_from_device) == 0x68, "invalid offset");
STATIC_ASSERT(offsetof(fpointer_event_t, is_touch_force_changed) == 0x69, "invalid offset");
STATIC_ASSERT(offsetof(fpointer_event_t, is_touch_first_move) == 0x6A, "invalid offset");

typedef struct fname_entry_header_s fname_entry_header_t;
struct fname_entry_header_s {
  uint16_t is_wide : 1;
  uint16_t lowercase_hash : 5;
  uint16_t len : 10;
};

typedef struct fname_entry_s fname_entry_t;
struct fname_entry_s {
  fname_entry_header_t header;
  union {
    uint8_t  ansi[FNAME_ENTRY_NAME_SIZE];
    uint16_t wide[FNAME_ENTRY_NAME_SIZE];
  } name;
};

typedef struct fname_entry_allocator_s fname_entry_allocator_t;
struct fname_entry_allocator_s {
  void    *rwlock;
  uint32_t current_block;
  uint32_t current_byte_cursor;
  uint8_t *blocks[FNAME_MAX_BLOCKS];
};

typedef struct fname_slot_s fname_slot_t;
struct fname_slot_s {
  uint32_t id_and_hash;
};

typedef struct fname_pool_shard_s fname_pool_shard_t;
struct fname_pool_shard_s {
  void                    *rwlock;
  uint32_t                 used_slots;
  uint32_t                 cap_mask;
  fname_slot_t            *slots;
  fname_entry_allocator_t *entries;
  uint32_t                 num_created_entries;
  uint32_t                 num_created_wide_entries;
};

typedef struct fname_pool_s fname_pool_t;
struct fname_pool_s {
  fname_entry_allocator_t entries;
  fname_pool_shard_t      cmp_shards[FNAME_POOL_MAX_SHARDS];
  // other fields ...
};

struct uclass_s;
struct ufunc_s;
struct ufield_s;
struct uenum_s;
struct uscript_struct_s;

typedef struct uobject_s uobject_t;
struct uobject_s {
  void             *vftable;
  eobj_flags_t      obj_flags;
  int32_t           internal_idx;
  struct uclass_s  *cls;
  fname_t           name;
  struct uobject_s *outer;
};
STATIC_ASSERT(sizeof(uobject_t) == 0x28, "size mismatch");

typedef struct ufield_s ufield_t;
struct ufield_s {
  uobject_t        base;
  struct ufield_s *next;
};
STATIC_ASSERT(offsetof(ufield_t, next) == 0x28, "invalid offset");

typedef struct fthread_safe_counter_s fthread_safe_counter_t;
struct fthread_safe_counter_s {
  volatile int32_t counter;
};

typedef struct ffield_variant_s ffield_variant_t;
struct ffield_variant_s {
  union {
    struct ffield_s *field;
    uobject_t       *obj;
  } container;
  uint8_t is_uobject;
  uint8_t _pad0[7];
};

typedef struct ffield_class_s ffield_class_t;
struct ffield_class_s {
  fname_t                name;
  uint64_t               id;
  uint64_t               cast_flags;
  uint32_t               class_flags;
  struct ffield_class_s *super_class;
  struct ffield_s       *default_obj;
  struct ffield_s *(*ctor_fn)(ffield_variant_t *, fname_t *, uint32_t);
  fthread_safe_counter_t unique_name_idx_counter;
};

typedef struct ffield_s ffield_t;
struct ffield_s {
  void            *vftable;
  ffield_class_t  *cls;
  ffield_variant_t owner;
  struct ffield_s *next;
  fname_t          name;
  uint32_t         flags;
};

STATIC_ASSERT(offsetof(ffield_t, cls) == 0x08, "invalid offset");
STATIC_ASSERT(offsetof(ffield_t, owner) == 0x10, "invalid offset");
STATIC_ASSERT(offsetof(ffield_t, next) == 0x20, "invalid offset");
STATIC_ASSERT(offsetof(ffield_t, name) == 0x28, "invalid offset");
STATIC_ASSERT(offsetof(ffield_t, flags) == 0x30, "invalid offset");

typedef struct fprop_s fprop_t;
struct fprop_s {
  ffield_t        base;
  int32_t         array_dim;
  int32_t         elem_size;
  eprop_flags_t   prop_flags;
  uint16_t        rep_idx;
  uint8_t         bp_rep_cond;
  int32_t         offset_internal;
  fname_t         rep_notify_func;
  struct fprop_s *prop_link_next;
  struct fprop_s *next_ref;
  struct fprop_s *dtor_link_next;
  struct fprop_s *post_ctor_link_next;
};

STATIC_ASSERT(offsetof(fprop_t, array_dim) == 0x38, "invalid offset");
STATIC_ASSERT(offsetof(fprop_t, elem_size) == 0x3C, "invalid offset");
STATIC_ASSERT(offsetof(fprop_t, prop_flags) == 0x40, "invalid offset");
STATIC_ASSERT(offsetof(fprop_t, rep_idx) == 0x48, "invalid offset");
STATIC_ASSERT(offsetof(fprop_t, offset_internal) == 0x4C, "invalid offset");
STATIC_ASSERT(offsetof(fprop_t, rep_notify_func) == 0x50, "invalid offset");
STATIC_ASSERT(offsetof(fprop_t, prop_link_next) == 0x58, "invalid offset");
STATIC_ASSERT(offsetof(fprop_t, next_ref) == 0x60, "invalid offset");
STATIC_ASSERT(offsetof(fprop_t, dtor_link_next) == 0x68, "invalid offset");
STATIC_ASSERT(offsetof(fprop_t, post_ctor_link_next) == 0x70, "invalid offset");

/* ByteProperty */
typedef struct fprop_byte_s fprop_byte_t;
struct fprop_byte_s {
  fprop_t         base;
  struct uenum_s *uenum;
};

STATIC_ASSERT(offsetof(fprop_byte_t, uenum) == 0x78, "invalid offset");

/* UInt16Property */
typedef struct fprop_uint16_s fprop_uint16_t;
struct fprop_uint16_s {
  fprop_t base;
};

/* UInt32Property */
typedef struct fprop_uint32_s fprop_uint32_t;
struct fprop_uint32_s {
  fprop_t base;
};

/* UInt64Property */
typedef struct fprop_uint64_s fprop_uint64_t;
struct fprop_uint64_s {
  fprop_t base;
};

/* Int8Property */
typedef struct fprop_int8_s fprop_int8_t;
struct fprop_int8_s {
  fprop_t base;
};

/* Int16Property */
typedef struct fprop_int16_s fprop_int16_t;
struct fprop_int16_s {
  fprop_t base;
};

/* IntProperty */
typedef struct fprop_int_s fprop_int_t;
struct fprop_int_s {
  fprop_t base;
};

/* Int64Property */
typedef struct fprop_int64_s fprop_int64_t;
struct fprop_int64_s {
  fprop_t base;
};

/* FloatProperty */
typedef struct fprop_float_s fprop_float_t;
struct fprop_float_s {
  fprop_t base;
};

/* DoubleProperty */
typedef struct fprop_double_s fprop_double_t;
struct fprop_double_s {
  fprop_t base;
};

/* BoolProperty */
typedef struct fprop_bool_s fprop_bool_t;
struct fprop_bool_s {
  fprop_t base;
  uint8_t field_size;
  uint8_t byte_offset;
  uint8_t byte_mask;
  uint8_t field_mask;
};
STATIC_ASSERT(offsetof(fprop_bool_t, field_size) == 0x78, "invalid offset");
STATIC_ASSERT(offsetof(fprop_bool_t, byte_offset) == 0x79, "invalid offset");
STATIC_ASSERT(offsetof(fprop_bool_t, byte_mask) == 0x7a, "invalid offset");
STATIC_ASSERT(offsetof(fprop_bool_t, field_mask) == 0x7b, "invalid offset");

/* ObjectPropertyBase */
typedef struct fprop_obj_base_s fprop_obj_base_t;
struct fprop_obj_base_s {
  fprop_t          base;
  struct uclass_s *prop_class;
};
STATIC_ASSERT(offsetof(fprop_obj_base_t, prop_class) == 0x78, "invalid offset");

/* ObjectProperty */
typedef struct fprop_obj_s fprop_obj_t;
struct fprop_obj_s {
  fprop_obj_base_t base;
};

/* WeakObjectProperty */
typedef struct fprop_obj_weak_s fprop_obj_weak_t;
struct fprop_obj_weak_s {
  fprop_obj_base_t base;
};

/* LazyObjectProperty */
typedef struct fprop_obj_lazy_s fprop_obj_lazy_t;
struct fprop_obj_lazy_s {
  fprop_obj_base_t base;
};

/* SoftObjectProperty */
typedef struct fprop_obj_soft_s fprop_obj_soft_t;
struct fprop_obj_soft_s {
  fprop_obj_base_t base;
};

/* ClassProperty */
typedef struct fprop_class_s fprop_class_t;
struct fprop_class_s {
  fprop_obj_t      base;
  struct uclass_s *meta_class;
};
STATIC_ASSERT(offsetof(fprop_class_t, meta_class) == 0x80, "invalid offset");

/* SoftClassProperty */
typedef struct fprop_class_soft_s fprop_class_soft_t;
struct fprop_class_soft_s {
  fprop_obj_soft_t base;
  struct uclass_s *meta_class;
};
STATIC_ASSERT(offsetof(fprop_class_soft_t, meta_class) == 0x80, "invalid offset");

/* InterfaceProperty */
typedef struct fprop_iface_s fprop_iface_t;
struct fprop_iface_s {
  fprop_t          base;
  struct uclass_s *iface_class;
};
STATIC_ASSERT(offsetof(fprop_iface_t, iface_class) == 0x78, "invalid offset");

/* NameProperty */
typedef struct fprop_fname_s fprop_fname_t;
struct fprop_fname_s {
  fprop_t base;
};

/* StrProperty */
typedef struct fprop_str_s fprop_str_t;
struct fprop_str_s {
  fprop_t base;
};

typedef uint32_t earray_prop_flags_t;
enum {
  EAPF_NONE,
  EAPF_USES_MEM_IMAGE_ALLOCATOR,
};

/* ArrayProperty */
typedef struct fprop_array_s fprop_array_t;
struct fprop_array_s {
  fprop_t             base;
  fprop_t            *inner;
  earray_prop_flags_t array_flags;
};
STATIC_ASSERT(offsetof(fprop_array_t, inner) == 0x78, "invalid offset");
STATIC_ASSERT(offsetof(fprop_array_t, array_flags) == 0x80, "invalid offset");

typedef uint32_t emap_prop_flags_t;
enum {
  EMPF_NONE,
  EMPF_USES_MEM_IMAGE_ALLOCATOR,
};

typedef struct fscript_sparse_array_layout_s fscript_sparse_array_layout_t;
struct fscript_sparse_array_layout_s {
  int32_t alignment;
  int32_t size;
};

typedef struct fscript_set_layout_s fscript_set_layout_t;
struct fscript_set_layout_s {
  int32_t                       hash_next_id_offset;
  int32_t                       hash_idx_offset;
  int32_t                       size;
  fscript_sparse_array_layout_t sparse_array_layout;
};

typedef struct fscript_map_layout_s fscript_map_layout_t;
struct fscript_map_layout_s {
  int32_t              value_offset;
  fscript_set_layout_t set_layout;
};

/* MapProperty */
typedef struct fprop_map_s fprop_map_t;
struct fprop_map_s {
  fprop_t              base;
  fprop_t             *key_prop;
  fprop_t             *val_prop;
  fscript_map_layout_t map_layout;
  emap_prop_flags_t    map_flags;
};
STATIC_ASSERT(offsetof(fprop_map_t, key_prop) == 0x78, "invalid offset");
STATIC_ASSERT(offsetof(fprop_map_t, val_prop) == 0x80, "invalid offset");
STATIC_ASSERT(offsetof(fprop_map_t, map_layout) == 0x88, "invalid offset");
STATIC_ASSERT(offsetof(fprop_map_t, map_flags) == 0xa0, "invalid offset");

/* SetProperty */
typedef struct fprop_set_s fprop_set_t;
struct fprop_set_s {
  fprop_t              base;
  fprop_t             *elem_prop;
  fscript_set_layout_t set_layout;
};
STATIC_ASSERT(offsetof(fprop_set_t, elem_prop) == 0x78, "invalid offset");
STATIC_ASSERT(offsetof(fprop_set_t, set_layout) == 0x80, "invalid offset");

/* StructProperty */
typedef struct fprop_struct_s fprop_struct_t;
struct fprop_struct_s {
  fprop_t                  base;
  struct uscript_struct_s *script_struct;
};
STATIC_ASSERT(offsetof(fprop_struct_t, script_struct) == 0x78, "invalid offset");

/* DelegateProperty */
typedef struct fprop_delegate_s fprop_delegate_t;
struct fprop_delegate_s {
  fprop_t         base;
  struct ufunc_s *signature_func;
};
STATIC_ASSERT(offsetof(fprop_delegate_t, signature_func) == 0x78, "invalid offset");

/* MulticastDelegateProperty */
typedef struct fprop_mcast_delegate_s fprop_mcast_delegate_t;
struct fprop_mcast_delegate_s {
  fprop_t         base;
  struct ufunc_s *signature_func;
};
STATIC_ASSERT(offsetof(fprop_mcast_delegate_t, signature_func) == 0x78, "invalid offset");

typedef struct fprop_mcastinline_delegate_inline_s fprop_mcastinline_delegate_inline_t;
struct fprop_mcastinline_delegate_inline_s {
  fprop_mcast_delegate_t base;
};

typedef struct fprop_mcastinline_delegate_sparse_s fprop_mcastinline_delegate_sparse_t;
struct fprop_mcastinline_delegate_sparse_s {
  fprop_mcast_delegate_t base;
};

/* TextProperty */
typedef struct fprop_text_s fprop_text_t;
struct fprop_text_s {
  fprop_t base;
};

/* NumericProperty */
typedef struct fprop_numeric_s fprop_numeric_t;
struct fprop_numeric_s {
  fprop_t base;
};

/* EnumProperty */
typedef struct fprop_enum_s fprop_enum_t;
struct fprop_enum_s {
  fprop_t          base;
  fprop_numeric_t *underlying_prop;
  struct uenum_s  *uenum;
};
STATIC_ASSERT(offsetof(fprop_enum_t, underlying_prop) == 0x78, "invalid offset");
STATIC_ASSERT(offsetof(fprop_enum_t, uenum) == 0x80, "invalid offset");

/* FieldPathProperty */
typedef struct fprop_field_path_s fprop_field_path_t;
struct fprop_field_path_s {
  fprop_t         base;
  ffield_class_t *prop_class;
};
STATIC_ASSERT(offsetof(fprop_field_path_t, prop_class) == 0x78, "invalid offset");

typedef struct uenum_s uenum_t;
struct uenum_s {
  ufield_t  base;
  fstring_t cpp_type;
  TARRAY(TPAIR(fname_t, int64_t)) names;
  uint32_t cpp_form;
  uint32_t enum_flags;
  void    *enum_display_name_fn;
};

STATIC_ASSERT(offsetof(uenum_t, cpp_type) == 0x30, "invalid offset");
STATIC_ASSERT(offsetof(uenum_t, names) == 0x40, "invalid offset");
STATIC_ASSERT(offsetof(uenum_t, cpp_form) == 0x50, "invalid offset");
STATIC_ASSERT(offsetof(uenum_t, enum_flags) == 0x54, "invalid offset");
STATIC_ASSERT(offsetof(uenum_t, enum_display_name_fn) == 0x58, "invalid offset");

typedef TARRAY(uint8_t) ustruct_script_t;

// ustruct_t->children    (UObject/UField chain):   use for UFunction discovery
// ustruct_t->child_props (FField/FProperty chain): use for fields and params
typedef struct ustruct_s ustruct_t;
struct ustruct_s {
  ufield_t          base;
  uint64_t          unk[2];
  struct ustruct_s *super_struct;
  ufield_t         *children;
  struct ffield_s  *child_props;
  int32_t           props_size;
  int32_t           min_alignment;
  ustruct_script_t  script;
  struct fprop_s   *prop_link;
  struct fprop_s   *ref_link;
  struct fprop_s   *dtor_link;
  struct fprop_s   *post_ctor_link;
  TARRAY(uobject_t *) script_and_prop_obj_refs;
  void *unresolved_script_props;
  void *unversioned_schema;
};

STATIC_ASSERT(offsetof(ustruct_t, super_struct) == 0x40, "invalid offset");
STATIC_ASSERT(offsetof(ustruct_t, children) == 0x48, "invalid offset");
STATIC_ASSERT(offsetof(ustruct_t, child_props) == 0x50, "invalid offset");
STATIC_ASSERT(offsetof(ustruct_t, props_size) == 0x58, "invalid offset");
STATIC_ASSERT(offsetof(ustruct_t, min_alignment) == 0x5C, "invalid offset");
STATIC_ASSERT(offsetof(ustruct_t, script) == 0x60, "invalid offset");
STATIC_ASSERT(offsetof(ustruct_t, prop_link) == 0x70, "invalid offset");
STATIC_ASSERT(offsetof(ustruct_t, ref_link) == 0x78, "invalid offset");
STATIC_ASSERT(offsetof(ustruct_t, dtor_link) == 0x80, "invalid offset");
STATIC_ASSERT(offsetof(ustruct_t, post_ctor_link) == 0x88, "invalid offset");
STATIC_ASSERT(offsetof(ustruct_t, script_and_prop_obj_refs) == 0x90, "invalid offset");
STATIC_ASSERT(offsetof(ustruct_t, unresolved_script_props) == 0xA0, "invalid offset");

typedef struct icpp_struct_ops_s icpp_struct_ops_t;
struct icpp_struct_ops_s {
  void   *vftable;
  int32_t size;
  int32_t alignment;
};
STATIC_ASSERT(offsetof(icpp_struct_ops_t, size) == 0x08, "invalid offset");
STATIC_ASSERT(offsetof(icpp_struct_ops_t, alignment) == 0x0c, "invalid offset");

typedef struct uscript_struct_s uscript_struct_t;
struct uscript_struct_s {
  ustruct_t          base;
  estruct_flags_t    struct_flags;
  bool               prep_cpp_struct_ops_completed;
  icpp_struct_ops_t *cpp_struct_ops;
};
STATIC_ASSERT(offsetof(uscript_struct_t, struct_flags) == 0xb0, "invalid offset");
STATIC_ASSERT(offsetof(uscript_struct_t, prep_cpp_struct_ops_completed) == 0xb4, "invalid offset");
STATIC_ASSERT(offsetof(uscript_struct_t, cpp_struct_ops) == 0xb8, "invalid offset");

typedef struct ufunc_s ufunc_t;
struct ufunc_s {
  ustruct_t       base;
  uint32_t        func_flags;
  uint8_t         num_params;
  uint16_t        params_size;
  uint16_t        return_val_offset;
  uint16_t        rpc_id;
  uint16_t        rpc_rsp_id;
  struct fprop_s *first_prop_to_init;
  void           *event_graph_func;
  void           *event_graph_call_offset;
  void           *func;
};

STATIC_ASSERT(offsetof(ufunc_t, func_flags) == 0xB0, "invalid offset");
STATIC_ASSERT(offsetof(ufunc_t, num_params) == 0xB4, "invalid offset");
STATIC_ASSERT(offsetof(ufunc_t, params_size) == 0xB6, "invalid offset");
STATIC_ASSERT(offsetof(ufunc_t, return_val_offset) == 0xB8, "invalid offset");
STATIC_ASSERT(offsetof(ufunc_t, rpc_id) == 0xBA, "invalid offset");
STATIC_ASSERT(offsetof(ufunc_t, rpc_rsp_id) == 0xBC, "invalid offset");
STATIC_ASSERT(offsetof(ufunc_t, first_prop_to_init) == 0xC0, "invalid offset");
STATIC_ASSERT(offsetof(ufunc_t, event_graph_func) == 0xC8, "invalid offset");
STATIC_ASSERT(offsetof(ufunc_t, event_graph_call_offset) == 0xD0, "invalid offset");
STATIC_ASSERT(offsetof(ufunc_t, func) == 0xD8, "invalid offset");

typedef struct uclass_s uclass_t;
struct uclass_s {
  ustruct_t base;

  void (*class_ctor)(void *);
  uobject_t *(*class_vftable_helper_ctor_caller_type)(void *);
  void (*class_and_ref_objs_type)(uobject_t *, void *);

  uint32_t class_unique_and_cooked;
  uint32_t class_flags;
  uint64_t class_cast_flags;

  struct uclass_s *class_within;
  uobject_t       *class_generated_by;
  fname_t          class_config_name;
  TARRAY(void *) class_reps;
  TARRAY(ufield_t *) net_fields;
  int32_t first_owned_class_rep;

  uobject_t *cdo;
  void      *sparse_class_data;
  void      *sparse_class_data_struct;
};

STATIC_ASSERT(offsetof(uclass_t, class_ctor) == 0xB0, "invalid offset");
STATIC_ASSERT(offsetof(uclass_t, class_vftable_helper_ctor_caller_type) == 0xB8, "invalid offset");
STATIC_ASSERT(offsetof(uclass_t, class_and_ref_objs_type) == 0xC0, "invalid offset");
STATIC_ASSERT(offsetof(uclass_t, class_unique_and_cooked) == 0xC8, "invalid offset");
STATIC_ASSERT(offsetof(uclass_t, class_flags) == 0xCC, "invalid offset");
STATIC_ASSERT(offsetof(uclass_t, class_cast_flags) == 0xD0, "invalid offset");
STATIC_ASSERT(offsetof(uclass_t, class_within) == 0xD8, "invalid offset");
STATIC_ASSERT(offsetof(uclass_t, class_generated_by) == 0xE0, "invalid offset");
STATIC_ASSERT(offsetof(uclass_t, class_config_name) == 0xE8, "invalid offset");
STATIC_ASSERT(offsetof(uclass_t, net_fields) == 0x100, "invalid offset");
STATIC_ASSERT(offsetof(uclass_t, first_owned_class_rep) == 0x110, "invalid offset");
STATIC_ASSERT(offsetof(uclass_t, cdo) == 0x118, "invalid offset");
STATIC_ASSERT(offsetof(uclass_t, sparse_class_data) == 0x120, "invalid offset");
STATIC_ASSERT(offsetof(uclass_t, sparse_class_data_struct) == 0x128, "invalid offset");

typedef struct {
  int32_t object_idx;
  int32_t object_serial;
} fweak_object_ptr_t;

STATIC_ASSERT(sizeof(fweak_object_ptr_t) == 0x08, "size mismatch");

typedef struct {
  fweak_object_ptr_t object;
  fname_t            function_name;
} fscript_delegate_t;

STATIC_ASSERT(sizeof(fscript_delegate_t) == 0x10, "size mismatch");

typedef struct {
  fscript_delegate_t *data;
  int32_t             num;
  int32_t             max;
} fmulticast_script_delegate_t;

STATIC_ASSERT(sizeof(fmulticast_script_delegate_t) == 0x10, "size mismatch");

typedef struct {
  fname_t tag_name;
} fgameplay_tag_t;

STATIC_ASSERT(sizeof(fgameplay_tag_t) == 0x08, "size mismatch");

typedef struct foutput_device_s foutput_device_t;
struct foutput_device_s {
  void   *vftable;
  uint8_t suppress_event_tag;
  uint8_t auto_emit_line_terminator;
};

typedef struct fout_parm_rec_s fout_parm_rec_t;
struct fout_parm_rec_s {
  fprop_t                *prop;
  uint8_t                *prop_addr;
  struct fout_parm_rec_s *next_out_param;
};

typedef struct {
  uint32_t  inline_data[8];
  uint32_t *secondary_data;
  int32_t   num;
  int32_t   max;
} fflow_stack_t;

typedef struct fframe_s fframe_t;
struct fframe_s {
  foutput_device_t base;
  ufunc_t         *node;
  uobject_t       *obj;
  uint8_t         *code;
  uint8_t         *locals;
  fprop_t         *most_recent_prop;
  uint8_t         *most_recent_prop_addr;
  fflow_stack_t    flow_stack;
  struct fframe_s *prev_frame;
  fout_parm_rec_t *out_params;
  ffield_t        *prop_chain_for_compiled_in;
  ufunc_t         *current_native_func;
  uint8_t          array_ctx_failed;
};
STATIC_ASSERT(offsetof(fframe_t, node) == 0x10, "invalid offset");
STATIC_ASSERT(offsetof(fframe_t, obj) == 0x18, "invalid offset");
STATIC_ASSERT(offsetof(fframe_t, code) == 0x20, "invalid offset");
STATIC_ASSERT(offsetof(fframe_t, locals) == 0x28, "invalid offset");
STATIC_ASSERT(offsetof(fframe_t, most_recent_prop) == 0x30, "invalid offset");
STATIC_ASSERT(offsetof(fframe_t, most_recent_prop_addr) == 0x38, "invalid offset");
STATIC_ASSERT(offsetof(fframe_t, flow_stack) == 0x40, "invalid offset");
STATIC_ASSERT(offsetof(fframe_t, prev_frame) == 0x70, "invalid offset");
STATIC_ASSERT(offsetof(fframe_t, out_params) == 0x78, "invalid offset");
STATIC_ASSERT(offsetof(fframe_t, prop_chain_for_compiled_in) == 0x80, "invalid offset");
STATIC_ASSERT(offsetof(fframe_t, current_native_func) == 0x88, "invalid offset");
STATIC_ASSERT(offsetof(fframe_t, array_ctx_failed) == 0x90, "invalid offset");

typedef void(__fastcall *fnative_func_ptr_t)(uobject_t *context, fframe_t *stack, void *result);

typedef struct fuobject_item_s fuobject_item_t;
struct fuobject_item_s {
  uobject_t *obj;
  int32_t    flags;
  int32_t    cluster_root_idx;
  int32_t    serial_num;
};

typedef struct fchunked_fixed_uobject_array_s fchunked_fixed_uobject_array_t;
struct fchunked_fixed_uobject_array_s {
  fuobject_item_t **objs;
  fuobject_item_t  *preallocated_objs;
  int32_t           max_elems;
  int32_t           num_elems;
  int32_t           max_chunks;
  int32_t           num_chunks;
};

/* NOTE: creation and deletion listeners use the same callback signature */
typedef struct fuobject_listener_s fuobject_listener_t;

typedef void (*fuobject_listener_destroy_t)(fuobject_listener_t *self);
typedef void (*fuobject_listener_notify_cb_t)(fuobject_listener_t *self, const uobject_t *obj, int32_t idx);
typedef void (*fuobject_listener_shutdown_cb_t)(fuobject_listener_t *self);

typedef struct fuobject_listener_vftable_s fuobject_listener_vftable_t;
struct fuobject_listener_vftable_s {
  fuobject_listener_destroy_t     destroy;
  fuobject_listener_notify_cb_t   on_notify;
  fuobject_listener_shutdown_cb_t on_shutdown;
};

struct fuobject_listener_s {
  fuobject_listener_vftable_t *vftable;
};

typedef struct fcritical_section_s fcritical_section_t;
struct fcritical_section_s {
  void *opaque1[1];
  long  opaque2[2];
  void *opaque3[3];
};

typedef TARRAY(fuobject_listener_t *) tarray_fuobject_listener_t;

typedef struct fuobject_array_s fuobject_array_t;
struct fuobject_array_s {
  int32_t                        obj_first_cg_idx;
  int32_t                        obj_last_non_cg_idx;
  int32_t                        max_objs_not_considered_by_gc;
  bool                           open_for_disregard_for_gc;
  fchunked_fixed_uobject_array_t obj_objs;
  fcritical_section_t            obj_objs_critical;
  TARRAY(int32_t) obj_available_list;
  tarray_fuobject_listener_t create_listeners;
  tarray_fuobject_listener_t delete_listeners;
  fcritical_section_t        delete_listeners_critical;
  fthread_safe_counter_t     master_serial_num;
};
STATIC_ASSERT(sizeof(fuobject_array_t) == 0xB8, "size mismatch");

typedef struct fasset_data_s fasset_data_t;
struct fasset_data_s {
  fname_t obj_path;
  fname_t package_name;
  fname_t package_path;
  fname_t asset_name;
  fname_t asset_class;
};

typedef struct furl_s furl_t;
struct furl_s {
  fstring_t protocol;
  fstring_t host;
  int32_t   port;
  int32_t   valid;
  fstring_t map;
  fstring_t redirect_url;
  TARRAY(fstring_t) op;
  fstring_t portal;
};

typedef void uworld_t;

typedef struct fio_env_s fio_env_t;
struct fio_env_s {
  fstring_t path;
  int32_t   order;
};
STATIC_ASSERT(sizeof(fio_env_t) == 24, "size mismatch");

typedef struct fguid_s fguid_t;
struct fguid_s {
  uint32_t a;
  uint32_t b;
  uint32_t c;
  uint32_t d;
};
STATIC_ASSERT(sizeof(fguid_t) == 16, "size mismatch");

typedef struct faes_key_s faes_key_t;
struct faes_key_s {
  uint8_t key[32];
};
STATIC_ASSERT(sizeof(faes_key_t) == 32, "size mismatch");

typedef uint32_t eio_err_code_t;
enum {
  IO_ERROR_OK,
  IO_ERROR_UNKNOWN,
  IO_ERROR_INVALID_CODE,
  IO_ERROR_CANCELLED,
  IO_ERROR_FILE_OPEN_FAILED,
  IO_ERROR_FILE_NOT_OPEN,
  IO_ERROR_READ_ERROR,
  IO_ERROR_WRITE_ERROR,
  IO_ERROR_NOT_FOUND,
  IO_ERROR_CORRUPT_TOC,
  IO_ERROR_UNKNOWN_CHUNK_ID,
  IO_ERROR_INVALID_PARAMETER,
  IO_ERROR_SIGNATURE_ERROR,
  IO_ERROR_INVALID_ENCRYPTION_KEY
};

static inline const char *
eio_err_code_to_str(eio_err_code_t code)
{
  switch (code) {
  case IO_ERROR_OK:
    return "ok";
  case IO_ERROR_UNKNOWN:
    return "unknown";
  case IO_ERROR_INVALID_CODE:
    return "invalid code";
  case IO_ERROR_CANCELLED:
    return "cancelled";
  case IO_ERROR_FILE_OPEN_FAILED:
    return "file open failed";
  case IO_ERROR_FILE_NOT_OPEN:
    return "file not open";
  case IO_ERROR_READ_ERROR:
    return "read error";
  case IO_ERROR_WRITE_ERROR:
    return "write error";
  case IO_ERROR_NOT_FOUND:
    return "not found";
  case IO_ERROR_CORRUPT_TOC:
    return "corrupt TOC";
  case IO_ERROR_UNKNOWN_CHUNK_ID:
    return "uknown chunk ID";
  case IO_ERROR_INVALID_PARAMETER:
    return "invalid parameter";
  case IO_ERROR_SIGNATURE_ERROR:
    return "signature error";
  case IO_ERROR_INVALID_ENCRYPTION_KEY:
    return "invalid encryption key";
  }
  return "-";
}

typedef struct fio_status_s fio_status_t;
struct fio_status_s {
  eio_err_code_t err_code;
  uint16_t       err_msg[FIO_STATUS_ERR_MSG_LEN];
};
STATIC_ASSERT(sizeof(fio_status_t) == 260, "size mismatch");

typedef struct fio_dispatcher_impl_s fio_dispatcher_impl_t;
struct fio_dispatcher_impl_s {
  bool is_multithreaded;
  // lots of other fields..
};

typedef void fpak_platform_file_t;

typedef struct flayer_actor_stats_s flayer_actor_stats_t;
struct flayer_actor_stats_s {
  uclass_t *type;
  int32_t   total;
};

typedef struct ulayer_s ulayer_t;
struct ulayer_s {
  uobject_t base;
  fname_t   layer_name;
  uint32_t  is_visible;
  TARRAY(flayer_actor_stats_t) actor_stats;
};

typedef int32_t einternal_object_flags_t;
enum {
  IOF_NONE                 = 0,
  IOF_REACHABLE_IN_CLUSTER = (1 << 23),
  IOF_CLUSTER_ROOT         = (1 << 24),
  IOF_NATIVE               = (1 << 25),
  IOF_ASYNC                = (1 << 26),
  IOF_ASYNC_LOADING        = (1 << 27),
  IOF_UNREACHABLE          = (1 << 28),
  IOF_PENDING_KILL         = (1 << 29),
  IOF_ROOT_SET             = (1 << 30),
  IOF_PENDING_CONSTRUCTION = (1 << 31),
  IOF_GC_KEEP_FLAGS        = (IOF_NATIVE | IOF_ASYNC | IOF_ASYNC_LOADING),
};

typedef struct fstatic_construct_obj_params_s fstatic_construct_obj_params_t;
struct fstatic_construct_obj_params_s {
  uclass_t                *cls;
  uobject_t               *outer;
  fname_t                  name;
  eobj_flags_t             set_flags;
  einternal_object_flags_t internal_set_flags;
  bool                     copy_transients_from_class_defaults;
  bool                     assume_template_is_archetype;
  uobject_t               *tmpl;
  void                    *instance_graph;
  void                    *external_package;
};

typedef struct fvector_s fvector_t;
struct fvector_s {
  float x;
  float y;
  float z;
};

typedef struct fquat_s fquat_t;
struct fquat_s {
  float x;
  float y;
  float z;
  float w;
};

typedef struct ftransform_s ftransform_t;
ALIGNED_TYPE(struct, 16) ftransform_s
{
  fquat_t   rotation;
  fvector_t translation;
  float     _pad0[1];
  fvector_t scale3d;
  float     _pad1[1];
};
STATIC_ASSERT(sizeof(ftransform_t) == 48, "invalid size");

typedef int efind_name_t;
enum {
  FNAME_FIND,
  FNAME_FIND_OR_ADD,
  FNAME_REPLACE_NOT_SAFE_FOR_THREADING,
};

typedef struct begin_actor_spawn_s begin_actor_spawn_params_t;
struct begin_actor_spawn_s {
  uworld_t    *world_ctx_obj;
  uclass_t    *actor_class;
  ftransform_t spawn_transform;
  uint8_t      collision_handling_override;
  void        *owner;
  void        *return_value;
};
STATIC_ASSERT(offsetof(begin_actor_spawn_params_t, actor_class) == 8, "invalid offset");
STATIC_ASSERT(offsetof(begin_actor_spawn_params_t, spawn_transform) == 16, "invalid offset");
STATIC_ASSERT(offsetof(begin_actor_spawn_params_t, collision_handling_override) == 64, "invalid offset");
STATIC_ASSERT(offsetof(begin_actor_spawn_params_t, owner) == 72, "invalid offset");
STATIC_ASSERT(offsetof(begin_actor_spawn_params_t, return_value) == 80, "invalid offset");

typedef struct finish_actor_spawn_s finish_actor_spawn_params_t;
struct finish_actor_spawn_s {
  void        *actor;
  uint8_t      _pad0[8];
  ftransform_t spawn_transform;
  void        *return_value;
};
STATIC_ASSERT(offsetof(finish_actor_spawn_params_t, spawn_transform) == 16, "invalid offset");
STATIC_ASSERT(offsetof(finish_actor_spawn_params_t, return_value) == 64, "invalid offset");

typedef struct fsoft_object_path_s fsoft_object_path_t;
struct fsoft_object_path_s {
  fname_t   asset_path_name;
  fstring_t sub_path_string;
};

typedef struct execute_console_cmd_params_s execute_console_cmd_params_t;
struct execute_console_cmd_params_s {
  uobject_t *world_ctx_obj;
  fstring_t  cmd;
  void      *specific_player; // APlayerController *
};

/* ===================================================== FNAME ====================================================== */

bool
unreal_fname_equal(fname_t a, fname_t b, bool ignore_num);
fname_entry_t *
unreal_fname_entry_get(uint32_t cmp_idx);

uint64_t
unreal_fname_utf8_len(fname_t name);
uint64_t
unreal_fname_utf8_write(uint8_t *buf, uint64_t max_len, fname_t name);
str_t
unreal_fname_to_str(fname_t fname, arena_t *perm);
fname_t
unreal_fname_from_str(str_t s, efind_name_t find_type);

typedef bool (*fname_pool_iter_cb_t)(fname_t name, fname_entry_t *entry, void *user);
void
unreal_fname_pool_iterate(fname_pool_iter_cb_t cb, void *user);

bool
unreal_fname_entry_match_text(fname_entry_t *entry, str_t text, bool ignore_case, bool exact_match);
bool
unreal_fname_entry_match_text16(fname_entry_t *entry, str16_t text, bool ignore_case, bool exact_match);
bool
unreal_fname_match_text(fname_t name, str_t text, bool ignore_case, bool exact_match);
bool
unreal_fname_match_text16(fname_t name, str16_t text, bool ignore_case, bool exact_match);

/* ==================================================== FSTRING ===================================================== */
fstring_t
unreal_fstring_view_from_str16(str16_t s);
str16_t
unreal_fstring_view_to_str16(fstring_t fs);
fstring_t
unreal_fstring_from_str(str_t s, arena_t *perm);
str_t
unreal_fstring_to_str(fstring_t fs, arena_t *perm);

/* =================================================== FIOSTATUS ==================================================== */
str_t
unreal_fio_status_to_str(fio_status_t status, arena_t *perm);

/* ==================================================== UOBJECT ===================================================== */
typedef struct unreal_common_s unreal_common_t;
struct unreal_common_s {
  // core classes
  uclass_t  *core_object;          // Class /Script/CoreUObject.Object
  uclass_t  *core_class;           // Class /Script/CoreUObject.Class
  uclass_t  *core_scriptstruct;    // Class /Script/CoreUObject.ScriptStruct
  uclass_t  *core_func;            // Class /Script/CoreUObject.Function
  uclass_t  *core_enum;            // Class /Script/CoreUObject.Enum
  uclass_t  *core_package;         // Class /Script/CoreUObject.Package
  uclass_t  *actor;                // Class /Script/Engine.Actor
  uclass_t  *player_ctrl;          // Class /Script/Engine.PlayerController
  uclass_t  *local_player;         // Class /Script/Engine.LocalPlayer
  uclass_t  *pawn;                 // Class /Script/Engine.Pawn
  uclass_t  *hud;                  // Class /Script/Engine.HUD
  uclass_t  *world;                // Class /Script/Engine.World
  uclass_t  *game_instance;        // Class /Script/Engine.GameInstance
  uclass_t  *level;                // Class /Script/Engine.Level
  uclass_t  *actor_component;      // Class /Script/Engine.ActorComponent
  uclass_t  *widget;               // Class /Script/UMG.Widget
  uclass_t  *blueprint;            // Class /Script/Engine.Blueprint
  uclass_t  *data_asset;           // Class /Script/Engine.DataAsset
  uclass_t  *data_table;           // Class /Script/Engine.DataTable
  uobject_t *gameplay_statics_cdo; // GameplayStatics /Script/Engine.Default__GameplayStatics
  ufunc_t   *begin_spawn;          // Function /Script/Engine.GameplayStatics.BeginDeferredActorSpawnFromClass
  ufunc_t   *finish_spawn;         // Function /Script/Engine.GameplayStatics.FinishSpawningActor
  ufunc_t   *destroy_actor;        // Function /Script/Engine.Actor.K2_DestroyActor
  uclass_t  *kismet_sys_lib_cls;
  ufunc_t   *exec_console_cmd;

  // fnames
  fname_t bool_prop;
  fname_t byte_prop;
  fname_t int8_prop;
  fname_t int16_prop;
  fname_t int_prop;
  fname_t int32_prop;
  fname_t int64_prop;
  fname_t uint16_prop;
  fname_t uint32_prop;
  fname_t uint64_prop;
  fname_t float_prop;
  fname_t double_prop;
  fname_t name_prop;
  fname_t str_prop;
  fname_t text_prop;
  fname_t obj_prop;
  fname_t class_prop;
  fname_t soft_obj_prop;
  fname_t soft_class_prop;
  fname_t weak_obj_prop;
  fname_t lazy_obj_prop;
  fname_t interface_prop;
  fname_t struct_prop;
  fname_t array_prop;
  fname_t set_prop;
  fname_t map_prop;
  fname_t enum_prop;
  fname_t delegate_prop;
  fname_t mcast_delegate_prop;
  fname_t mcast_inline_delegate_prop;
  fname_t mcast_sparse_delegate_prop;

  fname_t rotator;
  fname_t color;

  fname_t vector;
  fname_t vector_net_quantize;
  fname_t vector_net_quantize10;
  fname_t vector_net_quantize100;
  fname_t vector_net_quantize_normal;
  fname_t vector2d;
  fname_t vector4;

  fname_t linear_color;
  fname_t quat;
  fname_t transform;

  fname_t key;
  fname_t gameplay_tag;
  fname_t gameplay_tag_container;

  fname_t guid;
  fname_t int_point;
  fname_t int_vector;
};

void
unreal_common_collect(unreal_common_t *common);

typedef enum {
  UOBJECT_LISTENER_KIND_CREATE,
  UOBJECT_LISTENER_KIND_DELETE,
} uobject_listener_kind_t;

typedef void (*uobject_on_notify_cb_t)(uobject_t *obj, int32_t idx, void *user);

typedef struct uobject_listener_s uobject_listener_t;
struct uobject_listener_s {
  fuobject_listener_vftable_t *vftable;
  fuobject_listener_vftable_t  vftable_backing;
  uobject_listener_kind_t      kind;
  bool                         occupied;
  uobject_on_notify_cb_t       on_notify;
  void                        *user;
};

void
uobject_listener_destroy_cb(fuobject_listener_t *self);
void
uobject_listener_on_notify_cb(fuobject_listener_t *self, const uobject_t *obj, int32_t idx);
void
uobject_listener_on_shutdown_cb(fuobject_listener_t *self);

void
unreal_register_uobject_listener(uobject_listener_kind_t kind, uobject_on_notify_cb_t notify_cb, void *user);
void
unreal_deregister_object_listener(uobject_listener_kind_t kind, uobject_on_notify_cb_t notify_cb, void *user);

void
unreal_uobject_listener_add(uobject_listener_t *listener);
void
unreal_uobject_listener_del(uobject_listener_t *listener);

fuobject_item_t *
unreal_uobject_array_get_item(int idx);
bool
unreal_uobject_array_item_is_valid(fuobject_item_t *item);
uobject_t *
unreal_uobject_array_get_obj(int idx);
bool
unreal_uobject_is_a(uobject_t *obj, uclass_t *cls);
bool
unreal_uobject_is_default(uobject_t *obj);
bool
unreal_uobject_is_valid(uobject_t *obj);

uint64_t
unreal_uobject_get_name_len(uobject_t *obj);
uint64_t
unreal_uobject_write_name(uobject_t *obj, uint8_t *dst, uint64_t cap);
str_t
unreal_uobject_push_name(uobject_t *obj, arena_t *perm);

bool
unreal_outer_chain_contains(uobject_t *obj, str_t str, bool ignore_case, bool exact_match);
bool
unreal_super_chain_contains(uclass_t *cls, str_t str, bool ignore_case, bool exact_match);

uint64_t
unreal_uobject_get_full_name_len(uobject_t *obj);
uint64_t
unreal_uobject_write_full_name(uobject_t *obj, uint8_t *dst, uint64_t cap);
str_t
unreal_uobject_push_full_name(uobject_t *obj, arena_t *perm);

int
unreal_uobject_array_count(void);
int
unreal_uobject_array_capacity(void);
int
unreal_uobject_array_num_chunks(void);
int
unreal_uobject_array_max_chunks(void);

int
unreal_uobject_array_first_gc_index(void);
int
unreal_uobject_array_last_non_gc_index(void);
bool
unreal_uobject_array_is_open_for_disregard_for_gc(void);

uobject_t *
unreal_uobject_find(str_t name, bool ignore_case, bool exact_match);
uclass_t *
unreal_uclass_find(str_t name, bool ignore_case, bool exact_match);
uobject_t *
unreal_uobject_find_first_of(uclass_t *cls);
uclass_t *
unreal_uobject_find_class_by_full_name(str_t full_name);
uobject_t *
unreal_uobject_find_by_full_name(uclass_t *cls, str_t full_name);

void
unreal_process_event(uobject_t *self, ufunc_t *func, void *params);

uobject_t *
unreal_spawn_actor(uobject_t *world_ctx_obj, uclass_t *cls);
void
unreal_despawn_actor(uobject_t *actor);
uclass_t *
unreal_load_class(uclass_t *base_cls, uobject_t *outer, str_t name, str_t filename, uint32_t load_flags);

bool
unreal_mount_pak(str_t file_path, int order);
bool
unreal_mount_iostore(str_t file_path, int order);

fnative_func_ptr_t
unreal_get_native(uint8_t opcode);
bool
unreal_fframe_step(fframe_t *stack, void *result);

/* ================================================ CLASS INTROSPECTION ============================================= */
fprop_t *
unreal_ustruct_find_prop(ustruct_t *s, str_t name);
ufunc_t *
unreal_ustruct_find_func(ustruct_t *s, str_t name);

ufunc_t *
unreal_ustruct_find_func_fname(ustruct_t *s, fname_t name, bool ignore_num);

void *
unreal_get_mcast_sparse_delegate(uobject_t *s, fname_t name);

/* ================================================= PROPERTY ACCESS ================================================ */
static inline void *
unreal_uprop_ptr(fprop_t *prop, void *container)
{
  return (uint8_t *)container + prop->offset_internal;
}

static inline bool
unreal_uprop_get_bool(fprop_bool_t *prop, void *container)
{
  uint8_t *byte = (uint8_t *)container + prop->base.offset_internal + prop->byte_offset;
  return (*byte & prop->field_mask) != 0;
}

static inline void
unreal_uprop_set_bool(fprop_bool_t *prop, void *container, bool val)
{
  uint8_t *byte = (uint8_t *)container + prop->base.offset_internal + prop->byte_offset;
  if (val) {
    *byte |= prop->field_mask;
  } else {
    *byte &= ~prop->field_mask;
  }
}

static inline fname_t
unreal_uprop_get_fname(fprop_t *prop, void *container)
{
  return *(fname_t *)unreal_uprop_ptr(prop, container);
}

static inline void
unreal_uprop_set_fname(fprop_t *prop, void *container, fname_t val)
{
  *(fname_t *)unreal_uprop_ptr(prop, container) = val;
}

static inline fstring_t
unreal_uprop_get_fstr(fprop_t *prop, void *container)
{
  return *(fstring_t *)unreal_uprop_ptr(prop, container);
}

static inline uobject_t *
unreal_uprop_get_obj(fprop_t *prop, void *container)
{
  return *(uobject_t **)unreal_uprop_ptr(prop, container);
}

static inline void
unreal_uprop_set_obj(fprop_t *prop, void *container, uobject_t *val)
{
  *(uobject_t **)unreal_uprop_ptr(prop, container) = val;
}

#endif /* UE_TYPES_H */
