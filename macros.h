#ifndef MACROS_H
#define MACROS_H

#define b_is_on(i, b) ((i>>b) & 1)
#define b_is_off(i, b) (!((i>>b) & 1))

#define b_set_on(i, b) (i |= (1 << b))
#define b_set_off(i, b) (i &= ~(1 << b))
#define b_set(i, b, v) (i = (i & ~(1 << b)) | ((v ? 1 : 0) << b))

#endif
