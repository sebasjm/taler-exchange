/*
  This file is part of TALER
  Copyright (C) 2014-2019 Taler Systems SA

  TALER is free software; you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as
  published by the Free Software Foundation; either version 3,
  or (at your option) any later version.

  TALER is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty
  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General
  Public License along with TALER; see the file COPYING.  If not,
  see <http://www.gnu.org/licenses/>
*/
/**
 * @file taler-exchange-httpd_withdraw.c
 * @brief Handle /reserves/$RESERVE_PUB/withdraw requests
 * @author Florian Dold
 * @author Benedikt Mueller
 * @author Christian Grothoff
 */
#include "platform.h"
#include <gnunet/gnunet_util_lib.h>
#include <jansson.h>
#include "taler_json_lib.h"
#include "taler_mhd_lib.h"
#include "taler-exchange-httpd_withdraw.h"
#include "taler-exchange-httpd_responses.h"
#include "taler-exchange-httpd_keys.h"


/**
 * Perform RSA signature before checking with the database?
 * Reduces time spent in transaction, but may cause us to
 * waste CPU time if DB check fails.
 */
#define OPTIMISTIC_SIGN 1


/**
 * Send reserve history information to client with the
 * message that we have insufficient funds for the
 * requested withdraw operation.
 *
 * @param connection connection to the client
 * @param ebalance expected balance based on our database
 * @param rh reserve history to return
 * @return MHD result code
 */
static MHD_RESULT
reply_withdraw_insufficient_funds (
  struct MHD_Connection *connection,
  const struct TALER_Amount *ebalance,
  const struct TALER_EXCHANGEDB_ReserveHistory *rh)
{
  json_t *json_history;
  struct TALER_Amount balance;

  json_history = TEH_RESPONSE_compile_reserve_history (rh,
                                                       &balance);
  if (NULL == json_history)
    return TALER_MHD_reply_with_error (connection,
                                       MHD_HTTP_INTERNAL_SERVER_ERROR,
                                       TALER_EC_EXCHANGE_WITHDRAW_HISTORY_ERROR_INSUFFICIENT_FUNDS,
                                       NULL);
  if (0 !=
      TALER_amount_cmp (&balance,
                        ebalance))
  {
    GNUNET_break (0);
    json_decref (json_history);
    return TALER_MHD_reply_with_error (connection,
                                       MHD_HTTP_INTERNAL_SERVER_ERROR,
                                       TALER_EC_GENERIC_DB_INVARIANT_FAILURE,
                                       "reserve balance corrupt");
  }
  return TALER_MHD_reply_json_pack (
    connection,
    MHD_HTTP_CONFLICT,
    "{s:s, s:I, s:o, s:o}",
    "hint",
    TALER_ErrorCode_get_hint (TALER_EC_EXCHANGE_WITHDRAW_INSUFFICIENT_FUNDS),
    "code",
    (json_int_t) TALER_EC_EXCHANGE_WITHDRAW_INSUFFICIENT_FUNDS,
    "balance",
    TALER_JSON_from_amount (&balance),
    "history",
    json_history);
}


/**
 * Context for #withdraw_transaction.
 */
struct WithdrawContext
{
  /**
   * Details about the withdrawal request.
   */
  struct TALER_WithdrawRequestPS wsrd;

  /**
   * Value of the coin plus withdraw fee.
   */
  struct TALER_Amount amount_required;

  /**
   * Hash of the denomination public key.
   */
  struct GNUNET_HashCode denom_pub_hash;

  /**
   * Signature over the request.
   */
  struct TALER_ReserveSignatureP signature;

  /**
   * Blinded planchet.
   */
  char *blinded_msg;

  /**
   * Number of bytes in @e blinded_msg.
   */
  size_t blinded_msg_len;

  /**
   * Set to the resulting signed coin data to be returned to the client.
   */
  struct TALER_EXCHANGEDB_CollectableBlindcoin collectable;

};


/**
 * Function implementing withdraw transaction.  Runs the
 * transaction logic; IF it returns a non-error code, the transaction
 * logic MUST NOT queue a MHD response.  IF it returns an hard error,
 * the transaction logic MUST queue a MHD response and set @a mhd_ret.
 * IF it returns the soft error code, the function MAY be called again
 * to retry and MUST not queue a MHD response.
 *
 * Note that "wc->collectable.sig" may already be set before entering
 * this function, either because OPTIMISTIC_SIGN was used and we signed
 * before entering the transaction, or because this function is run
 * twice (!) by #TEH_DB_run_transaction() and the first time created
 * the signature and then failed to commit.  Furthermore, we may get
 * a 2nd correct signature briefly if "get_withdraw_info" succeeds and
 * finds one in the DB.  To avoid signing twice, the function may
 * return a valid signature in "wc->collectable.sig" **even if it failed**.
 * The caller must thus free the signature in either case.
 *
 * @param cls a `struct WithdrawContext *`
 * @param connection MHD request which triggered the transaction
 * @param session database session to use
 * @param[out] mhd_ret set to MHD response status for @a connection,
 *             if transaction failed (!)
 * @return transaction status
 */
static enum GNUNET_DB_QueryStatus
withdraw_transaction (void *cls,
                      struct MHD_Connection *connection,
                      struct TALER_EXCHANGEDB_Session *session,
                      MHD_RESULT *mhd_ret)
{
  struct WithdrawContext *wc = cls;
  struct TALER_EXCHANGEDB_Reserve r;
  enum GNUNET_DB_QueryStatus qs;
  struct TALER_DenominationSignature denom_sig;

#if OPTIMISTIC_SIGN
  /* store away optimistic signature to protect
     it from being overwritten by get_withdraw_info */
  denom_sig = wc->collectable.sig;
  wc->collectable.sig.rsa_signature = NULL;
#endif
  qs = TEH_plugin->get_withdraw_info (TEH_plugin->cls,
                                      session,
                                      &wc->wsrd.h_coin_envelope,
                                      &wc->collectable);
  if (0 > qs)
  {
    GNUNET_break (GNUNET_DB_STATUS_SOFT_ERROR == qs);
    if (GNUNET_DB_STATUS_HARD_ERROR == qs)
      *mhd_ret = TALER_MHD_reply_with_error (connection,
                                             MHD_HTTP_INTERNAL_SERVER_ERROR,
                                             TALER_EC_GENERIC_DB_FETCH_FAILED,
                                             "withdraw details");
    wc->collectable.sig = denom_sig;
    return qs;
  }

  /* Don't sign again if we have already signed the coin */
  if (GNUNET_DB_STATUS_SUCCESS_ONE_RESULT == qs)
  {
    /* Toss out the optimistic signature, we got another one from the DB;
       optimization trade-off loses in this case: we unnecessarily computed
       a signature :-( */
#if OPTIMISTIC_SIGN
    GNUNET_CRYPTO_rsa_signature_free (denom_sig.rsa_signature);
#endif
    return GNUNET_DB_STATUS_SUCCESS_ONE_RESULT;
  }
  /* We should never get more than one result, and we handled
     the errors (negative case) above, so that leaves no results. */
  GNUNET_assert (GNUNET_DB_STATUS_SUCCESS_NO_RESULTS == qs);
  wc->collectable.sig = denom_sig; /* Note: might still be NULL if we didn't do OPTIMISTIC_SIGN */

  /* Check if balance is sufficient */
  r.pub = wc->wsrd.reserve_pub; /* other fields of 'r' initialized in reserves_get (if successful) */
  GNUNET_log (GNUNET_ERROR_TYPE_INFO,
              "Trying to withdraw from reserve: %s\n",
              TALER_B2S (&r.pub));
  qs = TEH_plugin->reserves_get (TEH_plugin->cls,
                                 session,
                                 &r);
  if (0 > qs)
  {
    if (GNUNET_DB_STATUS_HARD_ERROR == qs)
      *mhd_ret = TALER_MHD_reply_with_error (connection,
                                             MHD_HTTP_INTERNAL_SERVER_ERROR,
                                             TALER_EC_GENERIC_DB_FETCH_FAILED,
                                             "reserves");
    return qs;
  }
  if (GNUNET_DB_STATUS_SUCCESS_NO_RESULTS == qs)
  {
    *mhd_ret = TALER_MHD_reply_with_error (connection,
                                           MHD_HTTP_NOT_FOUND,
                                           TALER_EC_EXCHANGE_WITHDRAW_RESERVE_UNKNOWN,
                                           NULL);
    return GNUNET_DB_STATUS_HARD_ERROR;
  }
  if (0 < TALER_amount_cmp (&wc->amount_required,
                            &r.balance))
  {
    struct TALER_EXCHANGEDB_ReserveHistory *rh;

    /* The reserve does not have the required amount (actual
     * amount + withdraw fee) */
#if GNUNET_EXTRA_LOGGING
    {
      char *amount_required;
      char *r_balance;

      amount_required = TALER_amount_to_string (&wc->amount_required);
      r_balance = TALER_amount_to_string (&r.balance);
      TALER_LOG_DEBUG ("Asked %s over a reserve worth %s\n",
                       amount_required,
                       r_balance);
      GNUNET_free (amount_required);
      GNUNET_free (r_balance);
    }
#endif
    qs = TEH_plugin->get_reserve_history (TEH_plugin->cls,
                                          session,
                                          &wc->wsrd.reserve_pub,
                                          &rh);
    if (NULL == rh)
    {
      if (GNUNET_DB_STATUS_HARD_ERROR == qs)
        *mhd_ret = TALER_MHD_reply_with_error (connection,
                                               MHD_HTTP_INTERNAL_SERVER_ERROR,
                                               TALER_EC_GENERIC_DB_FETCH_FAILED,
                                               "reserve history");
      return GNUNET_DB_STATUS_HARD_ERROR;
    }
    *mhd_ret = reply_withdraw_insufficient_funds (connection,
                                                  &r.balance,
                                                  rh);
    TEH_plugin->free_reserve_history (TEH_plugin->cls,
                                      rh);
    return GNUNET_DB_STATUS_HARD_ERROR;
  }

  /* Balance is good, sign the coin! */
#if ! OPTIMISTIC_SIGN
  if (NULL == wc->collectable.sig.rsa_signature)
  {
    enum TALER_ErrorCode ec;

    wc->collectable.sig
      = TEH_keys_denomination_sign (&wc->denom_pub_hash,
                                    wc->blinded_msg,
                                    wc->blinded_msg_len,
                                    &ec);
    if (NULL == wc->collectable.sig.rsa_signature)
    {
      GNUNET_break (0);
      *mhd_ret = TALER_MHD_reply_with_ec (connection,
                                          ec,
                                          NULL);
      return GNUNET_DB_STATUS_HARD_ERROR;
    }
  }
#endif
  wc->collectable.denom_pub_hash = wc->denom_pub_hash;
  wc->collectable.amount_with_fee = wc->amount_required;
  wc->collectable.reserve_pub = wc->wsrd.reserve_pub;
  wc->collectable.h_coin_envelope = wc->wsrd.h_coin_envelope;
  wc->collectable.reserve_sig = wc->signature;
  qs = TEH_plugin->insert_withdraw_info (TEH_plugin->cls,
                                         session,
                                         &wc->collectable);
  if (0 > qs)
  {
    GNUNET_break (GNUNET_DB_STATUS_SOFT_ERROR == qs);
    if (GNUNET_DB_STATUS_HARD_ERROR == qs)
      *mhd_ret = TALER_MHD_reply_with_error (connection,
                                             MHD_HTTP_INTERNAL_SERVER_ERROR,
                                             TALER_EC_GENERIC_DB_STORE_FAILED,
                                             "withdraw details");
    return qs;
  }
  return qs;
}


/**
 * Handle a "/reserves/$RESERVE_PUB/withdraw" request.  Parses the
 * "reserve_pub" EdDSA key of the reserve and the requested "denom_pub" which
 * specifies the key/value of the coin to be withdrawn, and checks that the
 * signature "reserve_sig" makes this a valid withdrawal request from the
 * specified reserve.  If so, the envelope with the blinded coin "coin_ev" is
 * passed down to execute the withdrawal operation.
 *
 * @param rh context of the handler
 * @param connection the MHD connection to handle
 * @param root uploaded JSON data
 * @param args array of additional options (first must be the
 *         reserve public key, the second one should be "withdraw")
 * @return MHD result code
 */
MHD_RESULT
TEH_handler_withdraw (const struct TEH_RequestHandler *rh,
                      struct MHD_Connection *connection,
                      const json_t *root,
                      const char *const args[2])
{
  struct WithdrawContext wc;
  struct GNUNET_JSON_Specification spec[] = {
    GNUNET_JSON_spec_varsize ("coin_ev",
                              (void **) &wc.blinded_msg,
                              &wc.blinded_msg_len),
    GNUNET_JSON_spec_fixed_auto ("reserve_sig",
                                 &wc.signature),
    GNUNET_JSON_spec_fixed_auto ("denom_pub_hash",
                                 &wc.denom_pub_hash),
    GNUNET_JSON_spec_end ()
  };
  enum TALER_ErrorCode ec;
  struct TEH_DenominationKey *dk;

  (void) rh;
  if (GNUNET_OK !=
      GNUNET_STRINGS_string_to_data (args[0],
                                     strlen (args[0]),
                                     &wc.wsrd.reserve_pub,
                                     sizeof (wc.wsrd.reserve_pub)))
  {
    GNUNET_break_op (0);
    return TALER_MHD_reply_with_error (connection,
                                       MHD_HTTP_BAD_REQUEST,
                                       TALER_EC_MERCHANT_GENERIC_RESERVE_PUB_MALFORMED,
                                       args[0]);
  }

  {
    enum GNUNET_GenericReturnValue res;

    res = TALER_MHD_parse_json_data (connection,
                                     root,
                                     spec);
    if (GNUNET_OK != res)
      return (GNUNET_SYSERR == res) ? MHD_NO : MHD_YES;
  }
  {
    unsigned int hc;
    enum TALER_ErrorCode ec;
    struct GNUNET_TIME_Absolute now;

    dk = TEH_keys_denomination_by_hash (&wc.denom_pub_hash,
                                        &ec,
                                        &hc);
    if (NULL == dk)
    {
      GNUNET_JSON_parse_free (spec);
      return TALER_MHD_reply_with_error (connection,
                                         hc,
                                         ec,
                                         NULL);
    }
    now = GNUNET_TIME_absolute_get ();
    if (now.abs_value_us >= dk->meta.expire_withdraw.abs_value_us)
    {
      /* This denomination is past the expiration time for withdraws */
      GNUNET_JSON_parse_free (spec);
      return TALER_MHD_reply_with_error (
        connection,
        MHD_HTTP_GONE,
        TALER_EC_EXCHANGE_GENERIC_DENOMINATION_EXPIRED,
        NULL);
    }
    if (now.abs_value_us < dk->meta.start.abs_value_us)
    {
      /* This denomination is not yet valid */
      GNUNET_JSON_parse_free (spec);
      return TALER_MHD_reply_with_error (
        connection,
        MHD_HTTP_PRECONDITION_FAILED,
        TALER_EC_EXCHANGE_GENERIC_DENOMINATION_VALIDITY_IN_FUTURE,
        NULL);
    }
    if (dk->recoup_possible)
    {
      /* This denomination has been revoked */
      GNUNET_JSON_parse_free (spec);
      return TALER_MHD_reply_with_error (
        connection,
        MHD_HTTP_GONE,
        TALER_EC_EXCHANGE_GENERIC_DENOMINATION_REVOKED,
        NULL);
    }
  }

  {
    if (0 >
        TALER_amount_add (&wc.amount_required,
                          &dk->meta.value,
                          &dk->meta.fee_withdraw))
    {
      GNUNET_JSON_parse_free (spec);
      return TALER_MHD_reply_with_error (connection,
                                         MHD_HTTP_INTERNAL_SERVER_ERROR,
                                         TALER_EC_EXCHANGE_WITHDRAW_AMOUNT_FEE_OVERFLOW,
                                         NULL);
    }
    TALER_amount_hton (&wc.wsrd.amount_with_fee,
                       &wc.amount_required);
  }

  /* verify signature! */
  wc.wsrd.purpose.size
    = htonl (sizeof (wc.wsrd));
  wc.wsrd.purpose.purpose
    = htonl (TALER_SIGNATURE_WALLET_RESERVE_WITHDRAW);
  wc.wsrd.h_denomination_pub
    = wc.denom_pub_hash;
  GNUNET_CRYPTO_hash (wc.blinded_msg,
                      wc.blinded_msg_len,
                      &wc.wsrd.h_coin_envelope);
  if (GNUNET_OK !=
      GNUNET_CRYPTO_eddsa_verify (TALER_SIGNATURE_WALLET_RESERVE_WITHDRAW,
                                  &wc.wsrd,
                                  &wc.signature.eddsa_signature,
                                  &wc.wsrd.reserve_pub.eddsa_pub))
  {
    TALER_LOG_WARNING (
      "Client supplied invalid signature for withdraw request\n");
    GNUNET_JSON_parse_free (spec);
    return TALER_MHD_reply_with_error (connection,
                                       MHD_HTTP_FORBIDDEN,
                                       TALER_EC_EXCHANGE_WITHDRAW_RESERVE_SIGNATURE_INVALID,
                                       NULL);
  }

#if OPTIMISTIC_SIGN
  /* Sign before transaction! */
  wc.collectable.sig
    = TEH_keys_denomination_sign (&wc.denom_pub_hash,
                                  wc.blinded_msg,
                                  wc.blinded_msg_len,
                                  &ec);
  if (NULL == wc.collectable.sig.rsa_signature)
  {
    GNUNET_break (0);
    GNUNET_JSON_parse_free (spec);
    return TALER_MHD_reply_with_ec (connection,
                                    ec,
                                    NULL);
  }
#endif

  /* run transaction and sign (if not optimistically signed before) */
  {
    MHD_RESULT mhd_ret;

    if (GNUNET_OK !=
        TEH_DB_run_transaction (connection,
                                "run withdraw",
                                &mhd_ret,
                                &withdraw_transaction,
                                &wc))
    {
      /* Even if #withdraw_transaction() failed, it may have created a signature
         (or we might have done it optimistically above). */
      if (NULL != wc.collectable.sig.rsa_signature)
        GNUNET_CRYPTO_rsa_signature_free (wc.collectable.sig.rsa_signature);
      GNUNET_JSON_parse_free (spec);
      return mhd_ret;
    }
  }

  /* Clean up and send back final (positive) response */
  GNUNET_JSON_parse_free (spec);

  {
    MHD_RESULT ret;

    ret = TALER_MHD_reply_json_pack (
      connection,
      MHD_HTTP_OK,
      "{s:o}",
      "ev_sig", GNUNET_JSON_from_rsa_signature (
        wc.collectable.sig.rsa_signature));
    GNUNET_CRYPTO_rsa_signature_free (wc.collectable.sig.rsa_signature);
    return ret;
  }
}


/* end of taler-exchange-httpd_withdraw.c */
