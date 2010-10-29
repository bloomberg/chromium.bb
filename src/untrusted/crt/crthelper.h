typedef void (*FUN_PTR) (void);

#define FUN_PTR_BEGIN_MARKER ((FUN_PTR) (-1))
#define FUN_PTR_END_MARKER ((FUN_PTR) 0)

#define ATTR_USED __attribute__((used))
#define ATTR_SEC(sec)\
  __attribute__ ((__used__, section(sec), aligned(sizeof(FUN_PTR))))
#define ADD_FUN_PTR_TO_SEC(name, sec, ptr)       \
  static FUN_PTR name ATTR_SEC(sec)  = { ptr }
