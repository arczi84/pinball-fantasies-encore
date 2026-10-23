/* GCC 16 rc18's libstdc++ is built against newlib's headers, where errno is a function
   (`int *__errno(void)`); libnix's __errno is a variable, a pointer to the errno. Linked as
   they come, libstdc++'s calls jump into libnix's data: an illegal instruction at the first
   std::filesystem::exists. The Makefile renames libstdc++'s ___errno to ___encore_errno in a
   copy of the library, and this is the function it then calls. */
#include <errno.h>

int *__encore_errno(void) { return __errno; }
