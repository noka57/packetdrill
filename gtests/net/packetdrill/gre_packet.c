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
 * Implementation for module for formatting GRE packets.
 */

#include "gre_packet.h"

#include "ip_packet.h"
#include "gre.h"

int gre_header_append(struct packet *packet, char **error)
{
	struct header *header;

	header = packet_append_header(packet, HEADER_GRE, sizeof(struct gre));
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

	return STATUS_OK;
}

int gre_header_finish(struct packet *packet,
                      struct header *header, struct header *next_inner)
{
	struct gre *gre = header->h.gre;
	int gre_bytes = sizeof(struct gre) + next_inner->total_bytes;

	gre->protocol = htons(header_type_info(next_inner->type)->eth_proto);

	header->total_bytes = gre_bytes;

	return STATUS_OK;
}
