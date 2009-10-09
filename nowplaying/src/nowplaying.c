/*
 * Copyright (c) 2009 Alexey Smirnov <alexey.smirnov@gmx.com>
 *
 * See the file LICENSE for information on usage and redistribution
 * of this file, and for a DISCLAMER OF ALL WARRANTIES.
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <deadbeef/deadbeef.h>

#ifdef HAVE_DEBUG
  #define trace(...) \
    fprintf (stderr, "nowplaying: " __VA_ARGS__);
#else
  #define trace(...)
#endif

static DB_misc_t plugin;
static DB_functions_t *deadbeef;

char pathname[1024], config[1024];
char format[1024] = "%a - %t";

DB_plugin_t *
nowplaying_load (DB_functions_t *api);

static int
nowplaying_songstarted (DB_event_song_t *ev, uintptr_t data);

static int
nowplaying_init (void);

static int
nowplaying_release (void);

static int
npformat (DB_playItem_t *song, char *str, size_t size, const char *format);

DB_plugin_t *
nowplaying_load (DB_functions_t *api)
{
  deadbeef = api;
  return DB_PLUGIN (&plugin);
}

static int
nowplaying_songstarted (DB_event_song_t *ev, uintptr_t data)
{
  char playing[1024];
  size_t length = npformat (ev->song, playing, 1024, format);

  FILE *out = fopen (pathname, "wt");

  if (out != NULL)
    {
      fputs (playing, out);
      fclose (out);

      trace ("write `%s', length = %d\n", playing, length);
    }
  else
    {
      trace ("open `%s' error: %s\n", pathname, strerror (errno));
    }

  return 0;
}

static int
nowplaying_init (void)
{
  snprintf (pathname, 1024, "%s/current_song", deadbeef->get_config_dir());
  snprintf (config, 1024, "%s/nowplaying", deadbeef->get_config_dir());

  FILE *cfg_file = fopen (config, "rt");

  if (cfg_file != NULL)
    {
      if (fgets (format, 1024, cfg_file) < 0) {
          trace ("invalid format");
          fclose (cfg_file);

          return -1;
      }

      trace ("read format `%s'", format);

      fclose (cfg_file);
    }
  else
    {
      trace ("config file `%s' not found. use default settings.", config);
    }

  deadbeef->ev_subscribe (DB_PLUGIN (&plugin), DB_EV_SONGSTARTED,
                          DB_CALLBACK (nowplaying_songstarted), 0);
  return 0;
}

static int
nowplaying_release (void)
{
  deadbeef->ev_unsubscribe (DB_PLUGIN (&plugin), DB_EV_SONGSTARTED,
                            DB_CALLBACK (nowplaying_songstarted), 0);
  return 0;
}

static int
npformat (DB_playItem_t *song, char *str, size_t size, const char *format)
{
  const char *p = format, *val;
  size_t cur = 0, len, ssize = size - 1;

  while (*p != '\0' && cur < ssize)
    {
      if (*p != '%')
        {
          str[cur++] = *p++;
        }
      else
        {
          ++p; --ssize;
          if (*p != '\0')
            {
              if (*p != '%')
                {
                  switch (*p++)
                    {
                    case 'A':
                      val = deadbeef->pl_find_meta (song, "album");
                      break;
                    case 'a':
                      val = deadbeef->pl_find_meta (song, "artist");
                      break;
                    case 't':
                      val = deadbeef->pl_find_meta (song, "title");
                      break;
                    case 'n':
                      val = deadbeef->pl_find_meta (song, "track");
                      break;
                    default:
                      trace ("warning: unknow conversion type "
                             "character `%c' in format\n", *(p - 1));
                      continue;
                    }

                  if (val == NULL)
                    {
                      val = "<null>";
                    }

                  len = strlen (val);

                  if (len + cur > ssize)
                    {
                      len = ssize - cur;
                    }

                  strncpy (str + cur, val, len);
                  cur += len;
                }
              else
                {
                  str[cur++] = *p;
                  ++p;
                }
            }
        }
    }
  str[cur] = '\0';
  return cur;
}

static DB_misc_t plugin = {
    DB_PLUGIN_SET_API_VERSION
    .plugin.version_major = 0,
    .plugin.version_minor = 1,

    .plugin.name          = "nowplaying",
    .plugin.descr         = "Write to file information about current song",

    .plugin.author        = "Alexey Smirnov",
    .plugin.email         = "aysv@users.sourceforge.net",
    .plugin.website       = "http://github.com/asv/deadbeef-plugins-3rd-party",

    .plugin.start         = nowplaying_init,
    .plugin.stop          = nowplaying_release,

    .plugin.type          = DB_PLUGIN_MISC
};
