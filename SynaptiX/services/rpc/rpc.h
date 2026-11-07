#ifndef RPC_H
#define RPC_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cJSON.h"

typedef cJSON * (*rpc_func)(void *arg,cJSON *params);

typedef struct RPC{
	char *name;
	rpc_func func;
    void *arg;
}RPC_t;

cJSON *  RPC_Call(char *name,cJSON *param);

extern const RPC_t  *const rpc_funcs;
extern const size_t rpc_funcs_nums;

#ifdef __cplusplus
}
#endif
#endif // RPC_H