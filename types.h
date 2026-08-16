#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

typedef unsigned int uint;

typedef enum { e_success, e_failure } Status;
typedef enum { e_encode, e_decode, e_unsupported } OperationType;

#endif
