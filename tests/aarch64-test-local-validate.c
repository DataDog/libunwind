/* Test that local (UNW_LOCAL_ONLY) dwarf_get and dwarf_getfp honor
   cursor->validate on AArch64.

   Before the fix these accessors read the address directly, so an
   unmapped address with validate set raises SIGSEGV. After the fix they
   return -1 and do not read.  */

/* At -O2 the compiler may drop the unmapped read, because its result is
   unused. Keep it at -O0 so the test shows the bug.  */
#pragma GCC optimize ("O0")

#define UNW_LOCAL_ONLY 1
#include "libunwind_i.h"

#include <stdio.h>
#include <string.h>

static unw_word_t value = 42;
static unw_fpreg_t fpvalue;

/* Stub for the address validator. Only the two valid test addresses are
   "mapped". The real validator is tested elsewhere.  */
bool
unw_address_is_valid (unw_word_t addr, size_t len)
{
  return addr == (unw_word_t) &value || addr == (unw_word_t) &fpvalue;
}

int
main (void)
{
  struct cursor c;
  unw_word_t out = 0;
  unw_fpreg_t fout;
  const unw_word_t unmapped = 0x10;

  memset (&c, 0, sizeof c);
  c.dwarf.as_arg = &c;

  /* validate off: plain read, as before the fix */
  c.validate = 0;
  if (dwarf_get (&c.dwarf, DWARF_MEM_LOC (c.dwarf, (unw_word_t) &value), &out) != 0
      || out != 42)
    return 1;

  /* validate on, mapped address: read succeeds */
  c.validate = 1;
  out = 0;
  if (dwarf_get (&c.dwarf, DWARF_MEM_LOC (c.dwarf, (unw_word_t) &value), &out) != 0
      || out != 42)
    return 2;

  /* validate on, unmapped address: error, no read */
  if (dwarf_get (&c.dwarf, DWARF_MEM_LOC (c.dwarf, unmapped), &out) != -1)
    return 3;

  /* same for dwarf_getfp */
  memset (&fpvalue, 0, sizeof fpvalue);
  if (dwarf_getfp (&c.dwarf, DWARF_MEM_LOC (c.dwarf, (unw_word_t) &fpvalue), &fout) != 0)
    return 4;
  if (dwarf_getfp (&c.dwarf, DWARF_MEM_LOC (c.dwarf, unmapped), &fout) != -1)
    return 5;

  printf ("ok\n");
  return 0;
}
