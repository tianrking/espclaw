/*
 * ESPClaw - util/ratelimit.h
 * LLM API rate limiting to control costs.
 *
 * Implements sliding window rate limiting:
 * - Max N requests per hour
 * - Max M requests per day
 * - Persists daily count across reboots
 */
#ifndef RATELIMIT_H
#define RATELIMIT_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Initialize rate limiter.
 * Loads persisted state from NVS.
 */
void ratelimit_init(void);

/**
 * Check if a request is allowed.
 *
 * @param reason     Buffer to write rejection reason if denied
 * @param reason_len Size of reason buffer
 * @return true if request is allowed, false if rate limited
 */
bool ratelimit_check(char *reason, size_t reason_len);

/**
 * Record that a request was made.
 * Call after successful LLM response.
 */
void ratelimit_record_request(void);

/**
 * Get current usage statistics.
 */
int ratelimit_get_requests_today(void);
int ratelimit_get_requests_this_hour(void);

/**
 * Manually reset daily counter.
 * Useful for testing or admin override.
 */
void ratelimit_reset_daily(void);

#endif /* RATELIMIT_H */
