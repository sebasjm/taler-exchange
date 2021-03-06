/*
  This file is part of TALER
  Copyright (C) 2014-2018 Taler Systems SA

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
 * @file taler-exchange-httpd_transfers_get.c
 * @brief Handle wire transfer(s) GET requests
 * @author Christian Grothoff
 */
#include "platform.h"
#include <gnunet/gnunet_util_lib.h>
#include <jansson.h>
#include <microhttpd.h>
#include <pthread.h>
#include "taler_signatures.h"
#include "taler-exchange-httpd_keys.h"
#include "taler-exchange-httpd_transfers_get.h"
#include "taler-exchange-httpd_responses.h"
#include "taler_json_lib.h"
#include "taler_mhd_lib.h"


/**
 * Information about one of the transactions that was
 * aggregated, to be returned in the /transfers response.
 */
struct AggregatedDepositDetail
{

  /**
   * We keep deposit details in a DLL.
   */
  struct AggregatedDepositDetail *next;

  /**
   * We keep deposit details in a DLL.
   */
  struct AggregatedDepositDetail *prev;

  /**
   * Hash of the contract terms.
   */
  struct GNUNET_HashCode h_contract_terms;

  /**
   * Coin's public key of the deposited coin.
   */
  struct TALER_CoinSpendPublicKeyP coin_pub;

  /**
   * Total value of the coin in the deposit.
   */
  struct TALER_Amount deposit_value;

  /**
   * Fees charged by the exchange for the deposit of this coin.
   */
  struct TALER_Amount deposit_fee;
};


/**
 * A merchant asked for transaction details about a wire transfer.
 * Provide them. Generates the 200 reply.
 *
 * @param connection connection to the client
 * @param total total amount that was transferred
 * @param merchant_pub public key of the merchant
 * @param h_wire destination account
 * @param wire_fee wire fee that was charged
 * @param exec_time execution time of the wire transfer
 * @param wdd_head linked list with details about the combined deposits
 * @return MHD result code
 */
static MHD_RESULT
reply_transfer_details (struct MHD_Connection *connection,
                        const struct TALER_Amount *total,
                        const struct TALER_MerchantPublicKeyP *merchant_pub,
                        const struct GNUNET_HashCode *h_wire,
                        const struct TALER_Amount *wire_fee,
                        struct GNUNET_TIME_Absolute exec_time,
                        const struct AggregatedDepositDetail *wdd_head)
{
  json_t *deposits;
  struct TALER_WireDepositDetailP dd;
  struct GNUNET_HashContext *hash_context;
  struct TALER_WireDepositDataPS wdp;
  struct TALER_ExchangePublicKeyP pub;
  struct TALER_ExchangeSignatureP sig;


  GNUNET_TIME_round_abs (&exec_time);
  deposits = json_array ();
  if (NULL == deposits)
  {
    return TALER_MHD_reply_with_error (connection,
                                       MHD_HTTP_INTERNAL_SERVER_ERROR,
                                       TALER_EC_GENERIC_JSON_ALLOCATION_FAILURE,
                                       "json_array() failed");

  }
  hash_context = GNUNET_CRYPTO_hash_context_start ();
  for (const struct AggregatedDepositDetail *wdd_pos = wdd_head;
       NULL != wdd_pos;
       wdd_pos = wdd_pos->next)
  {
    dd.h_contract_terms = wdd_pos->h_contract_terms;
    dd.execution_time = GNUNET_TIME_absolute_hton (exec_time);
    dd.coin_pub = wdd_pos->coin_pub;
    TALER_amount_hton (&dd.deposit_value,
                       &wdd_pos->deposit_value);
    TALER_amount_hton (&dd.deposit_fee,
                       &wdd_pos->deposit_fee);
    GNUNET_CRYPTO_hash_context_read (hash_context,
                                     &dd,
                                     sizeof (struct TALER_WireDepositDetailP));
    if (0 !=
        json_array_append_new (deposits,
                               json_pack ("{s:o, s:o, s:o, s:o}",
                                          "h_contract_terms",
                                          GNUNET_JSON_from_data_auto (
                                            &wdd_pos->h_contract_terms),
                                          "coin_pub",
                                          GNUNET_JSON_from_data_auto (
                                            &wdd_pos->coin_pub),
                                          "deposit_value",
                                          TALER_JSON_from_amount (
                                            &wdd_pos->deposit_value),
                                          "deposit_fee",
                                          TALER_JSON_from_amount (
                                            &wdd_pos->deposit_fee))))
    {
      json_decref (deposits);
      GNUNET_CRYPTO_hash_context_abort (hash_context);
      return TALER_MHD_reply_with_error (connection,
                                         MHD_HTTP_INTERNAL_SERVER_ERROR,
                                         TALER_EC_GENERIC_JSON_ALLOCATION_FAILURE,
                                         "json_array_append_new() failed");
    }
  }
  wdp.purpose.purpose = htonl (TALER_SIGNATURE_EXCHANGE_CONFIRM_WIRE_DEPOSIT);
  wdp.purpose.size = htonl (sizeof (struct TALER_WireDepositDataPS));
  TALER_amount_hton (&wdp.total,
                     total);
  TALER_amount_hton (&wdp.wire_fee,
                     wire_fee);
  wdp.merchant_pub = *merchant_pub;
  wdp.h_wire = *h_wire;
  GNUNET_CRYPTO_hash_context_finish (hash_context,
                                     &wdp.h_details);
  {
    enum TALER_ErrorCode ec;

    if (TALER_EC_NONE !=
        (ec = TEH_keys_exchange_sign (&wdp,
                                      &pub,
                                      &sig)))
    {
      json_decref (deposits);
      return TALER_MHD_reply_with_ec (connection,
                                      ec,
                                      NULL);
    }
  }

  return TALER_MHD_reply_json_pack (connection,
                                    MHD_HTTP_OK,
                                    "{s:o, s:o, s:o, s:o, s:o, s:o, s:o, s:o}",
                                    "total", TALER_JSON_from_amount (total),
                                    "wire_fee", TALER_JSON_from_amount (
                                      wire_fee),
                                    "merchant_pub",
                                    GNUNET_JSON_from_data_auto (
                                      merchant_pub),
                                    "h_wire", GNUNET_JSON_from_data_auto (
                                      h_wire),
                                    "execution_time",
                                    GNUNET_JSON_from_time_abs (exec_time),
                                    "deposits", deposits,
                                    "exchange_sig",
                                    GNUNET_JSON_from_data_auto (&sig),
                                    "exchange_pub",
                                    GNUNET_JSON_from_data_auto (&pub));
}


/**
 * Closure for #handle_transaction_data.
 */
struct WtidTransactionContext
{

  /**
   * Identifier of the wire transfer to track.
   */
  struct TALER_WireTransferIdentifierRawP wtid;

  /**
   * Total amount of the wire transfer, as calculated by
   * summing up the individual amounts. To be rounded down
   * to calculate the real transfer amount at the end.
   * Only valid if @e is_valid is #GNUNET_YES.
   */
  struct TALER_Amount total;

  /**
   * Public key of the merchant, only valid if @e is_valid
   * is #GNUNET_YES.
   */
  struct TALER_MerchantPublicKeyP merchant_pub;

  /**
   * Hash of the wire details of the merchant (identical for all
   * deposits), only valid if @e is_valid is #GNUNET_YES.
   */
  struct GNUNET_HashCode h_wire;

  /**
   * Wire fee applicable at @e exec_time.
   */
  struct TALER_Amount wire_fee;

  /**
   * Execution time of the wire transfer
   */
  struct GNUNET_TIME_Absolute exec_time;

  /**
   * Head of DLL with deposit details for transfers GET response.
   */
  struct AggregatedDepositDetail *wdd_head;

  /**
   * Tail of DLL with deposit details for transfers GET response.
   */
  struct AggregatedDepositDetail *wdd_tail;

  /**
   * Which method was used to wire the funds?
   */
  char *wire_method;

  /**
   * JSON array with details about the individual deposits.
   */
  json_t *deposits;

  /**
   * Initially #GNUNET_NO, if we found no deposits so far.  Set to
   * #GNUNET_YES if we got transaction data, and the database replies
   * remained consistent with respect to @e merchant_pub and @e h_wire
   * (as they should).  Set to #GNUNET_SYSERR if we encountered an
   * internal error.
   */
  int is_valid;

};


/**
 * Function called with the results of the lookup of the individual deposits
 * that were aggregated for the given wire transfer.
 *
 * @param cls our context for transmission
 * @param rowid which row in the DB is the information from (for diagnostics), ignored
 * @param merchant_pub public key of the merchant (should be same for all callbacks with the same @e cls)
 * @param h_wire hash of wire transfer details of the merchant (should be same for all callbacks with the same @e cls)
 * @param wire where the funds were sent
 * @param exec_time execution time of the wire transfer (should be same for all callbacks with the same @e cls)
 * @param h_contract_terms which proposal was this payment about
 * @param denom_pub denomination public key of the @a coin_pub (ignored)
 * @param coin_pub which public key was this payment about
 * @param deposit_value amount contributed by this coin in total
 * @param deposit_fee deposit fee charged by exchange for this coin
 */
static void
handle_deposit_data (void *cls,
                     uint64_t rowid,
                     const struct TALER_MerchantPublicKeyP *merchant_pub,
                     const struct GNUNET_HashCode *h_wire,
                     const json_t *wire,
                     struct GNUNET_TIME_Absolute exec_time,
                     const struct GNUNET_HashCode *h_contract_terms,
                     const struct TALER_DenominationPublicKey *denom_pub,
                     const struct TALER_CoinSpendPublicKeyP *coin_pub,
                     const struct TALER_Amount *deposit_value,
                     const struct TALER_Amount *deposit_fee)
{
  struct WtidTransactionContext *ctx = cls;
  char *wire_method;

  (void) rowid;
  (void) denom_pub;
  if (GNUNET_SYSERR == ctx->is_valid)
    return;
  if (NULL == (wire_method = TALER_JSON_wire_to_method (wire)))
  {
    GNUNET_break (0);
    ctx->is_valid = GNUNET_SYSERR;
    return;
  }
  if (GNUNET_NO == ctx->is_valid)
  {
    /* First one we encounter, setup general information in 'ctx' */
    ctx->merchant_pub = *merchant_pub;
    ctx->h_wire = *h_wire;
    ctx->exec_time = exec_time;
    ctx->wire_method = wire_method; /* captures the reference */
    ctx->is_valid = GNUNET_YES;
    if (0 >
        TALER_amount_subtract (&ctx->total,
                               deposit_value,
                               deposit_fee))
    {
      GNUNET_break (0);
      ctx->is_valid = GNUNET_SYSERR;
      return;
    }
  }
  else
  {
    struct TALER_Amount delta;

    /* Subsequent data, check general information matches that in 'ctx';
       (it should, otherwise the deposits should not have been aggregated) */
    if ( (0 != GNUNET_memcmp (&ctx->merchant_pub,
                              merchant_pub)) ||
         (0 != strcmp (wire_method,
                       ctx->wire_method)) ||
         (0 != GNUNET_memcmp (&ctx->h_wire,
                              h_wire)) )
    {
      GNUNET_break (0);
      ctx->is_valid = GNUNET_SYSERR;
      GNUNET_free (wire_method);
      return;
    }
    GNUNET_free (wire_method);
    if (0 >
        TALER_amount_subtract (&delta,
                               deposit_value,
                               deposit_fee))
    {
      GNUNET_break (0);
      ctx->is_valid = GNUNET_SYSERR;
      return;
    }
    if (0 >
        TALER_amount_add (&ctx->total,
                          &ctx->total,
                          &delta))
    {
      GNUNET_break (0);
      ctx->is_valid = GNUNET_SYSERR;
      return;
    }
  }

  {
    struct AggregatedDepositDetail *wdd;

    wdd = GNUNET_new (struct AggregatedDepositDetail);
    wdd->deposit_value = *deposit_value;
    wdd->deposit_fee = *deposit_fee;
    wdd->h_contract_terms = *h_contract_terms;
    wdd->coin_pub = *coin_pub;
    GNUNET_CONTAINER_DLL_insert (ctx->wdd_head,
                                 ctx->wdd_tail,
                                 wdd);
  }
}


/**
 * Free data structure reachable from @a ctx, but not @a ctx itself.
 *
 * @param ctx context to free
 */
static void
free_ctx (struct WtidTransactionContext *ctx)
{
  struct AggregatedDepositDetail *wdd;

  while (NULL != (wdd = ctx->wdd_head))
  {
    GNUNET_CONTAINER_DLL_remove (ctx->wdd_head,
                                 ctx->wdd_tail,
                                 wdd);
    GNUNET_free (wdd);
  }
  GNUNET_free (ctx->wire_method);
  ctx->wire_method = NULL;
}


/**
 * Execute a "/transfers" GET operation.  Returns the deposit details of the
 * deposits that were aggregated to create the given wire transfer.
 *
 * If it returns a non-error code, the transaction logic MUST
 * NOT queue a MHD response.  IF it returns an hard error, the
 * transaction logic MUST queue a MHD response and set @a mhd_ret.  IF
 * it returns the soft error code, the function MAY be called again to
 * retry and MUST not queue a MHD response.
 *
 * @param cls closure
 * @param connection MHD request which triggered the transaction
 * @param session database session to use
 * @param[out] mhd_ret set to MHD response status for @a connection,
 *             if transaction failed (!)
 * @return transaction status
 */
static enum GNUNET_DB_QueryStatus
get_transfer_deposits (void *cls,
                       struct MHD_Connection *connection,
                       struct TALER_EXCHANGEDB_Session *session,
                       MHD_RESULT *mhd_ret)
{
  struct WtidTransactionContext *ctx = cls;
  enum GNUNET_DB_QueryStatus qs;
  struct GNUNET_TIME_Absolute wire_fee_start_date;
  struct GNUNET_TIME_Absolute wire_fee_end_date;
  struct TALER_MasterSignatureP wire_fee_master_sig;
  struct TALER_Amount closing_fee;

  /* resetting to NULL/0 in case transaction was repeated after
     serialization failure */
  free_ctx (ctx);
  qs = TEH_plugin->lookup_wire_transfer (TEH_plugin->cls,
                                         session,
                                         &ctx->wtid,
                                         &handle_deposit_data,
                                         ctx);
  if (0 > qs)
  {
    if (GNUNET_DB_STATUS_HARD_ERROR == qs)
    {
      GNUNET_break (0);
      *mhd_ret = TALER_MHD_reply_with_error (connection,
                                             MHD_HTTP_INTERNAL_SERVER_ERROR,
                                             TALER_EC_GENERIC_DB_FETCH_FAILED,
                                             "wire transfer");
    }
    return qs;
  }
  if (GNUNET_SYSERR == ctx->is_valid)
  {
    GNUNET_break (0);
    *mhd_ret = TALER_MHD_reply_with_error (connection,
                                           MHD_HTTP_INTERNAL_SERVER_ERROR,
                                           TALER_EC_GENERIC_DB_INVARIANT_FAILURE,
                                           "wire history malformed");
    return GNUNET_DB_STATUS_HARD_ERROR;
  }
  if (GNUNET_NO == ctx->is_valid)
  {
    *mhd_ret = TALER_MHD_reply_with_error (connection,
                                           MHD_HTTP_NOT_FOUND,
                                           TALER_EC_EXCHANGE_TRANSFERS_GET_WTID_NOT_FOUND,
                                           NULL);
    return GNUNET_DB_STATUS_HARD_ERROR;
  }
  qs = TEH_plugin->get_wire_fee (TEH_plugin->cls,
                                 session,
                                 ctx->wire_method,
                                 ctx->exec_time,
                                 &wire_fee_start_date,
                                 &wire_fee_end_date,
                                 &ctx->wire_fee,
                                 &closing_fee,
                                 &wire_fee_master_sig);
  if (0 >= qs)
  {
    if ( (GNUNET_DB_STATUS_HARD_ERROR == qs) ||
         (GNUNET_DB_STATUS_SUCCESS_NO_RESULTS == qs) )
    {
      GNUNET_break (0);
      *mhd_ret = TALER_MHD_reply_with_error (connection,
                                             MHD_HTTP_INTERNAL_SERVER_ERROR,
                                             TALER_EC_EXCHANGE_TRANSFERS_GET_WIRE_FEE_NOT_FOUND,
                                             NULL);
    }
    return qs;
  }
  if (0 >
      TALER_amount_subtract (&ctx->total,
                             &ctx->total,
                             &ctx->wire_fee))
  {
    GNUNET_break (0);
    *mhd_ret = TALER_MHD_reply_with_error (connection,
                                           MHD_HTTP_INTERNAL_SERVER_ERROR,
                                           TALER_EC_EXCHANGE_TRANSFERS_GET_WIRE_FEE_INCONSISTENT,
                                           NULL);
    return GNUNET_DB_STATUS_HARD_ERROR;
  }
  return GNUNET_DB_STATUS_SUCCESS_ONE_RESULT;
}


/**
 * Handle a GET "/transfers/$WTID" request.
 *
 * @param rh context of the handler
 * @param connection the MHD connection to handle
 * @param args array of additional options (length: 1, just the wtid)
 * @return MHD result code
 */
MHD_RESULT
TEH_handler_transfers_get (const struct TEH_RequestHandler *rh,
                           struct MHD_Connection *connection,
                           const char *const args[1])
{
  struct WtidTransactionContext ctx;
  MHD_RESULT mhd_ret;

  (void) rh;
  memset (&ctx,
          0,
          sizeof (ctx));
  if (GNUNET_OK !=
      GNUNET_STRINGS_string_to_data (args[0],
                                     strlen (args[0]),
                                     &ctx.wtid,
                                     sizeof (ctx.wtid)))
  {
    GNUNET_break_op (0);
    return TALER_MHD_reply_with_error (connection,
                                       MHD_HTTP_BAD_REQUEST,
                                       TALER_EC_EXCHANGE_TRANSFERS_GET_WTID_MALFORMED,
                                       args[0]);
  }
  if (GNUNET_OK !=
      TEH_DB_run_transaction (connection,
                              "run transfers GET",
                              &mhd_ret,
                              &get_transfer_deposits,
                              &ctx))
  {
    free_ctx (&ctx);
    return mhd_ret;
  }
  mhd_ret = reply_transfer_details (connection,
                                    &ctx.total,
                                    &ctx.merchant_pub,
                                    &ctx.h_wire,
                                    &ctx.wire_fee,
                                    ctx.exec_time,
                                    ctx.wdd_head);
  free_ctx (&ctx);
  return mhd_ret;
}


/* end of taler-exchange-httpd_transfers_get.c */
