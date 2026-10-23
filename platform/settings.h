/* Settings as text: one "key = value" a line, lines starting with # or ; are comments. A file is
 * read into the store, changed, and written back with its lines in their order, its comments and
 * the keys this build does not know kept as they were, so the player can edit it by hand. Pure
 * string work: reading and writing the file is platform/main.c's (it keeps the ROM, the keys and
 * the controllers' buttons here: settings.ini). */
#ifndef PM_SETTINGS_H
#define PM_SETTINGS_H
#include <stddef.h>

typedef struct pm_settings pm_settings;

pm_settings *pm_settings_new(void);
void pm_settings_free(pm_settings *s);
/* read text into the store (keys already there take the text's values); the entries read */
int pm_settings_parse(pm_settings *s, const char *text, size_t len);
/* a key's value (NULL when absent), or as an integer (fallback when absent or not a number) */
const char *pm_settings_get(const pm_settings *s, const char *key);
int pm_settings_get_int(const pm_settings *s, const char *key, int fallback);
/* set a key (added at the end when new); the store is dirty when a value changed */
void pm_settings_set(pm_settings *s, const char *key, const char *value);
void pm_settings_set_int(pm_settings *s, const char *key, int value);
/* add a key with its default when it is absent (so a new file lists every setting) */
void pm_settings_default(pm_settings *s, const char *key, const char *value);
/* a key taken out (one a build no longer has); dirty when it was there */
void pm_settings_remove(pm_settings *s, const char *key);
/* the text: written into out when size is enough (with its terminating 0); returns the size it needs */
size_t pm_settings_format(const pm_settings *s, char *out, size_t size);
/* changed since it was last written (pm_settings_clean after writing it) */
int pm_settings_dirty(const pm_settings *s);
void pm_settings_clean(pm_settings *s);

#endif
