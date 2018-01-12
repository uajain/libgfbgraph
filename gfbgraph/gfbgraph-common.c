/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*-  */
/*
 * libgfbgraph - GObject library for Facebook Graph API
 * Copyright (C) 2013-2015 Álvaro Peña <alvaropg@gmail.com>
 *
 * GFBGraph is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GFBGraph is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GFBGraph.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gfbgraph-common.h"

#include <rest/rest-proxy.h>
#include <libsoup/soup.h>

#define FACEBOOK_ENDPOINT "https://graph.facebook.com/v2.10"

/**
 * gfbgraph_new_rest_call:
 * @authorizer: a #GFBGraphAuthorizer.
 *
 * Create a new #RestProxyCall pointing to the Facebook Graph API url (https://graph.facebook.com)
 * and processed by the authorizer to allow queries.
 *
 * Returns: (transfer full): a new #RestProxyCall or %NULL in case of error.
 **/
RestProxyCall*
gfbgraph_new_rest_call (GFBGraphAuthorizer *authorizer)
{
        RestProxy *proxy;
        RestProxyCall *rest_call;

        g_return_val_if_fail (GFBGRAPH_IS_AUTHORIZER (authorizer), NULL);

        proxy = rest_proxy_new (FACEBOOK_ENDPOINT, FALSE);
        rest_call = rest_proxy_new_call (proxy);

        gfbgraph_authorizer_process_call (authorizer, rest_call);

        g_object_unref (proxy);

        return rest_call;
}

gboolean
gfbgraph_upload_file_exists_and_mime_type_check (GFile *file)
{
  return g_file_query_exists (file, NULL) &&
         g_file_has_uri_scheme (file, "file");
  /* TODO: Check for permissible mime_types */

}

gint
gfbgraph_new_multipart_upload_soup_call (GFBGraphAuthorizer *authorizer, GFile *file, GHashTable *params)
{
    GError *error = NULL;
    GFileInfo *file_info;
    SoupBuffer *buffer = NULL;
    SoupMessage *message = NULL;
    SoupMultipart *multipart = NULL;
    SoupSession *session = NULL;
    const gchar *mime_type;
    const gchar *name;
    gchar *contents = NULL;
    gchar *path = NULL;
    gchar *url = NULL;
    gsize length = 0;
    guint status = 0;

    url = g_strconcat (FACEBOOK_ENDPOINT, "/me/photos", NULL); /* TODO: use gfbgraph_connectable_get_connection_path */

    path = g_file_get_path (file);
    file_info = g_file_query_info (file,
                                   G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME "," G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                   G_FILE_QUERY_INFO_NONE, NULL, &error);
    if (error != NULL)
      {
        g_warning ("Error while retrieve the file info: %s", error->message);
        g_error_free (error);
        goto out;
      }

    mime_type = g_file_info_get_content_type (file_info);
    name = g_file_info_get_display_name (file_info);

    if (!g_file_get_contents (path, &contents, &length, &error))
      {
        g_warning ("Error in file loading: %s", error->message);
        g_error_free (error);
        goto out;
      }

    session = soup_session_new ();

    buffer = soup_buffer_new (SOUP_MEMORY_STATIC, contents, length);

    multipart = soup_multipart_new (SOUP_FORM_MIME_TYPE_MULTIPART);
    soup_multipart_append_form_file (multipart, "file", name, mime_type, buffer);

    /* append other parameters, if available, to the form request */
    if (g_hash_table_size (params) > 0)
      {
        GHashTableIter iter;
        const gchar *key;
        const gchar *value;

        g_hash_table_iter_init (&iter, params);
        while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &value))
          {
            soup_multipart_append_form_string (multipart, key, value);
          }
      }

    message = soup_form_request_new_from_multipart (url, multipart);

    gfbgraph_authorizer_process_message (GFBGRAPH_AUTHORIZER (authorizer), message);

    status = soup_session_send_message (session, message);

    soup_multipart_free (multipart);
    soup_buffer_free (buffer);
    g_clear_object (&message);
    g_clear_object (&session);

  out:
    g_object_unref (file);
    g_clear_pointer (&contents, g_free);
    g_clear_pointer (&path, g_free);
    g_clear_pointer (&url, g_free);

    return status;
}
