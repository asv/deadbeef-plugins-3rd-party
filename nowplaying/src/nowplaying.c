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

#define PLUGIN_NAME "nowplaying"

#ifdef HAVE_DEBUG
  #define trace(format, args...) \
    fprintf (stderr, PLUGIN_NAME ": " format "\n", ##args);
#else
  #define trace(format, args...)
#endif

static DB_misc_t plugin;
static DB_functions_t *deadbeef;

static char pathname[1024], config[1024];
static char format[1024] = "%a - %t";

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

      trace ("write `%s', length = %d", playing, length);
    }
  else
    {
      trace ("open `%s' error: %s", pathname, strerror (errno));
    }

  return 0;
}

static int
nowplaying_init (void)
{
  snprintf (pathname, sizeof (pathname),
            "%s/current_song", deadbeef->get_config_dir());

  snprintf (config, sizeof (config),
            "%s/nowplaying", deadbeef->get_config_dir());

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
  char times[15];
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
                    case 'A': // album
                      val = deadbeef->pl_find_meta (song, "album");
                      break;
                    case 'a': // artist
                      val = deadbeef->pl_find_meta (song, "artist");
                      break;
                    case 't': // title
                      val = deadbeef->pl_find_meta (song, "title");
                      break;
                    case 'n': // tracknum
                      val = deadbeef->pl_find_meta (song, "track");
                      break;
                    case 'F': // format
                      val = song->filetype;
                      break;
                    case 'T': // totaltime
                        {
                          int minutes = song->duration / 60,
                              seconds = song->duration - minutes * 60;
                          snprintf (times, 14, "%d:%02d", minutes, seconds);
                          val = times;
                        }
                      break;
                    case 'C': // playtime
                        {
                          int minutes = song->playtime / 60,
                              seconds = song->playtime - minutes * 60;
                          snprintf (times, 14, "%d:%02d", minutes, seconds);
                          val = times;
                        }
                    default:
                      trace ("warning: unknow conversion type "
                             "character `%c' in format", *(p - 1));
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

    .plugin.name          = PLUGIN_NAME,
    .plugin.descr         = "Write information about current song into file",

    .plugin.author        = "Alexey Smirnov",
    .plugin.email         = "aysv@users.sourceforge.net",
    .plugin.website       = "http://github.com/asv/deadbeef-plugins-3rd-party",

    .plugin.start         = nowplaying_init,
    .plugin.stop          = nowplaying_release,

    .plugin.type          = DB_PLUGIN_MISC
};
