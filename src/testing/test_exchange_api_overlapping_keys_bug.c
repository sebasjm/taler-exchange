/*
  This file is part of TALER
  Copyright (C) 2018, 2019 Taler Systems SA

  TALER is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published
  by the Free Software Foundation; either version 3, or (at your
  option) any later version.

  TALER is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public
  License along with TALER; see the file COPYING.  If not, see
  <http://www.gnu.org/licenses/>
*/

/**
 * @file testing/test_exchange_api_overlapping_keys_bug.c
 * @brief testcase to test exchange's /keys cherry picking ability and
 *          other /keys related operations
 * @author Marcello Stanisci
 * @author Christian Grothoff
 */

#include "platform.h"
#include "taler_util.h"
#include "taler_signatures.h"
#include "taler_exchange_service.h"
#include "taler_json_lib.h"
#include <gnunet/gnunet_util_lib.h>
#include <microhttpd.h>
#include "taler_bank_service.h"
#include "taler_fakebank_lib.h"
#include "taler_testing_lib.h"

/**
 * Configuration file we use.  One (big) configuration is used
 * for the various components for this test.
 */
#define CONFIG_FILE "test_exchange_api_keys_cherry_picking.conf"

/**
 * Exchange configuration data.
 */
static struct TALER_TESTING_ExchangeConfiguration ec;


/**
 * Main function that will tell the interpreter what commands to
 * run.
 *
 * @param cls closure
 */
static void
run (void *cls,
     struct TALER_TESTING_Interpreter *is)
{
  struct TALER_TESTING_Command commands[] = {
    TALER_TESTING_cmd_auditor_add ("add-auditor-OK",
                                   MHD_HTTP_NO_CONTENT,
                                   false),
    TALER_TESTING_cmd_wire_add ("add-wire-account",
                                "payto://x-taler-bank/localhost/2",
                                MHD_HTTP_NO_CONTENT,
                                false),
    TALER_TESTING_cmd_exec_offline_sign_keys ("offline-sign-future-keys",
                                              CONFIG_FILE),
    TALER_TESTING_cmd_check_keys_pull_all_keys ("refetch /keys",
                                                1),
    TALER_TESTING_cmd_check_keys ("first-download",
                                  1),
    /* Causes GET /keys?last_denom_issue=0 */
    TALER_TESTING_cmd_check_keys_with_last_denom ("second-download",
                                                  1,
                                                  "zero"),
    TALER_TESTING_cmd_end ()
  };

  TALER_TESTING_run (is,
                     commands);
}


int
main (int argc,
      char *const *argv)
{
  /* These environment variables get in the way... */
  unsetenv ("XDG_DATA_HOME");
  unsetenv ("XDG_CONFIG_HOME");
  GNUNET_log_setup ("test-exchange-api-overlapping-keys-bug",
                    "DEBUG", NULL);
  TALER_TESTING_cleanup_files (CONFIG_FILE);
  /* @helpers.  Run keyup, create tables, ... Note: it
   * fetches the port number from config in order to see
   * if it's available. */
  switch (TALER_TESTING_prepare_exchange (CONFIG_FILE,
                                          GNUNET_YES,
                                          &ec))
  {
  case GNUNET_SYSERR:
    GNUNET_break (0);
    return 1;
  case GNUNET_NO:
    return 77;
  case GNUNET_OK:
    if (GNUNET_OK !=
        /* Set up event loop and reschedule context, plus
         * start/stop the exchange.  It calls TALER_TESTING_setup
         * which creates the 'is' object.
         */
        TALER_TESTING_setup_with_exchange (&run,
                                           NULL,
                                           CONFIG_FILE))
      return 1;
    break;
  default:
    GNUNET_break (0);
    return 1;
  }
  return 0;
}


/* end of test_exchange_api_overlapping_keys_bug.c */
