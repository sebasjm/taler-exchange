/*
     This file is part of GNU Taler
     Copyright (C) 2012-2020 Taler Systems SA

     GNU Taler is free software: you can redistribute it and/or modify it
     under the terms of the GNU Lesser General Public License as published
     by the Free Software Foundation, either version 3 of the License,
     or (at your option) any later version.

     GNU Taler is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     Lesser General Public License for more details.

     You should have received a copy of the GNU Lesser General Public License
     along with this program.  If not, see <http://www.gnu.org/licenses/>.

     SPDX-License-Identifier: LGPL3.0-or-later

     Note: the LGPL does not apply to all components of GNU Taler,
     but it does apply to this file.
 */
#include "taler_error_codes.h"
#include <stddef.h>


/**
 * A pair containing an error code and its hint.
 */
struct ErrorCodeAndHint
{
  /**
   * The error code.
   */
  enum TALER_ErrorCode ec;

  /**
   * The hint.
   */
  const char *hint;
};


/**
 * The list of all error codes with their hints.
 */
static const struct ErrorCodeAndHint code_hint_pairs[] = {

  {
    .ec = TALER_EC_NONE,
    .hint = "Special code to indicate success (no error)."
  },
  {
    .ec = TALER_EC_INVALID,
    .hint = "A non-integer error code was returned in the JSON response."
  },
  {
    .ec = TALER_EC_GENERIC_INVALID_RESPONSE,
    .hint = "The response we got from the server was not even in JSON format."
  },
  {
    .ec = TALER_EC_GENERIC_TIMEOUT,
    .hint = "An operation timed out."
  },
  {
    .ec = TALER_EC_GENERIC_VERSION_MALFORMED,
    .hint = "The version string given does not follow the expected CURRENT:REVISION:AGE Format."
  },
  {
    .ec = TALER_EC_GENERIC_REPLY_MALFORMED,
    .hint = "The service responded with a reply that was in JSON but did not satsify the protocol. Note that invalid cryptographic signatures should have signature-specific error codes."
  },
  {
    .ec = TALER_EC_GENERIC_METHOD_INVALID,
    .hint = "The HTTP method used is invalid for this endpoint."
  },
  {
    .ec = TALER_EC_GENERIC_ENDPOINT_UNKNOWN,
    .hint = "There is no endpoint defined for the URL provided by the client."
  },
  {
    .ec = TALER_EC_GENERIC_JSON_INVALID,
    .hint = "The JSON in the client's request was malformed (generic parse error)."
  },
  {
    .ec = TALER_EC_GENERIC_PAYTO_URI_MALFORMED,
    .hint = "The payto:// URI provided by the client is malformed."
  },
  {
    .ec = TALER_EC_GENERIC_PARAMETER_MISSING,
    .hint = "A required parameter in the request was missing."
  },
  {
    .ec = TALER_EC_GENERIC_PARAMETER_MALFORMED,
    .hint = "A parameter in the request was malformed."
  },
  {
    .ec = TALER_EC_GENERIC_CURRENCY_MISMATCH,
    .hint = "The currencies involved in the operation do not match."
  },
  {
    .ec = TALER_EC_GENERIC_URI_TOO_LONG,
    .hint = "The URI is longer than the longest URI the HTTP server is willing to parse."
  },
  {
    .ec = TALER_EC_GENERIC_UPLOAD_EXCEEDS_LIMIT,
    .hint = "The body is too large to be permissible for the endpoint."
  },
  {
    .ec = TALER_EC_GENERIC_DB_SETUP_FAILED,
    .hint = "The service failed initialize its connection to the database."
  },
  {
    .ec = TALER_EC_GENERIC_DB_START_FAILED,
    .hint = "The service encountered an error event to just start the database transaction."
  },
  {
    .ec = TALER_EC_GENERIC_DB_STORE_FAILED,
    .hint = "The service failed to store information in its database."
  },
  {
    .ec = TALER_EC_GENERIC_DB_FETCH_FAILED,
    .hint = "The service failed to fetch information from its database."
  },
  {
    .ec = TALER_EC_GENERIC_DB_COMMIT_FAILED,
    .hint = "The service encountered an error event to commit the database transaction (hard, unrecoverable error)."
  },
  {
    .ec = TALER_EC_GENERIC_DB_SOFT_FAILURE,
    .hint = "The service encountered an error event to commit the database transaction, even after repeatedly retrying it there was always a conflicting transaction. (This indicates a repeated serialization error; should only happen if some client maliciously tries to create conflicting concurrent transactions.)"
  },
  {
    .ec = TALER_EC_GENERIC_DB_INVARIANT_FAILURE,
    .hint = "The service's database is inconsistent and violates service-internal invariants."
  },
  {
    .ec = TALER_EC_GENERIC_INTERNAL_INVARIANT_FAILURE,
    .hint = "The HTTP server experienced an internal invariant failure (bug)."
  },
  {
    .ec = TALER_EC_GENERIC_FAILED_COMPUTE_JSON_HASH,
    .hint = "The service could not compute a cryptographic hash over some JSON value."
  },
  {
    .ec = TALER_EC_GENERIC_PARSER_OUT_OF_MEMORY,
    .hint = "The HTTP server had insufficient memory to parse the request."
  },
  {
    .ec = TALER_EC_GENERIC_ALLOCATION_FAILURE,
    .hint = "The HTTP server failed to allocate memory."
  },
  {
    .ec = TALER_EC_GENERIC_JSON_ALLOCATION_FAILURE,
    .hint = "The HTTP server failed to allocate memory for building JSON reply."
  },
  {
    .ec = TALER_EC_EXCHANGE_GENERIC_BAD_CONFIGURATION,
    .hint = "Exchange is badly configured and thus cannot operate."
  },
  {
    .ec = TALER_EC_EXCHANGE_GENERIC_OPERATION_UNKNOWN,
    .hint = "Operation specified unknown for this endpoint."
  },
  {
    .ec = TALER_EC_EXCHANGE_GENERIC_WRONG_NUMBER_OF_SEGMENTS,
    .hint = "The number of segments included in the URI does not match the number of segments expected by the endpoint."
  },
  {
    .ec = TALER_EC_EXCHANGE_GENERIC_COIN_CONFLICTING_DENOMINATION_KEY,
    .hint = "The same coin was already used with a different denomination previously."
  },
  {
    .ec = TALER_EC_EXCHANGE_GENERIC_COINS_INVALID_COIN_PUB,
    .hint = "The public key of given to a \"/coins/\" endpoint of the exchange was malformed."
  },
  {
    .ec = TALER_EC_EXCHANGE_GENERIC_DENOMINATION_KEY_UNKNOWN,
    .hint = "The exchange is not aware of the denomination key the wallet requested for the operation."
  },
  {
    .ec = TALER_EC_EXCHANGE_DENOMINATION_SIGNATURE_INVALID,
    .hint = "The signature of the denomination key over the coin is not valid."
  },
  {
    .ec = TALER_EC_EXCHANGE_GENERIC_KEYS_MISSING,
    .hint = "The exchange failed to perform the operation as it could not find the private keys. This is a problem with the exchange setup, not with the client's request."
  },
  {
    .ec = TALER_EC_EXCHANGE_GENERIC_DENOMINATION_VALIDITY_IN_FUTURE,
    .hint = "Validity period of the denomination lies in the future."
  },
  {
    .ec = TALER_EC_EXCHANGE_GENERIC_DENOMINATION_EXPIRED,
    .hint = "Denomination key of the coin is past its expiration time for the requested operation."
  },
  {
    .ec = TALER_EC_EXCHANGE_DEPOSITS_GET_NOT_FOUND,
    .hint = "The exchange did not find information about the specified transaction in the database."
  },
  {
    .ec = TALER_EC_EXCHANGE_DEPOSITS_GET_INVALID_H_WIRE,
    .hint = "The wire hash of given to a \"/deposits/\" handler was malformed."
  },
  {
    .ec = TALER_EC_EXCHANGE_DEPOSITS_GET_INVALID_MERCHANT_PUB,
    .hint = "The merchant key of given to a \"/deposits/\" handler was malformed."
  },
  {
    .ec = TALER_EC_EXCHANGE_DEPOSITS_GET_INVALID_H_CONTRACT_TERMS,
    .hint = "The hash of the contract terms given to a \"/deposits/\" handler was malformed."
  },
  {
    .ec = TALER_EC_EXCHANGE_DEPOSITS_GET_INVALID_COIN_PUB,
    .hint = "The coin public key of given to a \"/deposits/\" handler was malformed."
  },
  {
    .ec = TALER_EC_EXCHANGE_DEPOSITS_GET_INVALID_SIGNATURE_BY_EXCHANGE,
    .hint = "The signature returned by the exchange in a /deposits/ request was malformed."
  },
  {
    .ec = TALER_EC_EXCHANGE_DEPOSITS_GET_MERCHANT_SIGNATURE_INVALID,
    .hint = "The signature of the merchant is invalid."
  },
  {
    .ec = TALER_EC_EXCHANGE_WITHDRAW_INSUFFICIENT_FUNDS,
    .hint = "The given reserve does not have sufficient funds to admit the requested withdraw operation at this time.  The response includes the current \"balance\" of the reserve as well as the transaction \"history\" that lead to this balance."
  },
  {
    .ec = TALER_EC_EXCHANGE_WITHDRAW_RESERVE_UNKNOWN,
    .hint = "The exchange has no information about the \"reserve_pub\" that was given."
  },
  {
    .ec = TALER_EC_EXCHANGE_WITHDRAW_AMOUNT_FEE_OVERFLOW,
    .hint = "The amount to withdraw together with the fee exceeds the numeric range for Taler amounts.  This is not a client failure, as the coin value and fees come from the exchange's configuration."
  },
  {
    .ec = TALER_EC_EXCHANGE_WITHDRAW_SIGNATURE_FAILED,
    .hint = "The exchange failed to create the signature using the denomination key."
  },
  {
    .ec = TALER_EC_EXCHANGE_WITHDRAW_RESERVE_SIGNATURE_INVALID,
    .hint = "The signature of the reserve is not valid."
  },
  {
    .ec = TALER_EC_EXCHANGE_WITHDRAW_HISTORY_ERROR_INSUFFICIENT_FUNDS,
    .hint = "When computing the reserve history, we ended up with a negative overall balance, which should be impossible."
  },
  {
    .ec = TALER_EC_EXCHANGE_WITHDRAW_VALIDITY_IN_PAST,
    .hint = "Withdraw period of the coin to be withdrawn is in the past."
  },
  {
    .ec = TALER_EC_EXCHANGE_WITHDRAW_DENOMINATION_KEY_LOST,
    .hint = "Withdraw period of the coin to be withdrawn is in the past."
  },
  {
    .ec = TALER_EC_EXCHANGE_WITHDRAW_UNBLIND_FAILURE,
    .hint = "The client failed to unblind the blind signature."
  },
  {
    .ec = TALER_EC_EXCHANGE_DEPOSIT_INSUFFICIENT_FUNDS,
    .hint = "The respective coin did not have sufficient residual value for the /deposit operation (i.e. due to double spending). The \"history\" in the response provides the transaction history of the coin proving this fact."
  },
  {
    .ec = TALER_EC_EXCHANGE_DEPOSIT_COIN_SIGNATURE_INVALID,
    .hint = "The signature made by the coin over the deposit permission is not valid."
  },
  {
    .ec = TALER_EC_EXCHANGE_DEPOSIT_NEGATIVE_VALUE_AFTER_FEE,
    .hint = "The stated value of the coin after the deposit fee is subtracted would be negative."
  },
  {
    .ec = TALER_EC_EXCHANGE_DEPOSIT_REFUND_DEADLINE_AFTER_WIRE_DEADLINE,
    .hint = "The stated refund deadline is after the wire deadline."
  },
  {
    .ec = TALER_EC_EXCHANGE_DEPOSIT_INVALID_WIRE_FORMAT_JSON,
    .hint = "The exchange failed to canonicalize and hash the given wire format. For example, the merchant failed to provide the \"salt\" or a valid payto:// URI in the wire details.  Note that while the exchange will do some basic sanity checking on the wire details, it cannot warrant that the banking system will ultimately be able to route to the specified address, even if this check passed."
  },
  {
    .ec = TALER_EC_EXCHANGE_DEPOSIT_INVALID_WIRE_FORMAT_CONTRACT_HASH_CONFLICT,
    .hint = "The hash of the given wire address does not match the wire hash specified in the proposal data."
  },
  {
    .ec = TALER_EC_EXCHANGE_DEPOSIT_INVALID_SIGNATURE_BY_EXCHANGE,
    .hint = "The signature provided by the exchange is not valid."
  },
  {
    .ec = TALER_EC_EXCHANGE_RESERVES_GET_STATUS_UNKNOWN,
    .hint = "The reserve status was requested using a unknown key."
  },
  {
    .ec = TALER_EC_EXCHANGE_MELT_INSUFFICIENT_FUNDS,
    .hint = "The respective coin did not have sufficient residual value for the /refresh/melt operation.  The \"history\" in this response provdes the \"residual_value\" of the coin, which may be less than its \"original_value\"."
  },
  {
    .ec = TALER_EC_EXCHANGE_MELT_COIN_HISTORY_COMPUTATION_FAILED,
    .hint = "The exchange had an internal error reconstructing the transaction history of the coin that was being melted."
  },
  {
    .ec = TALER_EC_EXCHANGE_MELT_FEES_EXCEED_CONTRIBUTION,
    .hint = "The exchange encountered melt fees exceeding the melted coin's contribution."
  },
  {
    .ec = TALER_EC_EXCHANGE_MELT_COIN_SIGNATURE_INVALID,
    .hint = "The signature made with the coin to be melted is invalid."
  },
  {
    .ec = TALER_EC_EXCHANGE_MELT_HISTORY_DB_ERROR_INSUFFICIENT_FUNDS,
    .hint = "The exchange failed to obtain the transaction history of the given coin from the database while generating an insufficient funds errors."
  },
  {
    .ec = TALER_EC_EXCHANGE_MELT_COIN_EXPIRED_NO_ZOMBIE,
    .hint = "The denomination of the given coin has past its expiration date and it is also not a valid zombie (that is, was not refreshed with the fresh coin being subjected to recoup)."
  },
  {
    .ec = TALER_EC_EXCHANGE_MELT_INVALID_SIGNATURE_BY_EXCHANGE,
    .hint = "The signature returned by the exchange in a melt request was malformed."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFRESHES_REVEAL_COMMITMENT_VIOLATION,
    .hint = "The provided transfer keys do not match up with the original commitment.  Information about the original commitment is included in the response."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFRESHES_REVEAL_SIGNING_ERROR,
    .hint = "Failed to produce the blinded signatures over the coins to be returned."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFRESHES_REVEAL_SESSION_UNKNOWN,
    .hint = "The exchange is unaware of the refresh session specified in the request."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFRESHES_REVEAL_CNC_TRANSFER_ARRAY_SIZE_INVALID,
    .hint = "The size of the cut-and-choose dimension of the private transfer keys request does not match #TALER_CNC_KAPPA - 1."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFRESHES_REVEAL_NEW_DENOMS_ARRAY_SIZE_EXCESSIVE,
    .hint = "The number of coins to be created in refresh exceeds the limits of the exchange. private transfer keys request does not match #TALER_CNC_KAPPA - 1."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFRESHES_REVEAL_NEW_DENOMS_ARRAY_SIZE_MISMATCH,
    .hint = "The number of envelopes given does not match the number of denomination keys given."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFRESHES_REVEAL_COST_CALCULATION_OVERFLOW,
    .hint = "The exchange encountered a numeric overflow totaling up the cost for the refresh operation."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFRESHES_REVEAL_AMOUNT_INSUFFICIENT,
    .hint = "The exchange's cost calculation shows that the melt amount is below the costs of the transaction."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFRESHES_REVEAL_LINK_SIGNATURE_INVALID,
    .hint = "The signature made with the coin over the link data is invalid."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFRESHES_REVEAL_INVALID_RCH,
    .hint = "The refresh session hash given to a /refreshes/ handler was malformed."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFRESHES_REVEAL_OPERATION_INVALID,
    .hint = "Operation specified invalid for this endpoint."
  },
  {
    .ec = TALER_EC_EXCHANGE_LINK_COIN_UNKNOWN,
    .hint = "The coin specified in the link request is unknown to the exchange."
  },
  {
    .ec = TALER_EC_EXCHANGE_TRANSFERS_GET_WTID_MALFORMED,
    .hint = "The public key of given to a /transfers/ handler was malformed."
  },
  {
    .ec = TALER_EC_EXCHANGE_TRANSFERS_GET_WTID_NOT_FOUND,
    .hint = "The exchange did not find information about the specified wire transfer identifier in the database."
  },
  {
    .ec = TALER_EC_EXCHANGE_TRANSFERS_GET_WIRE_FEE_NOT_FOUND,
    .hint = "The exchange did not find information about the wire transfer fees it charged."
  },
  {
    .ec = TALER_EC_EXCHANGE_TRANSFERS_GET_WIRE_FEE_INCONSISTENT,
    .hint = "The exchange found a wire fee that was above the total transfer value (and thus could not have been charged)."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFUND_COIN_NOT_FOUND,
    .hint = "The exchange knows literally nothing about the coin we were asked to refund. But without a transaction history, we cannot issue a refund. This is kind-of OK, the owner should just refresh it directly without executing the refund."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFUND_CONFLICT_DEPOSIT_INSUFFICIENT,
    .hint = "We could not process the refund request as the coin's transaction history does not permit the requested refund because then refunds would exceed the deposit amount.  The \"history\" in the response proves this."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFUND_DEPOSIT_NOT_FOUND,
    .hint = "The exchange knows about the coin we were asked to refund, but not about the specific /deposit operation.  Hence, we cannot issue a refund (as we do not know if this merchant public key is authorized to do a refund)."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFUND_MERCHANT_ALREADY_PAID,
    .hint = "The exchange can no longer refund the customer/coin as the money was already transferred (paid out) to the merchant. (It should be past the refund deadline.)"
  },
  {
    .ec = TALER_EC_EXCHANGE_REFUND_FEE_TOO_LOW,
    .hint = "The refund fee specified for the request is lower than the refund fee charged by the exchange for the given denomination key of the refunded coin."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFUND_FEE_ABOVE_AMOUNT,
    .hint = "The refunded amount is smaller than the refund fee, which would result in a negative refund."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFUND_MERCHANT_SIGNATURE_INVALID,
    .hint = "The signature of the merchant is invalid."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFUND_MERCHANT_SIGNING_FAILED,
    .hint = "Merchant backend failed to create the refund confirmation signature."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFUND_INVALID_SIGNATURE_BY_EXCHANGE,
    .hint = "The signature returned by the exchange in a refund request was malformed."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFUND_INVALID_FAILURE_PROOF_BY_EXCHANGE,
    .hint = "The failure proof returned by the exchange is incorrect."
  },
  {
    .ec = TALER_EC_EXCHANGE_REFUND_INCONSISTENT_AMOUNT,
    .hint = "Conflicting refund granted before with different amount but same refund transaction ID."
  },
  {
    .ec = TALER_EC_EXCHANGE_RECOUP_SIGNATURE_INVALID,
    .hint = "The given coin signature is invalid for the request."
  },
  {
    .ec = TALER_EC_EXCHANGE_RECOUP_WITHDRAW_NOT_FOUND,
    .hint = "The exchange could not find the corresponding withdraw operation. The request is denied."
  },
  {
    .ec = TALER_EC_EXCHANGE_RECOUP_COIN_BALANCE_ZERO,
    .hint = "The coin's remaining balance is zero.  The request is denied."
  },
  {
    .ec = TALER_EC_EXCHANGE_RECOUP_BLINDING_FAILED,
    .hint = "The exchange failed to reproduce the coin's blinding."
  },
  {
    .ec = TALER_EC_EXCHANGE_RECOUP_COIN_BALANCE_NEGATIVE,
    .hint = "The coin's remaining balance is zero.  The request is denied."
  },
  {
    .ec = TALER_EC_EXCHANGE_KEYS_TIMETRAVEL_FORBIDDEN,
    .hint = "This exchange does not allow clients to request /keys for times other than the current (exchange) time."
  },
  {
    .ec = TALER_EC_EXCHANGE_WIRE_SIGNATURE_INVALID,
    .hint = "A signature in the server's response was malformed."
  },
  {
    .ec = TALER_EC_EXCHANGE_DENOMINATION_HELPER_UNAVAILABLE,
    .hint = "The exchange failed to talk to the process responsible for its private denomination keys."
  },
  {
    .ec = TALER_EC_EXCHANGE_DENOMINATION_HELPER_BUG,
    .hint = "The response from the denomination key helper process was malformed."
  },
  {
    .ec = TALER_EC_EXCHANGE_DENOMINATION_HELPER_TOO_EARLY,
    .hint = "The helper refuses to sign with the key, because it is too early: the validity period has not yet started."
  },
  {
    .ec = TALER_EC_EXCHANGE_SIGNKEY_HELPER_UNAVAILABLE,
    .hint = "The exchange failed to talk to the process responsible for its private signing keys."
  },
  {
    .ec = TALER_EC_EXCHANGE_SIGNKEY_HELPER_BUG,
    .hint = "The response from the online signing key helper process was malformed."
  },
  {
    .ec = TALER_EC_EXCHANGE_SIGNKEY_HELPER_TOO_EARLY,
    .hint = "The helper refuses to sign with the key, because it is too early: the validity period has not yet started."
  },
  {
    .ec = TALER_EC_EXCHANGE_MANAGEMENT_AUDITOR_NOT_FOUND,
    .hint = "The auditor that was supposed to be disabled is unknown to this exchange."
  },
  {
    .ec = TALER_EC_EXCHANGE_MANAGEMENT_AUDITOR_MORE_RECENT_PRESENT,
    .hint = "The exchange has a more recently signed conflicting instruction and is thus refusing the current change (replay detected)."
  },
  {
    .ec = TALER_EC_EXCHANGE_MANAGEMENT_AUDITOR_ADD_SIGNATURE_INVALID,
    .hint = "The signature to add or enable the auditor does not validate."
  },
  {
    .ec = TALER_EC_EXCHANGE_MANAGEMENT_AUDITOR_DEL_SIGNATURE_INVALID,
    .hint = "The signature to disable the auditor does not validate."
  },
  {
    .ec = TALER_EC_EXCHANGE_MANAGEMENT_DENOMINATION_REVOKE_SIGNATURE_INVALID,
    .hint = "The signature to revoke the denomination does not validate."
  },
  {
    .ec = TALER_EC_EXCHANGE_MANAGEMENT_SIGNKEY_REVOKE_SIGNATURE_INVALID,
    .hint = "The signature to revoke the online signing key does not validate."
  },
  {
    .ec = TALER_EC_EXCHANGE_MANAGEMENT_WIRE_MORE_RECENT_PRESENT,
    .hint = "The exchange has a more recently signed conflicting instruction and is thus refusing the current change (replay detected)."
  },
  {
    .ec = TALER_EC_EXCHANGE_MANAGEMENT_KEYS_SIGNKEY_UNKNOWN,
    .hint = "The signingkey specified is unknown to the exchange."
  },
  {
    .ec = TALER_EC_EXCHANGE_MANAGEMENT_WIRE_DETAILS_SIGNATURE_INVALID,
    .hint = "The signature to publish wire account does not validate."
  },
  {
    .ec = TALER_EC_EXCHANGE_MANAGEMENT_WIRE_ADD_SIGNATURE_INVALID,
    .hint = "The signature to add the wire account does not validate."
  },
  {
    .ec = TALER_EC_EXCHANGE_MANAGEMENT_WIRE_DEL_SIGNATURE_INVALID,
    .hint = "The signature to disable the wire account does not validate."
  },
  {
    .ec = TALER_EC_EXCHANGE_MANAGEMENT_WIRE_NOT_FOUND,
    .hint = "The wire account to be disabled is unknown to the exchange."
  },
  {
    .ec = TALER_EC_EXCHANGE_MANAGEMENT_WIRE_FEE_SIGNATURE_INVALID,
    .hint = "The signature to affirm wire fees does not validate."
  },
  {
    .ec = TALER_EC_EXCHANGE_MANAGEMENT_WIRE_FEE_MISMATCH,
    .hint = "The signature conflicts with a previous signature affirming different fees."
  },
  {
    .ec = TALER_EC_EXCHANGE_MANAGEMENT_KEYS_DENOMKEY_ADD_SIGNATURE_INVALID,
    .hint = "The signature affirming the denomination key is invalid."
  },
  {
    .ec = TALER_EC_EXCHANGE_MANAGEMENT_KEYS_SIGNKEY_ADD_SIGNATURE_INVALID,
    .hint = "The signature affirming the signing key is invalid."
  },
  {
    .ec = TALER_EC_EXCHANGE_AUDITORS_AUDITOR_SIGNATURE_INVALID,
    .hint = "The auditor signature over the denomination meta data is invalid."
  },
  {
    .ec = TALER_EC_EXCHANGE_AUDITORS_AUDITOR_UNKNOWN,
    .hint = "The auditor that was specified is unknown to this exchange."
  },
  {
    .ec = TALER_EC_EXCHANGE_AUDITORS_AUDITOR_INACTIVE,
    .hint = "The auditor that was specified is no longer used by this exchange."
  },
  {
    .ec = TALER_EC_MERCHANT_GENERIC_INSTANCE_UNKNOWN,
    .hint = "The backend could not find the merchant instance specified in the request."
  },
  {
    .ec = TALER_EC_MERCHANT_GENERIC_HOLE_IN_WIRE_FEE_STRUCTURE,
    .hint = "The start and end-times in the wire fee structure leave a hole. This is not allowed."
  },
  {
    .ec = TALER_EC_MERCHANT_GENERIC_RESERVE_PUB_MALFORMED,
    .hint = "The reserve key of given to a /reserves/ handler was malformed."
  },
  {
    .ec = TALER_EC_MERCHANT_GENERIC_FAILED_TO_LOAD_TEMPLATE,
    .hint = "The backend could not locate a required template to generate an HTML reply."
  },
  {
    .ec = TALER_EC_MERCHANT_GENERIC_FAILED_TO_EXPAND_TEMPLATE,
    .hint = "The backend could not expand the template to generate an HTML reply."
  },
  {
    .ec = TALER_EC_MERCHANT_GENERIC_ORDER_UNKNOWN,
    .hint = "The proposal is not known to the backend."
  },
  {
    .ec = TALER_EC_MERCHANT_GENERIC_PRODUCT_UNKNOWN,
    .hint = "The order provided to the backend could not be completed, because a product to be completed via inventory data is not actually in our inventory."
  },
  {
    .ec = TALER_EC_MERCHANT_GENERIC_TIP_ID_UNKNOWN,
    .hint = "The tip ID is unknown.  This could happen if the tip has expired."
  },
  {
    .ec = TALER_EC_MERCHANT_GENERIC_DB_CONTRACT_CONTENT_INVALID,
    .hint = "The contract obtained from the merchant backend was malformed."
  },
  {
    .ec = TALER_EC_MERCHANT_GENERIC_CONTRACT_HASH_DOES_NOT_MATCH_ORDER,
    .hint = "The order we found does not match the provided contract hash."
  },
  {
    .ec = TALER_EC_MERCHANT_GENERIC_EXCHANGE_KEYS_FAILURE,
    .hint = "The exchange failed to provide a valid response to the merchant's /keys request."
  },
  {
    .ec = TALER_EC_MERCHANT_GENERIC_EXCHANGE_TIMEOUT,
    .hint = "The exchange failed to respond to the merchant on time."
  },
  {
    .ec = TALER_EC_MERCHANT_GENERIC_EXCHANGE_CONNECT_FAILURE,
    .hint = "The merchant failed to talk to the exchange."
  },
  {
    .ec = TALER_EC_MERCHANT_GENERIC_EXCHANGE_REPLY_MALFORMED,
    .hint = "The exchange returned a maformed response."
  },
  {
    .ec = TALER_EC_MERCHANT_GENERIC_EXCHANGE_UNEXPECTED_STATUS,
    .hint = "The exchange returned an unexpected response status."
  },
  {
    .ec = TALER_EC_MERCHANT_GET_ORDERS_EXCHANGE_TRACKING_FAILURE,
    .hint = "The exchange failed to provide a valid answer to the tracking request, thus those details are not in the response."
  },
  {
    .ec = TALER_EC_MERCHANT_GET_ORDERS_ID_EXCHANGE_REQUEST_FAILURE,
    .hint = "The merchant backend failed to construct the request for tracking to the exchange, thus tracking details are not in the response."
  },
  {
    .ec = TALER_EC_MERCHANT_GET_ORDERS_ID_EXCHANGE_LOOKUP_START_FAILURE,
    .hint = "The merchant backend failed trying to contact the exchange for tracking details, thus those details are not in the response."
  },
  {
    .ec = TALER_EC_MERCHANT_GET_ORDERS_ID_INVALID_TOKEN,
    .hint = "The token used to authenticate the client is invalid for this order."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAY_INSUFFICIENT_FUNDS,
    .hint = "The exchange responded saying that funds were insufficient (for example, due to double-spending)."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAY_DENOMINATION_KEY_NOT_FOUND,
    .hint = "The denomination key used for payment is not listed among the denomination keys of the exchange."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAY_DENOMINATION_KEY_AUDITOR_FAILURE,
    .hint = "The denomination key used for payment is not audited by an auditor approved by the merchant."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAY_AMOUNT_OVERFLOW,
    .hint = "There was an integer overflow totaling up the amounts or deposit fees in the payment."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAY_FEES_EXCEED_PAYMENT,
    .hint = "The deposit fees exceed the total value of the payment."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAY_INSUFFICIENT_DUE_TO_FEES,
    .hint = "After considering deposit and wire fees, the payment is insufficient to satisfy the required amount for the contract.  The client should revisit the logic used to calculate fees it must cover."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAY_PAYMENT_INSUFFICIENT,
    .hint = "Even if we do not consider deposit and wire fees, the payment is insufficient to satisfy the required amount for the contract."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAY_COIN_SIGNATURE_INVALID,
    .hint = "The signature over the contract of one of the coins was invalid."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAY_EXCHANGE_LOOKUP_FAILED,
    .hint = "When we tried to find information about the exchange to issue the deposit, we failed.  This usually only happens if the merchant backend is somehow unable to get its own HTTP client logic to work."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAY_REFUND_DEADLINE_PAST_WIRE_TRANSFER_DEADLINE,
    .hint = "The refund deadline in the contract is after the transfer deadline."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAY_OFFER_EXPIRED,
    .hint = "The payment is too late, the offer has expired."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAY_MERCHANT_FIELD_MISSING,
    .hint = "The \"merchant\" field is missing in the proposal data. This is an internal error as the proposal is from the merchant's own database at this point."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAY_WIRE_HASH_UNKNOWN,
    .hint = "Failed to locate merchant's account information matching the wire hash given in the proposal."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAY_DENOMINATION_DEPOSIT_EXPIRED,
    .hint = "The deposit time for the denomination has expired."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAY_EXCHANGE_WIRE_FEE_ADDITION_FAILED,
    .hint = "The exchange of the deposited coin charges a wire fee that could not be added to the total (total amount too high)."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAY_REFUNDED,
    .hint = "The contract was not fully paid because of refunds. Note that clients MAY treat this as paid if, for example, contracts must be executed despite of refunds."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAY_REFUNDS_EXCEED_PAYMENTS,
    .hint = "According to our database, we have refunded more than we were paid (which should not be possible)."
  },
  {
    .ec = TALER_EC_DEAD_QQQ_PAY_MERCHANT_POST_ORDERS_ID_ABORT_REFUND_REFUSED_PAYMENT_COMPLETE,
    .hint = "Legacy stuff. Remove me with protocol v1."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAY_EXCHANGE_FAILED,
    .hint = "The payment failed at the exchange."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAID_CONTRACT_HASH_MISMATCH,
    .hint = "The contract hash does not match the given order ID."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_PAID_COIN_SIGNATURE_INVALID,
    .hint = "The signature of the merchant is not valid for the given contract hash."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_ABORT_EXCHANGE_REFUND_FAILED,
    .hint = "The merchant failed to send the exchange the refund request."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_ABORT_EXCHANGE_LOOKUP_FAILED,
    .hint = "The merchant failed to find the exchange to process the lookup."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_ABORT_CONTRACT_NOT_FOUND,
    .hint = "The merchant could not find the contract."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_ABORT_REFUND_REFUSED_PAYMENT_COMPLETE,
    .hint = "The payment was already completed and thus cannot be aborted anymore."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_ABORT_CONTRACT_HASH_MISSMATCH,
    .hint = "The hash provided by the wallet does not match the order."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_ABORT_COINS_ARRAY_EMPTY,
    .hint = "The array of coins cannot be empty."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_CLAIM_NOT_FOUND,
    .hint = "We could not claim the order because the backend is unaware of it."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_CLAIM_ALREADY_CLAIMED,
    .hint = "We could not claim the order because someone else claimed it first."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_CLAIM_CLIENT_INTERNAL_FAILURE,
    .hint = "The client-side experienced an internal failure."
  },
  {
    .ec = TALER_EC_MERCHANT_POST_ORDERS_ID_REFUND_SIGNATURE_FAILED,
    .hint = "The backend failed to sign the refund request."
  },
  {
    .ec = TALER_EC_MERCHANT_TIP_PICKUP_UNBLIND_FAILURE,
    .hint = "The client failed to unblind the signature returned by the merchant."
  },
  {
    .ec = TALER_EC_MERCHANT_TIP_PICKUP_EXCHANGE_ERROR,
    .hint = "The exchange returned a failure code for the withdraw operation."
  },
  {
    .ec = TALER_EC_MERCHANT_TIP_PICKUP_SUMMATION_FAILED,
    .hint = "The merchant failed to add up the amounts to compute the pick up value."
  },
  {
    .ec = TALER_EC_MERCHANT_TIP_PICKUP_HAS_EXPIRED,
    .hint = "The tip expired."
  },
  {
    .ec = TALER_EC_MERCHANT_TIP_PICKUP_AMOUNT_EXCEEDS_TIP_REMAINING,
    .hint = "The requested withdraw amount exceeds the amount remaining to be picked up."
  },
  {
    .ec = TALER_EC_MERCHANT_TIP_PICKUP_DENOMINATION_UNKNOWN,
    .hint = "The merchant did not find the specified denomination key in the exchange's key set."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_ORDERS_INSTANCE_CONFIGURATION_LACKS_WIRE,
    .hint = "The backend lacks a wire transfer method configuration option for the given instance. Thus, this instance is unavailable (not findable for creating new orders)."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_ORDERS_NO_LOCALTIME,
    .hint = "The proposal had no timestamp and the backend failed to obtain the local time. Likely to be an internal error."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_ORDERS_PROPOSAL_PARSE_ERROR,
    .hint = "The order provided to the backend could not be parsed, some required fields were missing or ill-formed."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_ORDERS_ALREADY_EXISTS,
    .hint = "The backend encountered an error: the proposal already exists."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_PATCH_ORDERS_ID_FORGET_PATH_SYNTAX_INCORRECT,
    .hint = "One of the paths to forget is malformed."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_PATCH_ORDERS_ID_FORGET_PATH_NOT_FORGETTABLE,
    .hint = "One of the paths to forget was not marked as forgettable."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_DELETE_ORDERS_AWAITING_PAYMENT,
    .hint = "The order provided to the backend could not be deleted, our offer is still valid and awaiting payment."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_ORDERS_ID_REFUND_INCONSISTENT_AMOUNT,
    .hint = "The amount to be refunded is inconsistent: either is lower than the previous amount being awarded, or it is too big to be paid back. In this second case, the fault stays on the business dept. side."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_ORDERS_ID_REFUND_ORDER_UNPAID,
    .hint = "The frontend gave an unpaid order id to issue the refund to."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_ORDERS_ID_REFUND_NOT_ALLOWED_BY_CONTRACT,
    .hint = "The refund delay was set to 0 and thus no refunds are allowed for this order."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_TRANSFERS_REQUEST_ERROR,
    .hint = "We internally failed to execute the /track/transfer request."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_TRANSFERS_CONFLICTING_REPORTS,
    .hint = "The exchange gave conflicting information about a coin which has been wire transferred."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_TRANSFERS_BAD_WIRE_FEE,
    .hint = "The exchange charged a different wire fee than what it originally advertised, and it is higher."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_TRANSFERS_ACCOUNT_NOT_FOUND,
    .hint = "We did not find the account that the transfer was made to."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_INSTANCES_ALREADY_EXISTS,
    .hint = "The merchant backend cannot create an instance under the given identifier as one already exists. Use PATCH to modify the existing entry."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_PRODUCTS_CONFLICT_PRODUCT_EXISTS,
    .hint = "The product ID exists."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_PATCH_PRODUCTS_TOTAL_LOST_REDUCED,
    .hint = "The update would have reduced the total amount of product lost, which is not allowed."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_PATCH_PRODUCTS_TOTAL_LOST_EXCEEDS_STOCKS,
    .hint = "The update would have mean that more stocks were lost than what remains from total inventory after sales, which is not allowed."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_PATCH_PRODUCTS_TOTAL_STOCKED_REDUCED,
    .hint = "The update would have reduced the total amount of product in stock, which is not allowed."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_PRODUCTS_LOCK_INSUFFICIENT_STOCKS,
    .hint = "The lock request is for more products than we have left (unlocked) in stock."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_DELETE_PRODUCTS_CONFLICTING_LOCK,
    .hint = "The deletion request is for a product that is locked."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_RESERVES_UNSUPPORTED_WIRE_METHOD,
    .hint = "The requested wire method is not supported by the exchange."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_DELETE_RESERVES_NO_SUCH_RESERVE,
    .hint = "The reserve could not be deleted because it is unknown."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_TIP_AUTHORIZE_RESERVE_EXPIRED,
    .hint = "The reserve that was used to fund the tips has expired."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_TIP_AUTHORIZE_RESERVE_UNKNOWN,
    .hint = "The reserve that was used to fund the tips was not found in the DB."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_TIP_AUTHORIZE_INSUFFICIENT_FUNDS,
    .hint = "The backend knows the instance that was supposed to support the tip, and it was configured for tipping. However, the funds remaining are insufficient to cover the tip, and the merchant should top up the reserve."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_POST_TIP_AUTHORIZE_RESERVE_NOT_FOUND,
    .hint = "The backend failed to find a reserve needed to authorize the tip."
  },
  {
    .ec = TALER_EC_MERCHANT_PRIVATE_GET_ORDERS_ID_AMOUNT_ARITHMETIC_FAILURE,
    .hint = "The merchant backend encountered a failure in computing the deposit total."
  },
  {
    .ec = TALER_EC_AUDITOR_DEPOSIT_CONFIRMATION_SIGNATURE_INVALID,
    .hint = "The signature from the exchange on the deposit confirmation is invalid."
  },
  {
    .ec = TALER_EC_BANK_SAME_ACCOUNT,
    .hint = "Wire transfer attempted with credit and debit party being the same bank account."
  },
  {
    .ec = TALER_EC_BANK_UNALLOWED_DEBIT,
    .hint = "Wire transfer impossible, due to financial limitation of the party that attempted the payment."
  },
  {
    .ec = TALER_EC_BANK_NEGATIVE_NUMBER_AMOUNT,
    .hint = "Negative number was used (as value and/or fraction) to initiate a Amount object."
  },
  {
    .ec = TALER_EC_BANK_NUMBER_TOO_BIG,
    .hint = "A number too big was used (as value and/or fraction) to initiate a amount object."
  },
  {
    .ec = TALER_EC_BANK_LOGIN_FAILED,
    .hint = "Could not login for the requested operation."
  },
  {
    .ec = TALER_EC_BANK_UNKNOWN_ACCOUNT,
    .hint = "The bank account referenced in the requested operation was not found. Returned along \"400 Not found\"."
  },
  {
    .ec = TALER_EC_BANK_TRANSACTION_NOT_FOUND,
    .hint = "The transaction referenced in the requested operation (typically a reject operation), was not found."
  },
  {
    .ec = TALER_EC_BANK_BAD_FORMAT_AMOUNT,
    .hint = "Bank received a malformed amount string."
  },
  {
    .ec = TALER_EC_BANK_REJECT_NO_RIGHTS,
    .hint = "The client does not own the account credited by the transaction which is to be rejected, so it has no rights do reject it.  To be returned along HTTP 403 Forbidden."
  },
  {
    .ec = TALER_EC_BANK_UNMANAGED_EXCEPTION,
    .hint = "This error code is returned when no known exception types captured the exception, and comes along with a 500 Internal Server Error."
  },
  {
    .ec = TALER_EC_BANK_SOFT_EXCEPTION,
    .hint = "This error code is used for all those exceptions that do not really need a specific error code to return to the client, but need to signal the middleware that the bank is not responding with 500 Internal Server Error.  Used for example when a client is trying to register with a unavailable username."
  },
  {
    .ec = TALER_EC_BANK_TRANSFER_REQUEST_UID_REUSED,
    .hint = "The request UID for a request to transfer funds has already been used, but with different details for the transfer."
  },
  {
    .ec = TALER_EC_BANK_WITHDRAWAL_OPERATION_RESERVE_SELECTION_CONFLICT,
    .hint = "The withdrawal operation already has a reserve selected.  The current request conflicts with the existing selection."
  },
  {
    .ec = TALER_EC_SYNC_ACCOUNT_UNKNOWN,
    .hint = "The sync service failed find the account in its database."
  },
  {
    .ec = TALER_EC_SYNC_BAD_IF_NONE_MATCH,
    .hint = "The SHA-512 hash provided in the If-None-Match header is malformed."
  },
  {
    .ec = TALER_EC_SYNC_BAD_IF_MATCH,
    .hint = "The SHA-512 hash provided in the If-Match header is malformed or missing."
  },
  {
    .ec = TALER_EC_SYNC_BAD_SYNC_SIGNATURE,
    .hint = "The signature provided in the \"Sync-Signature\" header is malformed or missing."
  },
  {
    .ec = TALER_EC_SYNC_INVALID_SIGNATURE,
    .hint = "The signature provided in the \"Sync-Signature\" header does not match the account, old or new Etags."
  },
  {
    .ec = TALER_EC_SYNC_MALFORMED_CONTENT_LENGTH,
    .hint = "The \"Content-length\" field for the upload is not a number."
  },
  {
    .ec = TALER_EC_SYNC_EXCESSIVE_CONTENT_LENGTH,
    .hint = "The \"Content-length\" field for the upload is too big based on the server's terms of service."
  },
  {
    .ec = TALER_EC_SYNC_OUT_OF_MEMORY_ON_CONTENT_LENGTH,
    .hint = "The server is out of memory to handle the upload. Trying again later may succeed."
  },
  {
    .ec = TALER_EC_SYNC_INVALID_UPLOAD,
    .hint = "The uploaded data does not match the Etag."
  },
  {
    .ec = TALER_EC_SYNC_PAYMENT_GENERIC_TIMEOUT,
    .hint = "HTTP server experienced a timeout while awaiting promised payment."
  },
  {
    .ec = TALER_EC_SYNC_PAYMENT_CREATE_BACKEND_ERROR,
    .hint = "Sync could not setup the payment request with its own backend."
  },
  {
    .ec = TALER_EC_SYNC_PREVIOUS_BACKUP_UNKNOWN,
    .hint = "The sync service failed find the backup to be updated in its database."
  },
  {
    .ec = TALER_EC_SYNC_MISSING_CONTENT_LENGTH,
    .hint = "The \"Content-length\" field for the upload is missing."
  },
  {
    .ec = TALER_EC_WALLET_EXCHANGE_PROTOCOL_VERSION_INCOMPATIBLE,
    .hint = "The wallet does not implement a version of the exchange protocol that is compatible with the protocol version of the exchange."
  },
  {
    .ec = TALER_EC_WALLET_UNEXPECTED_EXCEPTION,
    .hint = "The wallet encountered an unexpected exception.  This is likely a bug in the wallet implementation."
  },
  {
    .ec = TALER_EC_WALLET_RECEIVED_MALFORMED_RESPONSE,
    .hint = "The wallet received a response from a server, but the response can't be parsed."
  },
  {
    .ec = TALER_EC_WALLET_NETWORK_ERROR,
    .hint = "The wallet tried to make a network request, but it received no response."
  },
  {
    .ec = TALER_EC_WALLET_HTTP_REQUEST_THROTTLED,
    .hint = "The wallet tried to make a network request, but it was throttled."
  },
  {
    .ec = TALER_EC_WALLET_UNEXPECTED_REQUEST_ERROR,
    .hint = "The wallet made a request to a service, but received an error response it does not know how to handle."
  },
  {
    .ec = TALER_EC_WALLET_EXCHANGE_DENOMINATIONS_INSUFFICIENT,
    .hint = "The denominations offered by the exchange are insufficient.  Likely the exchange is badly configured or not maintained."
  },
  {
    .ec = TALER_EC_WALLET_CORE_API_OPERATION_UNKNOWN,
    .hint = "The wallet does not support the operation requested by a client."
  },
  {
    .ec = TALER_EC_WALLET_INVALID_TALER_PAY_URI,
    .hint = "The given taler://pay URI is invalid."
  },
  {
    .ec = TALER_EC_WALLET_EXCHANGE_COIN_SIGNATURE_INVALID,
    .hint = "The signature on a coin by the exchange's denomination key is invalid after unblinding it."
  },
  {
    .ec = TALER_EC_WALLET_EXCHANGE_WITHDRAW_RESERVE_UNKNOWN_AT_EXCHANGE,
    .hint = "The exchange does not know about the reserve (yet), and thus withdrawal can't progress."
  },
  {
    .ec = TALER_EC_WALLET_CORE_NOT_AVAILABLE,
    .hint = "The wallet core service is not available."
  },
  {
    .ec = TALER_EC_WALLET_WITHDRAWAL_OPERATION_ABORTED_BY_BANK,
    .hint = "The bank has aborted a withdrawal operation, and thus a withdrawal can't complete."
  },
  {
    .ec = TALER_EC_WALLET_HTTP_REQUEST_GENERIC_TIMEOUT,
    .hint = "An HTTP request made by the wallet timed out."
  },
  {
    .ec = TALER_EC_WALLET_ORDER_ALREADY_CLAIMED,
    .hint = "The order has already been claimed by another wallet."
  },
  {
    .ec = TALER_EC_WALLET_WITHDRAWAL_GROUP_INCOMPLETE,
    .hint = "A group of withdrawal operations (typically for the same reserve at the same exchange) has errors and will be tried again later."
  },
  {
    .ec = TALER_EC_WALLET_TIPPING_COIN_SIGNATURE_INVALID,
    .hint = "The signature on a coin by the exchange's denomination key (obtained through the merchant via tipping) is invalid after unblinding it."
  },
  {
    .ec = TALER_EC_WALLET_BANK_INTEGRATION_PROTOCOL_VERSION_INCOMPATIBLE,
    .hint = "The wallet does not implement a version of the bank integration API that is compatible with the version offered by the bank."
  },
  {
    .ec = TALER_EC_WALLET_CONTRACT_TERMS_BASE_URL_MISMATCH,
    .hint = "The wallet processed a taler://pay URI, but the merchant base URL in the downloaded contract terms does not match the merchant base URL derived from the URI."
  },
  {
    .ec = TALER_EC_WALLET_CONTRACT_TERMS_SIGNATURE_INVALID,
    .hint = "The merchant's signature on the contract terms is invalid."
  },
  {
    .ec = TALER_EC_ANASTASIS_REDUCER_ACTION_INVALID,
    .hint = "The given action is invalid for the current state of the reducer."
  },
  {
    .ec = TALER_EC_END,
    .hint = "End of error code range."
  },

};


/**
 * The length of @e code_hint_pairs.
 */
static const unsigned int code_hint_pairs_length = 260;


/**
 * Returns a hint for a given error code.
 *
 * @param ec the error code.
 * @return the hint if it could be found, otherwise "<no hint found>".
 */
const char *
TALER_ErrorCode_get_hint (enum TALER_ErrorCode ec)
{
  unsigned int lower = 0;
  unsigned int upper = code_hint_pairs_length - 1;
  unsigned int mid = upper / 2;
  while (lower <= upper)
  {
    mid = (upper + lower) / 2;
    if (code_hint_pairs[mid].ec < ec)
    {
      lower = mid + 1;
    }
    else if (code_hint_pairs[mid].ec > ec)
    {
      upper = mid - 1;
    }
    else
    {
      return code_hint_pairs[mid].hint;
    }
  }
  return "<no hint found>";
}
