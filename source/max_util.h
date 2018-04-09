#ifndef YC_MAX_UTIL_H_
#define YC_MAX_UTIL_H_

// ========  HEADER FILE FOR MISCELLANEOUS MAX UTILITIES  ========

#include "ext.h"       // Header file for all objects, should always be first
#include "ext_obex.h"  // Header file for all objects, required for new style Max object
#include "z_dsp.h"     // Header file for MSP objects, included here for t_double type

// ====  OUTPUTTING INFORMATION  ====

#define _TRACE false
#define TRACE(...) do { if (_TRACE) object_post((t_object *)x, "TRACE:  " __VA_ARGS__); } while (0)

#define _POST true
#define POST(...) do { if (_POST) object_post((t_object *)x, __VA_ARGS__); } while (0)

#define WARNING(...) do { object_warn((t_object *)x, __VA_ARGS__); } while (0)

#define MY_ERR(...)  do { object_error((t_object *)x, __VA_ARGS__); } while (0)

#define MY_ASSERT(test, err, ...) if (test) { object_error((t_object *)x, __VA_ARGS__); return err; }
#define MY_ASSERT_GOTO(test, label, ...) if (test) { object_error((t_object *)x, __VA_ARGS__); goto label; }

// ====  ENUM  ====

typedef enum _my_err
{
  ERR_NULL,    // For initialization, should never be returned
  ERR_NONE,
  ERR_ARG_TYPE,
  ERR_ARG_VALUE,
  ERR_ALLOC,
  ERR_ALREADY_ALLOC,
  ERR_NULL_PTR,
  ERR_NOT_YET_ALLOC,
  ERR_DICT_NONE,
  ERR_DICT_PROTECT,
  ERR_SYNTAX,
  ERR_INDEX,
  ERR_COUNT,
  ERR_STR_LEN,
  ERR_ARR_FULL,
  ERR_ARG0,
  ERR_ARG1,
  ERR_ARG2,
  ERR_ARG3,
  ERR_ARG4,
  ERR_LOCKED,
  ERR_MISC

} t_my_err;

// ========  END OF HEADER FILE  ========

#endif
