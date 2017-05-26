#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2017 Nicholas Frechette & Animation Compression Library contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
////////////////////////////////////////////////////////////////////////////////

#include "acl/memory.h"
#include "acl/assert.h"
#include "acl/algorithm_globals.h"
#include "acl/bitset.h"
#include "acl/algorithm/full_precision_common.h"
#include "acl/compression/compressed_clip_impl.h"
#include "acl/compression/skeleton.h"
#include "acl/compression/animation_clip.h"

#include <stdint.h>

//////////////////////////////////////////////////////////////////////////
// Full Precision Encoder
//
// The goal of the full precision format is to be used as a reference
// point for compression speed, compressed size, and decompression speed.
// This will not be a raw format in that we will at least drop constant
// or bind pose tracks. As such, it is near-raw but not quite.
//
// This is the highest precision encoder and the fastest to compress.
//
// Data format:
//    TODO: Detail the format
//////////////////////////////////////////////////////////////////////////

namespace acl
{
	namespace internal
	{
		inline void write(const Quat_64& rotation, float* ptr, uint32_t& offset)
		{
			// TODO: Drop quaternion W
			ptr[offset++] = float(quat_get_x(rotation));
			ptr[offset++] = float(quat_get_y(rotation));
			ptr[offset++] = float(quat_get_z(rotation));
			ptr[offset++] = float(quat_get_w(rotation));
		}

		inline void write(const Vector4_64& translation, float* ptr, uint32_t& offset)
		{
			ptr[offset++] = float(vector_get_x(translation));
			ptr[offset++] = float(vector_get_y(translation));
			ptr[offset++] = float(vector_get_z(translation));
		}
	}

	inline void get_num_animated_tracks(const AnimationClip& clip, uint32_t& out_num_constant_rotation_tracks, uint32_t& out_num_constant_translation_tracks,
										uint32_t& out_num_animated_rotation_tracks, uint32_t& out_num_animated_translation_tracks)
	{
		uint16_t num_bones = clip.get_num_bones();

		uint32_t num_constant_rotation_tracks = 0;
		uint32_t num_constant_translation_tracks = 0;
		uint32_t num_animated_rotation_tracks = 0;
		uint32_t num_animated_translation_tracks = 0;

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const AnimatedBone& bone = clip.get_animated_bone(bone_index);

			if (!bone.rotation_track.is_default())
			{
				if (bone.rotation_track.is_constant())
					num_constant_rotation_tracks++;
				else
					num_animated_rotation_tracks++;
			}

			if (!bone.translation_track.is_default())
			{
				if (bone.translation_track.is_constant())
					num_constant_translation_tracks++;
				else
					num_animated_translation_tracks++;
			}
		}

		out_num_constant_rotation_tracks = num_constant_rotation_tracks;
		out_num_constant_translation_tracks = num_constant_translation_tracks;
		out_num_animated_rotation_tracks = num_animated_rotation_tracks;
		out_num_animated_translation_tracks = num_animated_translation_tracks;
	}

	inline void write_default_track_bitset(FullPrecisionHeader& header, const AnimationClip& clip, uint32_t bitset_size)
	{
		uint32_t* default_tracks_bitset = header.get_default_tracks_bitset();
		uint32_t default_track_offset = 0;

		for (uint16_t bone_index = 0; bone_index < header.num_bones; ++bone_index)
		{
			const AnimatedBone& bone = clip.get_animated_bone(bone_index);

			bool is_rotation_default = bone.rotation_track.is_default();
			bool is_translation_default = bone.translation_track.is_default();

			bitset_set(default_tracks_bitset, bitset_size, default_track_offset++, is_rotation_default);
			bitset_set(default_tracks_bitset, bitset_size, default_track_offset++, is_translation_default);
		}
	}

	inline void write_constant_track_bitset(FullPrecisionHeader& header, const AnimationClip& clip, uint32_t bitset_size)
	{
		uint32_t* constant_tracks_bitset = header.get_constant_tracks_bitset();
		uint32_t constant_track_offset = 0;

		for (uint16_t bone_index = 0; bone_index < header.num_bones; ++bone_index)
		{
			const AnimatedBone& bone = clip.get_animated_bone(bone_index);

			bool is_rotation_constant = bone.rotation_track.is_constant();
			bool is_translation_constant = bone.translation_track.is_constant();

			bitset_set(constant_tracks_bitset, bitset_size, constant_track_offset++, is_rotation_constant);
			bitset_set(constant_tracks_bitset, bitset_size, constant_track_offset++, is_translation_constant);
		}
	}

	inline void write_constant_track_data(FullPrecisionHeader& header, const AnimationClip& clip, uint32_t num_constant_floats)
	{
		float* constant_data = header.get_constant_track_data();
		uint32_t constant_data_offset = 0;
		uint32_t constant_data_max_offset = num_constant_floats;

		for (uint16_t bone_index = 0; bone_index < header.num_bones; ++bone_index)
		{
			const AnimatedBone& bone = clip.get_animated_bone(bone_index);

			bool is_rotation_constant = bone.rotation_track.is_constant();
			bool is_translation_constant = bone.translation_track.is_constant();

			if (is_rotation_constant)
			{
				Quat_64 rotation = bone.rotation_track.get_sample(0);
				internal::write(rotation, constant_data, constant_data_offset);
			}

			if (is_translation_constant)
			{
				Vector4_64 translation = bone.translation_track.get_sample(0);
				internal::write(translation, constant_data, constant_data_offset);
			}

			ensure(constant_data_offset <= constant_data_max_offset);
		}

		ensure(constant_data_offset == constant_data_max_offset);
	}

	inline void write_animated_track_data(FullPrecisionHeader& header, const AnimationClip& clip, uint32_t num_animated_floats)
	{
		float* animated_track_data = header.get_track_data();
		uint32_t animated_track_data_offset = 0;
		uint32_t animated_track_data_max_offset = num_animated_floats;

		// Data is sorted first by time, second by bone.
		// This ensures that all bones are contiguous in memory when we sample a particular time.
		for (uint32_t sample_index = 0; sample_index < header.num_samples; ++sample_index)
		{
			for (uint16_t bone_index = 0; bone_index < header.num_bones; ++bone_index)
			{
				const AnimatedBone& bone = clip.get_animated_bone(bone_index);

				if (!bone.rotation_track.is_default() && !bone.rotation_track.is_constant())
				{
					Quat_64 rotation = bone.rotation_track.get_sample(sample_index);
					internal::write(rotation, animated_track_data, animated_track_data_offset);
				}

				if (!bone.translation_track.is_default() && !bone.translation_track.is_constant())
				{
					Vector4_64 translation = bone.translation_track.get_sample(sample_index);
					internal::write(translation, animated_track_data, animated_track_data_offset);
				}

				ensure(animated_track_data_offset <= animated_track_data_max_offset);
			}
		}

		ensure(animated_track_data_offset == animated_track_data_max_offset);
	}

	inline CompressedClip* full_precision_encoder(Allocator& allocator, const AnimationClip& clip, const RigidSkeleton& skeleton)
	{
		uint16_t num_bones = clip.get_num_bones();
		uint32_t num_samples = clip.get_num_samples();

		uint32_t num_constant_rotation_tracks = 0;
		uint32_t num_constant_translation_tracks = 0;
		uint32_t num_animated_rotation_tracks;
		uint32_t num_animated_translation_tracks;
		get_num_animated_tracks(clip, num_constant_rotation_tracks, num_constant_translation_tracks, num_animated_rotation_tracks, num_animated_translation_tracks);

		uint32_t num_constant_floats = (num_constant_rotation_tracks * 4) + (num_constant_translation_tracks * 3);
		uint32_t num_animated_floats = (num_animated_rotation_tracks * 4) + (num_animated_translation_tracks * 3);

		uint32_t bitset_size = ((num_bones * FullPrecisionConstants::NUM_TRACKS_PER_BONE) + FullPrecisionConstants::BITSET_WIDTH - 1) / FullPrecisionConstants::BITSET_WIDTH;

		uint32_t buffer_size = 0;
		buffer_size += sizeof(CompressedClip);
		buffer_size += sizeof(FullPrecisionHeader);
		buffer_size += sizeof(uint32_t) * bitset_size;		// Default tracks bitset
		buffer_size += sizeof(uint32_t) * bitset_size;		// Constant tracks bitset
		buffer_size += sizeof(float) * num_constant_floats;	// Constant track data
		buffer_size += sizeof(float) * num_animated_floats;	// Animated track data

		uint8_t* buffer = allocate_type_array<uint8_t>(allocator, buffer_size, 16);

		CompressedClip* compressed_clip = make_compressed_clip(buffer, buffer_size, AlgorithmType::FullPrecision);

		FullPrecisionHeader& header = get_full_precision_header(*compressed_clip);
		header.num_bones = num_bones;
		header.num_samples = num_samples;
		header.sample_rate = clip.get_sample_rate();
		header.num_animated_rotation_tracks = num_animated_rotation_tracks;
		header.num_animated_translation_tracks = num_animated_translation_tracks;
		header.default_tracks_bitset_offset = sizeof(FullPrecisionHeader);
		header.constant_tracks_bitset_offset = header.default_tracks_bitset_offset + (sizeof(uint32_t) * bitset_size);
		header.constant_track_data_offset = header.constant_tracks_bitset_offset + (sizeof(uint32_t) * bitset_size);	// Aligned to 4 bytes
		header.track_data_offset = header.constant_track_data_offset + (sizeof(float) * num_constant_floats);			// Aligned to 4 bytes

		write_default_track_bitset(header, clip, bitset_size);
		write_constant_track_bitset(header, clip, bitset_size);
		write_constant_track_data(header, clip, num_constant_floats);
		write_animated_track_data(header, clip, num_animated_floats);

		return compressed_clip;
	}
}