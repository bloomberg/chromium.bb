/*
 * $Id: process_info.h 760 2010-10-30 17:41:07Z g.rodola $
 *
 * Helper functions related to fetching process information. Used by _psutil_osx
 * module methods.
 */

#include <Python.h>


typedef struct kinfo_proc kinfo_proc;

int get_proc_list(kinfo_proc **procList, size_t *procCount);
int getcmdargs(long pid, PyObject **exec_path, PyObject **envlist, PyObject **arglist);
PyObject* get_arg_list(long pid);
int get_kinfo_proc(pid_t pid, struct kinfo_proc *kp);
