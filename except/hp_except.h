#include <setjmp.h>

struct hp_except_frame {
    volatile struct hp_except_frame *prev;
    jmp_buf                         jmp_buf;
    int                             code;
    void                            *arg;
};

volatile struct hp_except_frame * volatile hp_except_frp;

#define HP_EXCEPT_CODE  (hp_except_frp->code)
#define HP_EXCEPT_ARG   (hp_except_frp->arg)

void hp_except_raise(int code, void *arg);

#define HP_EXCEPT_RAISE(x, y)  (hp_except_raise((x), (y)))

#define HP_EXCEPT_RERAISE  goto hp_except_reraise

#define HP_EXCEPT_BEGIN(fr)					    \
  {								    \
    volatile int           hp_setjmp_rc;			    \
    volatile unsigned char hp_caughtf;				    \
    								    \
    (fr)->prev = hp_except_frp;						\
    hp_except_frp = (fr);						\
    									\
    hp_setjmp_rc = setjmp(* (jmp_buf *) &(fr)->jmp_buf);		\
    hp_caughtf   = 0;                                                

#define HP_EXCEPT_TRY_BEGIN			\
    if (hp_setjmp_rc == 0) {

#define HP_EXCEPT_TRY_END			\
    }

#define HP_EXCEPT_CATCH_BEGIN(x)					\
    if (hp_setjmp_rc != 0 && (x)) {					\
      hp_caughtf = 1;                                                  

#define HP_EXCEPT_CATCH_END						\
      goto hp_except_caught_check;					\
    }                                                                  

#define HP_EXCEPT_NONE_BEGIN			\
    if (hp_setjmp_rc == 0) {

#define HP_EXCEPT_NONE_END			\
    }

#define HP_EXCEPT_END							\
  hp_except_reraise:							\
    hp_caughtf = 0;							\
  									\
  hp_except_caught_check:						\
  hp_except_frp = (fr)->prev;						\
  									\
    if (hp_setjmp_rc != 0 && !hp_caughtf) {				\
      hp_except_raise((fr)->code, (fr)->arg);				\
    }									\
  }

enum {
  HP_EXCEPT_NO_MEM = 1,		/* Memory allocation failed */
  HP_EXCEPT_BAD_REF,		/* Bad reference */
  HP_EXCEPT_IDX_RANGE,		/* Index out of range */
  HP_EXCEPT_VAL_RANGE		/* Value out of range */
};


#ifdef __UNIT_TEST__
struct unit_test {
  unsigned char frp_nil;
  struct {
    unsigned char raise;
    int           code;
    void          *arg;
  } foo;

  struct {
    unsigned char catch;
    unsigned char caught;
    int           caught_code;
    void          *caught_arg;
    unsigned char reraise;
    unsigned char none;
  } bar, test;
};
#endif

