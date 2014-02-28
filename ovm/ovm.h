/** ************************************************************************

\file ovm.h

Public interface for Object Virtual Machine

***************************************************************************/

typedef long long   obj_integer_val_t;
typedef long double obj_float_val_t;

struct _list {
  struct _list *prev, *next;
};

/** @brief Object types */

enum obj_type {
  OBJ_TYPE_BASE = 0x55aa1231,

  OBJ_TYPE_OBJECT = OBJ_TYPE_BASE, /**< Parent of all types, not instantiable */
  OBJ_TYPE_NIL,			/**< Type of NIL  */
  OBJ_TYPE_POINTER,
  OBJ_TYPE_BOOLEAN,		/**< Boolean */
  OBJ_TYPE_NUMBER,		/**< Number */
  OBJ_TYPE_INTEGER,		/**< Fixed-point number */
  OBJ_TYPE_FLOAT,		/**< Floating-point number  */
  OBJ_TYPE_BLOCK,
  OBJ_TYPE_STRING,		/**< String of characters */
  OBJ_TYPE_BYTES,
  OBJ_TYPE_WORDS,
  OBJ_TYPE_DWORDS,
  OBJ_TYPE_QWORDS,
  OBJ_TYPE_BITS,
  OBJ_TYPE_DPTR,		/**< Dual-pointer */
  OBJ_TYPE_PAIR,		/**< Pair of objects */
  OBJ_TYPE_LIST,		/**< List */
  OBJ_TYPE_ARRAY,		/**< Array */
  OBJ_TYPE_DICT,		/**< Dictionary */
  OBJ_TYPE_LAST,

  OBJ_NUM_TYPES  = OBJ_TYPE_LAST - OBJ_TYPE_BASE
};

struct obj {
  struct _list  _list_node[1];
  unsigned      ref_cnt;
  enum obj_type type;
  union {
    void *ptrval;
#define PTRVAL(x)  ((x)->val.ptrval)
    unsigned boolval;
#define BOOLVAL(x)  ((x)->val.boolval)
    obj_integer_val_t intval;
#define INTVAL(x)  ((x)->val.intval)
    obj_float_val_t floatval;
#define FLOATVAL(x)  ((x)->val.floatval)
    struct objval_block {
      unsigned size;
      void     *ptr;
    } blockval;
    struct objval_string {
      unsigned size;
      char     *data;
    } strval;
#define STR_SIZE(x)  ((x)->val.strval.size)
#define STR_DATA(x)  ((x)->val.strval.data)
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
#define CAR(x)  ((x)->val.dptrval.car)
#define CDR(x)  ((x)->val.dptrval.cdr)
    struct objval_array {
      unsigned   size;
      struct obj **data;
    } arrayval;
#define ARRAY_SIZE(x)  ((x)->val.arrayval.size)
#define ARRAY_DATA(x)  ((x)->val.arrayval.data)
    struct objval_dict {
      struct objval_array base;
      unsigned            cnt;
    } dictval;
#define DICT_SIZE(x)  ((x)->val.dictval.base.size)
#define DICT_DATA(x)  ((x)->val.dictval.base.data)
#define DICT_CNT(x)   ((x)->val.dictval.cnt)
  } val;
  void *dummy[1];		/* Pad up to size 32 (power of 2) */
};
typedef struct obj *obj_t, *obj_var[1];

enum {
  OVM_NUM_REGS = 8
};

struct ovm {
  struct obj *obj_pool;
  struct obj **work, **work_end;
  struct obj **stack, **stack_end;

  struct {
    struct _list _list[2];
    unsigned     idx_alloced, idx_free;
  } obj_list;
  struct obj *reg[OVM_NUM_REGS];
  struct obj **sp;
  struct obj *cl_tbl[OBJ_NUM_TYPES];
  int        errno;
  void       (*err_hook)(struct ovm *);
};

enum {
  OBJ_ERRNO_NONE       = 0,	/**< No error */
  OBJ_ERRNO_MEM        = -1,	/**< Out of memory */
  OBJ_ERRNO_BAD_METHOD = -2,	/**< Invalid method */
  OBJ_ERRNO_BAD_REG    = -3,	/**< Invalid register */
  OBJ_ERRNO_BAD_TYPE   = -4,	/**< Invalid type */
  OBJ_ERRNO_BAD_VALUE  = -5,	/**< Invalid value */
  OBJ_ERRNO_RANGE      = -6	/**< Index out of range */
};

#define R0  0
#define R1  1
#define R2  2
#define R3  3
#define R4  4
#define R5  5
#define R6  6
#define R7  7

void ovm_init(struct ovm *vm, unsigned obj_pool_size, void *obj_pool, unsigned work_size, void *work, unsigned stack_size, void *stack);
void ovm_fini(struct ovm *vm);

void ovm_pick(struct ovm *vm, unsigned r1, unsigned ofs);
void ovm_dropn(struct ovm *vm, unsigned n);
void ovm_drop(struct ovm *vm);
void ovm_popm(struct ovm *vm, unsigned r1, unsigned n);
void ovm_pop(struct ovm *vm, unsigned r1);
void ovm_pushm(struct ovm *vm, unsigned r1, unsigned n);
void ovm_push(struct ovm *vm, unsigned r1);
void ovm_move(struct ovm *vm, unsigned r1, unsigned r2);
void ovm_load(struct ovm *vm, unsigned r1, obj_t *work);
void ovm_store(struct ovm *vm, unsigned r1, obj_t *work);
void ovm_cl_dict(struct ovm *vm, unsigned type, unsigned r1);
unsigned ovm_type(struct ovm *vm, unsigned r1);
unsigned obj_type_parent(unsigned type);

enum obj_op {
  OBJ_OP_ABS,			/**< Absoulte value */
  OBJ_OP_ADD,			/**< Arithmetic add */
  OBJ_OP_AND,			/**< Bitwise or boolean and */
  OBJ_OP_APPEND,		/**< Append */
  OBJ_OP_AT,			/**< Keyed collection lookup */
  OBJ_OP_AT_PUT,		/**< Keyed collection update */
  OBJ_OP_CAR,			/**< First element of pair or list */
  OBJ_OP_CDR,			/**< Second element of pair, or rest of list */
  OBJ_OP_COUNT,			/**< Number of objects in collection */
  OBJ_OP_DEL,			/**< Keyed collection delete */
  OBJ_OP_DIV,			/**< Arithmetic divide */
  OBJ_OP_EQ,			/**< Test for equality */
  OBJ_OP_FILTER,		/**< Apply boolean filter */
  OBJ_OP_GT,			/**< Arithmetic > */
  OBJ_OP_HASH,			/**< Hash */
  OBJ_OP_JOIN,			/**< String join, with separator */
  OBJ_OP_KEYS,			/**< Keys in keyed collection */
  /* OBJ_OP_LSH */
  OBJ_OP_LT,			/**< Arithmetic < */
  OBJ_OP_MINUS,			/**< Arithmetic negation */
  OBJ_OP_MOD,			/**< Arithmetic modulus */
  OBJ_OP_MULT,			/**< Arithmetic multiplication */
  OBJ_OP_NOT,			/**< Bitwise one's complement or boolean negation */
  OBJ_OP_OR,			/**< Bitwise or boolean or */
  OBJ_OP_REVERSE,		/**< Reverse ordered collection */
  /* OBJ_OP_RSH */
  OBJ_OP_SIZE,			/**< Size of collection */
  OBJ_OP_SLICE,			/**< Slice subset of collection */
  OBJ_OP_SORT,			/**< Sort collection */
  OBJ_OP_SPLIT,			/**< Split string, with delimeter */
  OBJ_OP_SUB,			/**< Arithmetic subtraction */
  OBJ_OP_XOR,			/**< Bitwise or boolean exclusive-or */
  OBJ_NUM_OPS
};

void ovm_call(struct ovm *vm, unsigned r1, unsigned op, ...);

void ovm_newc(struct ovm *vm, unsigned r1, unsigned type, ...);
void ovm_new(struct ovm *vm, unsigned r1, unsigned type, ...);
void ovm_news(struct ovm *vm, unsigned r1, unsigned len, char *s);

void *            ovm_ptr_val(struct ovm *vm, unsigned r1);
unsigned          ovm_bool_val(struct ovm *vm, unsigned r1);
obj_integer_val_t ovm_integer_val(struct ovm *vm, unsigned r1);
obj_float_val_t   ovm_float_val(struct ovm *vm, unsigned r1);
char *            ovm_string_val(struct ovm *vm, unsigned r1);
unsigned          ovm_string_size(struct ovm *vm, unsigned r1);
