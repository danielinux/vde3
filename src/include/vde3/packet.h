/* Copyright (C) 2009 - Virtualsquare Team
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */
/**
 * @file
 */

#ifndef __VDE3_PACKET_H__
#define __VDE3_PACKET_H__
#include <stdint.h>
#include <string.h>

#include <vde3/common.h>
#define VDE_PACKET_SIZE 2048

// A packet exchanged by vde engines
// (it should be used more or less like Linux socket buffers).
//
// - allocated/freed by the same connection (probably using cached memory)
// - copied mostly by connections, but also by engines if they need to cache it
//   or to mangle it in particular ways

/**
 * @brief A vde packet header.
 */
struct __attribute__((packed)) vde_hdr{
  uint8_t version; //!< Header version
  uint8_t type; //!< Type of payload
  uint16_t pkt_len; //!< Payload length
};
typedef struct vde_hdr vde_hdr;


/**
 * @brief A vde packet.
 */
struct vde_pkt_content {
  uint32_t _pos; //!< The position of the packet in the pool
  void *_pool; //!< pointer to the pool this packet belongs to
  uint32_t usage_count; //!< Usage count, if 0, area is freed
  uint8_t data[0]; //!< Allocated memory
};

typedef struct vde_pkt_content vde_pkt_content;



struct vde_pkt {
	struct vde_pkt *next;
    void *head, *tail, *payload; /* pointer to start, end and payload inside 'data' field */
	struct vde_pkt_content *pkt;
	vde_hdr *hdr; //!< Pointer to vde_header inside data
	unsigned int numtries; //!< Number of retries to pass through next connector
	unsigned int data_size; //!< The total size of memory allocated in data
	uint32_t _pool_pos;
	void *_pool_pool;
};

typedef struct vde_pkt vde_pkt;


#endif /* __VDE3_PACKET_H__ */
