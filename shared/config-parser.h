/*
 * Copyright © 2008 Kristian Høgsberg
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

#ifdef  __cplusplus
extern "C" {
#endif

enum config_key_type {
	CONFIG_KEY_INTEGER,		/* typeof data = int */
	CONFIG_KEY_UNSIGNED_INTEGER,	/* typeof data = unsigned int */
	CONFIG_KEY_STRING,		/* typeof data = char* */
	CONFIG_KEY_BOOLEAN		/* typeof data = int */
};

struct config_key {
	const char *name;
	enum config_key_type type;
	void *data;
};

struct config_section {
	const char *name;
	const struct config_key *keys;
	int num_keys;
	void (*done)(void *data);
};

enum weston_option_type {
	WESTON_OPTION_INTEGER,
	WESTON_OPTION_UNSIGNED_INTEGER,
	WESTON_OPTION_STRING,
	WESTON_OPTION_BOOLEAN
};

struct weston_option {
	enum weston_option_type type;
	const char *name;
	int short_name;
	void *data;
};

int
parse_options(const struct weston_option *options,
	      int count, int *argc, char *argv[]);

struct weston_config_section;
struct weston_config;

struct weston_config_section *
weston_config_get_section(struct weston_config *config, const char *section,
			  const char *key, const char *value);
int
weston_config_section_get_int(struct weston_config_section *section,
			      const char *key,
			      int32_t *value, int32_t default_value);
int
weston_config_section_get_uint(struct weston_config_section *section,
			       const char *key,
			       uint32_t *value, uint32_t default_value);
int
weston_config_section_get_double(struct weston_config_section *section,
				 const char *key,
				 double *value, double default_value);
int
weston_config_section_get_string(struct weston_config_section *section,
				 const char *key,
				 char **value,
				 const char *default_value);
int
weston_config_section_get_bool(struct weston_config_section *section,
			       const char *key,
			       int *value, int default_value);
struct weston_config *
weston_config_parse(const char *name);

const char *
weston_config_get_full_path(struct weston_config *config);

void
weston_config_destroy(struct weston_config *config);

int weston_config_next_section(struct weston_config *config,
			       struct weston_config_section **section,
			       const char **name);


#ifdef  __cplusplus
}
#endif

#endif /* CONFIGPARSER_H */

