/*
  This file is part of TALER
  Copyright (C) 2018-2020 Taler Systems SA

  TALER is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3, or (at your
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
 * @file testing/testing_api_cmd_deposit.c
 * @brief command for testing /deposit.
 * @author Marcello Stanisci
 */
#include "platform.h"
#include "taler_json_lib.h"
#include <gnunet/gnunet_curl_lib.h>
#include "taler_testing_lib.h"
#include "taler_signatures.h"
#include "backoff.h"


/**
 * How often do we retry before giving up?
 */
#define NUM_RETRIES 5

/**
 * How long do we wait AT MOST when retrying?
 */
#define MAX_BACKOFF GNUNET_TIME_relative_multiply ( \
    GNUNET_TIME_UNIT_MILLISECONDS, 100)


/**
 * State for a "deposit" CMD.
 */
struct DepositState
{

  /**
   * Amount to deposit.
   */
  struct TALER_Amount amount;

  /**
   * Deposit fee.
   */
  struct TALER_Amount deposit_fee;

  /**
   * Reference to any command that is able to provide a coin.
   */
  const char *coin_reference;

  /**
   * If @e coin_reference refers to an operation that generated
   * an array of coins, this value determines which coin to pick.
   */
  unsigned int coin_index;

  /**
   * Wire details of who is depositing -- this would be merchant
   * wire details in a normal scenario.
   */
  json_t *wire_details;

  /**
   * JSON string describing what a proposal is about.
   */
  json_t *contract_terms;

  /**
   * Refund deadline. Zero for no refunds.
   */
  struct GNUNET_TIME_Absolute refund_deadline;

  /**
   * Set (by the interpreter) to a fresh private key.  This
   * key will be used to sign the deposit request.
   */
  struct TALER_MerchantPrivateKeyP merchant_priv;

  /**
   * Deposit handle while operation is running.
   */
  struct TALER_EXCHANGE_DepositHandle *dh;

  /**
   * Timestamp of the /deposit operation in the wallet (contract signing time).
   */
  struct GNUNET_TIME_Absolute wallet_timestamp;

  /**
   * Interpreter state.
   */
  struct TALER_TESTING_Interpreter *is;

  /**
   * Task scheduled to try later.
   */
  struct GNUNET_SCHEDULER_Task *retry_task;

  /**
   * How long do we wait until we retry?
   */
  struct GNUNET_TIME_Relative backoff;

  /**
   * Expected HTTP response code.
   */
  unsigned int expected_response_code;

  /**
   * How often should we retry on (transient) failures?
   */
  unsigned int do_retry;

  /**
   * Set to #GNUNET_YES if the /deposit succeeded
   * and we now can provide the resulting traits.
   */
  int deposit_succeeded;

  /**
   * When did the exchange receive the deposit?
   */
  struct GNUNET_TIME_Absolute exchange_timestamp;

  /**
   * Signing key used by the exchange to sign the
   * deposit confirmation.
   */
  struct TALER_ExchangePublicKeyP exchange_pub;

  /**
   * Signature from the exchange on the
   * deposit confirmation.
   */
  struct TALER_ExchangeSignatureP exchange_sig;

  /**
   * Reference to previous deposit operation.
   * Only present if we're supposed to replay the previous deposit.
   */
  const char *deposit_reference;

  /**
   * Did we set the parameters for this deposit command?
   *
   * When we're referencing another deposit operation,
   * this will only be set after the command has been started.
   */
  int command_initialized;

  /**
   * Reference to fetch the merchant private key from.
   * If NULL, we generate our own, fresh merchant key.
   */
  const char *merchant_priv_reference;
};


/**
 * Run the command.
 *
 * @param cls closure.
 * @param cmd the command to execute.
 * @param is the interpreter state.
 */
static void
deposit_run (void *cls,
             const struct TALER_TESTING_Command *cmd,
             struct TALER_TESTING_Interpreter *is);


/**
 * Task scheduled to re-try #deposit_run.
 *
 * @param cls a `struct DepositState`
 */
static void
do_retry (void *cls)
{
  struct DepositState *ds = cls;

  ds->retry_task = NULL;
  ds->is->commands[ds->is->ip].last_req_time
    = GNUNET_TIME_absolute_get ();
  deposit_run (ds,
               NULL,
               ds->is);
}


/**
 * Callback to analyze the /deposit response, just used to
 * check if the response code is acceptable.
 *
 * @param cls closure.
 * @param hr HTTP response details
 * @param exchange_timestamp when did the exchange receive the deposit permission
 * @param exchange_sig signature provided by the exchange
 *        (NULL on errors)
 * @param exchange_pub public key of the exchange,
 *        used for signing the response.
 */
static void
deposit_cb (void *cls,
            const struct TALER_EXCHANGE_HttpResponse *hr,
            const struct GNUNET_TIME_Absolute exchange_timestamp,
            const struct TALER_ExchangeSignatureP *exchange_sig,
            const struct TALER_ExchangePublicKeyP *exchange_pub)
{
  struct DepositState *ds = cls;

  ds->dh = NULL;
  if (ds->expected_response_code != hr->http_status)
  {
    if (0 != ds->do_retry)
    {
      ds->do_retry--;
      if ( (0 == hr->http_status) ||
           (TALER_EC_GENERIC_DB_SOFT_FAILURE == hr->ec) ||
           (MHD_HTTP_INTERNAL_SERVER_ERROR == hr->http_status) )
      {
        GNUNET_log (GNUNET_ERROR_TYPE_INFO,
                    "Retrying deposit failed with %u/%d\n",
                    hr->http_status,
                    (int) hr->ec);
        /* on DB conflicts, do not use backoff */
        if (TALER_EC_GENERIC_DB_SOFT_FAILURE == hr->ec)
          ds->backoff = GNUNET_TIME_UNIT_ZERO;
        else
          ds->backoff = GNUNET_TIME_randomized_backoff (ds->backoff,
                                                        MAX_BACKOFF);
        ds->is->commands[ds->is->ip].num_tries++;
        ds->retry_task
          = GNUNET_SCHEDULER_add_delayed (ds->backoff,
                                          &do_retry,
                                          ds);
        return;
      }
    }
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Unexpected response code %u to command %s in %s:%u\n",
                hr->http_status,
                ds->is->commands[ds->is->ip].label,
                __FILE__,
                __LINE__);
    json_dumpf (hr->reply,
                stderr,
                0);
    TALER_TESTING_interpreter_fail (ds->is);
    return;
  }
  if (MHD_HTTP_OK == hr->http_status)
  {
    ds->deposit_succeeded = GNUNET_YES;
    ds->exchange_timestamp = exchange_timestamp;
    ds->exchange_pub = *exchange_pub;
    ds->exchange_sig = *exchange_sig;
  }
  TALER_TESTING_interpreter_next (ds->is);
}


/**
 * Run the command.
 *
 * @param cls closure.
 * @param cmd the command to execute.
 * @param is the interpreter state.
 */
static void
deposit_run (void *cls,
             const struct TALER_TESTING_Command *cmd,
             struct TALER_TESTING_Interpreter *is)
{
  struct DepositState *ds = cls;
  const struct TALER_TESTING_Command *coin_cmd;
  const struct TALER_CoinSpendPrivateKeyP *coin_priv;
  struct TALER_CoinSpendPublicKeyP coin_pub;
  const struct TALER_EXCHANGE_DenomPublicKey *denom_pub;
  const struct TALER_DenominationSignature *denom_pub_sig;
  struct TALER_CoinSpendSignatureP coin_sig;
  struct GNUNET_TIME_Absolute wire_deadline;
  struct TALER_MerchantPublicKeyP merchant_pub;
  struct GNUNET_HashCode h_contract_terms;

  (void) cmd;
  ds->is = is;
  if (NULL != ds->deposit_reference)
  {
    /* We're copying another deposit operation, initialize here. */
    const struct TALER_TESTING_Command *cmd;
    struct DepositState *ods;

    cmd = TALER_TESTING_interpreter_lookup_command (is,
                                                    ds->deposit_reference);
    if (NULL == cmd)
    {
      GNUNET_break (0);
      TALER_TESTING_interpreter_fail (is);
      return;
    }
    ods = cmd->cls;
    ds->coin_reference = ods->coin_reference;
    ds->coin_index = ods->coin_index;
    ds->wire_details = json_incref (ods->wire_details);
    ds->contract_terms = json_incref (ods->contract_terms);
    ds->wallet_timestamp = ods->wallet_timestamp;
    ds->refund_deadline = ods->refund_deadline;
    ds->amount = ods->amount;
    ds->merchant_priv = ods->merchant_priv;
    ds->command_initialized = GNUNET_YES;
  }
  else if (NULL != ds->merchant_priv_reference)
  {
    /* We're copying the merchant key from another deposit operation */
    const struct TALER_MerchantPrivateKeyP *merchant_priv;
    const struct TALER_TESTING_Command *cmd;

    cmd = TALER_TESTING_interpreter_lookup_command (is,
                                                    ds->merchant_priv_reference);
    if (NULL == cmd)
    {
      GNUNET_break (0);
      TALER_TESTING_interpreter_fail (is);
      return;
    }
    if ( (GNUNET_OK !=
          TALER_TESTING_get_trait_merchant_priv (cmd,
                                                 0,
                                                 &merchant_priv)) )
    {
      GNUNET_break (0);
      TALER_TESTING_interpreter_fail (is);
      return;
    }
    ds->merchant_priv = *merchant_priv;
  }
  GNUNET_assert (ds->coin_reference);
  coin_cmd = TALER_TESTING_interpreter_lookup_command (is,
                                                       ds->coin_reference);
  if (NULL == coin_cmd)
  {
    GNUNET_break (0);
    TALER_TESTING_interpreter_fail (is);
    return;
  }

  if ( (GNUNET_OK !=
        TALER_TESTING_get_trait_coin_priv (coin_cmd,
                                           ds->coin_index,
                                           &coin_priv)) ||
       (GNUNET_OK !=
        TALER_TESTING_get_trait_denom_pub (coin_cmd,
                                           ds->coin_index,
                                           &denom_pub)) ||
       (GNUNET_OK !=
        TALER_TESTING_get_trait_denom_sig (coin_cmd,
                                           ds->coin_index,
                                           &denom_pub_sig)) ||
       (GNUNET_OK !=
        TALER_JSON_contract_hash (ds->contract_terms,
                                  &h_contract_terms)) )
  {
    GNUNET_break (0);
    TALER_TESTING_interpreter_fail (is);
    return;
  }
  ds->deposit_fee = denom_pub->fee_deposit;
  GNUNET_CRYPTO_eddsa_key_get_public (&coin_priv->eddsa_priv,
                                      &coin_pub.eddsa_pub);

  if (0 != ds->refund_deadline.abs_value_us)
  {
    struct GNUNET_TIME_Relative refund_deadline;

    refund_deadline = GNUNET_TIME_absolute_get_remaining (ds->refund_deadline);
    wire_deadline = GNUNET_TIME_relative_to_absolute
                      (GNUNET_TIME_relative_multiply (refund_deadline, 2));
  }
  else
  {
    ds->refund_deadline = ds->wallet_timestamp;
    wire_deadline = GNUNET_TIME_relative_to_absolute (GNUNET_TIME_UNIT_ZERO);
  }
  GNUNET_CRYPTO_eddsa_key_get_public (&ds->merchant_priv.eddsa_priv,
                                      &merchant_pub.eddsa_pub);
  (void) GNUNET_TIME_round_abs (&wire_deadline);
  {
    struct GNUNET_HashCode h_wire;

    GNUNET_assert (GNUNET_OK ==
                   TALER_JSON_merchant_wire_signature_hash (ds->wire_details,
                                                            &h_wire));
    TALER_EXCHANGE_deposit_permission_sign (&ds->amount,
                                            &denom_pub->fee_deposit,
                                            &h_wire,
                                            &h_contract_terms,
                                            &denom_pub->h_key,
                                            coin_priv,
                                            ds->wallet_timestamp,
                                            &merchant_pub,
                                            ds->refund_deadline,
                                            &coin_sig);
  }
  ds->dh = TALER_EXCHANGE_deposit (is->exchange,
                                   &ds->amount,
                                   wire_deadline,
                                   ds->wire_details,
                                   &h_contract_terms,
                                   &coin_pub,
                                   denom_pub_sig,
                                   &denom_pub->key,
                                   ds->wallet_timestamp,
                                   &merchant_pub,
                                   ds->refund_deadline,
                                   &coin_sig,
                                   &deposit_cb,
                                   ds);
  if (NULL == ds->dh)
  {
    GNUNET_break (0);
    TALER_TESTING_interpreter_fail (is);
    return;
  }
}


/**
 * Free the state of a "deposit" CMD, and possibly cancel a
 * pending operation thereof.
 *
 * @param cls closure, must be a `struct DepositState`.
 * @param cmd the command which is being cleaned up.
 */
static void
deposit_cleanup (void *cls,
                 const struct TALER_TESTING_Command *cmd)
{
  struct DepositState *ds = cls;

  if (NULL != ds->dh)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                "Command %u (%s) did not complete\n",
                ds->is->ip,
                cmd->label);
    TALER_EXCHANGE_deposit_cancel (ds->dh);
    ds->dh = NULL;
  }
  if (NULL != ds->retry_task)
  {
    GNUNET_SCHEDULER_cancel (ds->retry_task);
    ds->retry_task = NULL;
  }
  json_decref (ds->wire_details);
  json_decref (ds->contract_terms);
  GNUNET_free (ds);
}


/**
 * Offer internal data from a "deposit" CMD, to other commands.
 *
 * @param cls closure.
 * @param[out] ret result.
 * @param trait name of the trait.
 * @param index index number of the object to offer.
 *
 * @return #GNUNET_OK on success.
 */
static int
deposit_traits (void *cls,
                const void **ret,
                const char *trait,
                unsigned int index)
{
  struct DepositState *ds = cls;
  const struct TALER_TESTING_Command *coin_cmd;
  /* Will point to coin cmd internals. */
  const struct TALER_CoinSpendPrivateKeyP *coin_spent_priv;

  if (GNUNET_YES != ds->command_initialized)
  {
    /* No access to traits yet. */
    GNUNET_break (0);
    return GNUNET_NO;
  }

  coin_cmd
    = TALER_TESTING_interpreter_lookup_command (ds->is,
                                                ds->coin_reference);
  if (NULL == coin_cmd)
  {
    GNUNET_break (0);
    TALER_TESTING_interpreter_fail (ds->is);
    return GNUNET_NO;
  }
  if (GNUNET_OK !=
      TALER_TESTING_get_trait_coin_priv (coin_cmd,
                                         ds->coin_index,
                                         &coin_spent_priv))
  {
    GNUNET_break (0);
    TALER_TESTING_interpreter_fail (ds->is);
    return GNUNET_NO;
  }
  {
    struct TALER_TESTING_Trait traits[] = {
      /* First two traits are only available if
         ds->traits is #GNUNET_YES */
      TALER_TESTING_make_trait_exchange_pub (0,
                                             &ds->exchange_pub),
      TALER_TESTING_make_trait_exchange_sig (0,
                                             &ds->exchange_sig),
      /* These traits are always available */
      TALER_TESTING_make_trait_coin_priv (0,
                                          coin_spent_priv),
      TALER_TESTING_make_trait_wire_details (0,
                                             ds->wire_details),
      TALER_TESTING_make_trait_contract_terms (0,
                                               ds->contract_terms),
      TALER_TESTING_make_trait_merchant_priv (0,
                                              &ds->merchant_priv),
      TALER_TESTING_make_trait_amount_obj (
        TALER_TESTING_CMD_DEPOSIT_TRAIT_IDX_DEPOSIT_VALUE,
        &ds->amount),
      TALER_TESTING_make_trait_amount_obj (
        TALER_TESTING_CMD_DEPOSIT_TRAIT_IDX_DEPOSIT_FEE,
        &ds->deposit_fee),
      TALER_TESTING_make_trait_absolute_time (0,
                                              &ds->exchange_timestamp),
      TALER_TESTING_trait_end ()
    };

    return TALER_TESTING_get_trait ((ds->deposit_succeeded)
                                    ? traits
                                    : &traits[2],
                                    ret,
                                    trait,
                                    index);
  }
}


/**
 * Create a "deposit" command.
 *
 * @param label command label.
 * @param coin_reference reference to any operation that can
 *        provide a coin.
 * @param coin_index if @a withdraw_reference offers an array of
 *        coins, this parameter selects which one in that array.
 *        This value is currently ignored, as only one-coin
 *        withdrawals are implemented.
 * @param target_account_payto target account for the "deposit"
 *        request.
 * @param contract_terms contract terms to be signed over by the
 *        coin.
 * @param refund_deadline refund deadline, zero means 'no refunds'.
 *        Note, if time were absolute, then it would have come
 *        one day and disrupt tests meaning.
 * @param amount how much is going to be deposited.
 * @param expected_response_code expected HTTP response code.
 *
 * @return the command.
 */
struct TALER_TESTING_Command
TALER_TESTING_cmd_deposit (const char *label,
                           const char *coin_reference,
                           unsigned int coin_index,
                           const char *target_account_payto,
                           const char *contract_terms,
                           struct GNUNET_TIME_Relative refund_deadline,
                           const char *amount,
                           unsigned int expected_response_code)
{
  struct DepositState *ds;
  json_t *wire_details;

  wire_details = TALER_TESTING_make_wire_details (target_account_payto);
  ds = GNUNET_new (struct DepositState);
  ds->coin_reference = coin_reference;
  ds->coin_index = coin_index;
  ds->wire_details = wire_details;
  ds->contract_terms = json_loads (contract_terms,
                                   JSON_REJECT_DUPLICATES,
                                   NULL);
  GNUNET_CRYPTO_eddsa_key_create (&ds->merchant_priv.eddsa_priv);
  if (NULL == ds->contract_terms)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Failed to parse contract terms `%s' for CMD `%s'\n",
                contract_terms,
                label);
    GNUNET_assert (0);
  }
  ds->wallet_timestamp = GNUNET_TIME_absolute_get ();
  (void) GNUNET_TIME_round_abs (&ds->wallet_timestamp);

  json_object_set_new (ds->contract_terms,
                       "timestamp",
                       GNUNET_JSON_from_time_abs (ds->wallet_timestamp));
  if (0 != refund_deadline.rel_value_us)
  {
    ds->refund_deadline = GNUNET_TIME_relative_to_absolute (refund_deadline);
    (void) GNUNET_TIME_round_abs (&ds->refund_deadline);
    json_object_set_new (ds->contract_terms,
                         "refund_deadline",
                         GNUNET_JSON_from_time_abs (ds->refund_deadline));
  }
  GNUNET_assert (GNUNET_OK ==
                 TALER_string_to_amount (amount,
                                         &ds->amount));
  ds->expected_response_code = expected_response_code;
  ds->command_initialized = GNUNET_YES;
  {
    struct TALER_TESTING_Command cmd = {
      .cls = ds,
      .label = label,
      .run = &deposit_run,
      .cleanup = &deposit_cleanup,
      .traits = &deposit_traits
    };

    return cmd;
  }
}


/**
 * Create a "deposit" command that references an existing merchant key.
 *
 * @param label command label.
 * @param coin_reference reference to any operation that can
 *        provide a coin.
 * @param coin_index if @a withdraw_reference offers an array of
 *        coins, this parameter selects which one in that array.
 *        This value is currently ignored, as only one-coin
 *        withdrawals are implemented.
 * @param target_account_payto target account for the "deposit"
 *        request.
 * @param contract_terms contract terms to be signed over by the
 *        coin.
 * @param refund_deadline refund deadline, zero means 'no refunds'.
 *        Note, if time were absolute, then it would have come
 *        one day and disrupt tests meaning.
 * @param amount how much is going to be deposited.
 * @param expected_response_code expected HTTP response code.
 * @param merchant_priv_reference reference to another operation
 *        that has a merchant private key trait
 *
 * @return the command.
 */
struct TALER_TESTING_Command
TALER_TESTING_cmd_deposit_with_ref (const char *label,
                                    const char *coin_reference,
                                    unsigned int coin_index,
                                    const char *target_account_payto,
                                    const char *contract_terms,
                                    struct GNUNET_TIME_Relative refund_deadline,
                                    const char *amount,
                                    unsigned int expected_response_code,
                                    const char *merchant_priv_reference)
{
  struct DepositState *ds;
  json_t *wire_details;

  wire_details = TALER_TESTING_make_wire_details (target_account_payto);
  ds = GNUNET_new (struct DepositState);
  ds->merchant_priv_reference = merchant_priv_reference;
  ds->coin_reference = coin_reference;
  ds->coin_index = coin_index;
  ds->wire_details = wire_details;
  ds->contract_terms = json_loads (contract_terms,
                                   JSON_REJECT_DUPLICATES,
                                   NULL);
  if (NULL == ds->contract_terms)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Failed to parse contract terms `%s' for CMD `%s'\n",
                contract_terms,
                label);
    GNUNET_assert (0);
  }
  ds->wallet_timestamp = GNUNET_TIME_absolute_get ();
  (void) GNUNET_TIME_round_abs (&ds->wallet_timestamp);

  json_object_set_new (ds->contract_terms,
                       "timestamp",
                       GNUNET_JSON_from_time_abs (ds->wallet_timestamp));
  if (0 != refund_deadline.rel_value_us)
  {
    ds->refund_deadline = GNUNET_TIME_relative_to_absolute (refund_deadline);
    (void) GNUNET_TIME_round_abs (&ds->refund_deadline);
    json_object_set_new (ds->contract_terms,
                         "refund_deadline",
                         GNUNET_JSON_from_time_abs (ds->refund_deadline));
  }
  GNUNET_assert (GNUNET_OK ==
                 TALER_string_to_amount (amount,
                                         &ds->amount));
  ds->expected_response_code = expected_response_code;
  ds->command_initialized = GNUNET_YES;
  {
    struct TALER_TESTING_Command cmd = {
      .cls = ds,
      .label = label,
      .run = &deposit_run,
      .cleanup = &deposit_cleanup,
      .traits = &deposit_traits
    };

    return cmd;
  }
}


/**
 * Create a "deposit" command that repeats an existing
 * deposit command.
 *
 * @param label command label.
 * @param deposit_reference which deposit command should we repeat
 * @param expected_response_code expected HTTP response code.
 * @return the command.
 */
struct TALER_TESTING_Command
TALER_TESTING_cmd_deposit_replay (const char *label,
                                  const char *deposit_reference,
                                  unsigned int expected_response_code)
{
  struct DepositState *ds;

  ds = GNUNET_new (struct DepositState);
  ds->deposit_reference = deposit_reference;
  ds->expected_response_code = expected_response_code;
  {
    struct TALER_TESTING_Command cmd = {
      .cls = ds,
      .label = label,
      .run = &deposit_run,
      .cleanup = &deposit_cleanup,
      .traits = &deposit_traits
    };

    return cmd;
  }
}


/**
 * Modify a deposit command to enable retries when we get transient
 * errors from the exchange.
 *
 * @param cmd a deposit command
 * @return the command with retries enabled
 */
struct TALER_TESTING_Command
TALER_TESTING_cmd_deposit_with_retry (struct TALER_TESTING_Command cmd)
{
  struct DepositState *ds;

  GNUNET_assert (&deposit_run == cmd.run);
  ds = cmd.cls;
  ds->do_retry = NUM_RETRIES;
  return cmd;
}


/* end of testing_api_cmd_deposit.c */
