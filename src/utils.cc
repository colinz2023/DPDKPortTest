#include <stdarg.h>
#include "utils.h"

std::string FormatStr(const char* fmt, ...) {
#define BUFFER_SIZE 40960
  char temp[BUFFER_SIZE];
  va_list args;
  va_start(args, fmt);
  int nWrite = vsnprintf(temp, BUFFER_SIZE, fmt, args);

  va_end(args);
  return std::string(temp);
}
