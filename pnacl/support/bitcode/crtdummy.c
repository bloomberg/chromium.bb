/* Due to a bug in libLTO, this extra file is needed to
 * guarantee that the symbols in crtbegin.bc are available.
 */

void __do_global_dtors_aux(void);
void *__dso_handle;

/* This function is never actually invoked */
void __crtbegin_dummy() __attribute__((used));
void __crtbegin_dummy()
{
  __do_global_dtors_aux();
  __dso_handle = 0;
}
