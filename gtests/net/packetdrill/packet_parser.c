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
 * Implementation for a module to parse TCP/IP packets.
 */

#include "packet_parser.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "checksum.h"
#include "ethernet.h"
#include "gre.h"
#include "ip.h"
#include "ip_address.h"
#include "logging.h"
#include "packet.h"
#include "tcp.h"

static int parse_ipv4(struct packet *packet, u8 *header_start, u8 *packet_end,
                      char **error);
static int parse_ipv6(struct packet *packet, u8 *header_start, u8 *packet_end,
                      char **error);
static int parse_mpls(struct packet *packet, u8 *header_start, u8 *packet_end,
                      char **error);
static int parse_layer3_packet_by_proto(struct packet *packet,
                                        u16 proto, u8 *header_start,
                                        u8 *packet_end, char **error);
static int parse_layer4(struct packet *packet, u8 *header_start,
                        int layer4_protocol, int layer4_bytes,
                        u8 *packet_end, bool *is_inner, char **error);

static int parse_layer2_packet(struct packet *packet,
                               u8 *header_start, u8 *packet_end,
                               char **error)
{
	u8 *p = header_start;
	struct ether_header *ether = NULL;

	/* Find Ethernet header */
	if (p + sizeof(*ether) > packet_end)
	{
#ifdef ECOS
		int len = strlen("Ethernet header overflows packet");
		*error = malloc(len);
		snprintf(*error, len, "Ethernet header overflows packet");

#else
		asprintf(error, "Ethernet header overflows packet");
#endif
		goto error_out;
	}
	ether = (struct ether_header *)p;
	p += sizeof(*ether);
	packet->l2_header_bytes = sizeof(*ether);

	return parse_layer3_packet_by_proto(packet, ntohs(ether->ether_type),
	                                    p, packet_end, error);

error_out:
	return PACKET_BAD;
}

static int parse_layer3_packet_by_proto(struct packet *packet,
                                        u16 proto, u8 *header_start,
                                        u8 *packet_end, char **error)
{
	u8 *p = header_start;

	if (proto == ETHERTYPE_IP)
	{
		struct ipv4 *ip = NULL;

		/* Examine IPv4 header. */
		if (p + sizeof(struct ipv4) > packet_end)
		{
#ifdef ECOS
			int len = strlen("IPv4 header overflows packet");
			*error = malloc(len);
			snprintf(*error, len, "IPv4 header overflows packet");
#else
			asprintf(error, "IPv4 header overflows packet");
#endif
			goto error_out;
		}

		/* Look at the IP version number, which is in the first 4 bits
		 * of both IPv4 and IPv6 packets.
		 */
		ip = (struct ipv4 *)p;
		if (ip->version == 4)
		{
			return parse_ipv4(packet, p, packet_end, error);
		}
		else
		{
#ifdef ECOS
			int len = strlen("Bad IP version for ETHERTYPE_IP");
			*error = malloc(len);
			snprintf(*error, len, "Bad IP version for ETHERTYPE_IP");
#else
			asprintf(error, "Bad IP version for ETHERTYPE_IP");
#endif
			goto error_out;
		}
	}
	else if (proto == ETHERTYPE_IPV6)
	{
		struct ipv6 *ip = NULL;

		/* Examine IPv6 header. */
		if (p + sizeof(struct ipv6) > packet_end)
		{

#ifdef ECOS
			int len = strlen("IPv6 header overflows packet");
			*error = malloc(len);
			snprintf(*error, len, "IPv6 header overflows packet");
#else
			asprintf(error, "IPv6 header overflows packet");
#endif
			goto error_out;
		}

		/* Look at the IP version number, which is in the first 4 bits
		 * of both IPv4 and IPv6 packets.
		 */
		ip = (struct ipv6 *)p;
		if (ip->version == 6)
		{
			return parse_ipv6(packet, p, packet_end, error);
		}
		else
		{
#ifdef ECOS
			int len = strlen("Bad IP version for ETHERTYPE_IPV6");
			*error = malloc(len);
			snprintf(*error, len, "Bad IP version for ETHERTYPE_IPV6");
#else
			asprintf(error, "Bad IP version for ETHERTYPE_IPV6");
#endif
			goto error_out;
		}
	}
	else if ((proto == ETHERTYPE_MPLS_UC) ||
	         (proto == ETHERTYPE_MPLS_MC))
	{
		return parse_mpls(packet, p, packet_end, error);
	}
	else
	{
		return PACKET_UNKNOWN_L4;
	}

error_out:
	return PACKET_BAD;
}

static int parse_layer3_packet(struct packet *packet,
                               u8 *header_start, u8 *packet_end,
                               char **error)
{
	u8 *p = header_start;
	/* Note that packet_end points to the byte beyond the end of packet. */
	struct ipv4 *ip = NULL;

	/* Examine IPv4/IPv6 header. */
	if (p + sizeof(struct ipv4) > packet_end)
	{
#ifdef ECOS
		int len = strlen("IP header overflows packet");
		*error = malloc(len);
		snprintf(*error, len, "IP header overflows packet");
#else
		asprintf(error, "IP header overflows packet");
#endif
		return PACKET_BAD;
	}

	/* Look at the IP version number, which is in the first 4 bits
	 * of both IPv4 and IPv6 packets.
	 */
	ip = (struct ipv4 *) (p);
	if (ip->version == 4)
		return parse_ipv4(packet, p, packet_end, error);
	else if (ip->version == 6)
		return parse_ipv6(packet, p, packet_end, error);
#ifdef ECOS
	int len = strlen("Unsupported IP version");
	*error = malloc(len);
	snprintf(*error, len, "Unsupported IP version");
#else
	asprintf(error, "Unsupported IP version");
#endif
	return PACKET_BAD;
}

int parse_packet(struct packet *packet, int in_bytes,
                 enum packet_layer_t layer, char **error)
{
	assert(in_bytes <= packet->buffer_bytes);
	char *message = NULL;		/* human-readable error summary */
	char *hex = NULL;		/* hex dump of bad packet */
	enum packet_parse_result_t result = PACKET_BAD;
	u8 *header_start = packet->buffer;
	/* packet_end points to the byte beyond the end of packet. */
	u8 *packet_end = packet->buffer + in_bytes;

	if (layer == PACKET_LAYER_2_ETHERNET)
		result = parse_layer2_packet(packet, header_start, packet_end,
		                             error);
	else if (layer == PACKET_LAYER_3_IP)
		result = parse_layer3_packet(packet, header_start, packet_end,
		                             error);
	else
		assert(!"bad layer");

	if (result != PACKET_BAD)
		return result;

	/* Error. Add a packet hex dump to the error string we're returning. */
	hex_dump(packet->buffer, in_bytes, &hex);
	message = *error;

#ifdef ECOS
	int len = strlen(": packet of  bytes:\n") + strlen(message) + strlen(hex) + 8;
	*error = malloc(len);
	snprintf(*error, len, "%s: packet of %d bytes:\n%s", message, in_bytes, hex);
#else
	asprintf(error, "%s: packet of %d bytes:\n%s", message, in_bytes, hex);
#endif
	free(message);
	free(hex);

	return PACKET_BAD;
}

/* Parse the IPv4 header and the TCP header inside. Return a
 * packet_parse_result_t.
 * Note that packet_end points to the byte beyond the end of packet.
 */
static int parse_ipv4(struct packet *packet, u8 *header_start, u8 *packet_end,
                      char **error)
{
	struct header *ip_header = NULL;
	u8 *p = header_start;
	const bool is_outer = (packet->ip_bytes == 0);
	bool is_inner = false;
	enum packet_parse_result_t result = PACKET_BAD;
	struct ipv4 *ipv4 = (struct ipv4 *) (p);

	const int ip_header_bytes = ipv4_header_len(ipv4);
	assert(ip_header_bytes >= 0);
	if (ip_header_bytes < sizeof(*ipv4))
	{
#ifdef ECOS
		int len = strlen("IP header too short");
		*error = malloc(len);
		snprintf(*error, len, "IP header too short");
#else
		asprintf(error, "IP header too short");
#endif
		goto error_out;
	}
	if (p + ip_header_bytes > packet_end)
	{
#ifdef ECOS
		int len = strlen("Full IP header overflows packet");
		*error = malloc(len);
		snprintf(*error, len, "Full IP header overflows packet");
#else
		asprintf(error, "Full IP header overflows packet");
#endif
		goto error_out;
	}
	const int ip_total_bytes = ntohs(ipv4->tot_len);

	if (p + ip_total_bytes > packet_end)
	{
#ifdef ECOS
		int len = strlen("IP payload overflows packet");
		*error = malloc(len);
		snprintf(*error, len, "IP payload overflows packet");
#else
		asprintf(error, "IP payload overflows packet");
#endif
		goto error_out;
	}
	if (ip_header_bytes > ip_total_bytes)
	{
#ifdef ECOS
		int len = strlen("IP header bigger than datagram");
		*error = malloc(len);
		snprintf(*error, len, "IP header bigger than datagram");
#else
		asprintf(error, "IP header bigger than datagram");
#endif
		goto error_out;
	}
	if (ntohs(ipv4->frag_off) & IP_MF)  	/* more fragments? */
	{
#ifdef ECOS
		int len = strlen("More fragments remaining");
		*error = malloc(len);
		snprintf(*error, len, "More fragments remaining");
#else
		asprintf(error, "More fragments remaining");
#endif
		goto error_out;
	}
	if (ntohs(ipv4->frag_off) & IP_OFFMASK)    /* fragment offset */
	{
#ifdef ECOS
		int len = strlen("Non-zero fragment offset");
		*error = malloc(len);
		snprintf(*error, len, "Non-zero fragment offset");
#else
		asprintf(error, "Non-zero fragment offset");
#endif
		goto error_out;
	}
	const u16 checksum = ipv4_checksum(ipv4, ip_header_bytes);
	if (checksum != 0)
	{
#ifdef ECOS
		int len = strlen("Bad IP checksum");
		*error = malloc(len);
		snprintf(*error, len, "Bad IP checksum");
#else
		asprintf(error, "Bad IP checksum");
#endif
		goto error_out;
	}

	ip_header = packet_append_header(packet, HEADER_IPV4, ip_header_bytes);
	if (ip_header == NULL)
	{
#ifdef ECOS
		int len = strlen("Too many nested headers at IPv4 header");
		*error = malloc(len);
		snprintf(*error, len, "Too many nested headers at IPv4 header");
#else
		asprintf(error, "Too many nested headers at IPv4 header");
#endif
		goto error_out;
	}
	ip_header->total_bytes = ip_total_bytes;

	/* Move on to the header inside. */
	p += ip_header_bytes;
	assert(p <= packet_end);

	if (DEBUG_LOGGING)
	{
		char src_string[ADDR_STR_LEN];
		char dst_string[ADDR_STR_LEN];
		struct ip_address src_ip, dst_ip;
		ip_from_ipv4(&ipv4->src_ip, &src_ip);
		ip_from_ipv4(&ipv4->dst_ip, &dst_ip);
		DEBUGP("src IP: %s\n", ip_to_string(&src_ip, src_string));
		DEBUGP("dst IP: %s\n", ip_to_string(&dst_ip, dst_string));
	}

	/* Examine the L4 header. */
	const int layer4_bytes = ip_total_bytes - ip_header_bytes;
	const int layer4_protocol = ipv4->protocol;
	result = parse_layer4(packet, p, layer4_protocol, layer4_bytes,
	                      packet_end, &is_inner, error);

	/* If this is the innermost IP header then this is the primary. */
	if (is_inner)
		packet->ipv4 = ipv4;
	/* If this is the outermost IP header then this is the packet length. */
	if (is_outer)
		packet->ip_bytes = ip_total_bytes;

	return result;

error_out:
	return PACKET_BAD;
}

/* Parse the IPv6 header and the TCP header inside. We do not
 * currently support parsing IPv6 extension headers or any layer 4
 * protocol other than TCP. Return a packet_parse_result_t.
 * Note that packet_end points to the byte beyond the end of packet.
 */
static int parse_ipv6(struct packet *packet, u8 *header_start, u8 *packet_end,
                      char **error)
{
	struct header *ip_header = NULL;
	u8 *p = header_start;
	const bool is_outer = (packet->ip_bytes == 0);
	bool is_inner = false;
	struct ipv6 *ipv6 = (struct ipv6 *) (p);
	enum packet_parse_result_t result = PACKET_BAD;

	/* Check that header fits in sniffed packet. */
	const int ip_header_bytes = sizeof(*ipv6);
	if (p + ip_header_bytes > packet_end)
	{
#ifdef ECOS
		int len = strlen("IPv6 header overflows packet");
		*error = malloc(len);
		snprintf(*error, len, "IPv6 header overflows packet");
#else
		asprintf(error, "IPv6 header overflows packet");
#endif
		goto error_out;
	}

	/* Check that payload fits in sniffed packet. */
	const int ip_total_bytes = (ip_header_bytes +
	                            ntohs(ipv6->payload_len));

	if (p + ip_total_bytes > packet_end)
	{
#ifdef ECOS
		int len = strlen("IPv6 payload overflows packet");
		*error = malloc(len);
		snprintf(*error, len, "IPv6 payload overflows packet");
#else
		asprintf(error, "IPv6 payload overflows packet");
#endif
		goto error_out;
	}
	assert(ip_header_bytes <= ip_total_bytes);

	ip_header = packet_append_header(packet, HEADER_IPV6, ip_header_bytes);
	if (ip_header == NULL)
	{
#ifdef ECOS
		int len = strlen("Too many nested headers at IPv6 header");
		*error = malloc(len);
		snprintf(*error, len, "Too many nested headers at IPv6 header");
#else
		asprintf(error, "Too many nested headers at IPv6 header");
#endif
		goto error_out;
	}
	ip_header->total_bytes = ip_total_bytes;

	/* Move on to the header inside. */
	p += ip_header_bytes;
	assert(p <= packet_end);

	if (DEBUG_LOGGING)
	{
		char src_string[ADDR_STR_LEN];
		char dst_string[ADDR_STR_LEN];
		struct ip_address src_ip, dst_ip;
		ip_from_ipv6(&ipv6->src_ip, &src_ip);
		ip_from_ipv6(&ipv6->dst_ip, &dst_ip);
		DEBUGP("src IP: %s\n", ip_to_string(&src_ip, src_string));
		DEBUGP("dst IP: %s\n", ip_to_string(&dst_ip, dst_string));
	}

	/* Examine the L4 header. */
	const int layer4_bytes = ip_total_bytes - ip_header_bytes;
	const int layer4_protocol = ipv6->next_header;
	result = parse_layer4(packet, p, layer4_protocol, layer4_bytes,
	                      packet_end, &is_inner, error);

	/* If this is the innermost IP header then this is the primary. */
	if (is_inner)
		packet->ipv6 = ipv6;
	/* If this is the outermost IP header then this is the packet length. */
	if (is_outer)
		packet->ip_bytes = ip_total_bytes;

	return result;

error_out:
	return PACKET_BAD;
}

/* Parse the TCP header. Return a packet_parse_result_t. */
static int parse_tcp(struct packet *packet, u8 *layer4_start, int layer4_bytes,
                     u8 *packet_end, char **error)
{
	struct header *tcp_header = NULL;
	u8 *p = layer4_start;

	assert(layer4_bytes >= 0);
	if (layer4_bytes < sizeof(struct tcp))
	{
#ifdef ECOS
		int len = strlen("Truncated TCP header");
		*error = malloc(len);
		snprintf(*error, len, "Truncated TCP header");
#else
		asprintf(error, "Truncated TCP header");
#endif
		goto error_out;
	}
	packet->tcp = (struct tcp *) p;
	const int tcp_header_len = packet_tcp_header_len(packet);
	if (tcp_header_len < sizeof(struct tcp))
	{
#ifdef ECOS
		int len = strlen("TCP data offset too small");
		*error = malloc(len);
		snprintf(*error, len, "TCP data offset too small");
#else
		asprintf(error, "TCP data offset too small");
#endif
		goto error_out;
	}
	if (tcp_header_len > layer4_bytes)
	{
#ifdef ECOS
		int len = strlen("TCP data offset too big");
		*error = malloc(len);
		snprintf(*error, len, "TCP data offset too big");
#else
		asprintf(error, "TCP data offset too big");
#endif
		goto error_out;
	}

	tcp_header = packet_append_header(packet, HEADER_TCP, tcp_header_len);
	if (tcp_header == NULL)
	{
#ifdef ECOS
		int len = strlen("Too many nested headers at TCP header");
		*error = malloc(len);
		snprintf(*error, len, "Too many nested headers at TCP header");
#else
		asprintf(error, "Too many nested headers at TCP header");
#endif
		goto error_out;
	}
	tcp_header->total_bytes = layer4_bytes;

	p += layer4_bytes;
	assert(p <= packet_end);

	DEBUGP("TCP src port: %d\n", ntohs(packet->tcp->src_port));
	DEBUGP("TCP dst port: %d\n", ntohs(packet->tcp->dst_port));
	return PACKET_OK;

error_out:
	return PACKET_BAD;
}

/* Parse the UDP header. Return a packet_parse_result_t. */
static int parse_udp(struct packet *packet, u8 *layer4_start, int layer4_bytes,
                     u8 *packet_end, char **error)
{
	struct header *udp_header = NULL;
	u8 *p = layer4_start;

	assert(layer4_bytes >= 0);
	if (layer4_bytes < sizeof(struct udp))
	{
#ifdef ECOS
		int len = strlen("Truncated UDP header");
		*error = malloc(len);
		snprintf(*error, len, "Truncated UDP header");
#else
		asprintf(error, "Truncated UDP header");
#endif
		goto error_out;
	}
	packet->udp = (struct udp *) p;
	const int udp_len = ntohs(packet->udp->len);
	const int udp_header_len = sizeof(struct udp);
	if (udp_len < udp_header_len)
	{
#ifdef ECOS
		int len = strlen("UDP datagram length too small for UDP header");
		*error = malloc(len);
		snprintf(*error, len, "UDP datagram length too small for UDP header");
#else
		asprintf(error, "UDP datagram length too small for UDP header");
#endif
		goto error_out;
	}
	if (udp_len < layer4_bytes)
	{
#ifdef ECOS
		int len = strlen("UDP datagram length too small");
		*error = malloc(len);
		snprintf(*error, len, "UDP datagram length too small");
#else
		asprintf(error, "UDP datagram length too small");
#endif
		goto error_out;
	}
	if (udp_len > layer4_bytes)
	{
#ifdef ECOS
		int len = strlen("UDP datagram length too big");
		*error = malloc(len);
		snprintf(*error, len, "UDP datagram length too big");
#else
		asprintf(error, "UDP datagram length too big");
#endif
		goto error_out;
	}

	udp_header = packet_append_header(packet, HEADER_UDP, udp_header_len);
	if (udp_header == NULL)
	{
#ifdef ECOS
		int len = strlen("Too many nested headers at UDP header");
		*error = malloc(len);
		snprintf(*error, len, "Too many nested headers at UDP header");
#else
		asprintf(error, "Too many nested headers at UDP header");
#endif
		goto error_out;
	}
	udp_header->total_bytes = layer4_bytes;

	p += layer4_bytes;
	assert(p <= packet_end);

	DEBUGP("UDP src port: %d\n", ntohs(packet->udp->src_port));
	DEBUGP("UDP dst port: %d\n", ntohs(packet->udp->dst_port));
	return PACKET_OK;

error_out:
	return PACKET_BAD;
}

/* Parse the ICMPv4 header. Return a packet_parse_result_t. */
static int parse_icmpv4(struct packet *packet, u8 *layer4_start,
                        int layer4_bytes, u8 *packet_end, char **error)
{
	struct header *icmp_header = NULL;
	const int icmp_header_len = sizeof(struct icmpv4);
	u8 *p = layer4_start;

	assert(layer4_bytes >= 0);
	/* Make sure the immediately preceding header was IPv4. */
	if (packet_inner_header(packet)->type != HEADER_IPV4)
	{
#ifdef ECOS
		int len = strlen("Bad IP version for IPPROTO_ICMP");
		*error = malloc(len);
		snprintf(*error, len, "Bad IP version for IPPROTO_ICMP");
#else
		asprintf(error, "Bad IP version for IPPROTO_ICMP");
#endif
		goto error_out;
	}

	if (layer4_bytes < sizeof(struct icmpv4))
	{
#ifdef ECOS
		int len = strlen("Truncated ICMPv4 header");
		*error = malloc(len);
		snprintf(*error, len, "Truncated ICMPv4 header");
#else
		asprintf(error, "Truncated ICMPv4 header");
#endif
		goto error_out;
	}

	packet->icmpv4 = (struct icmpv4 *) p;

	icmp_header = packet_append_header(packet, HEADER_ICMPV4,
	                                   icmp_header_len);
	if (icmp_header == NULL)
	{
#ifdef ECOS
		int len = strlen("Too many nested headers at ICMPV4 header");
		*error = malloc(len);
		snprintf(*error, len, "Too many nested headers at ICMPV4 header");
#else
		asprintf(error, "Too many nested headers at ICMPV4 header");
#endif
		goto error_out;
	}
	icmp_header->total_bytes = layer4_bytes;

	p += layer4_bytes;
	assert(p <= packet_end);

	return PACKET_OK;

error_out:
	return PACKET_BAD;
}

/* Parse the ICMPv6 header. Return a packet_parse_result_t. */
static int parse_icmpv6(struct packet *packet, u8 *layer4_start,
                        int layer4_bytes, u8 *packet_end, char **error)
{
	struct header *icmp_header = NULL;
	const int icmp_header_len = sizeof(struct icmpv6);
	u8 *p = layer4_start;

	assert(layer4_bytes >= 0);
	/* Make sure the immediately preceding header was IPv6. */
	if (packet_inner_header(packet)->type != HEADER_IPV6)
	{
#ifdef ECOS
		int len = strlen("Bad IP version for IPPROTO_ICMPV6");
		*error = malloc(len);
		snprintf(*error, len, "Bad IP version for IPPROTO_ICMPV6");
#else
		asprintf(error, "Bad IP version for IPPROTO_ICMPV6");
#endif
		goto error_out;
	}
	if (layer4_bytes < sizeof(struct icmpv6))
	{
#ifdef ECOS
		int len = strlen("Truncated ICMPv6 header");
		*error = malloc(len);
		snprintf(*error, len, "Truncated ICMPv6 header");
#else
		asprintf(error, "Truncated ICMPv6 header");
#endif
		goto error_out;
	}

	packet->icmpv6 = (struct icmpv6 *) p;

	icmp_header = packet_append_header(packet, HEADER_ICMPV6,
	                                   icmp_header_len);
	if (icmp_header == NULL)
	{
#ifdef ECOS
		int len = strlen("Too many nested headers at ICMPV6 header");
		*error = malloc(len);
		snprintf(*error, len, "Too many nested headers at ICMPV6 header");
#else
		asprintf(error, "Too many nested headers at ICMPV6 header");
#endif
		goto error_out;
	}
	icmp_header->total_bytes = layer4_bytes;

	p += layer4_bytes;
	assert(p <= packet_end);

	return PACKET_OK;

error_out:
	return PACKET_BAD;
}

/* Parse the GRE header. Return a packet_parse_result_t. */
static int parse_gre(struct packet *packet, u8 *layer4_start, int layer4_bytes,
                     u8 *packet_end, char **error)
{
	struct header *gre_header = NULL;
	u8 *p = layer4_start;
	struct gre *gre = (struct gre *) p;

	assert(layer4_bytes >= 0);
	if (layer4_bytes < sizeof(struct gre))
	{
#ifdef ECOS
		int len = strlen("Truncated GRE header");
		*error = malloc(len);
		snprintf(*error, len, "Truncated GRE header");
#else
		asprintf(error, "Truncated GRE header");
#endif
		goto error_out;
	}
	if (gre->version != 0)
	{
#ifdef ECOS
		int len = strlen("GRE header has unsupported version number");
		*error = malloc(len);
		snprintf(*error, len, "GRE header has unsupported version number");
#else
		asprintf(error, "GRE header has unsupported version number");
#endif
		goto error_out;
	}
	if (gre->has_routing)
	{
#ifdef ECOS
		int len = strlen("GRE header has unsupported routing info");
		*error = malloc(len);
		snprintf(*error, len, "GRE header has unsupported routing info");
#else
		asprintf(error, "GRE header has unsupported routing info");
#endif
		goto error_out;
	}
	const int gre_header_len = gre_len(gre);
	if (gre_header_len < sizeof(struct gre))
	{
#ifdef ECOS
		int len = strlen("GRE header length too small for GRE header");
		*error = malloc(len);
		snprintf(*error, len, "GRE header length too small for GRE header");
#else
		asprintf(error, "GRE header length too small for GRE header");
#endif
		goto error_out;
	}
	if (gre_header_len > layer4_bytes)
	{
#ifdef ECOS
		int len = strlen("GRE header length too big");
		*error = malloc(len);
		snprintf(*error, len, "GRE header length too big");
#else
		asprintf(error, "GRE header length too big");
#endif
		goto error_out;
	}

	assert(p + layer4_bytes <= packet_end);

	DEBUGP("GRE header len: %d\n", gre_header_len);

	gre_header = packet_append_header(packet, HEADER_GRE, gre_header_len);
	if (gre_header == NULL)
	{
#ifdef ECOS
		int len = strlen("Too many nested headers at GRE header");
		*error = malloc(len);
		snprintf(*error, len, "Too many nested headers at GRE header");
#else
		asprintf(error, "Too many nested headers at GRE header");
#endif
		goto error_out;
	}
	gre_header->total_bytes = layer4_bytes;

	p += gre_header_len;
	assert(p <= packet_end);
	return parse_layer3_packet_by_proto(packet, ntohs(gre->protocol),
	                                    p, packet_end, error);

error_out:
	return PACKET_BAD;
}

int parse_mpls(struct packet *packet, u8 *header_start, u8 *packet_end,
               char **error)
{
	struct header *mpls_header = NULL;
	u8 *p = header_start;
	int mpls_header_bytes = 0;
	int mpls_total_bytes = packet_end - p;
	bool is_stack_bottom = false;

	do
	{
		struct mpls *mpls_entry = (struct mpls *)(p);

		if (p + sizeof(struct mpls) > packet_end)
		{
#ifdef ECOS
			int len = strlen("MPLS stack entry overflows packet");
			*error = malloc(len);
			snprintf(*error, len, "MPLS stack entry overflows packet");
#else
			asprintf(error, "MPLS stack entry overflows packet");
#endif
			goto error_out;
		}

		is_stack_bottom = mpls_entry_stack(mpls_entry);

		p += sizeof(struct mpls);
		mpls_header_bytes += sizeof(struct mpls);
	}
	while (!is_stack_bottom && p < packet_end);

	assert(mpls_header_bytes <= mpls_total_bytes);

	mpls_header = packet_append_header(packet, HEADER_MPLS,
	                                   mpls_header_bytes);
	if (mpls_header == NULL)
	{
#ifdef ECOS
		int len = strlen("Too many nested headers at MPLS header");
		*error = malloc(len);
		snprintf(*error, len, "Too many nested headers at MPLS header");
#else
		asprintf(error, "Too many nested headers at MPLS header");
#endif
		goto error_out;
	}
	mpls_header->total_bytes = mpls_total_bytes;

	/* Move on to the header inside the MPLS label stack. */
	assert(p <= packet_end);
	return parse_layer3_packet(packet, p, packet_end, error);

error_out:
	return PACKET_BAD;
}

static int parse_layer4(struct packet *packet, u8 *layer4_start,
                        int layer4_protocol, int layer4_bytes,
                        u8 *packet_end, bool *is_inner, char **error)
{
	if (layer4_protocol == IPPROTO_TCP)
	{
		*is_inner = true;	/* found inner-most layer 4 */
		return parse_tcp(packet, layer4_start, layer4_bytes, packet_end,
		                 error);
	}
	else if (layer4_protocol == IPPROTO_UDP)
	{
		*is_inner = true;	/* found inner-most layer 4 */
		return parse_udp(packet, layer4_start, layer4_bytes, packet_end,
		                 error);
	}
	else if (layer4_protocol == IPPROTO_ICMP)
	{
		*is_inner = true;	/* found inner-most layer 4 */
		return parse_icmpv4(packet, layer4_start, layer4_bytes,
		                    packet_end, error);
	}
	else if (layer4_protocol == IPPROTO_ICMPV6)
	{
		*is_inner = true;	/* found inner-most layer 4 */
		return parse_icmpv6(packet, layer4_start, layer4_bytes,
		                    packet_end, error);
	}
	else if (layer4_protocol == IPPROTO_GRE)
	{
		*is_inner = false;
		return parse_gre(packet, layer4_start, layer4_bytes, packet_end,
		                 error);
	}
	else if (layer4_protocol == IPPROTO_IPIP)
	{
		*is_inner = false;
		return parse_ipv4(packet, layer4_start, packet_end, error);
	}
	else if (layer4_protocol == IPPROTO_IPV6)
	{
		*is_inner = false;
		return parse_ipv6(packet, layer4_start, packet_end, error);
	}
	return PACKET_UNKNOWN_L4;
}
