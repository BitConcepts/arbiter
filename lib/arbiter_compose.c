/* SPDX-License-Identifier: MIT */

/**
 * @file arbiter_compose.c
 * @brief Multi-model composition — shared fact bus implementation.
 *
 * Static array of attached contexts (CONFIG_ARBITER_MAX_MODELS slots).
 * All storage is pre-allocated.  No malloc.
 */

#include <arbiter/arbiter_compose.h>
#include <string.h>
#include <zephyr/kernel.h>

int ARBITER_compose_init(struct ARBITER_fact_bus *bus,
			 struct ARBITER_fact_value *facts,
			 uint16_t max_facts)
{
	if (unlikely(bus == NULL || facts == NULL)) {
		return ARBITER_EINVAL;
	}

	memset(bus, 0, sizeof(*bus));
	bus->facts = facts;
	bus->max_facts = max_facts;

	/* Zero the shared fact array */
	memset(facts, 0, (size_t)max_facts * sizeof(*facts));

	return ARBITER_OK;
}

int ARBITER_compose_attach(struct ARBITER_fact_bus *bus,
			   struct ARBITER_ctx *ctx,
			   const struct ARBITER_fact_mapping *mapping,
			   uint16_t map_count)
{
	if (unlikely(bus == NULL || ctx == NULL || mapping == NULL)) {
		return ARBITER_EINVAL;
	}

	/* Find a free slot */
	for (uint16_t i = 0; i < CONFIG_ARBITER_MAX_MODELS; i++) {
		if (!bus->attachments[i].active) {
			bus->attachments[i].ctx = ctx;
			bus->attachments[i].mapping = mapping;
			bus->attachments[i].map_count = map_count;
			bus->attachments[i].active = true;
			bus->attachment_count++;
			return ARBITER_OK;
		}
	}

	return ARBITER_EOVERFLOW;
}

int ARBITER_compose_detach(struct ARBITER_fact_bus *bus,
			   struct ARBITER_ctx *ctx)
{
	if (unlikely(bus == NULL || ctx == NULL)) {
		return ARBITER_EINVAL;
	}

	for (uint16_t i = 0; i < CONFIG_ARBITER_MAX_MODELS; i++) {
		if (bus->attachments[i].active &&
		    bus->attachments[i].ctx == ctx) {
			bus->attachments[i].active = false;
			bus->attachments[i].ctx = NULL;
			bus->attachments[i].mapping = NULL;
			bus->attachments[i].map_count = 0;
			bus->attachment_count--;
			return ARBITER_OK;
		}
	}

	return ARBITER_EINVAL;
}

int ARBITER_compose_set_fact(struct ARBITER_fact_bus *bus,
			     uint16_t fact_id, int32_t value)
{
	if (unlikely(bus == NULL)) {
		return ARBITER_EINVAL;
	}
	if (unlikely(fact_id >= bus->max_facts)) {
		return ARBITER_ERANGE;
	}

	bus->facts[fact_id].prev_value = bus->facts[fact_id].value;
	bus->facts[fact_id].value = value;
	bus->facts[fact_id].valid = true;
	bus->facts[fact_id].changed =
		(bus->facts[fact_id].value != bus->facts[fact_id].prev_value);

	return ARBITER_OK;
}

int ARBITER_compose_sync(struct ARBITER_fact_bus *bus)
{
	if (unlikely(bus == NULL)) {
		return ARBITER_EINVAL;
	}

	for (uint16_t i = 0; i < CONFIG_ARBITER_MAX_MODELS; i++) {
		const struct ARBITER_compose_attachment *__restrict att =
			&bus->attachments[i];

		if (!att->active) {
			continue;
		}

		struct ARBITER_fact_value *__restrict model_vals =
			att->ctx->fact_values;
		const uint16_t model_max = att->ctx->snapshot.count;
		const struct ARBITER_fact_mapping *__restrict map =
			att->mapping;
		const uint16_t mc = att->map_count;

		for (uint16_t m = 0; m < mc; m++) {
			const uint16_t bid = map[m].bus_fact_id;
			const uint16_t mid = map[m].model_fact_id;

			if (likely(bid < bus->max_facts && mid < model_max)) {
				model_vals[mid].prev_value =
					model_vals[mid].value;
				model_vals[mid].value =
					bus->facts[bid].value;
				model_vals[mid].valid =
					bus->facts[bid].valid;
				model_vals[mid].changed =
					(model_vals[mid].value !=
					 model_vals[mid].prev_value);
			}
		}
	}

	return ARBITER_OK;
}

int ARBITER_compose_eval_all(struct ARBITER_fact_bus *bus,
			     struct ARBITER_result *results,
			     struct ARBITER_trace *traces)
{
	if (unlikely(bus == NULL || results == NULL)) {
		return ARBITER_EINVAL;
	}

	uint16_t idx = 0;

	for (uint16_t i = 0; i < CONFIG_ARBITER_MAX_MODELS; i++) {
		const struct ARBITER_compose_attachment *__restrict att =
			&bus->attachments[i];

		if (!att->active) {
			continue;
		}

		struct ARBITER_ctx *__restrict ctx = att->ctx;
		struct ARBITER_snapshot snap;

		/* Take snapshot */
		int rc = ARBITER_snapshot_begin(ctx, &snap);

		if (unlikely(rc != ARBITER_OK)) {
			results[idx].status = rc;
			idx++;
			continue;
		}

		/* Evaluate */
		struct ARBITER_trace *trace =
			(traces != NULL) ? &traces[idx] : NULL;

		rc = ARBITER_eval(ctx->model, &snap, &results[idx], trace);
		results[idx].status = rc;
		idx++;
	}

	return ARBITER_OK;
}
