#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>

#include "ovm.h"

#define _ARRAY_SIZE(a)  (sizeof(a) / sizeof((a)[0]))

#define FIELD_OFS(s, f)                   ((int) &((s *) 0)->f)
#define FIELD_PTR_TO_STRUCT_PTR(p, s, f)  ((s *)((char *)(p) - FIELD_OFS(s, f)))


static struct _list *
list_init(struct _list *li)
{
  return (li->prev = li->next = li);
}

static unsigned
list_empty(struct _list *li)
{
  return (li->next == li);
}

static struct _list *
list_end(struct _list *li)
{
  return (li);
}

static struct _list *
list_first(struct _list *li)
{
  return (li->next);
}

static struct _list *
list_last(struct _list *li)
{
  return (li->prev);
}

static struct _list *
list_prev(struct _list *nd)
{
  return (nd->prev);
}

static struct _list *
list_next(struct _list *nd)
{
  return (nd->next);
}

static struct _list *
list_insert(struct _list *nd, struct _list *before)
{
  struct _list *p = before->prev;

  nd->prev = p;
  nd->next = before;

  return (p->next = before->prev = nd);
}

static struct _list *
list_erase(struct _list *nd)
{
  struct _list *p = nd->prev, *q = nd->next;

  p->next = q;
  q->prev = p;

  nd->prev = nd->next = 0;

  return (nd);
}

#define R(x)  (vm->reg[x])

static unsigned
obj_type(struct obj *obj)
{
  return (obj ? obj->type : OBJ_TYPE_NIL);
}

static unsigned
is_list(struct obj *p)
{
  switch (obj_type(p)) {
  case OBJ_TYPE_NIL:
  case OBJ_TYPE_LIST:
    return (1);
  default:
    ;
  }

  return (0);
}

static unsigned
obj_type_parent(unsigned type)
{
  switch (type) {
  case OBJ_TYPE_NIL:
  case OBJ_TYPE_POINTER:
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

static void obj_nil_newc(struct ovm *vm, struct obj **pp);
static void obj_bool_newc(struct ovm *vm, struct obj **pp, unsigned val);
static void obj_integer_newc(struct ovm *vm, struct obj **pp, obj_integer_val_t val);
static void obj_float_newc(struct ovm *vm, struct obj **pp, obj_float_val_t val);
static void obj_string_newc(struct ovm *vm, struct obj **pp, unsigned n, ...);
static void obj_string_newv(struct ovm *vm, struct obj **pp, struct obj *arr);
static void obj_pair_newc(struct ovm *vm, struct obj **pp, struct obj *car, struct obj *cdr);
static void obj_list_newc(struct ovm *vm, struct obj **pp, struct obj *car, struct obj *cdr);
static void obj_array_newc(struct ovm *vm, struct obj **pp, unsigned size);
static void obj_dict_newc(struct ovm *vm, struct obj **pp, unsigned size);

static void obj_free(struct ovm *vm, struct obj *obj);

static void
ovm_error(struct ovm *vm, int errno)
{
  vm->errno = errno;
  if (vm->err_hook)  (*vm->err_hook)(vm);
}

static struct obj *
obj_retain(struct obj *obj)
{
  if (obj)  ++obj->ref_cnt;

  return (obj);
}

static void obj_release(struct ovm *vm, struct obj *obj);

static void
obj_free_block(struct ovm *vm, struct obj *obj)
{
  free(obj->val.blockval.ptr);

  obj->val.blockval.size = 0;
  obj->val.blockval.ptr  = 0;
}

static void
obj_free_dptr(struct ovm *vm, struct obj *obj)
{
  obj_release(vm, CAR(obj));
  obj_release(vm, CDR(obj));

  CAR(obj) = 0;
  CDR(obj) = 0;
}

static void
obj_free_array(struct ovm *vm, struct obj *obj)
{
  struct obj **p;
  unsigned   n;
  
  for (p = ARRAY_DATA(obj), n = ARRAY_SIZE(obj); n; --n, ++p) {
    obj_release(vm, *p);
  }
}

static void
obj_free(struct ovm *vm, struct obj *obj)
{
  unsigned type;

  for (type = obj_type(obj); type != OBJ_TYPE_OBJECT; type = obj_type_parent(type)) {
    switch (type) {
    case OBJ_TYPE_BOOLEAN:
    case OBJ_TYPE_INTEGER:
    case OBJ_TYPE_FLOAT:
    case OBJ_TYPE_PAIR:
    case OBJ_TYPE_LIST:
    case OBJ_TYPE_DICT:
      break;
    case OBJ_TYPE_BLOCK:
      obj_free_block(vm, obj);
      break;
    case OBJ_TYPE_DPTR:
      obj_free_dptr(vm, obj);
      break;
    case OBJ_TYPE_ARRAY:
      obj_free_array(vm, obj);
      break;
    default:
      ;
    }
  }

  list_erase(obj->_list_node);

  list_insert(obj->_list_node, &vm->obj_list._list[vm->obj_list.idx_free]);
}

static void
obj_release(struct ovm *vm, struct obj *obj)
{
  if (obj == 0)  return;

  assert(obj->ref_cnt != 0);

  if (--obj->ref_cnt != 0)  return;

  obj_free(vm, obj);
}

static void
obj_assign(struct ovm *vm, struct obj **p, struct obj *obj)
{
  struct obj *temp;

  temp = *p;
  obj_retain(*p = obj);
  obj_release(vm, temp);
}

static struct obj **
_ovm_reg(struct ovm *vm, unsigned reg)
{
  assert(reg < _ARRAY_SIZE(vm->reg));

  return (&vm->reg[reg]);
}

static struct obj **
_ovm_pick(struct ovm *vm, unsigned ofs)
{
  struct obj **result = &vm->sp[ofs];

  assert(result < vm->stack_end);

  return (result);
}

static void
obj_alloc(struct ovm *vm, struct obj **pp, unsigned type)
{
  struct obj *q;

  assert(type >= OBJ_TYPE_BASE && type < OBJ_TYPE_LAST);

  switch (type) {
  case OBJ_TYPE_OBJECT:
    assert(0);			/* Not instantiable */
  case OBJ_TYPE_NIL:
    q = 0;
    break;
  default:
    {
      struct _list *li;
      struct _list *nd;
    
      li = &vm->obj_list._list[vm->obj_list.idx_free];
      if (list_first(li) == list_end(li)) {
	assert(0);
      }
      
      nd = list_first(li);
      list_erase(nd);

      q = FIELD_PTR_TO_STRUCT_PTR(nd, struct obj, _list_node);
      memset(q, 0, sizeof(*q));
      q->type = type;
      
      list_insert(nd, list_end(&vm->obj_list._list[vm->obj_list.idx_alloced]));
    }
  }

  obj_assign(vm, pp, q);
}

static struct obj **
ovm_falloc(struct ovm *vm, unsigned n)
{
  struct obj **result;
  
  assert((vm->sp - n) >= vm->stack);

  result = vm->sp;

  vm->sp -= n;
  memset(vm->sp, 0, n * sizeof(*vm->sp));

  return (result);
}

static void
ovm_ffree(struct ovm *vm, struct obj **fp)
{
  assert(fp <= vm->stack_end);

  for ( ; vm->sp < fp; ++vm->sp)  obj_release(vm, *vm->sp);
}

void
ovm_cl_dict(struct ovm *vm, unsigned type, unsigned r1)
{
  struct obj **pp = _ovm_reg(vm, r1);

  assert(type >= OBJ_TYPE_BASE && type < OBJ_TYPE_LAST);

  obj_assign(vm, pp, vm->cl_tbl[type - OBJ_TYPE_BASE]);
}

/***************************************************************************/

#if 0

static void
crc_init(unsigned *r)
{
  *r = ~0;
}

static unsigned crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static unsigned
crc32(unsigned *r, unsigned n, unsigned char *p)
{
  for ( ; n; --n, ++p)  *r = crc32_tab[(*r ^ *p) & 0xFF] ^ (*r >> 8);

  return (*r);
}

#else

static void
crc_init(unsigned *r)
{
  *r = 0;
}

static unsigned
crc32(unsigned *r, unsigned n, unsigned char *p)
{
  for ( ; n; --n, ++p)  *r = ((19 * *r) / 7) + *p;

  return (*r);
}

#endif

/***************************************************************************/

static void
obj_bad_method(struct ovm *vm, struct obj **pp, va_list ap)
{
  ovm_error(vm, OBJ_ERRNO_BAD_METHOD);
}

#define METHOD_1(nm, nf, ex)						\
  static void								\
  nm (struct ovm *vm, struct obj **pp, va_list ap)			\
  {									\
    nf (vm, pp, (ex));							\
  }

#define METHOD_2(nm, ty, nf, vf, op)					\
  static void								\
  nm (struct ovm *vm, struct obj **pp, va_list ap)			\
  {									\
    struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));		\
									\
    if (obj_type(q) != (ty)) {						\
      ovm_error(vm, OBJ_ERRNO_BAD_TYPE);					\
      return;								\
    }									\
    									\
    nf (vm, pp, (*pp)->val. vf op q->val. vf);				\
  }

/***************************************************************************/

static int obj_nil_parse(struct ovm *vm, struct obj **pp, unsigned n, char *p);
static int obj_bool_parse(struct ovm *vm, struct obj **pp, unsigned n, char *p);
static int obj_int_parse(struct ovm *vm, struct obj **pp, unsigned n, char *p);
static int obj_float_parse(struct ovm *vm, struct obj **pp, unsigned n, char *p);
static int obj_pair_parse(struct ovm *vm, struct obj **pp, unsigned n, char *p);
static int obj_list_parse(struct ovm *vm, struct obj **pp, unsigned n, char *p);
static int obj_array_parse(struct ovm *vm, struct obj **pp, unsigned n, char *p);
static int obj_dict_parse(struct ovm *vm, struct obj **pp, unsigned n, char *p);
static int obj_str_parse(struct ovm *vm, struct obj **pp, unsigned n, char *p);

static void obj_nil_tostring(struct ovm *vm, struct obj **pp, struct obj *q);
static void obj_bool_tostring(struct ovm *vm, struct obj **pp, struct obj *q);
static void obj_integer_tostring(struct ovm *vm, struct obj **pp, struct obj *q);
static void obj_float_tostring(struct ovm *vm, struct obj **pp, struct obj *q);
static void obj_str_tostring(struct ovm *vm, struct obj **pp, struct obj *q);
static void obj_pair_tostring(struct ovm *vm, struct obj **pp, struct obj *q);
static void obj_list_tostring(struct ovm *vm, struct obj **pp, struct obj *q);
static void obj_array_tostring(struct ovm *vm, struct obj **pp, struct obj *q);
static void obj_dict_tostring(struct ovm *vm, struct obj **pp, struct obj *q);

static struct obj *_obj_dict_at(struct ovm *vm, struct obj *dict, struct obj *key);
static void _obj_dict_at_put(struct ovm *vm, struct obj *dict, struct obj *key, struct obj *val);
static void _obj_dict_del(struct ovm *vm, struct obj *dict, struct obj *key);


static unsigned
delim_find(unsigned *pn, char **pp, char d)
{
  unsigned n = *pn, lvl, qlvl;
  char     *p = *pp, c, dc, *dd;

  for (qlvl = lvl = 0; n; --n, ++p) {
    c = *p;

    if (c == d && qlvl == 0 && lvl == 0) {
      *pn = n;
      *pp = p;
      
      return (1);
    }

    switch (c) {
    case '\\':
      if (n > 1) {
	++p;  --n;
      }
      break;
    case '<':
    case '(':
    case '[':
    case '{':
      ++lvl;
      break;
    case '>':
    case ')':
    case ']':
    case '}':
      --lvl;
      break;
    case '"':
      qlvl = !qlvl;
    }
  }

  return (0);
}

static void
trim(unsigned *pn, char **pp)
{
  unsigned n;
  char     *p, *q;

  for (p = *pp, n = *pn; n && isspace(*p); --n, ++p);
  for (q = p + n - 1; n && isspace(*q); --n, --q);

  *pn = n;
  *pp = p;
}

static int
obj_str_parse(struct ovm *vm, struct obj **pp, unsigned n, char *p)
{
  char     c;
  unsigned negf;

  trim(&n, &p);
  if (n == 0)  return (-1);

  switch (c = p[0]) {
  case '<':
    return (obj_pair_parse(vm, pp, n, p));
  case '(':
    return (obj_list_parse(vm, pp, n, p));
  case '[':
    return (obj_array_parse(vm, pp, n, p));
  case '{':
    return (obj_dict_parse(vm, pp, n, p));
  case '#':
    return (obj_nil_parse(vm, pp, n, p) == 0
	    || obj_bool_parse(vm, pp, n, p) == 0
	    ? 0 : -1
	    );
  case '"':
    if (p[n - 1] != '"')  return (-1);
    obj_string_newc(vm, pp, 1, n - 2, p + 1);
    return (0);

  default:
    if (obj_int_parse(vm, pp, n, p) == 0
	|| obj_float_parse(vm, pp, n, p) == 0
	) {
      return (0);
    }
  }

  return (-1);
}

static void
obj_tostring(struct ovm *vm, struct obj **pp, struct obj *q)
{
  switch (obj_type(q)) {
  case OBJ_TYPE_NIL:
    obj_nil_tostring(vm, pp, q);
    return;
  case OBJ_TYPE_BOOLEAN:
    obj_bool_tostring(vm, pp, q);
    return;
  case OBJ_TYPE_INTEGER:
    obj_integer_tostring(vm, pp, q);
    return;
  case OBJ_TYPE_FLOAT:
    obj_float_tostring(vm, pp, q);
    return;
  case OBJ_TYPE_STRING:
    obj_str_tostring(vm, pp, q);
    return;
  case OBJ_TYPE_PAIR:
    obj_pair_tostring(vm, pp, q);
    return;
  case OBJ_TYPE_LIST:
    obj_list_tostring(vm, pp, q);
    return;
  case OBJ_TYPE_ARRAY:
    obj_array_tostring(vm, pp, q);
    return;
  case OBJ_TYPE_DICT:
    obj_dict_tostring(vm, pp, q);
    return;
  default:
    ;
  }
  
  assert(0);
}

/***************************************************************************/

static void
obj_nil_newc(struct ovm *vm, struct obj **pp)
{
  obj_assign(vm, pp, 0);
}

static int
obj_nil_parse(struct ovm *vm, struct obj **pp, unsigned n, char *p)
{
  trim(&n, &p);

  if (n == 4 && strncmp(p, "#nil", n) == 0) {
    obj_nil_newc(vm, pp);
    return (vm->errno != OBJ_ERRNO_NONE ? -1 : 0);
  }
  
  return (-1);
}

static void
obj_nil_tostring(struct ovm *vm, struct obj **pp, struct obj *q)
{
  obj_string_newc(vm, pp, 1, 4, "#nil");
}

static void
obj_nil_new(struct ovm *vm, struct obj **pp, va_list ap)
{
  obj_nil_newc(vm, pp);
}

static void
obj_nil_append(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));

  switch (obj_type(q)) {
  case OBJ_TYPE_NIL:
  case OBJ_TYPE_LIST:
    break;
  default:
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }

  obj_assign(vm, pp, q);
}

static void
obj_nil_at(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));

  vm->errno = obj_type(q) != OBJ_TYPE_INTEGER
    ? OBJ_ERRNO_BAD_TYPE : OBJ_ERRNO_RANGE
    ;
}

static void
obj_nil_eq(struct ovm *vm, struct obj **pp, va_list ap)
{
  obj_bool_newc(vm, pp, *_ovm_reg(vm, va_arg(ap, unsigned)) == 0);
}

static void
obj_nil_filter(struct ovm *vm, struct obj **pp, va_list ap)
{
  obj_assign(vm, pp, 0);
}

static void
obj_nil_hash(struct ovm *vm, struct obj **pp, va_list ap)
{
  obj_integer_newc(vm, pp, 0);
}

static void
obj_nil_reverse(struct ovm *vm, struct obj **pp, va_list ap)
{
  obj_assign(vm, pp, 0);
}

static void
obj_nil_size(struct ovm *vm, struct obj **pp, va_list ap)
{
  obj_integer_newc(vm, pp, 0);
}

static void
obj_nil_slice(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));
  struct obj *r = *_ovm_reg(vm, va_arg(ap, unsigned));

  if (obj_type(q) != OBJ_TYPE_INTEGER || obj_type(r) != OBJ_TYPE_INTEGER) {
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }

  obj_nil_newc(vm, pp);
}

/***************************************************************************/

static void
obj_ptr_newc(struct ovm *vm, struct obj **pp, void *val)
{
  obj_alloc(vm, pp, OBJ_TYPE_POINTER);

  if (vm->errno != OBJ_ERRNO_NONE)  return;

  PTRVAL(*pp) = val;
}

/***************************************************************************/

static void
obj_bool_newc(struct ovm *vm, struct obj **pp, unsigned val)
{
  obj_alloc(vm, pp, OBJ_TYPE_BOOLEAN);

  if (vm->errno != OBJ_ERRNO_NONE)  return;

  BOOLVAL(*pp) = (val != 0);
}

static int
obj_bool_parse(struct ovm *vm, struct obj **pp, unsigned n, char *p)
{
  unsigned val;

  trim(&n, &p);

  if (n == 5 && strncmp(p, "#true", n) == 0) {
    val = 1;
  } else if(n == 6 && strncmp(p, "#false", n) == 0) {
    val = 0;
  } else {
    return (-1);
  }

  obj_bool_newc(vm, pp, val);

  return (vm->errno != OBJ_ERRNO_NONE ? -1 : 0);
}

static void
obj_bool_tostring(struct ovm *vm, struct obj **pp, struct obj *q)
{
  unsigned n;
  char     *s;

  if (BOOLVAL(q)) {
    n = 5;
    s = "#true";
  } else {
    n = 6;
    s = "#false";
  }

  obj_string_newc(vm, pp, 1, n, s);
}

static void
obj_bool_new(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));
  unsigned   val;

  switch (obj_type(q)) {
  case OBJ_TYPE_BOOLEAN:
    obj_assign(vm, pp, q);
    return;
  case OBJ_TYPE_INTEGER:
    val = (INTVAL(q) != 0);
    break;
  case OBJ_TYPE_FLOAT:
    val = (FLOATVAL(q) != 0.0);
    break;
  case OBJ_TYPE_STRING:
    if (obj_bool_parse(vm, pp, STR_SIZE(q) - 1, STR_DATA(q)) == 0)  return;
    ovm_error(vm, OBJ_ERRNO_BAD_VALUE);
    return;
  default:
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }

  obj_bool_newc(vm, pp, val);
}

METHOD_2(obj_bool_and, OBJ_TYPE_BOOLEAN, obj_bool_newc, boolval, &&)

static void
obj_bool_eq(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));

  obj_bool_newc(vm,
		pp,
		obj_type(q) == OBJ_TYPE_BOOLEAN && (BOOLVAL(*pp) != 0) == (BOOLVAL(q) != 0)
		);
}

METHOD_1(obj_bool_hash, obj_integer_newc, BOOLVAL(*pp) != 0);

METHOD_1(obj_bool_not, obj_bool_newc, BOOLVAL(*pp) == 0);

METHOD_2(obj_bool_or, OBJ_TYPE_BOOLEAN, obj_bool_newc, boolval, ||);

METHOD_2(obj_bool_xor, OBJ_TYPE_BOOLEAN, obj_bool_newc, boolval, ^);

/***************************************************************************/

static void
obj_integer_newc(struct ovm *vm, struct obj **pp, obj_integer_val_t val)
{
  obj_alloc(vm, pp, OBJ_TYPE_INTEGER);

  if (vm->errno != OBJ_ERRNO_NONE)  return;

  INTVAL(*pp) = val;
}

static int
obj_int_parse(struct ovm *vm, struct obj **pp, unsigned n, char *p)
{
  char     c, buf[n + 1], *fmt, *q;
  unsigned k;
  obj_integer_val_t val;

  trim(&n, &p);

  if (n >= 2 && p[0] == '0') {
    if (n >= 3 && (p[1] | 0x20) == 'x') {
      p += 2;  n -= 2;
      
      for (q = p, k = n; k; --k, ++q) {
	if (!((c = *q) >= '0' && c <= '9' || (c |= 0x20) >= 'a' && c <= 'f'))  return (-1);
      }

      fmt = "%llx";
    } else {
      ++p;  --n;

      for (q = p, k = n; k; --k, ++q) {
	if (!((c = *q) >= '0' && c <= '7'))  return (-1);
      }

      fmt = "%llo";
    }
  } else if (n >= 1 && (c = p[0]) >= '0' && c <= '9'
	     || n >= 2 && p[0] == '-' && (c = p[1]) >= '0' && c <= '9'
      ) {
    q = p;  k = n;
    if (*q == '-') {
      ++q;  --k;
    }
    for ( ; k; --k, ++q) {
      if (!((c = *q) >= '0' && c <= '9'))  return (-1);
    }

    fmt = "%lld";
  } else {
    return (-1);
  }

  memcpy(buf, p, n);
  buf[n] = 0;
  if (sscanf(buf, fmt, &val) != 1)  return (-1);

  obj_integer_newc(vm, pp, val);

  return (vm->errno != OBJ_ERRNO_NONE ? -1 : 0);
}

static char *
obj_integer_tostring_fmt(struct ovm *vm)
{
  static const char s[] = "tostring-format", dflt[] = "%lld";

  char *result = (char *) dflt;
  struct obj **fp, *p, *q;

  fp = ovm_falloc(vm, 1);

  obj_string_newc(vm, &fp[-1], 1, sizeof(s) - 1, s);

  if (vm->errno == OBJ_ERRNO_NONE
      && (p = _obj_dict_at(vm, vm->cl_tbl[OBJ_TYPE_INTEGER - OBJ_TYPE_BASE], fp[-1]))
      && obj_type(q = CDR(p)) == OBJ_TYPE_STRING
      ) {
    result = STR_DATA(q);
  }

  ovm_ffree(vm, fp);

  return (result);
}

static void
obj_integer_tostring(struct ovm *vm, struct obj **pp, struct obj *q)
{
  char buf[64];

  snprintf(buf, sizeof(buf), obj_integer_tostring_fmt(vm), INTVAL(q));
  
  obj_string_newc(vm, pp, 1, strlen(buf), buf);
}

static void
obj_integer_new(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj        *q = *_ovm_reg(vm, va_arg(ap, unsigned));
  obj_integer_val_t val;

  switch (obj_type(q)) {
  case OBJ_TYPE_BOOLEAN:
    val = (BOOLVAL(q) != 0);
    break;
  case OBJ_TYPE_INTEGER:
    obj_assign(vm, pp, q);
    return;
  case OBJ_TYPE_FLOAT:
    val = (obj_integer_val_t) FLOATVAL(q);
    break;
  case OBJ_TYPE_STRING:
    if (obj_int_parse(vm, pp, STR_SIZE(q) - 1, STR_DATA(q)) == 0)  return;
    ovm_error(vm, OBJ_ERRNO_BAD_VALUE);
    return;
  default:
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }

  obj_integer_newc(vm, pp, val);
}

METHOD_1(obj_integer_abs, obj_integer_newc, abs(INTVAL(*pp)));

METHOD_2(obj_integer_add, OBJ_TYPE_INTEGER, obj_integer_newc, intval, +);

METHOD_2(obj_integer_and, OBJ_TYPE_INTEGER, obj_integer_newc, intval, &);

METHOD_2(obj_integer_div, OBJ_TYPE_INTEGER, obj_integer_newc, intval, /);

static void
obj_integer_eq(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));
  
  obj_bool_newc(vm, pp, obj_type(q) == OBJ_TYPE_INTEGER && INTVAL(*pp) == INTVAL(q));
}

METHOD_2(obj_integer_gt, OBJ_TYPE_INTEGER, obj_bool_newc, intval, >);

static void
obj_integer_hash(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp;
  unsigned   r[1];

  crc_init(r);
  obj_integer_newc(vm,
		   pp,
		   (obj_integer_val_t) crc32(r,
					     sizeof(INTVAL(p)),
					     (unsigned char *) &INTVAL(p)
					     )
		   );
}

METHOD_2(obj_integer_lt, OBJ_TYPE_INTEGER, obj_bool_newc, intval, <);

METHOD_1(obj_integer_minus, obj_integer_newc, -INTVAL(*pp));

METHOD_2(obj_integer_mod, OBJ_TYPE_INTEGER, obj_integer_newc, intval, %);

METHOD_2(obj_integer_mult, OBJ_TYPE_INTEGER, obj_integer_newc, intval, *);

METHOD_2(obj_integer_or, OBJ_TYPE_INTEGER, obj_integer_newc, intval, |);

METHOD_2(obj_integer_sub, OBJ_TYPE_INTEGER, obj_integer_newc, intval, -);

METHOD_2(obj_integer_xor, OBJ_TYPE_INTEGER, obj_integer_newc, intval, ^);

/***************************************************************************/

static void
obj_float_newc(struct ovm *vm, struct obj **pp, obj_float_val_t val)
{
  obj_alloc(vm, pp, OBJ_TYPE_FLOAT);

  if (vm->errno != OBJ_ERRNO_NONE)  return;

  FLOATVAL(*pp) = val;
}

static int
obj_float_parse(struct ovm *vm, struct obj **pp, unsigned n, char *p)
{
  char            c, buf[n + 1], *q;
  unsigned        k, i, dpcnt;
  obj_float_val_t val;

  trim(&n, &p);

  q = p;  k = n;

  if (k < 1)  return (-1);
  if (*q == '-') {
    ++q;  --k;
    if (k < 1)  return (-1);
  }
  for (dpcnt = i = 0; k; --k, ++q) {
    if ((c = *q) >= '0' && c <= '9') {
      ++i;
      continue;
    }

    if (c == '.') {
      if (i < 1 || ++dpcnt > 1)  return (-1);
      i = 0;
      continue;
    }

    if ((c | 0x20) == 'e')  break;

    return (-1);
  }

  if (dpcnt != 0 && i == 0)  return (-1);

  if (k > 0) {
    ++q;  --k;

    if (k < 1)  return (-1);
    if (*q == '-') {
      ++q;  --k;
      if (k < 1)  return (-1);
    }
    for ( ; k; --k, ++q) {
      if (!((c = *q) >= '0' && c <= '9'))  return (-1);
    }
  }

  memcpy(buf, p, n);
  buf[n] = 0;
  if (sscanf(buf, "%Lg", &val) != 1)  return (-1);

  obj_float_newc(vm, pp, val);

  return (vm->errno != OBJ_ERRNO_NONE ? -1 : 0);
}

static char *
obj_float_tostring_fmt(struct ovm *vm)
{
  static const char s[] = "tostring-format", dflt[] = "%Lg";

  char *result = (char *) dflt;
  struct obj **fp, *p, *q;

  fp = ovm_falloc(vm, 1);

  obj_string_newc(vm, &fp[-1], 1, sizeof(s) - 1, s);

  if (vm->errno == OBJ_ERRNO_NONE
      && (p = _obj_dict_at(vm, vm->cl_tbl[OBJ_TYPE_FLOAT - OBJ_TYPE_BASE], fp[-1]))
      && obj_type(q = CDR(p)) == OBJ_TYPE_STRING
      ) {
    result = STR_DATA(q);
  }

  ovm_ffree(vm, fp);

  return (result);
}

static void
obj_float_tostring(struct ovm *vm, struct obj **pp, struct obj *q)
{
  char buf[64];

  snprintf(buf, sizeof(buf), obj_float_tostring_fmt(vm), FLOATVAL(q));
  
  obj_string_newc(vm, pp, 1, strlen(buf), buf);
}

static void
obj_float_new(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));
  obj_float_val_t val;

  switch (obj_type(q)) {
  case OBJ_TYPE_BOOLEAN:
    val = (BOOLVAL(q) != 0);
    break;
  case OBJ_TYPE_INTEGER:
    val = (obj_float_val_t) INTVAL(q);
    break;
  case OBJ_TYPE_FLOAT:
    obj_assign(vm, pp, q);
    return;
  case OBJ_TYPE_STRING:
    if (obj_float_parse(vm, pp, STR_SIZE(q) - 1, STR_DATA(q)) == 0)  return;
    ovm_error(vm, OBJ_ERRNO_BAD_VALUE);
    return;
  default:
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }

  obj_float_newc(vm, pp, val);
}

METHOD_1(obj_float_abs, obj_float_newc, abs(FLOATVAL(*pp)));

METHOD_2(obj_float_add, OBJ_TYPE_FLOAT, obj_float_newc, floatval, +);

METHOD_2(obj_float_div, OBJ_TYPE_FLOAT, obj_float_newc, floatval, /);

static void
obj_float_eq(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));

  obj_bool_newc(vm, pp, obj_type(q) == OBJ_TYPE_FLOAT && FLOATVAL(*pp) == FLOATVAL(q));
}

METHOD_2(obj_float_gt, OBJ_TYPE_FLOAT, obj_bool_newc, floatval, >);

static void
obj_float_hash(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp;
  unsigned   r[1];

  crc_init(r);
  obj_integer_newc(vm,
		   pp,
		   (obj_integer_val_t) crc32(r,
					     sizeof(FLOATVAL(p)),
					     (unsigned char *) &FLOATVAL(p)
					     )
		   );
}

METHOD_2(obj_float_lt, OBJ_TYPE_FLOAT, obj_bool_newc, floatval, <);

METHOD_1(obj_float_minus, obj_float_newc, -FLOATVAL(*pp));

METHOD_2(obj_float_mult, OBJ_TYPE_FLOAT, obj_float_newc, floatval, *);

METHOD_2(obj_float_sub, OBJ_TYPE_FLOAT, obj_float_newc, floatval, -);

/***************************************************************************/

static void
obj_str_tostring(struct ovm *vm, struct obj **pp, struct obj *q)
{
  obj_string_newc(vm, pp, 3, 1, "\"",
		             STR_SIZE(q) - 1, STR_DATA(q),
		             1, "\""
		  );
}

static void
obj_string_newc(struct ovm *vm, struct obj **pp, unsigned n, ...)
{
  va_list    ap;
  unsigned   size, len, nn;
  char       *p, *s;

  va_start(ap, n);
  for (size = 0, nn = n; nn; --nn) {
    len = va_arg(ap, unsigned);
    s   = va_arg(ap, char *);
    
    size += len;
  }
  ++size;
  va_end(ap);

  obj_alloc(vm, pp, OBJ_TYPE_STRING);

  if (vm->errno != OBJ_ERRNO_NONE)  return;

  if ((p = malloc(STR_SIZE(*pp) = size)) == 0) {
    obj_assign(vm, pp, 0);

    ovm_error(vm, OBJ_ERRNO_MEM);

    return;
  }

  STR_DATA(*pp) = p;

  va_start(ap, n);
  for (nn = n; nn; --nn) {
    len = va_arg(ap, unsigned);
    s   = va_arg(ap, char *);

    memcpy(p, s, len);
    p += len;
  }
  va_end(ap);

  *p = 0;
}

static void
obj_string_newv(struct ovm *vm, struct obj **pp, struct obj *a)
{
  unsigned   size, n, nn;
  struct obj **q;
  char       *p;

  for (size = 0, q = ARRAY_DATA(a), n = ARRAY_SIZE(a); n; --n, ++q) {
    size += STR_SIZE(*q) - 1;
  }
  ++size;

  obj_alloc(vm, pp, OBJ_TYPE_STRING);

  if (vm->errno != OBJ_ERRNO_NONE)  return;

  if ((p = malloc(STR_SIZE(*pp) = size)) == 0) {
    obj_assign(vm, pp, 0);

    ovm_error(vm, OBJ_ERRNO_MEM);

    return;
  }

  STR_DATA(*pp) = p;

  for (q = ARRAY_DATA(a), n = ARRAY_SIZE(a); n; --n, ++q) {
    memcpy(p, STR_DATA(*q), nn = STR_SIZE(*q) - 1);
    p += nn;
  }
  *p = 0;
}

static unsigned list_len(struct obj *p);

static void
obj_string_new(struct ovm *vm, struct obj **pp, va_list ap)
{
  obj_tostring(vm, pp, *_ovm_reg(vm, va_arg(ap, unsigned)));
}

static void
obj_string_append(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp, *q = *_ovm_reg(vm, va_arg(ap, unsigned));

  if (obj_type(q) != OBJ_TYPE_STRING) {
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }
  
  obj_string_newc(vm, pp, 2, STR_SIZE(p) - 1, STR_DATA(p),
		             STR_SIZE(q) - 1, STR_DATA(q)
		  );
}

static void
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

    return;
  }

  if (*start < 0)  *start = 0;
  if (end > size)  end = size;
  *len = end - *start;
}

static void
obj_string_at(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp, *q = *_ovm_reg(vm, va_arg(ap, unsigned));
  int i, n;


  if (obj_type(q) != OBJ_TYPE_INTEGER) {
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }

  i = INTVAL(q);
  n = 1;
  slice_idxs((int) STR_SIZE(p), &i, &n);
  
  obj_string_newc(vm, pp, 1, n, &STR_DATA(p)[i]);
}

static void
obj_string_copy(struct ovm *vm, struct obj **pp, struct obj *s)
{
  obj_string_newc(vm, pp, 1, STR_SIZE(s) - 1, STR_DATA(s));
}

static void
obj_string_eq(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));

  obj_bool_newc(vm,
		pp,
		obj_type(q) == OBJ_TYPE_STRING && strcmp(STR_DATA(*pp), STR_DATA(q)) == 0
		);
}

static void
obj_string_gt(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));

  if (obj_type(q) != OBJ_TYPE_STRING) {
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }
  
  obj_bool_newc(vm, pp, strcmp(STR_DATA(*pp), STR_DATA(q)) > 0);
}

static void
obj_string_hash(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp;
  unsigned r[1];

  crc_init(r);
  obj_integer_newc(vm, pp, crc32(r, STR_SIZE(p) - 1, (unsigned char *) STR_DATA(p)));
}

static void
obj_string_join(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp, *q = *_ovm_reg(vm, va_arg(ap, unsigned)), **fp, **rr;
  unsigned i, n;

  if (!is_list(q)) {
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }

  fp = ovm_falloc(vm, 1);

  n = list_len(q);
  obj_array_newc(vm, &fp[-1], n <= 1 ? n : 2 * n - 1);
  for (i = 0, rr = ARRAY_DATA(fp[-1]); q; q = CDR(q), ++i) {
    if (i > 0) {
      obj_assign(vm, rr, p);
      ++rr;
    }
    obj_tostring(vm, rr, CAR(q));
    ++rr;
  }

  obj_string_newv(vm, pp, fp[-1]);

  ovm_ffree(vm, fp);
}

static void
obj_string_lt(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));

  if (obj_type(q) != OBJ_TYPE_STRING) {
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }
  
  obj_bool_newc(vm, pp, strcmp(STR_DATA(*pp), STR_DATA(q)) < 0);
}

static void
obj_string_reverse(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj **fp, *p;
  unsigned   n;
  char       *q, *r;

  fp = ovm_falloc(vm, 1);

  obj_string_copy(vm, &fp[-1], *pp);

  n = STR_SIZE(fp[-1]);
  for (q = STR_DATA(fp[-1]), r = q + n - 2; q < r; ++q, --r) {
    char temp = *q;
    *q = *r;
    *r = temp;
  }

  obj_assign(vm, pp, fp[-1]);

  ovm_ffree(vm, fp);
}

static void
_obj_string_size(struct ovm *vm, struct obj **pp, va_list ap)
{
  obj_integer_newc(vm, pp, STR_SIZE(*pp) - 1);
}

static void
obj_string_slice(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp;
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));
  struct obj *r = *_ovm_reg(vm, va_arg(ap, unsigned));
  int        i, n;

  if (obj_type(q) != OBJ_TYPE_INTEGER || obj_type(r) != OBJ_TYPE_INTEGER) {
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }

  i = INTVAL(q);
  n = INTVAL(r);
  slice_idxs((int) STR_SIZE(p) - 1, &i, &n);

  obj_string_newc(vm, pp, 1, n, &STR_DATA(p)[i]);
}

static void
obj_string_split(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp;
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));
  struct obj **fp, **qq;
  char *r, *s;

  if (obj_type(q) != OBJ_TYPE_STRING) {
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }

  fp = ovm_falloc(vm, 2);

  for (qq = &fp[-1], r = STR_DATA(p); ; r = s + STR_SIZE(q) - 1) {
    s = strstr(r, STR_DATA(q));

    obj_string_newc(vm, &fp[-2], 1, s ? s - r : strlen(r), r);
    obj_list_newc(vm, qq, fp[-2], 0);
    qq = &CDR(*qq);

    if (s == 0)  break;
  }

  obj_assign(vm, pp, fp[-1]);

  ovm_ffree(vm, fp);
}

/***************************************************************************/

static void
obj_dptr_newc(struct ovm *vm, struct obj **pp, unsigned type, struct obj *car, struct obj *cdr)
{
  obj_alloc(vm, pp, type);

  if (vm->errno != OBJ_ERRNO_NONE)  return;

  obj_assign(vm, &CAR(*pp), car);
  obj_assign(vm, &CDR(*pp), cdr);
}

static void
obj_dptr_car(struct ovm *vm, struct obj **pp, va_list ap)
{
  obj_assign(vm, pp, CAR(*pp));
}

static void
obj_dptr_cdr(struct ovm *vm, struct obj **pp, va_list ap)
{
  obj_assign(vm, pp, CDR(*pp));
}

/***************************************************************************/

static void
obj_pair_newc(struct ovm *vm, struct obj **pp, struct obj *car, struct obj *cdr)
{
  obj_dptr_newc(vm, pp, OBJ_TYPE_PAIR, car, cdr);
}

static int
obj_pair_parse(struct ovm *vm, struct obj **pp, unsigned n, char *p)
{
  int        result = -1;
  struct obj **fp;
  char       *st;

  trim(&n, &p);
  
  if (!(n >= 2 && p[0] == '<' && p[n - 1] == '>'))  return (-1);
  ++p;
  n -= 2;
  trim(&n, &p);
  st = p;
  if (!delim_find(&n, &p, ','))  return (-1);
 
  fp = ovm_falloc(vm, 1);

  obj_pair_newc(vm, &fp[-1], 0, 0);
  if (obj_str_parse(vm, &CAR(fp[-1]), p - st, st) < 0
      || obj_str_parse(vm, &CDR(fp[-1]), n - 1, p + 1) < 0
      ) {
    goto done;
  }
  
  obj_assign(vm, pp, fp[-1]);
  result = 0;

 done:
  ovm_ffree(vm, fp);

  return (result);
}

static void
obj_pair_tostring(struct ovm *vm, struct obj **pp, struct obj *q)
{
  struct obj **fp;

  fp = ovm_falloc(vm, 2);
  
  obj_tostring(vm, &fp[-1], CAR(q));
  if (vm->errno != OBJ_ERRNO_NONE)  goto done;
  obj_tostring(vm, &fp[-2], CDR(q));
  if (vm->errno != OBJ_ERRNO_NONE)  goto done;
  obj_string_newc(vm, pp, 5, 1, "<",
		             STR_SIZE(fp[-1]) - 1, STR_DATA(fp[-1]),
		             2, ", ",
		             STR_SIZE(fp[-2]) - 1, STR_DATA(fp[-2]),
		             1, ">"
		  );
 done:
  ovm_ffree(vm, fp);
}

static void
obj_pair_new(struct ovm *vm, struct obj **pp, va_list ap)
{
  unsigned argc = va_arg(ap, unsigned);

  switch (argc) {
  case 1:
    {
      struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));

      if (obj_type(q) == OBJ_TYPE_STRING) {
	if (obj_pair_parse(vm, pp, STR_SIZE(q) - 1, STR_DATA(q)) < 0) {
	  ovm_error(vm, OBJ_ERRNO_BAD_VALUE);
	}
	return;
      }

      ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    }
    return;
  case 2:
    {
      struct obj *car = *_ovm_reg(vm, va_arg(ap, unsigned));
      struct obj *cdr = *_ovm_reg(vm, va_arg(ap, unsigned));
      
      obj_pair_newc(vm, pp, car, cdr);
    }
    return;
  default:
    ;
  }

  ovm_error(vm, OBJ_ERRNO_BAD_VALUE);
}

static void
obj_pair_eq(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp;
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));
  struct obj **fp;

  fp = ovm_falloc(vm, 1);

  obj_bool_newc(vm, &fp[-1], 0);
  if (vm->errno != OBJ_ERRNO_NONE)  goto done1;

  if (obj_type(q) == OBJ_TYPE_PAIR) {
    ovm_pushm(vm, 1, 2);

    obj_assign(vm, &R(1), CAR(p));
    obj_assign(vm, &R(2), CAR(q));
    ovm_call(vm, 1, OBJ_OP_EQ, 2);
    if (vm->errno != OBJ_ERRNO_NONE)  goto done2;
    if (BOOLVAL(R(1))) {
      obj_assign(vm, &R(1), CDR(p));
      obj_assign(vm, &R(2), CDR(q));
      ovm_call(vm, 1, OBJ_OP_EQ, 2);
      if (vm->errno != OBJ_ERRNO_NONE)  goto done2;
      if (BOOLVAL(R(1))) {
	BOOLVAL(fp[-1]) = 1;
      }
    }
    
  done2:
    ovm_popm(vm, 1, 2);
  }

 done1:
  if (vm->errno == OBJ_ERRNO_NONE)  obj_assign(vm, pp, fp[-1]);

  ovm_ffree(vm, fp);
}

static void
obj_pair_hash(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp;
  struct obj **fp;

  fp = ovm_falloc(vm, 1);

  obj_integer_newc(vm, &fp[-1], 0);
  if (vm->errno != OBJ_ERRNO_NONE)  goto done1;

  ovm_push(vm, 1);

  obj_assign(vm, &R(1), CAR(p));
  ovm_call(vm, 1, OBJ_OP_HASH);
  if (vm->errno != OBJ_ERRNO_NONE)  goto done2;
  INTVAL(fp[-1]) += INTVAL(R(1));
  obj_assign(vm, &R(1), CDR(p));
  ovm_call(vm, 1, OBJ_OP_HASH);
  if (vm->errno != OBJ_ERRNO_NONE)  goto done2;
  INTVAL(fp[-1]) += INTVAL(R(1));

 done2:
  ovm_pop(vm, 1);

  if (vm->errno == OBJ_ERRNO_NONE)  obj_assign(vm, pp, fp[-1]);

 done1:
  ovm_ffree(vm, fp);
}

static void
obj_pair_reverse(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp, **fp;

  fp = ovm_falloc(vm, 1);

  obj_pair_newc(vm, &fp[-1], CDR(p), CAR(p));

  obj_assign(vm, pp, fp[-1]);

  ovm_ffree(vm, fp);
}

/***************************************************************************/

static unsigned
list_len(struct obj *p)
{
  unsigned result;

  for (result = 0; p; p = CDR(p))  ++result;

  return (result);
}

static void
obj_list_newc(struct ovm *vm, struct obj **pp, struct obj *car, struct obj *cdr)
{
  obj_dptr_newc(vm, pp, OBJ_TYPE_LIST, car, cdr);
}

static int
obj_list_parse(struct ovm *vm, struct obj **pp, unsigned n, char *p)
{
  int        result = -1;
  struct obj **fp, **qq;
  char       *st;
  unsigned   f;
  
  trim(&n, &p);

  if (!(n >= 2 && p[0] == '(' &&  p[n - 1] == ')'))  return (-1);
  ++p;
  n -= 2;
  trim(&n, &p);

  fp = ovm_falloc(vm, 2);

  for (qq = &fp[-1]; n > 0; ) {
    st = p;
    f = delim_find(&n, &p, ',');
    if (obj_str_parse(vm, &fp[-2], f ? p - st : n, st) < 0)  goto done;

    obj_list_newc(vm, qq, fp[-2], 0);
    if (vm->errno != OBJ_ERRNO_NONE)  goto done;

    qq = &CDR(*qq);
    
    if (!f)  break;
    
    ++p;
    --n;
  }

  obj_assign(vm, pp, fp[-1]);
  result = 0;

 done:
  ovm_ffree(vm, fp);

  return (result);
}

static void
obj_list_tostring(struct ovm *vm, struct obj **pp, struct obj *q)
{
  struct obj **fp, **qq;
  unsigned   n, i;
  
  fp = ovm_falloc(vm, 1);

  n = list_len(q);
  if (n > 1)  n += n - 1;
  n += 2;;

  obj_array_newc(vm, &fp[-1], n);
  if (vm->errno != OBJ_ERRNO_NONE)  goto done;

  qq = ARRAY_DATA(fp[-1]);
  
  obj_string_newc(vm, qq, 1, 1, "(");
  if (vm->errno != OBJ_ERRNO_NONE)  goto done;
  ++qq;
  for (i = 0; q; ++i, q = CDR(q)) {
    if (i > 0) {
      obj_string_newc(vm, qq, 1, 2, ", ");
      if (vm->errno != OBJ_ERRNO_NONE)  goto done;
      ++qq;
    }
    obj_tostring(vm, qq, CAR(q));
    if (vm->errno != OBJ_ERRNO_NONE)  goto done;
    ++qq;
  }
  obj_string_newc(vm, qq, 1, 1, ")");
  if (vm->errno != OBJ_ERRNO_NONE)  goto done;

  obj_string_newv(vm, pp, fp[-1]);

 done:
  ovm_ffree(vm, fp);
}

static void
obj_list_new(struct ovm *vm, struct obj **pp, va_list ap)
{
  unsigned   argc = va_arg(ap, unsigned);
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned)), *cdr;

  switch (argc) {
  case 1:
    switch (obj_type(q)) {
    case OBJ_TYPE_STRING:
      if (obj_list_parse(vm, pp, STR_SIZE(q) - 1, STR_DATA(q)) == 0)  return;
      ovm_error(vm, OBJ_ERRNO_BAD_VALUE);
      return;

    case OBJ_TYPE_ARRAY:
      {
	struct obj **fp, **qq, **rr;
	char *r;
	unsigned n;

	fp = ovm_falloc(vm, 1);

	for (qq = &fp[-1], rr = ARRAY_DATA(q), n = ARRAY_SIZE(q); n; --n, ++rr) {
	  obj_list_newc(vm, qq, *rr, 0);
	  qq = &CDR(*qq);
	}

	obj_assign(vm, pp, fp[-1]);

	ovm_ffree(vm, fp);
      }
      return;

    case OBJ_TYPE_DICT:
    default:
      ;
    }

    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;

  case 2:
    cdr = *_ovm_reg(vm, va_arg(ap, unsigned));
    
    if (!is_list(cdr)) {
      ovm_error(vm, OBJ_ERRNO_BAD_VALUE);
      return;
    }

    obj_list_newc(vm, pp, q, cdr);

    return;

  default:
    ;
  }

  ovm_error(vm, OBJ_ERRNO_BAD_VALUE);
}

static void
obj_list_append(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp, *q = *_ovm_reg(vm, va_arg(ap, unsigned)), **qq;
  struct obj **fp;

  if (!is_list(q)) {
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }

  fp = ovm_falloc(vm, 1);
  
  for (qq = &fp[-1]; p; p = CDR(p)) {
    obj_list_newc(vm, qq, CAR(p), 0);
    if (vm->errno != OBJ_ERRNO_NONE)  goto done;
    qq = &CDR(*qq);
  }
  obj_assign(vm, qq, q);

  obj_assign(vm, pp, fp[-1]);

 done:  
  ovm_ffree(vm, fp);
}

static void
obj_list_at(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp, *q = *_ovm_reg(vm, va_arg(ap, unsigned));
  int        i, n;

  if (obj_type(q) != OBJ_TYPE_INTEGER) {
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }

  i = INTVAL(q);
  n = 1;
  slice_idxs((int) list_len(p), &i, &n);
  if (n != 1) {
    ovm_error(vm, OBJ_ERRNO_RANGE);
    return;
  }

  for ( ; i; --i, p = CDR(p));
  obj_assign(vm, pp, CAR(p));
}

static void
obj_list_eq(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp, *q = *_ovm_reg(vm, va_arg(ap, unsigned));
  struct obj **fp;

  fp = ovm_falloc(vm, 1);

  obj_bool_newc(vm, &fp[-1], 0);
  if (vm->errno != OBJ_ERRNO_NONE)  goto done1;

  if (obj_type(q) == OBJ_TYPE_LIST) {
    ovm_pushm(vm, 1, 2);

    for ( ; p && q; p = CDR(p), q = CDR(q)) {
      obj_assign(vm, &R(1), CAR(p));
      obj_assign(vm, &R(2), CAR(q));
      ovm_call(vm, 1, OBJ_OP_EQ, 2);
      if (vm->errno != OBJ_ERRNO_NONE)  goto done2;
      if (!BOOLVAL(R(1)))  break;
    }
    if (p == 0 && q == 0)  BOOLVAL(fp[-1]) = 1;

  done2:
    ovm_popm(vm, 1, 2);
  }

 done1:
  if (vm->errno == OBJ_ERRNO_NONE)  obj_assign(vm, pp, fp[-1]);

  ovm_ffree(vm, fp);
}

static void
obj_list_filter(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp;
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned)), **qq, *r;
  
  struct obj **fp;

  if (!is_list(q)) {
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }

  fp = ovm_falloc(vm, 1);

  for (qq = &fp[-1]; p && q; p = CDR(p), q = CDR(q)) {
    r = CAR(q);
    if (obj_type(r) != OBJ_TYPE_BOOLEAN) {
      ovm_error(vm, OBJ_ERRNO_BAD_VALUE);
      goto done;
    }
    if (BOOLVAL(r)) {
      obj_list_newc(vm, qq, CAR(p), 0);
      qq = &CDR(*qq);
    }
  }

  obj_assign(vm, pp, fp[-1]);

 done:
  ovm_ffree(vm, fp);
}

static void
obj_list_hash(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp, **qq;
  struct obj **fp;

  fp = ovm_falloc(vm, 1);

  obj_integer_newc(vm, &fp[-1], 0);
  if (vm->errno != OBJ_ERRNO_NONE)  goto done1;

  ovm_push(vm, 1);
  for ( ; p; p = CDR(p)) {
    obj_assign(vm, &R(1), CAR(p));
    ovm_call(vm, 1, OBJ_OP_HASH);
    if (vm->errno != OBJ_ERRNO_NONE)  goto done2;
    INTVAL(fp[-1]) += INTVAL(R(1));
  }
 done2:
  ovm_pop(vm, 1);

 done1:
  if (vm->errno == OBJ_ERRNO_NONE)  obj_assign(vm, pp, fp[-1]);

  ovm_ffree(vm, fp);
}

static void
obj_list_reverse(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj **fp, *p;

  fp = ovm_falloc(vm, 2);

  for (p = *pp; p; p = CDR(p)) {
    obj_list_newc(vm, &fp[-2], CAR(p), fp[-1]);
    obj_assign(vm, &fp[-1], fp[-2]);
  }

  obj_assign(vm, pp, fp[-1]);

  ovm_ffree(vm, fp);
}

static void
obj_list_size(struct ovm *vm, struct obj **pp, va_list ap)
{
  obj_integer_newc(vm, pp, list_len(*pp));
}

static void
obj_list_slice(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp;
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));
  struct obj *r = *_ovm_reg(vm, va_arg(ap, unsigned));
  int        i, n;
  struct obj **fp, **qq;

  if (obj_type(q) != OBJ_TYPE_INTEGER || obj_type(r) != OBJ_TYPE_INTEGER) {
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }

  i = INTVAL(q);
  n = INTVAL(r);
  slice_idxs((int) list_len(p), &i, &n);

  fp = ovm_falloc(vm, 1);

  for ( ; i; --i)  p = CDR(p);
  for (qq = &fp[-1]; n; --n) {
    obj_list_newc(vm, qq, CAR(p), 0);
    if (vm->errno != OBJ_ERRNO_NONE)  goto done;
    qq = &CDR(*qq);
  }

  obj_assign(vm, pp, fp[-1]);

 done:
  ovm_ffree(vm, fp);
}

/***************************************************************************/

static void
_obj_array_newc(struct ovm *vm, struct obj **pp, unsigned type, unsigned size)
{
  obj_alloc(vm, pp, type);

  if (vm->errno != OBJ_ERRNO_NONE)  return;

  ARRAY_SIZE(*pp) = size;
  ARRAY_DATA(*pp) = size ? calloc(size, sizeof(ARRAY_DATA(*pp)[0])) : 0;
}

static void
obj_array_newc(struct ovm *vm, struct obj **pp, unsigned size)
{
  _obj_array_newc(vm, pp, OBJ_TYPE_ARRAY, size);
}

static int
obj_array_parse(struct ovm *vm, struct obj **pp, unsigned n, char *p)
{
  int        result = -1;
  struct obj **fp;
  struct obj **qq;
  char       *st, *q;
  unsigned   sz, k, f;

  trim(&n, &p);
  
  if (!(n >= 2 && p[0] == '[' && p[n - 1] == ']'))  return (-1);
  ++p;
  n -= 2;
  trim(&n, &p);
  
  if (n == 0) {
    sz = 0;
  } else {
    for (sz = 1, q = p, k = n; ; ++sz) {
      if (!delim_find(&k, &q, ','))  break;
      ++q;
      --k;
    }
  }

  fp = ovm_falloc(vm, 1);

  obj_array_newc(vm, &fp[-1], sz);
  if (vm->errno != OBJ_ERRNO_NONE)  goto done;
  for (qq = ARRAY_DATA(fp[-1]); sz; --sz, ++qq) {
    st = p;
    f = delim_find(&n, &p, ',');
    if (obj_str_parse(vm, qq, f ? p - st : n, st) < 0)  goto done;
    
    ++p;
    --n;
  }

  obj_assign(vm, pp, fp[-1]);

  result = 0;

 done:
  ovm_ffree(vm, fp);

  return (result);
}

static void
obj_array_tostring(struct ovm *vm, struct obj **pp, struct obj *q)
{
  struct obj **fp, **qq, **rr;
  unsigned   n, i;

  fp = ovm_falloc(vm, 1);
  
  n = ARRAY_SIZE(q);
  if (n > 1)  n += n - 1;
  n += 2;

  obj_array_newc(vm, &fp[-1], n);
  qq = ARRAY_DATA(fp[-1]);
  rr = ARRAY_DATA(q);
  
  obj_string_newc(vm, qq, 1, 1, "[");
  ++qq;
  for (i = 0, n = ARRAY_SIZE(q); n; --n, ++i, ++rr) {
    if (i > 0) {
      obj_string_newc(vm, qq, 1, 2, ", ");
      ++qq;
    }
    obj_tostring(vm, qq, *rr);
    ++qq;
  }
  obj_string_newc(vm, qq, 1, 1, "]");

  obj_string_newv(vm, pp, fp[-1]);

  ovm_ffree(vm, fp);
}

static void
obj_array_copy(struct ovm *vm, struct obj **pp, struct obj *a)
{
  unsigned   n;
  struct obj **fp;
  struct obj **qq, **rr;
  
  fp = ovm_falloc(vm, 1);
  
  obj_array_newc(vm, &fp[-1], n = ARRAY_SIZE(a));
  for (qq = ARRAY_DATA(fp[-1]), rr = ARRAY_DATA(a); n; --n, ++rr, ++qq) {
    obj_assign(vm, qq, *rr);
  }
  
  obj_assign(vm, pp, fp[-1]);
  
  ovm_ffree(vm, fp);
}

static void
obj_array_new(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));

  switch (obj_type(q)) {
  case OBJ_TYPE_INTEGER:
    if (INTVAL(q) < 0) {
      ovm_error(vm, OBJ_ERRNO_BAD_VALUE);
      return;
    }
    obj_array_newc(vm, pp, INTVAL(q));
    return;
  case OBJ_TYPE_STRING:
    if (obj_array_parse(vm, pp, STR_SIZE(q) - 1, STR_DATA(q)) == 0)  return;
    ovm_error(vm, OBJ_ERRNO_BAD_VALUE);
    return;
  case OBJ_TYPE_ARRAY:
    obj_array_copy(vm, pp, q);
    return;
  case OBJ_TYPE_DICT:
    {
      unsigned   n;
      struct obj **fp;
      struct obj **qq, **rr, *s;

      fp = ovm_falloc(vm, 1);

      obj_array_newc(vm, &fp[-1], DICT_CNT(q));
      for (qq = ARRAY_DATA(fp[-1]), rr = DICT_DATA(q), n = DICT_SIZE(q); n; --n, ++rr) {
	for (s = *rr; s; s = CDR(s), ++qq) {
	  obj_assign(vm, qq, CAR(s));
	}
      }
      
      obj_assign(vm, pp, fp[-1]);

      ovm_ffree(vm, fp);
    }
    return;
  default:
    if (is_list(q)) {
      unsigned   n;
      struct obj **fp;
      struct obj **qq;

      fp = ovm_falloc(vm, 1);
      
      obj_array_newc(vm, &fp[-1], n = list_len(q));
      for (qq = ARRAY_DATA(fp[-1]); n; --n, ++qq, q = CDR(q)) {
	obj_assign(vm, qq, CAR(q));
      }
      
      obj_assign(vm, pp, fp[-1]);

      ovm_ffree(vm, fp);
      
      return;
    }
  }
  
  ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
  return;
}

static void
obj_array_append(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp, *q = *_ovm_reg(vm, va_arg(ap, unsigned)), **rr, **ss;
  struct obj **fp;
  unsigned   n;
  
  if (obj_type(q) != OBJ_TYPE_ARRAY) {
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }

  fp = ovm_falloc(vm, 1);

  obj_array_newc(vm, &fp[-1], ARRAY_SIZE(p) + ARRAY_SIZE(q));
  for (rr = ARRAY_DATA(fp[-1]), ss = ARRAY_DATA(p), n = ARRAY_SIZE(p); n; --n, ++ss, ++rr) {
    obj_assign(vm, rr, *ss);
  }
  for (ss = ARRAY_DATA(q), n = ARRAY_SIZE(q); n; --n, ++ss, ++rr) {
    obj_assign(vm, rr, *ss);
  }

  obj_assign(vm, pp, fp[-1]);
  
  ovm_ffree(vm, fp);
}

static void
obj_array_at(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp, *q = *_ovm_reg(vm, va_arg(ap, unsigned));
  int        i, n;

  if (obj_type(q) != OBJ_TYPE_INTEGER) {
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }
  
  i = INTVAL(q);
  n = 1;
  slice_idxs(ARRAY_SIZE(p), &i, &n);
  if (n == 0) {
    ovm_error(vm, OBJ_ERRNO_RANGE);
    return;
  }

  obj_assign(vm, pp, ARRAY_DATA(p)[i]);
}

static void
obj_array_at_put(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp;
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));
  struct obj *r = *_ovm_reg(vm, va_arg(ap, unsigned));
  int        i, n;

  if (obj_type(q) != OBJ_TYPE_INTEGER) {
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }

  i = INTVAL(q);
  n = 1;
  slice_idxs(ARRAY_SIZE(p), &i, &n);
  if (n == 0) {
    ovm_error(vm, OBJ_ERRNO_RANGE);
    return;
  }

  obj_assign(vm, &ARRAY_DATA(p)[i], r);
}

static void
obj_array_eq(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp, *q = *_ovm_reg(vm, va_arg(ap, unsigned));
  struct obj **qq, **rr, **fp;
  unsigned n;

  fp = ovm_falloc(vm, 1);

  obj_bool_newc(vm, &fp[-1], 0);

  if (obj_type(q) != OBJ_TYPE_ARRAY
      || (n = ARRAY_SIZE(p) != ARRAY_SIZE(q))
      ) {
    goto done1;
  }

  ovm_pushm(vm, 1, 2);

  for (qq = ARRAY_DATA(p), rr = ARRAY_DATA(q); n; --n, ++qq, ++rr) {
    obj_assign(vm, &R(1), *qq);
    obj_assign(vm, &R(2), *rr);
    ovm_call(vm, 1, OBJ_OP_EQ, 2);
    if (vm->errno != OBJ_ERRNO_NONE)  goto done2;
    if (!BOOLVAL(R(1)))  break;
  }
  if (n == 0)  BOOLVAL(fp[-1]) = 1;

 done2:
  ovm_popm(vm, 1, 2);

 done1:
  if (vm->errno == OBJ_ERRNO_NONE)  obj_assign(vm, pp, fp[-1]);

  ovm_ffree(vm, fp);
}

static void
obj_array_filter(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp;
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned)), **qq, **rr, *r, *s;
  struct obj **fp;
  unsigned n;

  if (!is_list(q)) {
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }

  for (n = 0, r = q; r; r = CDR(r)) {
    if (obj_type(s = CAR(r)) != OBJ_TYPE_BOOLEAN) {
      ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
      return;
    }
    if (BOOLVAL(s))  ++n;
  }

  fp = ovm_falloc(vm, 1);

  if (n > ARRAY_SIZE(p))  n = ARRAY_SIZE(p);
  obj_array_newc(vm, &fp[-1], n);

  for (qq = ARRAY_DATA(fp[-1]), rr = ARRAY_DATA(p); n; ++rr, q = CDR(q)) {
    if (BOOLVAL(CAR(q))) {
      obj_assign(vm, qq, *rr);
      ++qq;
      --n;
    }
  }

  obj_assign(vm, pp, fp[-1]);

 done:
  ovm_ffree(vm, fp);
  
}

static void
obj_array_reverse(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp;
  struct obj **fp, **qq, **rr;
  unsigned n;

  fp = ovm_falloc(vm, 1);

  obj_array_newc(vm, &fp[-1], n = ARRAY_SIZE(p));
  for (qq = ARRAY_DATA(fp[-1]), rr = ARRAY_DATA(p) + n - 1; n; --n, --rr, ++qq) {
    obj_assign(vm, qq, *rr);
  }

  obj_assign(vm, pp, fp[-1]);

  ovm_ffree(vm, fp);
}

static void
_obj_array_slice(struct ovm *vm, struct obj **pp, struct obj **qq, unsigned n)
{
  struct obj **fp, **rr, **ss;

  fp = ovm_falloc(vm, 1);

  obj_array_newc(vm, &fp[-1], n);
  for (rr = ARRAY_DATA(fp[-1]); n; --n, ++qq, ++rr) {
    obj_assign(vm, rr, *qq);
  }

  obj_assign(vm, pp, fp[-1]);

  ovm_ffree(vm, fp);
}

static void
obj_array_size(struct ovm *vm, struct obj **pp, va_list ap)
{
  obj_integer_newc(vm, pp, ARRAY_SIZE(*pp));
}

static void
obj_array_slice(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp;
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));
  struct obj *r = *_ovm_reg(vm, va_arg(ap, unsigned));
  int        i, j, n;
  struct obj **fp, **rr, **ss;

  if (obj_type(q) != OBJ_TYPE_INTEGER || obj_type(r) != OBJ_TYPE_INTEGER) {
    ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
    return;
  }

  i = INTVAL(q);
  n = INTVAL(r);
  slice_idxs((int) ARRAY_SIZE(p), &i, &n);

  _obj_array_slice(vm, pp, &ARRAY_DATA(p)[i], n);
}

static void
obj_array_sort_insert(struct ovm *vm, struct obj **pp)
{
  struct obj *p = *pp, **rr;
  unsigned   n = ARRAY_SIZE(p), f, j, k;

  if (n < 2)  return;

  ovm_pushm(vm, 1, 2);

  for (j = 1; j < n; ++j) {
    for (rr = ARRAY_DATA(p) + j, k = 0; k < j; ++k, --rr) {
      obj_assign(vm, &R(1), rr[-1]);
      obj_assign(vm, &R(2), rr[0]);
      ovm_call(vm, 1, OBJ_OP_GT, 2);
      if (vm->errno != OBJ_ERRNO_NONE)  goto done;
      if (!BOOLVAL(R(1)))  break;
      obj_assign(vm, &R(1), rr[-1]);
      obj_assign(vm, &R(2), rr[0]);
      obj_assign(vm, &rr[-1], R(2));
      obj_assign(vm, &rr[0], R(1));
    }
  }

 done:
  ovm_popm(vm, 1, 2);
}

static void
obj_array_sort_merge(struct ovm *vm, struct obj **pp)
{
  struct obj *p = *pp, **fp, **qq, **rr, **ss;
  unsigned n = ARRAY_SIZE(p), nn, n1, n2;

  if (n < 12) {
    obj_array_sort_insert(vm, pp);
    return;
  }
  
  fp = ovm_falloc(vm, 3);

  ovm_pushm(vm, 1, 2);

  nn = n / 2;
  _obj_array_slice(vm, &fp[-2], ARRAY_DATA(p), nn);
  _obj_array_slice(vm, &fp[-3], ARRAY_DATA(p) + nn, n - nn);

  obj_array_sort_merge(vm, &fp[-2]);
  if (vm->errno != OBJ_ERRNO_NONE)  goto done;
  obj_array_sort_merge(vm, &fp[-3]);
  if (vm->errno != OBJ_ERRNO_NONE)  goto done;
  
  obj_array_newc(vm, &fp[-1], n);
  if (vm->errno != OBJ_ERRNO_NONE)  goto done;
  for (qq = ARRAY_DATA(fp[-1]),
	 rr = ARRAY_DATA(fp[-2]),
	 ss = ARRAY_DATA(fp[-3]),
	 n1 = ARRAY_SIZE(fp[-2]),
	 n2 = ARRAY_SIZE(fp[-3]);
       n1 > 0 || n2 > 0;
       ++qq
       ){
    if (n1 > 0) {
      if (n2 > 0) {
	obj_assign(vm, &R(1), *rr);
	obj_assign(vm, &R(2), *ss);
	ovm_call(vm, 1, OBJ_OP_GT, 2);
	if (vm->errno != OBJ_ERRNO_NONE)  goto done;
	if (BOOLVAL(R(1)))  goto take2;
      }
      goto take1;
    }
    
  take2:
    obj_assign(vm, qq, *ss);
    ++ss;
    --n2;
    continue;
    
  take1:
    obj_assign(vm, qq, *rr);
    ++rr;
    --n1;
  }

 done:
  ovm_popm(vm, 1, 2);

  if (vm->errno == OBJ_ERRNO_NONE)  obj_assign(vm, pp, fp[-1]);

  ovm_ffree(vm, fp);
}

static void
obj_array_sort(struct ovm *vm, struct obj **pp, va_list ap)
{
  obj_array_sort_merge(vm, pp);
}

/***************************************************************************/

static void
_obj_dict_find(struct ovm *vm, struct obj *dict, struct obj *key, struct obj ***pbucket, struct obj ***pprev)
{
  struct obj **bucket, **rr, *r;

  ovm_pushm(vm, 1, 2);

  obj_assign(vm, &R(1), key);
  ovm_call(vm, 1, OBJ_OP_HASH);
  bucket = &DICT_DATA(dict)[INTVAL(R(1)) % DICT_SIZE(dict)];
  if (pbucket)  *pbucket = bucket;

  *pprev = 0;
  for (rr = bucket; r = *rr; rr = &CDR(r)) {
    obj_assign(vm, &R(1), CAR(CAR(r)));
    obj_assign(vm, &R(2), key);
    ovm_call(vm, 1, OBJ_OP_EQ, 2);
    if (BOOLVAL(R(1))) {
      *pprev = rr;
      break;
    }
  }

  ovm_popm(vm, 1, 2);
}

static struct obj *
_obj_dict_at(struct ovm *vm, struct obj *dict, struct obj *key)
{
  struct obj **pp;

  _obj_dict_find(vm, dict, key, 0, &pp);
  return (pp ? CAR(*pp) : 0);
}

static void
_obj_dict_at_put(struct ovm *vm, struct obj *dict, struct obj *key, struct obj *val)
{
  struct obj **bucket, **pp, **qq, *p, **fp;

  _obj_dict_find(vm, dict, key, &bucket, &pp);
  if (pp) {
    obj_assign(vm, &CDR(CAR(*pp)), val);

    return;
  }

  fp = ovm_falloc(vm, 2);
  
  obj_pair_newc(vm, &fp[-2], key, val);
  obj_list_newc(vm, &fp[-1], fp[-2], *bucket);
  obj_assign(vm, bucket, fp[-1]);

  ovm_ffree(vm, fp);
  
  ++DICT_CNT(dict);
}

static void
_obj_dict_del(struct ovm *vm, struct obj *dict, struct obj *key)
{
  struct obj **pp;

  _obj_dict_find(vm, dict, key, 0, &pp);
  if (pp == 0)  return;

  assert(DICT_CNT(dict) > 0);

  obj_assign(vm, pp, CDR(*pp));
  --DICT_CNT(dict);
}

static unsigned
obj_dict_dflt_size(struct ovm *vm)
{
  static const char s[] = "default-size";

  unsigned   result = 32;
  struct obj **fp, *p, *q;

  fp = ovm_falloc(vm, 1);

  obj_string_newc(vm, &fp[-1], 1, sizeof(s) - 1, s);

  if ((p = _obj_dict_at(vm, vm->cl_tbl[OBJ_TYPE_DICT - OBJ_TYPE_BASE], fp[-1]))
      && obj_type(q = CDR(p)) == OBJ_TYPE_INTEGER
      && INTVAL(q) >= 0
      ) {
    result = INTVAL(q);
  }

  ovm_ffree(vm, fp);

  return (result);
}

static void
obj_dict_newc(struct ovm *vm, struct obj **pp, unsigned size)
{
  _obj_array_newc(vm, pp, OBJ_TYPE_DICT, size == 0 ? obj_dict_dflt_size(vm) : size);
}

static int
obj_dict_parse(struct ovm *vm, struct obj **pp, unsigned n, char *p)
{
  int result = -1;
  struct obj **fp;
  char       *st, *q;
  unsigned   f, k;

  trim(&n, &p);

  if (!(n >= 2 && p[0] == '{' && p[n - 1] == '}'))  return (-1);
  ++p;  n -= 2;
  trim(&n, &p);

  fp = ovm_falloc(vm, 3);

  obj_dict_newc(vm, &fp[-1], 0);
  for (;;) {
    st = p;
    f = delim_find(&n, &p, ',');

    q = st;
    k = f ? p - st : n;

    if (!delim_find(&k, &q, ':')
	|| obj_str_parse(vm, &fp[-2], q - st, st) < 0
	|| obj_str_parse(vm, &fp[-3], k - 1, q + 1) < 0
	) {
      goto done;
    }

    _obj_dict_at_put(vm, fp[-1], fp[-2], fp[-3]);

    if (!f)  break;

    ++p;
    --n;
  }

  obj_assign(vm, pp, fp[-1]);

  result = 0;

 done:
  ovm_ffree(vm, fp);

  return (result);
}

static void
obj_dict_tostring(struct ovm *vm, struct obj **pp, struct obj *q)
{
  struct obj **fp, **rr, **ss, *t;
  unsigned   i, n;

  n = 2 + 3 * DICT_CNT(q);
  if (DICT_CNT(q) > 1)  n += DICT_CNT(q) - 1;

  fp = ovm_falloc(vm, 1);

  obj_array_newc(vm, &fp[-1], n);
  rr = ARRAY_DATA(fp[-1]);
  
  obj_string_newc(vm, rr, 1, 1, "{");
  ++rr;
  for (i = 0, ss = ARRAY_DATA(q), n = ARRAY_SIZE(q); n; --n, ++ss) {
    for (t = *ss; t; t = CDR(t), ++i) {
      if (i > 0) {
	obj_string_newc(vm, rr, 1, 2, ", ");
	++rr;
      }
      obj_tostring(vm, rr, CAR(CAR(t)));
      ++rr;
      obj_string_newc(vm, rr, 1, 2, ": ");
      ++rr;
      obj_tostring(vm, rr, CDR(CAR(t)));
      ++rr;
    }
  }
  obj_string_newc(vm, rr, 1, 1, "}");

  obj_string_newv(vm, pp, fp[-1]);

  ovm_ffree(vm, fp);
}

static void
obj_dict_new(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned)), **fp, **qq, *r, *s;
  unsigned n;

  switch (obj_type(q)) {
  case OBJ_TYPE_INTEGER:
    if (INTVAL(q) < 0) {
      ovm_error(vm, OBJ_ERRNO_BAD_VALUE);
      return;
    }

    obj_dict_newc(vm, pp, INTVAL(q));
    return;
  case OBJ_TYPE_ARRAY:
    for (qq = ARRAY_DATA(q), n = ARRAY_SIZE(q); n; --n, ++q) {
      if (obj_type(*qq) != OBJ_TYPE_PAIR) {
	ovm_error(vm, OBJ_ERRNO_BAD_VALUE);
	return;
      }
    }

    fp = ovm_falloc(vm, 1);

    obj_dict_newc(vm, &fp[-1], 0);
    for (qq = ARRAY_DATA(q), n = ARRAY_SIZE(q); n; --n, ++q) {
      r = *qq;
      _obj_dict_at_put(vm, fp[-1], CAR(r), CDR(r));
    }

    obj_assign(vm, pp, fp[-1]);

    ovm_ffree(vm, fp);

    return;

  case OBJ_TYPE_DICT:
    fp = ovm_falloc(vm, 1);

    obj_dict_newc(vm, &fp[-1], 0);
    for (qq = ARRAY_DATA(q), n = ARRAY_SIZE(q); n; --n, ++q) {
      for (r = *qq; r; r = CDR(r)) {
	s = CAR(r);
	_obj_dict_at_put(vm, fp[-1], CAR(s), CDR(s));
      }
    }

    obj_assign(vm, pp, fp[-1]);

    ovm_ffree(vm, fp);

    return;

  default:
    if (is_list(q)) {
      for (qq = ARRAY_DATA(q), n = ARRAY_SIZE(q); n; --n, ++q) {
	if (obj_type(*qq) != OBJ_TYPE_PAIR) {
	  ovm_error(vm, OBJ_ERRNO_BAD_VALUE);
	  return;
	}
      }
      
      fp = ovm_falloc(vm, 1);
      
      obj_dict_newc(vm, &fp[-1], 0);
      for (qq = ARRAY_DATA(q), n = ARRAY_SIZE(q); n; --n, ++q) {
	r = *qq;
	_obj_dict_at_put(vm, fp[-1], CAR(r), CDR(r));
      }
      
      obj_assign(vm, pp, fp[-1]);
      
      ovm_ffree(vm, fp);
      
      return;
    }
  }

  ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
}

static void
obj_dict_append(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp, *q = *_ovm_reg(vm, va_arg(ap, unsigned));
  struct obj **qq, *r;
  unsigned   n;

  switch (obj_type(q)) {
  case OBJ_TYPE_DICT:
    for (qq = DICT_DATA(q), n = DICT_SIZE(q); n; --n, ++q) {
      for (r = *qq; r; r = CDR(r)) {
	_obj_dict_at_put(vm, p, CAR(CAR(r)), CDR(CAR(r)));
      }
    }
    return;
  default:
    ;
  }
  
  ovm_error(vm, OBJ_ERRNO_BAD_TYPE);
}

static void
obj_dict_at(struct ovm *vm, struct obj **pp, va_list ap)
{
  obj_assign(vm, pp, _obj_dict_at(vm, *pp, *_ovm_reg(vm, va_arg(ap, unsigned))));
}

static void
obj_dict_at_put(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *q = *_ovm_reg(vm, va_arg(ap, unsigned));
  struct obj *r = *_ovm_reg(vm, va_arg(ap, unsigned));
  
  _obj_dict_at_put(vm, *pp, q, r);
}

static void
obj_dict_count(struct ovm *vm, struct obj **pp, va_list ap)
{
  obj_integer_newc(vm, pp, DICT_CNT(*pp));
}

static void
obj_dict_del(struct ovm *vm, struct obj **pp, va_list ap)
{
  _obj_dict_del(vm, *pp, *_ovm_reg(vm, va_arg(ap, unsigned)));
}

static void
obj_dict_keys(struct ovm *vm, struct obj **pp, va_list ap)
{
  struct obj *p = *pp, **fp, **rr, **ss, *s;
  unsigned   n;

  fp = ovm_falloc(vm, 1);

  rr = &fp[-1];
  for (ss = ARRAY_DATA(p), n = ARRAY_SIZE(p); n; --n, ++ss) {
    for (s = *ss; s; s = CDR(s)) {
      obj_list_newc(vm, rr, CAR(CAR(s)), 0);
      rr = &CDR(*rr);
    }
  }

  obj_assign(vm, pp, fp[-1]);

  ovm_ffree(vm, fp);
}

/***************************************************************************/

void (*op_func_tbl[OBJ_NUM_TYPES][OBJ_NUM_OPS])(struct ovm *, struct obj **, va_list) = {
  /* OBJ_TYPE_OBJECT */
  { 0 },
  
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
    obj_nil_filter,		/* OBJ_OP_FILTER */
    0,				/* OBJ_OP_GT */
    obj_nil_hash,		/* OBJ_OP_HASH */
    0,				/* OBJ_OP_JOIN */
    0,				/* OBJ_OP_KEYS */
    0,				/* OBJ_OP_LT */
    0,				/* OBJ_OP_MINUS */
    0,				/* OBJ_OP_MOD */
    0,				/* OBJ_OP_MULT */
    0,				/* OBJ_OP_NOT */
    0,				/* OBJ_OP_OR */
    obj_nil_reverse,		/* OBJ_OP_REVERSE */
    obj_nil_size,		/* OBJ_OP_SIZE */
    obj_nil_slice,		/* OBJ_OP_SLICE */
    0,				/* OBJ_OP_SORT */
    0,				/* OBJ_OP_SPLIT */
    0,				/* OBJ_OP_SUB */
    0				/* OBJ_OP_XOR */
  },

  /* OBJ_TYPE_POINTER */
  { 0 },
  
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
    0,				/* OBJ_OP_FILTER */
    0,				/* OBJ_OP_GT */
    obj_bool_hash,		/* OBJ_OP_HASH */
    0,				/* OBJ_OP_JOIN */
    0,				/* OBJ_OP_KEYS */
    0,				/* OBJ_OP_LT */
    0,				/* OBJ_OP_MINUS */
    0,				/* OBJ_OP_MOD */
    0,				/* OBJ_OP_MULT */
    obj_bool_not,		/* OBJ_OP_NOT */
    obj_bool_or,		/* OBJ_OP_OR */
    0,				/* OBJ_OP_REVERSE */
    0,				/* OBJ_OP_SIZE */
    0,				/* OBJ_OP_SLICE */
    0,				/* OBJ_OP_SORT */
    0,				/* OBJ_OP_SPLIT */
    0,				/* OBJ_OP_SUB */
    obj_bool_xor		/* OBJ_OP_XOR */
  },

  /* OBJ_TYPE_NUMBER */
  { 0 },

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
    0,				/* OBJ_OP_FILTER */
    obj_integer_gt,		/* OBJ_OP_GT */
    obj_integer_hash,		/* OBJ_OP_HASH */
    0,				/* OBJ_OP_JOIN */
    0,				/* OBJ_OP_KEYS */
    obj_integer_lt,		/* OBJ_OP_LT */
    obj_integer_minus,		/* OBJ_OP_MINUS */
    obj_integer_mod,		/* OBJ_OP_MOD */
    obj_integer_mult,		/* OBJ_OP_MULT */
    0,				/* OBJ_OP_NOT */
    obj_integer_or,		/* OBJ_OP_OR */
    0,				/* OBJ_OP_REVERSE */
    0,				/* OBJ_OP_SIZE */
    0,				/* OBJ_OP_SLICE */
    0,				/* OBJ_OP_SORT */
    0,				/* OBJ_OP_SPLIT */
    obj_integer_sub,		/* OBJ_OP_SUB */
    obj_integer_xor		/* OBJ_OP_XOR */
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
    0,				/* OBJ_OP_FILTER */
    obj_float_gt,		/* OBJ_OP_GT */
    obj_float_hash,		/* OBJ_OP_HASH */
    0,				/* OBJ_OP_JOIN */
    0,				/* OBJ_OP_KEYS */
    obj_float_lt,		/* OBJ_OP_LT */
    obj_float_minus,		/* OBJ_OP_MINUS */
    0,				/* OBJ_OP_MOD */
    obj_float_mult,		/* OBJ_OP_MULT */
    0,				/* OBJ_OP_NOT */
    0,				/* OBJ_OP_OR */
    0,				/* OBJ_OP_REVERSE */
    0,				/* OBJ_OP_SIZE */
    0,				/* OBJ_OP_SLICE */
    0,				/* OBJ_OP_SORT */
    0,				/* OBJ_OP_SPLIT */
    obj_float_sub,		/* OBJ_OP_SUB */
    0				/* OBJ_OP_XOR */
  },

  /* OBJ_TYPE_BLOCK */
  { 0 },

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
    0,				/* OBJ_OP_FILTER */
    obj_string_gt,		/* OBJ_OP_GT */
    obj_string_hash,		/* OBJ_OP_HASH */
    obj_string_join,		/* OBJ_OP_JOIN */
    0,				/* OBJ_OP_KEYS */
    obj_string_lt,		/* OBJ_OP_LT */
    0,				/* OBJ_OP_MINUS */
    0,				/* OBJ_OP_MOD */
    0,				/* OBJ_OP_MULT */
    0,				/* OBJ_OP_NOT */
    0,				/* OBJ_OP_OR */
    obj_string_reverse,		/* OBJ_OP_REVERSE */
    _obj_string_size,		/* OBJ_OP_SIZE */
    obj_string_slice,		/* OBJ_OP_SLICE */
    0,				/* OBJ_OP_SORT */
    obj_string_split,		/* OBJ_OP_SPLIT */
    0,				/* OBJ_OP_SUB */
    0				/* OBJ_OP_XOR */
  },

  /* OBJ_TYPE_BYTES */
  { 0 },

  /* OBJ_TYPE_WORDS */
  { 0 },

  /* OBJ_TYPE_DWORDS */
  { 0 },

  /* OBJ_TYPE_QWORDS */
  { 0 },

  /* OBJ_TYPE_BITS */
  { 0 },

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
    0,				/* OBJ_OP_FILTER */
    0,				/* OBJ_OP_GT */
    0,				/* OBJ_OP_HASH */
    0,				/* OBJ_OP_JOIN */
    0,				/* OBJ_OP_KEYS */
    0,				/* OBJ_OP_LT */
    0,				/* OBJ_OP_MINUS */
    0,				/* OBJ_OP_MOD */
    0,				/* OBJ_OP_MULT */
    0,				/* OBJ_OP_NOT */
    0,				/* OBJ_OP_OR */
    0,				/* OBJ_OP_REVERSE */
    0,				/* OBJ_OP_SIZE */
    0,				/* OBJ_OP_SLICE */
    0,				/* OBJ_OP_SORT */
    0,				/* OBJ_OP_SPLIT */
    0,				/* OBJ_OP_SUB */
    0				/* OBJ_OP_XOR */
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
    0,				/* OBJ_OP_FILTER */
    0,				/* OBJ_OP_GT */
    obj_pair_hash,		/* OBJ_OP_HASH */
    0,				/* OBJ_OP_JOIN */
    0,				/* OBJ_OP_KEYS */
    0,				/* OBJ_OP_LT */
    0,				/* OBJ_OP_MINUS */
    0,				/* OBJ_OP_MOD */
    0,				/* OBJ_OP_MULT */
    0,				/* OBJ_OP_NOT */
    0,				/* OBJ_OP_OR */
    obj_pair_reverse,		/* OBJ_OP_REVERSE */
    0,				/* OBJ_OP_SIZE */
    0,				/* OBJ_OP_SLICE */
    0,				/* OBJ_OP_SORT */
    0,				/* OBJ_OP_SPLIT */
    0,				/* OBJ_OP_SUB */
    0				/* OBJ_OP_XOR */
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
    obj_list_filter,		/* OBJ_OP_FILTER */
    0,				/* OBJ_OP_GT */
    obj_list_hash,		/* OBJ_OP_HASH */
    0,				/* OBJ_OP_JOIN */
    0,				/* OBJ_OP_KEYS */
    0,				/* OBJ_OP_LT */
    0,				/* OBJ_OP_MINUS */
    0,				/* OBJ_OP_MOD */
    0,				/* OBJ_OP_MULT */
    0,				/* OBJ_OP_NOT */
    0,				/* OBJ_OP_OR */
    obj_list_reverse,		/* OBJ_OP_REVERSE */
    obj_list_size,		/* OBJ_OP_SIZE */
    obj_list_slice,		/* OBJ_OP_SLICE */
    0,				/* OBJ_OP_SORT */
    0,				/* OBJ_OP_SPLIT */
    0,				/* OBJ_OP_SUB */
    0				/* OBJ_OP_XOR */
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
    obj_array_eq,		/* OBJ_OP_EQ */
    obj_array_filter,		/* OBJ_OP_FILTER */
    0,				/* OBJ_OP_GT */
    0,				/* OBJ_OP_HASH */
    0,				/* OBJ_OP_JOIN */
    0,				/* OBJ_OP_KEYS */
    0,				/* OBJ_OP_LT */
    0,				/* OBJ_OP_MINUS */
    0,				/* OBJ_OP_MOD */
    0,				/* OBJ_OP_MULT */
    0,				/* OBJ_OP_NOT */
    0,				/* OBJ_OP_OR */
    obj_array_reverse,		/* OBJ_OP_REVERSE */
    obj_array_size,		/* OBJ_OP_SIZE */
    obj_array_slice,		/* OBJ_OP_SLICE */
    obj_array_sort,		/* OBJ_OP_SORT */
    0,				/* OBJ_OP_SPLIT */
    0,				/* OBJ_OP_SUB */
    0				/* OBJ_OP_XOR */
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
    obj_bad_method,		/* OBJ_OP_FILTER */
    0,				/* OBJ_OP_GT */
    0,				/* OBJ_OP_HASH */
    0,				/* OBJ_OP_JOIN */
    obj_dict_keys,		/* OBJ_OP_KEYS */
    0,				/* OBJ_OP_LT */
    0,				/* OBJ_OP_MINUS */
    0,				/* OBJ_OP_MOD */
    0,				/* OBJ_OP_MULT */
    0,				/* OBJ_OP_NOT */
    0,				/* OBJ_OP_OR */
    obj_bad_method,		/* OBJ_OP_REVERSE */
    0,				/* OBJ_OP_SIZE */
    obj_bad_method,		/* OBJ_OP_SLICE */
    obj_bad_method,		/* OBJ_OP_SORT */
    0,				/* OBJ_OP_SPLIT */
    0,				/* OBJ_OP_SUB */
    0				/* OBJ_OP_XOR */
  }
};

/***************************************************************************/

void
ovm_fini(struct ovm *vm)
{
  /** @todo Free all obj data (strings, arrays) */

  unsigned i;

  for (i = 0; i < _ARRAY_SIZE(vm->cl_tbl); ++i) {
    obj_free(vm, vm->cl_tbl[i]);
  }
  
  if (vm->obj_pool)  free(vm->obj_pool);
  if (vm->stack)     free(vm->stack);
}

void
ovm_init(struct ovm *vm,
	    unsigned      obj_pool_size,
	    void          *obj_pool,
	    unsigned      work_size,
	    void          *work,
	    unsigned      stack_size,
	    void          *stack
	    )
{
  struct _list *li;
  struct obj   *p;
  unsigned     i, n;

  memset(vm, 0, sizeof(*vm));
  
  li = &vm->obj_list._list[vm->obj_list.idx_alloced = 0];
  list_init(li);
  li = &vm->obj_list._list[vm->obj_list.idx_free = 1];
  list_init(li);
  obj_pool_size /= sizeof(struct obj);
  vm->obj_pool = (obj_t) obj_pool;
  for (p = vm->obj_pool, n = obj_pool_size; n; --n, ++p) {
    list_insert(p->_list_node, list_end(li));
  }

  memset(work, 0, work_size);
  work_size /= sizeof(obj_t);
  vm->work = (obj_t *) work;
  vm->work_end = vm->work + work_size;

  stack_size /= sizeof(obj_t);
  vm->stack = (obj_t *) stack;
  vm->stack_end = vm->stack + stack_size;
  vm->sp = vm->stack_end;

  vm->errno = OBJ_ERRNO_NONE;

  for (i = 0; i < _ARRAY_SIZE(vm->cl_tbl); ++i) {
    obj_dict_newc(vm, &vm->cl_tbl[i], 32);
    if (vm->errno != OBJ_ERRNO_NONE)  goto done;
  }

 done:
  if (vm->errno != OBJ_ERRNO_NONE)  ovm_fini(vm);
}

void
ovm_pick(struct ovm *vm, unsigned r1, unsigned ofs)
{
  obj_assign(vm, _ovm_reg(vm, r1), *_ovm_pick(vm, ofs));
}

void
ovm_dropn(struct ovm *vm, unsigned n)
{
  struct obj **p;
  unsigned   nn;

  assert((vm->sp + n) <= vm->stack_end);

  for (p = vm->sp, nn = n; nn; --nn, ++p)  obj_release(vm, *p);

  vm->sp += n;
}

void
ovm_drop(struct ovm *vm)
{
  ovm_dropn(vm, 1);
}

void
ovm_popm(struct ovm *vm, unsigned r1, unsigned n)
{
  struct obj **qq;

  assert((vm->sp + n) <= vm->stack_end);
  assert((r1 + n) <= _ARRAY_SIZE(vm->reg));

  for (qq = &vm->reg[r1 + n - 1]; n; --n, --qq, ++vm->sp) {
    obj_assign(vm, qq, *vm->sp);
    obj_release(vm, *vm->sp);
  }
}

void
ovm_pop(struct ovm *vm, unsigned r1)
{
  ovm_popm(vm, r1, 1);
}

void
ovm_pushm(struct ovm *vm, unsigned r1, unsigned n)
{
  struct obj **pp, **qq;

  assert((vm->sp - n) >= vm->stack);
  assert((r1 + n) <= _ARRAY_SIZE(vm->reg));

  pp = vm->sp - n;
  memset(pp, 0, n * sizeof(*pp));
  for (qq = &vm->reg[r1]; n; --n, ++qq) {
    obj_assign(vm, --vm->sp, *qq);
  }
}

void
ovm_push(struct ovm *vm, unsigned r1)
{
  ovm_pushm(vm, r1, 1);
}

void
ovm_load(struct ovm *vm, unsigned r1, obj_t *work)
{
  assert(work >= vm->work && work < vm->work_end);

  obj_assign(vm, _ovm_reg(vm, r1), *work);
}

void
ovm_store(struct ovm *vm, unsigned r1, obj_t *work)
{
  assert(work >= vm->work && work < vm->work_end);

  obj_assign(vm, work, *_ovm_reg(vm, r1));
}

unsigned
ovm_type(struct ovm *vm, unsigned r1)
{
  return (obj_type(*_ovm_reg(vm, r1)));
}

unsigned
obj_type_is_subtype_of(unsigned type, unsigned parent)
{
  for ( ; type != OBJ_TYPE_OBJECT; type = obj_type_parent(type)) {
    if (type == parent)  return (1);
  }

  return (0);
}

void
ovm_move(struct ovm *vm, unsigned r1, unsigned r2)
{
  obj_assign(vm, _ovm_reg(vm, r1), *_ovm_reg(vm, r2));
}

int
ovm_errno(struct ovm *vm)
{
  return (vm->errno);
}

void
ovm_err_clr(struct ovm *vm)
{
  vm->errno = OBJ_ERRNO_NONE;
}

void
ovm_err_hook_set(struct ovm *vm, void (*func)(struct ovm *))
{
  vm->err_hook = func;
}

void
ovm_newc(struct ovm *vm, unsigned r1, unsigned type, ...)
{
  struct obj **pp, *q;
  va_list ap;

  pp = _ovm_reg(vm, r1);

  if (vm->errno != OBJ_ERRNO_NONE)  return;

  va_start(ap, type);

  switch (type) {
  case OBJ_TYPE_POINTER:
    obj_ptr_newc(vm, pp, va_arg(ap, void *));
    break;
  case OBJ_TYPE_BOOLEAN:
    obj_bool_newc(vm, pp, va_arg(ap, unsigned));
    break;
  case OBJ_TYPE_INTEGER:
    obj_integer_newc(vm, pp, va_arg(ap, obj_integer_val_t));
    break;
  case OBJ_TYPE_FLOAT:
    obj_float_newc(vm, pp, va_arg(ap, obj_float_val_t));
    break;
  case OBJ_TYPE_STRING:
    {
      unsigned len = va_arg(ap, unsigned);
      char     *s  = va_arg(ap, char *);
      
      obj_string_newc(vm, pp, 1, len, s);
    }
    break;
  case OBJ_TYPE_ARRAY:
    obj_array_newc(vm, pp, va_arg(ap, unsigned));
    break;
  case OBJ_TYPE_DICT:
    obj_dict_newc(vm, pp, va_arg(ap, unsigned));
    break;
  default:
    assert(0);
  }

  va_end(ap);
}

void
ovm_news(struct ovm *vm, unsigned r1, unsigned len, char *s)
{
  obj_str_parse(vm, _ovm_reg(vm, r1), len, s);
}

void
ovm_new(struct ovm *vm, unsigned r1, unsigned type, ...)
{
  struct obj **pp = _ovm_reg(vm, r1);
  va_list ap;

  if (vm->errno != OBJ_ERRNO_NONE)  return;

  va_start(ap, type);

  switch (type) {
  case OBJ_TYPE_NIL:
    obj_nil_new(vm, pp, ap);
    break;
  case OBJ_TYPE_BOOLEAN:
    obj_bool_new(vm, pp, ap);
    break;
  case OBJ_TYPE_INTEGER:
    obj_integer_new(vm, pp, ap);
    break;
  case OBJ_TYPE_FLOAT:
    obj_float_new(vm, pp, ap);
    break;
  case OBJ_TYPE_STRING:
    obj_string_new(vm, pp, ap);
    break;
  case OBJ_TYPE_PAIR:
    obj_pair_new(vm, pp, ap);
    break;
  case OBJ_TYPE_LIST:
    obj_list_new(vm, pp, ap);
    break;
  case OBJ_TYPE_ARRAY:
    obj_array_new(vm, pp, ap);
    break;
  case OBJ_TYPE_DICT:
    obj_dict_new(vm, pp, ap);
    break;
  default:
    assert(0);
  }

  va_end(ap);
}


void
ovm_call(struct ovm *vm, unsigned r1, unsigned op, ...)
{
  struct obj **pp;
  va_list    ap;
  unsigned   type;
  void       (*f)(struct ovm *, struct obj **, va_list) = 0;

  if (vm->errno != OBJ_ERRNO_NONE)  return;

  pp = _ovm_reg(vm, r1);

  va_start(ap, op);

  for (type = obj_type(*pp); type != OBJ_TYPE_OBJECT; type = obj_type_parent(type)) {
    if (f = op_func_tbl[type - OBJ_TYPE_BASE][op]) {
      (*f)(vm, pp, ap);
      break;
    }
  }
  if (f == 0)  ovm_error(vm, OBJ_ERRNO_BAD_METHOD);
  
  va_end(ap);
}


void *
ovm_ptr_val(struct ovm *vm, unsigned r1)
{
  struct obj *p = *_ovm_reg(vm, r1);

  assert(obj_type(p) == OBJ_TYPE_POINTER);

  return (PTRVAL(p));
}

unsigned
ovm_bool_val(struct ovm *vm, unsigned r1)
{
  struct obj *p = *_ovm_reg(vm, r1);

  assert (obj_type(p) == OBJ_TYPE_BOOLEAN);

  return (BOOLVAL(p) != 0);
}

obj_integer_val_t
ovm_integer_val(struct ovm *vm, unsigned r1)
{
  struct obj *p = *_ovm_reg(vm, r1);

  assert (obj_type(p) == OBJ_TYPE_INTEGER);

  return (INTVAL(p));
}

obj_float_val_t
ovm_float_val(struct ovm *vm, unsigned r1)
{
  struct obj *p = *_ovm_reg(vm, r1);

  assert(obj_type(p) == OBJ_TYPE_FLOAT);

  return (FLOATVAL(p));
}

char *
ovm_string_val(struct ovm *vm, unsigned r1)
{
  struct obj *p = *_ovm_reg(vm, r1);

  assert(obj_type(p) == OBJ_TYPE_STRING);

  return (STR_DATA(p));
}

unsigned
ovm_string_size(struct ovm *vm, unsigned r1)
{
  struct obj *p = *_ovm_reg(vm, r1);

  assert(obj_type(p) == OBJ_TYPE_STRING);

  return (STR_SIZE(p) - 1);
}
