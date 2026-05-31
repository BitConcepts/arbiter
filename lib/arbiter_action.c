/* SPDX-License-Identifier: MIT */

#include <arbiter/arbiter.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(arbiter, CONFIG_ARBITER_LOG_LEVEL);

int ARBITER_dispatch_actions(const struct ARBITER_model *model,
			   const struct ARBITER_result *result)
{
	if (model == NULL || result == NULL) {
		return ARBITER_EINVAL;
	}

	int dispatched = 0;

	for (uint16_t i = 0; i < result->requested_action_count; i++) {
		uint16_t action_id = result->requested_actions[i];

		/* Find the action definition */
		const struct ARBITER_action_def *action = NULL;

		for (uint16_t a = 0; a < model->action_count; a++) {
			if (model->actions[a].id == action_id) {
				action = &model->actions[a];
				break;
			}
		}

		if (action == NULL) {
			LOG_WRN("Action %u not found in model", action_id);
			continue;
		}

		switch (action->type) {
		case ARBITER_ACTION_CALLBACK:
			if (action->callback != NULL) {
				LOG_DBG("Dispatching callback: %s",
					action->name ? action->name : "?");
				action->callback();
				dispatched++;
			} else {
				LOG_WRN("Callback is NULL for action %s",
					action->name ? action->name : "?");
			}
			break;

		case ARBITER_ACTION_LOG:
			LOG_INF("Action [log]: %s",
				action->name ? action->name : "unnamed");
			dispatched++;
			break;

		case ARBITER_ACTION_NOTIFY:
			LOG_INF("Action [notify]: %s",
				action->name ? action->name : "unnamed");
			dispatched++;
			break;

		case ARBITER_ACTION_SET_FACT:
		case ARBITER_ACTION_SET_MODE:
		case ARBITER_ACTION_RAISE_FAULT:
		case ARBITER_ACTION_CLEAR_FAULT:
			/* These are handled inline during evaluation */
			dispatched++;
			break;

		default:
			LOG_WRN("Unknown action type: %d", action->type);
			break;
		}
	}

	LOG_DBG("Dispatched %d/%u actions", dispatched,
		result->requested_action_count);
	return dispatched;
}
