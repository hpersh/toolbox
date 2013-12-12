
void hp_assert_fail(const char * const func, unsigned line, const char * const expr);

#define HP_ASSERT(x) \
  do { if (!(x))   hp_assert_fail(__FUNCTION__, __LINE__, #x); } while (0)
