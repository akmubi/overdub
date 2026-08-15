#ifndef MOD_UNREAL_H
#define MOD_UNREAL_H

#include "mod_sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#define TARRAY_INLINE(T, N) \
  struct {                  \
    struct {                \
      T     inline_data[N]; \
      void *secondary_data; \
    }       data;           \
    int32_t num;            \
    int32_t max;            \
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

#define TSPARSE(T)                          \
  struct {                                  \
    TARRAY(TSPARSE_SLOT(T)) data;           \
    tbit_array_t            alloc_flags;    \
    int32_t                 first_free_idx; \
    int32_t                 num_free_idx;   \
  }

#define TSET_ELEM(T)      \
  struct {                \
    T       val;          \
    int32_t hash_next_id; \
    int32_t hash_idx;     \
  }

#define TSET(T)                      \
  struct {                           \
    TSPARSE(TSET_ELEM(T)) elems;     \
    hash_allocator_t      hash;      \
    int32_t               hash_size; \
  }

#define TMAP(TK, TV)           \
  struct {                     \
    TSET(TPAIR(TK, TV)) pairs; \
  }

#define TSET_INVALID_ID (-1)

#define TSPARSE_NUM(SPARSE)                 ((SPARSE)->data.num - (SPARSE)->num_free_idx)
#define TSPARSE_MAX_INDEX(SPARSE)           ((SPARSE)->data.num)
#define TSPARSE_IS_ALLOCATED(SPARSE, IDX)   unreal_tbit_array_is_set(&(SPARSE)->alloc_flags, (IDX))
#define TSPARSE_ELEM_AT(SPARSE, IDX)        (&(SPARSE)->data.data[(IDX)].elem_data)
#define TSPARSE_FIRST_INDEX(SPARSE)         unreal_tbit_array_find_next_set(&(SPARSE)->alloc_flags, 0)
#define TSPARSE_NEXT_INDEX(SPARSE, IDX)     unreal_tbit_array_find_next_set(&(SPARSE)->alloc_flags, (IDX) + 1)
#define TSPARSE_FOR_EACH_INDEX(SPARSE, IDX) for (int32_t IDX = TSPARSE_FIRST_INDEX(SPARSE); IDX != TSET_INVALID_ID; IDX = TSPARSE_NEXT_INDEX((SPARSE), IDX))

#define TSET_NUM(SET)                       TSPARSE_NUM(&(SET)->elems)
#define TSET_MAX_INDEX(SET)                 TSPARSE_MAX_INDEX(&(SET)->elems)
#define TSET_IS_ALLOCATED(SET, IDX)         TSPARSE_IS_ALLOCATED(&(SET)->elems, (IDX))
#define TSET_ELEM_AT(SET, IDX)              TSPARSE_ELEM_AT(&(SET)->elems, (IDX))
#define TSET_FIRST_INDEX(SET)               TSPARSE_FIRST_INDEX(&(SET)->elems)
#define TSET_NEXT_INDEX(SET, IDX)           TSPARSE_NEXT_INDEX(&(SET)->elems, (IDX))
#define TSET_FOR_EACH_INDEX(SET, IDX)       for (int32_t IDX = TSET_FIRST_INDEX(SET); IDX != TSET_INVALID_ID; IDX = TSET_NEXT_INDEX((SET), IDX))

#define TMAP_NUM(MAP)                       TSET_NUM(&(MAP)->pairs)
#define TMAP_MAX_INDEX(MAP)                 TSET_MAX_INDEX(&(MAP)->pairs)
#define TMAP_IS_ALLOCATED(MAP, IDX)         TSET_IS_ALLOCATED(&(MAP)->pairs, (IDX))
#define TMAP_PAIR_AT(MAP, IDX)              (&TSET_ELEM_AT(&(MAP)->pairs, (IDX))->val)
#define TMAP_KEY_AT(MAP, IDX)               (TMAP_PAIR_AT((MAP), (IDX))->key)
#define TMAP_VALUE_AT(MAP, IDX)             (TMAP_PAIR_AT((MAP), (IDX))->value)
#define TMAP_FIRST_INDEX(MAP)               TSET_FIRST_INDEX(&(MAP)->pairs)
#define TMAP_NEXT_INDEX(MAP, IDX)           TSET_NEXT_INDEX(&(MAP)->pairs, (IDX))
#define TMAP_FOR_EACH_INDEX(MAP, IDX)       for (int32_t IDX = TMAP_FIRST_INDEX(MAP); IDX != TSET_INVALID_ID; IDX = TMAP_NEXT_INDEX((MAP), IDX))

#define TMAP_DECLARE_FUNCS(NAME, MAP_TYPE, KEY_TYPE, VALUE_TYPE) \
  VALUE_TYPE *                                                   \
  NAME##_find(MAP_TYPE *map, KEY_TYPE key);

#define TMAP_DEFINE_FUNCS(NAME, MAP_TYPE, KEY_TYPE, VALUE_TYPE, KEY_EQUAL, KEY_HASH)                             \
  VALUE_TYPE *                                                                                                   \
  NAME##_find(MAP_TYPE *map, KEY_TYPE key)                                                                       \
  {                                                                                                              \
    if (!map || TMAP_NUM(map) == 0 || map->pairs.hash_size <= 0) {                                               \
      return NULL;                                                                                               \
    }                                                                                                            \
                                                                                                                 \
    int32_t elem_idx = unreal_tset_hash_head(&map->pairs.hash, map->pairs.hash_size, (uint32_t)(KEY_HASH(key))); \
    while (elem_idx != TSET_INVALID_ID) {                                                                        \
      if (elem_idx < 0 || elem_idx >= TMAP_MAX_INDEX(map) || !TMAP_IS_ALLOCATED(map, elem_idx)) {                \
        MOD_ASSERT_MSG(false, "invalid TSet hash element index: %d", elem_idx);                                  \
        return NULL;                                                                                             \
      }                                                                                                          \
                                                                                                                 \
      if (KEY_EQUAL(TMAP_PAIR_AT(map, elem_idx)->key, key)) {                                                    \
        return &TMAP_PAIR_AT(map, elem_idx)->value;                                                              \
      }                                                                                                          \
                                                                                                                 \
      int32_t next_idx = TSET_ELEM_AT(&map->pairs, elem_idx)->hash_next_id;                                      \
      if (next_idx == elem_idx) {                                                                                \
        MOD_ASSERT_MSG(false, "TSet hash chain points to itself at index %d", elem_idx);                         \
        return NULL;                                                                                             \
      }                                                                                                          \
      elem_idx = next_idx;                                                                                       \
    }                                                                                                            \
                                                                                                                 \
    return NULL;                                                                                                 \
  }

typedef struct tmulticast_delegate_s tmulticast_delegate_t;
struct tmulticast_delegate_s {
  TARRAY(void) invocation_list;
  int32_t      compaction_threshold;
  int32_t      invocation_list_lock_count;
};

typedef struct bit_array_allocator_s bit_array_allocator_t;
struct bit_array_allocator_s {
  uint32_t inline_data[4];
  void    *secondary_data;
};
MOD_STATIC_ASSERT(sizeof(bit_array_allocator_t) == 24, "size mismatch");

typedef struct tbit_array_s tbit_array_t;
struct tbit_array_s {
  bit_array_allocator_t allocator;
  int32_t               num_bits;
  int32_t               max_bits;
};
MOD_STATIC_ASSERT(sizeof(tbit_array_t) == 32, "size mismatch");

typedef struct hash_allocator_s hash_allocator_t;
struct hash_allocator_s {
  int32_t inline_data[1];
  void   *secondary_data;
};
MOD_STATIC_ASSERT(sizeof(hash_allocator_t) == 16, "size mismatch");

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
MOD_STATIC_ASSERT(sizeof(fstring_t) == 16, "size mismatch");

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
MOD_STATIC_ASSERT(offsetof(ftext_data_t, local_str) == 0x08, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ftext_data_t, display_str) == 0x18, "invalid offset");

typedef struct ftext_s ftext_t;
struct ftext_s {
  TSHARED_PTR(ftext_data_t) text_data;
  etext_flags_t flags;
};
MOD_STATIC_ASSERT(sizeof(ftext_t) == 24, "size mismatch");
MOD_STATIC_ASSERT(offsetof(ftext_t, flags) == 0x10, "invalid offset");

typedef struct fscript_name_s fscript_name_t;
struct fscript_name_s {
  uint32_t cmp_idx;
  uint32_t display_idx;
  uint32_t num;
};
MOD_STATIC_ASSERT(sizeof(fscript_name_t) == 12, "size mismatch");

typedef struct frwlock_s {
  void *ptr;
} frwlock_t;
MOD_STATIC_ASSERT(sizeof(frwlock_t) == 8, "size mismatch");

typedef struct fcritical_section_s fcritical_section_t;
struct fcritical_section_s {
  void *opaque1[1];
  long  opaque2[2];
  void *opaque3[3];
};
MOD_STATIC_ASSERT(sizeof(fcritical_section_t) == 0x28, "size mismatch");

typedef struct fkey_s fkey_t;
struct fkey_s {
  fname_t name;
  TSHARED_PTR(void) details;
};
MOD_STATIC_ASSERT(sizeof(fkey_t) == 24, "size mismatch");

typedef struct finput_key_event_args_s finput_key_event_args_t;
struct finput_key_event_args_s {
  void          *viewport;
  int32_t        controller_id;
  fkey_t         key;
  einput_event_t event;
  float          amount_depressed;
  bool           is_touch_event;
};
MOD_STATIC_ASSERT(offsetof(finput_key_event_args_t, key) == 0x10, "invalid offset");
MOD_STATIC_ASSERT(offsetof(finput_key_event_args_t, event) == 0x28, "invalid offset");
MOD_STATIC_ASSERT(offsetof(finput_key_event_args_t, amount_depressed) == 0x2C, "invalid offset");
MOD_STATIC_ASSERT(offsetof(finput_key_event_args_t, is_touch_event) == 0x30, "invalid offset");

typedef struct fvector2d_s fvector2d_t;
struct fvector2d_s {
  float x;
  float y;
};
MOD_STATIC_ASSERT(sizeof(fvector2d_t) == 8, "size mismatch");

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
MOD_STATIC_ASSERT(sizeof(fmodifier_keys_state_t) == 2, "size mismatch");

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
MOD_STATIC_ASSERT(sizeof(finput_event_t) == 0x18, "size mismatch");
MOD_STATIC_ASSERT(offsetof(finput_event_t, modifier_keys) == 0x08, "invalid offset");
MOD_STATIC_ASSERT(offsetof(finput_event_t, is_repeat) == 0x0A, "invalid offset");
MOD_STATIC_ASSERT(offsetof(finput_event_t, user_idx) == 0x0C, "invalid offset");
MOD_STATIC_ASSERT(offsetof(finput_event_t, event_path) == 0x10, "invalid offset");

typedef struct fkey_event_s fkey_event_t;
struct fkey_event_s {
  finput_event_t base;
  fkey_t         key;
  uint32_t       character_code;
  uint32_t       key_code;
};
MOD_STATIC_ASSERT(sizeof(fkey_event_t) == 0x38, "size mismatch");
MOD_STATIC_ASSERT(offsetof(fkey_event_t, key) == 0x18, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fkey_event_t, character_code) == 0x30, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fkey_event_t, key_code) == 0x34, "invalid offset");

typedef struct fanalog_input_event_s fanalog_input_event_t;
struct fanalog_input_event_s {
  fkey_event_t base;
  float        analog_value;
  uint8_t      _pad0[4];
};
MOD_STATIC_ASSERT(sizeof(fanalog_input_event_t) == 0x40, "size mismatch");
MOD_STATIC_ASSERT(offsetof(fanalog_input_event_t, analog_value) == 0x38, "invalid offset");

typedef struct fcharacter_event_s fcharacter_event_t;
struct fcharacter_event_s {
  finput_event_t base;
  uint16_t       character;
  uint8_t        _pad0[6];
};
MOD_STATIC_ASSERT(sizeof(fcharacter_event_t) == 0x20, "size mismatch");
MOD_STATIC_ASSERT(offsetof(fcharacter_event_t, character) == 0x18, "invalid offset");

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
MOD_STATIC_ASSERT(sizeof(fpointer_event_t) == 0x70, "size mismatch");
MOD_STATIC_ASSERT(offsetof(fpointer_event_t, screen_space_pos) == 0x18, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fpointer_event_t, last_screen_space_pos) == 0x20, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fpointer_event_t, cursor_delta) == 0x28, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fpointer_event_t, pressed_buttons) == 0x30, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fpointer_event_t, effecting_button) == 0x38, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fpointer_event_t, pointer_idx) == 0x50, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fpointer_event_t, touchpad_idx) == 0x54, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fpointer_event_t, force) == 0x58, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fpointer_event_t, is_touch_event) == 0x5C, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fpointer_event_t, gesture_type) == 0x5D, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fpointer_event_t, wheel_or_gesture_delta) == 0x60, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fpointer_event_t, is_direction_inverted_from_device) == 0x68, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fpointer_event_t, is_touch_force_changed) == 0x69, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fpointer_event_t, is_touch_first_move) == 0x6A, "invalid offset");

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

typedef struct fprimary_asset_type_s {
  fname_t name;
} fprimary_asset_type_t;

typedef struct fprimary_asset_id_s
{
  fprimary_asset_type_t primary_asset_type;
  fname_t               primary_asset_name;
} fprimary_asset_id_t;

typedef enum {
  ETT_HIDDEN        = 0x0,
  ETT_ALPHABETICAL  = 0x1,
  ETT_NUMERICAL     = 0x2,
  ETT_DIMENSIONAL   = 0x3,
  ETT_CHRONOLOGICAL = 0x4,
} etag_type_t;

typedef enum {
  EDMT_NORMAL = 0x0,
  EDMT_WORLD  = 0x1,
  EDMT_PIE    = 0x2,
} eduplicate_mode_type_t;

typedef uint32_t erename_flags_t;

typedef struct fasset_registry_tag_s {
  fname_t     name;
  fstring_t   value;
  etag_type_t type;
  uint32_t    display_flags;
} fasset_registry_tag_t;

struct ustruct_s;
struct uclass_s;
struct ufunc_s;
struct ufield_s;
struct uenum_s;
struct uscript_struct_s;
struct uobject_s;
struct uworld_s;
struct fprop_s;
struct foutput_device_s;
struct fout_parm_rec_s;
struct fframe_s;

typedef TARRAY(struct uobject_s *)                   tarray_uobjectptr_t;
typedef TARRAY(void)                                 tarray_void_t;
typedef TMAP(fstring_t, fstring_t)                   tmap_fstring_fstring_t;
typedef TMAP(struct uobject_s *, struct uobject_s *) tmap_uobjectptr_uobjectptr_t;
typedef TARRAY(fasset_registry_tag_t)                tarray_fasset_registry_tag_t;

typedef void                     (__fastcall *uobject_destructor_fn_t)                                     (struct uobject_s *);
typedef void                     (__fastcall *uobject_register_deps_fn_t)                                  (struct uobject_s *);
typedef void                     (__fastcall *uobject_deferred_register_fn_t)                              (struct uobject_s *, struct uclass_s *, const wchar_t *, const wchar_t *);
typedef bool                     (__fastcall *uobject_can_be_cluster_root_fn_t)                            (struct uobject_s *);
typedef bool                     (__fastcall *uobject_can_be_in_cluster_fn_t)                              (struct uobject_s *);
typedef void                     (__fastcall *uobject_create_cluster_fn_t)                                 (struct uobject_s *);
typedef void                     (__fastcall *uobject_on_cluster_marked_as_pending_kill_fn_t)              (struct uobject_s *);
typedef fstring_t                (__fastcall *uobject_get_detailed_info_internal_fn_t)                     (struct uobject_s *);
typedef void                     (__fastcall *uobject_post_init_props_fn_t)                                (struct uobject_s *);
typedef void                     (__fastcall *uobject_post_cdo_contruct_fn_t)                              (struct uobject_s *);
typedef bool                     (__fastcall *uobject_pre_save_root_fn_t)                                  (struct uobject_s *, const wchar_t *);
typedef void                     (__fastcall *uobject_post_save_root_fn_t)                                 (struct uobject_s *, bool);
typedef void                     (__fastcall *uobject_pre_save_fn_t)                                       (struct uobject_s *, const void *);
typedef bool                     (__fastcall *uobject_is_ready_for_async_post_load_fn_t)                   (struct uobject_s *);
typedef void                     (__fastcall *uobject_post_load_fn_t)                                      (struct uobject_s *);
typedef void                     (__fastcall *uobject_post_load_subobjects_fn_t)                           (struct uobject_s *, void *);
typedef void                     (__fastcall *uobject_begin_destroy_fn_t)                                  (struct uobject_s *);
typedef bool                     (__fastcall *uobject_is_ready_for_finish_destroy_fn_t)                    (struct uobject_s *);
typedef void                     (__fastcall *uobject_finish_destroy_fn_t)                                 (struct uobject_s *);
typedef void                     (__fastcall *uobject_serialize_fn_t)                                      (struct uobject_s *, void *);
typedef void                     (__fastcall *uobject_serialize_farchive_fn_t)                             (struct uobject_s *, void *);
typedef void                     (__fastcall *uobject_shutdown_after_error_fn_t)                           (struct uobject_s *);
typedef void                     (__fastcall *uobject_post_interp_change_fn_t)                             (struct uobject_s *, struct fprop_s *);
typedef void                     (__fastcall *uobject_post_rename_fn_t)                                    (struct uobject_s *, struct uobject_s *, const fname_t);
typedef void                     (__fastcall *uobject_pre_duplicate_fn_t)                                  (struct uobject_s *, void *);
typedef void                     (__fastcall *uobject_post_duplicate_from_type_fn_t)                       (struct uobject_s *, eduplicate_mode_type_t);
typedef void                     (__fastcall *uobject_post_duplicate_fn_t)                                 (struct uobject_s *, bool);
typedef bool                     (__fastcall *uobject_needs_load_for_client_fn_t)                          (struct uobject_s *);
typedef bool                     (__fastcall *uobject_needs_load_for_server_fn_t)                          (struct uobject_s *);
typedef bool                     (__fastcall *uobject_needs_load_for_target_platform_fn_t)                 (struct uobject_s *, const void *);
typedef bool                     (__fastcall *uobject_needs_load_for_editor_game_fn_t)                     (struct uobject_s *);
typedef bool                     (__fastcall *uobject_is_editor_only_fn_t)                                 (struct uobject_s *);
typedef bool                     (__fastcall *uobject_has_non_editor_only_references_fn_t)                 (struct uobject_s *);
typedef bool                     (__fastcall *uobject_is_post_load_thread_safe_fn_t)                       (struct uobject_s *);
typedef bool                     (__fastcall *uobject_is_destruction_thread_safe_fn_t)                     (struct uobject_s *);
typedef void                     (__fastcall *uobject_get_preload_deps_fn_t)                               (struct uobject_s *, tarray_uobjectptr_t *);
typedef void                     (__fastcall *uobject_get_prestream_packages_fn_t)                         (struct uobject_s *, tarray_uobjectptr_t *);
typedef void                     (__fastcall *uobject_export_custom_props_fn_t)                            (struct uobject_s *, struct foutput_device_s *, uint32_t);
typedef void                     (__fastcall *uobject_import_custom_props_fn_t)                            (struct uobject_s *, const wchar_t *, void *);
typedef void                     (__fastcall *uobject_post_edit_import_fn_t)                               (struct uobject_s *);
typedef void                     (__fastcall *uobject_post_reload_config_fn_t)                             (struct uobject_s *, struct fprop_s *);
typedef bool                     (__fastcall *uobject_rename_fn_t)                                         (struct uobject_s *, const wchar_t *, struct uobject_s *, erename_flags_t);
typedef fstring_t                (__fastcall *uobject_get_desc_fn_t)                                       (struct uobject_s *);
typedef struct uscript_struct_s *(__fastcall *uobject_get_sparse_class_data_struct_fn_t)                   (struct uobject_s *);
typedef struct uworld_s         *(__fastcall *uobject_get_world_fn_t)                                      (struct uobject_s *);
typedef bool                     (__fastcall *uobject_get_native_prop_vals_fn_t)                           (struct uobject_s *, tmap_fstring_fstring_t *, uint32_t);
typedef void                     (__fastcall *uobject_get_resource_size_ex_fn_t)                           (struct uobject_s *, void *);
typedef fname_t                  (__fastcall *uobject_get_exporter_name_fn_t)                              (struct uobject_s *);
typedef void                    *(__fastcall *uobject_get_restore_for_uobject_overwrite_fn_t)              (struct uobject_s *);
typedef bool                     (__fastcall *uobject_are_native_props_identical_to_fn_t)                  (struct uobject_s *, struct uobject_s *);
typedef void                     (__fastcall *uobject_get_asset_registry_tags_fn_t)                        (struct uobject_s *, tarray_fasset_registry_tag_t *);
typedef bool                     (__fastcall *uobject_is_asset_fn_t)                                       (struct uobject_s *);
typedef fprimary_asset_id_t     *(__fastcall *uobject_get_primary_asset_id_fn_t)                           (struct uobject_s *, fprimary_asset_id_t *);
typedef bool                     (__fastcall *uobject_is_localized_resource_fn_t)                          (struct uobject_s *);
typedef bool                     (__fastcall *uobject_is_safe_for_root_set_fn_t)                           (struct uobject_s *);
typedef void                     (__fastcall *uobject_tag_subobjects_fn_t)                                 (struct uobject_s *, eobj_flags_t);
typedef void                     (__fastcall *uobject_get_lifetime_replicated_props_fn_t)                  (struct uobject_s *, tarray_void_t *);
typedef bool                     (__fastcall *uobject_is_name_stable_for_networking_fn_t)                  (struct uobject_s *);
typedef bool                     (__fastcall *uobject_is_full_name_stable_for_networking_fn_t)             (struct uobject_s *);
typedef bool                     (__fastcall *uobject_is_supported_for_networking_fn_t)                    (struct uobject_s *);
typedef void                     (__fastcall *uobject_get_subobjects_with_stable_names_for_networking_fn_t)(struct uobject_s *, tarray_uobjectptr_t *);
typedef void                     (__fastcall *uobject_pre_net_receive_fn_t)                                (struct uobject_s *);
typedef void                     (__fastcall *uobject_post_net_receive_fn_t)                               (struct uobject_s *);
typedef void                     (__fastcall *uobject_post_rep_notifies_fn_t)                              (struct uobject_s *);
typedef void                     (__fastcall *uobject_pre_destroy_from_replication_fn_t)                   (struct uobject_s *);
typedef void                     (__fastcall *uobject_build_subobject_mapping_fn_t)                        (struct uobject_s *, struct uobject_s *, tmap_uobjectptr_uobjectptr_t *);
typedef const wchar_t           *(__fastcall *uobject_get_config_override_platform_fn_t)                   (struct uobject_s *);
typedef void                     (__fastcall *uobject_override_per_object_config_section_fn_t)             (struct uobject_s *, fstring_t *);
typedef void                     (__fastcall *uobject_process_event_fn_t)                                  (struct uobject_s *, struct ufunc_s *, void *);
typedef int32_t                  (__fastcall *uobject_get_function_callspace_fn_t)                         (struct uobject_s *, struct ufunc_s *, struct fframe_s *);
typedef bool                     (__fastcall *uobject_call_remote_function_fn_t)                           (struct uobject_s *, struct ufunc_s *, void *, struct fout_parm_rec_s *, struct fframe_s *);
typedef bool                     (__fastcall *uobject_process_console_exec_fn_t)                           (struct uobject_s *, const wchar_t *, struct foutput_device_s *, struct uobject_s *);
typedef struct uclass_s         *(__fastcall *uobject_regenerate_class_fn_t)                               (struct uobject_s *, struct uclass_s *, struct uobject_s *);
typedef void                     (__fastcall *uobject_mark_as_editor_only_subobject_fn_t)                  (struct uobject_s *);
typedef bool                     (__fastcall *uobject_check_default_subobjects_internal_fn_t)              (struct uobject_s *);
typedef void                     (__fastcall *uobject_validate_generated_rep_enums_fn_t)                   (struct uobject_s *, const tarray_void_t *);
typedef void                     (__fastcall *uobject_set_net_push_id_dynamic_fn_t)                        (struct uobject_s *, const int32_t);
typedef int32_t                  (__fastcall *uobject_get_net_push_id_dynamic_fn_t)                        (struct uobject_s *);

typedef struct uobject_vtable_s uobject_vtable_t;
struct uobject_vtable_s {
  uobject_destructor_fn_t                                      destructor;
  uobject_register_deps_fn_t                                   register_deps;
  uobject_deferred_register_fn_t                               deferred_register;
  uobject_can_be_cluster_root_fn_t                             can_be_cluster_root;
  uobject_can_be_in_cluster_fn_t                               can_be_in_cluster;
  uobject_create_cluster_fn_t                                  create_cluster;
  uobject_on_cluster_marked_as_pending_kill_fn_t               on_cluster_marked_as_pending_kill;
  uobject_get_detailed_info_internal_fn_t                      get_detailed_info_internal;
  uobject_post_init_props_fn_t                                 post_init_props;
  uobject_post_cdo_contruct_fn_t                               post_cdo_contruct;
  uobject_pre_save_root_fn_t                                   pre_save_root;
  uobject_post_save_root_fn_t                                  post_save_root;
  uobject_pre_save_fn_t                                        pre_save;
  uobject_is_ready_for_async_post_load_fn_t                    is_ready_for_async_post_load;
  uobject_post_load_fn_t                                       post_load;
  uobject_post_load_subobjects_fn_t                            post_load_subobjects;
  uobject_begin_destroy_fn_t                                   begin_destroy;
  uobject_is_ready_for_finish_destroy_fn_t                     is_ready_for_finish_destroy;
  uobject_finish_destroy_fn_t                                  finish_destroy;
  uobject_serialize_fn_t                                       serialize;
  uobject_serialize_farchive_fn_t                              serialize_farchive;
  uobject_shutdown_after_error_fn_t                            shutdown_after_error;
  uobject_post_interp_change_fn_t                              post_interp_change;
  uobject_post_rename_fn_t                                     post_rename;
  uobject_pre_duplicate_fn_t                                   pre_duplicate;
  uobject_post_duplicate_from_type_fn_t                        post_duplicate_from_type;
  uobject_post_duplicate_fn_t                                  post_duplicate;
  uobject_needs_load_for_client_fn_t                           needs_load_for_client;
  uobject_needs_load_for_server_fn_t                           needs_load_for_server;
  uobject_needs_load_for_target_platform_fn_t                  needs_load_for_target_platform;
  uobject_needs_load_for_editor_game_fn_t                      needs_load_for_editor_game;
  uobject_is_editor_only_fn_t                                  is_editor_only;
  uobject_has_non_editor_only_references_fn_t                  has_non_editor_only_references;
  uobject_is_post_load_thread_safe_fn_t                        is_post_load_thread_safe;
  uobject_is_destruction_thread_safe_fn_t                      is_destruction_thread_safe;
  uobject_get_preload_deps_fn_t                                get_preload_deps;
  uobject_get_prestream_packages_fn_t                          get_prestream_packages;
  uobject_export_custom_props_fn_t                             export_custom_props;
  uobject_import_custom_props_fn_t                             import_custom_props;
  uobject_post_edit_import_fn_t                                post_edit_import;
  uobject_post_reload_config_fn_t                              post_reload_config;
  uobject_rename_fn_t                                          rename;
  uobject_get_desc_fn_t                                        get_desc;
  uobject_get_sparse_class_data_struct_fn_t                    get_sparse_class_data_struct;
  uobject_get_world_fn_t                                       get_world;
  uobject_get_native_prop_vals_fn_t                            get_native_prop_vals;
  uobject_get_resource_size_ex_fn_t                            get_resource_size_ex;
  uobject_get_exporter_name_fn_t                               get_exporter_name;
  uobject_get_restore_for_uobject_overwrite_fn_t               get_restore_for_uobject_overwrite;
  uobject_are_native_props_identical_to_fn_t                   are_native_props_identical_to;
  uobject_get_asset_registry_tags_fn_t                         get_asset_registry_tags;
  uobject_is_asset_fn_t                                        is_asset;
  uobject_get_primary_asset_id_fn_t                            get_primary_asset_id;
  uobject_is_localized_resource_fn_t                           is_localized_resource;
  uobject_is_safe_for_root_set_fn_t                            is_safe_for_root_set;
  uobject_tag_subobjects_fn_t                                  tag_subobjects;
  uobject_get_lifetime_replicated_props_fn_t                   get_lifetime_replicated_props;
  uobject_is_name_stable_for_networking_fn_t                   is_name_stable_for_networking;
  uobject_is_full_name_stable_for_networking_fn_t              is_full_name_stable_for_networking;
  uobject_is_supported_for_networking_fn_t                     is_supported_for_networking;
  uobject_get_subobjects_with_stable_names_for_networking_fn_t get_subobjects_with_stable_names_for_networking;
  uobject_pre_net_receive_fn_t                                 pre_net_receive;
  uobject_post_net_receive_fn_t                                post_net_receive;
  uobject_post_rep_notifies_fn_t                               post_rep_notifies;
  uobject_pre_destroy_from_replication_fn_t                    pre_destroy_from_replication;
  uobject_build_subobject_mapping_fn_t                         build_subobject_mapping;
  uobject_get_config_override_platform_fn_t                    get_config_override_platform;
  uobject_override_per_object_config_section_fn_t              override_per_object_config_section;
  uobject_process_event_fn_t                                   process_event;
  uobject_get_function_callspace_fn_t                          get_function_callspace;
  uobject_call_remote_function_fn_t                            call_remote_function;
  uobject_process_console_exec_fn_t                            process_console_exec;
  uobject_regenerate_class_fn_t                                regenerate_class;
  uobject_mark_as_editor_only_subobject_fn_t                   mark_as_editor_only_subobject;
  uobject_check_default_subobjects_internal_fn_t               check_default_subobjects_internal;
  uobject_validate_generated_rep_enums_fn_t                    validate_generated_rep_enums;
  uobject_set_net_push_id_dynamic_fn_t                         set_net_push_id_dynamic;
  uobject_get_net_push_id_dynamic_fn_t                         get_net_push_id_dynamic;
};
MOD_STATIC_ASSERT(sizeof(uobject_vtable_t) == 0x270, "size mismatch");

typedef struct uobject_s uobject_t;
struct uobject_s {
  uobject_vtable_t *vtable;
  eobj_flags_t      obj_flags;
  int32_t           internal_idx;
  struct uclass_s  *cls;
  fname_t           name;
  struct uobject_s *outer;
};
MOD_STATIC_ASSERT(sizeof(uobject_t) == 0x28, "size mismatch");

struct ufield_s;

typedef void (__fastcall *ufield_add_cpp_prop_fn_t)(struct ufield_s *, struct fprop_s *);
typedef void (__fastcall *ufield_bind_fn_t)        (struct ufield_s *); 

typedef struct ufield_vtable_s ufield_vtable_t;
struct ufield_vtable_s {
  uobject_destructor_fn_t                                      destructor;
  uobject_register_deps_fn_t                                   register_deps;
  uobject_deferred_register_fn_t                               deferred_register;
  uobject_can_be_cluster_root_fn_t                             can_be_cluster_root;
  uobject_can_be_in_cluster_fn_t                               can_be_in_cluster;
  uobject_create_cluster_fn_t                                  create_cluster;
  uobject_on_cluster_marked_as_pending_kill_fn_t               on_cluster_marked_as_pending_kill;
  uobject_get_detailed_info_internal_fn_t                      get_detailed_info_internal;
  uobject_post_init_props_fn_t                                 post_init_props;
  uobject_post_cdo_contruct_fn_t                               post_cdo_contruct;
  uobject_pre_save_root_fn_t                                   pre_save_root;
  uobject_post_save_root_fn_t                                  post_save_root;
  uobject_pre_save_fn_t                                        pre_save;
  uobject_is_ready_for_async_post_load_fn_t                    is_ready_for_async_post_load;
  uobject_post_load_fn_t                                       post_load;
  uobject_post_load_subobjects_fn_t                            post_load_subobjects;
  uobject_begin_destroy_fn_t                                   begin_destroy;
  uobject_is_ready_for_finish_destroy_fn_t                     is_ready_for_finish_destroy;
  uobject_finish_destroy_fn_t                                  finish_destroy;
  uobject_serialize_fn_t                                       serialize;
  uobject_serialize_farchive_fn_t                              serialize_farchive;
  uobject_shutdown_after_error_fn_t                            shutdown_after_error;
  uobject_post_interp_change_fn_t                              post_interp_change;
  uobject_post_rename_fn_t                                     post_rename;
  uobject_pre_duplicate_fn_t                                   pre_duplicate;
  uobject_post_duplicate_from_type_fn_t                        post_duplicate_from_type;
  uobject_post_duplicate_fn_t                                  post_duplicate;
  uobject_needs_load_for_client_fn_t                           needs_load_for_client;
  uobject_needs_load_for_server_fn_t                           needs_load_for_server;
  uobject_needs_load_for_target_platform_fn_t                  needs_load_for_target_platform;
  uobject_needs_load_for_editor_game_fn_t                      needs_load_for_editor_game;
  uobject_is_editor_only_fn_t                                  is_editor_only;
  uobject_has_non_editor_only_references_fn_t                  has_non_editor_only_references;
  uobject_is_post_load_thread_safe_fn_t                        is_post_load_thread_safe;
  uobject_is_destruction_thread_safe_fn_t                      is_destruction_thread_safe;
  uobject_get_preload_deps_fn_t                                get_preload_deps;
  uobject_get_prestream_packages_fn_t                          get_prestream_packages;
  uobject_export_custom_props_fn_t                             export_custom_props;
  uobject_import_custom_props_fn_t                             import_custom_props;
  uobject_post_edit_import_fn_t                                post_edit_import;
  uobject_post_reload_config_fn_t                              post_reload_config;
  uobject_rename_fn_t                                          rename;
  uobject_get_desc_fn_t                                        get_desc;
  uobject_get_sparse_class_data_struct_fn_t                    get_sparse_class_data_struct;
  uobject_get_world_fn_t                                       get_world;
  uobject_get_native_prop_vals_fn_t                            get_native_prop_vals;
  uobject_get_resource_size_ex_fn_t                            get_resource_size_ex;
  uobject_get_exporter_name_fn_t                               get_exporter_name;
  uobject_get_restore_for_uobject_overwrite_fn_t               get_restore_for_uobject_overwrite;
  uobject_are_native_props_identical_to_fn_t                   are_native_props_identical_to;
  uobject_get_asset_registry_tags_fn_t                         get_asset_registry_tags;
  uobject_is_asset_fn_t                                        is_asset;
  uobject_get_primary_asset_id_fn_t                            get_primary_asset_id;
  uobject_is_localized_resource_fn_t                           is_localized_resource;
  uobject_is_safe_for_root_set_fn_t                            is_safe_for_root_set;
  uobject_tag_subobjects_fn_t                                  tag_subobjects;
  uobject_get_lifetime_replicated_props_fn_t                   get_lifetime_replicated_props;
  uobject_is_name_stable_for_networking_fn_t                   is_name_stable_for_networking;
  uobject_is_full_name_stable_for_networking_fn_t              is_full_name_stable_for_networking;
  uobject_is_supported_for_networking_fn_t                     is_supported_for_networking;
  uobject_get_subobjects_with_stable_names_for_networking_fn_t get_subobjects_with_stable_names_for_networking;
  uobject_pre_net_receive_fn_t                                 pre_net_receive;
  uobject_post_net_receive_fn_t                                post_net_receive;
  uobject_post_rep_notifies_fn_t                               post_rep_notifies;
  uobject_pre_destroy_from_replication_fn_t                    pre_destroy_from_replication;
  uobject_build_subobject_mapping_fn_t                         build_subobject_mapping;
  uobject_get_config_override_platform_fn_t                    get_config_override_platform;
  uobject_override_per_object_config_section_fn_t              override_per_object_config_section;
  uobject_process_event_fn_t                                   process_event;
  uobject_get_function_callspace_fn_t                          get_function_callspace;
  uobject_call_remote_function_fn_t                            call_remote_function;
  uobject_process_console_exec_fn_t                            process_console_exec;
  uobject_regenerate_class_fn_t                                regenerate_class;
  uobject_mark_as_editor_only_subobject_fn_t                   mark_as_editor_only_subobject;
  uobject_check_default_subobjects_internal_fn_t               check_default_subobjects_internal;
  uobject_validate_generated_rep_enums_fn_t                    validate_generated_rep_enums;
  uobject_set_net_push_id_dynamic_fn_t                         set_net_push_id_dynamic;
  uobject_get_net_push_id_dynamic_fn_t                         get_net_push_id_dynamic;
  ufield_add_cpp_prop_fn_t                                     add_cpp_prop;
  ufield_bind_fn_t                                             bind;
};
MOD_STATIC_ASSERT(sizeof(ufield_vtable_t) == 0x280, "size mismatch");

typedef struct ufield_s ufield_t;
struct ufield_s {
  /* uobject_t */
  ufield_vtable_t  *vtable;
  eobj_flags_t      obj_flags;
  int32_t           internal_idx;
  struct uclass_s  *cls;
  fname_t           name;
  struct uobject_s *outer;
  /* ufield_t */
  struct ufield_s  *next;
};
MOD_STATIC_ASSERT(offsetof(ufield_t, next) == 0x28, "invalid offset");

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
  struct ffield_s       *(*ctor_fn)(ffield_variant_t *, fname_t *, uint32_t);
  fthread_safe_counter_t unique_name_idx_counter;
};

typedef TARRAY(struct ffield_s *) tarray_ffieldptr_t;

typedef void             (__fastcall *ffield_destructor_fn_t)              (struct ffield_s *);
typedef void             (__fastcall *ffield_serialize_fn_t)               (struct ffield_s *, void *);
typedef void             (__fastcall *ffield_post_load_fn_t)               (struct ffield_s *);
typedef void             (__fastcall *ffield_get_preload_deps_fn_t)        (struct ffield_s *, tarray_uobjectptr_t *);
typedef void             (__fastcall *ffield_begin_destroy_fn_t)           (struct ffield_s *);
typedef void             (__fastcall *ffield_add_referenced_objs_fn_t)     (struct ffield_s *, void *);
typedef void             (__fastcall *ffield_add_cpp_prop_fn_t)            (struct ffield_s *, struct fprop_s *);
typedef void             (__fastcall *ffield_bind_fn_t)                    (struct ffield_s *);
typedef void             (__fastcall *ffield_post_duplicate_fn_t)          (struct ffield_s *, struct ffield_s *);
typedef struct ffield_s *(__fastcall *ffield_get_inner_field_by_name_fn_t) (struct ffield_s *, fname_t *);
typedef void             (__fastcall *ffield_get_inner_fields_fn_t)        (struct ffield_s *, tarray_ffieldptr_t *);

typedef struct ffield_vtable_s ffield_vtable_t;
struct ffield_vtable_s {
  ffield_destructor_fn_t              destructor;
  ffield_serialize_fn_t               serialize;
  ffield_post_load_fn_t               post_load;
  ffield_get_preload_deps_fn_t        get_preload_deps;
  ffield_begin_destroy_fn_t           begin_destroy;
  ffield_add_referenced_objs_fn_t     add_referenced_objs;
  ffield_add_cpp_prop_fn_t            add_cpp_prop;
  ffield_bind_fn_t                    bind;
  ffield_post_duplicate_fn_t          post_duplicate;
  ffield_get_inner_field_by_name_fn_t get_inner_field_by_name;
  ffield_get_inner_fields_fn_t        get_inner_fields;
};
MOD_STATIC_ASSERT(sizeof(ffield_vtable_t) == 0x58, "size mismatch");

typedef struct ffield_s ffield_t;
struct ffield_s {
  ffield_vtable_t *vtable;
  ffield_class_t  *cls;
  ffield_variant_t owner;
  struct ffield_s *next;
  fname_t          name;
  uint32_t         flags;
};

MOD_STATIC_ASSERT(offsetof(ffield_t, cls)   == 0x08, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ffield_t, owner) == 0x10, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ffield_t, next)  == 0x20, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ffield_t, name)  == 0x28, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ffield_t, flags) == 0x30, "invalid offset");

typedef struct fguid_s fguid_t;
struct fguid_s {
  uint32_t a;
  uint32_t b;
  uint32_t c;
  uint32_t d;
};
MOD_STATIC_ASSERT(sizeof(fguid_t) == 16, "size mismatch");

typedef enum {
  ECFTR_USE_SERIALIZE_ITEM = 0x0,
  ECFTR_CANNOT_CONVERT     = 0x1,
  ECFTR_CONVERTED          = 0x2,
} econvert_from_type_result_t;

typedef struct fprop_tag_s {
  struct fprop_s *prop;
  fname_t         type;
  uint8_t         bool_val;
  fname_t         name;
  fname_t         struct_name;
  fname_t         enum_name;
  fname_t         inner_type;
  fname_t         value_jtype;
  int32_t         size;
  int32_t         array_index;
  int64_t         size_offset;
  fguid_t         struct_guid;
  uint8_t         has_prop_guid;
  fguid_t         prop_guid;
} fprop_tag_t;

typedef struct fstructured_archive_slot_s {
  void *ar;
} fstructured_archive_slot_t;

typedef enum {
  EPORT_NONE   = 0x0,
  EPORT_STRONG = 0x1,
  EPORT_WEAK   = 0x2,
} eprop_obj_ref_type_t;

struct fprop_struct_s;

typedef TARRAY(uint8_t)                 tarray_uint8_t;
typedef TARRAY(struct fprop_struct_s *) tarray_fprop_structptr_t;

typedef fstring_t                   (__fastcall *fprop_get_cpp_macro_type_fn_t)                 (struct fprop_s *, fstring_t *);
typedef bool                        (__fastcall *fprop_pass_cpp_args_by_ref_fn_t)               (struct fprop_s *);
typedef fstring_t                   (__fastcall *fprop_get_cpp_type_fn_t)                       (struct fprop_s *, fstring_t *, uint32_t);
typedef fstring_t                   (__fastcall *fprop_get_cpp_type_forward_declaration_fn_t)   (struct fprop_s *);
typedef void                        (__fastcall *fprop_link_internal_fn_t)                      (struct fprop_s *, void *);
typedef econvert_from_type_result_t (__fastcall *fprop_convert_from_type_fn_t)                  (struct fprop_s *, fprop_tag_t *, fstructured_archive_slot_t, uint8_t *, struct ustruct_s *);
typedef bool                        (__fastcall *fprop_identical_fn_t)                          (struct fprop_s *, const void *, const void *, uint32_t);
typedef void                        (__fastcall *fprop_serialize_item_fn_t)                     (struct fprop_s *, fstructured_archive_slot_t, void *, const void *);
typedef bool                        (__fastcall *fprop_netserialize_item_fn_t)                  (struct fprop_s *, void *, void *, void *, tarray_uint8_t *);
typedef bool                        (__fastcall *fprop_supports_net_shared_serialization_fn_t)  (struct fprop_s *);
typedef void                        (__fastcall *fprop_export_text_item_fn_t)                   (struct fprop_s *, fstring_t *, const void *, const void *, struct uobject_s *, int32_t, struct uobject_s *);
typedef const wchar_t              *(__fastcall *fprop_import_text_internal_fn_t)               (struct fprop_s *, const wchar_t *, void *, int32_t, struct uobject_s *, struct foutput_device_s *);
typedef void                        (__fastcall *fprop_copy_values_internal_fn_t)               (struct fprop_s *, void *, const void *, int32_t);
typedef uint32_t                    (__fastcall *fprop_get_value_type_hash_internal_fn_t)       (struct fprop_s *, const void *);
typedef void                        (__fastcall *fprop_copy_single_value_to_script_vm_fn_t)     (struct fprop_s *, void *, const void *);
typedef void                        (__fastcall *fprop_copy_complete_value_to_script_vm_fn_t)   (struct fprop_s *, void *, const void *);
typedef void                        (__fastcall *fprop_copy_single_value_from_script_vm_fn_t)   (struct fprop_s *, void *, const void *);
typedef void                        (__fastcall *fprop_copy_complete_value_from_script_vm_fn_t) (struct fprop_s *, void *, const void *);
typedef void                        (__fastcall *fprop_clear_value_internal_fn_t)               (struct fprop_s *, void *);
typedef void                        (__fastcall *fprop_destroy_value_internal_fn_t)             (struct fprop_s *, void *);
typedef void                        (__fastcall *fprop_initialize_value_internal_fn_t)          (struct fprop_s *, void *);
typedef fname_t                     (__fastcall *fprop_get_id_fn_t)                             (struct fprop_s *);
typedef void                        (__fastcall *fprop_instance_subobjects_fn_t)                (struct fprop_s *, void *, const void *, struct uobject_s *, void *);
typedef int32_t                     (__fastcall *fprop_get_min_alignment_fn_t)                  (struct fprop_s *);
typedef bool                        (__fastcall *fprop_contains_object_reference_fn_t)          (struct fprop_s *, tarray_fprop_structptr_t *, eprop_obj_ref_type_t);
typedef void                        (__fastcall *fprop_emit_reference_info_fn_t)                (struct fprop_s *, struct uclass_s *, int32_t, tarray_fprop_structptr_t *);
typedef bool                        (__fastcall *fprop_same_type_fn_t)                          (struct fprop_s *, struct fprop_s *);

typedef struct fprop_vtable_s fprop_vtable_t;
struct fprop_vtable_s {
  ffield_destructor_fn_t                        destructor;
  ffield_serialize_fn_t                         serialize;
  ffield_post_load_fn_t                         post_load;
  ffield_get_preload_deps_fn_t                  get_preload_deps;
  ffield_begin_destroy_fn_t                     begin_destroy;
  ffield_add_referenced_objs_fn_t               add_referenced_objs;
  ffield_add_cpp_prop_fn_t                      add_cpp_prop;
  ffield_bind_fn_t                              bind;
  ffield_post_duplicate_fn_t                    post_duplicate;
  ffield_get_inner_field_by_name_fn_t           get_inner_field_by_name;
  ffield_get_inner_fields_fn_t                  get_inner_fields;
  fprop_get_cpp_macro_type_fn_t                 get_cpp_macro_type;
  fprop_pass_cpp_args_by_ref_fn_t               pass_cpp_args_by_ref;
  fprop_get_cpp_type_fn_t                       get_cpp_type;
  fprop_get_cpp_type_forward_declaration_fn_t   get_cpp_type_forward_declaration;
  fprop_link_internal_fn_t                      link_internal;
  fprop_convert_from_type_fn_t                  convert_from_type;
  fprop_identical_fn_t                          identical;
  fprop_serialize_item_fn_t                     serialize_item;
  fprop_netserialize_item_fn_t                  netserialize_item;
  fprop_supports_net_shared_serialization_fn_t  supports_net_shared_serialization;
  fprop_export_text_item_fn_t                   export_text_item;
  fprop_import_text_internal_fn_t               import_text_internal;
  fprop_copy_values_internal_fn_t               copy_values_internal;
  fprop_get_value_type_hash_internal_fn_t       get_value_type_hash_internal;
  fprop_copy_single_value_to_script_vm_fn_t     copy_single_value_to_script_vm;
  fprop_copy_complete_value_to_script_vm_fn_t   copy_complete_value_to_script_vm;
  fprop_copy_single_value_from_script_vm_fn_t   copy_single_value_from_script_vm;
  fprop_copy_complete_value_from_script_vm_fn_t copy_complete_value_from_script_vm;
  fprop_clear_value_internal_fn_t               clear_value_internal;
  fprop_destroy_value_internal_fn_t             destroy_value_internal;
  fprop_initialize_value_internal_fn_t          initialize_value_internal;
  fprop_get_id_fn_t                             get_id;
  fprop_instance_subobjects_fn_t                instance_subobjects;
  fprop_get_min_alignment_fn_t                  get_min_alignment;
  fprop_contains_object_reference_fn_t          contains_object_reference;
  fprop_emit_reference_info_fn_t                emit_reference_info;
  fprop_same_type_fn_t                          same_type;
};
MOD_STATIC_ASSERT(sizeof(fprop_vtable_t) == 0x130, "size mismatch");

typedef struct fprop_s fprop_t;
struct fprop_s {
  /* ffield_t */
  fprop_vtable_t  *vtable;
  ffield_class_t  *cls;
  ffield_variant_t owner;
  struct ffield_s *next;
  fname_t          name;
  uint32_t         flags;
  uint32_t         pad;

  /* fprop_t */
  int32_t          array_dim;
  int32_t          elem_size;
  eprop_flags_t    prop_flags;
  uint16_t         rep_idx;
  uint8_t          bp_rep_cond;
  int32_t          offset_internal;
  fname_t          rep_notify_func;
  struct fprop_s  *prop_link_next;
  struct fprop_s  *next_ref;
  struct fprop_s  *dtor_link_next;
  struct fprop_s  *post_ctor_link_next;
};

MOD_STATIC_ASSERT(offsetof(fprop_t, array_dim) == 0x38, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fprop_t, elem_size) == 0x3C, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fprop_t, prop_flags) == 0x40, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fprop_t, rep_idx) == 0x48, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fprop_t, offset_internal) == 0x4C, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fprop_t, rep_notify_func) == 0x50, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fprop_t, prop_link_next) == 0x58, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fprop_t, next_ref) == 0x60, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fprop_t, dtor_link_next) == 0x68, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fprop_t, post_ctor_link_next) == 0x70, "invalid offset");

/* ByteProperty */
typedef struct fprop_byte_s fprop_byte_t;
struct fprop_byte_s {
  fprop_t         base;
  struct uenum_s *uenum;
};

MOD_STATIC_ASSERT(offsetof(fprop_byte_t, uenum) == 0x78, "invalid offset");

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
MOD_STATIC_ASSERT(offsetof(fprop_bool_t, field_size) == 0x78, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fprop_bool_t, byte_offset) == 0x79, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fprop_bool_t, byte_mask) == 0x7a, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fprop_bool_t, field_mask) == 0x7b, "invalid offset");

/* ObjectPropertyBase */
typedef struct fprop_obj_base_s fprop_obj_base_t;
struct fprop_obj_base_s {
  fprop_t          base;
  struct uclass_s *prop_class;
};
MOD_STATIC_ASSERT(offsetof(fprop_obj_base_t, prop_class) == 0x78, "invalid offset");

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
MOD_STATIC_ASSERT(offsetof(fprop_class_t, meta_class) == 0x80, "invalid offset");

/* SoftClassProperty */
typedef struct fprop_class_soft_s fprop_class_soft_t;
struct fprop_class_soft_s {
  fprop_obj_soft_t base;
  struct uclass_s *meta_class;
};
MOD_STATIC_ASSERT(offsetof(fprop_class_soft_t, meta_class) == 0x80, "invalid offset");

/* InterfaceProperty */
typedef struct fprop_iface_s fprop_iface_t;
struct fprop_iface_s {
  fprop_t          base;
  struct uclass_s *iface_class;
};
MOD_STATIC_ASSERT(offsetof(fprop_iface_t, iface_class) == 0x78, "invalid offset");

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
MOD_STATIC_ASSERT(offsetof(fprop_array_t, inner) == 0x78, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fprop_array_t, array_flags) == 0x80, "invalid offset");

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
MOD_STATIC_ASSERT(offsetof(fprop_map_t, key_prop)   == 0x78, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fprop_map_t, val_prop)   == 0x80, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fprop_map_t, map_layout) == 0x88, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fprop_map_t, map_flags)  == 0xa0, "invalid offset");

/* SetProperty */
typedef struct fprop_set_s fprop_set_t;
struct fprop_set_s {
  fprop_t              base;
  fprop_t             *elem_prop;
  fscript_set_layout_t set_layout;
};
MOD_STATIC_ASSERT(offsetof(fprop_set_t, elem_prop) == 0x78, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fprop_set_t, set_layout) == 0x80, "invalid offset");

/* StructProperty */
typedef struct fprop_struct_s fprop_struct_t;
struct fprop_struct_s {
  fprop_t                  base;
  struct uscript_struct_s *script_struct;
};
MOD_STATIC_ASSERT(offsetof(fprop_struct_t, script_struct) == 0x78, "invalid offset");

/* DelegateProperty */
typedef struct fprop_delegate_s fprop_delegate_t;
struct fprop_delegate_s {
  fprop_t         base;
  struct ufunc_s *signature_func;
};
MOD_STATIC_ASSERT(offsetof(fprop_delegate_t, signature_func) == 0x78, "invalid offset");

/* MulticastDelegateProperty */
typedef struct fprop_mcast_delegate_s fprop_mcast_delegate_t;
struct fprop_mcast_delegate_s {
  fprop_t         base;
  struct ufunc_s *signature_func;
};
MOD_STATIC_ASSERT(offsetof(fprop_mcast_delegate_t, signature_func) == 0x78, "invalid offset");

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
MOD_STATIC_ASSERT(offsetof(fprop_enum_t, underlying_prop) == 0x78, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fprop_enum_t, uenum) == 0x80, "invalid offset");

/* FieldPathProperty */
typedef struct fprop_field_path_s fprop_field_path_t;
struct fprop_field_path_s {
  fprop_t         base;
  ffield_class_t *prop_class;
};
MOD_STATIC_ASSERT(offsetof(fprop_field_path_t, prop_class) == 0x78, "invalid offset");

typedef TARRAY(TPAIR(fname_t, int64_t)) tarray_tpair_fname_int64_t;

typedef struct uenum_s uenum_t;
struct uenum_s {
  ufield_t                   base;
  fstring_t                  cpp_type;
  tarray_tpair_fname_int64_t names;
  uint32_t                   cpp_form;
  uint32_t                   enum_flags;
  void                      *enum_display_name_fn;
};

MOD_STATIC_ASSERT(offsetof(uenum_t, cpp_type) == 0x30, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uenum_t, names) == 0x40, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uenum_t, cpp_form) == 0x50, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uenum_t, enum_flags) == 0x54, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uenum_t, enum_display_name_fn) == 0x58, "invalid offset");

typedef TARRAY(uint8_t) ustruct_script_t;

typedef struct fstruct_base_chain_s fstruct_base_chain_t;
struct fstruct_base_chain_s {
  struct fstruct_base_chain_s **struct_base_chain_array;
  int32_t                       num_struct_bases_in_chain_minus_one;
};

typedef struct ustruct_s *(__fastcall *ustruct_get_inheritance_super_fn_t)        (struct ustruct_s *);
typedef void              (__fastcall *ustruct_link_fn_t)                         (struct ustruct_s *, void *, bool);
typedef void              (__fastcall *ustruct_serialize_bin_fn_t)                (struct ustruct_s *, void *, void *);
typedef void              (__fastcall *ustruct_serialize_bin2_fn_t)               (struct ustruct_s *, fstructured_archive_slot_t, void *);
typedef void              (__fastcall *ustruct_serialize_tagged_props_fn_t)       (struct ustruct_s *, void *, uint8_t *, struct ustruct_s *, uint8_t *, const struct uobject_s *);
typedef void              (__fastcall *ustruct_serialize_tagged_props2_fn_t)      (struct ustruct_s *, fstructured_archive_slot_t, uint8_t *, struct ustruct_s *, uint8_t *, const struct uobject_s *);
typedef void              (__fastcall *ustruct_init_struct_fn_t)                  (struct ustruct_s *, void *, int32_t);
typedef void              (__fastcall *ustruct_destroy_struct_fn_t)               (struct ustruct_s *, void *, int32_t);
typedef struct fprop_s   *(__fastcall *ustruct_custom_find_prop_fn_t)             (struct ustruct_s *, const fname_t);
typedef expr_token_t      (__fastcall *ustruct_serialize_expr_fn_t)               (struct ustruct_s *, int32_t *, void *);
typedef const wchar_t    *(__fastcall *ustruct_get_prefix_cpp_fn_t)               (struct ustruct_s *);
typedef void              (__fastcall *ustruct_set_super_struct_fn_t)             (struct ustruct_s *, struct ustruct_s *);
typedef fstring_t         (__fastcall *ustruct_prop_name_to_display_name_fn_t)    (struct ustruct_s *, fname_t);
typedef fstring_t         (__fastcall *ustruct_get_authored_name_for_field_fn_t)  (struct ustruct_s *, const struct ufield_s *);
typedef fstring_t         (__fastcall *ustruct_get_authored_name_for_field2_fn_t) (struct ustruct_s *, const struct ffield_s *);
typedef bool              (__fastcall *ustruct_is_struct_trashed_fn_t)            (struct ustruct_s *);
typedef fname_t           (__fastcall *ustruct_find_prop_name_from_guid_fn_t)     (struct ustruct_s *, const fguid_t *);
typedef fguid_t           (__fastcall *ustruct_find_prop_guid_from_name_fn_t)     (struct ustruct_s *, const fname_t);
typedef bool              (__fastcall *ustruct_are_prop_guids_available_fn_t)     (struct ustruct_s *);

typedef struct ustruct_vtable_s ustruct_vtable_t;
struct ustruct_vtable_s {
  uobject_destructor_fn_t                                      destructor;
  uobject_register_deps_fn_t                                   register_deps;
  uobject_deferred_register_fn_t                               deferred_register;
  uobject_can_be_cluster_root_fn_t                             can_be_cluster_root;
  uobject_can_be_in_cluster_fn_t                               can_be_in_cluster;
  uobject_create_cluster_fn_t                                  create_cluster;
  uobject_on_cluster_marked_as_pending_kill_fn_t               on_cluster_marked_as_pending_kill;
  uobject_get_detailed_info_internal_fn_t                      get_detailed_info_internal;
  uobject_post_init_props_fn_t                                 post_init_props;
  uobject_post_cdo_contruct_fn_t                               post_cdo_contruct;
  uobject_pre_save_root_fn_t                                   pre_save_root;
  uobject_post_save_root_fn_t                                  post_save_root;
  uobject_pre_save_fn_t                                        pre_save;
  uobject_is_ready_for_async_post_load_fn_t                    is_ready_for_async_post_load;
  uobject_post_load_fn_t                                       post_load;
  uobject_post_load_subobjects_fn_t                            post_load_subobjects;
  uobject_begin_destroy_fn_t                                   begin_destroy;
  uobject_is_ready_for_finish_destroy_fn_t                     is_ready_for_finish_destroy;
  uobject_finish_destroy_fn_t                                  finish_destroy;
  uobject_serialize_fn_t                                       serialize;
  uobject_serialize_farchive_fn_t                              serialize_farchive;
  uobject_shutdown_after_error_fn_t                            shutdown_after_error;
  uobject_post_interp_change_fn_t                              post_interp_change;
  uobject_post_rename_fn_t                                     post_rename;
  uobject_pre_duplicate_fn_t                                   pre_duplicate;
  uobject_post_duplicate_from_type_fn_t                        post_duplicate_from_type;
  uobject_post_duplicate_fn_t                                  post_duplicate;
  uobject_needs_load_for_client_fn_t                           needs_load_for_client;
  uobject_needs_load_for_server_fn_t                           needs_load_for_server;
  uobject_needs_load_for_target_platform_fn_t                  needs_load_for_target_platform;
  uobject_needs_load_for_editor_game_fn_t                      needs_load_for_editor_game;
  uobject_is_editor_only_fn_t                                  is_editor_only;
  uobject_has_non_editor_only_references_fn_t                  has_non_editor_only_references;
  uobject_is_post_load_thread_safe_fn_t                        is_post_load_thread_safe;
  uobject_is_destruction_thread_safe_fn_t                      is_destruction_thread_safe;
  uobject_get_preload_deps_fn_t                                get_preload_deps;
  uobject_get_prestream_packages_fn_t                          get_prestream_packages;
  uobject_export_custom_props_fn_t                             export_custom_props;
  uobject_import_custom_props_fn_t                             import_custom_props;
  uobject_post_edit_import_fn_t                                post_edit_import;
  uobject_post_reload_config_fn_t                              post_reload_config;
  uobject_rename_fn_t                                          rename;
  uobject_get_desc_fn_t                                        get_desc;
  uobject_get_sparse_class_data_struct_fn_t                    get_sparse_class_data_struct;
  uobject_get_world_fn_t                                       get_world;
  uobject_get_native_prop_vals_fn_t                            get_native_prop_vals;
  uobject_get_resource_size_ex_fn_t                            get_resource_size_ex;
  uobject_get_exporter_name_fn_t                               get_exporter_name;
  uobject_get_restore_for_uobject_overwrite_fn_t               get_restore_for_uobject_overwrite;
  uobject_are_native_props_identical_to_fn_t                   are_native_props_identical_to;
  uobject_get_asset_registry_tags_fn_t                         get_asset_registry_tags;
  uobject_is_asset_fn_t                                        is_asset;
  uobject_get_primary_asset_id_fn_t                            get_primary_asset_id;
  uobject_is_localized_resource_fn_t                           is_localized_resource;
  uobject_is_safe_for_root_set_fn_t                            is_safe_for_root_set;
  uobject_tag_subobjects_fn_t                                  tag_subobjects;
  uobject_get_lifetime_replicated_props_fn_t                   get_lifetime_replicated_props;
  uobject_is_name_stable_for_networking_fn_t                   is_name_stable_for_networking;
  uobject_is_full_name_stable_for_networking_fn_t              is_full_name_stable_for_networking;
  uobject_is_supported_for_networking_fn_t                     is_supported_for_networking;
  uobject_get_subobjects_with_stable_names_for_networking_fn_t get_subobjects_with_stable_names_for_networking;
  uobject_pre_net_receive_fn_t                                 pre_net_receive;
  uobject_post_net_receive_fn_t                                post_net_receive;
  uobject_post_rep_notifies_fn_t                               post_rep_notifies;
  uobject_pre_destroy_from_replication_fn_t                    pre_destroy_from_replication;
  uobject_build_subobject_mapping_fn_t                         build_subobject_mapping;
  uobject_get_config_override_platform_fn_t                    get_config_override_platform;
  uobject_override_per_object_config_section_fn_t              override_per_object_config_section;
  uobject_process_event_fn_t                                   process_event;
  uobject_get_function_callspace_fn_t                          get_function_callspace;
  uobject_call_remote_function_fn_t                            call_remote_function;
  uobject_process_console_exec_fn_t                            process_console_exec;
  uobject_regenerate_class_fn_t                                regenerate_class;
  uobject_mark_as_editor_only_subobject_fn_t                   mark_as_editor_only_subobject;
  uobject_check_default_subobjects_internal_fn_t               check_default_subobjects_internal;
  uobject_validate_generated_rep_enums_fn_t                    validate_generated_rep_enums;
  uobject_set_net_push_id_dynamic_fn_t                         set_net_push_id_dynamic;
  uobject_get_net_push_id_dynamic_fn_t                         get_net_push_id_dynamic;

  ufield_add_cpp_prop_fn_t                                     add_cpp_prop;
  ufield_bind_fn_t                                             bind;

  ustruct_get_inheritance_super_fn_t                           get_inheritance_super;
  ustruct_link_fn_t                                            link;
  ustruct_serialize_bin_fn_t                                   serialize_bin;
  ustruct_serialize_bin2_fn_t                                  serialize_bin2;
  ustruct_serialize_tagged_props_fn_t                          serialize_tagged_props;
  ustruct_serialize_tagged_props2_fn_t                         serialize_tagged_props2;
  ustruct_init_struct_fn_t                                     init_struct;
  ustruct_destroy_struct_fn_t                                  destroy_struct;
  ustruct_custom_find_prop_fn_t                                custom_find_prop;
  ustruct_serialize_expr_fn_t                                  serialize_expr;
  ustruct_get_prefix_cpp_fn_t                                  get_prefix_cpp;
  ustruct_set_super_struct_fn_t                                set_super_struct;
  ustruct_prop_name_to_display_name_fn_t                       prop_name_to_display_name;
  ustruct_get_authored_name_for_field_fn_t                     get_authored_name_for_field;
  ustruct_get_authored_name_for_field2_fn_t                    get_authored_name_for_field2;
  ustruct_is_struct_trashed_fn_t                               is_struct_trashed;
  ustruct_find_prop_name_from_guid_fn_t                        find_prop_name_from_guid;
  ustruct_find_prop_guid_from_name_fn_t                        find_prop_guid_from_name;
  ustruct_are_prop_guids_available_fn_t                        are_prop_guids_available;
};
MOD_STATIC_ASSERT(sizeof(ustruct_vtable_t) == 0x318, "size mismatch");

// ustruct_t->children    (UObject/UField chain):   use for UFunction discovery
// ustruct_t->child_props (FField/FProperty chain): use for fields and params
typedef struct ustruct_s ustruct_t;
struct ustruct_s {
  /* uobject_t */
  ustruct_vtable_t    *vtable;
  eobj_flags_t         obj_flags;
  int32_t              internal_idx;
  struct uclass_s     *cls;
  fname_t              name;
  struct uobject_s    *outer;
  /* ufield_t */
  struct ufield_s     *next;
  /* ustruct_t */
  fstruct_base_chain_t chain;
  struct ustruct_s    *super_struct;
  ufield_t            *children;
  struct ffield_s     *child_props;
  int32_t              props_size;
  int32_t              min_alignment;
  ustruct_script_t     script;
  struct fprop_s      *prop_link;
  struct fprop_s      *ref_link;
  struct fprop_s      *dtor_link;
  struct fprop_s      *post_ctor_link;
  tarray_uobjectptr_t  script_and_prop_obj_refs;
  void                *unresolved_script_props;
  void                *unversioned_schema;
};

MOD_STATIC_ASSERT(offsetof(ustruct_t, super_struct)             == 0x40, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ustruct_t, children)                 == 0x48, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ustruct_t, child_props)              == 0x50, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ustruct_t, props_size)               == 0x58, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ustruct_t, min_alignment)            == 0x5C, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ustruct_t, script)                   == 0x60, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ustruct_t, prop_link)                == 0x70, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ustruct_t, ref_link)                 == 0x78, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ustruct_t, dtor_link)                == 0x80, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ustruct_t, post_ctor_link)           == 0x88, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ustruct_t, script_and_prop_obj_refs) == 0x90, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ustruct_t, unresolved_script_props)  == 0xA0, "invalid offset");
MOD_STATIC_ASSERT(sizeof(ustruct_t)                             == 0xB0, "size mismatch");

typedef struct icpp_struct_ops_s icpp_struct_ops_t;
struct icpp_struct_ops_s {
  void   *vftable;
  int32_t size;
  int32_t alignment;
};
MOD_STATIC_ASSERT(offsetof(icpp_struct_ops_t, size) == 0x08, "invalid offset");
MOD_STATIC_ASSERT(offsetof(icpp_struct_ops_t, alignment) == 0x0c, "invalid offset");

typedef struct uscript_struct_s uscript_struct_t;
struct uscript_struct_s {
  /* uobject_t */
  ustruct_vtable_t    *vtable;
  eobj_flags_t         obj_flags;
  int32_t              internal_idx;
  struct uclass_s     *cls;
  fname_t              name;
  struct uobject_s    *outer;
  /* ufield_t */
  struct ufield_s     *next;
  /* ustruct_t */
  fstruct_base_chain_t chain;
  struct ustruct_s    *super_struct;
  ufield_t            *children;
  struct ffield_s     *child_props;
  int32_t              props_size;
  int32_t              min_alignment;
  ustruct_script_t     script;
  struct fprop_s      *prop_link;
  struct fprop_s      *ref_link;
  struct fprop_s      *dtor_link;
  struct fprop_s      *post_ctor_link;
  tarray_uobjectptr_t  script_and_prop_obj_refs;
  void                *unresolved_script_props;
  void                *unversioned_schema;
  /* uscript_struct_t */
  estruct_flags_t      struct_flags;
  bool                 prep_cpp_struct_ops_completed;
  icpp_struct_ops_t   *cpp_struct_ops;
};
MOD_STATIC_ASSERT(offsetof(uscript_struct_t, struct_flags) == 0xb0, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uscript_struct_t, prep_cpp_struct_ops_completed) == 0xb4, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uscript_struct_t, cpp_struct_ops) == 0xb8, "invalid offset");

typedef struct ufunc_s ufunc_t;
struct ufunc_s {
  /* uobject_t */
  ustruct_vtable_t    *vtable;
  eobj_flags_t         obj_flags;
  int32_t              internal_idx;
  struct uclass_s     *cls;
  fname_t              name;
  struct uobject_s    *outer;
  /* ufield_t */
  struct ufield_s     *next;
  /* ustruct_t */
  fstruct_base_chain_t chain;
  struct ustruct_s    *super_struct;
  ufield_t            *children;
  struct ffield_s     *child_props;
  int32_t              props_size;
  int32_t              min_alignment;
  ustruct_script_t     script;
  struct fprop_s      *prop_link;
  struct fprop_s      *ref_link;
  struct fprop_s      *dtor_link;
  struct fprop_s      *post_ctor_link;
  tarray_uobjectptr_t  script_and_prop_obj_refs;
  void                *unresolved_script_props;
  void                *unversioned_schema;
  /* ufunc_t */
  uint32_t             func_flags;
  uint8_t              num_params;
  uint16_t             params_size;
  uint16_t             return_val_offset;
  uint16_t             rpc_id;
  uint16_t             rpc_rsp_id;
  struct fprop_s      *first_prop_to_init;
  void                *event_graph_func;
  void                *event_graph_call_offset;
  void                *func;
};
MOD_STATIC_ASSERT(offsetof(ufunc_t, func_flags)              == 0xB0, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ufunc_t, num_params)              == 0xB4, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ufunc_t, params_size)             == 0xB6, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ufunc_t, return_val_offset)       == 0xB8, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ufunc_t, rpc_id)                  == 0xBA, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ufunc_t, rpc_rsp_id)              == 0xBC, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ufunc_t, first_prop_to_init)      == 0xC0, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ufunc_t, event_graph_func)        == 0xC8, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ufunc_t, event_graph_call_offset) == 0xD0, "invalid offset");
MOD_STATIC_ASSERT(offsetof(ufunc_t, func)                    == 0xD8, "invalid offset");
MOD_STATIC_ASSERT(sizeof(ufunc_t)                            == 0xE0, "size mismatch");

typedef struct foverride_s {
  fname_t          component_name;
  struct uclass_s *component_class;
} foverride_t;

typedef struct fsubobject_init_s {
  struct uobject_s *subobject;
  struct uobject_s *templ;
} fsubobject_init_t;

typedef TARRAY_INLINE(foverride_t,       8) foverrides_t;
typedef TARRAY_INLINE(fsubobject_init_t, 8) fsubobjects_to_init_t;

typedef struct fobject_initializer_s {
  struct uobject_s      *obj;
  struct uobject_s      *object_archetype;
  bool                   copy_transients_from_class_defaults;
  bool                   should_init_props_from_archetype;
  bool                   subobject_class_init_allowed;
  void                  *instance_graph;
  foverrides_t           component_overrides;
  fsubobjects_to_init_t  component_inits;
  struct uobject_s      *last_constructed_object;
} fobject_initializer_t;

MOD_STATIC_ASSERT(offsetof(fobject_initializer_t, object_archetype)                    == 0x008, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fobject_initializer_t, copy_transients_from_class_defaults) == 0x010, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fobject_initializer_t, should_init_props_from_archetype)    == 0x011, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fobject_initializer_t, subobject_class_init_allowed)        == 0x012, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fobject_initializer_t, instance_graph)                      == 0x018, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fobject_initializer_t, component_overrides)                 == 0x020, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fobject_initializer_t, component_inits)                     == 0x0B0, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fobject_initializer_t, last_constructed_object)             == 0x140, "invalid offset");
MOD_STATIC_ASSERT(sizeof(fobject_initializer_t)                                        == 0x148, "size mismatch");

typedef struct uclass_s  *(__fastcall *uclass_get_authoritative_class_fn_t)             (struct uclass_s *);
typedef void              (__fastcall *uclass_post_init_instance_fn_t)                  (struct uclass_s *, struct uobject_s *);
typedef void              (__fastcall *uclass_init_props_from_custom_list_fn_t)         (struct uclass_s *, uint8_t *, const uint8_t *);
typedef void              (__fastcall *uclass_setup_object_initializer_fn_t)            (struct uclass_s *, fobject_initializer_t *);
typedef uint8_t          *(__fastcall *uclass_get_persistent_uber_graph_frame_fn_t)     (struct uclass_s *, struct uobject_s *, struct ufunc_s *);
typedef void              (__fastcall *uclass_create_persistent_uber_graph_frame_fn_t)  (struct uclass_s *, struct uobject_s *, bool, bool, struct uclass_s *);
typedef void              (__fastcall *uclass_destroy_persistent_uber_graph_frame_fn_t) (struct uclass_s *, struct uobject_s *, bool);
typedef void              (__fastcall *uclass_serialize_default_object_fn_t)            (struct uclass_s *, struct uobject_s *, void *);
typedef void              (__fastcall *uclass_serialize_default_object2_fn_t)           (struct uclass_s *, struct uobject_s *, fstructured_archive_slot_t);
typedef void              (__fastcall *uclass_post_load_default_object_fn_t)            (struct uclass_s *, struct uobject_s *);
typedef void              (__fastcall *uclass_purge_class_fn_t)                         (struct uclass_s *, bool);
typedef bool              (__fastcall *uclass_is_function_implemented_in_script_fn_t)   (struct uclass_s *, fname_t);
typedef bool              (__fastcall *uclass_has_prop_fn_t)                            (struct uclass_s *, struct fprop_s *);
typedef struct uobject_s *(__fastcall *uclass_find_archetype_fn_t)                      (struct uclass_s *, const struct uclass_s *, const fname_t);
typedef struct uobject_s *(__fastcall *uclass_get_archetype_for_cdo_fn_t)               (struct uclass_s *);
typedef void             *(__fastcall *uclass_get_archetype_for_sparse_class_data_fn_t) (struct uclass_s *);
typedef void              (__fastcall *uclass_get_default_object_preload_deps_fn_t)     (struct uclass_s *, tarray_uobjectptr_t *);
typedef struct uobject_s *(__fastcall *uclass_create_default_object_fn_t)               (struct uclass_s *); 

typedef struct uclass_vtable_s uclass_vtable_t;
struct uclass_vtable_s {
  uobject_destructor_fn_t                                      destructor;
  uobject_register_deps_fn_t                                   register_deps;
  uobject_deferred_register_fn_t                               deferred_register;
  uobject_can_be_cluster_root_fn_t                             can_be_cluster_root;
  uobject_can_be_in_cluster_fn_t                               can_be_in_cluster;
  uobject_create_cluster_fn_t                                  create_cluster;
  uobject_on_cluster_marked_as_pending_kill_fn_t               on_cluster_marked_as_pending_kill;
  uobject_get_detailed_info_internal_fn_t                      get_detailed_info_internal;
  uobject_post_init_props_fn_t                                 post_init_props;
  uobject_post_cdo_contruct_fn_t                               post_cdo_contruct;
  uobject_pre_save_root_fn_t                                   pre_save_root;
  uobject_post_save_root_fn_t                                  post_save_root;
  uobject_pre_save_fn_t                                        pre_save;
  uobject_is_ready_for_async_post_load_fn_t                    is_ready_for_async_post_load;
  uobject_post_load_fn_t                                       post_load;
  uobject_post_load_subobjects_fn_t                            post_load_subobjects;
  uobject_begin_destroy_fn_t                                   begin_destroy;
  uobject_is_ready_for_finish_destroy_fn_t                     is_ready_for_finish_destroy;
  uobject_finish_destroy_fn_t                                  finish_destroy;
  uobject_serialize_fn_t                                       serialize;
  uobject_serialize_farchive_fn_t                              serialize_farchive;
  uobject_shutdown_after_error_fn_t                            shutdown_after_error;
  uobject_post_interp_change_fn_t                              post_interp_change;
  uobject_post_rename_fn_t                                     post_rename;
  uobject_pre_duplicate_fn_t                                   pre_duplicate;
  uobject_post_duplicate_from_type_fn_t                        post_duplicate_from_type;
  uobject_post_duplicate_fn_t                                  post_duplicate;
  uobject_needs_load_for_client_fn_t                           needs_load_for_client;
  uobject_needs_load_for_server_fn_t                           needs_load_for_server;
  uobject_needs_load_for_target_platform_fn_t                  needs_load_for_target_platform;
  uobject_needs_load_for_editor_game_fn_t                      needs_load_for_editor_game;
  uobject_is_editor_only_fn_t                                  is_editor_only;
  uobject_has_non_editor_only_references_fn_t                  has_non_editor_only_references;
  uobject_is_post_load_thread_safe_fn_t                        is_post_load_thread_safe;
  uobject_is_destruction_thread_safe_fn_t                      is_destruction_thread_safe;
  uobject_get_preload_deps_fn_t                                get_preload_deps;
  uobject_get_prestream_packages_fn_t                          get_prestream_packages;
  uobject_export_custom_props_fn_t                             export_custom_props;
  uobject_import_custom_props_fn_t                             import_custom_props;
  uobject_post_edit_import_fn_t                                post_edit_import;
  uobject_post_reload_config_fn_t                              post_reload_config;
  uobject_rename_fn_t                                          rename;
  uobject_get_desc_fn_t                                        get_desc;
  uobject_get_sparse_class_data_struct_fn_t                    get_sparse_class_data_struct;
  uobject_get_world_fn_t                                       get_world;
  uobject_get_native_prop_vals_fn_t                            get_native_prop_vals;
  uobject_get_resource_size_ex_fn_t                            get_resource_size_ex;
  uobject_get_exporter_name_fn_t                               get_exporter_name;
  uobject_get_restore_for_uobject_overwrite_fn_t               get_restore_for_uobject_overwrite;
  uobject_are_native_props_identical_to_fn_t                   are_native_props_identical_to;
  uobject_get_asset_registry_tags_fn_t                         get_asset_registry_tags;
  uobject_is_asset_fn_t                                        is_asset;
  uobject_get_primary_asset_id_fn_t                            get_primary_asset_id;
  uobject_is_localized_resource_fn_t                           is_localized_resource;
  uobject_is_safe_for_root_set_fn_t                            is_safe_for_root_set;
  uobject_tag_subobjects_fn_t                                  tag_subobjects;
  uobject_get_lifetime_replicated_props_fn_t                   get_lifetime_replicated_props;
  uobject_is_name_stable_for_networking_fn_t                   is_name_stable_for_networking;
  uobject_is_full_name_stable_for_networking_fn_t              is_full_name_stable_for_networking;
  uobject_is_supported_for_networking_fn_t                     is_supported_for_networking;
  uobject_get_subobjects_with_stable_names_for_networking_fn_t get_subobjects_with_stable_names_for_networking;
  uobject_pre_net_receive_fn_t                                 pre_net_receive;
  uobject_post_net_receive_fn_t                                post_net_receive;
  uobject_post_rep_notifies_fn_t                               post_rep_notifies;
  uobject_pre_destroy_from_replication_fn_t                    pre_destroy_from_replication;
  uobject_build_subobject_mapping_fn_t                         build_subobject_mapping;
  uobject_get_config_override_platform_fn_t                    get_config_override_platform;
  uobject_override_per_object_config_section_fn_t              override_per_object_config_section;
  uobject_process_event_fn_t                                   process_event;
  uobject_get_function_callspace_fn_t                          get_function_callspace;
  uobject_call_remote_function_fn_t                            call_remote_function;
  uobject_process_console_exec_fn_t                            process_console_exec;
  uobject_regenerate_class_fn_t                                regenerate_class;
  uobject_mark_as_editor_only_subobject_fn_t                   mark_as_editor_only_subobject;
  uobject_check_default_subobjects_internal_fn_t               check_default_subobjects_internal;
  uobject_validate_generated_rep_enums_fn_t                    validate_generated_rep_enums;
  uobject_set_net_push_id_dynamic_fn_t                         set_net_push_id_dynamic;
  uobject_get_net_push_id_dynamic_fn_t                         get_net_push_id_dynamic;

  ufield_add_cpp_prop_fn_t                                     add_cpp_prop;
  ufield_bind_fn_t                                             bind;

  ustruct_get_inheritance_super_fn_t                           get_inheritance_super;
  ustruct_link_fn_t                                            link;
  ustruct_serialize_bin_fn_t                                   serialize_bin;
  ustruct_serialize_bin2_fn_t                                  serialize_bin2;
  ustruct_serialize_tagged_props_fn_t                          serialize_tagged_props;
  ustruct_serialize_tagged_props2_fn_t                         serialize_tagged_props2;
  ustruct_init_struct_fn_t                                     init_struct;
  ustruct_destroy_struct_fn_t                                  destroy_struct;
  ustruct_custom_find_prop_fn_t                                custom_find_prop;
  ustruct_serialize_expr_fn_t                                  serialize_expr;
  ustruct_get_prefix_cpp_fn_t                                  get_prefix_cpp;
  ustruct_set_super_struct_fn_t                                set_super_struct;
  ustruct_prop_name_to_display_name_fn_t                       prop_name_to_display_name;
  ustruct_get_authored_name_for_field_fn_t                     get_authored_name_for_field;
  ustruct_get_authored_name_for_field2_fn_t                    get_authored_name_for_field2;
  ustruct_is_struct_trashed_fn_t                               is_struct_trashed;
  ustruct_find_prop_name_from_guid_fn_t                        find_prop_name_from_guid;
  ustruct_find_prop_guid_from_name_fn_t                        find_prop_guid_from_name;
  ustruct_are_prop_guids_available_fn_t                        are_prop_guids_available;

  uclass_get_authoritative_class_fn_t                          get_authoritative_class;
  uclass_post_init_instance_fn_t                               post_init_instance;
  uclass_init_props_from_custom_list_fn_t                      init_props_from_custom_list;
  uclass_setup_object_initializer_fn_t                         setup_object_initializer;
  uclass_get_persistent_uber_graph_frame_fn_t                  get_persistent_uber_graph_frame;
  uclass_create_persistent_uber_graph_frame_fn_t               create_persistent_uber_graph_frame;
  uclass_destroy_persistent_uber_graph_frame_fn_t              destroy_persistent_uber_graph_frame;
  uclass_serialize_default_object_fn_t                         serialize_default_object;
  uclass_serialize_default_object2_fn_t                        serialize_default_object2;
  uclass_post_load_default_object_fn_t                         post_load_default_object;
  uclass_purge_class_fn_t                                      purge_class;
  uclass_is_function_implemented_in_script_fn_t                is_function_implemented_in_script;
  uclass_has_prop_fn_t                                         has_prop;
  uclass_find_archetype_fn_t                                   find_archetype;
  uclass_get_archetype_for_cdo_fn_t                            get_archetype_for_cdo;
  uclass_get_archetype_for_sparse_class_data_fn_t              get_archetype_for_sparse_class_data;
  uclass_get_default_object_preload_deps_fn_t                  get_default_object_preload_deps;
  uclass_create_default_object_fn_t                            create_default_object;
};
MOD_STATIC_ASSERT(sizeof(uclass_vtable_t) == 0x3A8, "size mismatch");

typedef void              (*uclass_ctor_fn_t)(const fobject_initializer_t *);
typedef struct uobject_s *(*uclass_class_vtable_helper_ctor_caller_fn_t)(void *);
typedef void              (*uclass_class_add_referenced_objects_fn_t)(struct uobject_s *, void *);
typedef void              (*fnative_func_ptr_fn_t)(struct uobject_s *ctx, struct fframe_s *stack, void *const z_param_result);

typedef struct fnative_func_lookup_s fnative_func_lookup_t;
struct fnative_func_lookup_s {
  fname_t               name;
  fnative_func_ptr_fn_t pointer;
};

typedef struct fgc_ref_token_stream_s fgc_ref_token_stream_t;
struct fgc_ref_token_stream_s {
  TARRAY(uint32_t) tokens;
};

typedef TARRAY(struct ufield_s *)       tarray_ufieldptr_t;
typedef TARRAY(fnative_func_lookup_t)   tarray_fnative_func_lookup_t;
typedef TMAP(fname_t, struct ufunc_s *) tmap_fname_ufuncptr_t;

typedef struct uclass_s uclass_t;
struct uclass_s {
  /* uobject_t */
  uclass_vtable_t                            *vtable;
  eobj_flags_t                                obj_flags;
  int32_t                                     internal_idx;
  struct uclass_s                            *cls;
  fname_t                                     name;
  struct uobject_s                           *outer;
  /* ufield_t */
  struct ufield_s                            *next;
  /* ustruct_t */
  fstruct_base_chain_t                        chain;
  struct ustruct_s                           *super_struct;
  ufield_t                                   *children;
  struct ffield_s                            *child_props;
  int32_t                                     props_size;
  int32_t                                     min_alignment;
  ustruct_script_t                            script;
  struct fprop_s                             *prop_link;
  struct fprop_s                             *ref_link;
  struct fprop_s                             *dtor_link;
  struct fprop_s                             *post_ctor_link;
  tarray_uobjectptr_t                         script_and_prop_obj_refs;
  void                                       *unresolved_script_props;
  void                                       *unversioned_schema;
  /* uclass_t */
  uclass_ctor_fn_t                            class_ctor;
  uclass_class_vtable_helper_ctor_caller_fn_t class_vtable_helper_ctor_caller;
  uclass_class_add_referenced_objects_fn_t    class_add_referenced_objects;
  uint32_t                                    class_unique_and_cooked;
  uint32_t                                    class_flags;
  uint64_t                                    class_cast_flags;
  struct uclass_s                            *class_within;
  uobject_t                                  *class_generated_by;
  fname_t                                     class_config_name;
  tarray_void_t                               class_reps;
  tarray_ufieldptr_t                          net_fields;
  int32_t                                     first_owned_class_rep;
  uobject_t                                  *cdo;
  void                                       *sparse_class_data;
  struct uscript_struct_s                    *sparse_class_data_struct;
  tmap_fname_ufuncptr_t                       func_map;
  tmap_fname_ufuncptr_t                       super_func_map;
  frwlock_t                                   super_func_map_lock;
  tarray_void_t                               interfaces;
  fgc_ref_token_stream_t                      ref_token_stream;
  fcritical_section_t                         ref_token_stream_critical;
  tarray_fnative_func_lookup_t                native_func_lookup_table;
};

MOD_STATIC_ASSERT(offsetof(uclass_t, class_ctor)                      == 0x0B0, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, class_vtable_helper_ctor_caller) == 0x0B8, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, class_add_referenced_objects)    == 0x0C0, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, class_unique_and_cooked)         == 0x0C8, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, class_flags)                     == 0x0CC, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, class_cast_flags)                == 0x0D0, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, class_within)                    == 0x0D8, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, class_generated_by)              == 0x0E0, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, class_config_name)               == 0x0E8, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, class_reps)                      == 0x0F0, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, net_fields)                      == 0x100, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, first_owned_class_rep)           == 0x110, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, cdo)                             == 0x118, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, sparse_class_data)               == 0x120, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, sparse_class_data_struct)        == 0x128, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, func_map)                        == 0x130, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, super_func_map)                  == 0x180, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, super_func_map_lock)             == 0x1D0, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, interfaces)                      == 0x1D8, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, ref_token_stream)                == 0x1E8, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, ref_token_stream_critical)       == 0x1F8, "invalid offset");
MOD_STATIC_ASSERT(offsetof(uclass_t, native_func_lookup_table)        == 0x220, "invalid offset");
MOD_STATIC_ASSERT(sizeof(uclass_t)                                    == 0x230, "size mismatch");

struct udata_table_s;
struct ftable_row_base_s;

typedef TMAP(fname_t, uint8_t *) tmap_fname_uint8ptr_t;
typedef TARRAY(fstring_t)        tarray_fstring_t;

TMAP_DECLARE_FUNCS(unreal_tmap_fname_uint8ptr, tmap_fname_uint8ptr_t, fname_t, uint8_t *)

typedef void (__fastcall *ftable_row_base_destructor_fn_t)           (struct ftable_row_base_s *);
typedef void (__fastcall *ftable_row_base_on_post_data_import_fn_t)  (struct ftable_row_base_s *, struct udata_table_s *, fname_t, tarray_fstring_t *);
typedef void (__fastcall *ftable_row_base_on_data_table_changed_fn_t)(struct ftable_row_base_s *, struct udata_table_s *, fname_t);

typedef struct ftable_row_vtable_s ftable_row_vtable_t;
struct ftable_row_vtable_s {
  ftable_row_base_destructor_fn_t            destructor;
  ftable_row_base_on_post_data_import_fn_t   on_post_data_import;
  ftable_row_base_on_data_table_changed_fn_t on_data_table_changed;
};
MOD_STATIC_ASSERT(sizeof(ftable_row_vtable_t) == 0x18, "size mismatch");

typedef struct ftable_row_base_s ftable_row_base_t;
struct ftable_row_base_s {
  ftable_row_vtable_t *vtable;
};

typedef tmap_fname_uint8ptr_t       *(__fastcall *udata_table_get_non_const_row_map_fn_t)          (struct udata_table_s *);
typedef void                         (__fastcall *udata_table_add_row_internal_fn_t)               (struct udata_table_s *, fname_t, uint8_t *);
typedef const tmap_fname_uint8ptr_t *(__fastcall *udata_table_get_row_map_const_fn_t)              (struct udata_table_s *);
typedef const tmap_fname_uint8ptr_t *(__fastcall *udata_table_get_row_map_fn_t)                    (struct udata_table_s *);
typedef bool                         (__fastcall *udata_table_allow_duplicate_rows_on_import_fn_t) (struct udata_table_s *);
typedef void                         (__fastcall *udata_table_empty_table_fn_t)                    (struct udata_table_s *);
typedef void                         (__fastcall *udata_table_remove_row_fn_t)                     (struct udata_table_s *, fname_t);
typedef void                         (__fastcall *udata_table_add_row_fn_t)                        (struct udata_table_s *, fname_t, const ftable_row_base_t *);

typedef struct udata_table_vtable_s udata_table_vtable_t;
struct udata_table_vtable_s {
  uobject_destructor_fn_t                                      destructor;
  uobject_register_deps_fn_t                                   register_deps;
  uobject_deferred_register_fn_t                               deferred_register;
  uobject_can_be_cluster_root_fn_t                             can_be_cluster_root;
  uobject_can_be_in_cluster_fn_t                               can_be_in_cluster;
  uobject_create_cluster_fn_t                                  create_cluster;
  uobject_on_cluster_marked_as_pending_kill_fn_t               on_cluster_marked_as_pending_kill;
  uobject_get_detailed_info_internal_fn_t                      get_detailed_info_internal;
  uobject_post_init_props_fn_t                                 post_init_props;
  uobject_post_cdo_contruct_fn_t                               post_cdo_contruct;
  uobject_pre_save_root_fn_t                                   pre_save_root;
  uobject_post_save_root_fn_t                                  post_save_root;
  uobject_pre_save_fn_t                                        pre_save;
  uobject_is_ready_for_async_post_load_fn_t                    is_ready_for_async_post_load;
  uobject_post_load_fn_t                                       post_load;
  uobject_post_load_subobjects_fn_t                            post_load_subobjects;
  uobject_begin_destroy_fn_t                                   begin_destroy;
  uobject_is_ready_for_finish_destroy_fn_t                     is_ready_for_finish_destroy;
  uobject_finish_destroy_fn_t                                  finish_destroy;
  uobject_serialize_fn_t                                       serialize;
  uobject_serialize_farchive_fn_t                              serialize_farchive;
  uobject_shutdown_after_error_fn_t                            shutdown_after_error;
  uobject_post_interp_change_fn_t                              post_interp_change;
  uobject_post_rename_fn_t                                     post_rename;
  uobject_pre_duplicate_fn_t                                   pre_duplicate;
  uobject_post_duplicate_from_type_fn_t                        post_duplicate_from_type;
  uobject_post_duplicate_fn_t                                  post_duplicate;
  uobject_needs_load_for_client_fn_t                           needs_load_for_client;
  uobject_needs_load_for_server_fn_t                           needs_load_for_server;
  uobject_needs_load_for_target_platform_fn_t                  needs_load_for_target_platform;
  uobject_needs_load_for_editor_game_fn_t                      needs_load_for_editor_game;
  uobject_is_editor_only_fn_t                                  is_editor_only;
  uobject_has_non_editor_only_references_fn_t                  has_non_editor_only_references;
  uobject_is_post_load_thread_safe_fn_t                        is_post_load_thread_safe;
  uobject_is_destruction_thread_safe_fn_t                      is_destruction_thread_safe;
  uobject_get_preload_deps_fn_t                                get_preload_deps;
  uobject_get_prestream_packages_fn_t                          get_prestream_packages;
  uobject_export_custom_props_fn_t                             export_custom_props;
  uobject_import_custom_props_fn_t                             import_custom_props;
  uobject_post_edit_import_fn_t                                post_edit_import;
  uobject_post_reload_config_fn_t                              post_reload_config;
  uobject_rename_fn_t                                          rename;
  uobject_get_desc_fn_t                                        get_desc;
  uobject_get_sparse_class_data_struct_fn_t                    get_sparse_class_data_struct;
  uobject_get_world_fn_t                                       get_world;
  uobject_get_native_prop_vals_fn_t                            get_native_prop_vals;
  uobject_get_resource_size_ex_fn_t                            get_resource_size_ex;
  uobject_get_exporter_name_fn_t                               get_exporter_name;
  uobject_get_restore_for_uobject_overwrite_fn_t               get_restore_for_uobject_overwrite;
  uobject_are_native_props_identical_to_fn_t                   are_native_props_identical_to;
  uobject_get_asset_registry_tags_fn_t                         get_asset_registry_tags;
  uobject_is_asset_fn_t                                        is_asset;
  uobject_get_primary_asset_id_fn_t                            get_primary_asset_id;
  uobject_is_localized_resource_fn_t                           is_localized_resource;
  uobject_is_safe_for_root_set_fn_t                            is_safe_for_root_set;
  uobject_tag_subobjects_fn_t                                  tag_subobjects;
  uobject_get_lifetime_replicated_props_fn_t                   get_lifetime_replicated_props;
  uobject_is_name_stable_for_networking_fn_t                   is_name_stable_for_networking;
  uobject_is_full_name_stable_for_networking_fn_t              is_full_name_stable_for_networking;
  uobject_is_supported_for_networking_fn_t                     is_supported_for_networking;
  uobject_get_subobjects_with_stable_names_for_networking_fn_t get_subobjects_with_stable_names_for_networking;
  uobject_pre_net_receive_fn_t                                 pre_net_receive;
  uobject_post_net_receive_fn_t                                post_net_receive;
  uobject_post_rep_notifies_fn_t                               post_rep_notifies;
  uobject_pre_destroy_from_replication_fn_t                    pre_destroy_from_replication;
  uobject_build_subobject_mapping_fn_t                         build_subobject_mapping;
  uobject_get_config_override_platform_fn_t                    get_config_override_platform;
  uobject_override_per_object_config_section_fn_t              override_per_object_config_section;
  uobject_process_event_fn_t                                   process_event;
  uobject_get_function_callspace_fn_t                          get_function_callspace;
  uobject_call_remote_function_fn_t                            call_remote_function;
  uobject_process_console_exec_fn_t                            process_console_exec;
  uobject_regenerate_class_fn_t                                regenerate_class;
  uobject_mark_as_editor_only_subobject_fn_t                   mark_as_editor_only_subobject;
  uobject_check_default_subobjects_internal_fn_t               check_default_subobjects_internal;
  uobject_validate_generated_rep_enums_fn_t                    validate_generated_rep_enums;
  uobject_set_net_push_id_dynamic_fn_t                         set_net_push_id_dynamic;
  uobject_get_net_push_id_dynamic_fn_t                         get_net_push_id_dynamic;

  udata_table_get_non_const_row_map_fn_t                       get_non_const_row_map;
  udata_table_add_row_internal_fn_t                            add_row_internal;
  udata_table_get_row_map_const_fn_t                           get_row_map_const;
  udata_table_get_row_map_fn_t                                 get_row_map;
  udata_table_allow_duplicate_rows_on_import_fn_t              allow_duplicate_rows_on_import;
  udata_table_empty_table_fn_t                                 empty_table;
  udata_table_remove_row_fn_t                                  remove_row;
  udata_table_add_row_fn_t                                     add_row;
};
MOD_STATIC_ASSERT(sizeof(udata_table_vtable_t) == 0x2B0, "size mismatch");

typedef tmulticast_delegate_t fon_data_table_changed_t;

typedef struct udata_table_s udata_table_t;
struct udata_table_s {
  /* uobject_t */
  udata_table_vtable_t    *vtable;
  eobj_flags_t             obj_flags;
  int32_t                  internal_idx;
  struct uclass_s         *cls;
  fname_t                  name;
  struct uobject_s        *outer;
  /* udata_table_t */
  uscript_struct_t        *row_struct;
  tmap_fname_uint8ptr_t    row_map;
  uint8_t                  flags;
  fstring_t                import_key_field;
  fon_data_table_changed_t on_data_table_changed_delegate;
};

MOD_STATIC_ASSERT(offsetof(udata_table_t, row_struct)                     == 0x28, "invalid offset");
MOD_STATIC_ASSERT(offsetof(udata_table_t, row_map)                        == 0x30, "invalid offset");
MOD_STATIC_ASSERT(offsetof(udata_table_t, flags)                          == 0x80, "invalid offset");
MOD_STATIC_ASSERT(offsetof(udata_table_t, import_key_field)               == 0x88, "invalid offset");
MOD_STATIC_ASSERT(offsetof(udata_table_t, on_data_table_changed_delegate) == 0x98, "invalid offset");
MOD_STATIC_ASSERT(sizeof(udata_table_t)                                   == 0xB0, "size mismatch");

typedef struct {
  int32_t object_idx;
  int32_t object_serial;
} fweak_object_ptr_t;

MOD_STATIC_ASSERT(sizeof(fweak_object_ptr_t) == 0x08, "size mismatch");

typedef struct {
  fweak_object_ptr_t object;
  fname_t            function_name;
} fscript_delegate_t;

MOD_STATIC_ASSERT(sizeof(fscript_delegate_t) == 0x10, "size mismatch");

typedef struct {
  fscript_delegate_t *data;
  int32_t             num;
  int32_t             max;
} fmulticast_script_delegate_t;

MOD_STATIC_ASSERT(sizeof(fmulticast_script_delegate_t) == 0x10, "size mismatch");

typedef struct {
  fname_t tag_name;
} fgameplay_tag_t;

MOD_STATIC_ASSERT(sizeof(fgameplay_tag_t) == 0x08, "size mismatch");

typedef uint8_t elog_verbosity_type_t;
#define ELVT_NO_LOGGING     0x0
#define ELVT_FATAL          0x1
#define ELVT_ERROR          0x2
#define ELVT_WARNING        0x3
#define ELVT_DISPLAY        0x4
#define ELVT_LOG            0x5
#define ELVT_VERBOSE        0x6
#define ELVT_VERY_VERBOSE   0x7
#define ELVT_ALL            0x7
#define ELVT_NUM_VERBOSITY  0x8
#define ELVT_VERBOSITY_MASK 0xF
#define ELVT_SET_COLOR      0x40
#define ELVT_BREAK_ON_LOG   0x80

struct foutput_device_s;

typedef void (__fastcall *foutput_device_destructor_fn_t)                     (struct foutput_device_s *);
typedef void (__fastcall *foutput_device_serialize_time_fn_t)                 (struct foutput_device_s *, const wchar_t *, elog_verbosity_type_t, const fname_t *, const double);
typedef void (__fastcall *foutput_device_serialize_fn_t)                      (struct foutput_device_s *, const wchar_t *, elog_verbosity_type_t, const fname_t *);
typedef void (__fastcall *foutput_device_flush_fn_t)                          (struct foutput_device_s *);
typedef void (__fastcall *foutput_device_tear_down_fn_t)                      (struct foutput_device_s *);
typedef void (__fastcall *foutput_device_dump_fn_t)                           (struct foutput_device_s *, void *);
typedef bool (__fastcall *foutput_device_is_memory_only_fn_t)                 (struct foutput_device_s *);
typedef bool (__fastcall *foutput_device_can_be_used_on_any_thread_fn_t)      (struct foutput_device_s *);
typedef bool (__fastcall *foutput_device_can_be_used_on_multiple_threads_fn_t)(struct foutput_device_s *);

typedef struct foutput_device_vtable_s foutput_device_vtable_t;
struct foutput_device_vtable_s {
  foutput_device_destructor_fn_t                      destructor;
  foutput_device_serialize_time_fn_t                  serialize_time;
  foutput_device_serialize_fn_t                       serialize;
  foutput_device_flush_fn_t                           flush;
  foutput_device_tear_down_fn_t                       tear_down;
  foutput_device_dump_fn_t                            dump;
  foutput_device_is_memory_only_fn_t                  is_memory_only;
  foutput_device_can_be_used_on_any_thread_fn_t       can_be_used_on_any_thread;
  foutput_device_can_be_used_on_multiple_threads_fn_t can_be_used_on_multiple_threads;
};
MOD_STATIC_ASSERT(sizeof(foutput_device_vtable_t) == 0x48, "size mismatch");

typedef struct foutput_device_s foutput_device_t;
struct foutput_device_s {
  foutput_device_vtable_t *vftable;
  uint8_t                  suppress_event_tag;
  uint8_t                  auto_emit_line_terminator;
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
MOD_STATIC_ASSERT(offsetof(fframe_t, node) == 0x10, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fframe_t, obj) == 0x18, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fframe_t, code) == 0x20, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fframe_t, locals) == 0x28, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fframe_t, most_recent_prop) == 0x30, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fframe_t, most_recent_prop_addr) == 0x38, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fframe_t, flow_stack) == 0x40, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fframe_t, prev_frame) == 0x70, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fframe_t, out_params) == 0x78, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fframe_t, prop_chain_for_compiled_in) == 0x80, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fframe_t, current_native_func) == 0x88, "invalid offset");
MOD_STATIC_ASSERT(offsetof(fframe_t, array_ctx_failed) == 0x90, "invalid offset");

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
MOD_STATIC_ASSERT(sizeof(fuobject_array_t) == 0xB8, "size mismatch");

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

typedef struct uworld_s uworld_t;
struct uworld_s {
  /* uobject */
  uobject_vtable_t *vtable;
  eobj_flags_t      obj_flags;
  int32_t           internal_idx;
  struct uclass_s  *cls;
  fname_t           name;
  struct uobject_s *outer;
};

typedef struct fio_env_s fio_env_t;
struct fio_env_s {
  fstring_t path;
  int32_t   order;
};
MOD_STATIC_ASSERT(sizeof(fio_env_t) == 24, "size mismatch");

typedef struct faes_key_s faes_key_t;
struct faes_key_s {
  uint8_t key[32];
};
MOD_STATIC_ASSERT(sizeof(faes_key_t) == 32, "size mismatch");

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

/* Returns a static readable name for an IO error code. */
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
MOD_STATIC_ASSERT(sizeof(fio_status_t) == 260, "size mismatch");

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
MOD_ALIGNED_TYPE(struct, 16) ftransform_s
{
  fquat_t   rotation;
  fvector_t translation;
  float     _pad0[1];
  fvector_t scale3d;
  float     _pad1[1];
};
MOD_STATIC_ASSERT(sizeof(ftransform_t) == 48, "invalid size");

typedef struct begin_actor_spawn_s begin_actor_spawn_params_t;
struct begin_actor_spawn_s {
  uworld_t    *world_ctx_obj;
  uclass_t    *actor_class;
  ftransform_t spawn_transform;
  uint8_t      collision_handling_override;
  void        *owner;
  void        *return_value;
};
MOD_STATIC_ASSERT(offsetof(begin_actor_spawn_params_t, actor_class) == 8, "invalid offset");
MOD_STATIC_ASSERT(offsetof(begin_actor_spawn_params_t, spawn_transform) == 16, "invalid offset");
MOD_STATIC_ASSERT(offsetof(begin_actor_spawn_params_t, collision_handling_override) == 64, "invalid offset");
MOD_STATIC_ASSERT(offsetof(begin_actor_spawn_params_t, owner) == 72, "invalid offset");
MOD_STATIC_ASSERT(offsetof(begin_actor_spawn_params_t, return_value) == 80, "invalid offset");

typedef struct finish_actor_spawn_s finish_actor_spawn_params_t;
struct finish_actor_spawn_s {
  void        *actor;
  uint8_t      _pad0[8];
  ftransform_t spawn_transform;
  void        *return_value;
};
MOD_STATIC_ASSERT(offsetof(finish_actor_spawn_params_t, spawn_transform) == 16, "invalid offset");
MOD_STATIC_ASSERT(offsetof(finish_actor_spawn_params_t, return_value) == 64, "invalid offset");

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

typedef struct unreal_cached_objects_s unreal_cached_objects_t;
struct unreal_cached_objects_s {
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
};

extern unreal_cached_objects_t g_cached_objects;

/*
 * Scans the current UObject array and fills g_cached_objects with commonly used classes and functions.
 * Call it on initialization.
 * Helpers that use the cache assert when required entries are missing.
 */
void
unreal_cache_objects(void);

/* ===================================================== FNAME ====================================================== */

/* Returns the engine's live FName pool. */
fname_pool_t *
unreal_get_name_pool(void);

/* Compares FNames by comparison index and, unless ignore_num is set, by number. */
bool
unreal_fname_equal(fname_t a, fname_t b, bool ignore_num);
/* Compute FName hash. */
uint32_t
unreal_fname_hash(fname_t name);
/* Check if FName is None. */
bool
unreal_fname_is_none(fname_t name);
/* Returns the raw pool entry for a comparison index. The engine owns the returned memory. */
fname_entry_t *
unreal_fname_entry_get(uint32_t cmp_idx);
/* Returns the UTF-8 byte length of an FName, including its numeric suffix when present. */
uint64_t
unreal_fname_utf8_len(fname_t name);
/* Writes an FName as UTF-8 and returns the bytes written. No null terminator is added. */
uint64_t
unreal_fname_utf8_write(uint8_t *buf, uint64_t max_len, fname_t name);
/* Converts an FName to a null-terminated UTF-8 string allocated in an arena. */
str_t
unreal_fname_to_str(fname_t fname, mod_arena_t arena);
/*
 * Finds or creates an FName using the requested EFindName mode.
 * FNAME_FIND does not add missing names.
 * Modes that modify the pool should only be used where Unreal allows it.
 */
fname_t
unreal_fname_from_str(str_t s, efind_name_t find_type);

typedef bool (*fname_pool_iter_cb_t)(fname_t name, fname_entry_t *entry, void *user);
/*
 * Walks the current FName pool and calls cb for each entry.
 * Returning false stops the walk.
 */
void
unreal_fname_pool_iterate(fname_pool_iter_cb_t cb, void *user);

/*
 * Matches UTF-8 text against an FName entry by exact match or substring.
 * UTF-8 multibyte text is not decoded for wide entries.
 * Case insensitive comparison is ASCII-only.
 */
bool
unreal_fname_entry_match_text(fname_entry_t *entry, str_t text, bool ignore_case, bool exact_match);
/*
 * Matches UTF-16 text against an FName entry by exact match or substring.
 * Case insensitive comparison is ASCII-only.
 */
bool
unreal_fname_entry_match_text16(fname_entry_t *entry, str16_t text, bool ignore_case, bool exact_match);
/* Matches UTF-8 text against an FName's pool entry. The FName number suffix is ignored. */
bool
unreal_fname_match_text(fname_t name, str_t text, bool ignore_case, bool exact_match);
/* Matches UTF-16 text against an FName's pool entry. The FName number suffix is ignored. */
bool
unreal_fname_match_text16(fname_t name, str16_t text, bool ignore_case, bool exact_match);

/* ==================================================== FSTRING ===================================================== */

/*
 * Converts UTF-8 into arena-backed UTF-16 and wraps it as an FString view.
 * The memory is not owned by Unreal and must not be passed to code that frees or resizes the FString.
 */
fstring_t
unreal_fstring_from_str(str_t s, mod_arena_t arena);
/* Copies an FString to a null-terminated UTF-8 arena string. */
str_t
unreal_fstring_to_str(fstring_t fs, mod_arena_t arena);
/* Compute FString hash. */
uint32_t
unreal_fstring_hash(fstring_t str);
/* =================================================== FIOSTATUS ==================================================== */
/* Formats an I/O status as its message, numeric code, and readable code name. */
str_t
unreal_fio_status_to_str(fio_status_t status, mod_arena_t perm);

/* ==================================================== UOBJECT ===================================================== */

/* Returns the engine's live global UObject array, or NULL when unavailable. */
fuobject_array_t *
unreal_get_object_array(void);
/* Returns the current GWorld value. It may be NULL during startup, shutdown, or map changes. */
uworld_t *
unreal_get_current_world(void);

/* Registers a create or delete callback for this mod. Returns false when registration fails. */
bool
unreal_register_uobject_listener(uobject_listener_kind_t kind, uobject_notify_cb_t notify_cb, void *user);
/* Removes a listener matching the same kind, callback, and user pointer. */
void
unreal_deregister_uobject_listener(uobject_listener_kind_t kind, uobject_notify_cb_t notify_cb, void *user);

/* Returns a raw object array slot by index, even when the slot itself is not valid. */
fuobject_item_t *
unreal_uobject_array_get_item(int idx);
/* Checks that a slot has an object and is not marked unreachable or pending kill. */
bool
unreal_uobject_array_item_is_valid(fuobject_item_t *item);
/* Returns the valid UObject at an array index, or NULL. */
uobject_t *
unreal_uobject_array_get_obj(int idx);
/* Checks an object's class and every superclass against cls. */
bool
unreal_uobject_is_a(uobject_t *obj, uclass_t *cls);
/* Checks whether an object is a class default object or archetype object. */
bool
unreal_uobject_is_default(uobject_t *obj);
/*
 * Checks the object's array index, slot flags, and that the slot still points to the same object.
 * This is only a snapshot and does not keep the object alive after the call.
 */
bool
unreal_uobject_is_valid(uobject_t *obj);

/* Returns the UTF-8 byte length of an object's FName. */
uint64_t
unreal_uobject_get_name_len(uobject_t *obj);
/* Writes an object's FName as UTF-8 and returns the bytes written. No terminator is added. */
uint64_t
unreal_uobject_write_name(uobject_t *obj, uint8_t *dst, uint64_t cap);
/* Copies an object's name into a null-terminated arena string. */
str_t
unreal_uobject_push_name(uobject_t *obj, mod_arena_t arena);

/* Checks the object and each outer for a matching FName. */
bool
unreal_outer_chain_contains(uobject_t *obj, str_t str, bool ignore_case, bool exact_match);
/* Checks the class and each superclass for a matching FName. */
bool
unreal_super_chain_contains(uclass_t *cls, str_t str, bool ignore_case, bool exact_match);

/* Returns the byte length of the outer chain and object name joined with dots. */
uint64_t
unreal_uobject_get_full_name_len(uobject_t *obj);
/* Writes the outer chain from root to object with dot separators. No terminator is added. */
uint64_t
unreal_uobject_write_full_name(uobject_t *obj, uint8_t *dst, uint64_t cap);
/* Copies the outer chain from root to object into a null-terminated arena string. */
str_t
unreal_uobject_push_full_name(uobject_t *obj, mod_arena_t arena);

/* Returns the current number of UObject array elements, including empty or invalid slots. */
int
unreal_uobject_array_count(void);
/* Returns the maximum element capacity of the current UObject array. */
int
unreal_uobject_array_capacity(void);
/* Returns the number of currently allocated UObject chunks. */
int
unreal_uobject_array_num_chunks(void);
/* Returns the maximum UObject chunk count. */
int
unreal_uobject_array_max_chunks(void);

/* Returns the first object index considered by garbage collection. */
int
unreal_uobject_array_first_gc_index(void);
/* Returns the last object index excluded from normal garbage collection. */
int
unreal_uobject_array_last_non_gc_index(void);
/* Returns whether Unreal is still accepting objects into the disregard-for-GC range. */
bool
unreal_uobject_array_is_open_for_disregard_for_gc(void);

/* Scans valid objects and returns the first whose FName matches. The number suffix is ignored. */
uobject_t *
unreal_uobject_find(str_t name, bool ignore_case, bool exact_match);
/* Scans valid UClass objects and returns the first name match. unreal_cache_objects must have run first. */
uclass_t *
unreal_uclass_find(str_t name, bool ignore_case, bool exact_match);
/* Returns the first non-default object that is an instance of cls or one of its subclasses. */
uobject_t *
unreal_uobject_find_first_of(uclass_t *cls);
/* Finds a UClass by an exact, case-sensitive dotted outer-chain name. */
uclass_t *
unreal_uobject_find_class_by_full_name(str_t full_name);
/* Finds an object by exact, case-sensitive full name and optional class filter. */
uobject_t *
unreal_uobject_find_by_full_name(uclass_t *cls, str_t full_name);

/*
 * Calls Unreal's ProcessEvent for self and func.
 * params must match the function's parameter memory layout, including return and out fields.
 * Call this only from game thread.
 */
void
unreal_process_event(uobject_t *self, ufunc_t *func, void *params);

/*
 * Spawns an actor through GameplayStatics using deferred begin and finish calls.
 * It uses an identity transform, always-spawn collision handling, and no owner.
 * unreal_cache_objects must have run first.
 */
uobject_t *
unreal_spawn_actor(uobject_t *world_ctx_obj, uclass_t *cls);
/* Calls K2_DestroyActor on an actor. unreal_cache_objects must have run first. */
void
unreal_despawn_actor(uobject_t *actor);
/* Loads a UObject through the host's Unreal loader and returns NULL on failure. */
uobject_t *
unreal_static_load_object(uclass_t *obj_cls, uobject_t *outer, str_t name, str_t filename, uint32_t load_flags, void *sandbox, bool allow_obj_reconcile, void *instancing_ctx);
/* Loads a UClass through the host's Unreal loader and returns NULL on failure. */
uclass_t *
unreal_static_load_class(uclass_t *base_cls, uobject_t *outer, str_t name, str_t filename, uint32_t load_flags);

/* Mounts a pak file using the supplied engine mount order. */
bool
unreal_mount_pak(str_t file_path, int order);
/* Mounts an IO Store container using the supplied engine mount order. */
bool
unreal_mount_iostore(str_t file_path, int order);

/* Returns the native bytecode handler registered for an opcode, or NULL. */
fnative_func_ptr_t
unreal_get_native(uint8_t opcode);
/*
 * Reads one opcode from stack->code, advances the code pointer, and calls its native handler.
 * Returns false when no handler exists.
 * The opcode byte remains consumed in that case.
 */
bool
unreal_fframe_step(fframe_t *stack, void *result);

/* ================================================ CLASS INTROSPECTION ============================================= */
/* Searches s->prop_link for an exact, case-sensitive property name. */
fprop_t *
unreal_ustruct_find_prop(ustruct_t *s, str_t name);
/* Searches UField children on a struct and its super structs for an exact function name. */
ufunc_t *
unreal_ustruct_find_func(ustruct_t *s, str_t name);

/* Searches UField children on a struct and its super structs for an FName. */
ufunc_t *
unreal_ustruct_find_func_fname(ustruct_t *s, fname_t name, bool ignore_num);

/* Returns the raw sparse multicast delegate storage for an owner and delegate name. */
void *
unreal_get_mcast_sparse_delegate(uobject_t *s, fname_t name);

/* ================================================= PROPERTY ACCESS ================================================ */
/* Returns a raw property address by adding offset_internal to a container pointer. No checks are made. */
static inline void *
unreal_uprop_ptr(fprop_t *prop, void *container)
{
  return (uint8_t *)container + prop->offset_internal;
}

/* Reads a packed Unreal bool property directly from container memory. */
static inline bool
unreal_uprop_get_bool(fprop_bool_t *prop, void *container)
{
  uint8_t *byte = (uint8_t *)container + prop->base.offset_internal + prop->byte_offset;
  return (*byte & prop->field_mask) != 0;
}

/* Writes a packed Unreal bool property directly without calling engine setters or notifications. */
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

/* Reads an FName property directly from container memory. */
static inline fname_t
unreal_uprop_get_fname(fprop_t *prop, void *container)
{
  return *(fname_t *)unreal_uprop_ptr(prop, container);
}

/* Writes an FName property directly without calling engine setters or notifications. */
static inline void
unreal_uprop_set_fname(fprop_t *prop, void *container, fname_t val)
{
  *(fname_t *)unreal_uprop_ptr(prop, container) = val;
}

/* Returns a shallow copy of an FString property. Its data remains owned by the engine object. */
static inline fstring_t
unreal_uprop_get_fstr(fprop_t *prop, void *container)
{
  return *(fstring_t *)unreal_uprop_ptr(prop, container);
}

/* Reads a UObject property pointer directly from container memory. */
static inline uobject_t *
unreal_uprop_get_obj(fprop_t *prop, void *container)
{
  return *(uobject_t **)unreal_uprop_ptr(prop, container);
}

/* Writes a UObject property pointer directly without validation or engine notifications. */
static inline void
unreal_uprop_set_obj(fprop_t *prop, void *container, uobject_t *val)
{
  *(uobject_t **)unreal_uprop_ptr(prop, container) = val;
}

/* =============================================== CONTAINER UTILITIES ============================================== */

/* Returns true when idx is an allocated/set bit in the bit array. */
bool
unreal_tbit_array_is_set(tbit_array_t *bits, int32_t idx);
/* Returns the first set bit at or after start_idx, or TSET_INVALID_ID when there is none. */
int32_t
unreal_tbit_array_find_next_set(tbit_array_t *bits, int32_t start_idx);
/* Returns the first TSet element id in the bucket for key_hash, or TSET_INVALID_ID. */
int32_t
unreal_tset_hash_head(hash_allocator_t *hash, int32_t hash_size, uint32_t key_hash);

/* Returns false when the connected host predates the reflected map helpers. */
bool
unreal_map_helpers_available(void);
void
unreal_map_add(void *map, fprop_map_t *prop, const void *key, const void *val);
bool
unreal_map_remove(void *map, fprop_map_t *prop, const void *key);
bool
unreal_map_find(void *map, fprop_map_t *prop, const void *key, void *out_val);

/* =================================================== DATA TABLE =================================================== */

tmap_fname_uint8ptr_t *
unreal_udata_table_get_row_map(udata_table_t *table);
void
unreal_udata_table_add_row(udata_table_t *table, fname_t row_name, const ftable_row_base_t *row_data);
void
unreal_udata_table_remove_row(udata_table_t *table, fname_t row_name);
void
unreal_udata_table_empty(udata_table_t *table);
uint8_t *
unreal_udata_table_find_row(udata_table_t *table, fname_t row_name);

#ifdef __cplusplus
}
#endif

#endif /* UE_TYPES_H */
