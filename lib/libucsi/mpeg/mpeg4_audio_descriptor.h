/*
 * section and descriptor parser
 *
 * Copyright (C) 2005 Kenneth Aafloy (kenneth@linuxtv.org)
 * Copyright (C) 2005 Andrew de Quincey (adq_dvb@lidskialf.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef _UCSI_MPEG4_AUDIO_DESCRIPTOR
#define _UCSI_MPEG4_AUDIO_DESCRIPTOR 1

#include <ucsi/descriptor.h>
#include <ucsi/common.h>

/**
 * mpeg4_audio_descriptor structure.
 */
struct mpeg4_audio_descriptor {
	struct descriptor d;

	uint8_t mpeg4_audio_profile_and_level;
} packed;

/**
 * Process an mpeg4_audio_descriptor.
 *
 * @param d Generic descriptor structure.
 * @return Pointer to an mpeg4_audio_descriptor structure, or NULL on error.
 */
static inline struct mpeg4_audio_descriptor*
	mpeg4_audio_descriptor_codec(struct descriptor* d)
{
	if (d->len != (sizeof(struct mpeg4_audio_descriptor) - 2))
		return NULL;

	return (struct mpeg4_audio_descriptor*) d;
}

#endif
