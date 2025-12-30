#include "libunwind_i.h"

//! Set an alternative function to use in place of dl_iterate_phdr.
/*! Suggested use is to specify an async-signal safe implementation.
 *  If not set (or set to NULL) the system dl_iterate_phdr will
 *  be used. */
void
unw_set_iterate_phdr_function (unw_addr_space_t as, unw_iterate_phdr_func_t function)
{
  if (!atomic_load(&tdep_init_done))
    tdep_init ();

#ifndef UNW_REMOTE_ONLY
  if (function)
    __atomic_store_n(&as->iterate_phdr_function, function, __ATOMIC_SEQ_CST);
  else
# if defined(HAVE_DL_ITERATE_PHDR)
    __atomic_store_n(&as->iterate_phdr_function, dl_iterate_phdr, __ATOMIC_SEQ_CST);
# else
    __atomic_store_n(&as->iterate_phdr_function, NULL, __ATOMIC_SEQ_CST);
# endif
#endif
}
