/*
 * Copyright (c) 2009 Alexey Smirnov <alexey.smirnov@gmx.com>
 *
 * See the file LICENSE for information on usage and redistribution
 * of this file, and for a DISCLAMER OF ALL WARRANTIES.
 */

#include "config.h"

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <limits.h>

#include <deadbeef/deadbeef.h>

#ifdef HAVE_DEBUG
  #define trace(format, args...) \
    fprintf (stderr, PACKAGE_NAME ": " format "\n", ##args);
#else
  #define trace(format, args...)
#endif

static DB_misc_t plugin;
static DB_functions_t *deadbeef;

static char location[PATH_MAX];
static char format[1024];

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
  size_t length = npformat (ev->song, playing, sizeof(playing), format);

  FILE *out = fopen (location, "w+t");

  if (out != NULL)
    {
      fputs (playing, out);
      fclose (out);

      trace ("write `%s', length = %d", playing, length);
    }
  else
    {
      trace ("open `%s' error: %s", location, strerror (errno));
    }

  return 0;
}

static int
nowplaying_init (void)
{
  char pathname[PATH_MAX];

  snprintf (pathname, sizeof (pathname),
            "%s/current_song", deadbeef->get_config_dir());

  strcpy (format, deadbeef->conf_get_str ("nowplaying.format", "%a - %t"));
  strcpy (location, deadbeef->conf_get_str ("nowplaying.location", pathname));

  trace ("read format `%s'", format);
  trace ("read location `%s'", location);

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
                          float duration = deadbeef->pl_get_item_duration (song);
                          int minutes = duration / 60,
                              seconds = duration - minutes * 60;
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
    .plugin.version_minor = 2,

    .plugin.name          = PACKAGE_NAME,
    .plugin.descr         = "Write information about current song into file",

    .plugin.author        = "Alexey Smirnov",
    .plugin.email         = "aysv@users.sourceforge.net",
    .plugin.website       = "http://github.com/asv/deadbeef-plugins-3rd-party",

    .plugin.start         = nowplaying_init,
    .plugin.stop          = nowplaying_release,

    .plugin.type          = DB_PLUGIN_MISC
};
