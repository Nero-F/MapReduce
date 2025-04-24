#ifndef _DEFER_H_
#define _DEFER_H_

#define DEFER_MERGE(a, b) a##b
#define DEFER_VARCNAME(a) DEFER_MERGE(defer_scope_var, a)
#define DEFER_FUNCNAME(a) DEFER_MERGE(defer_scope_func, a)
#define DEFER(BLOCK)                                                           \
  void DEFER_FUNCNAME(__LINE__)(__attribute__((unused)) int *a) BLOCK;         \
  __attribute__((cleanup(DEFER_FUNCNAME(__LINE__)))) int DEFER_VARCNAME(       \
      __LINE__)

#endif /* _DEFER_H_ */
