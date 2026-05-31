/* SPDX-License-Identifier: MIT */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <arbiter/arbiter.h>

LOG_MODULE_REGISTER(firewall, LOG_LEVEL_INF);

extern const struct ARBITER_model ARBITER_generated_model;
static struct ARBITER_ctx ctx;

void app_fw_drop_packet(void)       { LOG_WRN("FW: DROP"); }
void app_fw_enable_syn_cookies(void){ LOG_WRN("FW: SYN cookies enabled"); }
void app_fw_log_anomaly(void)       { LOG_WRN("FW: Anomaly logged"); }
void app_net_throttle(void)         { LOG_INF("FW: Throttling"); }
void app_net_reject_new_conn(void)  { LOG_INF("FW: Rejecting new conn"); }

/* Fact indices (canonical alphabetical order across imports) */
enum {
	F_ACTION = 0, F_ICMP_RATE, F_IS_DNS, F_IS_SYN,
	F_ANOMALY, F_SRC_BLOCK, F_SRC_ALLOW, F_SYN_SCORE,
	F_CONN_COUNT, F_DST_PORT, F_ESTABLISHED, F_PKT_LEN,
	F_PROTOCOL, F_RATE_PPS, F_SRC_CLASS,
};

static void eval_packet(const char *desc, uint32_t port, uint32_t proto,
			bool blocklist, bool allowlist, bool established,
			bool syn, uint32_t rate)
{
	ARBITER_set_i32(&ctx, F_ACTION, 0); /* pending */
	ARBITER_set_u32(&ctx, F_DST_PORT, port);
	ARBITER_set_i32(&ctx, F_PROTOCOL, proto);
	ARBITER_set_bool(&ctx, F_SRC_BLOCK, blocklist);
	ARBITER_set_bool(&ctx, F_SRC_ALLOW, allowlist);
	ARBITER_set_bool(&ctx, F_ESTABLISHED, established);
	ARBITER_set_bool(&ctx, F_IS_SYN, syn);
	ARBITER_set_bool(&ctx, F_ANOMALY, false);
	ARBITER_set_bool(&ctx, F_IS_DNS, port == 53);
	ARBITER_set_u32(&ctx, F_RATE_PPS, rate);
	ARBITER_set_u32(&ctx, F_PKT_LEN, 128);
	ARBITER_set_u32(&ctx, F_ICMP_RATE, 0);
	ARBITER_set_u32(&ctx, F_CONN_COUNT, 10);

	struct ARBITER_snapshot snap;
	struct ARBITER_result result;

	ARBITER_snapshot_begin(&ctx, &snap);
	ARBITER_eval(&ARBITER_generated_model, &snap, &result, NULL);

	const char *actions[] = {"PENDING","ACCEPT","DROP","REJECT","LOG+ACCEPT","LOG+DROP"};
	int a = ctx.fact_values[F_ACTION].value;

	LOG_INF("  %-30s -> %s (rules=%u)", desc,
		(a >= 0 && a <= 5) ? actions[a] : "?",
		result.requested_action_count);
}

int main(void)
{
	LOG_INF("=== arbiter Embedded Firewall ===");
	ARBITER_init(&ctx, &ARBITER_generated_model);

	eval_packet("CoAP UDP/5683",        5683, 2, false, false, false, false, 50);
	eval_packet("MQTT-TLS TCP/8883",    8883, 1, false, false, false, false, 20);
	eval_packet("Blocklisted source",   80,   1, true,  false, false, false, 10);
	eval_packet("Allowlisted source",   443,  1, false, true,  false, false, 10);
	eval_packet("Established TCP",      8883, 1, false, false, true,  false, 100);
	eval_packet("SYN flood (6000 pps)", 80,   1, false, false, false, true,  6000);
	eval_packet("Unknown port (deny)",  9999, 1, false, false, false, false, 5);
	eval_packet("DNS query",            53,   2, false, false, false, false, 10);

	return 0;
}
