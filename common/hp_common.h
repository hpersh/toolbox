#ifndef __HP_COMMON_H
#define __HP_COMMON_H

typedef unsigned char      uint8;
typedef unsigned short     uint16;
typedef unsigned           uint32;
typedef unsigned long long uint64;
typedef signed char        int8;
typedef signed short       int16;
typedef signed             int32;
typedef signed long long   int64;

#define ARRAY_SIZE(a)  (sizeof(a) / sizeof((a)[0]))

#define FIELD_OFS(s, f)                   ((int) &((s *) 0)->f)
#define FIELD_PTR_TO_STRUCT_PTR(p, s, f)  ((s *)((char *)(p) - FIELD_OFS(s, f)))

#define MALLOC_T(t, n)  ((t *) malloc((n) * sizeof(t)))
#define CALLOC_T(t, n)  ((t *) calloc((n), sizeof(t)))

#endif
