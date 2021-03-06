/*
  This file is part of TALER
  Copyright (C) 2014-2018 Taler Systems SA

  TALER is free software; you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation; either version 3, or (at your option) any later version.

  TALER is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
  A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along with
  TALER; see the file COPYING.  If not, see
  <http://www.gnu.org/licenses/>
*/
/**
 * @file lib/exchange_api_curl_defaults.c
 * @brief curl easy handle defaults
 * @author Florian Dold
 */

#include "exchange_api_curl_defaults.h"


/**
 * Get a curl handle with the right defaults for the exchange lib.  In the
 * future, we might manage a pool of connections here.
 *
 * @param url URL to query
 */
CURL *
TALER_EXCHANGE_curl_easy_get_ (const char *url)
{
  CURL *eh;

  eh = curl_easy_init ();
  if (NULL == eh)
    return NULL;
  GNUNET_assert (CURLE_OK ==
                 curl_easy_setopt (eh,
                                   CURLOPT_URL,
                                   url));
  GNUNET_assert (CURLE_OK ==
                 curl_easy_setopt (eh,
                                   CURLOPT_FOLLOWLOCATION,
                                   1L));
  /* limit MAXREDIRS to 5 as a simple security measure against
     a potential infinite loop caused by a malicious target */
  GNUNET_assert (CURLE_OK ==
                 curl_easy_setopt (eh,
                                   CURLOPT_MAXREDIRS,
                                   5L));
  GNUNET_assert (CURLE_OK ==
                 curl_easy_setopt (eh,
                                   CURLOPT_TCP_FASTOPEN,
                                   1L));
  return eh;
}
