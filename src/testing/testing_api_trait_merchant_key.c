/*
  This file is part of TALER
  Copyright (C) 2018 Taler Systems SA

  TALER is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published
  by the Free Software Foundation; either version 3, or (at your
  option) any later version.

  TALER is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public
  License along with TALER; see the file COPYING.  If not, see
  <http://www.gnu.org/licenses/>
*/
/**
 * @file testing/testing_api_trait_merchant_key.c
 * @brief traits to offer peer's (private) keys
 * @author Marcello Stanisci
 */
#include "platform.h"
#include "taler_json_lib.h"
#include <gnunet/gnunet_curl_lib.h>
#include "taler_signatures.h"
#include "taler_testing_lib.h"

#define TALER_TESTING_TRAIT_MERCHANT_PRIV "merchant-priv"
#define TALER_TESTING_TRAIT_MERCHANT_PUB "merchant-pub-pub"

/**
 * Obtain a private key from a "peer".  Used e.g. to obtain
 * a merchant's priv to sign a /track request.
 *
 * @param cmd command that is offering the key.
 * @param index (typically zero) which key to return if there
 *        are multiple on offer.
 * @param[out] priv set to the key coming from @a cmd.
 * @return #GNUNET_OK on success.
 */
int
TALER_TESTING_get_trait_merchant_priv
  (const struct TALER_TESTING_Command *cmd,
  unsigned int index,
  const struct TALER_MerchantPrivateKeyP **priv)
{
  return cmd->traits (cmd->cls,
                      (const void **) priv,
                      TALER_TESTING_TRAIT_MERCHANT_PRIV,
                      index);
}


/**
 * Offer private key, typically done when CMD_1 needs it to
 * sign a request.
 *
 * @param index (typically zero) which key to return if there are
 *        multiple on offer.
 * @param priv which object should be offered.
 * @return the trait.
 */
struct TALER_TESTING_Trait
TALER_TESTING_make_trait_merchant_priv (unsigned int index,
                                        const struct
                                        TALER_MerchantPrivateKeyP *priv)
{
  struct TALER_TESTING_Trait ret = {
    .index = index,
    .trait_name = TALER_TESTING_TRAIT_MERCHANT_PRIV,
    .ptr = (const void *) priv
  };

  return ret;
}


/**
 * Obtain a public key from a "peer".  Used e.g. to obtain
 * a merchant's public key to use backend's API.
 *
 * @param cmd command offering the key.
 * @param index (typically zero) which key to return if there
 *        are multiple on offer.
 * @param[out] pub set to the key coming from @a cmd.
 * @return #GNUNET_OK on success.
 */
int
TALER_TESTING_get_trait_merchant_pub
  (const struct TALER_TESTING_Command *cmd,
  unsigned int index,
  const struct TALER_MerchantPublicKeyP **pub)
{
  return cmd->traits (cmd->cls,
                      (const void **) pub,
                      TALER_TESTING_TRAIT_MERCHANT_PUB,
                      index);
}


/**
 * Offer public key.
 *
 * @param index (typically zero) which key to return if there
 *        are multiple on offer.  NOTE: if one key is offered, it
 *        is mandatory to set this as zero.
 * @param pub which object should be returned.
 * @return the trait.
 */
struct TALER_TESTING_Trait
TALER_TESTING_make_trait_merchant_pub (unsigned int index,
                                       const struct
                                       TALER_MerchantPublicKeyP *pub)
{
  struct TALER_TESTING_Trait ret = {
    .index = index,
    .trait_name = TALER_TESTING_TRAIT_MERCHANT_PUB,
    .ptr = (const void *) pub
  };

  return ret;
}


/* end of testing_api_trait_merchant_key.c */
