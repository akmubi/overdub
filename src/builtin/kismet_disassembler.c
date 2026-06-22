#include "uobject_search_internal.h"

#include "arena.h"
#include "log.h"
#include "scratch.h"
#include "str.h"
#include "unreal.h"

#include "vendor_stb.h"

#define INDENT_STR STR_LIT("  ")

#define SCRIPT_64KB_BYTECODE_LIMIT 0

#if SCRIPT_64KB_BYTECODE_LIMIT
typedef uint16_t code_skip_size_t;
#else
typedef uint32_t code_skip_size_t;
#endif

typedef enum {
  EBTL_EMPTY,
  EBTL_LOCALIZED_TEXT,
  EBTL_INVARIANT_TEXT,
  EBTL_LITERAL_STRING,
  EBTL_STRING_TABLE_ENTRY,
} eblueprint_text_literal_kind_t;

typedef enum {
  EST_CLASS = 0,
  EST_CLASS_SCOPE,
  EST_INSTANCE,
  EST_EVENT,
  EST_INLINE_EVENT,
  EST_RESUME_EVENT,
  EST_PURE_NODE_ENTRY,
  EST_NODE_DEBUG_SITE,
  EST_NODE_ENTRY,
  EST_NODE_EXIT,
  EST_PUSH_STATE,
  EST_RESTORE_STATE,
  EST_RESET_STATE,
  EST_SUSPEND_STATE,
  EST_POP_STATE,
  EST_TUNNEL_END_OF_THREAD,
  EST_STOP
} escript_instrumentation_kind_t;

static void
set_error_msg(disassember_ctx_t *ctx, const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  stbsp_vsnprintf(ctx->error_msg, sizeof(ctx->error_msg), fmt, va);
  va_end(va);
  ctx->failed = true;
}

static inline void
add_indent(disassember_ctx_t *ctx)
{
  ctx->depth += 1;
}

static inline void
drop_indent(disassember_ctx_t *ctx)
{
  if (ctx->depth > 0) {
    ctx->depth -= 1;
  }
}

static bool
can_read(disassember_ctx_t *ctx, uint32_t size)
{
  if (!ctx || ctx->failed) {
    return false;
  }

  if (ctx->script.num < 0) {
    set_error_msg(ctx, "negative script size");
    return false;
  }

  uint32_t num = (uint32_t)ctx->script.num;
  if (ctx->pos > num || size > num - ctx->pos) {
    set_error_msg(ctx, "bytecode read past end: pos: %u need: %u size: %u", ctx->pos, size, num);
    return false;
  }

  return true;
}

static bool
read_bytes(disassember_ctx_t *ctx, void *dst, uint32_t size)
{
  if (!can_read(ctx, size)) {
    if (dst && size) {
      mem_zero(dst, size);
    }
    return false;
  }

  mem_copy(dst, ctx->script.data + ctx->pos, size);
  ctx->pos += size;
  return true;
}

static uint8_t
read_byte(disassember_ctx_t *ctx)
{
  uint8_t v = 0;
  read_bytes(ctx, &v, sizeof(v));
  return v;
}

static uint16_t
read_word(disassember_ctx_t *ctx)
{
  uint8_t b[2] = {0};
  read_bytes(ctx, b, sizeof(b));
  return (uint16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
}

static int32_t
read_int(disassember_ctx_t *ctx)
{
  uint8_t b[4] = {0};
  read_bytes(ctx, b, sizeof(b));

  uint32_t u = ((uint32_t)b[0] <<  0) |
               ((uint32_t)b[1] <<  8) |
               ((uint32_t)b[2] << 16) |
               ((uint32_t)b[3] << 24);

  return (int32_t)u;
}

static uint64_t
read_qword(disassember_ctx_t *ctx)
{
  uint8_t b[8] = {0};
  read_bytes(ctx, b, sizeof(b));

  return ((uint64_t)b[0] <<  0) |
         ((uint64_t)b[1] <<  8) |
         ((uint64_t)b[2] << 16) |
         ((uint64_t)b[3] << 24) |
         ((uint64_t)b[4] << 32) |
         ((uint64_t)b[5] << 40) |
         ((uint64_t)b[6] << 48) |
         ((uint64_t)b[7] << 56);
}

static float
read_float(disassember_ctx_t *ctx)
{
  union {
    float   f;
    int32_t i;
  } result = {0};

  result.i = read_int(ctx);
  return result.f;
}

static void *
read_pointer(disassember_ctx_t *ctx)
{
  return (void *)read_qword(ctx);
}

static code_skip_size_t
read_skip_count(disassember_ctx_t *ctx)
{
#if SCRIPT_64KB_BYTECODE_LIMIT
  return read_word(ctx);
#else
  return read_int(ctx);
#endif
}

static str_t
read_name(disassember_ctx_t *ctx, arena_t *arena)
{
  fscript_name_t val = {0};

  if (!read_bytes(ctx, &val, sizeof(val))) {
    return STR_LIT("<bad-name>");
  }

  fname_t name = {
    .cmp_idx = val.cmp_idx,
    .num     = val.num,
  };

  str_t result = unreal_fname_to_str(name, arena);
  if (str_is_empty(result)) {
    result = STR_LIT("<bad-name>");
  }

  return result;
}

static bool
read_string8(disassember_ctx_t *ctx, str_t *out, arena_t *arena)
{
  ASSERT(out != NULL);
  *out = STR_NULL;

  if (ctx->script.num < 0) {
    set_error_msg(ctx, "negative script size");
    return false;
  }

  uint32_t num   = (uint32_t)ctx->script.num;
  uint32_t start = ctx->pos;
  uint32_t end   = start;

  while (end < num && ctx->script.data[end] != 0) {
    end += 1;
  }

  if (end >= num) {
    set_error_msg(ctx, "unterminated ansi string at offset 0x%08X", start);
    return false;
  }

  uint32_t len  = end - start;
  uint8_t *data = ARENA_PUSH_ARRAY_ZERO(arena, uint8_t, (uint64_t)len + 1);
  if (data) {
    if (len > 0) {
      mem_copy(data, ctx->script.data + start, len);
    }

    ctx->pos = end + 1; // consume terminator
    *out     = str_make(data, len);
  }
  return true;
}

static bool
read_string16(disassember_ctx_t *ctx, str_t *out, arena_t *arena)
{
  ASSERT(out != NULL);
  *out = STR_NULL;

  if (ctx->script.num < 0) {
    set_error_msg(ctx, "negative script size");
    return false;
  }

  uint32_t num   = (uint32_t)ctx->script.num;
  uint32_t start = ctx->pos;
  uint32_t end   = start;

  bool found_nul = false;
  while (end + 1 < num) {
    uint16_t ch  = (uint16_t)ctx->script.data[end + 0] | (uint16_t)(ctx->script.data[end + 1] << 8);
    end         += 2;

    if (ch == 0) {
      found_nul = true;
      break;
    }
  }

  if (!found_nul) {
    set_error_msg(ctx, "unterminated unicode string at offset 0x%08X", start);
    return false;
  }

  uint32_t byte_len = (end - start) - 2; // exclude terminator
  uint32_t len16    = byte_len / 2;

  str_t       result = STR_NULL;
  tmp_arena_t tmp    = scratch_begin(arena);
  {
    uint16_t *data = ARENA_PUSH_ARRAY(tmp.arena, uint16_t, len16 + 1);
    if (data) {
      for (uint32_t i = 0; i < len16; ++i) {
        uint32_t p = start + i * 2;
        data[i]    = (uint16_t)ctx->script.data[p + 0] | (uint16_t)(ctx->script.data[p + 1] << 8);
      }
      data[len16] = 0;

      result = str_from_str16(arena, str16_make(data, len16));
    }
  }
  scratch_end(tmp);

  ctx->pos = end;
  *out     = result;
  return true;
}

static bool
read_string(disassember_ctx_t *ctx, str_t *out, arena_t *arena)
{
  ASSERT(out != NULL);
  *out = STR_NULL;

  if (!can_read(ctx, 1)) {
    return false;
  }

  expr_token_t opcode = (expr_token_t)read_byte(ctx);
  switch (opcode) {
  case EX_STRING_CONST: {
    return read_string8(ctx, out, arena);
  }

  case EX_UNICODE_STRING_CONST: {
    return read_string16(ctx, out, arena);
  }

  default: {
    set_error_msg(ctx, "read_string: unexpected opcode. Expected %02X or %02X, got %02X", EX_STRING_CONST, EX_UNICODE_STRING_CONST, opcode);
    return false;
  }
  }
}

static str_t
get_name_safe(ffield_t *field, arena_t *arena)
{
  if (field) {
    return unreal_fname_to_str(field->name, arena);
  }
  return STR_LIT("none");
}

static void
push_line_vargs(disassember_ctx_t *ctx, const char *fmt, va_list va)
{
  va_list va2;
  va_copy(va2, va);
  int needed = stbsp_vsnprintf(NULL, 0, fmt, va2);
  va_end(va2);

  if (needed <= 0) {
    return;
  }

  str_t    indent_str = INDENT_STR;
  uint64_t indent_len = indent_str.len * ctx->depth;

  uint64_t len  = indent_len + (uint64_t)needed;
  uint8_t *data = ARENA_PUSH_ARRAY(ctx->arena, uint8_t, len + 1);
  if (!data) {
    return;
  }

  for (uint64_t i = 0; i < indent_len; i += indent_str.len) {
    mem_copy(data + i, indent_str.data, indent_str.len);
  }

  stbsp_vsnprintf((char *)(data + indent_len), (int)(needed + 1), fmt, va);

  str_list_push(ctx->arena, &ctx->list, str_make(data, len));
}

static void ATTR_FORMAT(2, 3)
push_line(disassember_ctx_t *ctx, const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  push_line_vargs(ctx, fmt, va);
  va_end(va);
}

static expr_token_t
serialize_expr(disassember_ctx_t *ctx);
static bool
serialize_until(disassember_ctx_t *ctx, expr_token_t end);
static void
process_cast_byte(disassember_ctx_t *ctx, int32_t cast_type);
static void
process_common(disassember_ctx_t *ctx, expr_token_t opcode);

str_list_t
unreal_disassemble_structure(ustruct_t *s, arena_t *arena)
{
  ASSERT(s != NULL);
  if (!s || !s->script.data || s->script.num <= 0) {
    return (str_list_t){0};
  }

  disassember_ctx_t ctx = {
    .script = s->script,
    .pos    = 0,
    .arena  = arena,
    .list   = {0},
    .depth  = 0,
    .failed = false,
  };

  tmp_arena_t tmp = scratch_begin(arena);
  {
    uint32_t num = (uint32_t)s->script.num;
    while (!ctx.failed && ctx.pos < num) {
      push_line(&ctx, "0x%08X:", ctx.pos);

      add_indent(&ctx);
      serialize_expr(&ctx);
      drop_indent(&ctx);
    }

    if (ctx.failed) {
      push_line(&ctx, "ERROR: %s", ctx.error_msg);
    }
  }
  scratch_end(tmp);

  return ctx.list;
}

static expr_token_t
serialize_expr(disassember_ctx_t *ctx)
{
  if (!can_read(ctx, 1)) {
    return EX_MAX;
  }

  expr_token_t opcode = (expr_token_t)read_byte(ctx);

  add_indent(ctx);
  process_common(ctx, opcode);
  drop_indent(ctx);

  return opcode;
}

bool
serialize_until(disassember_ctx_t *ctx, expr_token_t end)
{
  while (!ctx->failed && ctx->pos < (uint32_t)ctx->script.num) {
    if (serialize_expr(ctx) == end) {
      return true;
    }
  }

  if (!ctx->failed) {
    set_error_msg(ctx, "missing terminator opcode 0x%02X", end);
  }

  return false;
}

void
process_cast_byte(disassember_ctx_t *ctx, int32_t cast_type)
{
  (void)cast_type;
  /* expression of cast */
  serialize_expr(ctx);
}

void
process_common(disassember_ctx_t *ctx, expr_token_t opcode)
{
  if (ctx->failed) {
    return;
  }

  switch (opcode) {
  case EX_PRIMITIVE_CAST: {
    uint8_t conversion_type = read_byte(ctx);
    push_line(ctx, "$%02X: PrimitiveCast of type %02X", opcode, conversion_type);
    add_indent(ctx);

    push_line(ctx, "argument:");
    process_cast_byte(ctx, conversion_type);
    drop_indent(ctx);
    break;
  }

  case EX_SET_SET: {
    push_line(ctx, "$%02X: set set", opcode);
    serialize_expr(ctx);
    read_int(ctx);
    serialize_until(ctx, EX_END_SET);
    break;
  }

  case EX_END_SET: {
    push_line(ctx, "$%02X: EX_END_SET", opcode);
    break;
  }

  case EX_SET_CONST: {
    fprop_t *inner_prop = (fprop_t *)read_pointer(ctx);
    int32_t  num        = read_int(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t name = get_name_safe((ffield_t *)inner_prop, tmp.arena);
      push_line(ctx, "$%02X: set set const - elements number: %d, inner property: %.*s", opcode, num, STR_ARG(name));
    }
    scratch_end(tmp);

    serialize_until(ctx, EX_END_SET_CONST);
    break;
  }

  case EX_END_SET_CONST: {
    push_line(ctx, "$%02X: EX_END_SET_CONST", opcode);
    break;
  }

  case EX_SET_MAP: {
    push_line(ctx, "$%02X: set map", opcode);
    serialize_expr(ctx);
    read_int(ctx);
    serialize_until(ctx, EX_END_MAP);
    break;
  }

  case EX_END_MAP: {
    push_line(ctx, "$%02X: EX_END_MAP", opcode);
    break;
  }

  case EX_MAP_CONST: {
    fprop_t *key_prop = read_pointer(ctx);
    fprop_t *val_prop = read_pointer(ctx);
    int32_t  num      = read_int(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t key_name = get_name_safe((ffield_t *)key_prop, tmp.arena);
      str_t val_name = get_name_safe((ffield_t *)val_prop, tmp.arena);
      push_line(ctx, "$%02X: set map const - elements number: %d, key property: %.*s, val property: %.*s", opcode, num, STR_ARG(key_name), STR_ARG(val_name));
    }
    scratch_end(tmp);

    serialize_until(ctx, EX_END_MAP_CONST);
    break;
  }

  case EX_END_MAP_CONST: {
    push_line(ctx, "$%02X: EX_END_MAP_CONST", opcode);
    break;
  }

  case EX_OBJ_TO_INTERFACE_CAST: {
    uclass_t *interface_class = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t cls_name = unreal_uobject_push_name((uobject_t *)interface_class, tmp.arena);
      push_line(ctx, "$%02X: ObjToInterfaceCast to %.*s", opcode, STR_ARG(cls_name));
    }
    scratch_end(tmp);

    serialize_expr(ctx);
    break;
  }

  case EX_CROSS_INTERFACE_CAST: {
    uclass_t *interface_class = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t cls_name = unreal_uobject_push_name((uobject_t *)interface_class, tmp.arena);
      push_line(ctx, "$%02X: InterfaceToInterfaceCast to %.*s", opcode, STR_ARG(cls_name));
    }
    scratch_end(tmp);

    serialize_expr(ctx);
    break;
  }

  case EX_INTERFACE_TO_OBJ_CAST: {
    uclass_t *obj_class = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t cls_name = unreal_uobject_push_name((uobject_t *)obj_class, tmp.arena);
      push_line(ctx, "$%02X: InterfaceToObjCast to %.*s", opcode, STR_ARG(cls_name));
    }
    scratch_end(tmp);

    serialize_expr(ctx);
    break;
  }

  case EX_LET: {
    push_line(ctx, "$%02X: Let (Variable = Expression)", opcode);
    add_indent(ctx);
    read_pointer(ctx);

    push_line(ctx, "Variable:");
    serialize_expr(ctx);

    push_line(ctx, "Expression:");
    serialize_expr(ctx);

    drop_indent(ctx);
    break;
  }

  case EX_LET_OBJ:
  case EX_LET_WEAK_OBJ_PTR: {
    if (opcode == EX_LET_OBJ) {
      push_line(ctx, "$%02X: Let Obj (Variable = Expression)", opcode);
    } else {
      push_line(ctx, "$%02X: Let WeakObjPtr (Variable = Expression)", opcode);
    }
    add_indent(ctx);

    push_line(ctx, "Variable:");
    serialize_expr(ctx);

    push_line(ctx, "Expression:");
    serialize_expr(ctx);

    drop_indent(ctx);

    break;
  }

  case EX_LET_BOOL: {
    push_line(ctx, "$%02X: Let Bool (Variable = Expression)", opcode);
    add_indent(ctx);

    push_line(ctx, "Variable:");
    serialize_expr(ctx);

    push_line(ctx, "Expression:");
    serialize_expr(ctx);

    drop_indent(ctx);
    break;
  }

  case EX_LET_VALUE_ON_PERSISTENT_FRAME: {
    push_line(ctx, "$%02X: Let ValueOnPersistentFrame", opcode);
    add_indent(ctx);

    fprop_t *prop = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t prop_name = get_name_safe((ffield_t *)prop, tmp.arena);
      push_line(ctx, "Destination variable: %.*s, offset: %d", STR_ARG(prop_name), (prop) ? prop->offset_internal : 0);
    }
    scratch_end(tmp);

    push_line(ctx, "Expression:");
    serialize_expr(ctx);

    drop_indent(ctx);
    break;
  }

  case EX_STRUCT_MEMBER_CONTEXT: {
    push_line(ctx, "$%02X: Struct member context", opcode);
    add_indent(ctx);

    fprop_t *prop = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t name = get_name_safe((ffield_t *)prop, tmp.arena);
      push_line(ctx, "Member named %.*s @ offset %d", STR_ARG(name), (prop) ? prop->offset_internal : 0);
    }
    scratch_end(tmp);

    push_line(ctx, "Expression to struct:");
    serialize_expr(ctx);

    drop_indent(ctx);
    break;
  }

  case EX_LET_DELEGATE: {
    push_line(ctx, "$%02X: Let Delegate (Variable = Expression)", opcode);
    add_indent(ctx);

    push_line(ctx, "Variable:");
    serialize_expr(ctx);

    push_line(ctx, "Expression:");
    serialize_expr(ctx);

    drop_indent(ctx);
    break;
  }

  case EX_LOCAL_VIRTUAL_FUNCTION: {
    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t func_name = read_name(ctx, tmp.arena);
      push_line(ctx, "$%02X: Local Virtual Script Function named %.*s", opcode, STR_ARG(func_name));
    }
    scratch_end(tmp);

    serialize_until(ctx, EX_END_FUNCTION_PARMS);
    break;
  }

  case EX_LOCAL_FINAL_FUNCTION: {
    ustruct_t *stack_node = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t outer_name = STR_LIT("(null)");
      str_t name       = STR_LIT("(null)");
      if (stack_node) {
        if (stack_node->base.base.outer) {
          outer_name = unreal_fname_to_str(stack_node->base.base.outer->name, tmp.arena);
        }
        name = unreal_fname_to_str(stack_node->base.base.name, tmp.arena);
      }
      push_line(ctx, "$%02X: Local Final Script Function (stack node named %.*s::%.*s)", opcode, STR_ARG(outer_name), STR_ARG(name));
    }
    scratch_end(tmp);

    serialize_until(ctx, EX_END_FUNCTION_PARMS);
    break;
  }

  case EX_LET_MULTICAST_DELEGATE: {
    push_line(ctx, "$%02X: Let MulticastDelegate (Variable = Expression)", opcode);
    add_indent(ctx);

    push_line(ctx, "Variable:");
    serialize_expr(ctx);

    push_line(ctx, "Expression:");
    serialize_expr(ctx);

    drop_indent(ctx);
    break;
  }

  case EX_COMPUTED_JUMP: {
    push_line(ctx, "$%02X: Computed Jump, offset specified by expression:", opcode);

    add_indent(ctx);
    serialize_expr(ctx);
    drop_indent(ctx);
    break;
  }

  case EX_JUMP: {
    code_skip_size_t skip_count = read_skip_count(ctx);
    push_line(ctx, "$%02X: Jump to offset 0x%08X", opcode, skip_count);
    break;
  }

  case EX_LOCAL_VARIABLE: {
    fprop_t *prop = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t name = STR_LIT("(null)");
      if (prop) {
        name = unreal_fname_to_str(prop->base.name, tmp.arena);
      }
      push_line(ctx, "$%02X: Local variable named %.*s", opcode, STR_ARG(name));
    }
    scratch_end(tmp);
    break;
  }

  case EX_DEFAULT_VARIABLE: {
    fprop_t *prop = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t name = STR_LIT("(null)");
      if (prop) {
        name = unreal_fname_to_str(prop->base.name, tmp.arena);
      }
      push_line(ctx, "$%02X: Default variable named %.*s", opcode, STR_ARG(name));
    }
    scratch_end(tmp);
    break;
  }

  case EX_INSTANCE_VARIABLE: {
    fprop_t *prop = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t name = STR_LIT("(null)");
      if (prop) {
        name = unreal_fname_to_str(prop->base.name, tmp.arena);
      }
      push_line(ctx, "$%02X: Instance variable named %.*s", opcode, STR_ARG(name));
    }
    scratch_end(tmp);
    break;
  }

  case EX_LOCAL_OUT_VARIABLE: {
    fprop_t *prop = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t name = STR_LIT("(null)");
      if (prop) {
        name = unreal_fname_to_str(prop->base.name, tmp.arena);
      }
      push_line(ctx, "$%02X: Local out variable named %.*s", opcode, STR_ARG(name));
    }
    scratch_end(tmp);
    break;
  }

  case EX_CLASS_SPARSE_DATA_VARIABLE: {
    fprop_t *prop = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t name = STR_LIT("(null)");
      if (prop) {
        name = unreal_fname_to_str(prop->base.name, tmp.arena);
      }
      push_line(ctx, "$%02X: Class sparse data variable named %.*s", opcode, STR_ARG(name));
    }
    scratch_end(tmp);
    break;
  }

  case EX_INTERFACE_CONTEXT: {
    push_line(ctx, "$%02X: EX_INTERFACE_CONTEXT:", opcode);
    serialize_expr(ctx);
    break;
  }

  case EX_DEPRECATED_OP_4A: {
    push_line(ctx, "$%02X: this opcode has been removed and does nothing.", opcode);
    break;
  }

  case EX_NOTHING: {
    push_line(ctx, "$%02X: EX_NOTHING", opcode);
    break;
  }

  case EX_END_OF_SCRIPT: {
    push_line(ctx, "$%02X: EX_END_OF_SCRIPT", opcode);
    break;
  }

  case EX_END_FUNCTION_PARMS: {
    push_line(ctx, "$%02X: EX_END_FUNCTION_PARMS", opcode);
    break;
  }

  case EX_END_STRUCT_CONST: {
    push_line(ctx, "$%02X: EX_END_STRUCT_CONST", opcode);
    break;
  }

  case EX_END_ARRAY: {
    push_line(ctx, "$%02X: EX_END_ARRAY", opcode);
    break;
  }

  case EX_END_ARRAY_CONST: {
    push_line(ctx, "$%02X: EX_END_ARRAY_CONST", opcode);
    break;
  }

  case EX_INT_ZERO: {
    push_line(ctx, "$%02X: EX_INT_ZERO", opcode);
    break;
  }

  case EX_INT_ONE: {
    push_line(ctx, "$%02X: EX_INT_ONE", opcode);
    break;
  }

  case EX_TRUE: {
    push_line(ctx, "$%02X: EX_TRUE", opcode);
    break;
  }

  case EX_FALSE: {
    push_line(ctx, "$%02X: EX_FALSE", opcode);
    break;
  }

  case EX_NO_OBJECT: {
    push_line(ctx, "$%02X: EX_NO_OBJECT", opcode);
    break;
  }

  case EX_NO_INTERFACE: {
    push_line(ctx, "$%02X: EX_NO_INTERFACE", opcode);
    break;
  }

  case EX_SELF: {
    push_line(ctx, "$%02X: EX_SELF", opcode);
    break;
  }

  case EX_END_PARM_VALUE: {
    push_line(ctx, "$%02X: EX_END_PARM_VALUE", opcode);
    break;
  }

  case EX_RETURN: {
    push_line(ctx, "$%02X: Return expression", opcode);
    serialize_expr(ctx);
    break;
  }

  case EX_CALL_MATH: {
    ustruct_t *stack_node = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t outer_name = STR_LIT("None");
      str_t name       = STR_LIT("None");
      if (stack_node) {
        if (stack_node->base.base.outer) {
          outer_name = unreal_fname_to_str(stack_node->base.base.outer->name, tmp.arena);
        }
        name = unreal_fname_to_str(stack_node->base.base.name, tmp.arena);
      }
      push_line(ctx, "$%02X: Call Math (stack node %.*s::%.*s)", opcode, STR_ARG(outer_name), STR_ARG(name));
    }
    scratch_end(tmp);

    serialize_until(ctx, EX_END_FUNCTION_PARMS);
    break;
  }

  case EX_FINAL_FUNCTION: {
    ustruct_t *stack_node = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t outer_name = STR_LIT("(null)");
      str_t name       = STR_LIT("(null)");
      if (stack_node) {
        if (stack_node->base.base.outer) {
          outer_name = unreal_fname_to_str(stack_node->base.base.outer->name, tmp.arena);
        }
        name = unreal_fname_to_str(stack_node->base.base.name, tmp.arena);
      }
      push_line(ctx, "$%02X: Final Function (stack node %.*s::%.*s)", opcode, STR_ARG(outer_name), STR_ARG(name));
    }
    scratch_end(tmp);

    serialize_until(ctx, EX_END_FUNCTION_PARMS);
    break;
  }

  case EX_CALL_MULTICAST_DELEGATE: {
    ustruct_t *stack_node = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t outer_name = STR_LIT("(null)");
      str_t name       = STR_LIT("(null)");
      if (stack_node) {
        if (stack_node->base.base.outer) {
          outer_name = unreal_fname_to_str(stack_node->base.base.outer->name, tmp.arena);
        }
        name = unreal_fname_to_str(stack_node->base.base.name, tmp.arena);
      }
      push_line(ctx, "$%02X: Call Multicast Delegate (signature %.*s::%.*s) delegate:", opcode, STR_ARG(outer_name), STR_ARG(name));
    }
    scratch_end(tmp);

    serialize_expr(ctx);

    push_line(ctx, "Params:");
    serialize_until(ctx, EX_END_FUNCTION_PARMS);
    break;
  }

  case EX_VIRTUAL_FUNCTION: {
    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t func_name = read_name(ctx, tmp.arena);
      push_line(ctx, "$%02X: Virtual Function named %.*s", opcode, STR_ARG(func_name));
    }
    scratch_end(tmp);

    serialize_until(ctx, EX_END_FUNCTION_PARMS);
    break;
  }

  case EX_CLASS_CONTEXT:
  case EX_CONTEXT:
  case EX_CONTEXT_FAIL_SILENT: {
    push_line(ctx, "$%02X: %s", opcode, opcode == EX_CLASS_CONTEXT ? "Class Context" : "Context");
    add_indent(ctx);

    push_line(ctx, "ObjectExpression:");
    serialize_expr(ctx);

    if (opcode == EX_CONTEXT_FAIL_SILENT) {
      push_line(ctx, "Can fail silently on access none");
    }

    code_skip_size_t skip_count = read_skip_count(ctx);
    code_skip_size_t offset     = ctx->pos + sizeof(ffield_t *) + skip_count;
    push_line(ctx, "Skip 0x%08X bytes to offset 0x%08X", skip_count, offset);

    ffield_t *field = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t name = STR_LIT("(null)");
      if (field) {
        name = unreal_fname_to_str(field->name, tmp.arena);
      }
      push_line(ctx, "R-Value Property: %.*s", STR_ARG(name));
    }
    scratch_end(tmp);

    push_line(ctx, "ContextExpression:");
    serialize_expr(ctx);

    drop_indent(ctx);
    break;
  }

  case EX_INT_CONST: {
    int32_t const_value = read_int(ctx);
    push_line(ctx, "$%02X: literal int32 %d", opcode, const_value);
    break;
  }

  case EX_INT64_CONST: {
    int64_t const_value = read_qword(ctx);
    push_line(ctx, "$%02X: literal int64 0x%llX", opcode, const_value);
    break;
  }

  case EX_UINT64_CONST: {
    uint64_t const_value = read_qword(ctx);
    push_line(ctx, "$%02X: literal uint64 0x%llX", opcode, const_value);
    break;
  }

  case EX_SKIP_OFFSET_CONST: {
    code_skip_size_t const_value = read_skip_count(ctx);
    push_line(ctx, "$%02X: literal CodeSkipSizeType 0x%08X", opcode, const_value);
    break;
  }

  case EX_FLOAT_CONST: {
    float const_value = read_float(ctx);
    push_line(ctx, "$%02X: literal float %f", opcode, const_value);
    break;
  }

  case EX_STRING_CONST: {
    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t const_value = STR_NULL;
      if (read_string8(ctx, &const_value, tmp.arena)) {
        push_line(ctx, "$%02X: literal ansi string \"%.*s\"", opcode, STR_ARG(const_value));
      }
    }
    scratch_end(tmp);
    break;
  }

  case EX_UNICODE_STRING_CONST: {
    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t const_value = STR_NULL;
      if (read_string16(ctx, &const_value, tmp.arena)) {
        push_line(ctx, "$%02X: literal unicode string \"%.*s\"", opcode, STR_ARG(const_value));
      }
    }
    scratch_end(tmp);
    break;
  }

  case EX_TEXT_CONST: {
    eblueprint_text_literal_kind_t kind = (eblueprint_text_literal_kind_t)read_byte(ctx);
    switch (kind) {
    case EBTL_EMPTY: {
      push_line(ctx, "$%02X: literal text - empty", opcode);
      break;
    }

    case EBTL_LOCALIZED_TEXT: {
      tmp_arena_t tmp = scratch_begin(ctx->arena);
      {
        str_t source_str = STR_NULL;
        str_t key_str    = STR_NULL;
        str_t namespace  = STR_NULL;

        if (read_string(ctx, &source_str, tmp.arena) && read_string(ctx, &key_str, tmp.arena) && read_string(ctx, &namespace, tmp.arena)) {
          push_line(
            ctx, "$%02X: literal text - localized text { namespace: \"%.*s\", key: \"%.*s\", source: \"%.*s\"}", opcode, STR_ARG(namespace), STR_ARG(key_str), STR_ARG(source_str));
        }
      }
      scratch_end(tmp);
      break;
    }

    case EBTL_INVARIANT_TEXT: {
      tmp_arena_t tmp = scratch_begin(ctx->arena);
      {
        str_t source_str = STR_NULL;

        if (read_string(ctx, &source_str, tmp.arena)) {
          push_line(ctx, "$%02X: literal text - invariant text: \"%.*s\"", opcode, STR_ARG(source_str));
        }
      }
      scratch_end(tmp);
      break;
    }

    case EBTL_LITERAL_STRING: {
      tmp_arena_t tmp = scratch_begin(ctx->arena);
      {
        str_t source_str = STR_NULL;

        if (read_string(ctx, &source_str, tmp.arena)) {
          push_line(ctx, "$%02X: literal text - literal string: \"%.*s\"", opcode, STR_ARG(source_str));
        }
      }
      scratch_end(tmp);
      break;
    }

    case EBTL_STRING_TABLE_ENTRY: {
      read_pointer(ctx);

      tmp_arena_t tmp = scratch_begin(ctx->arena);
      {
        str_t table_id_str = STR_NULL;
        str_t key_str      = STR_NULL;

        if (read_string(ctx, &table_id_str, tmp.arena) && read_string(ctx, &key_str, tmp.arena)) {
          push_line(ctx, "$%02X: literal text - string table entry: { tableid: \"%.*s\", key: \"%.*s\"}", opcode, STR_ARG(table_id_str), STR_ARG(key_str));
        }
      }
      scratch_end(tmp);

      break;
    }

    default:
      set_error_msg(ctx, "Unknown Blueprint Text Literal Type: %02X", kind);
      break;
    }
    break;
  }

  case EX_PROPERTY_CONST: {
    fprop_t *ptr = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t name = STR_LIT("(null)");
      if (ptr) {
        name = unreal_fname_to_str(ptr->base.name, tmp.arena);
      }
      push_line(ctx, "$%02X: EX_PROPERTY_CONST (%p:%.*s)", opcode, ptr, STR_ARG(name));
    }
    scratch_end(tmp);
    break;
  }

  case EX_OBJECT_CONST: {
    uobject_t *ptr = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t name = STR_LIT("(null)");
      if (ptr) {
        if (!ptr->cls || !unreal_uobject_is_valid(ptr)) {
          name = STR_LIT("(not a valid object)");
        } else {
          name = unreal_uobject_push_full_name(ptr, tmp.arena);
        }
      }
      push_line(ctx, "$%02X: EX_OBJECT_CONST (%p:%.*s)", opcode, ptr, STR_ARG(name));
    }
    scratch_end(tmp);
    break;
  }

  case EX_SOFT_OBJECT_CONST: {
    push_line(ctx, "$%02X: EX_SOFT_OBJECT_CONST", opcode);
    serialize_expr(ctx);
    break;
  }

  case EX_FIELD_PATH_CONST: {
    push_line(ctx, "$%02X: EX_FIELD_PATH_CONST", opcode);
    serialize_expr(ctx);
    break;
  }

  case EX_NAME_CONST: {
    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t const_value = read_name(ctx, tmp.arena);
      push_line(ctx, "$%02X: literal name %.*s", opcode, STR_ARG(const_value));
    }
    scratch_end(tmp);
    break;
  }

  case EX_ROTATION_CONST: {
    float pitch = read_float(ctx);
    float yaw   = read_float(ctx);
    float roll  = read_float(ctx);

    push_line(ctx, "$%02X: literal rotation (%f,%f,%f)", opcode, pitch, yaw, roll);
    break;
  }

  case EX_VECTOR_CONST: {
    float x = read_float(ctx);
    float y = read_float(ctx);
    float z = read_float(ctx);

    push_line(ctx, "$%02X: literal vector (%f,%f,%f)", opcode, x, y, z);
    break;
  }

  case EX_TRANSFORM_CONST: {
    float rot_x = read_float(ctx);
    float rot_y = read_float(ctx);
    float rot_z = read_float(ctx);
    float rot_w = read_float(ctx);

    float trans_x = read_float(ctx);
    float trans_y = read_float(ctx);
    float trans_z = read_float(ctx);

    float scale_x = read_float(ctx);
    float scale_y = read_float(ctx);
    float scale_z = read_float(ctx);

    push_line(ctx, "$%02X: literal transform R(%f,%f,%f,%f) T(%f,%f,%f) S(%f,%f,%f)", opcode, rot_x, rot_y, rot_z, rot_w, trans_x, trans_y, trans_z, scale_x, scale_y, scale_z);
    break;
  }

  case EX_STRUCT_CONST: {
    uscript_struct_t *sstruct = read_pointer(ctx);

    int32_t serialized_size = read_int(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t name = STR_LIT("None");
      if (sstruct) {
        name = unreal_uobject_push_name((uobject_t *)sstruct, tmp.arena);
      }
      push_line(ctx, "$%02X: literal struct %.*s (serialized size: %d)", opcode, STR_ARG(name), serialized_size);
    }
    scratch_end(tmp);

    serialize_until(ctx, EX_END_STRUCT_CONST);
    break;
  }

  case EX_SET_ARRAY: {
    push_line(ctx, "$%02X: set array", opcode);
    serialize_expr(ctx);
    serialize_until(ctx, EX_END_ARRAY);
    break;
  }

  case EX_ARRAY_CONST: {
    fprop_t *inner_prop = read_pointer(ctx);

    int32_t num = read_int(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t name = get_name_safe((ffield_t *)inner_prop, tmp.arena);
      push_line(ctx, "$%02X: set array const - elements number: %d, inner property: %.*s", opcode, num, STR_ARG(name));
    }
    scratch_end(tmp);

    serialize_until(ctx, EX_END_ARRAY_CONST);
    break;
  }

  case EX_BYTE_CONST: {
    uint8_t const_value = read_byte(ctx);
    push_line(ctx, "$%02X: literal byte %d", opcode, const_value);
    break;
  }

  case EX_INT_CONST_BYTE: {
    int32_t const_value = read_byte(ctx);
    push_line(ctx, "$%02X: literal int %d", opcode, const_value);
    break;
  }

  case EX_META_CAST: {
    uclass_t *cls = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t name = STR_LIT("None");
      if (cls) {
        name = unreal_uobject_push_name((uobject_t *)cls, tmp.arena);
      }
      push_line(ctx, "$%02X: MetaCast to %.*s of expr:", opcode, STR_ARG(name));
    }
    scratch_end(tmp);

    serialize_expr(ctx);
    break;
  }

  case EX_DYNAMIC_CAST: {
    uclass_t *cls = read_pointer(ctx);

    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t name = STR_LIT("None");
      if (cls) {
        name = unreal_uobject_push_name((uobject_t *)cls, tmp.arena);
      }
      push_line(ctx, "$%02X: DynamicCast to %.*s of expr:", opcode, STR_ARG(name));
    }
    scratch_end(tmp);

    serialize_expr(ctx);
    break;
  }

  case EX_JUMP_IF_NOT: {
    code_skip_size_t skip_count = read_skip_count(ctx);

    push_line(ctx, "$%02X: Jump to offset 0x%08X if not expr:", opcode, skip_count);

    serialize_expr(ctx);
    break;
  }

  case EX_ASSERT: {
    uint16_t line_number   = read_word(ctx);
    uint8_t  in_debug_mode = read_byte(ctx);

    push_line(ctx, "$%02X: assert at line %d, in debug mode = %d with expr:", opcode, line_number, in_debug_mode);

    serialize_expr(ctx);
    break;
  }

  case EX_SKIP: {
    code_skip_size_t w = read_skip_count(ctx);
    push_line(ctx, "$%02X: possibly skip 0x%08X bytes of expr:", opcode, w);

    serialize_expr(ctx);
    break;
  }

  case EX_INSTANCE_DELEGATE: {
    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t func_name = read_name(ctx, tmp.arena);
      push_line(ctx, "$%02X: instance delegate function named %.*s", opcode, STR_ARG(func_name));
    }
    scratch_end(tmp);
    break;
  }

  case EX_ADD_MULTICAST_DELEGATE: {
    push_line(ctx, "$%02X: Add Multicast delegate", opcode);
    serialize_expr(ctx);
    serialize_expr(ctx);
    break;
  }

  case EX_REMOVE_MULTICAST_DELEGATE: {
    push_line(ctx, "$%02X: Remove Multicast delegate", opcode);
    serialize_expr(ctx);
    serialize_expr(ctx);
    break;
  }

  case EX_CLEAR_MULTICAST_DELEGATE: {
    push_line(ctx, "$%02X: Clear Multicast delegate", opcode);
    serialize_expr(ctx);
    break;
  }

  case EX_BIND_DELEGATE: {
    tmp_arena_t tmp = scratch_begin(ctx->arena);
    {
      str_t func_name = read_name(ctx, tmp.arena);
      push_line(ctx, "$%02X: Bind Delegate %.*s", opcode, STR_ARG(func_name));
    }
    scratch_end(tmp);

    push_line(ctx, "Delegate:");
    serialize_expr(ctx);

    push_line(ctx, "Object:");
    serialize_expr(ctx);
    break;
  }

  case EX_PUSH_EXECUTION_FLOW: {
    code_skip_size_t skip_count = read_skip_count(ctx);
    push_line(ctx, "$%02X: FlowStack.Push(0x%08X)", opcode, skip_count);
    break;
  }

  case EX_POP_EXECUTION_FLOW: {
    push_line(ctx, "$%02X: if (FlowStack.Num()) { jump to statement at FlowStack.Pop(); } else { ERROR!!! }", opcode);
    break;
  }

  case EX_POP_EXECUTION_FLOW_IF_NOT: {
    push_line(ctx, "$%02X: if (!condition) { if (FlowStack.Num()) { jump to statement at FlowStack.Pop(); } else { ERROR!!! } }", opcode);
    serialize_expr(ctx);
    break;
  }

  case EX_BREAKPOINT: {
    push_line(ctx, "$%02X: <<< BREAKPOINT >>>", opcode);
    break;
  }

  case EX_WIRE_TRACEPOINT: {
    push_line(ctx, "$%02X: .. wire debug site ..", opcode);
    break;
  }

  case EX_INSTRUMENTATION_EVENT: {
    escript_instrumentation_kind_t event_type = (escript_instrumentation_kind_t)read_byte(ctx);
    switch (event_type) {
    case EST_INLINE_EVENT:
      push_line(ctx, "$%02X: .. instrumented inline event ..", opcode);
      break;
    case EST_STOP:
      push_line(ctx, "$%02X: .. instrumented event stop ..", opcode);
      break;
    case EST_PURE_NODE_ENTRY:
      push_line(ctx, "$%02X: .. instrumented pure node entry site ..", opcode);
      break;
    case EST_NODE_DEBUG_SITE:
      push_line(ctx, "$%02X: .. instrumented debug site ..", opcode);
      break;
    case EST_NODE_ENTRY:
      push_line(ctx, "$%02X: .. instrumented wire entry site ..", opcode);
      break;
    case EST_NODE_EXIT:
      push_line(ctx, "$%02X: .. instrumented wire exit site ..", opcode);
      break;
    case EST_PUSH_STATE:
      push_line(ctx, "$%02X: .. push execution state ..", opcode);
      break;
    case EST_RESTORE_STATE:
      push_line(ctx, "$%02X: .. restore execution state ..", opcode);
      break;
    case EST_RESET_STATE:
      push_line(ctx, "$%02X: .. reset execution state ..", opcode);
      break;
    case EST_SUSPEND_STATE:
      push_line(ctx, "$%02X: .. suspend execution state ..", opcode);
      break;
    case EST_POP_STATE:
      push_line(ctx, "$%02X: .. pop execution state ..", opcode);
      break;
    case EST_TUNNEL_END_OF_THREAD:
      push_line(ctx, "$%02X: .. tunnel end of thread ..", opcode);
      break;
    default:
      // TODO: handle other cases and set error on unknown case
      break;
    }
    break;
  }

  case EX_TRACEPOINT: {
    push_line(ctx, "$%02X: .. debug site ..", opcode);
    break;
  }

  case EX_SWITCH_VALUE: {
    uint16_t         num_cases  = read_word(ctx);
    code_skip_size_t after_skip = read_skip_count(ctx);

    push_line(ctx, "$%02X: switch value %d cases, end in 0x%08X", opcode, num_cases, after_skip);
    add_indent(ctx);

    push_line(ctx, "Index:");
    serialize_expr(ctx);

    for (uint16_t case_idx = 0; case_idx < num_cases; ++case_idx) {
      push_line(ctx, "[%d] Case Index (label: 0x%08X)", case_idx, ctx->pos);
      serialize_expr(ctx);

      code_skip_size_t offset_to_next_case = read_skip_count(ctx);
      push_line(ctx, "[%d] Offset to the next case 0x%08X", case_idx, offset_to_next_case);
      push_line(ctx, "[%d] Case Result:", case_idx);
      serialize_expr(ctx);
    }

    push_line(ctx, "Default result (label: 0x%08X)", ctx->pos);
    serialize_expr(ctx);

    push_line(ctx, "(label: 0x%08X)", ctx->pos);
    drop_indent(ctx);
    break;
  }

  case EX_ARRAY_GET_BY_REF: {
    push_line(ctx, "$%02X: Array Get-By-Ref Index", opcode);
    add_indent(ctx);
    serialize_expr(ctx);
    serialize_expr(ctx);
    drop_indent(ctx);
    break;
  }

  case EX_MAX:
  default: {
    push_line(ctx, "$%02X: unknown bytecode; ignoring it", opcode);
    break;
  }
  }
}
