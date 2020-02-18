/* Subprocesses with pipes.

   Copyright (C) 2002, 2004, 2005, 2006 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Written by Paul Eggert <eggert@twinsun.com>
   and Florian Krohm <florian@edamail.fishkill.ibm.com>.  */

#include <config.h>

#include "subpipe.h"

#include <errno.h>

#include <signal.h>
#if ! defined SIGCHLD && defined SIGCLD
# define SIGCHLD SIGCLD
#endif

#include <stdlib.h>

#include <unistd.h>
#ifndef STDIN_FILENO
# define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
# define STDOUT_FILENO 1
#endif
#if ! HAVE_DUP2 && ! defined dup2
# include <fcntl.h>
# define dup2(f, t) (close (t), fcntl (f, F_DUPFD, t))
#endif

#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
# define WEXITSTATUS(stat_val) ((unsigned int) (stat_val) >> 8)
#endif
#ifndef WIFEXITED
# define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

#if HAVE_VFORK_H
# include <vfork.h>
#endif
#if ! HAVE_WORKING_VFORK
# define vfork fork
#endif

#include "error.h"
#include "unistd-safer.h"

#include "gettext.h"
#define _(Msgid)  gettext (Msgid)

#ifndef __attribute__
/* This feature is available in gcc versions 2.5 and later.  */
# if ! defined __GNUC__ || __GNUC__ < 2 || \
(__GNUC__ == 2 && __GNUC_MINOR__ < 5) || __STRICT_ANSI__
#  define __attribute__(Spec) /* empty */
# endif
#endif

#ifndef ATTRIBUTE_UNUSED
# define ATTRIBUTE_UNUSED __attribute__ ((__unused__))
#endif

/* Initialize this module.  */

void
init_subpipe (void)
{
#ifdef SIGCHLD
  /* System V fork+wait does not work if SIGCHLD is ignored.  */
  signal (SIGCHLD, SIG_DFL);
#endif
}


/* Create a subprocess that is run as a filter.  ARGV is the
   NULL-terminated argument vector for the subprocess.  Store read and
   write file descriptors for communication with the subprocess into
   FD[0] and FD[1]: input meant for the process can be written into
   FD[0], and output from the process can be read from FD[1].  Return
   the subprocess id.

   To avoid deadlock, the invoker must not let incoming data pile up
   in FD[1] while writing data to FD[0].  */

pid_t
create_subpipe (char const * const *argv, int fd[2])
{
  int pipe_fd[2];
  int child_fd[2];
  pid_t pid;

  if (pipe_safer (child_fd) != 0 || pipe_safer (pipe_fd) != 0)
    error (EXIT_FAILURE, errno, "pipe");
  fd[0] = child_fd[1];
  fd[1] = pipe_fd[0];
  child_fd[1] = pipe_fd[1];

/* WIN32 patch taken from the Mingw port:
   http://prdownloads.sourceforge.net/mingw/bison-1.875-2003.02.10-1-src.tar.bz2?download
*/
#ifdef _WIN32
  /* In windows we must control what the child inherits with the parent process
   * and then use a spawn function instead of a fork/exec pair.
   *
   * So, in assigning the pipe pair we must dup stdio to save the originals,
   * dup2 the pipe handles to the stdio handles, spawn and then dup2 the saved
   * handles back to stdio and close the dupped ones.
   */
  int osi, oso;
  
  if ((osi = dup (STDIN_FILENO)) < 0)
      error (EXIT_FAILURE, errno, "pipe");
  setmode(osi, O_NOINHERIT);
  if ((oso = dup (STDOUT_FILENO)) < 0)
      error (EXIT_FAILURE, errno, "pipe");
  setmode(oso, O_NOINHERIT);

  if (dup2(child_fd[0], STDIN_FILENO) < 0)
      error (EXIT_FAILURE, errno, "pipe");
  if (dup2(child_fd[1], STDOUT_FILENO) < 0)
      error (EXIT_FAILURE, errno, "pipe");

#include <conio.h>
  pid = spawnvp(P_NOWAIT, argv[0], argv);
  if (pid < 0) {
  	  printf ("spawnvp: %s\n", argv[0]);
      error (EXIT_FAILURE, errno, argv[0]);
  }
  if (dup2 (osi, STDIN_FILENO))
      error (EXIT_FAILURE, errno, "dup2");
  close (osi);
  if (dup2 (oso, STDOUT_FILENO))
      error (EXIT_FAILURE, errno, "dup2");
  close (oso);

#else
  pid = vfork ();
  if (pid < 0)
    error (EXIT_FAILURE, errno, "fork");

  if (! pid)
    {
      /* Child.  */
      close (fd[0]);
      close (fd[1]);
      dup2 (child_fd[0], STDIN_FILENO);
      close (child_fd[0]);
      dup2 (child_fd[1], STDOUT_FILENO);
      close (child_fd[1]);

      /* The cast to (char **) rather than (char * const *) is needed
	 for portability to older hosts with a nonstandard prototype
	 for execvp.  */
      execvp (argv[0], (char **) argv);

      _exit (errno == ENOENT ? 127 : 126);
    }
#endif /* _WIN32 */
  /* Parent.  */
  close (child_fd[0]);
  close (child_fd[1]);
  return pid;
}


/* Wait for the subprocess to exit.  */

void
reap_subpipe (pid_t pid, char const *program)
{
#if HAVE_WAITPID || defined waitpid
  int wstatus;
  if (waitpid (pid, &wstatus, 0) < 0)
    error (EXIT_FAILURE, errno, "waitpid");
  else
    {
      int status = WIFEXITED (wstatus) ? WEXITSTATUS (wstatus) : -1;
      if (status)
	error (EXIT_FAILURE, 0,
	       _(status == 126
		 ? "subsidiary program `%s' could not be invoked"
		 : status == 127
		 ? "subsidiary program `%s' not found"
		 : status < 0
		 ? "subsidiary program `%s' failed"
		 : "subsidiary program `%s' failed (exit status %d)"),
	       program, status);
    }
#endif
}

void
end_of_output_subpipe (pid_t pid ATTRIBUTE_UNUSED,
		       int fd[2] ATTRIBUTE_UNUSED)
{
}
