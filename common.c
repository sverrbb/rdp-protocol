#include "common.h"

/**
 * Check error for function calls
 * @param res: result of function call
 * @param msg: name of function
 */
void check_error(int res, char *msg){
  if (res == -1) {
    perror(msg);
    free_all_rdp_connections();
    exit(EXIT_FAILURE);
  }
}
