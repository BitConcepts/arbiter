/* SPDX-License-Identifier: MIT */

#include <arbiter/arbiter.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

LOG_MODULE_DECLARE(arbiter, CONFIG_ARBITER_LOG_LEVEL);

/* Application must provide this function to give the shell access to the ctx */
extern struct ARBITER_ctx *ARBITER_shell_get_ctx(void);

static int cmd_info(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct ARBITER_ctx *ctx = ARBITER_shell_get_ctx();

	if (ctx == NULL || !ctx->initialized) {
		shell_error(sh, "arbiter not initialized");
		return -1;
	}

	const struct ARBITER_model *m = ctx->model;

	shell_print(sh, "Model:      %s", m->name ? m->name : "(unnamed)");
	shell_print(sh, "Facts:      %u", m->fact_count);
	shell_print(sh, "Rules:      %u", m->rule_count);
	shell_print(sh, "Conditions: %u", m->condition_count);
	shell_print(sh, "Actions:    %u", m->action_count);
	shell_print(sh, "Modes:      %u", m->mode_count);
	shell_print(sh, "Version:    %s", ARBITER_version_string());

	return 0;
}

static int cmd_facts(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct ARBITER_ctx *ctx = ARBITER_shell_get_ctx();

	if (ctx == NULL || !ctx->initialized) {
		shell_error(sh, "arbiter not initialized");
		return -1;
	}

	shell_print(sh, "%-4s %-24s %-8s %-10s %-5s", "ID", "Name", "Type",
		    "Value", "Valid");

	for (uint16_t i = 0; i < ctx->model->fact_count; i++) {
		const struct ARBITER_fact_def *def = &ctx->model->facts[i];
		const struct ARBITER_fact_value *fv = &ctx->fact_values[i];
		const char *type_str;

		switch (def->type) {
		case ARBITER_FACT_BOOL:
			type_str = "bool";
			break;
		case ARBITER_FACT_INT32:
			type_str = "int32";
			break;
		case ARBITER_FACT_UINT32:
			type_str = "uint32";
			break;
		case ARBITER_FACT_ENUM:
			type_str = "enum";
			break;
		default:
			type_str = "?";
			break;
		}

		shell_print(sh, "%-4u %-24s %-8s %-10d %-5s",
			    i, def->name ? def->name : "?",
			    type_str, fv->value,
			    fv->valid ? "yes" : "no");
	}

	return 0;
}

static int cmd_eval(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct ARBITER_ctx *ctx = ARBITER_shell_get_ctx();

	if (ctx == NULL || !ctx->initialized) {
		shell_error(sh, "arbiter not initialized");
		return -1;
	}

	struct ARBITER_snapshot snap;
	struct ARBITER_result result;

	int ret = ARBITER_snapshot_begin(ctx, &snap);

	if (ret != ARBITER_OK) {
		shell_error(sh, "Snapshot failed: %d", ret);
		return ret;
	}

	ret = ARBITER_eval(ctx->model, &snap, &result, NULL);
	if (ret != ARBITER_OK) {
		shell_error(sh, "Eval failed: %d", ret);
		return ret;
	}

	ctx->last_eval_op_count = result.eval_op_count;

	shell_print(sh, "Mode: %u  Faults: 0x%08x  Actions: %u  Ops: %u",
		    result.current_mode, result.raised_faults,
		    result.requested_action_count, result.eval_op_count);

	return 0;
}

static int cmd_model_hash(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct ARBITER_ctx *ctx = ARBITER_shell_get_ctx();

	if (ctx == NULL || !ctx->initialized) {
		shell_error(sh, "arbiter not initialized");
		return -1;
	}

	shell_fprintf(sh, SHELL_NORMAL, "Model hash: ");
	for (int i = 0; i < 32; i++) {
		shell_fprintf(sh, SHELL_NORMAL, "%02x",
			      ctx->model->model_hash[i]);
	}
	shell_fprintf(sh, SHELL_NORMAL, "\n");

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(ARBITER_cmds,
	SHELL_CMD(info, NULL, "Show model info", cmd_info),
	SHELL_CMD(facts, NULL, "Show all facts", cmd_facts),
	SHELL_CMD(eval, NULL, "Trigger evaluation", cmd_eval),
	SHELL_CMD(model-hash, NULL, "Show model hash", cmd_model_hash),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(arbiter, &ARBITER_cmds, "arbiter reasoning engine", NULL);
