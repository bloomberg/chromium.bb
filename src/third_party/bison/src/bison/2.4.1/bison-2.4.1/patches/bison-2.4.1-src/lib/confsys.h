#ifndef _CONFSYS_H_
#define _CONFSYS_H_ 1

#ifdef _WIN32
#  define close		_close
#  define dup			_dup
#  define dup2			_dup2
#  define fdopen		_fdopen
#  define setmode		_setmode
#  define spawnvp		_spawnvp
#  define strdup		_strdup
#endif /* _WIN32 */

#endif /* _CONFSYS_H_ */
