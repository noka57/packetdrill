/*
 * Copyright 2013 Google Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
/*
 * Author: ncardwell@google.com (Neal Cardwell)
 *
 * Implementation for module for formatting MPLS packets.
 */

#include "mpls_packet.h"

#include "mpls.h"

int new_mpls_stack_entry(s64 label, s64 traffic_class,
                         bool is_stack_bottom, s64 ttl,
                         struct mpls *mpls, char **error)
{
	if ((label < 0) || (label >= (1 << 20)))
	{
#ifdef ECOS
		int len = strlen("MPLS label out of range for 20 bits");
		*error = malloc(len);
		snprintf(*error, len, "MPLS label out of range for 20 bits");
#else
		asprintf(error, "MPLS label out of range for 20 bits");
#endif
		return STATUS_ERR;
	}

	if ((traffic_class < 0) || (traffic_class >= (1 << 3)))
	{
#ifdef ECOS
		int len = strlen("MPLS traffic_class out of range for 3 bits");
		*error = malloc(len);
		snprintf(*error, len, "MPLS traffic_class out of range for 3 bits");
#else
		asprintf(error, "MPLS traffic_class out of range for 3 bits");
#endif
		return STATUS_ERR;
	}

	if ((ttl < 0) || (ttl >= (1 << 8)))
	{
#ifdef ECOS
		int len = strlen("MPLS ttl out of range for 8 bits");
		*error = malloc(len);
		snprintf(*error, len, "MPLS ttl out of range for 8 bits");
#else
		asprintf(error, "MPLS ttl out of range for 8 bits");
#endif
		return STATUS_ERR;
	}

	mpls_entry_set(label, traffic_class, is_stack_bottom, ttl, mpls);
	return STATUS_OK;
}

int mpls_header_append(struct packet *packet, struct mpls_stack *mpls_stack,
                       char **error)
{
	struct header *header;
	int mpls_bytes = mpls_stack->length * sizeof(struct mpls);

	header = packet_append_header(packet, HEADER_MPLS, mpls_bytes);
	if (header == NULL)
	{
#ifdef ECOS
		int len = strlen("too many headers");
		*error = malloc(len);
		snprintf(*error, len, "too many headers");
#else
		asprintf(error, "too many headers");
#endif
		return STATUS_ERR;
	}

	memcpy(header->h.mpls, mpls_stack->entries, mpls_bytes);

	return STATUS_OK;
}

int mpls_header_finish(struct packet *packet,
                       struct header *header, struct header *next_inner)
{
	int mpls_bytes = header->header_bytes + next_inner->total_bytes;

	header->total_bytes = mpls_bytes;

	return STATUS_OK;
}
