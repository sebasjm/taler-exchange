/*
  This file is part of TALER
  Copyright (C) 2014, 2015, 2016 Taler Systems SA

  TALER is free software; you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation; either version 3, or (at your option) any later version.

  TALER is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
  A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along with
  TALER; see the file COPYING.  If not, see <http://www.gnu.org/licenses/>
*/
/**
 * @file pq/pq_result_helper.c
 * @brief functions to initialize parameter arrays
 * @author Christian Grothoff
 */
#include "platform.h"
#include <gnunet/gnunet_util_lib.h>
#include "taler_pq_lib.h"


/**
 * Extract a currency amount from a query result according to the
 * given specification.
 *
 * @param result the result to extract the amount from
 * @param row which row of the result to extract the amount from (needed as results can have multiple rows)
 * @param currency currency to use for @a r_amount_nbo
 * @param val_name name of the column with the amount's "value", must include the substring "_val".
 * @param frac_name name of the column with the amount's "fractional" value, must include the substring "_frac".
 * @param[out] r_amount_nbo where to store the amount, in network byte order
 * @return
 *   #GNUNET_YES if all results could be extracted
 *   #GNUNET_NO if at least one result was NULL
 *   #GNUNET_SYSERR if a result was invalid (non-existing field)
 */
static int
extract_amount_nbo_helper (PGresult *result,
                           int row,
                           const char *currency,
                           const char *val_name,
                           const char *frac_name,
                           struct TALER_AmountNBO *r_amount_nbo)
{
  int val_num;
  int frac_num;
  int len;

  /* These checks are simply to check that clients obey by our naming
     conventions, and not for any functional reason */
  GNUNET_assert (NULL !=
                 strstr (val_name,
                         "_val"));
  GNUNET_assert (NULL !=
                 strstr (frac_name,
                         "_frac"));
  /* Set return value to invalid in case we don't finish */
  memset (r_amount_nbo,
          0,
          sizeof (struct TALER_AmountNBO));
  val_num = PQfnumber (result,
                       val_name);
  frac_num = PQfnumber (result,
                        frac_name);
  if (val_num < 0)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Field `%s' does not exist in result\n",
                val_name);
    return GNUNET_SYSERR;
  }
  if (frac_num < 0)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Field `%s' does not exist in result\n",
                frac_name);
    return GNUNET_SYSERR;
  }
  if ( (PQgetisnull (result,
                     row,
                     val_num)) ||
       (PQgetisnull (result,
                     row,
                     frac_num)) )
  {
    GNUNET_break (0);
    return GNUNET_NO;
  }
  /* Note that Postgres stores value in NBO internally,
     so no conversion needed in this case */
  r_amount_nbo->value = *(uint64_t *) PQgetvalue (result,
                                                  row,
                                                  val_num);
  r_amount_nbo->fraction = *(uint32_t *) PQgetvalue (result,
                                                     row,
                                                     frac_num);
  len = GNUNET_MIN (TALER_CURRENCY_LEN - 1,
                    strlen (currency));
  memcpy (r_amount_nbo->currency,
          currency,
          len);
  return GNUNET_OK;
}


/**
 * Extract data from a Postgres database @a result at row @a row.
 *
 * @param cls closure, a `const char *` giving the currency
 * @param result where to extract data from
 * @param row row to extract data from
 * @param fname name (or prefix) of the fields to extract from
 * @param[in,out] dst_size where to store size of result, may be NULL
 * @param[out] dst where to store the result
 * @return
 *   #GNUNET_YES if all results could be extracted
 *   #GNUNET_NO if at least one result was NULL
 *   #GNUNET_SYSERR if a result was invalid (non-existing field)
 */
static int
extract_amount_nbo (void *cls,
                    PGresult *result,
                    int row,
                    const char *fname,
                    size_t *dst_size,
                    void *dst)
{
  const char *currency = cls;
  char *val_name;
  char *frac_name;
  int ret;

  if (sizeof (struct TALER_AmountNBO) != *dst_size)
  {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  GNUNET_asprintf (&val_name,
                   "%s_val",
                   fname);
  GNUNET_asprintf (&frac_name,
                   "%s_frac",
                   fname);
  ret = extract_amount_nbo_helper (result,
                                   row,
                                   currency,
                                   val_name,
                                   frac_name,
                                   dst);
  GNUNET_free (val_name);
  GNUNET_free (frac_name);
  return ret;
}


/**
 * Currency amount expected.
 *
 * @param name name of the field in the table
 * @param currency the currency to use for @a amount
 * @param[out] amount where to store the result
 * @return array entry for the result specification to use
 */
struct GNUNET_PQ_ResultSpec
TALER_PQ_result_spec_amount_nbo (const char *name,
                                 const char *currency,
                                 struct TALER_AmountNBO *amount)
{
  struct GNUNET_PQ_ResultSpec res = {
    .conv = &extract_amount_nbo,
    .cls = (void *) currency,
    .dst = (void *) amount,
    .dst_size = sizeof (*amount),
    .fname = name
  };

  return res;
}


/**
 * Extract data from a Postgres database @a result at row @a row.
 *
 * @param cls closure, a `const char *` giving the currency
 * @param result where to extract data from
 * @param row row to extract data from
 * @param fname name (or prefix) of the fields to extract from
 * @param[in,out] dst_size where to store size of result, may be NULL
 * @param[out] dst where to store the result
 * @return
 *   #GNUNET_YES if all results could be extracted
 *   #GNUNET_NO if at least one result was NULL
 *   #GNUNET_SYSERR if a result was invalid (non-existing field)
 */
static int
extract_amount (void *cls,
                PGresult *result,
                int row,
                const char *fname,
                size_t *dst_size,
                void *dst)
{
  const char *currency = cls;
  struct TALER_Amount *r_amount = dst;
  char *val_name;
  char *frac_name;
  struct TALER_AmountNBO amount_nbo;
  int ret;

  if (sizeof (struct TALER_AmountNBO) != *dst_size)
  {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  GNUNET_asprintf (&val_name,
                   "%s_val",
                   fname);
  GNUNET_asprintf (&frac_name,
                   "%s_frac",
                   fname);
  ret = extract_amount_nbo_helper (result,
                                   row,
                                   currency,
                                   val_name,
                                   frac_name,
                                   &amount_nbo);
  TALER_amount_ntoh (r_amount,
                     &amount_nbo);
  GNUNET_free (val_name);
  GNUNET_free (frac_name);
  return ret;
}


/**
 * Currency amount expected.
 *
 * @param name name of the field in the table
 * @param currency the currency to use for @a amount
 * @param[out] amount where to store the result
 * @return array entry for the result specification to use
 */
struct GNUNET_PQ_ResultSpec
TALER_PQ_result_spec_amount (const char *name,
                             const char *currency,
                             struct TALER_Amount *amount)
{
  struct GNUNET_PQ_ResultSpec res = {
    .conv = &extract_amount,
    .cls = (void *) currency,
    .dst = (void *) amount,
    .dst_size = sizeof (*amount),
    .fname = name
  };

  return res;
}


/**
 * Extract data from a Postgres database @a result at row @a row.
 *
 * @param cls closure
 * @param result where to extract data from
 * @param row row to extract data from
 * @param fname name (or prefix) of the fields to extract from
 * @param[in,out] dst_size where to store size of result, may be NULL
 * @param[out] dst where to store the result
 * @return
 *   #GNUNET_YES if all results could be extracted
 *   #GNUNET_NO if at least one result was NULL
 *   #GNUNET_SYSERR if a result was invalid (non-existing field)
 */
static int
extract_json (void *cls,
              PGresult *result,
              int row,
              const char *fname,
              size_t *dst_size,
              void *dst)
{
  json_t **j_dst = dst;
  const char *res;
  int fnum;
  json_error_t json_error;
  size_t slen;

  (void) cls;
  (void) dst_size;
  fnum = PQfnumber (result,
                    fname);
  if (fnum < 0)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Field `%s' does not exist in result\n",
                fname);
    return GNUNET_SYSERR;
  }
  if (PQgetisnull (result,
                   row,
                   fnum))
    return GNUNET_NO;
  slen = PQgetlength (result,
                      row,
                      fnum);
  res = (const char *) PQgetvalue (result,
                                   row,
                                   fnum);
  *j_dst = json_loadb (res,
                       slen,
                       JSON_REJECT_DUPLICATES,
                       &json_error);
  if (NULL == *j_dst)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Failed to parse JSON result for field `%s': %s (%s)\n",
                fname,
                json_error.text,
                json_error.source);
    return GNUNET_SYSERR;
  }
  return GNUNET_OK;
}


/**
 * Function called to clean up memory allocated
 * by a #GNUNET_PQ_ResultConverter.
 *
 * @param cls closure
 * @param rd result data to clean up
 */
static void
clean_json (void *cls,
            void *rd)
{
  json_t **dst = rd;

  (void) cls;
  if (NULL != *dst)
  {
    json_decref (*dst);
    *dst = NULL;
  }
}


/**
 * json_t expected.
 *
 * @param name name of the field in the table
 * @param[out] jp where to store the result
 * @return array entry for the result specification to use
 */
struct GNUNET_PQ_ResultSpec
TALER_PQ_result_spec_json (const char *name,
                           json_t **jp)
{
  struct GNUNET_PQ_ResultSpec res = {
    .conv = &extract_json,
    .cleaner = &clean_json,
    .dst = (void *) jp,
    .fname  = name
  };

  return res;
}


/**
 * Extract data from a Postgres database @a result at row @a row.
 *
 * @param cls closure
 * @param result where to extract data from
 * @param row the row to extract data from
 * @param fname name (or prefix) of the fields to extract from
 * @param[in,out] dst_size where to store size of result, may be NULL
 * @param[out] dst where to store the result
 * @return
 *   #GNUNET_YES if all results could be extracted
 *   #GNUNET_SYSERR if a result was invalid (non-existing field or NULL)
 */
static int
extract_round_time (void *cls,
                    PGresult *result,
                    int row,
                    const char *fname,
                    size_t *dst_size,
                    void *dst)
{
  struct GNUNET_TIME_Absolute *udst = dst;
  const struct GNUNET_TIME_AbsoluteNBO *res;
  struct GNUNET_TIME_Absolute tmp;
  int fnum;

  (void) cls;
  fnum = PQfnumber (result,
                    fname);
  if (fnum < 0)
  {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  if (PQgetisnull (result,
                   row,
                   fnum))
  {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  GNUNET_assert (NULL != dst);
  if (sizeof (struct GNUNET_TIME_Absolute) != *dst_size)
  {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  res = (struct GNUNET_TIME_AbsoluteNBO *) PQgetvalue (result,
                                                       row,
                                                       fnum);
  tmp = GNUNET_TIME_absolute_ntoh (*res);
  GNUNET_break (GNUNET_OK ==
                GNUNET_TIME_round_abs (&tmp));
  *udst = tmp;
  return GNUNET_OK;
}


/**
 * Rounded absolute time expected.
 * In contrast to #GNUNET_PQ_query_param_absolute_time_nbo(),
 * this function ensures that the result is rounded and can
 * be converted to JSON.
 *
 * @param name name of the field in the table
 * @param[out] at where to store the result
 * @return array entry for the result specification to use
 */
struct GNUNET_PQ_ResultSpec
TALER_PQ_result_spec_absolute_time (const char *name,
                                    struct GNUNET_TIME_Absolute *at)
{
  struct GNUNET_PQ_ResultSpec res = {
    .conv = &extract_round_time,
    .dst = (void *) at,
    .dst_size = sizeof (struct GNUNET_TIME_Absolute),
    .fname = name
  };

  return res;
}


/**
 * Extract data from a Postgres database @a result at row @a row.
 *
 * @param cls closure
 * @param result where to extract data from
 * @param row the row to extract data from
 * @param fname name (or prefix) of the fields to extract from
 * @param[in,out] dst_size where to store size of result, may be NULL
 * @param[out] dst where to store the result
 * @return
 *   #GNUNET_YES if all results could be extracted
 *   #GNUNET_SYSERR if a result was invalid (non-existing field or NULL)
 */
static int
extract_round_time_nbo (void *cls,
                        PGresult *result,
                        int row,
                        const char *fname,
                        size_t *dst_size,
                        void *dst)
{
  struct GNUNET_TIME_AbsoluteNBO *udst = dst;
  const struct GNUNET_TIME_AbsoluteNBO *res;
  struct GNUNET_TIME_Absolute tmp;
  int fnum;

  (void) cls;
  fnum = PQfnumber (result,
                    fname);
  if (fnum < 0)
  {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  if (PQgetisnull (result,
                   row,
                   fnum))
  {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  GNUNET_assert (NULL != dst);
  if (sizeof (struct GNUNET_TIME_AbsoluteNBO) != *dst_size)
  {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  res = (struct GNUNET_TIME_AbsoluteNBO *) PQgetvalue (result,
                                                       row,
                                                       fnum);
  tmp = GNUNET_TIME_absolute_ntoh (*res);
  GNUNET_break (GNUNET_OK ==
                GNUNET_TIME_round_abs (&tmp));
  *udst = GNUNET_TIME_absolute_hton (tmp);
  return GNUNET_OK;
}


/**
 * Rounded absolute time in network byte order expected.
 * In contrast to #GNUNET_PQ_query_param_absolute_time_nbo(),
 * this function ensures that the result is rounded and can
 * be converted to JSON.
 *
 * @param name name of the field in the table
 * @param[out] at where to store the result
 * @return array entry for the result specification to use
 */
struct GNUNET_PQ_ResultSpec
TALER_PQ_result_spec_absolute_time_nbo (const char *name,
                                        struct GNUNET_TIME_AbsoluteNBO *at)
{
  struct GNUNET_PQ_ResultSpec res = {
    .conv = &extract_round_time_nbo,
    .dst = (void *) at,
    .dst_size = sizeof (struct GNUNET_TIME_AbsoluteNBO),
    .fname = name
  };

  return res;
}


/* end of pq_result_helper.c */
