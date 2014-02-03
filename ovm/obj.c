#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>


#define ARRAY_SIZE(a)  (sizeof(a) / sizeof((a)[0]))

#define FIELD_OFS(s, f)                   ((int) &((s *) 0)->f)
#define FIELD_PTR_TO_STRUCT_PTR(p, s, f)  ((s *)((char *)(p) - FIELD_OFS(s, f)))


typedef long long   obj_integer_val_t;
#define OBJ_INTEGER_PRINTF_FMT  "%lld"
typedef long double obj_float_val_t;
#define OBJ_FLOAT_PRINTF_FMT  "%Lg"


struct list {
  struct list *prev, *next;
};

static struct list *
list_init(struct list *li)
{
  return (li->prev = li->next = li);
}

static unsigned
list_empty(struct list *li)
{
  return (li->next == li);
}

static struct list *
list_end(struct list *li)
{
  return (li);
}

static struct list *
list_first(struct list *li)
{
  return (li->next);
}

static struct list *
list_last(struct list *li)
{
  return (li->prev);
}

static struct list *
list_prev(struct list *nd)
{
  return (nd->prev);
}

static struct list *
list_next(struct list *nd)
{
  return (nd->next);
}

static struct list *
list_insert(struct list *nd, struct list *before)
{
  struct list *p = before->prev;

  nd->prev = p;
  nd->next = before;

  return (p->next = before->prev = nd);
}

static struct list *
list_erase(struct list *nd)
{
  struct list *p = nd->prev, *q = nd->next;

  p->next = q;
  q->prev = p;

  nd->prev = nd->next = 0;

  return (nd);
}

/** @brief Object types */

enum obj_type {
  OBJ_TYPE_BASE = 0x55aa1231,

  OBJ_TYPE_OBJECT = OBJ_TYPE_BASE, /**< Parent of all types, not instantiable */
  OBJ_TYPE_NIL,			/**< Type of NIL  */
  OBJ_TYPE_BOOLEAN,		/**< Boolean */
  OBJ_TYPE_NUMBER,		/**< Number */
  OBJ_TYPE_INTEGER,		/**< Integer */
  OBJ_TYPE_FLOAT,
  OBJ_TYPE_BLOCK,
  OBJ_TYPE_STRING,
  OBJ_TYPE_BYTES,
  OBJ_TYPE_WORDS,
  OBJ_TYPE_DWORDS,
  OBJ_TYPE_QWORDS,
  OBJ_TYPE_BITS,
  OBJ_TYPE_DPTR,
  OBJ_TYPE_PAIR,
  OBJ_TYPE_LIST,
  OBJ_TYPE_ARRAY,
  OBJ_TYPE_DICT,
  OBJ_TYPE_LAST,

  OBJ_NUM_TYPES  = OBJ_TYPE_LAST - OBJ_TYPE_BASE
};

struct obj {
  struct list   list_node[1];
  unsigned      ref_cnt;
  enum obj_type type;
  union {
    unsigned          boolval;
    obj_integer_val_t intval;
    obj_float_val_t   floatval;
    struct objval_block {
      unsigned size;
      void     *ptr;
    } blockval;
    struct objval_string {
      unsigned size;
      char     *data;
    } strval;
    struct objval_bytes {
      unsigned      size;
      unsigned char *data;
    } bytesval;
    struct objval_words {
      unsigned       size;
      unsigned short *data;
    } wordsval;
    struct objval_dwrods {
      unsigned      size;
      unsigned long *data;
    } dwordsval;
    struct objval_qwords {
      unsigned           size;
      unsigned long long *data;
    } qwordsval;
    struct objval_dptr {
      struct obj *car, *cdr;
    } dptrval;
    struct objval_array {
      unsigned   size;
      struct obj **data;
    } arrayval;
    struct objval_dict {
      struct objval_array base;
      unsigned            cnt;
    } dictval;
  } val;
};

static unsigned
_obj_type(struct obj *obj)
{
  return (obj ? obj->type : OBJ_TYPE_NIL);
}

static unsigned
_obj_type_parent(unsigned type)
{
  switch (type) {
  case OBJ_TYPE_NIL:
  case OBJ_TYPE_BOOLEAN:
  case OBJ_TYPE_NUMBER:
  case OBJ_TYPE_BLOCK:
  case OBJ_TYPE_DPTR:
    return (OBJ_TYPE_OBJECT);
  case OBJ_TYPE_INTEGER:
  case OBJ_TYPE_FLOAT:
    return (OBJ_TYPE_NUMBER);
  case OBJ_TYPE_STRING:
  case OBJ_TYPE_BYTES:
  case OBJ_TYPE_WORDS:
  case OBJ_TYPE_DWORDS:
  case OBJ_TYPE_QWORDS:
  case OBJ_TYPE_ARRAY:
    return (OBJ_TYPE_BLOCK);
  case OBJ_TYPE_BITS:
    return (OBJ_TYPE_DWORDS);
  case OBJ_TYPE_PAIR:
  case OBJ_TYPE_LIST:
    return (OBJ_TYPE_DPTR);
  case OBJ_TYPE_DICT:
    return (OBJ_TYPE_ARRAY);
  default:
    assert(0);
  }

  return (0);
}

unsigned
obj_type_is_subtype_of(unsigned type, unsigned parent)
{
  for ( ; type != OBJ_TYPE_OBJECT; type = _obj_type_parent(type)) {
    if (type == parent)  return (1);
  }
  return (0);
}

static unsigned
_obj_is_kind_of(struct obj *obj, unsigned parent)
{
  return (obj_type_is_subtype_of(_obj_type(obj), parent));
}

enum {
  OBJ_VM_NUM_REGS = 8
};

struct obj_vm {
  struct obj *obj_pool;
  struct obj **stack, **stack_end;

  struct {
    struct list list[2];
    unsigned    idx_alloced, idx_free;
  } obj_list;
  struct obj *reg[OBJ_VM_NUM_REGS];
  struct obj **sp;
  int        errno;
};

enum {
  OBJ_ERRNO_NONE       = 0,
  OBJ_ERRNO_MEM        = -1,
  OBJ_ERRNO_BAD_METHOD = -2,
  OBJ_ERRNO_BAD_REG    = -3,
  OBJ_ERRNO_BAD_TYPE   = -4,
  OBJ_ERRNO_BAD_VALUE  = -5,
  OBJ_ERRNO_RANGE      = -6
};

int
obj_vm_errno(struct obj_vm *vm)
{
  return (vm->errno);
}

void
obj_vm_errno_clr(struct obj_vm *vm)
{
  vm->errno = OBJ_ERRNO_NONE;
}

void
obj_vm_init(struct obj_vm *vm, unsigned obj_pool_size, unsigned stack_size)
{
  struct list *li;
  struct obj  *p;
  unsigned    n;

  vm->obj_pool = 0;
  vm->stack    = 0;
  
  if ((vm->obj_pool = (struct obj *) malloc(obj_pool_size * sizeof(*vm->obj_pool))) == 0
      || (vm->stack = (struct obj **) malloc(stack_size * sizeof(*vm->stack))) == 0
      ) {
    vm->errno = OBJ_ERRNO_MEM;
    goto done;
  }

  li = &vm->obj_list.list[vm->obj_list.idx_alloced = 0];
  list_init(li);
  li = &vm->obj_list.list[vm->obj_list.idx_free = 1];
  list_init(li);
  for (p = vm->obj_pool, n = obj_pool_size; n; --n, ++p) {
    list_insert(p->list_node, list_end(li));
  }

  vm->stack_end = vm->stack + stack_size;

  memset(vm->reg, 0, sizeof(vm->reg));
  vm->sp = vm->stack_end;

  vm->errno = OBJ_ERRNO_NONE;

 done:
  if (vm->errno != OBJ_ERRNO_NONE) {
    if (vm->obj_pool)  free(vm->obj_pool);
    if (vm->stack)     free(vm->stack);
  }
}

void
obj_vm_fini(struct obj_vm *vm)
{
  /** @todo Free all obj data (strings, arrays) */

  free(vm->obj_pool);
  free(vm->stack);
}

static struct obj *
_obj_retain(struct obj *obj)
{
  if (obj)  ++obj->ref_cnt;

  return (obj);
}

static void _obj_release(struct obj_vm *vm, struct obj *obj);

static void
_obj_free_block(struct obj_vm *vm, struct obj *obj)
{
  free(obj->val.blockval.ptr);

  obj->val.blockval.size = 0;
  obj->val.blockval.ptr  = 0;
}

static void
_obj_free_dptr(struct obj_vm *vm, struct obj *obj)
{
  _obj_release(vm, obj->val.dptrval.car);
  _obj_release(vm, obj->val.dptrval.cdr);

  obj->val.dptrval.car = 0;
  obj->val.dptrval.cdr = 0;
}

static void
_obj_free_array(struct obj_vm *vm, struct obj *obj)
{
  struct obj **p;
  unsigned   n;
  
  for (p = obj->val.arrayval.data, n = obj->val.arrayval.size; n; --n, ++p) {
    _obj_release(vm, *p);
  }
}

static void
_obj_free(struct obj_vm *vm, struct obj *obj)
{
  unsigned type;

  for (type = _obj_type(obj); type != OBJ_TYPE_OBJECT; type = _obj_type_parent(type)) {
    switch (type) {
    case OBJ_TYPE_BOOLEAN:
    case OBJ_TYPE_INTEGER:
    case OBJ_TYPE_FLOAT:
    case OBJ_TYPE_PAIR:
    case OBJ_TYPE_LIST:
    case OBJ_TYPE_DICT:
      break;
    case OBJ_TYPE_BLOCK:
      _obj_free_block(vm, obj);
      break;
    case OBJ_TYPE_DPTR:
      _obj_free_dptr(vm, obj);
      break;
    case OBJ_TYPE_ARRAY:
      _obj_free_array(vm, obj);
      break;
    default:
      ;
    }
  }

  list_erase(obj->list_node);

  list_insert(obj->list_node, &vm->obj_list.list[vm->obj_list.idx_free]);
}

static void
_obj_release(struct obj_vm *vm, struct obj *obj)
{
  if (obj == 0)  return;

  assert(obj->ref_cnt != 0);

  if (--obj->ref_cnt != 0)  return;

  _obj_free(vm, obj);
}

static void
_obj_assign(struct obj_vm *vm, struct obj **p, struct obj *obj)
{
  struct obj *temp;

  temp = *p;
  _obj_retain(*p = obj);
  _obj_release(vm, temp);
}

static struct obj **
_obj_vm_reg(struct obj_vm *vm, unsigned reg)
{
  if (reg >= ARRAY_SIZE(vm->reg)) {
    assert(0);
  }

  return (&vm->reg[reg]);
}

static struct obj *
obj_vm_reg(struct obj_vm *vm, unsigned reg)
{
  return (*_obj_vm_reg(vm, reg));
}

static struct obj **
_obj_vm_pick(struct obj_vm *vm, unsigned ofs)
{
  if ((vm->sp + ofs) >= vm->stack_end) {
    assert(0);
  }

  return (&vm->sp[ofs]);
}

void
obj_vm_pick(struct obj_vm *vm, unsigned r1, unsigned ofs)
{
  _obj_assign(vm, _obj_vm_reg(vm, r1), *_obj_vm_pick(vm, ofs));
}

void
obj_vm_drop(struct obj_vm *vm)
{
  assert(vm->sp < vm->stack_end);

  _obj_release(vm, *vm->sp);

  ++vm->sp;
}

void
obj_vm_dropn(struct obj_vm *vm, unsigned n)
{
  struct obj **p;
  unsigned   nn;

  assert((vm->sp + n) <= vm->stack_end);

  for (p = vm->sp, nn = n; nn; --nn, ++p)  _obj_release(vm, *p);

  vm->sp += n;
}

void
obj_vm_pop(struct obj_vm *vm, unsigned r1)
{
  assert(vm->sp < vm->stack_end);

  _obj_assign(vm, _obj_vm_reg(vm, r1), *vm->sp);

  obj_vm_drop(vm);
}

static void
_obj_vm_push(struct obj_vm *vm, struct obj *obj)
{
  assert(vm->sp > vm->stack);

  *--vm->sp = 0;

  _obj_assign(vm, vm->sp, obj);
}

void
obj_vm_push(struct obj_vm *vm, unsigned r1)
{
  _obj_vm_push(vm, obj_vm_reg(vm, r1));
}

unsigned
obj_vm_type(struct obj_vm *vm, unsigned r1)
{
  return (_obj_type(obj_vm_reg(vm, r1)));
}

unsigned
obj_vm_is_kind_of(struct obj_vm *vm, unsigned reg, unsigned parent)
{
  return (_obj_is_kind_of(obj_vm_reg(vm, reg), parent));
}

void
obj_vm_move(struct obj_vm *vm, unsigned r1, unsigned r2)
{
  _obj_assign(vm, _obj_vm_reg(vm, r1), obj_vm_reg(vm, r2));
}

static struct obj **
_obj_alloc(struct obj_vm *vm, unsigned type)
{
  struct obj *obj;

  assert(type >= OBJ_TYPE_BASE && type < OBJ_TYPE_LAST);

  switch (type) {
  case OBJ_TYPE_OBJECT:
    assert(0);			/* Not instantiable */
  case OBJ_TYPE_NIL:
    obj = 0;
    break;
  default:
    {
      struct list *li;
      struct list *nd;
    
      li = &vm->obj_list.list[vm->obj_list.idx_free];
      if (list_first(li) == list_end(li)) {
	assert(0);
      }
      
      nd = list_first(li);
      list_erase(nd);

      obj = FIELD_PTR_TO_STRUCT_PTR(nd, struct obj, list_node);
      memset(obj, 0, sizeof(*obj));
      obj->type = type;
      
      list_insert(nd, list_end(&vm->obj_list.list[vm->obj_list.idx_alloced]));
    }
  }

  if (*--vm->sp = obj)  obj->ref_cnt = 1;

  return (vm->sp);
}

/***************************************************************************/

enum obj_op {
  OBJ_OP_ABS,
  OBJ_OP_ADD,
  OBJ_OP_AND,
  OBJ_OP_APPEND,
  OBJ_OP_AT,
  OBJ_OP_AT_PUT,
  OBJ_OP_CAR,
  OBJ_OP_CDR,
  OBJ_OP_COUNT,
  OBJ_OP_DEL,
  OBJ_OP_DIV,
  OBJ_OP_EQ,
  OBJ_OP_GT,
  OBJ_OP_HASH,
  OBJ_OP_KEYS,
  /* OBJ_OP_LSH */
  OBJ_OP_LT,
  OBJ_OP_MINUS,
  OBJ_OP_MOD,
  OBJ_OP_MULT,
  OBJ_OP_NOT,
  OBJ_OP_OR,
  /* OBJ_OP_RSH */
  OBJ_OP_SIZE,
  OBJ_OP_SLICE,
  OBJ_OP_SUB,
  OBJ_NUM_OPS
};

void obj_vm_method_call(struct obj_vm *vm, unsigned r1, unsigned op, ...);

/***************************************************************************/

static unsigned
crc_init(unsigned *r)
{
  *r = 0;
}

static unsigned
crc32(unsigned *r, unsigned n, unsigned char *p)
{
  for ( ; n; --n, ++p)  *r += *p;

  return (*r);
}

/***************************************************************************/

static struct obj **__obj_nil_new(struct obj_vm *vm);
static struct obj **__obj_bool_new(struct obj_vm *vm, unsigned val);
static struct obj **__obj_integer_new(struct obj_vm *vm, obj_integer_val_t val);
static struct obj **__obj_float_new(struct obj_vm *vm, obj_float_val_t val);
static struct obj **__obj_string_new(struct obj_vm *vm, unsigned n, ...);
static struct obj **__obj_pair_new(struct obj_vm *vm, struct obj *car, struct obj *cdr);
static struct obj **__obj_list_new(struct obj_vm *vm, struct obj *car, struct obj *cdr);
static struct obj **__obj_array_new(struct obj_vm *vm, unsigned type, unsigned size);
void obj_vm_newc(struct obj_vm *vm, unsigned r1, unsigned type, ...);
void obj_vm_new(struct obj_vm *vm, unsigned r1, unsigned type, ...);

static void
obj_bad_method(struct obj_vm *vm, unsigned r1, va_list ap)
{
  vm->errno = OBJ_ERRNO_BAD_METHOD;
}

/***************************************************************************/

static struct obj **
__obj_nil_new(struct obj_vm *vm)
{
  return (_obj_alloc(vm, OBJ_TYPE_NIL));
}

static void
_obj_nil_new(struct obj_vm *vm, unsigned r1, va_list ap)
{
  _obj_assign(vm, _obj_vm_reg(vm, r1), 0);
}

static void
obj_nil_append(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  switch (_obj_type(q)) {
  case OBJ_TYPE_NIL:
    return;
  case OBJ_TYPE_LIST:
    break;
  default:
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  _obj_assign(vm, _obj_vm_reg(vm, r1), q);
}

static void
obj_nil_at(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_INTEGER) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  vm->errno = OBJ_ERRNO_RANGE;
}

static void
obj_nil_eq(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  __obj_bool_new(vm, q == 0);
  obj_vm_pop(vm, r1);
}

static void
obj_nil_hash(struct obj_vm *vm, unsigned r1, va_list ap)
{
  __obj_integer_new(vm, 0);
  obj_vm_pop(vm, r1);
}

static void
obj_nil_size(struct obj_vm *vm, unsigned r1, va_list ap)
{
  __obj_integer_new(vm, 0);
  obj_vm_pop(vm, r1);
}

static void
obj_nil_slice(struct obj_vm *vm, unsigned r1, va_list ap)
{
  _obj_assign(vm, _obj_vm_reg(vm, r1), 0);
}

/***************************************************************************/

static struct obj **
__obj_bool_new(struct obj_vm *vm, unsigned val)
{
  struct obj **result = _obj_alloc(vm, OBJ_TYPE_BOOLEAN);

  (*result)->val.boolval = (val != 0);

  return (result);
}

static void
_obj_bool_new(struct obj_vm *vm, unsigned r1, va_list ap)
{
  unsigned   val;
  struct obj *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  switch (_obj_type(q)) {
  case OBJ_TYPE_BOOLEAN:
    _obj_assign(vm, _obj_vm_reg(vm, r1), q);
    return;
  case OBJ_TYPE_INTEGER:
    val = (q->val.intval != 0);
    break;
  case OBJ_TYPE_FLOAT:
    val = (q->val.floatval != 0.0);
    break;
  case OBJ_TYPE_STRING:
    val = (q->val.strval.size != 0);
    break;
  default:
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  __obj_bool_new(vm, val);
  obj_vm_pop(vm, r1);
}

static void
obj_bool_and(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_BOOLEAN) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  __obj_bool_new(vm, p->val.boolval && q->val.boolval);
  obj_vm_pop(vm, r1);
}

static void
obj_bool_eq(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  __obj_bool_new(vm, _obj_type(q) == OBJ_TYPE_BOOLEAN && (p->val.boolval != 0) == (q->val.boolval != 0));
  obj_vm_pop(vm, r1);
}

static void
obj_bool_hash(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1);

  __obj_integer_new(vm, (p->val.boolval != 0));
  obj_vm_pop(vm, r1);
}

static void
obj_bool_not(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1);

  __obj_bool_new(vm, (p->val.boolval == 0));
  obj_vm_pop(vm, r1);
}

static void
obj_bool_or(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_BOOLEAN) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  __obj_bool_new(vm, p->val.boolval || q->val.boolval);
  obj_vm_pop(vm, r1);
}

unsigned
obj_bool_val(struct obj_vm *vm, unsigned r1)
{
  struct obj *p = obj_vm_reg(vm, r1);

  if (_obj_type(p) != OBJ_TYPE_BOOLEAN) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return (0);
  }

  return (p->val.boolval != 0);
}

/***************************************************************************/

static struct obj **
__obj_integer_new(struct obj_vm *vm, obj_integer_val_t val)
{
  struct obj **result = _obj_alloc(vm, OBJ_TYPE_INTEGER);

  (*result)->val.intval = val;

  return (result);
}

static void
_obj_integer_new(struct obj_vm *vm, unsigned r1, va_list ap)
{
  obj_integer_val_t val;
  struct obj        *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  switch (_obj_type(q)) {
  case OBJ_TYPE_BOOLEAN:
    val = (q->val.boolval != 0);
    break;
  case OBJ_TYPE_INTEGER:
    _obj_assign(vm, _obj_vm_reg(vm, r1), q);
    return;
  case OBJ_TYPE_FLOAT:
    val = (obj_integer_val_t) q->val.floatval;
    break;
  case OBJ_TYPE_STRING:
    if (q->val.strval.size >= 2
	&& q->val.strval.data[0] == '0'
	) {
      if (q->val.strval.size >= 3
	  && (q->val.strval.data[1] | 0x20) == 'x'
	  ) {
	if (sscanf(q->val.strval.data, "%llx", &val) != 1) {
	  vm->errno = OBJ_ERRNO_BAD_VALUE;
	  return;
	}
	break;
      }
      
      if (sscanf(q->val.strval.data, "%llo", &val) != 1) {
	vm->errno = OBJ_ERRNO_BAD_VALUE;
	return;
      }
      break;
    }
    if (sscanf(q->val.strval.data, "%lld", &val) != 1) {
      vm->errno = OBJ_ERRNO_BAD_VALUE;
      return;
    }
    break;
  default:
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  __obj_integer_new(vm, val);
  obj_vm_pop(vm, r1);
}

static void
obj_integer_abs(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1);

  __obj_integer_new(vm, abs(p->val.intval));
  obj_vm_pop(vm, r1);
}

static void
obj_integer_add(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_INTEGER) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  __obj_integer_new(vm, p->val.intval + q->val.intval);
  obj_vm_pop(vm, r1);
}

static void
obj_integer_and(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_INTEGER) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  __obj_integer_new(vm, p->val.intval & q->val.intval);
  obj_vm_pop(vm, r1);
}

static void
obj_integer_div(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_INTEGER) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  __obj_integer_new(vm, p->val.intval / q->val.intval);
  obj_vm_pop(vm, r1);
}

static void
obj_integer_eq(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  __obj_bool_new(vm, _obj_type(q) == OBJ_TYPE_INTEGER && p->val.intval == q->val.intval);
  obj_vm_pop(vm, r1);
}

static void
obj_integer_gt(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_INTEGER) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  __obj_bool_new(vm, p->val.intval > q->val.intval);
  obj_vm_pop(vm, r1);
}

static void
obj_integer_hash(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1);
  unsigned   r[1];

  crc_init(r);
  __obj_integer_new(vm, crc32(r, sizeof(p->val.intval), (unsigned char *) &p->val.intval));
  obj_vm_pop(vm, r1);
}

static void
obj_integer_lt(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_INTEGER) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  __obj_bool_new(vm, p->val.intval < q->val.intval);
  obj_vm_pop(vm, r1);
}

static void
obj_integer_minus(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1);

  __obj_integer_new(vm, -p->val.intval);
  obj_vm_pop(vm, r1);
}

static void
obj_integer_mod(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_INTEGER) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  __obj_integer_new(vm, p->val.intval % q->val.intval);
  obj_vm_pop(vm, r1);
}

static void
obj_integer_mult(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_INTEGER) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  __obj_integer_new(vm, p->val.intval * q->val.intval);
  obj_vm_pop(vm, r1);
}

static void
obj_integer_or(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_INTEGER) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  __obj_integer_new(vm, p->val.intval | q->val.intval);
  obj_vm_pop(vm, r1);
}

static void
obj_integer_sub(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_INTEGER) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  __obj_integer_new(vm, p->val.intval - q->val.intval);
  obj_vm_pop(vm, r1);
}

obj_integer_val_t
obj_integer_val(struct obj_vm *vm, unsigned r1)
{
  struct obj *p = obj_vm_reg(vm, r1);

  if (_obj_type(p) != OBJ_TYPE_INTEGER) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;

    return (0);
  }

  return (p->val.intval);
}

/***************************************************************************/

static struct obj **
__obj_float_new(struct obj_vm *vm, obj_float_val_t val)
{
  struct obj **result = _obj_alloc(vm, OBJ_TYPE_FLOAT);

  (*result)->val.floatval = val;

  return (result);
}

static void
_obj_float_new(struct obj_vm *vm, unsigned r1, va_list ap)
{
  obj_float_val_t val;
  struct obj      *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  switch (_obj_type(q)) {
  case OBJ_TYPE_BOOLEAN:
    val = (q->val.boolval != 0);
    break;
  case OBJ_TYPE_FLOAT:
    _obj_assign(vm, _obj_vm_reg(vm, r1), q);
    return;
  case OBJ_TYPE_INTEGER:
    val = (obj_float_val_t) q->val.intval;
    break;
  case OBJ_TYPE_STRING:
    sscanf(q->val.strval.data, "%Lg", &val);
    break;
  default:
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  __obj_float_new(vm, val);
  obj_vm_pop(vm, r1);
}

static void
obj_float_abs(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1);

  __obj_float_new(vm, abs(p->val.floatval));
  obj_vm_pop(vm, r1);
}

static void
obj_float_add(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_FLOAT) {
    assert(0);
    return;
  }

  __obj_float_new(vm, p->val.floatval + q->val.floatval);
  obj_vm_pop(vm, r1);
}

static void
obj_float_div(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_FLOAT) {
    assert(0);
    return;
  }

  __obj_float_new(vm, p->val.floatval / q->val.floatval);
  obj_vm_pop(vm, r1);
}

static void
obj_float_eq(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  __obj_bool_new(vm, _obj_type(q) == OBJ_TYPE_FLOAT && p->val.floatval == q->val.floatval);
  obj_vm_pop(vm, r1);
}

static void
obj_float_gt(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_FLOAT) {
    assert(0);
    return;
  }

  __obj_bool_new(vm, p->val.floatval > q->val.floatval);
  obj_vm_pop(vm, r1);
}

static void
obj_float_hash(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1);
  unsigned   r[1];

  crc_init(r);
  __obj_integer_new(vm, crc32(r, sizeof(p->val.floatval), (unsigned char *) &p->val.floatval));
  obj_vm_pop(vm, r1);
}

static void
obj_float_lt(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_FLOAT) {
    assert(0);
    return;
  }

  __obj_bool_new(vm, p->val.floatval < q->val.floatval);
  obj_vm_pop(vm, r1);
}

static void
obj_float_minus(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1);

  __obj_float_new(vm, -p->val.floatval);
  obj_vm_pop(vm, r1);
}

static void
obj_float_mult(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_FLOAT) {
    assert(0);
    return;
  }

  __obj_float_new(vm, p->val.floatval * q->val.floatval);
  obj_vm_pop(vm, r1);
}

static void
obj_float_sub(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_FLOAT) {
    assert(0);
    return;
  }

  __obj_float_new(vm, p->val.floatval - q->val.floatval);
  obj_vm_pop(vm, r1);
}

obj_float_val_t
obj_float_val(struct obj_vm *vm, unsigned r1)
{
  struct obj *p = obj_vm_reg(vm, r1);

  assert(_obj_type(p) == OBJ_TYPE_FLOAT);

  return (p->val.floatval);
}

/***************************************************************************/

static struct obj **
__obj_string_new(struct obj_vm *vm, unsigned n, ...)
{
  va_list    ap;
  unsigned   size, len, nn;
  struct obj **result;
  char       *p, *s;

  va_start(ap, n);
  for (size = 0, nn = n; nn; --nn) {
    len = va_arg(ap, unsigned);
    s   = va_arg(ap, char *);
    
    size += len;
  }
  ++size;
  va_end(ap);

  result = _obj_alloc(vm, OBJ_TYPE_STRING);
  (*result)->val.strval.data = p = malloc((*result)->val.strval.size = size);

  va_start(ap, n);
  for (nn = n; nn; --nn) {
    len = va_arg(ap, unsigned);
    s   = va_arg(ap, char *);

    memcpy(p, s, len);
    p += len;
  }
  va_end(ap);

  *p = 0;

  return (result);
}

static void
_obj_string_new(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *q = obj_vm_reg(vm, va_arg(ap, unsigned));
  char       buf[64], *data = buf;

  switch (_obj_type(q)) {
  case OBJ_TYPE_NIL:
    data = "#nil";
    break;
  case OBJ_TYPE_BOOLEAN:
    data = q->val.boolval ? "#true" : "#false";
    break;
  case OBJ_TYPE_INTEGER:
    sprintf(buf, OBJ_INTEGER_PRINTF_FMT, q->val.intval);
    break;
  case OBJ_TYPE_FLOAT:
    sprintf(buf, OBJ_FLOAT_PRINTF_FMT, q->val.floatval);
    break;
  case OBJ_TYPE_STRING:
    _obj_assign(vm, _obj_vm_reg(vm, r1), q);
    return;
  case OBJ_TYPE_PAIR:
    {
      struct obj **pp;

      pp = __obj_nil_new(vm);
      obj_vm_push(vm, 1);
      obj_vm_push(vm, 2);

      _obj_assign(vm, _obj_vm_reg(vm, 1), q->val.dptrval.car);
      obj_vm_new(vm, 1, OBJ_TYPE_STRING, 1);
      _obj_assign(vm, _obj_vm_reg(vm, 2), q->val.dptrval.cdr);
      obj_vm_new(vm, 2, OBJ_TYPE_STRING, 2);
      
      _obj_assign(vm, pp, *__obj_string_new(vm, 5, 1, "<",
					           obj_vm_reg(vm, 1)->val.strval.size - 1, obj_vm_reg(vm, 1)->val.strval.data,
					           2, ", ",
					           obj_vm_reg(vm, 2)->val.strval.size - 1, obj_vm_reg(vm, 2)->val.strval.data,
					           1, ">"
					    )
		  );

      obj_vm_drop(vm);
      obj_vm_pop(vm, 2);
      obj_vm_pop(vm, 1);
      obj_vm_pop(vm, r1);
    }
    return;
  case OBJ_TYPE_LIST:
    {
      struct obj **pp;
      unsigned   f;
      
      pp = __obj_string_new(vm, 1, 1, "(");
      obj_vm_push(vm, 1);
      for (f = 1; q; q = q->val.dptrval.cdr) {
	_obj_assign(vm, _obj_vm_reg(vm, 1), q->val.dptrval.car);
	obj_vm_new(vm, 1, OBJ_TYPE_STRING, 1);
	_obj_assign(vm, pp, *(f ? __obj_string_new(vm, 2, (*pp)->val.strval.size - 1, (*pp)->val.strval.data,
						          obj_vm_reg(vm, 1)->val.strval.size - 1, obj_vm_reg(vm, 1)->val.strval.data
						   ) 
		                : __obj_string_new(vm, 3, (*pp)->val.strval.size - 1, (*pp)->val.strval.data,
						          2, ", ",
						          obj_vm_reg(vm, 1)->val.strval.size - 1, obj_vm_reg(vm, 1)->val.strval.data
						   )
			      )
		    );
	obj_vm_drop(vm);

	f = 0;
      }
      _obj_assign(vm, pp, *__obj_string_new(vm, 2, (*pp)->val.strval.size - 1, (*pp)->val.strval.data,
					           1, ")"
					    )
		  );
      obj_vm_drop(vm);

      obj_vm_pop(vm, 1);
      obj_vm_pop(vm, r1);
    }
    return;
  case OBJ_TYPE_ARRAY:
    {
      struct obj **pp, **rr;
      unsigned   f, n;
      
      pp = __obj_string_new(vm, 1, 1, "[");
      obj_vm_push(vm, 1);
      for (f = 1, rr = q->val.arrayval.data, n = q->val.arrayval.size; n; --n, ++rr) {
	_obj_assign(vm, _obj_vm_reg(vm, 1), *rr);
	obj_vm_new(vm, 1, OBJ_TYPE_STRING, 1);
	_obj_assign(vm, pp, *(f ? __obj_string_new(vm, 2, (*pp)->val.strval.size - 1, (*pp)->val.strval.data,
						          obj_vm_reg(vm, 1)->val.strval.size - 1, obj_vm_reg(vm, 1)->val.strval.data
						   )
		                : __obj_string_new(vm, 3, (*pp)->val.strval.size - 1, (*pp)->val.strval.data,
						          2, ", ",
						          obj_vm_reg(vm, 1)->val.strval.size - 1, obj_vm_reg(vm, 1)->val.strval.data
						   )
			      )
		    );
	obj_vm_drop(vm);

	f = 0;
      }
      _obj_assign(vm, pp, *__obj_string_new(vm, 2, (*pp)->val.strval.size - 1, (*pp)->val.strval.data,
					           1, "]"
					    )
		  );
      obj_vm_drop(vm);

      obj_vm_pop(vm, 1);
      obj_vm_pop(vm, r1);
    }
    return;
  case OBJ_TYPE_DICT:
    {
      struct obj **pp, **rr, *s, *t;
      unsigned   f, n;
      
      pp = __obj_string_new(vm, 1, 1, "{");
      obj_vm_push(vm, 1);
      obj_vm_push(vm, 2);
      for (f = 1, rr = q->val.arrayval.data, n = q->val.arrayval.size; n; --n, ++rr) {
	for (s = *rr; s; s = s->val.dptrval.cdr) {
	  t = s->val.dptrval.car;
	  _obj_assign(vm, _obj_vm_reg(vm, 1), t->val.dptrval.car);
	  obj_vm_new(vm, 1, OBJ_TYPE_STRING, 1);
	  _obj_assign(vm, _obj_vm_reg(vm, 2), t->val.dptrval.cdr);
	  obj_vm_new(vm, 2, OBJ_TYPE_STRING, 2);
	  _obj_assign(vm, pp, *(f ? __obj_string_new(vm, 4, (*pp)->val.strval.size - 1, (*pp)->val.strval.data,
					 	            obj_vm_reg(vm, 1)->val.strval.size - 1, obj_vm_reg(vm, 1)->val.strval.data,
						            2, ": ",
						            obj_vm_reg(vm, 2)->val.strval.size - 1, obj_vm_reg(vm, 2)->val.strval.data
						     )
		                  : __obj_string_new(vm, 5, (*pp)->val.strval.size - 1, (*pp)->val.strval.data,
						            2, ", ",
						            obj_vm_reg(vm, 1)->val.strval.size - 1, obj_vm_reg(vm, 1)->val.strval.data,
						            2, ": ",
						            obj_vm_reg(vm, 2)->val.strval.size - 1, obj_vm_reg(vm, 2)->val.strval.data
						     )
				)
		      );
	  obj_vm_drop(vm);

	  f = 0;
	}
      }
      _obj_assign(vm, pp, *__obj_string_new(vm, 2, (*pp)->val.strval.size - 1, (*pp)->val.strval.data,
					           1, "}"
					    )
		  );
      obj_vm_drop(vm);

      obj_vm_pop(vm, 2);
      obj_vm_pop(vm, 1);
      obj_vm_pop(vm, r1);
    }
    return;

  default:
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  __obj_string_new(vm, 1, strlen(data), data);
  obj_vm_pop(vm, r1);
}

static void
obj_string_append(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));
  char       buf[2];

  if (_obj_type(q) != OBJ_TYPE_STRING) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }
  
  __obj_string_new(vm, 2, p->val.strval.size - 1, p->val.strval.data,
		          q->val.strval.size - 1, q->val.strval.data
		   );
  obj_vm_pop(vm, r1);
}

static void
obj_string_at(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));
  char       buf[1];

  if (_obj_type(q) != OBJ_TYPE_INTEGER ||q->val.intval >= p->val.strval.size) {
    assert(0);
  }
  
  buf[0] = p->val.strval.data[q->val.intval];

  __obj_string_new(vm, 1, 1, buf);
  obj_vm_pop(vm, r1);
}

static void
obj_string_eq(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  __obj_bool_new(vm, _obj_type(q) == OBJ_TYPE_STRING && strcmp(p->val.strval.data, q->val.strval.data) == 0);
  obj_vm_pop(vm, r1);
}

static void
obj_string_gt(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_STRING) {
    assert(0);
  }
  
  __obj_bool_new(vm, strcmp(p->val.strval.data, q->val.strval.data) > 0);
  obj_vm_pop(vm, r1);
}

static void
obj_string_hash(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1);
  unsigned   r[1];

  crc_init(r);
  __obj_integer_new(vm, crc32(r, p->val.strval.size - 1, (unsigned char *) p->val.strval.data));
  obj_vm_pop(vm, r1);
}

static void
obj_string_lt(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_STRING) {
    assert(0);
  }
  
  __obj_bool_new(vm, strcmp(p->val.strval.data, q->val.strval.data) < 0);
  obj_vm_pop(vm, r1);
}

static void
obj_string_size(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1);

  __obj_integer_new(vm, p->val.strval.size - 1);
  obj_vm_pop(vm, r1);
}

void
slice_idxs(int size, int *start, int *len)
{
  int end;

  if (*start < 0)  *start = size + *start;
  end = *start + *len;

  if (end < *start) {
    int temp;

    temp   = *start;
    *start = end + 1;
    end    = temp + 1;
  }

  if (end < 0 || *start >= size) {
    *start = *len = 0;
  } else {
    if (*start < 0)  *start = 0;
    if (end > size)  end = size;
    *len = end - *start;
  }
}

static void
obj_string_slice(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1);
  struct obj *q = obj_vm_reg(vm, va_arg(ap, unsigned));
  struct obj *r = obj_vm_reg(vm, va_arg(ap, unsigned));
  int        i, n;

  if (_obj_type(q) != OBJ_TYPE_INTEGER || _obj_type(r) != OBJ_TYPE_INTEGER) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  i = q->val.intval;
  n = r->val.intval;
  slice_idxs((int) p->val.strval.size - 1, &i, &n);

  __obj_string_new(vm, 1, n, &p->val.strval.data[i]);
  obj_vm_pop(vm, r1);
}


char *
obj_string_val(struct obj_vm *vm, unsigned r1)
{
  struct obj *p = obj_vm_reg(vm, r1);

  assert(_obj_type(p) == OBJ_TYPE_STRING);

  return (p->val.strval.data);
}

/***************************************************************************/

static struct obj **
__obj_bytes_new(struct obj_vm *vm, unsigned n, ...)
{
  va_list       ap;
  unsigned      size, len, nn;
  struct obj    **result;
  unsigned char *p, *s;

  va_start(ap, n);
  for (size = 0, nn = n; nn; --nn) {
    len = va_arg(ap, unsigned);
    s   = va_arg(ap, unsigned char *);
    
    size += len;
  }
  va_end(ap);

  result = _obj_alloc(vm, OBJ_TYPE_BYTES);
  (*result)->val.strval.data = p = malloc((*result)->val.strval.size = size);

  va_start(ap, n);
  for (nn = n; nn; --nn) {
    len = va_arg(ap, unsigned);
    s   = va_arg(ap, unsigned char *);

    memcpy(p, s, len);
    p += len;
  }
  va_end(ap);

  return (result);
}

static void
obj_bytes_and(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_BYTES) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  __obj_bytes_new(vm, 1, p->val.bytesval.size, p->val.bytesval.data);

  

  obj_vm_pop(vm, r1);
}

/***************************************************************************/

static struct obj **
__obj_dptr_new(struct obj_vm *vm, unsigned type, struct obj *car, struct obj *cdr)
{
  struct obj **result = _obj_alloc(vm, type);

  _obj_assign(vm, &(*result)->val.dptrval.car, car);
  _obj_assign(vm, &(*result)->val.dptrval.cdr, cdr);

  return (result);
}

static void
obj_dptr_car(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj **p = _obj_vm_reg(vm, r1);

  _obj_assign(vm, p, (*p)->val.dptrval.car);
}

static void
obj_dptr_cdr(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj **p = _obj_vm_reg(vm, r1);

  _obj_assign(vm, p, (*p)->val.dptrval.cdr);
}

/***************************************************************************/

static struct obj **
__obj_pair_new(struct obj_vm *vm, struct obj *car, struct obj *cdr)
{
  return (__obj_dptr_new(vm, OBJ_TYPE_PAIR, car, cdr));
}

static void
_obj_pair_new(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *car = obj_vm_reg(vm, va_arg(ap, unsigned));
  struct obj *cdr = obj_vm_reg(vm, va_arg(ap, unsigned));

  __obj_dptr_new(vm, OBJ_TYPE_PAIR, car, cdr);
  obj_vm_pop(vm, r1);
}

static void
obj_pair_eq(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1);
  struct obj *q = obj_vm_reg(vm, va_arg(ap, unsigned));
  struct obj **rr;

  rr = __obj_bool_new(vm, 0);

  if (_obj_type(q) == OBJ_TYPE_PAIR) {
    obj_vm_push(vm, 1);
    obj_vm_push(vm, 2);

    _obj_assign(vm, _obj_vm_reg(vm, 1), p->val.dptrval.car);
    _obj_assign(vm, _obj_vm_reg(vm, 2), q->val.dptrval.car);
    obj_vm_method_call(vm, 1, OBJ_OP_EQ, 2);
    if (obj_vm_reg(vm, 1)->val.boolval) {
      _obj_assign(vm, _obj_vm_reg(vm, 1), p->val.dptrval.cdr);
      _obj_assign(vm, _obj_vm_reg(vm, 2), q->val.dptrval.cdr);
      obj_vm_method_call(vm, 1, OBJ_OP_EQ, 2);
      if (obj_vm_reg(vm, 1)->val.boolval) {
	(*rr)->val.boolval = 1;
      }
    }

    obj_vm_pop(vm, 2);
    obj_vm_pop(vm, 1);
  }

  obj_vm_pop(vm, r1);
}

static void
obj_pair_hash(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), **pp;

  pp = __obj_integer_new(vm, 0);
  obj_vm_push(vm, 1);
  obj_vm_push(vm, 2);

  _obj_assign(vm, _obj_vm_reg(vm, 1), p->val.dptrval.car);
  obj_vm_method_call(vm, 1, OBJ_OP_HASH);
  _obj_assign(vm, _obj_vm_reg(vm, 2), p->val.dptrval.cdr);
  obj_vm_method_call(vm, 1, OBJ_OP_HASH);
  (*pp)->val.intval = obj_vm_reg(vm, 1)->val.intval + obj_vm_reg(vm, 2)->val.intval;

  obj_vm_pop(vm, 2);
  obj_vm_pop(vm, 1);
  obj_vm_pop(vm, r1);
}

/***************************************************************************/

static struct obj **
__obj_list_new(struct obj_vm *vm, struct obj *car, struct obj *cdr)
{
  return (__obj_dptr_new(vm, OBJ_TYPE_LIST, car, cdr));
}

static void
_obj_list_new(struct obj_vm *vm, unsigned r1, va_list ap)
{
  __obj_dptr_new(vm, OBJ_TYPE_LIST, obj_vm_reg(vm, va_arg(ap, unsigned)), 0);
  obj_vm_pop(vm, r1);
}

static unsigned
is_list(struct obj *p)
{
  switch (_obj_type(p)) {
  case OBJ_TYPE_NIL:
  case OBJ_TYPE_LIST:
    return (1);
  default:
    ;
  }

  return (0);
}

static unsigned
list_len(struct obj *p)
{
  unsigned result;

  for (result = 0; p; p = p->val.dptrval.cdr)  ++result;

  return (result);
}

static void
obj_list_append(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned)), **pp, **qq;

  switch (_obj_type(q)) {
  case OBJ_TYPE_NIL:
    return;
  case OBJ_TYPE_LIST:
    break;
  default:
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }
  
  pp = __obj_nil_new(vm);
  for ( ; p; p = p->val.dptrval.cdr) {
    qq = __obj_list_new(vm, p->val.dptrval.car, 0);
    _obj_assign(vm, pp, *qq);
    pp = &(*qq)->val.dptrval.cdr;
    obj_vm_drop(vm);
  }
  _obj_assign(vm, pp, q);

  obj_vm_pop(vm, r1);
}

static void
obj_list_at(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj        *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));
  obj_integer_val_t i, j, n;

  if (_obj_type(q) != OBJ_TYPE_INTEGER) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  n = list_len(p);
  if ((i = q->val.intval) < 0) {
    i = -i;

    if (i > n) {
      __obj_nil_new(vm);
    } else {
      for (j = 0, i = n - i; j < i; ++j)  p = p->val.dptrval.cdr;
      _obj_vm_push(vm, p->val.dptrval.car);
    }
  } else {
    if (i >= n) {
      __obj_nil_new(vm);
    } else {
      for (j = 0; j < i; ++j)  p = p->val.dptrval.cdr;
      _obj_vm_push(vm, p->val.dptrval.car);
    }
  }
  obj_vm_pop(vm, r1);
}

static void
obj_list_eq(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, va_arg(ap, unsigned));
  struct obj *q = obj_vm_reg(vm, va_arg(ap, unsigned));
  struct obj **rr;

  rr = __obj_bool_new(vm, 0);

  if (_obj_type(q) == OBJ_TYPE_LIST) {
    obj_vm_push(vm, 1);
    obj_vm_push(vm, 2);

    for ( ; p && q; p = p->val.dptrval.cdr, q = q->val.dptrval.cdr) {
      _obj_assign(vm, _obj_vm_reg(vm, 1), p->val.dptrval.car);
      _obj_assign(vm, _obj_vm_reg(vm, 2), q->val.dptrval.car);
      obj_vm_method_call(vm, 1, OBJ_OP_EQ, 2);
      if (!obj_vm_reg(vm, 1)->val.boolval)  break;
    }
    if (p == 0 && q == 0)  (*rr)->val.boolval = 1;

    obj_vm_pop(vm, 2);
    obj_vm_pop(vm, 1);
  }

  obj_vm_pop(vm, r1);
}

static void
obj_list_hash(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), **pp;

  pp = __obj_integer_new(vm, 0);
  obj_vm_push(vm, 1);

  for ( ; p; p = p->val.dptrval.cdr) {
    _obj_assign(vm, _obj_vm_reg(vm, 1), p->val.dptrval.car);
    obj_vm_method_call(vm, 1, OBJ_OP_HASH);
    (*pp)->val.intval += obj_vm_reg(vm, 1)->val.intval;
  }

  obj_vm_pop(vm, 1);
  obj_vm_pop(vm, r1);
}

static void
obj_list_size(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1);

  __obj_integer_new(vm, list_len(p));
  obj_vm_pop(vm, r1);
}

static void
obj_list_slice(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1);
  struct obj *q = obj_vm_reg(vm, va_arg(ap, unsigned));
  struct obj *r = obj_vm_reg(vm, va_arg(ap, unsigned));
  int        i, j, n;
  struct obj **pp;

  if (_obj_type(q) != OBJ_TYPE_INTEGER || _obj_type(r) != OBJ_TYPE_INTEGER) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  i = q->val.intval;
  n = r->val.intval;
  slice_idxs((int) list_len(p), &i, &n);

  for ( ; i; --i)  p = p->val.dptrval.cdr;
  pp = __obj_nil_new(vm);
  for ( ; n; --n) {
    _obj_assign(vm, pp, *__obj_list_new(vm, p->val.dptrval.car, 0));
    obj_vm_drop(vm);
    
    pp = &(*pp)->val.dptrval.cdr;
  }

  obj_vm_pop(vm, r1);
}

/***************************************************************************/

static struct obj **
__obj_array_new(struct obj_vm *vm, unsigned type, unsigned size)
{
  struct obj **result = _obj_alloc(vm, type);

  (*result)->val.arrayval.size = size;
  (*result)->val.arrayval.data = size ? calloc(size, sizeof((*result)->val.arrayval.data[0])) : 0;

  return (result);
}

static void
_obj_array_new(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  switch (_obj_type(q)) {
  case OBJ_TYPE_NIL:
    __obj_array_new(vm, OBJ_TYPE_ARRAY, 0);
    break;
  case OBJ_TYPE_INTEGER:
    if (q->val.intval < 0) {
      vm->errno = OBJ_ERRNO_BAD_VALUE;
      return;
    }
    __obj_array_new(vm, OBJ_TYPE_ARRAY, q->val.intval);
    break;
  case OBJ_TYPE_LIST:
    {
      unsigned   n;
      struct obj **pp, **qq;

      pp = __obj_array_new(vm, OBJ_TYPE_ARRAY, n = list_len(q));
      for (qq = (*pp)->val.arrayval.data; n; --n, ++qq, q = q->val.dptrval.cdr) {
	_obj_assign(vm, qq, q->val.dptrval.car);
      }
    }
    break;
  case OBJ_TYPE_ARRAY:
    {
      unsigned   n;
      struct obj **pp, **qq, **rr;

      pp = __obj_array_new(vm, OBJ_TYPE_ARRAY, n = q->val.arrayval.size);
      for (qq = (*pp)->val.arrayval.data, rr = q->val.arrayval.data; n; --n, ++rr, ++qq) {
	_obj_assign(vm, qq, *rr);
      }
    }
    break;
  case OBJ_TYPE_DICT:
    {
      unsigned   n;
      struct obj **pp, **qq, **rr, *s;

      pp = __obj_array_new(vm, OBJ_TYPE_ARRAY, q->val.dictval.cnt);
      for (qq = (*pp)->val.arrayval.data, rr = q->val.dictval.base.data, n = q->val.dictval.base.size; n; --n, ++rr, ++qq) {
	for (s = *rr; s; s = s->val.dptrval.cdr) {
	  _obj_assign(vm, qq, s->val.dptrval.car);
	}
      }
    }
    break;
  default:
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  obj_vm_pop(vm, r1);
}

static void
obj_array_append(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned)), **pp, **qq, **rr;
  unsigned   n;
  
  if (_obj_type(q) != OBJ_TYPE_ARRAY) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  pp = __obj_array_new(vm, OBJ_TYPE_ARRAY, p->val.arrayval.size + q->val.arrayval.size);
  
  for (qq = (*pp)->val.arrayval.data, rr = p->val.arrayval.data, n = p->val.arrayval.size; n; --n, ++rr, ++qq) {
    _obj_assign(vm, qq, *rr);
  }
  for (rr = q->val.arrayval.data, n = q->val.arrayval.size; n; --n, ++rr, ++qq) {
    _obj_assign(vm, qq, *rr);
  }

  obj_vm_pop(vm, r1);
}

static void
obj_array_at(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_INTEGER) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  if (q->val.intval < 0 || q->val.intval >= p->val.arrayval.size) {
    vm->errno = OBJ_ERRNO_RANGE;
    return;
  }
  
  _obj_assign(vm, _obj_vm_reg(vm, r1), p->val.arrayval.data[q->val.intval]);
}

static void
obj_array_at_put(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1);
  struct obj *q = obj_vm_reg(vm, va_arg(ap, unsigned));
  struct obj *r = obj_vm_reg(vm, va_arg(ap, unsigned));

  if (_obj_type(q) != OBJ_TYPE_INTEGER) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  if (q->val.intval < 0 || q->val.intval >= p->val.arrayval.size) {
    vm->errno = OBJ_ERRNO_RANGE;
    return;
  }
  
  _obj_assign(vm, &p->val.arrayval.data[q->val.intval], r);
}

static void
obj_array_size(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1);

  __obj_integer_new(vm, p->val.arrayval.size);
  obj_vm_pop(vm, r1);
}

static void
obj_array_slice(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1);
  struct obj *q = obj_vm_reg(vm, va_arg(ap, unsigned));
  struct obj *r = obj_vm_reg(vm, va_arg(ap, unsigned));
  int        i, j, n;
  struct obj **pp, **qq, **rr;

  if (_obj_type(q) != OBJ_TYPE_INTEGER || _obj_type(r) != OBJ_TYPE_INTEGER) {
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  i = q->val.intval;
  n = r->val.intval;
  slice_idxs((int) p->val.arrayval.size, &i, &n);

  pp = __obj_array_new(vm, OBJ_TYPE_ARRAY, n);
  for (rr = (*pp)->val.arrayval.data, qq = &p->val.arrayval.data[i]; n; --n, ++qq, ++rr) {
    _obj_assign(vm, rr, *qq);
  }
  obj_vm_pop(vm, r1);
}

/***************************************************************************/

enum {
  OBJ_DICT_DEFAULT_SIZE = 32
};

static void
_obj_dict_new(struct obj_vm *vm, unsigned r1, va_list ap)
{
#if 0
  /** @todo Complete */

  struct obj *q = obj_vm_reg(vm, va_arg(ap, unsigned));

  switch (_obj_type(q)) {
  case OBJ_TYPE_INTEGER:
    if (q->val.intval < 0) {
      vm->errno = OBJ_ERRNO_BAD_VALUE;
      return;
    }
    __obj_array_new(vm, OBJ_TYPE_DICT, q->val.intval);
    break;
  case OBJ_TYPE_NIL:
  case OBJ_TYPE_LIST:
    {
      for ( ; q && _obj_type(q->val.dptrval.car) == OBJ_TYPE_PAIR; q = q->val.dptrval.cdr);
      if (q == 0) {
      
	pp = __obj_array_new(vm, OBJ_TYPE_DICT, OBJ_DICT_DEFAULT_SIZE);

      }
      
      r = obj_vm_reg(vm, va_arg(ap, unsigned));
      if (_obj_type(r) != OBJ_TYPE_LIST) {

	pp = __obj_array_new(vm, OBJ_TYPE_DICT, OBJ_DICT_DEFAULT_SIZE);



      }

      


    }
    break;
  case OBJ_TYPE_ARRAY:
    break;
  case OBJ_TYPE_DICT:
    break;
  default:
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }

  obj_vm_pop(vm, r1);

#endif
}

static void
_obj_dict_find(struct obj_vm *vm, struct obj *dict, struct obj *key, struct obj ***pbucket, struct obj ***pprev)
{
  struct obj **pp, **qq, **bucket, **rr, *r;

  obj_vm_push(vm, 1);
  obj_vm_push(vm, 2);

  pp = _obj_vm_reg(vm, 1);
  qq = _obj_vm_reg(vm, 2);

  _obj_assign(vm, pp, key);
  obj_vm_method_call(vm, 1, OBJ_OP_HASH);
  bucket = &dict->val.dictval.base.data[(*pp)->val.intval % dict->val.dictval.base.size];
  if (pbucket)  *pbucket = bucket;

  *pprev = 0;
  for (rr = bucket; r = *rr; rr = &r->val.dptrval.cdr) {
    _obj_assign(vm, pp, r->val.dptrval.car->val.dptrval.car);
    _obj_assign(vm, qq, key);
    obj_vm_method_call(vm, 1, OBJ_OP_EQ, 2);
    if ((*pp)->val.boolval) {
      *pprev = rr;
      break;
    }
  }

  obj_vm_pop(vm, 2);
  obj_vm_pop(vm, 1);
}

static void
obj_dict_append(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p =obj_vm_reg(vm, r1), *q = obj_vm_reg(vm, va_arg(ap, unsigned));
  struct obj **qq, *r;
  unsigned   n;

  switch (_obj_type(q)) {
  case OBJ_TYPE_DICT:
    obj_vm_push(vm, 1);
    obj_vm_push(vm, 2);
    obj_vm_push(vm, 3);

    _obj_assign(vm, _obj_vm_reg(vm, 1), p);
    for (qq = q->val.dictval.base.data, n = q->val.dictval.base.size; n; --n, ++q) {
      for (r = *qq; r; r = r->val.dptrval.cdr) {
	_obj_assign(vm, _obj_vm_reg(vm, 2), r->val.dptrval.car->val.dptrval.car);
	_obj_assign(vm, _obj_vm_reg(vm, 3), r->val.dptrval.car->val.dptrval.cdr);
	obj_vm_method_call(vm, 1, OBJ_OP_AT_PUT, 2, 3);
      }
    }

    obj_vm_pop(vm, 3);
    obj_vm_pop(vm, 2);
    obj_vm_pop(vm, 1); 

    break;
  default:
    vm->errno = OBJ_ERRNO_BAD_TYPE;
    return;
  }
}

static void
obj_dict_at(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj **pp = _obj_vm_reg(vm, r1), **qq = _obj_vm_reg(vm, va_arg(ap, unsigned)), **prev;

  _obj_dict_find(vm, *pp, *qq, 0, &prev);
  _obj_assign(vm, pp, prev ? (*prev)->val.dptrval.car : 0);
}

static void
obj_dict_at_put(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj **pp = _obj_vm_reg(vm, r1);
  struct obj **qq = _obj_vm_reg(vm, va_arg(ap, unsigned));
  struct obj **rr = _obj_vm_reg(vm, va_arg(ap, unsigned));
  struct obj **bucket, **prev;

  _obj_dict_find(vm, *pp, *qq, &bucket, &prev);
  if (prev) {
    _obj_assign(vm, &(*prev)->val.dptrval.car->val.dptrval.cdr, *rr);
  } else {
    pp = __obj_pair_new(vm, *qq, *rr);
    qq = __obj_list_new(vm, *pp, *bucket);
    _obj_assign(vm, bucket, *qq);
    
    obj_vm_dropn(vm, 2);

    ++obj_vm_reg(vm, r1)->val.dictval.cnt;
  }
}

static void
obj_dict_count(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p = obj_vm_reg(vm, r1);

  __obj_integer_new(vm, p->val.dictval.cnt);
  obj_vm_pop(vm, r1);
}

static void
obj_dict_del(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj **pp = _obj_vm_reg(vm, r1), **qq = _obj_vm_reg(vm, va_arg(ap, unsigned)), **prev;

  _obj_dict_find(vm, *pp, *qq, 0, &prev);
  if (prev) {
    _obj_assign(vm, prev, (*prev)->val.dptrval.cdr);

    --obj_vm_reg(vm, r1)->val.dictval.cnt;
  }
}

static void
obj_dict_keys(struct obj_vm *vm, unsigned r1, va_list ap)
{
  struct obj *p, **q, *r, **pp, **qq;
  unsigned   n;

  pp = __obj_nil_new(vm);
  p  = obj_vm_reg(vm, r1);
  for (q = p->val.arrayval.data, n = p->val.arrayval.size; n; --n, ++q) {
    for (r = *q; r; r = r->val.dptrval.cdr) {
      qq = __obj_list_new(vm, r->val.dptrval.car->val.dptrval.car, *pp);
      _obj_assign(vm, pp, *qq);
      obj_vm_drop(vm);
    }
  }
  
  obj_vm_pop(vm, r1);
}

/***************************************************************************/

static void (* const op_func_tbl[OBJ_NUM_TYPES][OBJ_NUM_OPS])(struct obj_vm *, unsigned, va_list) = {
  /* OBJ_TYPE_OBJECT */
  { 0
  },

  /* OBJ_TYPE_NIL */
  { 0,				/* OBJ_OP_ABS */
    0,				/* OBJ_OP_ADD */
    0,				/* OBJ_OP_AND */
    obj_nil_append,		/* OBJ_OP_APPEND */
    obj_nil_at,			/* OBJ_OP_AT */
    0,				/* OBJ_OP_AT_PUT */
    0,				/* OBJ_OP_CAR */
    0,				/* OBJ_OP_CDR */
    0,				/* OBJ_OP_COUNT */
    0,				/* OBJ_OP_DEL */
    0,				/* OBJ_OP_DIV */
    obj_nil_eq,			/* OBJ_OP_EQ */
    0,				/* OBJ_OP_GT */
    obj_nil_hash,		/* OBJ_OP_HASH */
    0,				/* OBJ_OP_KEYS */
    0,				/* OBJ_OP_LT */
    0,				/* OBJ_OP_MINUS */
    0,				/* OBJ_OP_MOD */
    0,				/* OBJ_OP_MULT */
    0,				/* OBJ_OP_NOT */
    0,				/* OBJ_OP_OR */
    obj_nil_size,		/* OBJ_OP_SIZE */
    obj_nil_slice,		/* OBJ_OP_SLICE */
    0				/* OBJ_OP_SUB */
  },

  /* OBJ_TYPE_BOOLEAN */
  { 0,				/* OBJ_OP_ABS */
    0,				/* OBJ_OP_ADD */
    obj_bool_and,		/* OBJ_OP_AND */
    0,				/* OBJ_OP_APPEND */
    0,				/* OBJ_OP_AT */
    0,				/* OBJ_OP_AT_PUT */
    0,				/* OBJ_OP_CAR */
    0,				/* OBJ_OP_CDR */
    0,				/* OBJ_OP_COUNT */
    0,				/* OBJ_OP_DEL */
    0,				/* OBJ_OP_DIV */
    obj_bool_eq,		/* OBJ_OP_EQ */
    0,				/* OBJ_OP_GT */
    obj_bool_hash,		/* OBJ_OP_HASH */
    0,				/* OBJ_OP_KEYS */
    0,				/* OBJ_OP_LT */
    0,				/* OBJ_OP_MINUS */
    0,				/* OBJ_OP_MOD */
    0,				/* OBJ_OP_MULT */
    obj_bool_not,		/* OBJ_OP_NOT */
    obj_bool_or,		/* OBJ_OP_OR */
    0,				/* OBJ_OP_SIZE */
    0,				/* OBJ_OP_SLICE */
    0				/* OBJ_OP_SUB */
  },

  /* OBJ_TYPE_NUMBER */
  { 0
  },

  /* OBJ_TYPE_INTEGER */
  { 0,				/* OBJ_OP_ABS */
    obj_integer_add,		/* OBJ_OP_ADD */
    obj_integer_and,		/* OBJ_OP_AND */
    0,				/* OBJ_OP_APPEND */
    0,				/* OBJ_OP_AT */
    0,				/* OBJ_OP_AT_PUT */
    0,				/* OBJ_OP_CAR */
    0,				/* OBJ_OP_CDR */
    0,				/* OBJ_OP_COUNT */
    0,				/* OBJ_OP_DEL */
    obj_integer_div,		/* OBJ_OP_DIV */
    obj_integer_eq,		/* OBJ_OP_EQ */
    obj_integer_gt,		/* OBJ_OP_GT */
    obj_integer_hash,		/* OBJ_OP_HASH */
    0,				/* OBJ_OP_KEYS */
    obj_integer_lt,		/* OBJ_OP_LT */
    obj_integer_minus,		/* OBJ_OP_MINUS */
    obj_integer_mod,		/* OBJ_OP_MOD */
    obj_integer_mult,		/* OBJ_OP_MULT */
    0,				/* OBJ_OP_NOT */
    obj_integer_or,		/* OBJ_OP_OR */
    0,				/* OBJ_OP_SIZE */
    0,				/* OBJ_OP_SLICE */
    obj_integer_sub		/* OBJ_OP_SUB */
  },

  /* OBJ_TYPE_FLOAT */
  { 0,				/* OBJ_OP_ABS */
    obj_float_add,		/* OBJ_OP_ADD */
    0,				/* OBJ_OP_AND */
    0,				/* OBJ_OP_APPEND */
    0,				/* OBJ_OP_AT */
    0,				/* OBJ_OP_AT_PUT */
    0,				/* OBJ_OP_CAR */
    0,				/* OBJ_OP_CDR */
    0,				/* OBJ_OP_COUNT */
    0,				/* OBJ_OP_DEL */
    obj_float_div,		/* OBJ_OP_DIV */
    obj_float_eq,		/* OBJ_OP_EQ */
    obj_float_gt,		/* OBJ_OP_GT */
    obj_float_hash,		/* OBJ_OP_HASH */
    0,				/* OBJ_OP_KEYS */
    obj_float_lt,		/* OBJ_OP_LT */
    obj_float_minus,		/* OBJ_OP_MINUS */
    0,				/* OBJ_OP_MOD */
    obj_float_mult,		/* OBJ_OP_MULT */
    0,				/* OBJ_OP_NOT */
    0,				/* OBJ_OP_OR */
    0,				/* OBJ_OP_SIZE */
    0,				/* OBJ_OP_SLICE */
    obj_float_sub		/* OBJ_OP_SUB */
  },

  /* OBJ_TYPE_BLOCK */
  { 0
  },

  /* OBJ_TYPE_STRING */
  { 0,				/* OBJ_OP_ABS */
    0,				/* OBJ_OP_ADD */
    0,				/* OBJ_OP_AND */
    obj_string_append,		/* OBJ_OP_APPEND */
    obj_string_at,		/* OBJ_OP_AT */
    0,				/* OBJ_OP_AT_PUT */
    0,				/* OBJ_OP_CAR */
    0,				/* OBJ_OP_CDR */
    0,				/* OBJ_OP_COUNT */
    0,				/* OBJ_OP_DEL */
    0,				/* OBJ_OP_DIV */
    obj_string_eq,		/* OBJ_OP_EQ */
    obj_string_gt,		/* OBJ_OP_GT */
    obj_string_hash,		/* OBJ_OP_HASH */
    0,				/* OBJ_OP_KEYS */
    obj_string_lt,		/* OBJ_OP_LT */
    0,				/* OBJ_OP_MINUS */
    0,				/* OBJ_OP_MOD */
    0,				/* OBJ_OP_MULT */
    0,				/* OBJ_OP_NOT */
    0,				/* OBJ_OP_OR */
    obj_string_size,		/* OBJ_OP_SIZE */
    obj_string_slice,		/* OBJ_OP_SLICE */
    0				/* OBJ_OP_SUB */
  },

  /* OBJ_TYPE_BYTES */
  { 0
  },

  /* OBJ_TYPE_WORDS */
  { 0
  },

  /* OBJ_TYPE_DWORDS */
  { 0
  },

  /* OBJ_TYPE_QWORDS */
  { 0
  },

  /* OBJ_TYPE_BITS */
  { 0
  },

  /* OBJ_TYPE_DPTR */
  { 0,				/* OBJ_OP_ABS */
    0,				/* OBJ_OP_ADD */
    0,				/* OBJ_OP_AND */
    0,				/* OBJ_OP_APPEND */
    0,				/* OBJ_OP_AT */
    0,				/* OBJ_OP_AT_PUT */
    obj_dptr_car,		/* OBJ_OP_CAR */
    obj_dptr_cdr,		/* OBJ_OP_CDR */
    0,				/* OBJ_OP_COUNT */
    0,				/* OBJ_OP_DEL */
    0,				/* OBJ_OP_DIV */
    0,				/* OBJ_OP_EQ */
    0,				/* OBJ_OP_GT */
    0,				/* OBJ_OP_HASH */
    0,				/* OBJ_OP_KEYS */
    0,				/* OBJ_OP_LT */
    0,				/* OBJ_OP_MINUS */
    0,				/* OBJ_OP_MOD */
    0,				/* OBJ_OP_MULT */
    0,				/* OBJ_OP_NOT */
    0,				/* OBJ_OP_OR */
    0,				/* OBJ_OP_SIZE */
    0,				/* OBJ_OP_SLICE */
    0				/* OBJ_OP_SUB */
  },
  
  /* OBJ_TYPE_PAIR */
  { 0,				/* OBJ_OP_ABS */
    0,				/* OBJ_OP_ADD */
    0,				/* OBJ_OP_AND */
    0,				/* OBJ_OP_APPEND */
    0,				/* OBJ_OP_AT */
    0,				/* OBJ_OP_AT_PUT */
    0,				/* OBJ_OP_CAR */
    0,				/* OBJ_OP_CDR */
    0,				/* OBJ_OP_COUNT */
    0,				/* OBJ_OP_DEL */
    0,				/* OBJ_OP_DIV */
    obj_pair_eq,		/* OBJ_OP_EQ */
    0,				/* OBJ_OP_GT */
    obj_pair_hash,		/* OBJ_OP_HASH */
    0,				/* OBJ_OP_KEYS */
    0,				/* OBJ_OP_LT */
    0,				/* OBJ_OP_MINUS */
    0,				/* OBJ_OP_MOD */
    0,				/* OBJ_OP_MULT */
    0,				/* OBJ_OP_NOT */
    0,				/* OBJ_OP_OR */
    0,				/* OBJ_OP_SIZE */
    0,				/* OBJ_OP_SLICE */
    0				/* OBJ_OP_SUB */
  },
  
  /* OBJ_TYPE_LIST */
  { 0,				/* OBJ_OP_ABS */
    0,				/* OBJ_OP_ADD */
    0,				/* OBJ_OP_AND */
    obj_list_append,		/* OBJ_OP_APPEND */
    obj_list_at,		/* OBJ_OP_AT */
    0,				/* OBJ_OP_AT_PUT */
    0,				/* OBJ_OP_CAR */
    0,				/* OBJ_OP_CDR */
    0,				/* OBJ_OP_COUNT */
    0,				/* OBJ_OP_DEL */
    0,				/* OBJ_OP_DIV */
    obj_list_eq,		/* OBJ_OP_EQ */
    0,				/* OBJ_OP_GT */
    obj_list_hash,		/* OBJ_OP_HASH */
    0,				/* OBJ_OP_KEYS */
    0,				/* OBJ_OP_LT */
    0,				/* OBJ_OP_MINUS */
    0,				/* OBJ_OP_MOD */
    0,				/* OBJ_OP_MULT */
    0,				/* OBJ_OP_NOT */
    0,				/* OBJ_OP_OR */
    obj_list_size,		/* OBJ_OP_SIZE */
    obj_list_slice,		/* OBJ_OP_SLICE */
    0				/* OBJ_OP_SUB */
  },
  
  /* OBJ_TYPE_ARRAY */
  { 0,				/* OBJ_OP_ABS */
    0,				/* OBJ_OP_ADD */
    0,				/* OBJ_OP_AND */
    obj_array_append,		/* OBJ_OP_APPEND */
    obj_array_at,		/* OBJ_OP_AT */
    obj_array_at_put,		/* OBJ_OP_AT_PUT */
    0,				/* OBJ_OP_CAR */
    0,				/* OBJ_OP_CDR */
    0,				/* OBJ_OP_COUNT */
    0,				/* OBJ_OP_DEL */
    0,				/* OBJ_OP_DIV */
    0,				/* OBJ_OP_EQ */
    0,				/* OBJ_OP_GT */
    0,				/* OBJ_OP_HASH */
    0,				/* OBJ_OP_KEYS */
    0,				/* OBJ_OP_LT */
    0,				/* OBJ_OP_MINUS */
    0,				/* OBJ_OP_MOD */
    0,				/* OBJ_OP_MULT */
    0,				/* OBJ_OP_NOT */
    0,				/* OBJ_OP_OR */
    obj_array_size,		/* OBJ_OP_SIZE */
    obj_array_slice,		/* OBJ_OP_SLICE */
    0				/* OBJ_OP_SUB */
  },
  
  /* OBJ_TYPE_DICT */
  { 0,				/* OBJ_OP_ABS */
    0,				/* OBJ_OP_ADD */
    0,				/* OBJ_OP_AND */
    obj_dict_append,		/* OBJ_OP_APPEND */
    obj_dict_at,		/* OBJ_OP_AT */
    obj_dict_at_put,		/* OBJ_OP_AT_PUT */
    0,				/* OBJ_OP_CAR */
    0,				/* OBJ_OP_CDR */
    obj_dict_count,		/* OBJ_OP_COUNT */
    obj_dict_del,		/* OBJ_OP_DEL */
    0,				/* OBJ_OP_DIV */
    0,				/* OBJ_OP_EQ */
    0,				/* OBJ_OP_GT */
    0,				/* OBJ_OP_HASH */
    obj_dict_keys,		/* OBJ_OP_KEYS */
    0,				/* OBJ_OP_LT */
    0,				/* OBJ_OP_MINUS */
    0,				/* OBJ_OP_MOD */
    0,				/* OBJ_OP_MULT */
    0,				/* OBJ_OP_NOT */
    0,				/* OBJ_OP_OR */
    0,				/* OBJ_OP_SIZE */
    obj_bad_method,		/* OBJ_OP_SLICE */
    0				/* OBJ_OP_SUB */
  }
};

/***************************************************************************/

void
obj_vm_newc(struct obj_vm *vm, unsigned r1, unsigned type, ...)
{
  va_list ap;

  if (vm->errno != OBJ_ERRNO_NONE)  return;

  va_start(ap, type);

  switch (type) {
  case OBJ_TYPE_BOOLEAN:
    __obj_bool_new(vm, va_arg(ap, unsigned));
    break;
  case OBJ_TYPE_INTEGER:
    __obj_integer_new(vm, va_arg(ap, obj_integer_val_t));
    break;
  case OBJ_TYPE_FLOAT:
    __obj_float_new(vm, va_arg(ap, obj_float_val_t));
    break;
  case OBJ_TYPE_STRING:
    {
      char *s = va_arg(ap, char *);
      
      __obj_string_new(vm, 1, strlen(s), s);
    }
    break;
  case OBJ_TYPE_ARRAY:
  case OBJ_TYPE_DICT:
    __obj_array_new(vm, type, va_arg(ap, unsigned));
    break;
  default:
    assert(0);
  }

  obj_vm_pop(vm, r1);

  va_end(ap);
}

void
obj_vm_new(struct obj_vm *vm, unsigned r1, unsigned type, ...)
{
  va_list ap;

  if (vm->errno != OBJ_ERRNO_NONE)  return;

  va_start(ap, type);

  switch (type) {
  case OBJ_TYPE_NIL:
    _obj_nil_new(vm, r1, ap);
    break;
  case OBJ_TYPE_BOOLEAN:
    _obj_bool_new(vm, r1, ap);
    break;
  case OBJ_TYPE_INTEGER:
    _obj_integer_new(vm, r1, ap);
    break;
  case OBJ_TYPE_FLOAT:
    _obj_float_new(vm, r1, ap);
    break;
  case OBJ_TYPE_STRING:
    _obj_string_new(vm, r1, ap);
    break;
  case OBJ_TYPE_PAIR:
    _obj_pair_new(vm, r1, ap);
    break;
  case OBJ_TYPE_LIST:
    _obj_list_new(vm, r1, ap);
    break;
  case OBJ_TYPE_ARRAY:
    _obj_array_new(vm, r1, ap);
    break;
  case OBJ_TYPE_DICT:
    _obj_dict_new(vm, r1, ap);
    break;
  default:
    assert(0);
  }

  va_end(ap);
}

void
obj_vm_method_call(struct obj_vm *vm, unsigned r1, unsigned op, ...)
{
  va_list  ap;
  unsigned type;
  void     (*f)(struct obj_vm *, unsigned, va_list) = 0;

  if (vm->errno != OBJ_ERRNO_NONE)  return;

  va_start(ap, op);

  for (type = vm->reg[r1]->type; type != OBJ_TYPE_OBJECT; type = _obj_type_parent(type)) {
    if (f = op_func_tbl[type - OBJ_TYPE_BASE][op])  break;
  }
  if (f == 0) {
    vm->errno = OBJ_ERRNO_BAD_METHOD;
  } else {
    (*f)(vm, r1, ap);
  }
  
  va_end(ap);
}

#define R0  0
#define R1  1
#define R2  2
#define R3  3
#define R4  4
#define R5  5
#define R6  6
#define R7  7


void
obj_set_new(struct obj_vm *vm)
{
  obj_vm_newc(vm, R0, OBJ_TYPE_DICT, 32);
}

void
obj_set_insert(struct obj_vm *vm)
{
  obj_vm_push(vm, R1);
  obj_vm_push(vm, R2);
  obj_vm_push(vm, R3);

  obj_vm_pick(vm, R1, 3);
  obj_vm_pick(vm, R2, 4);
  obj_vm_new(vm, R3, OBJ_TYPE_NIL);

  obj_vm_method_call(vm, R1, OBJ_OP_AT_PUT, R2, R3);

  obj_vm_pop(vm, R3);
  obj_vm_pop(vm, R2);
  obj_vm_pop(vm, R1);
  
  obj_vm_dropn(vm, 2);
}

void
obj_set_del(struct obj_vm *vm)
{
  obj_vm_push(vm, R1);
  obj_vm_push(vm, R2);

  obj_vm_pick(vm, R1, 2);
  obj_vm_pick(vm, R2, 3);

  obj_vm_method_call(vm, R1, OBJ_OP_DEL, R2);

  obj_vm_pop(vm, R2);
  obj_vm_pop(vm, R1);
  
  obj_vm_dropn(vm, 2);
}

void
obj_set_member(struct obj_vm *vm)
{
  obj_vm_push(vm, R1);
  obj_vm_push(vm, R2);

  obj_vm_pick(vm, R0, 2);
  obj_vm_pick(vm, R1, 3);
  obj_vm_new(vm, R2, OBJ_TYPE_NIL);

  obj_vm_method_call(vm, R0, OBJ_OP_AT, R1);
  obj_vm_method_call(vm, R0, OBJ_OP_EQ, R2);
  obj_vm_method_call(vm, R0, OBJ_OP_NOT);

  obj_vm_pop(vm, R2);
  obj_vm_pop(vm, R1);
  
  obj_vm_dropn(vm, 2);
}

void
obj_set_members(struct obj_vm *vm)
{
  obj_vm_pop(vm, R0);
  obj_vm_method_call(vm, R0, OBJ_OP_KEYS);
}

/*
 * list item -- list
 */

void
obj_list_cons(struct obj_vm *vm)
{
  obj_vm_push(vm, R1);
  obj_vm_push(vm, R2);

  obj_vm_pick(vm, R0, 2);
  obj_vm_pick(vm, R1, 3);
  obj_vm_new(vm, R0, OBJ_TYPE_LIST, R0);
  obj_vm_method_call(vm, R0, OBJ_OP_APPEND, R1);

  obj_vm_pop(vm, R2);
  obj_vm_pop(vm, R1);
  
  obj_vm_dropn(vm, 2);
}

void
obj_fprint(struct obj_vm *vm, FILE *fp)
{
  obj_vm_push(vm, R1);

  obj_vm_pick(vm, R1, 1);
  obj_vm_new(vm, R1, OBJ_TYPE_STRING, R1);
  fprintf(fp, "%s", obj_string_val(vm, R1));

  obj_vm_pop(vm, R1);

  obj_vm_drop(vm);
}

void
obj_print(struct obj_vm *vm)
{
  obj_fprint(vm, stdout);
}

int
main(void)
{
  struct obj_vm vm[1];

  obj_vm_init(vm, 1000, 1000);

#if 0
  obj_vm_newc(vm, R1, OBJ_TYPE_STRING, "foo");
  obj_vm_newc(vm, R2, OBJ_TYPE_INTEGER, 42LL);
  obj_vm_new(vm, R2, OBJ_TYPE_LIST, R2);

  obj_vm_push(vm, R2);
  obj_vm_push(vm, R1);
  obj_list_cons(vm);

  obj_vm_push(vm, R0);  obj_print(vm);  printf("\n");
#endif

#if 0
  obj_set_new(vm);
  obj_vm_move(vm, R1, R0);

  obj_vm_newc(vm, R2, OBJ_TYPE_STRING, "foo");
  obj_vm_push(vm, R2);
  obj_vm_push(vm, R1);
  obj_set_insert(vm);

  obj_vm_push(vm, R2);
  obj_vm_push(vm, R1);
  obj_set_member(vm);

  obj_vm_push(vm, R0);  obj_print(vm);  printf("\n");

  obj_vm_newc(vm, R2, OBJ_TYPE_STRING, "sam");
  obj_vm_push(vm, R2);
  obj_vm_push(vm, R1);
  obj_set_insert(vm);

  obj_vm_newc(vm, R2, OBJ_TYPE_STRING, "foo");
  obj_vm_push(vm, R2);
  obj_vm_push(vm, R1);
  obj_set_insert(vm);

  obj_vm_newc(vm, R2, OBJ_TYPE_STRING, "bar");
  obj_vm_push(vm, R2);
  obj_vm_push(vm, R1);
  obj_set_insert(vm);

  obj_vm_push(vm, R1);
  obj_set_members(vm);

  obj_vm_push(vm, R0);  obj_print();

  obj_vm_push(vm, R1);
  obj_set_members(vm);
  obj_vm_newc(vm, R3, OBJ_TYPE_INTEGER, 1);
  obj_vm_newc(vm, R4, OBJ_TYPE_INTEGER, 1);
  obj_vm_method_call(vm, R0, OBJ_OP_SLICE, R3, R4);

  obj_vm_push(vm, R0);  obj_print();
#endif

#if 0
  obj_vm_newc(vm, R0, OBJ_TYPE_INTEGER, (obj_integer_val_t) 0);
  obj_vm_newc(vm, R1, OBJ_TYPE_INTEGER, (obj_integer_val_t) 10000000);
  obj_vm_newc(vm, R2, OBJ_TYPE_INTEGER, (obj_integer_val_t) 1);
  for (;;) {
    obj_vm_move(vm, R3, R0);
    obj_vm_method_call(vm, R3, OBJ_OP_LT, R1);
    if (!obj_bool_val(vm, R3))  break;
    obj_vm_method_call(vm, R0, OBJ_OP_ADD, R2);
  }
#endif

#if 1
  obj_vm_newc(vm, R0, OBJ_TYPE_INTEGER, (obj_integer_val_t) 1234);
  obj_vm_newc(vm, R1, OBJ_TYPE_INTEGER, (obj_integer_val_t) 5678);
  obj_vm_method_call(vm, R0, OBJ_OP_ADD, R1);
  obj_vm_new(vm, R1, OBJ_TYPE_STRING, R0);

  obj_vm_new(vm, R4, OBJ_TYPE_PAIR, R0, R1);
  obj_vm_move(vm, R5, R4);
  obj_vm_method_call(vm, R5, OBJ_OP_CAR);

  printf("%lld", obj_integer_val(vm, R5));
  printf("%s", obj_string_val(vm, R1));

  obj_vm_newc(vm, R0, OBJ_TYPE_INTEGER, (obj_integer_val_t) 0);
  obj_vm_newc(vm, R1, OBJ_TYPE_INTEGER, (obj_integer_val_t) 10);
  obj_vm_newc(vm, R2, OBJ_TYPE_INTEGER, (obj_integer_val_t) 1);
  for (;;) {
    obj_vm_move(vm, R3, R0);
    obj_vm_method_call(vm, R3, OBJ_OP_LT, R1);
    if (!obj_bool_val(vm, R3))  break;

    obj_vm_push(vm, R0);  obj_print(vm);  printf("\n");
    
    obj_vm_method_call(vm, R0, OBJ_OP_ADD, R2);
  }

  obj_vm_newc(vm, R1, OBJ_TYPE_DICT, (obj_integer_val_t) 32);

  obj_vm_newc(vm, R2, OBJ_TYPE_STRING, "sam");
  obj_vm_newc(vm, R3, OBJ_TYPE_FLOAT, (obj_float_val_t) 3.14);
  obj_vm_method_call(vm, R1, OBJ_OP_AT_PUT, R2, R3);

  obj_vm_newc(vm, R2, OBJ_TYPE_STRING, "foo");
  obj_vm_newc(vm, R3, OBJ_TYPE_INTEGER, (obj_integer_val_t) 42);
  obj_vm_method_call(vm, R1, OBJ_OP_AT_PUT, R2, R3);

  obj_vm_newc(vm, R2, OBJ_TYPE_STRING, "bar");
  obj_vm_newc(vm, R3, OBJ_TYPE_STRING, "xxx");
  obj_vm_method_call(vm, R1, OBJ_OP_AT_PUT, R2, R3);
  
  obj_vm_push(vm, R1);  obj_print(vm);  printf("\n");

  obj_vm_move(vm, R4, R1);
  obj_vm_method_call(vm, R4, OBJ_OP_KEYS);

  obj_vm_push(vm, R4);  obj_print(vm);  printf("\n");

  obj_vm_move(vm, R2, R1);
  obj_vm_newc(vm, R3, OBJ_TYPE_STRING, "foo");
  obj_vm_method_call(vm, R2, OBJ_OP_AT, R3);

  obj_vm_push(vm, R2);  obj_print(vm);  printf("\n");

  obj_vm_move(vm, R2, R1);
  obj_vm_newc(vm, R3, OBJ_TYPE_STRING, "sam");
  obj_vm_method_call(vm, R2, OBJ_OP_AT, R3);

  obj_vm_push(vm, R2);  obj_print(vm);  printf("\n");

  obj_vm_move(vm, R2, R1);
  obj_vm_newc(vm, R3, OBJ_TYPE_STRING, "gorp");
  obj_vm_method_call(vm, R2, OBJ_OP_AT, R3);
  assert(obj_vm_type(vm, R2) == OBJ_TYPE_NIL);

  obj_vm_newc(vm, R2, OBJ_TYPE_STRING, "foo");
  obj_vm_method_call(vm, R1, OBJ_OP_DEL, R2);

  obj_vm_push(vm, R1);  obj_print(vm);  printf("\n");

  obj_vm_newc(vm, R1, OBJ_TYPE_STRING, "The cat in the hat");
  obj_vm_newc(vm, R2, OBJ_TYPE_INTEGER, -5);
  obj_vm_newc(vm, R3, OBJ_TYPE_INTEGER, -5);
  obj_vm_move(vm, R4, R1);
  obj_vm_method_call(vm, R4, OBJ_OP_SLICE, R2, R3);

  obj_vm_push(vm, R4);  obj_print(vm);  printf("\n");
#endif
  
#if 0
  obj_vm_mode(vm, R4, R1);
  obj_vm_method_call(vm, R4, OBJ_OP_AT, R2);
  obj_vm_move(vm, R2, R1);
  obj_vm_method_call(vm, R2, OBJ_OP_CAR);

  obj_vm_push(vm, R2);  obj_print(vm);  printf("\n");

  obj_vm_move(vm, R2, R1);
  obj_vm_method_call(vm, R2, OBJ_OP_CDR);

  obj_vm_push(vm, R2);  obj_print(vm);  printf("\n");
#endif
  
  return (0);  
}
