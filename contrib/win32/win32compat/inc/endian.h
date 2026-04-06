#pragma once
/* Windows does not have endian.h; provide minimal compat */
#ifndef _ENDIAN_H_
#define _ENDIAN_H_
/* Windows is always little-endian on supported architectures */
#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN 4321
#define BYTE_ORDER LITTLE_ENDIAN
#endif
