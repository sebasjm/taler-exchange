/*
  This file is part of TALER
  Copyright (C) 2014-2020 Taler Systems SA

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
 * @file lib/exchange_api_deposit.c
 * @brief Implementation of the /deposit request of the exchange's HTTP API
 * @author Sree Harsha Totakura <sreeharsha@totakura.in>
 * @author Christian Grothoff
 */
#include "platform.h"
#include <jansson.h>
#include <microhttpd.h> /* just for HTTP status codes */
#include <gnunet/gnunet_util_lib.h>
#include <gnunet/gnunet_json_lib.h>
#include <gnunet/gnunet_curl_lib.h>
#include "taler_json_lib.h"
#include "taler_auditor_service.h"
#include "taler_exchange_service.h"
#include "exchange_api_handle.h"
#include "taler_signatures.h"
#include "exchange_api_curl_defaults.h"


/**
 * 1:#AUDITOR_CHANCE is the probability that we report deposits
 * to the auditor.
 *
 * 20==5% of going to auditor. This is possibly still too high, but set
 * deliberately this high for testing
 */
#define AUDITOR_CHANCE 20

/**
 * @brief A Deposit Handle
 */
struct TALER_EXCHANGE_DepositHandle
{

  /**
   * The connection to exchange this request handle will use
   */
  struct TALER_EXCHANGE_Handle *exchange;

  /**
   * The url for this request.
   */
  char *url;

  /**
   * Context for #TEH_curl_easy_post(). Keeps the data that must
   * persist for Curl to make the upload.
   */
  struct TALER_CURL_PostContext ctx;

  /**
   * Handle for the request.
   */
  struct GNUNET_CURL_Job *job;

  /**
   * Function to call with the result.
   */
  TALER_EXCHANGE_DepositResultCallback cb;

  /**
   * Closure for @a cb.
   */
  void *cb_cls;

  /**
   * Information the exchange should sign in response.
   */
  struct TALER_DepositConfirmationPS depconf;

  /**
   * Exchange signature, set for #auditor_cb.
   */
  struct TALER_ExchangeSignatureP exchange_sig;

  /**
   * Exchange signing public key, set for #auditor_cb.
   */
  struct TALER_ExchangePublicKeyP exchange_pub;

  /**
   * Value of the /deposit transaction, including fee.
   */
  struct TALER_Amount amount_with_fee;

  /**
   * @brief Public information about the coin's denomination key.
   * Note that the "key" field itself has been zero'ed out.
   */
  struct TALER_EXCHANGE_DenomPublicKey dki;

  /**
   * Chance that we will inform the auditor about the deposit
   * is 1:n, where the value of this field is "n".
   */
  unsigned int auditor_chance;

};


/**
 * Function called for each auditor to give us a chance to possibly
 * launch a deposit confirmation interaction.
 *
 * @param cls closure
 * @param ah handle to the auditor
 * @param auditor_pub public key of the auditor
 * @return NULL if no deposit confirmation interaction was launched
 */
static struct TEAH_AuditorInteractionEntry *
auditor_cb (void *cls,
            struct TALER_AUDITOR_Handle *ah,
            const struct TALER_AuditorPublicKeyP *auditor_pub)
{
  struct TALER_EXCHANGE_DepositHandle *dh = cls;
  const struct TALER_EXCHANGE_Keys *key_state;
  const struct TALER_EXCHANGE_SigningPublicKey *spk;
  struct TALER_Amount amount_without_fee;
  struct TEAH_AuditorInteractionEntry *aie;

  if (0 != GNUNET_CRYPTO_random_u32 (GNUNET_CRYPTO_QUALITY_WEAK,
                                     dh->auditor_chance))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_INFO,
                "Not providing deposit confirmation to auditor\n");
    return NULL;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_INFO,
              "Will provide deposit confirmation to auditor `%s'\n",
              TALER_B2S (auditor_pub));
  key_state = TALER_EXCHANGE_get_keys (dh->exchange);
  spk = TALER_EXCHANGE_get_signing_key_info (key_state,
                                             &dh->exchange_pub);
  if (NULL == spk)
  {
    GNUNET_break_op (0);
    return NULL;
  }
  TALER_amount_ntoh (&amount_without_fee,
                     &dh->depconf.amount_without_fee);
  aie = GNUNET_new (struct TEAH_AuditorInteractionEntry);
  aie->dch = TALER_AUDITOR_deposit_confirmation (
    ah,
    &dh->depconf.h_wire,
    &dh->depconf.h_contract_terms,
    GNUNET_TIME_absolute_ntoh (dh->depconf.exchange_timestamp),
    GNUNET_TIME_absolute_ntoh (dh->depconf.refund_deadline),
    &amount_without_fee,
    &dh->depconf.coin_pub,
    &dh->depconf.merchant,
    &dh->exchange_pub,
    &dh->exchange_sig,
    &key_state->master_pub,
    spk->valid_from,
    spk->valid_until,
    spk->valid_legal,
    &spk->master_sig,
    &TEAH_acc_confirmation_cb,
    aie);
  return aie;
}


/**
 * Verify that the signature on the "200 OK" response
 * from the exchange is valid.
 *
 * @param dh deposit handle
 * @param json json reply with the signature
 * @param[out] exchange_sig set to the exchange's signature
 * @param[out] exchange_pub set to the exchange's public key
 * @return #GNUNET_OK if the signature is valid, #GNUNET_SYSERR if not
 */
static int
verify_deposit_signature_ok (struct TALER_EXCHANGE_DepositHandle *dh,
                             const json_t *json,
                             struct TALER_ExchangeSignatureP *exchange_sig,
                             struct TALER_ExchangePublicKeyP *exchange_pub)
{
  const struct TALER_EXCHANGE_Keys *key_state;
  struct GNUNET_JSON_Specification spec[] = {
    GNUNET_JSON_spec_fixed_auto ("exchange_sig", exchange_sig),
    GNUNET_JSON_spec_fixed_auto ("exchange_pub", exchange_pub),
    TALER_JSON_spec_absolute_time_nbo ("exchange_timestamp",
                                       &dh->depconf.exchange_timestamp),
    GNUNET_JSON_spec_end ()
  };

  if (GNUNET_OK !=
      GNUNET_JSON_parse (json,
                         spec,
                         NULL, NULL))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  key_state = TALER_EXCHANGE_get_keys (dh->exchange);
  if (GNUNET_OK !=
      TALER_EXCHANGE_test_signing_key (key_state,
                                       exchange_pub))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  if (GNUNET_OK !=
      GNUNET_CRYPTO_eddsa_verify (TALER_SIGNATURE_EXCHANGE_CONFIRM_DEPOSIT,
                                  &dh->depconf,
                                  &exchange_sig->eddsa_signature,
                                  &exchange_pub->eddsa_pub))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  dh->exchange_sig = *exchange_sig;
  dh->exchange_pub = *exchange_pub;
  TEAH_get_auditors_for_dc (dh->exchange,
                            &auditor_cb,
                            dh);
  return GNUNET_OK;
}


/**
 * Verify that the signatures on the "403 FORBIDDEN" response from the
 * exchange demonstrating customer double-spending are valid.
 *
 * @param dh deposit handle
 * @param json json reply with the signature(s) and transaction history
 * @return #GNUNET_OK if the signature(s) is valid, #GNUNET_SYSERR if not
 */
static int
verify_deposit_signature_conflict (
  const struct TALER_EXCHANGE_DepositHandle *dh,
  const json_t *json)
{
  json_t *history;
  struct TALER_Amount total;
  enum TALER_ErrorCode ec;
  struct GNUNET_HashCode h_denom_pub;

  memset (&h_denom_pub,
          0,
          sizeof (h_denom_pub));
  history = json_object_get (json,
                             "history");
  if (GNUNET_OK !=
      TALER_EXCHANGE_verify_coin_history (&dh->dki,
                                          dh->dki.value.currency,
                                          &dh->depconf.coin_pub,
                                          history,
                                          &h_denom_pub,
                                          &total))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  ec = TALER_JSON_get_error_code (json);
  switch (ec)
  {
  case TALER_EC_EXCHANGE_DEPOSIT_INSUFFICIENT_FUNDS:
    if (0 >
        TALER_amount_add (&total,
                          &total,
                          &dh->amount_with_fee))
    {
      /* clearly not OK if our transaction would have caused
         the overflow... */
      return GNUNET_OK;
    }

    if (0 >= TALER_amount_cmp (&total,
                               &dh->dki.value))
    {
      /* transaction should have still fit */
      GNUNET_break (0);
      return GNUNET_SYSERR;
    }
    /* everything OK, proof of double-spending was provided */
    return GNUNET_OK;
  case TALER_EC_EXCHANGE_GENERIC_COIN_CONFLICTING_DENOMINATION_KEY:
    if (0 != GNUNET_memcmp (&dh->dki.h_key,
                            &h_denom_pub))
      return GNUNET_OK; /* indeed, proof with different denomination key provided */
    /* invalid proof provided */
    return GNUNET_SYSERR;
  default:
    /* unexpected error code */
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
}


/**
 * Function called when we're done processing the
 * HTTP /deposit request.
 *
 * @param cls the `struct TALER_EXCHANGE_DepositHandle`
 * @param response_code HTTP response code, 0 on error
 * @param response parsed JSON result, NULL on error
 */
static void
handle_deposit_finished (void *cls,
                         long response_code,
                         const void *response)
{
  struct TALER_EXCHANGE_DepositHandle *dh = cls;
  struct TALER_ExchangeSignatureP exchange_sig;
  struct TALER_ExchangePublicKeyP exchange_pub;
  struct TALER_ExchangeSignatureP *es = NULL;
  struct TALER_ExchangePublicKeyP *ep = NULL;
  const json_t *j = response;
  struct TALER_EXCHANGE_HttpResponse hr = {
    .reply = j,
    .http_status = (unsigned int) response_code
  };

  dh->job = NULL;
  switch (response_code)
  {
  case 0:
    hr.ec = TALER_EC_GENERIC_INVALID_RESPONSE;
    break;
  case MHD_HTTP_OK:
    if (GNUNET_OK !=
        verify_deposit_signature_ok (dh,
                                     j,
                                     &exchange_sig,
                                     &exchange_pub))
    {
      GNUNET_break_op (0);
      hr.http_status = 0;
      hr.ec = TALER_EC_EXCHANGE_DEPOSIT_INVALID_SIGNATURE_BY_EXCHANGE;
    }
    else
    {
      es = &exchange_sig;
      ep = &exchange_pub;
    }
    break;
  case MHD_HTTP_BAD_REQUEST:
    /* This should never happen, either us or the exchange is buggy
       (or API version conflict); just pass JSON reply to the application */
    hr.ec = TALER_JSON_get_error_code (j);
    hr.hint = TALER_JSON_get_error_hint (j);
    break;
  case MHD_HTTP_FORBIDDEN:
    hr.ec = TALER_JSON_get_error_code (j);
    hr.hint = TALER_JSON_get_error_hint (j);
    /* Nothing really to verify, exchange says one of the signatures is
       invalid; as we checked them, this should never happen, we
       should pass the JSON reply to the application */
    break;
  case MHD_HTTP_NOT_FOUND:
    hr.ec = TALER_JSON_get_error_code (j);
    hr.hint = TALER_JSON_get_error_hint (j);
    /* Nothing really to verify, this should never
     happen, we should pass the JSON reply to the application */
    break;
  case MHD_HTTP_CONFLICT:
    /* Double spending; check signatures on transaction history */
    if (GNUNET_OK !=
        verify_deposit_signature_conflict (dh,
                                           j))
    {
      GNUNET_break_op (0);
      hr.http_status = 0;
      hr.ec = TALER_EC_EXCHANGE_DEPOSIT_INVALID_SIGNATURE_BY_EXCHANGE;
    }
    else
    {
      hr.ec = TALER_JSON_get_error_code (j);
      hr.hint = TALER_JSON_get_error_hint (j);
    }
    break;
  case MHD_HTTP_GONE:
    /* could happen if denomination was revoked */
    /* Note: one might want to check /keys for revocation
       signature here, alas tricky in case our /keys
       is outdated => left to clients */
    hr.ec = TALER_JSON_get_error_code (j);
    hr.hint = TALER_JSON_get_error_hint (j);
    break;
  case MHD_HTTP_INTERNAL_SERVER_ERROR:
    hr.ec = TALER_JSON_get_error_code (j);
    hr.hint = TALER_JSON_get_error_hint (j);
    /* Server had an internal issue; we should retry, but this API
       leaves this to the application */
    break;
  default:
    /* unexpected response code */
    hr.ec = TALER_JSON_get_error_code (j);
    hr.hint = TALER_JSON_get_error_hint (j);
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Unexpected response code %u/%d for exchange deposit\n",
                (unsigned int) response_code,
                hr.ec);
    GNUNET_break_op (0);
    break;
  }
  dh->cb (dh->cb_cls,
          &hr,
          GNUNET_TIME_absolute_ntoh (dh->depconf.exchange_timestamp),
          es,
          ep);
  TALER_EXCHANGE_deposit_cancel (dh);
}


/**
 * Verify signature information about the deposit.
 *
 * @param dki public key information
 * @param amount the amount to be deposited
 * @param h_wire hash of the merchant???s account details
 * @param h_contract_terms hash of the contact of the merchant with the customer (further details are never disclosed to the exchange)
 * @param coin_pub coin???s public key
 * @param denom_pub denomination key with which the coin is signed
 * @param denom_pub_hash hash of @a denom_pub
 * @param denom_sig exchange???s unblinded signature of the coin
 * @param timestamp timestamp when the deposit was finalized
 * @param merchant_pub the public key of the merchant (used to identify the merchant for refund requests)
 * @param refund_deadline date until which the merchant can issue a refund to the customer via the exchange (can be zero if refunds are not allowed)
 * @param coin_sig the signature made with purpose #TALER_SIGNATURE_WALLET_COIN_DEPOSIT made by the customer with the coin???s private key.
 * @return #GNUNET_OK if signatures are OK, #GNUNET_SYSERR if not
 */
static int
verify_signatures (const struct TALER_EXCHANGE_DenomPublicKey *dki,
                   const struct TALER_Amount *amount,
                   const struct GNUNET_HashCode *h_wire,
                   const struct GNUNET_HashCode *h_contract_terms,
                   const struct TALER_CoinSpendPublicKeyP *coin_pub,
                   const struct TALER_DenominationSignature *denom_sig,
                   const struct TALER_DenominationPublicKey *denom_pub,
                   const struct GNUNET_HashCode *denom_pub_hash,
                   struct GNUNET_TIME_Absolute timestamp,
                   const struct TALER_MerchantPublicKeyP *merchant_pub,
                   struct GNUNET_TIME_Absolute refund_deadline,
                   const struct TALER_CoinSpendSignatureP *coin_sig)
{
  {
    struct TALER_DepositRequestPS dr = {
      .purpose.purpose = htonl (TALER_SIGNATURE_WALLET_COIN_DEPOSIT),
      .purpose.size = htonl (sizeof (dr)),
      .h_contract_terms = *h_contract_terms,
      .h_wire = *h_wire,
      .h_denom_pub = *denom_pub_hash,
      .wallet_timestamp = GNUNET_TIME_absolute_hton (timestamp),
      .refund_deadline = GNUNET_TIME_absolute_hton (refund_deadline),
      .merchant = *merchant_pub,
      .coin_pub = *coin_pub
    };

    TALER_amount_hton (&dr.amount_with_fee,
                       amount);
    TALER_amount_hton (&dr.deposit_fee,
                       &dki->fee_deposit);
    if (GNUNET_OK !=
        GNUNET_CRYPTO_eddsa_verify (TALER_SIGNATURE_WALLET_COIN_DEPOSIT,
                                    &dr,
                                    &coin_sig->eddsa_signature,
                                    &coin_pub->eddsa_pub))
    {
      GNUNET_break_op (0);
      TALER_LOG_WARNING ("Invalid coin signature on /deposit request!\n");
      {
        TALER_LOG_DEBUG ("... amount_with_fee was %s\n",
                         TALER_amount2s (amount));
        TALER_LOG_DEBUG ("... deposit_fee was %s\n",
                         TALER_amount2s (&dki->fee_deposit));
      }
      return GNUNET_SYSERR;
    }
  }

  /* check coin signature */
  {
    struct TALER_CoinPublicInfo coin_info = {
      .coin_pub = *coin_pub,
      .denom_pub_hash = *denom_pub_hash,
      .denom_sig = *denom_sig
    };

    if (GNUNET_YES !=
        TALER_test_coin_valid (&coin_info,
                               denom_pub))
    {
      GNUNET_break_op (0);
      TALER_LOG_WARNING ("Invalid coin passed for /deposit\n");
      return GNUNET_SYSERR;
    }
  }

  /* Check coin does make a contribution */
  if (0 < TALER_amount_cmp (&dki->fee_deposit,
                            amount))
  {
    GNUNET_break_op (0);
    TALER_LOG_WARNING ("Deposit amount smaller than fee\n");
    return GNUNET_SYSERR;
  }
  return GNUNET_OK;
}


/**
 * Sign a deposit permission.  Function for wallets.
 *
 * @param amount the amount to be deposited
 * @param deposit_fee the deposit fee we expect to pay
 * @param h_wire hash of the merchant???s account details
 * @param h_contract_terms hash of the contact of the merchant with the customer (further details are never disclosed to the exchange)
 * @param h_denom_pub hash of the coin denomination's public key
 * @param coin_priv coin???s private key
 * @param wallet_timestamp timestamp when the contract was finalized, must not be too far in the future
 * @param merchant_pub the public key of the merchant (used to identify the merchant for refund requests)
 * @param refund_deadline date until which the merchant can issue a refund to the customer via the exchange (can be zero if refunds are not allowed); must not be after the @a wire_deadline
 * @param[out] coin_sig set to the signature made with purpose #TALER_SIGNATURE_WALLET_COIN_DEPOSIT
 */
void
TALER_EXCHANGE_deposit_permission_sign (
  const struct TALER_Amount *amount,
  const struct TALER_Amount *deposit_fee,
  const struct GNUNET_HashCode *h_wire,
  const struct GNUNET_HashCode *h_contract_terms,
  const struct GNUNET_HashCode *h_denom_pub,
  const struct TALER_CoinSpendPrivateKeyP *coin_priv,
  struct GNUNET_TIME_Absolute wallet_timestamp,
  const struct TALER_MerchantPublicKeyP *merchant_pub,
  struct GNUNET_TIME_Absolute refund_deadline,
  struct TALER_CoinSpendSignatureP *coin_sig)
{
  struct TALER_DepositRequestPS dr = {
    .purpose.size = htonl
                      (sizeof (dr)),
    .purpose.purpose = htonl
                         (TALER_SIGNATURE_WALLET_COIN_DEPOSIT),
    .h_contract_terms = *h_contract_terms,
    .h_wire = *h_wire,
    .h_denom_pub = *h_denom_pub,
    .wallet_timestamp = GNUNET_TIME_absolute_hton (wallet_timestamp),
    .refund_deadline = GNUNET_TIME_absolute_hton (refund_deadline),
    .merchant = *merchant_pub
  };

  GNUNET_assert (GNUNET_OK ==
                 GNUNET_TIME_round_abs (&wallet_timestamp));
  GNUNET_assert (GNUNET_OK ==
                 GNUNET_TIME_round_abs (&refund_deadline));
  GNUNET_CRYPTO_eddsa_key_get_public (&coin_priv->eddsa_priv,
                                      &dr.coin_pub.eddsa_pub);
  TALER_amount_hton (&dr.amount_with_fee,
                     amount);
  TALER_amount_hton (&dr.deposit_fee,
                     deposit_fee);
  GNUNET_CRYPTO_eddsa_sign (&coin_priv->eddsa_priv,
                            &dr,
                            &coin_sig->eddsa_signature);
}


/**
 * Submit a deposit permission to the exchange and get the exchange's response.
 * Note that while we return the response verbatim to the caller for
 * further processing, we do already verify that the response is
 * well-formed (i.e. that signatures included in the response are all
 * valid).  If the exchange's reply is not well-formed, we return an
 * HTTP status code of zero to @a cb.
 *
 * We also verify that the @a coin_sig is valid for this deposit
 * request, and that the @a ub_sig is a valid signature for @a
 * coin_pub.  Also, the @a exchange must be ready to operate (i.e.  have
 * finished processing the /keys reply).  If either check fails, we do
 * NOT initiate the transaction with the exchange and instead return NULL.
 *
 * @param exchange the exchange handle; the exchange must be ready to operate
 * @param amount the amount to be deposited
 * @param wire_deadline date until which the merchant would like the exchange to settle the balance (advisory, the exchange cannot be
 *        forced to settle in the past or upon very short notice, but of course a well-behaved exchange will limit aggregation based on the advice received)
 * @param wire_details the merchant???s account details, in a format supported by the exchange
 * @param h_contract_terms hash of the contact of the merchant with the customer (further details are never disclosed to the exchange)
 * @param coin_pub coin???s public key
 * @param denom_pub denomination key with which the coin is signed
 * @param denom_sig exchange???s unblinded signature of the coin
 * @param timestamp timestamp when the contract was finalized, must not be too far in the future
 * @param merchant_pub the public key of the merchant (used to identify the merchant for refund requests)
 * @param refund_deadline date until which the merchant can issue a refund to the customer via the exchange (can be zero if refunds are not allowed); must not be after the @a wire_deadline
 * @param coin_sig the signature made with purpose #TALER_SIGNATURE_WALLET_COIN_DEPOSIT made by the customer with the coin???s private key.
 * @param cb the callback to call when a reply for this request is available
 * @param cb_cls closure for the above callback
 * @return a handle for this request; NULL if the inputs are invalid (i.e.
 *         signatures fail to verify).  In this case, the callback is not called.
 */
struct TALER_EXCHANGE_DepositHandle *
TALER_EXCHANGE_deposit (struct TALER_EXCHANGE_Handle *exchange,
                        const struct TALER_Amount *amount,
                        struct GNUNET_TIME_Absolute wire_deadline,
                        json_t *wire_details,
                        const struct GNUNET_HashCode *h_contract_terms,
                        const struct TALER_CoinSpendPublicKeyP *coin_pub,
                        const struct TALER_DenominationSignature *denom_sig,
                        const struct TALER_DenominationPublicKey *denom_pub,
                        struct GNUNET_TIME_Absolute timestamp,
                        const struct TALER_MerchantPublicKeyP *merchant_pub,
                        struct GNUNET_TIME_Absolute refund_deadline,
                        const struct TALER_CoinSpendSignatureP *coin_sig,
                        TALER_EXCHANGE_DepositResultCallback cb,
                        void *cb_cls)
{
  const struct TALER_EXCHANGE_Keys *key_state;
  const struct TALER_EXCHANGE_DenomPublicKey *dki;
  struct TALER_EXCHANGE_DepositHandle *dh;
  struct GNUNET_CURL_Context *ctx;
  json_t *deposit_obj;
  CURL *eh;
  struct GNUNET_HashCode h_wire;
  struct GNUNET_HashCode denom_pub_hash;
  struct TALER_Amount amount_without_fee;
  char arg_str[sizeof (struct TALER_CoinSpendPublicKeyP) * 2 + 32];

  {
    char pub_str[sizeof (struct TALER_CoinSpendPublicKeyP) * 2];
    char *end;

    end = GNUNET_STRINGS_data_to_string (
      coin_pub,
      sizeof (struct TALER_CoinSpendPublicKeyP),
      pub_str,
      sizeof (pub_str));
    *end = '\0';
    GNUNET_snprintf (arg_str,
                     sizeof (arg_str),
                     "/coins/%s/deposit",
                     pub_str);
  }
  (void) GNUNET_TIME_round_abs (&wire_deadline);
  (void) GNUNET_TIME_round_abs (&refund_deadline);
  if (refund_deadline.abs_value_us > wire_deadline.abs_value_us)
  {
    GNUNET_break (0);
    return NULL;
  }
  GNUNET_assert (GNUNET_YES ==
                 TEAH_handle_is_ready (exchange));
  /* initialize h_wire */
  if (GNUNET_OK !=
      TALER_JSON_merchant_wire_signature_hash (wire_details,
                                               &h_wire))
  {
    GNUNET_break (0);
    return NULL;
  }
  key_state = TALER_EXCHANGE_get_keys (exchange);
  dki = TALER_EXCHANGE_get_denomination_key (key_state,
                                             denom_pub);
  if (NULL == dki)
  {
    GNUNET_break (0);
    return NULL;
  }
  if (0 >
      TALER_amount_subtract (&amount_without_fee,
                             amount,
                             &dki->fee_deposit))
  {
    GNUNET_break_op (0);
    return NULL;
  }
  GNUNET_CRYPTO_rsa_public_key_hash (denom_pub->rsa_public_key,
                                     &denom_pub_hash);
  if (GNUNET_OK !=
      verify_signatures (dki,
                         amount,
                         &h_wire,
                         h_contract_terms,
                         coin_pub,
                         denom_sig,
                         denom_pub,
                         &denom_pub_hash,
                         timestamp,
                         merchant_pub,
                         refund_deadline,
                         coin_sig))
  {
    GNUNET_break_op (0);
    return NULL;
  }

  deposit_obj = json_pack ("{s:o, s:O," /* f/wire */
                           " s:o, s:o," /* h_wire, h_contract_terms */
                           " s:o," /* denom_pub */
                           " s:o, s:o," /* ub_sig, timestamp */
                           " s:o," /* merchant_pub */
                           " s:o, s:o," /* refund_deadline, wire_deadline */
                           " s:o}",     /* coin_sig */
                           "contribution", TALER_JSON_from_amount (amount),
                           "wire", wire_details,
                           "h_wire", GNUNET_JSON_from_data_auto (&h_wire),
                           "h_contract_terms", GNUNET_JSON_from_data_auto (
                             h_contract_terms),
                           "denom_pub_hash", GNUNET_JSON_from_data_auto (
                             &denom_pub_hash),
                           "ub_sig", GNUNET_JSON_from_rsa_signature (
                             denom_sig->rsa_signature),
                           "timestamp", GNUNET_JSON_from_time_abs (timestamp),
                           "merchant_pub", GNUNET_JSON_from_data_auto (
                             merchant_pub),
                           "refund_deadline", GNUNET_JSON_from_time_abs (
                             refund_deadline),
                           "wire_transfer_deadline", GNUNET_JSON_from_time_abs (
                             wire_deadline),
                           "coin_sig", GNUNET_JSON_from_data_auto (coin_sig)
                           );
  if (NULL == deposit_obj)
  {
    GNUNET_break (0);
    return NULL;
  }

  dh = GNUNET_new (struct TALER_EXCHANGE_DepositHandle);
  dh->auditor_chance = AUDITOR_CHANCE;
  dh->exchange = exchange;
  dh->cb = cb;
  dh->cb_cls = cb_cls;
  dh->url = TEAH_path_to_url (exchange,
                              arg_str);
  dh->depconf.purpose.size = htonl (sizeof (struct
                                            TALER_DepositConfirmationPS));
  dh->depconf.purpose.purpose = htonl (
    TALER_SIGNATURE_EXCHANGE_CONFIRM_DEPOSIT);
  dh->depconf.h_contract_terms = *h_contract_terms;
  dh->depconf.h_wire = h_wire;
  /* dh->depconf.exchange_timestamp; -- initialized later from exchange reply! */
  dh->depconf.refund_deadline = GNUNET_TIME_absolute_hton (refund_deadline);
  TALER_amount_hton (&dh->depconf.amount_without_fee,
                     &amount_without_fee);
  dh->depconf.coin_pub = *coin_pub;
  dh->depconf.merchant = *merchant_pub;
  dh->amount_with_fee = *amount;
  dh->dki = *dki;
  dh->dki.key.rsa_public_key = NULL; /* lifetime not warranted, so better
                                        not copy the pointer */

  eh = TALER_EXCHANGE_curl_easy_get_ (dh->url);
  if ( (NULL == eh) ||
       (GNUNET_OK !=
        TALER_curl_easy_post (&dh->ctx,
                              eh,
                              deposit_obj)) )
  {
    GNUNET_break (0);
    if (NULL != eh)
      curl_easy_cleanup (eh);
    json_decref (deposit_obj);
    GNUNET_free (dh->url);
    GNUNET_free (dh);
    return NULL;
  }
  json_decref (deposit_obj);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "URL for deposit: `%s'\n",
              dh->url);
  ctx = TEAH_handle_to_context (exchange);
  dh->job = GNUNET_CURL_job_add2 (ctx,
                                  eh,
                                  dh->ctx.headers,
                                  &handle_deposit_finished,
                                  dh);
  return dh;
}


/**
 * Change the chance that our deposit confirmation will be given to the
 * auditor to 100%.
 *
 * @param deposit the deposit permission request handle
 */
void
TALER_EXCHANGE_deposit_force_dc (struct TALER_EXCHANGE_DepositHandle *deposit)
{
  deposit->auditor_chance = 1;
}


/**
 * Cancel a deposit permission request.  This function cannot be used
 * on a request handle if a response is already served for it.
 *
 * @param deposit the deposit permission request handle
 */
void
TALER_EXCHANGE_deposit_cancel (struct TALER_EXCHANGE_DepositHandle *deposit)
{
  if (NULL != deposit->job)
  {
    GNUNET_CURL_job_cancel (deposit->job);
    deposit->job = NULL;
  }
  GNUNET_free (deposit->url);
  TALER_curl_easy_post_finished (&deposit->ctx);
  GNUNET_free (deposit);
}


/* end of exchange_api_deposit.c */
