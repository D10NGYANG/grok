/*
 *    Copyright (C) 2016-2020 Grok Image Compression Inc.
 *
 *    This source code is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This source code is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *    This source code incorporates work covered by the following copyright and
 *    permission notice:
 *
 * The copyright in this software is being made available under the 2-clauses
 * BSD License, included below. This software may be subject to other third
 * party and contributor rights, including patent rights, and no such rights
 * are granted under this license.
 *
 * Copyright (c) 2017, IntoPix SA <contact@intopix.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

/**
@file SparseBuffer.h
@brief Sparse array management

The functions in this file manage sparse arrays. Sparse arrays are arrays with
potentially large dimensions, but with very few samples actually set. Such sparse
arrays require allocating a small amount of memory, by just allocating memory
for blocks of the array that are set. The minimum memory allocation unit is a
a block. There is a trade-off to pick up an appropriate dimension for blocks.
If it is too big, and pixels set are far from each other, too much memory will
be used. If blocks are too small, the book-keeping costs of blocks will rise.
*/

/** @defgroup SparseBuffer SPARSE ARRAYS - Sparse arrays */
/*@{*/

#include <cstdint>

namespace grk {

class SparseBuffer {

public:

	/** Creates a new sparse array.
	 *
	 * @param width total width of the array.
	 * @param height total height of the array
	 * @param block_width width of a block.
	 * @param block_height height of a block.
	 *
	 * @return a new sparse array instance, or NULL in case of failure.
	 */
	SparseBuffer(uint32_t width,
					uint32_t height,
					uint32_t block_width,
					uint32_t block_height);

	/** Frees a sparse array.
	 *
	 */
	~SparseBuffer();

	/** Read the content of a rectangular region of the sparse array into a
	 * user buffer.
	 *
	 * Regions not written with write() are read as 0.
	 *
	 * @param x0 left x coordinate of the region to read in the sparse array.
	 * @param y0 top x coordinate of the region to read in the sparse array.
	 * @param x1 right x coordinate (not included) of the region to read in the sparse array. Must be greater than x0.
	 * @param y1 bottom y coordinate (not included) of the region to read in the sparse array. Must be greater than y0.
	 * @param dest user buffer to fill. Must be at least sizeof(int32) * ( (y1 - y0 - 1) * dest_line_stride + (x1 - x0 - 1) * dest_col_stride + 1) bytes large.
	 * @param dest_col_stride spacing (in elements, not in bytes) in x dimension between consecutive elements of the user buffer.
	 * @param dest_line_stride spacing (in elements, not in bytes) in y dimension between consecutive elements of the user buffer.
	 * @param forgiving if set to TRUE and the region is invalid, true will still be returned.
	 * @return true in case of success.
	 */
	bool read(uint32_t x0,
			 uint32_t y0,
			 uint32_t x1,
			 uint32_t y1,
			 int32_t* dest,
			 const uint32_t dest_col_stride,
			 const uint32_t dest_line_stride,
			 bool forgiving);

	/** Read the content of a rectangular region of the sparse array into a
	 * user buffer.
	 *
	 * Regions not written with write() are read as 0.
	 *
	 * @param region region to read in the sparse array.
	 * @param dest user buffer to fill. Must be at least sizeof(int32) * ( (y1 - y0 - 1) * dest_line_stride + (x1 - x0 - 1) * dest_col_stride + 1) bytes large.
	 * @param dest_col_stride spacing (in elements, not in bytes) in x dimension between consecutive elements of the user buffer.
	 * @param dest_line_stride spacing (in elements, not in bytes) in y dimension between consecutive elements of the user buffer.
	 * @param forgiving if set to TRUE and the region is invalid, true will still be returned.
	 * @return true in case of success.
	 */
	bool read(grk_rect_u32 region,
			 int32_t* dest,
			 const uint32_t dest_col_stride,
			 const uint32_t dest_line_stride,
			 bool forgiving);


	/** Write the content of a rectangular region into the sparse array from a
	 * user buffer.
	 *
	 * Blocks intersecting the region are allocated, if not already done.
	 *
	 * @param x0 left x coordinate of the region to write into the sparse array.
	 * @param y0 top x coordinate of the region to write into the sparse array.
	 * @param x1 right x coordinate (not included) of the region to write into the sparse array. Must be greater than x0.
	 * @param y1 bottom y coordinate (not included) of the region to write into the sparse array. Must be greater than y0.
	 * @param src user buffer to fill. Must be at least sizeof(int32) * ( (y1 - y0 - 1) * src_line_stride + (x1 - x0 - 1) * src_col_stride + 1) bytes large.
	 * @param src_col_stride spacing (in elements, not in bytes) in x dimension between consecutive elements of the user buffer.
	 * @param src_line_stride spacing (in elements, not in bytes) in y dimension between consecutive elements of the user buffer.
	 * @param forgiving if set to TRUE and the region is invalid, true will still be returned.
	 * @return true in case of success.
	 */
	bool write(uint32_t x0,
			  uint32_t y0,
			  uint32_t x1,
			  uint32_t y1,
			  const int32_t* src,
			  const uint32_t src_col_stride,
			  const uint32_t src_line_stride,
			  bool forgiving);

	/** Allocate all blocks for a rectangular region into the sparse array from a
	 * user buffer.
	 *
	 * Blocks intersecting the region are allocated
	 *
	 * @param x0 left x coordinate of the region to write into the sparse array.
	 * @param y0 top x coordinate of the region to write into the sparse array.
	 * @param x1 right x coordinate (not included) of the region to write into the sparse array. Must be greater than x0.
	 * @param y1 bottom y coordinate (not included) of the region to write into the sparse array. Must be greater than y0.
	 * @return true in case of success.
	 */
	bool alloc(              uint32_t x0,
							  uint32_t y0,
							  uint32_t x1,
							  uint32_t y1);

private:

	/** Returns whether region bounds are valid (non empty and within array bounds)
	 * @param x0 left x coordinate of the region.
	 * @param y0 top x coordinate of the region.
	 * @param x1 right x coordinate (not included) of the region. Must be greater than x0.
	 * @param y1 bottom y coordinate (not included) of the region. Must be greater than y0.
	 * @return true or false.
	 */
	bool is_region_valid(	uint32_t x0,
							uint32_t y0,
							uint32_t x1,
							uint32_t y1);

	bool read_or_write(uint32_t x0,
						uint32_t y0,
						uint32_t x1,
						uint32_t y1,
						int32_t* buf,
						const uint32_t buf_col_stride,
						const uint32_t buf_line_stride,
						bool forgiving,
						bool is_read_op);

	uint32_t width;
    uint32_t height;
    uint32_t log2_block_width;
    uint32_t log2_block_height;
    uint32_t block_width;
    uint32_t block_height;
    uint32_t block_area;
    uint32_t block_count_hor;
    uint32_t block_count_ver;
    int32_t** data_blocks;
};

}

/*@}*/

