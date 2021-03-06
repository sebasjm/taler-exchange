/*
  This file is part of TALER
  Copyright (C) 2016-2020 Taler Systems SA

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
 * @file taler-exchange-closer.c
 * @brief Process that closes expired reserves
 * @author Christian Grothoff
 */
#include "platform.h"
#include <gnunet/gnunet_util_lib.h>
#include <jansson.h>
#include <pthread.h>
#include "taler_exchangedb_lib.h"
#include "taler_exchangedb_plugin.h"
#include "taler_json_lib.h"
#include "taler_bank_service.h"


/**
 * What is the smallest unit we support for wire transfers?
 * We will need to round down to a multiple of this amount.
 */
static struct TALER_Amount currency_round_unit;

/**
 * What is the base URL of this exchange?  Used in the
 * wire transfer subjects to that merchants and governments
 * can ask for the list of aggregated deposits.
 */
static char *exchange_base_url;

/**
 * The exchange's configuration.
 */
static const struct GNUNET_CONFIGURATION_Handle *cfg;

/**
 * Our database plugin.
 */
static struct TALER_EXCHANGEDB_Plugin *db_plugin;

/**
 * Next task to run, if any.
 */
static struct GNUNET_SCHEDULER_Task *task;

/**
 * How long should we sleep when idle before trying to find more work?
 */
static struct GNUNET_TIME_Relative aggregator_idle_sleep_interval;

/**
 * Value to return from main(). 0 on success, non-zero
 * on serious errors.
 */
static enum
{
  GR_SUCCESS = 0,
  GR_WIRE_ACCOUNT_NOT_CONFIGURED = 1,
  GR_WIRE_TRANSFER_FEES_NOT_CONFIGURED = 2,
  GR_FAILURE_TO_ROUND_AMOUNT = 3,
  GR_DATABASE_INSERT_HARD_FAIL = 4,
  GR_DATABASE_SELECT_HARD_FAIL = 5,
  GR_DATABASE_COMMIT_HARD_FAIL = 6,
  GR_DATABASE_SESSION_START_FAIL = 7,
  GR_DATABASE_TRANSACTION_BEGIN_FAIL = 8,
  GR_CONFIGURATION_INVALID = 9,
  GR_CMD_LINE_UTF8_ERROR = 10,
  GR_CMD_LINE_OPTIONS_WRONG = 11,
  GR_INVALID_PAYTO_ENCOUNTERED = 12,
} global_ret;

/**
 * #GNUNET_YES if we are in test mode and should exit when idle.
 */
static int test_mode;


/**
 * Main work function that finds and triggers transfers for reserves
 * closures.
 *
 * @param cls closure
 */
static void
run_reserve_closures (void *cls);


/**
 * We're being aborted with CTRL-C (or SIGTERM). Shut down.
 *
 * @param cls closure
 */
static void
shutdown_task (void *cls)
{
  (void) cls;
  GNUNET_log (GNUNET_ERROR_TYPE_INFO,
              "Running shutdown\n");
  if (NULL != task)
  {
    GNUNET_SCHEDULER_cancel (task);
    task = NULL;
  }
  TALER_EXCHANGEDB_plugin_unload (db_plugin);
  db_plugin = NULL;
  TALER_EXCHANGEDB_unload_accounts ();
  cfg = NULL;
}


/**
 * Parse the configuration for wirewatch.
 *
 * @return #GNUNET_OK on success
 */
static int
parse_wirewatch_config (void)
{
  if (GNUNET_OK !=
      GNUNET_CONFIGURATION_get_value_string (cfg,
                                             "exchange",
                                             "BASE_URL",
                                             &exchange_base_url))
  {
    GNUNET_log_config_missing (GNUNET_ERROR_TYPE_ERROR,
                               "exchange",
                               "BASE_URL");
    return GNUNET_SYSERR;
  }
  if (GNUNET_OK !=
      GNUNET_CONFIGURATION_get_value_time (cfg,
                                           "exchange",
                                           "AGGREGATOR_IDLE_SLEEP_INTERVAL",
                                           &aggregator_idle_sleep_interval))
  {
    GNUNET_log_config_missing (GNUNET_ERROR_TYPE_ERROR,
                               "exchange",
                               "AGGREGATOR_IDLE_SLEEP_INTERVAL");
    return GNUNET_SYSERR;
  }
  if ( (GNUNET_OK !=
        TALER_config_get_amount (cfg,
                                 "taler",
                                 "CURRENCY_ROUND_UNIT",
                                 &currency_round_unit)) ||
       ( (0 != currency_round_unit.fraction) &&
         (0 != currency_round_unit.value) ) )
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Need non-zero amount in section `TALER' under `CURRENCY_ROUND_UNIT'\n");
    return GNUNET_SYSERR;
  }

  if (NULL ==
      (db_plugin = TALER_EXCHANGEDB_plugin_load (cfg)))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Failed to initialize DB subsystem\n");
    return GNUNET_SYSERR;
  }
  if (GNUNET_OK !=
      TALER_EXCHANGEDB_load_accounts (cfg))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "No wire accounts configured for debit!\n");
    TALER_EXCHANGEDB_plugin_unload (db_plugin);
    db_plugin = NULL;
    return GNUNET_SYSERR;
  }
  return GNUNET_OK;
}


/**
 * Perform a database commit. If it fails, print a warning.
 *
 * @param session session to perform the commit for.
 * @return status of commit
 */
static enum GNUNET_DB_QueryStatus
commit_or_warn (struct TALER_EXCHANGEDB_Session *session)
{
  enum GNUNET_DB_QueryStatus qs;

  qs = db_plugin->commit (db_plugin->cls,
                          session);
  if (GNUNET_DB_STATUS_SUCCESS_NO_RESULTS == qs)
    return qs;
  GNUNET_log ((GNUNET_DB_STATUS_SOFT_ERROR == qs)
              ? GNUNET_ERROR_TYPE_INFO
              : GNUNET_ERROR_TYPE_ERROR,
              "Failed to commit database transaction!\n");
  return qs;
}


/**
 * Closure for #expired_reserve_cb().
 */
struct ExpiredReserveContext
{

  /**
   * Database session we are using.
   */
  struct TALER_EXCHANGEDB_Session *session;

};


/**
 * Function called with details about expired reserves.
 * We trigger the reserve closure by inserting the respective
 * closing record and prewire instructions into the respective
 * tables.
 *
 * @param cls a `struct ExpiredReserveContext *`
 * @param reserve_pub public key of the reserve
 * @param left amount left in the reserve
 * @param account_payto_uri information about the bank account that initially
 *        caused the reserve to be created
 * @param expiration_date when did the reserve expire
 * @return transaction status code
 */
static enum GNUNET_DB_QueryStatus
expired_reserve_cb (void *cls,
                    const struct TALER_ReservePublicKeyP *reserve_pub,
                    const struct TALER_Amount *left,
                    const char *account_payto_uri,
                    struct GNUNET_TIME_Absolute expiration_date)
{
  struct ExpiredReserveContext *erc = cls;
  struct TALER_EXCHANGEDB_Session *session = erc->session;
  struct GNUNET_TIME_Absolute now;
  struct TALER_WireTransferIdentifierRawP wtid;
  struct TALER_Amount amount_without_fee;
  struct TALER_Amount closing_fee;
  int ret;
  enum GNUNET_DB_QueryStatus qs;
  struct TALER_EXCHANGEDB_WireAccount *wa;

  /* NOTE: potential optimization: use custom SQL API to not
     fetch this: */
  GNUNET_log (GNUNET_ERROR_TYPE_INFO,
              "Processing reserve closure at %s\n",
              GNUNET_STRINGS_absolute_time_to_string (expiration_date));
  now = GNUNET_TIME_absolute_get ();
  (void) GNUNET_TIME_round_abs (&now);

  /* lookup account we should use */
  wa = TALER_EXCHANGEDB_find_account_by_payto_uri (account_payto_uri);
  if (NULL == wa)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "No wire account configured to deal with target URI `%s'\n",
                account_payto_uri);
    global_ret = GR_WIRE_ACCOUNT_NOT_CONFIGURED;
    GNUNET_SCHEDULER_shutdown ();
    return GNUNET_DB_STATUS_HARD_ERROR;
  }

  /* lookup `closing_fee` from time of actual reserve expiration
     (we may be lagging behind!) */
  {
    struct TALER_Amount wire_fee;
    struct GNUNET_TIME_Absolute start_date;
    struct GNUNET_TIME_Absolute end_date;
    struct TALER_MasterSignatureP master_sig;
    enum GNUNET_DB_QueryStatus qs;

    qs = db_plugin->get_wire_fee (db_plugin->cls,
                                  session,
                                  wa->method,
                                  expiration_date,
                                  &start_date,
                                  &end_date,
                                  &wire_fee,
                                  &closing_fee,
                                  &master_sig);
    if (0 >= qs)
    {
      GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                  "Could not get wire fees for %s at %s. Aborting run.\n",
                  wa->method,
                  GNUNET_STRINGS_absolute_time_to_string (expiration_date));
      return GNUNET_DB_STATUS_HARD_ERROR;
    }
  }

  /* calculate transfer amount */
  ret = TALER_amount_subtract (&amount_without_fee,
                               left,
                               &closing_fee);
  if ( (GNUNET_SYSERR == ret) ||
       (GNUNET_NO == ret) )
  {
    /* Closing fee higher than or equal to remaining balance, close
       without wire transfer. */
    closing_fee = *left;
    GNUNET_assert (GNUNET_OK ==
                   TALER_amount_get_zero (left->currency,
                                          &amount_without_fee));
  }
  /* round down to enable transfer */
  if (GNUNET_SYSERR ==
      TALER_amount_round_down (&amount_without_fee,
                               &currency_round_unit))
  {
    GNUNET_break (0);
    global_ret = GR_FAILURE_TO_ROUND_AMOUNT;
    GNUNET_SCHEDULER_shutdown ();
    return GNUNET_DB_STATUS_HARD_ERROR;
  }
  if ( (0 == amount_without_fee.value) &&
       (0 == amount_without_fee.fraction) )
    ret = GNUNET_NO;

  /* NOTE: sizeof (*reserve_pub) == sizeof (wtid) right now, but to
     be future-compatible, we use the memset + min construction */
  memset (&wtid,
          0,
          sizeof (wtid));
  memcpy (&wtid,
          reserve_pub,
          GNUNET_MIN (sizeof (wtid),
                      sizeof (*reserve_pub)));
  if (GNUNET_SYSERR != ret)
    qs = db_plugin->insert_reserve_closed (db_plugin->cls,
                                           session,
                                           reserve_pub,
                                           now,
                                           account_payto_uri,
                                           &wtid,
                                           left,
                                           &closing_fee);
  else
    qs = GNUNET_DB_STATUS_HARD_ERROR;
  GNUNET_log (GNUNET_ERROR_TYPE_INFO,
              "Closing reserve %s over %s (%d, %d)\n",
              TALER_B2S (reserve_pub),
              TALER_amount2s (left),
              ret,
              qs);
  /* Check for hard failure */
  if ( (GNUNET_SYSERR == ret) ||
       (GNUNET_DB_STATUS_HARD_ERROR == qs) )
  {
    GNUNET_break (0);
    global_ret = GR_DATABASE_INSERT_HARD_FAIL;
    GNUNET_SCHEDULER_shutdown ();
    return GNUNET_DB_STATUS_HARD_ERROR;
  }
  if ( (GNUNET_OK != ret) ||
       (GNUNET_DB_STATUS_SUCCESS_ONE_RESULT != qs) )
  {
    /* Reserve balance was almost zero OR soft error */
    GNUNET_log (GNUNET_ERROR_TYPE_INFO,
                "Reserve was virtually empty, moving on\n");
    (void) commit_or_warn (session);
    return qs;
  }

  /* success, perform wire transfer */
  {
    void *buf;
    size_t buf_size;

    TALER_BANK_prepare_transfer (account_payto_uri,
                                 &amount_without_fee,
                                 exchange_base_url,
                                 &wtid,
                                 &buf,
                                 &buf_size);
    /* Commit our intention to execute the wire transfer! */
    qs = db_plugin->wire_prepare_data_insert (db_plugin->cls,
                                              session,
                                              wa->method,
                                              buf,
                                              buf_size);
    GNUNET_free (buf);
  }
  if (GNUNET_DB_STATUS_HARD_ERROR == qs)
  {
    GNUNET_break (0);
    global_ret = GR_DATABASE_INSERT_HARD_FAIL;
    GNUNET_SCHEDULER_shutdown ();
    return GNUNET_DB_STATUS_HARD_ERROR;
  }
  if (GNUNET_DB_STATUS_SOFT_ERROR == qs)
  {
    /* start again */
    return GNUNET_DB_STATUS_SUCCESS_NO_RESULTS;
  }
  return GNUNET_DB_STATUS_SUCCESS_ONE_RESULT;
}


/**
 * Main work function that finds and triggers transfers for reserves
 * closures.
 *
 * @param cls closure
 */
static void
run_reserve_closures (void *cls)
{
  struct TALER_EXCHANGEDB_Session *session;
  enum GNUNET_DB_QueryStatus qs;
  struct ExpiredReserveContext erc;
  struct GNUNET_TIME_Absolute now;

  (void) cls;
  task = NULL;
  if (NULL == (session = db_plugin->get_session (db_plugin->cls)))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Failed to obtain database session!\n");
    global_ret = GR_DATABASE_SESSION_START_FAIL;
    GNUNET_SCHEDULER_shutdown ();
    return;
  }

  if (GNUNET_OK !=
      db_plugin->start (db_plugin->cls,
                        session,
                        "aggregator reserve closures"))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Failed to start database transaction!\n");
    global_ret = GR_DATABASE_TRANSACTION_BEGIN_FAIL;
    GNUNET_SCHEDULER_shutdown ();
    return;
  }
  erc.session = session;
  now = GNUNET_TIME_absolute_get ();
  (void) GNUNET_TIME_round_abs (&now);
  GNUNET_log (GNUNET_ERROR_TYPE_INFO,
              "Checking for reserves to close by date %s\n",
              GNUNET_STRINGS_absolute_time_to_string (now));
  qs = db_plugin->get_expired_reserves (db_plugin->cls,
                                        session,
                                        now,
                                        &expired_reserve_cb,
                                        &erc);
  GNUNET_assert (1 >= qs);
  switch (qs)
  {
  case GNUNET_DB_STATUS_HARD_ERROR:
    GNUNET_break (0);
    db_plugin->rollback (db_plugin->cls,
                         session);
    global_ret = GR_DATABASE_SELECT_HARD_FAIL;
    GNUNET_SCHEDULER_shutdown ();
    return;
  case GNUNET_DB_STATUS_SOFT_ERROR:
    db_plugin->rollback (db_plugin->cls,
                         session);
    GNUNET_assert (NULL == task);
    task = GNUNET_SCHEDULER_add_now (&run_reserve_closures,
                                     NULL);
    return;
  case GNUNET_DB_STATUS_SUCCESS_NO_RESULTS:
    GNUNET_log (GNUNET_ERROR_TYPE_INFO,
                "No more idle reserves to close, going to sleep.\n");
    db_plugin->rollback (db_plugin->cls,
                         session);
    GNUNET_assert (NULL == task);
    if (GNUNET_YES == test_mode)
    {
      GNUNET_SCHEDULER_shutdown ();
    }
    else
    {
      task = GNUNET_SCHEDULER_add_delayed (aggregator_idle_sleep_interval,
                                           &run_reserve_closures,
                                           NULL);
    }
    return;
  case GNUNET_DB_STATUS_SUCCESS_ONE_RESULT:
    (void) commit_or_warn (session);
    GNUNET_assert (NULL == task);
    task = GNUNET_SCHEDULER_add_now (&run_reserve_closures,
                                     NULL);
    return;
  }
}


/**
 * First task.  Parses the configuration and starts the
 * main loop of #run_reserve_closures(). Also schedules
 * the #shutdown_task() to clean up.
 *
 * @param cls closure, NULL
 * @param args remaining command-line arguments
 * @param cfgfile name of the configuration file used (for saving, can be NULL!)
 * @param c configuration
 */
static void
run (void *cls,
     char *const *args,
     const char *cfgfile,
     const struct GNUNET_CONFIGURATION_Handle *c)
{
  (void) cls;
  (void) args;
  (void) cfgfile;

  cfg = c;
  if (GNUNET_OK != parse_wirewatch_config ())
  {
    cfg = NULL;
    global_ret = GR_CONFIGURATION_INVALID;
    return;
  }
  GNUNET_assert (NULL == task);
  task = GNUNET_SCHEDULER_add_now (&run_reserve_closures,
                                   NULL);
  GNUNET_SCHEDULER_add_shutdown (&shutdown_task,
                                 cls);
}


/**
 * The main function of the taler-exchange-closer.
 *
 * @param argc number of arguments from the command line
 * @param argv command line arguments
 * @return 0 ok, non-zero on error
 */
int
main (int argc,
      char *const *argv)
{
  struct GNUNET_GETOPT_CommandLineOption options[] = {
    GNUNET_GETOPT_option_timetravel ('T',
                                     "timetravel"),
    GNUNET_GETOPT_option_flag ('t',
                               "test",
                               "run in test mode and exit when idle",
                               &test_mode),
    GNUNET_GETOPT_OPTION_END
  };
  enum GNUNET_GenericReturnValue ret;

  if (GNUNET_OK !=
      GNUNET_STRINGS_get_utf8_args (argc, argv,
                                    &argc, &argv))
    return GR_CMD_LINE_UTF8_ERROR;
  ret = GNUNET_PROGRAM_run (
    argc, argv,
    "taler-exchange-closer",
    gettext_noop ("background process that closes expired reserves"),
    options,
    &run, NULL);
  GNUNET_free_nz ((void *) argv);
  if (GNUNET_SYSERR == ret)
    return GR_CMD_LINE_OPTIONS_WRONG;
  if (GNUNET_NO == ret)
    return 0;
  return global_ret;
}


/* end of taler-exchange-closer.c */
