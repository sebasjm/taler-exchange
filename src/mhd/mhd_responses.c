/*
  This file is part of TALER
  Copyright (C) 2014-2020 Taler Systems SA

  TALER is free software; you can redistribute it and/or modify it under the
  terms of the GNU Affero General Public License as published by the Free Software
  Foundation; either version 3, or (at your option) any later version.

  TALER is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
  A PARTICULAR PURPOSE.  See the GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License along with
  TALER; see the file COPYING.  If not, see <http://www.gnu.org/licenses/>
*/
/**
 * @file mhd_responses.c
 * @brief API for generating HTTP replies
 * @author Florian Dold
 * @author Benedikt Mueller
 * @author Christian Grothoff
 */
#include "platform.h"
#include <zlib.h>
#include "taler_util.h"
#include "taler_mhd_lib.h"


/**
 * Global options for response generation.
 */
static enum TALER_MHD_GlobalOptions TM_go;


/**
 * Set global options for response generation
 * within libtalermhd.
 *
 * @param go global options to use
 */
void
TALER_MHD_setup (enum TALER_MHD_GlobalOptions go)
{
  TM_go = go;
}


/**
 * Add headers we want to return in every response.
 * Useful for testing, like if we want to always close
 * connections.
 *
 * @param response response to modify
 */
void
TALER_MHD_add_global_headers (struct MHD_Response *response)
{
  if (0 != (TM_go & TALER_MHD_GO_FORCE_CONNECTION_CLOSE))
    GNUNET_break (MHD_YES ==
                  MHD_add_response_header (response,
                                           MHD_HTTP_HEADER_CONNECTION,
                                           "close"));
  /* The wallet, operating from a background page, needs CORS to
     be disabled otherwise browsers block access. */
  GNUNET_break (MHD_YES ==
                MHD_add_response_header (response,
                                         MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN,
                                         "*"));
}


/**
 * Is HTTP body deflate compression supported by the client?
 *
 * @param connection connection to check
 * @return #MHD_YES if 'deflate' compression is allowed
 *
 * Note that right now we're ignoring q-values, which is technically
 * not correct, and also do not support "*" anywhere but in a line by
 * itself.  This should eventually be fixed, see also
 * https://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html
 */
MHD_RESULT
TALER_MHD_can_compress (struct MHD_Connection *connection)
{
  const char *ae;
  const char *de;

  if (0 != (TM_go & TALER_MHD_GO_DISABLE_COMPRESSION))
    return MHD_NO;
  ae = MHD_lookup_connection_value (connection,
                                    MHD_HEADER_KIND,
                                    MHD_HTTP_HEADER_ACCEPT_ENCODING);
  if (NULL == ae)
    return MHD_NO;
  if (0 == strcmp (ae,
                   "*"))
    return MHD_YES;
  de = strstr (ae,
               "deflate");
  if (NULL == de)
    return MHD_NO;
  if ( ( (de == ae) ||
         (de[-1] == ',') ||
         (de[-1] == ' ') ) &&
       ( (de[strlen ("deflate")] == '\0') ||
         (de[strlen ("deflate")] == ',') ||
         (de[strlen ("deflate")] == ';') ) )
    return MHD_YES;
  return MHD_NO;
}


/**
 * Try to compress a response body.  Updates @a buf and @a buf_size.
 *
 * @param[in,out] buf pointer to body to compress
 * @param[in,out] buf_size pointer to initial size of @a buf
 * @return #MHD_YES if @a buf was compressed
 */
MHD_RESULT
TALER_MHD_body_compress (void **buf,
                         size_t *buf_size)
{
  Bytef *cbuf;
  uLongf cbuf_size;
  MHD_RESULT ret;

  cbuf_size = compressBound (*buf_size);
  cbuf = malloc (cbuf_size);
  if (NULL == cbuf)
    return MHD_NO;
  ret = compress (cbuf,
                  &cbuf_size,
                  (const Bytef *) *buf,
                  *buf_size);
  if ( (Z_OK != ret) ||
       (cbuf_size >= *buf_size) )
  {
    /* compression failed */
    free (cbuf);
    return MHD_NO;
  }
  free (*buf);
  *buf = (void *) cbuf;
  *buf_size = (size_t) cbuf_size;
  return MHD_YES;
}


/**
 * Make JSON response object.
 *
 * @param json the json object
 * @return MHD response object
 */
struct MHD_Response *
TALER_MHD_make_json (const json_t *json)
{
  struct MHD_Response *response;
  char *json_str;

  json_str = json_dumps (json,
                         JSON_INDENT (2));
  if (NULL == json_str)
  {
    GNUNET_break (0);
    return NULL;
  }
  response = MHD_create_response_from_buffer (strlen (json_str),
                                              json_str,
                                              MHD_RESPMEM_MUST_FREE);
  if (NULL == response)
  {
    free (json_str);
    GNUNET_break (0);
    return NULL;
  }
  TALER_MHD_add_global_headers (response);
  GNUNET_break (MHD_YES ==
                MHD_add_response_header (response,
                                         MHD_HTTP_HEADER_CONTENT_TYPE,
                                         "application/json"));
  return response;
}


/**
 * Send JSON object as response.
 *
 * @param connection the MHD connection
 * @param json the json object
 * @param response_code the http response code
 * @return MHD result code
 */
MHD_RESULT
TALER_MHD_reply_json (struct MHD_Connection *connection,
                      const json_t *json,
                      unsigned int response_code)
{
  struct MHD_Response *response;
  void *json_str;
  size_t json_len;
  MHD_RESULT is_compressed;

  json_str = json_dumps (json,
                         JSON_INDENT (2));
  if (NULL == json_str)
  {
    /**
     * This log helps to figure out which
     * function called this one and assert-failed.
     */
    TALER_LOG_ERROR ("Aborting json-packing for HTTP code: %u\n",
                     response_code);

    GNUNET_assert (0);
    return MHD_NO;
  }
  json_len = strlen (json_str);
  /* try to compress the body */
  is_compressed = MHD_NO;
  if (MHD_YES ==
      TALER_MHD_can_compress (connection))
    is_compressed = TALER_MHD_body_compress (&json_str,
                                             &json_len);
  response = MHD_create_response_from_buffer (json_len,
                                              json_str,
                                              MHD_RESPMEM_MUST_FREE);
  if (NULL == response)
  {
    free (json_str);
    GNUNET_break (0);
    return MHD_NO;
  }
  TALER_MHD_add_global_headers (response);
  GNUNET_break (MHD_YES ==
                MHD_add_response_header (response,
                                         MHD_HTTP_HEADER_CONTENT_TYPE,
                                         "application/json"));
  if (MHD_YES == is_compressed)
  {
    /* Need to indicate to client that body is compressed */
    if (MHD_NO ==
        MHD_add_response_header (response,
                                 MHD_HTTP_HEADER_CONTENT_ENCODING,
                                 "deflate"))
    {
      GNUNET_break (0);
      MHD_destroy_response (response);
      return MHD_NO;
    }
  }

  {
    MHD_RESULT ret;

    ret = MHD_queue_response (connection,
                              response_code,
                              response);
    MHD_destroy_response (response);
    return ret;
  }
}


/**
 * Send back a "204 No Content" response with headers
 * for the CORS pre-flight request.
 *
 * @param connection the MHD connection
 * @return MHD result code
 */
MHD_RESULT
TALER_MHD_reply_cors_preflight (struct MHD_Connection *connection)
{
  struct MHD_Response *response;

  response = MHD_create_response_from_buffer (0,
                                              NULL,
                                              MHD_RESPMEM_PERSISTENT);
  if (NULL == response)
    return MHD_NO;
  /* This adds the Access-Control-Allow-Origin header.
   * All endpoints of the exchange allow CORS. */
  TALER_MHD_add_global_headers (response);
  GNUNET_break (MHD_YES ==
                MHD_add_response_header (response,
                                         /* Not available as MHD constant yet */
                                         "Access-Control-Allow-Headers",
                                         "*"));

  {
    MHD_RESULT ret;

    ret = MHD_queue_response (connection,
                              MHD_HTTP_NO_CONTENT,
                              response);
    MHD_destroy_response (response);
    return ret;
  }
}


/**
 * Function to call to handle the request by building a JSON
 * reply from a format string and varargs.
 *
 * @param connection the MHD connection to handle
 * @param response_code HTTP response code to use
 * @param fmt format string for pack
 * @param ... varargs
 * @return MHD result code
 */
MHD_RESULT
TALER_MHD_reply_json_pack (struct MHD_Connection *connection,
                           unsigned int response_code,
                           const char *fmt,
                           ...)
{
  json_t *json;
  json_error_t jerror;

  {
    va_list argp;

    va_start (argp,
              fmt);
    json = json_vpack_ex (&jerror,
                          0,
                          fmt,
                          argp);
    va_end (argp);
  }

  if (NULL == json)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Failed to pack JSON with format `%s': %s\n",
                fmt,
                jerror.text);
    GNUNET_break (0);
    return MHD_NO;
  }

  {
    MHD_RESULT ret;

    ret = TALER_MHD_reply_json (connection,
                                json,
                                response_code);
    json_decref (json);
    return ret;
  }
}


/**
 * Make JSON response object.
 *
 * @param fmt format string for pack
 * @param ... varargs
 * @return MHD response object
 */
struct MHD_Response *
TALER_MHD_make_json_pack (const char *fmt,
                          ...)
{
  json_t *json;
  json_error_t jerror;

  {
    va_list argp;

    va_start (argp, fmt);
    json = json_vpack_ex (&jerror,
                          0,
                          fmt,
                          argp);
    va_end (argp);
  }

  if (NULL == json)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Failed to pack JSON with format `%s': %s\n",
                fmt,
                jerror.text);
    GNUNET_break (0);
    return MHD_NO;
  }

  {
    struct MHD_Response *response;

    response = TALER_MHD_make_json (json);
    json_decref (json);
    return response;
  }
}


/**
 * Create a response indicating an internal error.
 *
 * @param ec error code to return
 * @param detail additional optional detail about the error, can be NULL
 * @return a MHD response object
 */
struct MHD_Response *
TALER_MHD_make_error (enum TALER_ErrorCode ec,
                      const char *detail)
{
  return TALER_MHD_make_json_pack ("{s:I, s:s, s:s?}",
                                   "code", (json_int_t) ec,
                                   "hint", TALER_ErrorCode_get_hint (ec),
                                   "detail", detail);
}


/**
 * Send a response indicating an error.
 *
 * @param connection the MHD connection to use
 * @param ec error code uniquely identifying the error
 * @param http_status HTTP status code to use
 * @param detail additional optional detail about the error, can be NULL
 * @return a MHD result code
 */
MHD_RESULT
TALER_MHD_reply_with_error (struct MHD_Connection *connection,
                            unsigned int http_status,
                            enum TALER_ErrorCode ec,
                            const char *detail)
{
  return TALER_MHD_reply_json_pack (connection,
                                    http_status,
                                    "{s:I, s:s, s:s?}",
                                    "code", (json_int_t) ec,
                                    "hint", TALER_ErrorCode_get_hint (ec),
                                    "detail", detail);
}


/**
 * Send a response indicating an error. The HTTP status code is
 * to be derived from the @a ec.
 *
 * @param connection the MHD connection to use
 * @param ec error code uniquely identifying the error
 * @param detail additional optional detail about the error
 * @return a MHD result code
 */
MHD_RESULT
TALER_MHD_reply_with_ec (struct MHD_Connection *connection,
                         enum TALER_ErrorCode ec,
                         const char *detail)
{
  unsigned int hc = TALER_ErrorCode_get_http_status (ec);

  if ( (0 == hc) ||
       (UINT_MAX == hc) )
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Invalid Taler error code %d provided for response!\n",
                (int) ec);
    hc = MHD_HTTP_INTERNAL_SERVER_ERROR;
  }
  return TALER_MHD_reply_with_error (connection,
                                     hc,
                                     ec,
                                     detail);
}


/**
 * Send a response indicating that the request was too big.
 *
 * @param connection the MHD connection to use
 * @return a MHD result code
 */
MHD_RESULT
TALER_MHD_reply_request_too_large (struct MHD_Connection *connection)
{
  struct MHD_Response *response;

  response = MHD_create_response_from_buffer (0,
                                              NULL,
                                              MHD_RESPMEM_PERSISTENT);
  if (NULL == response)
    return MHD_NO;
  TALER_MHD_add_global_headers (response);

  {
    MHD_RESULT ret;

    ret = MHD_queue_response (connection,
                              MHD_HTTP_REQUEST_ENTITY_TOO_LARGE,
                              response);
    MHD_destroy_response (response);
    return ret;
  }
}


/**
 * Function to call to handle the request by sending
 * back a redirect to the AGPL source code.
 *
 * @param connection the MHD connection to handle
 * @param url where to redirect for the sources
 * @return MHD result code
 */
MHD_RESULT
TALER_MHD_reply_agpl (struct MHD_Connection *connection,
                      const char *url)
{
  const char *agpl =
    "This server is licensed under the Affero GPL. You will now be redirected to the source code.";
  struct MHD_Response *response;

  response = MHD_create_response_from_buffer (strlen (agpl),
                                              (void *) agpl,
                                              MHD_RESPMEM_PERSISTENT);
  if (NULL == response)
  {
    GNUNET_break (0);
    return MHD_NO;
  }
  TALER_MHD_add_global_headers (response);
  GNUNET_break (MHD_YES ==
                MHD_add_response_header (response,
                                         MHD_HTTP_HEADER_CONTENT_TYPE,
                                         "text/plain"));
  if (MHD_NO ==
      MHD_add_response_header (response,
                               MHD_HTTP_HEADER_LOCATION,
                               url))
  {
    GNUNET_break (0);
    MHD_destroy_response (response);
    return MHD_NO;
  }

  {
    MHD_RESULT ret;

    ret = MHD_queue_response (connection,
                              MHD_HTTP_FOUND,
                              response);
    MHD_destroy_response (response);
    return ret;
  }
}


/**
 * Function to call to handle the request by sending
 * back static data.
 *
 * @param connection the MHD connection to handle
 * @param http_status status code to return
 * @param mime_type content-type to use
 * @param body response payload
 * @param body_size number of bytes in @a body
 * @return MHD result code
 */
MHD_RESULT
TALER_MHD_reply_static (struct MHD_Connection *connection,
                        unsigned int http_status,
                        const char *mime_type,
                        const char *body,
                        size_t body_size)
{
  struct MHD_Response *response;

  response = MHD_create_response_from_buffer (body_size,
                                              (void *) body,
                                              MHD_RESPMEM_PERSISTENT);
  if (NULL == response)
  {
    GNUNET_break (0);
    return MHD_NO;
  }
  TALER_MHD_add_global_headers (response);
  if (NULL != mime_type)
    GNUNET_break (MHD_YES ==
                  MHD_add_response_header (response,
                                           MHD_HTTP_HEADER_CONTENT_TYPE,
                                           mime_type));
  {
    MHD_RESULT ret;

    ret = MHD_queue_response (connection,
                              http_status,
                              response);
    MHD_destroy_response (response);
    return ret;
  }
}


/* end of mhd_responses.c */
