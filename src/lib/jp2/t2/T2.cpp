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
 * Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2014, Professor Benoit Macq
 * Copyright (c) 2001-2003, David Janssens
 * Copyright (c) 2002-2003, Yannick Verschueren
 * Copyright (c) 2003-2007, Francois-Olivier Devaux
 * Copyright (c) 2003-2014, Antonin Descampe
 * Copyright (c) 2005, Herve Drolon, FreeImage Team
 * Copyright (c) 2008, 2011-2012, Centre National d'Etudes Spatiales (CNES), FR
 * Copyright (c) 2012, CS Systemes d'Information, France
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
#include "grok_includes.h"
#include "testing.h"
#include <memory>

//#define DEBUG_ENCODE_PACKETS

namespace grk {

bool T2::encode_packets(uint16_t tile_no, uint32_t max_layers,
		BufferedStream *stream, uint32_t *p_data_written,
		grk_codestream_info *cstr_info, uint32_t tp_num, uint32_t tp_pos,
		uint32_t pino) {

	uint32_t nb_bytes = 0;
	auto cp = tileProcessor->m_cp;
	auto image = tileProcessor->image;
	auto p_tile = tileProcessor->tile;
	auto tcp = &cp->tcps[tile_no];
	uint32_t nb_pocs = tcp->numpocs + 1;

	auto pi = pi_initialise_encode(image, cp, tile_no, FINAL_PASS);
	if (!pi)
		return false;

	pi_init_encode(pi, cp, tile_no, pino, tp_num, tp_pos, FINAL_PASS);

	auto current_pi = &pi[pino];
	if (current_pi->poc.prg == GRK_PROG_UNKNOWN) {
		pi_destroy(pi, nb_pocs);
		GROK_ERROR("encode_packets: Unknown progression order");
		return false;
	}
	while (pi_next(current_pi)) {
		if (current_pi->layno < max_layers) {
			nb_bytes = 0;

			if (!encode_packet(tile_no, tcp, current_pi, stream, &nb_bytes,
					cstr_info)) {
				pi_destroy(pi, nb_pocs);
				return false;
			}
			*p_data_written += nb_bytes;

			/* INDEX >> */
			if (cstr_info) {
				if (cstr_info->index_write) {
					auto info_TL = &cstr_info->tile[tile_no];
					auto info_PK = &info_TL->packet[cstr_info->packno];
					if (!cstr_info->packno) {
						info_PK->start_pos = info_TL->end_header + 1;
					} else {
						info_PK->start_pos =
								((cp->m_coding_params.m_enc.m_tp_on | tcp->POC)
										&& info_PK->start_pos) ?
										info_PK->start_pos :
										info_TL->packet[cstr_info->packno - 1].end_pos
												+ 1;
					}
					info_PK->end_pos = info_PK->start_pos + nb_bytes - 1;
					info_PK->end_ph_pos += info_PK->start_pos - 1; /* End of packet header which now only represents the distance
					 to start of packet is incremented by value of start of packet*/
				}

				cstr_info->packno++;
			}
			/* << INDEX */
			++p_tile->packno;
		}
	}
	pi_destroy(pi, nb_pocs);

	return true;
}

bool T2::encode_packets_simulate(uint16_t tile_no, uint32_t max_layers,
		uint32_t *all_packets_len, uint32_t max_len, uint32_t tp_pos,
		PacketLengthMarkers *markers) {

	assert(all_packets_len);
	auto cp = tileProcessor->m_cp;
	auto image = tileProcessor->image;
	auto tcp = cp->tcps + tile_no;
	uint32_t pocno = (cp->rsiz == GRK_PROFILE_CINEMA_4K) ? 2 : 1;
	uint32_t max_comp =
			cp->m_coding_params.m_enc.m_max_comp_size > 0 ? image->numcomps : 1;
	uint32_t nb_pocs = tcp->numpocs + 1;

	auto pi = pi_initialise_encode(image, cp, tile_no, THRESH_CALC);
	if (!pi)
		return false;

	*all_packets_len = 0;
	auto current_pi = pi;

	tileProcessor->m_packetTracker.clear();
#ifdef DEBUG_ENCODE_PACKETS
    GROK_INFO("simulate encode packets for layers below layno %d", max_layers);
#endif
	for (uint32_t compno = 0; compno < max_comp; ++compno) {
		uint64_t comp_len = 0;
		current_pi = pi;

		for (uint32_t poc = 0; poc < pocno; ++poc) {
			uint32_t tp_num = compno;
			pi_init_encode(pi, cp, tile_no, poc, tp_num, tp_pos, THRESH_CALC);

			if (current_pi->poc.prg == GRK_PROG_UNKNOWN) {
				pi_destroy(pi, nb_pocs);
				GROK_ERROR(
						"decode_packets_simulate: Unknown progression order");
				return false;
			}
			while (pi_next(current_pi)) {
				if (current_pi->layno < max_layers) {
					uint32_t bytesInPacket = 0;

					if (!encode_packet_simulate(tcp, current_pi, &bytesInPacket,
							max_len, markers)) {
						pi_destroy(pi, nb_pocs);
						return false;
					}

					comp_len += bytesInPacket;
					max_len -= bytesInPacket;
					*all_packets_len += bytesInPacket;
				}
			}

			if (cp->m_coding_params.m_enc.m_max_comp_size) {
				if (comp_len > cp->m_coding_params.m_enc.m_max_comp_size) {
					pi_destroy(pi, nb_pocs);
					return false;
				}
			}

			++current_pi;
		}
	}
	pi_destroy(pi, nb_pocs);
	return true;
}
bool T2::decode_packets(uint16_t tile_no, ChunkBuffer *src_buf,
		uint64_t *p_data_read) {

	auto cp = tileProcessor->m_cp;
	auto image = tileProcessor->image;
	auto tcp = cp->tcps + tile_no;
	auto p_tile = tileProcessor->tile;
	uint32_t nb_pocs = tcp->numpocs + 1;
	auto pi = pi_create_decode(image, cp, tile_no);
	if (!pi)
		return false;

	auto packetLengths = tileProcessor->plt_markers;
	// we don't currently support PLM markers,
	// so we disable packet length markers if we have both PLT and PLM
	bool usePlt = packetLengths && !cp->plm_markers;
	if (usePlt)
		packetLengths->getInit();
	for (uint32_t pino = 0; pino <= tcp->numpocs; ++pino) {
		/* if the resolution needed is too low, one dim of the tilec
		 * could be equal to zero
		 * and no packets are used to decode this resolution and
		 * current_pi->resno is always >=
		 * tile->comps[current_pi->compno].minimum_num_resolutions
		 * and no l_img_comp->resno_decoded are computed
		 */
		bool *first_pass_failed = new bool[image->numcomps];
		for (size_t k = 0; k < image->numcomps; ++k)
			first_pass_failed[k] = true;

		auto current_pi = pi + pino;
		if (current_pi->poc.prg == GRK_PROG_UNKNOWN) {
			pi_destroy(pi, nb_pocs);
			delete[] first_pass_failed;
			GROK_ERROR("decode_packets: Unknown progression order");
			return false;
		}
		while (pi_next(current_pi)) {
			auto tilec = p_tile->comps + current_pi->compno;
			auto skip_the_packet = current_pi->layno
					>= tcp->num_layers_to_decode
					|| current_pi->resno >= tilec->minimum_num_resolutions;

			auto img_comp = image->comps + current_pi->compno;
			uint32_t pltMarkerLen = 0;
			if (usePlt)
				pltMarkerLen = packetLengths->getNext();

			/*
			 GROK_INFO(
			 "packet prg=%d cmptno=%02d rlvlno=%02d prcno=%03d layrno=%02d\n",
			 current_pi->poc.prg1, current_pi->compno,
			 current_pi->resno, current_pi->precno,
			 current_pi->layno);
			 */
			if (!skip_the_packet && !tilec->whole_tile_decoding) {
				skip_the_packet = true;
				auto res = tilec->resolutions + current_pi->resno;
				for (uint32_t bandno = 0;
						bandno < res->numbands && skip_the_packet; ++bandno) {
					auto band = res->bands + bandno;
					auto prec = band->precincts + current_pi->precno;
					if (tilec->is_subband_area_of_interest(current_pi->resno,
							band->bandno, prec->x0, prec->y0, prec->x1,
							prec->y1)) {
						skip_the_packet = false;
						break;
					}
				}
			}

			uint64_t nb_bytes_read = 0;
			if (!skip_the_packet) {
				/*
				 printf("packet cmptno=%02d rlvlno=%02d prcno=%03d layrno=%02d -> %s\n",
				 current_pi->compno, current_pi->resno,
				 current_pi->precno, current_pi->layno, skip_the_packet ? "skipped" : "kept");
				 */
				first_pass_failed[current_pi->compno] = false;

				if (!decode_packet(tcp, current_pi, src_buf, &nb_bytes_read)) {
					pi_destroy(pi, nb_pocs);
					delete[] first_pass_failed;
					return false;
				}

				img_comp->resno_decoded = std::max<uint32_t>(current_pi->resno,
						img_comp->resno_decoded);

			} else {
				if (pltMarkerLen) {
					nb_bytes_read = pltMarkerLen;
					src_buf->incr_cur_chunk_offset(nb_bytes_read);
				} else if (!skip_packet(tcp, current_pi, src_buf,
						&nb_bytes_read)) {
					pi_destroy(pi, nb_pocs);
					delete[] first_pass_failed;
					return false;
				}
			}

			if (first_pass_failed[current_pi->compno]) {
				img_comp = image->comps + current_pi->compno;
				if (img_comp->resno_decoded == 0) {
					img_comp->resno_decoded =
							p_tile->comps[current_pi->compno].minimum_num_resolutions
									- 1;
				}
			}
			//GROK_INFO("T2 Packet length: %d", nb_bytes_read);
			*p_data_read += nb_bytes_read;
		}
		delete[] first_pass_failed;
	}
	pi_destroy(pi, nb_pocs);
	return true;
}

T2::T2(TileProcessor *tileProc) :
		tileProcessor(tileProc) {
}

bool T2::decode_packet(TileCodingParams *p_tcp, PacketIter *p_pi, ChunkBuffer *src_buf,
		uint64_t *p_data_read) {
	uint64_t max_length = src_buf->data_len - src_buf->get_global_offset();
	if (max_length == 0) {
		GROK_WARN("decode_packet: No data for either packet header\n"
				"or packet body for packet prg=%d "
				"cmptno=%02d reslvlno=%02d prcno=%03d layrno=%02d",
		 p_pi->poc.prg1, p_pi->compno,
		 p_pi->resno, p_pi->precno,
		 p_pi->layno);
		return true;
	}
	auto p_tile = tileProcessor->tile;
	auto res = &p_tile->comps[p_pi->compno].resolutions[p_pi->resno];
	bool read_data;
	uint64_t nb_bytes_read = 0;
	uint64_t nb_total_bytes_read = 0;
	*p_data_read = 0;
	if (!read_packet_header(p_tcp, p_pi, &read_data, src_buf, &nb_bytes_read)) {
		return false;
	}
	nb_total_bytes_read += nb_bytes_read;

	/* we should read data for the packet */
	if (read_data) {
		nb_bytes_read = 0;
		if (!read_packet_data(res, p_pi, src_buf, &nb_bytes_read)) {
			return false;
		}
		nb_total_bytes_read += nb_bytes_read;
	}
	*p_data_read = nb_total_bytes_read;
	return true;
}

bool T2::read_packet_header(TileCodingParams *p_tcp, PacketIter *p_pi,
		bool *p_is_data_present, ChunkBuffer *src_buf, uint64_t *p_data_read) {
	auto p_tile = tileProcessor->tile;
	auto res = &p_tile->comps[p_pi->compno].resolutions[p_pi->resno];
	auto p_src_data = src_buf->get_global_ptr();
	uint64_t max_length = src_buf->data_len - src_buf->get_global_offset();
	uint64_t nb_code_blocks = 0;
	auto active_src = p_src_data;

	if (p_pi->layno == 0) {
		/* reset tagtrees */
		for (uint32_t bandno = 0; bandno < res->numbands; ++bandno) {
			auto band = res->bands + bandno;
			if (band->isEmpty())
				continue;
			auto prc = &band->precincts[p_pi->precno];
			if (!(p_pi->precno < (band->numPrecincts))) {
				GROK_ERROR("Invalid precinct");
				return false;
			}
			if (prc->incltree)
				prc->incltree->reset();
			if (prc->imsbtree)
				prc->imsbtree->reset();
			nb_code_blocks = (uint64_t) prc->cw * prc->ch;
			for (uint64_t cblkno = 0; cblkno < nb_code_blocks; ++cblkno) {
				auto cblk = prc->cblks.dec + cblkno;
				cblk->numSegments = 0;
			}
		}
	}

	/* SOP markers */
	if (p_tcp->csty & J2K_CP_CSTY_SOP) {
		if (max_length < 6) {
			GROK_WARN("Not enough space for expected SOP marker");
		} else if ((*active_src) != 0xff || (*(active_src + 1) != 0x91)) {
			GROK_WARN("Expected SOP marker");
		} else {
			uint16_t packno = (uint16_t) (((uint16_t) active_src[4] << 8)
					| active_src[5]);
			if (packno != (p_tile->packno % 0x10000)) {
				GROK_ERROR(
						"SOP marker packet counter %d does not match expected counter %d",
						packno, p_tile->packno);
				return false;
			}
			p_tile->packno++;
			active_src += 6;
		}
	}

	/*
	 When the marker PPT/PPM is used the packet header are store in PPT/PPM marker
	 This part deal with this characteristic
	 step 1: Read packet header in the saved structure
	 step 2: Return to code stream for decoding
	 */
	uint8_t *header_data = nullptr;
	uint8_t **header_data_start = nullptr;
	size_t *modified_length_ptr = nullptr;
	size_t remaining_length = 0;
	auto cp = tileProcessor->m_cp;
	if (cp->ppm) { /* PPM */
		header_data_start = &cp->ppm_data;
		header_data = *header_data_start;
		modified_length_ptr = &(cp->ppm_len);

	} else if (p_tcp->ppt) { /* PPT */
		header_data_start = &(p_tcp->ppt_data);
		header_data = *header_data_start;
		modified_length_ptr = &(p_tcp->ppt_len);
	} else { /* Normal Case */
		header_data_start = &(active_src);
		header_data = *header_data_start;
		remaining_length = (size_t) (p_src_data + max_length - header_data);
		modified_length_ptr = &(remaining_length);
	}

	uint32_t present = 0;
	std::unique_ptr<BitIO> bio(
			new BitIO(header_data, *modified_length_ptr, false));
	if (*modified_length_ptr) {
		if (!bio->read(&present, 1)) {
			GROK_ERROR("read_packet_header: failed to read `present` bit ");
			return false;
		}
	}
	//GROK_INFO("present=%d ", present);
	if (!present) {
		if (!bio->inalign())
			return false;
		header_data += bio->numbytes();

		/* EPH markers */
		if (p_tcp->csty & J2K_CP_CSTY_EPH) {
			if ((*modified_length_ptr
					- (size_t) (header_data - *header_data_start)) < 2U) {
				GROK_WARN("Not enough space for expected EPH marker");
			} else if ((*header_data) != 0xff || (*(header_data + 1) != 0x92)) {
				GROK_WARN("Expected EPH marker");
			} else {
				header_data += 2;
			}
		}

		auto header_length = (size_t) (header_data - *header_data_start);
		*modified_length_ptr -= header_length;
		*header_data_start += header_length;

		*p_is_data_present = false;
		*p_data_read = (size_t) (active_src - p_src_data);
		src_buf->incr_cur_chunk_offset(*p_data_read);
		return true;
	}
	for (uint32_t bandno = 0; bandno < res->numbands; ++bandno) {
		auto band = res->bands + bandno;
		if (band->isEmpty())
			continue;
		auto prc = band->precincts + p_pi->precno;
		nb_code_blocks = (uint64_t) prc->cw * prc->ch;
		for (uint64_t cblkno = 0; cblkno < nb_code_blocks; cblkno++) {
			uint32_t included = 0, increment = 0;
			auto cblk = prc->cblks.dec + cblkno;

			/* if cblk not yet included before --> inclusion tagtree */
			if (!cblk->numSegments) {
				uint64_t value;
				if (!prc->incltree->decodeValue(bio.get(), cblkno,
						p_pi->layno + 1, &value)) {
					GROK_ERROR(
							"read_packet_header: failed to read `inclusion` bit ");
					return false;
				}
				if (value != tag_tree_uninitialized_node_value
						&& value != p_pi->layno) {
					std::string msg =
							"Illegal inclusion tag tree found when decoding packet header.\n";
					msg +=
							"This problem can occur if empty packets are used (i.e., packets whose first header\n";
					msg +=
							"bit is 0) and the value coded by the inclusion tag tree in a subsequent packet\n";
					msg +=
							"is not exactly equal to the index of the quality layer in which each code-block\n";
					msg +=
							"makes its first contribution.  Such an error may occur from a\n";
					msg +=
							"mis-interpretation of the standard.  The problem may also occur as a result of\n";
					msg += "a corrupted code-stream\n";
					GROK_WARN("%s", msg.c_str());

				}
#ifdef DEBUG_LOSSLESS_T2
				 cblk->included = value;
#endif
				included = (value <= p_pi->layno) ? 1 : 0;
			}
			/* else one bit */
			else {
				if (!bio->read(&included, 1)) {
					GROK_ERROR(
							"read_packet_header: failed to read `inclusion` bit ");
					return false;
				}

#ifdef DEBUG_LOSSLESS_T2
				 cblk->included = included;
#endif
			}

			/* if cblk not included */
			if (!included) {
				cblk->numPassesInPacket = 0;
				//GROK_INFO("included=%d ", included);
				continue;
			}

			/* if cblk not yet included --> zero-bitplane tagtree */
			if (!cblk->numSegments) {
				uint32_t K_msbs = 0;
				uint8_t value;
				bool rc = true;

				// see Taubman + Marcellin page 388
				// loop below stops at (# of missing bit planes  + 1)
				while ((rc = prc->imsbtree->decompress(bio.get(), cblkno,
						K_msbs, &value)) && !value) {
					++K_msbs;
				}
				assert(K_msbs >= 1);
				K_msbs--;

				if (!rc) {
					GROK_ERROR("Failed to decompress zero-bitplane tag tree ");
					return false;
				}

				if (K_msbs > band->numbps) {
					GROK_WARN(
							"More missing bit planes (%d) than band bit planes (%d).",
							K_msbs, band->numbps);
					cblk->numbps = band->numbps;
				} else {
					cblk->numbps = band->numbps - K_msbs;
				}
				// BIBO analysis gives sanity check on number of bit planes
				if (cblk->numbps
						> max_precision_jpeg_2000 + GRK_J2K_MAXRLVLS * 5) {
					GROK_WARN("Number of bit planes %u is impossibly large.",
							cblk->numbps);
					return false;
				}
				cblk->numlenbits = 3;
			}

			/* number of coding passes */
			if (!bio->getnumpasses(&cblk->numPassesInPacket)) {
				GROK_ERROR("read_packet_header: failed to read numpasses.");
				return false;
			}
			if (!bio->getcommacode(&increment)) {
				GROK_ERROR(
						"read_packet_header: failed to read length indicator increment.");
				return false;
			}

			/* length indicator increment */
			cblk->numlenbits += increment;
			uint32_t segno = 0;

			if (!cblk->numSegments) {
				if (!T2::init_seg(cblk, segno,
						p_tcp->tccps[p_pi->compno].cblk_sty, true)) {
					return false;
				}
			} else {
				segno = cblk->numSegments - 1;
				if (cblk->segs[segno].numpasses
						== cblk->segs[segno].maxpasses) {
					++segno;
					if (!T2::init_seg(cblk, segno,
							p_tcp->tccps[p_pi->compno].cblk_sty, false)) {
						return false;
					}
				}
			}
			auto blockPassesInPacket = (int32_t) cblk->numPassesInPacket;
			do {
				auto seg = cblk->segs + segno;
				/* sanity check when there is no mode switch */
				if (seg->maxpasses == max_passes_per_segment) {
					if (blockPassesInPacket
							> (int32_t) max_passes_per_segment) {
						GROK_WARN(
								"Number of code block passes (%d) in packet is suspiciously large.",
								blockPassesInPacket);
						// ToDO - we are truncating the number of passes at an arbitrary value of
						// max_passes_per_segment. We should probably either skip the rest of this
						// block, if possible, or do further sanity check on packet
						seg->numPassesInPacket = max_passes_per_segment;
					} else {
						seg->numPassesInPacket = (uint32_t) blockPassesInPacket;
					}

				} else {
					assert(seg->maxpasses >= seg->numpasses);
					seg->numPassesInPacket = (uint32_t) std::min<int32_t>(
							(int32_t) (seg->maxpasses - seg->numpasses),
							blockPassesInPacket);
				}
				uint32_t bits_to_read = cblk->numlenbits
						+ uint_floorlog2(seg->numPassesInPacket);
				if (bits_to_read > 32) {
					GROK_ERROR(
							"read_packet_header: too many bits in segment length ");
					return false;
				}
				if (!bio->read(&seg->numBytesInPacket, bits_to_read)) {
					GROK_WARN(
							"read_packet_header: failed to read segment length ");
				}
#ifdef DEBUG_LOSSLESS_T2
			 cblk->packet_length_info->push_back(grk_packet_length_info(seg->numBytesInPacket,
							 cblk->numlenbits + uint_floorlog2(seg->numPassesInPacket)));
#endif
				/*
				 GROK_INFO(
				 "included=%d numPassesInPacket=%d increment=%d len=%d ",
				 included, seg->numPassesInPacket, increment,
				 seg->newlen);
				 */
				blockPassesInPacket -= (int32_t) seg->numPassesInPacket;
				if (blockPassesInPacket > 0) {
					++segno;
					if (!T2::init_seg(cblk, segno,
							p_tcp->tccps[p_pi->compno].cblk_sty, false)) {
						return false;
					}
				}
			} while (blockPassesInPacket > 0);
		}
	}

	if (!bio->inalign()) {
		GROK_ERROR("Unable to read packet header");
		return false;
	}

	header_data += bio->numbytes();

	/* EPH markers */
	if (p_tcp->csty & J2K_CP_CSTY_EPH) {
		if ((*modified_length_ptr
				- (uint32_t) (header_data - *header_data_start)) < 2U) {
			GROK_WARN("Not enough space for expected EPH marker");
		} else if ((*header_data) != 0xff || (*(header_data + 1) != 0x92)) {
			GROK_WARN("Expected EPH marker");
		} else {
			header_data += 2;
		}
	}

	auto header_length = (size_t) (header_data - *header_data_start);
	//GROK_INFO("hdrlen=%d ", header_length);
	//GROK_INFO("packet body\n");
	*modified_length_ptr -= header_length;
	*header_data_start += header_length;
	*p_is_data_present = true;
	*p_data_read = (uint32_t) (active_src - p_src_data);
	src_buf->incr_cur_chunk_offset(*p_data_read);

	return true;
}

bool T2::read_packet_data(grk_resolution *res, PacketIter *p_pi,
		ChunkBuffer *src_buf, uint64_t *p_data_read) {
	for (uint32_t bandno = 0; bandno < res->numbands; ++bandno) {
		auto band = res->bands + bandno;
		auto prc = &band->precincts[p_pi->precno];
		uint64_t nb_code_blocks = (uint64_t) prc->cw * prc->ch;
		for (uint64_t cblkno = 0; cblkno < nb_code_blocks; ++cblkno) {
			auto cblk = prc->cblks.dec + cblkno;
			if (!cblk->numPassesInPacket) {
				++cblk;
				continue;
			}
			grk_seg *seg = nullptr;
			if (!cblk->numSegments) {
				seg = cblk->segs;
				++cblk->numSegments;
				cblk->compressedDataSize = 0;
			} else {
				seg = &cblk->segs[cblk->numSegments - 1];
				if (seg->numpasses == seg->maxpasses) {
					++seg;
					++cblk->numSegments;
				}
			}

			uint32_t numPassesInPacket = cblk->numPassesInPacket;
			do {
				size_t offset = (size_t) src_buf->get_global_offset();
				size_t len = src_buf->data_len;
				// Check possible overflow on segment length
				if (((offset + seg->numBytesInPacket) > len)) {
					GROK_WARN(
							"read packet data: segment offset (%u) plus segment length %u\n"
							"is greater than total length \n"
							"of all segments (%u) for codeblock "
							"%d (layer=%d, prec=%d, band=%d, res=%d, comp=%d)."
							" Truncating packet data.", offset,
							seg->numBytesInPacket, len, cblkno, p_pi->layno,
							p_pi->precno, bandno, p_pi->resno, p_pi->compno);
					seg->numBytesInPacket = (uint32_t) (len - offset);
				}
				//initialize dataindex to current contiguous size of code block
				if (seg->numpasses == 0)
					seg->dataindex = (uint32_t) cblk->compressedDataSize;

				// only add segment to seg_buffers if length is greater than zero
				if (seg->numBytesInPacket) {
					cblk->seg_buffers.push_back(src_buf->get_global_ptr(),
							seg->numBytesInPacket);
					*(p_data_read) += seg->numBytesInPacket;
					src_buf->incr_cur_chunk_offset(seg->numBytesInPacket);
					cblk->compressedDataSize += seg->numBytesInPacket;
					seg->len += seg->numBytesInPacket;
				}
				seg->numpasses += seg->numPassesInPacket;
				numPassesInPacket -= seg->numPassesInPacket;
				if (numPassesInPacket > 0) {
					++seg;
					++cblk->numSegments;
				}
			} while (numPassesInPacket > 0);
		} /* next code_block */
	}

	return true;
}
bool T2::skip_packet(TileCodingParams *p_tcp, PacketIter *p_pi, ChunkBuffer *src_buf,
		uint64_t *p_data_read) {
	bool read_data;
	uint64_t nb_bytes_read = 0;
	uint64_t nb_totabytes_read = 0;
	uint64_t max_length = (uint64_t) src_buf->get_cur_chunk_len();
	auto p_tile = tileProcessor->tile;

	*p_data_read = 0;
	if (!read_packet_header(p_tcp, p_pi, &read_data, src_buf, &nb_bytes_read))
		return false;
	nb_totabytes_read += nb_bytes_read;
	max_length -= nb_bytes_read;

	/* we should read data for the packet */
	if (read_data) {
		nb_bytes_read = 0;
		if (!skip_packet_data(
				&p_tile->comps[p_pi->compno].resolutions[p_pi->resno], p_pi,
				&nb_bytes_read, max_length)) {
			return false;
		}
		src_buf->incr_cur_chunk_offset(nb_bytes_read);
		nb_totabytes_read += nb_bytes_read;
	}
	*p_data_read = nb_totabytes_read;

	return true;
}

bool T2::skip_packet_data(grk_resolution *res, PacketIter *p_pi,
		uint64_t *p_data_read, uint64_t max_length) {
	*p_data_read = 0;
	for (uint32_t bandno = 0; bandno < res->numbands; ++bandno) {
		auto band = res->bands + bandno;
		if (band->isEmpty())
			continue;

		auto prc = &band->precincts[p_pi->precno];
		uint64_t nb_code_blocks = (uint64_t) prc->cw * prc->ch;
		for (uint64_t cblkno = 0; cblkno < nb_code_blocks; ++cblkno) {
			auto cblk = prc->cblks.dec + cblkno;
			if (!cblk->numPassesInPacket) {
				/* nothing to do */
				++cblk;
				continue;
			}
			grk_seg *seg = nullptr;
			if (!cblk->numSegments) {
				seg = cblk->segs;
				++cblk->numSegments;
				cblk->compressedDataSize = 0;
			} else {
				seg = &cblk->segs[cblk->numSegments - 1];
				if (seg->numpasses == seg->maxpasses) {
					++seg;
					++cblk->numSegments;
				}
			}
			uint32_t numPassesInPacket = cblk->numPassesInPacket;
			do {
				/* Check possible overflow then size */
				if (((*p_data_read + seg->numBytesInPacket) < (*p_data_read))
						|| ((*p_data_read + seg->numBytesInPacket) > max_length)) {
					GROK_ERROR(
							"skip: segment too long (%d) with max (%d) for codeblock %d (p=%d, b=%d, r=%d, c=%d)",
							seg->numBytesInPacket, max_length, cblkno,
							p_pi->precno, bandno, p_pi->resno, p_pi->compno);
					return false;
				}

				//GROK_INFO( "skip packet: p_data_read = %d, bytes in packet =  %d ",
				//		*p_data_read, seg->numBytesInPacket);
				*(p_data_read) += seg->numBytesInPacket;
				seg->numpasses += seg->numPassesInPacket;
				numPassesInPacket -= seg->numPassesInPacket;
				if (numPassesInPacket > 0) {
					++seg;
					++cblk->numSegments;
				}
			} while (numPassesInPacket > 0);
		}
	}
	return true;
}

bool T2::init_seg(grk_cblk_dec *cblk, uint32_t index, uint8_t cblk_sty,
		bool first) {
	uint32_t nb_segs = index + 1;

	if (nb_segs > cblk->numSegmentsAllocated) {
		auto new_segs = new grk_seg[cblk->numSegmentsAllocated
				+ cblk->numSegmentsAllocated];
		for (uint32_t i = 0; i < cblk->numSegmentsAllocated; ++i)
			new_segs[i] = cblk->segs[i];
		cblk->numSegmentsAllocated += default_numbers_segments;
		if (cblk->segs)
			delete[] cblk->segs;
		cblk->segs = new_segs;
	}

	auto seg = &cblk->segs[index];
	seg->clear();

	if (cblk_sty & GRK_CBLKSTY_TERMALL) {
		seg->maxpasses = 1;
	} else if (cblk_sty & GRK_CBLKSTY_LAZY) {
		if (first) {
			seg->maxpasses = 10;
		} else {
			auto last_seg = seg - 1;
			seg->maxpasses =
					((last_seg->maxpasses == 1) || (last_seg->maxpasses == 10)) ?
							2 : 1;
		}
	} else {
		seg->maxpasses = max_passes_per_segment;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------
bool T2::encode_packet(uint16_t tileno, TileCodingParams *tcp, PacketIter *pi,
		BufferedStream *stream, uint32_t *packet_bytes_written,
		grk_codestream_info *cstr_info) {
	assert(stream);
	uint32_t compno = pi->compno;
	uint32_t resno = pi->resno;
	uint64_t precno = pi->precno;
	uint32_t layno = pi->layno;
	auto tile = tileProcessor->tile;
	auto tilec = &tile->comps[compno];
	auto res = &tilec->resolutions[resno];
	size_t stream_start = stream->tell();

	if (tileProcessor->m_packetTracker.is_packet_encoded(compno, resno, precno,
			layno))
		return true;
	tileProcessor->m_packetTracker.packet_encoded(compno, resno, precno, layno);

#ifdef DEBUG_ENCODE_PACKETS
    GROK_INFO("encode packet compono=%d, resno=%d, precno=%d, layno=%d",
             compno, resno, precno, layno);
#endif

	// SOP marker
	if (tcp->csty & J2K_CP_CSTY_SOP) {
		if (!stream->write_byte(255))
			return false;
		if (!stream->write_byte(145))
			return false;
		if (!stream->write_byte(0))
			return false;
		if (!stream->write_byte(4))
			return false;
		/* packno is uint32_t modulo 65536, in big endian format */
		uint16_t packno = (uint16_t) (tile->packno % 0x10000);
		if (!stream->write_byte((uint8_t) (packno >> 8)))
			return false;
		if (!stream->write_byte((uint8_t) (packno & 0xff)))
			return false;
	}

	// initialize precinct and code blocks if this is the first layer
	if (!layno) {
		for (uint32_t bandno = 0; bandno < res->numbands; ++bandno) {
			auto band = res->bands + bandno;
			auto prc = band->precincts + precno;
			uint64_t nb_blocks = (uint64_t)prc->cw * prc->ch;

			if (band->isEmpty() || !nb_blocks) {
				band++;
				continue;
			}
			if (prc->incltree)
				prc->incltree->reset();
			if (prc->imsbtree)
				prc->imsbtree->reset();
			for (uint64_t cblkno = 0; cblkno < nb_blocks; ++cblkno) {
				auto cblk = prc->cblks.enc + cblkno;
				cblk->numPassesInPacket = 0;
				assert(band->numbps >= cblk->numbps);
				if (band->numbps < cblk->numbps) {
					GROK_WARN(
							"Code block %d bps greater than band bps. Skipping.",
							cblkno);
				} else {
					prc->imsbtree->setvalue(cblkno,
							(int64_t) (band->numbps - cblk->numbps));
				}
			}
		}
	}

	std::unique_ptr<BitIO> bio(new BitIO(stream, true));
	// Empty header bit. Grok always sets this to 1,
	// even though there is also an option to set it to zero.
	if (!bio->write(1, 1))
		return false;

	/* Writing Packet header */
	for (uint32_t bandno = 0; bandno < res->numbands; ++bandno) {
		auto band = res->bands + bandno;
		auto prc = band->precincts + precno;
		uint64_t nb_blocks = (uint64_t)prc->cw * prc->ch;

		if (band->isEmpty() || !nb_blocks) {
			band++;
			continue;
		}

		for (uint64_t cblkno = 0; cblkno < nb_blocks; ++cblkno) {
			auto cblk = prc->cblks.enc + cblkno;
			auto layer = cblk->layers + layno;

			if (!cblk->numPassesInPacket
					&& layer->numpasses) {
				prc->incltree->setvalue(cblkno, (int32_t) layno);
			}
		}

		auto cblk = prc->cblks.enc;
		for (uint64_t cblkno = 0; cblkno < nb_blocks; cblkno++) {
			auto layer = cblk->layers + layno;
			uint32_t increment = 0;
			uint32_t nump = 0;
			uint32_t len = 0;

			/* cblk inclusion bits */
			if (!cblk->numPassesInPacket) {
				bool rc = prc->incltree->compress(bio.get(), cblkno,
						(int32_t) (layno + 1));
				assert(rc);
				if (!rc)
				   return false;
#ifdef DEBUG_LOSSLESS_T2
					cblk->included = layno;
#endif
			} else {
#ifdef DEBUG_LOSSLESS_T2
					cblk->included = layer->numpasses != 0 ? 1 : 0;
#endif
				if (!bio->write(layer->numpasses != 0, 1))
					return false;
			}

			/* if cblk not included, go to next cblk  */
			if (!layer->numpasses) {
				++cblk;
				continue;
			}

			/* if first instance of cblk --> zero bit-planes information */
			if (!cblk->numPassesInPacket) {
				cblk->numlenbits = 3;
				bool rc = prc->imsbtree->compress(bio.get(), cblkno,
						tag_tree_uninitialized_node_value);
				assert(rc);
				if (!rc)
					return false;
			}
			/* number of coding passes included */
			bio->putnumpasses(layer->numpasses);
			uint32_t nb_passes = cblk->numPassesInPacket
					+ layer->numpasses;
			auto pass = cblk->passes
					+ cblk->numPassesInPacket;

			/* computation of the increase of the length indicator and insertion in the header     */
			for (uint32_t passno = cblk->numPassesInPacket;
					passno < nb_passes; ++passno) {
				++nump;
				len += pass->len;

				if (pass->term || passno == nb_passes - 1) {
					increment = (uint32_t) std::max<int32_t>(
							(int32_t) increment,
							int_floorlog2(len) + 1
									- ((int32_t) cblk->numlenbits
											+ int_floorlog2(nump)));
					len = 0;
					nump = 0;
				}
				++pass;
			}
			bio->putcommacode((int32_t) increment);

			/* computation of the new Length indicator */
			cblk->numlenbits += increment;

			pass = cblk->passes + cblk->numPassesInPacket;
			/* insertion of the codeword segment length */
			for (uint32_t passno = cblk->numPassesInPacket;
					passno < nb_passes; ++passno) {
				nump++;
				len += pass->len;

				if (pass->term || passno == nb_passes - 1) {
#ifdef DEBUG_LOSSLESS_T2
						cblk->packet_length_info->push_back(grk_packet_length_info(len, cblk->numlenbits + (uint32_t)int_floorlog2((int32_t)nump)));
#endif
					if (!bio->write(len,
							cblk->numlenbits + (uint32_t) int_floorlog2(nump)))
						return false;
					len = 0;
					nump = 0;
				}
				++pass;
			}
			++cblk;
		}
	}

	if (!bio->flush()) {
		GROK_ERROR("encode_packet: Bit IO flush failed while encoding packet");
		return false;
	}

	// EPH marker
	if (tcp->csty & J2K_CP_CSTY_EPH) {
		if (!stream->write_byte(255))
			return false;
		if (!stream->write_byte(146))
			return false;
	}

	/* << INDEX */
	/* End of packet header position. Currently only represents the distance to start of packet
	 Will be updated later by incrementing with packet start value*/
	//if (cstr_info && cstr_info->index_write) {
	//	 grk_packet_info  *info_PK = &cstr_info->tile[tileno].packet[cstr_info->packno];
	//	info_PK->end_ph_pos = (int64_t)(active_dest - dest);
	//}
	/* INDEX >> */

	/* Writing the packet body */
	for (uint32_t bandno = 0; bandno < res->numbands; bandno++) {
		auto band = res->bands + bandno;
		auto prc = band->precincts + precno;
		uint64_t nb_blocks = (uint64_t)prc->cw * prc->ch;

		if (band->isEmpty() || !nb_blocks) {
			band++;
			continue;
		}

		auto cblk = prc->cblks.enc;
		for (uint64_t cblkno = 0; cblkno < nb_blocks; ++cblkno) {
			auto cblk_layer = cblk->layers + layno;
			if (!cblk_layer->numpasses) {
				++cblk;
				continue;
			}

			if (cblk_layer->len) {
				if (!stream->write_bytes(cblk_layer->data, cblk_layer->len))
					return false;
			}
			cblk->numPassesInPacket += cblk_layer->numpasses;
			if (cstr_info && cstr_info->index_write) {
				grk_packet_info *info_PK =
						&cstr_info->tile[tileno].packet[cstr_info->packno];
				info_PK->disto += cblk_layer->disto;
				if (cstr_info->D_max < info_PK->disto) {
					cstr_info->D_max = info_PK->disto;
				}
			}
			++cblk;
		}
	}
	*packet_bytes_written += (uint32_t)(stream->tell() - stream_start);

#ifdef DEBUG_LOSSLESS_T2
		auto originalDataBytes = *packet_bytes_written - numHeaderBytes;
		auto roundRes = &tilec->round_trip_resolutions[resno];
		size_t nb_bytes_read = 0;
		auto src_buf = std::unique_ptr<ChunkBuffer>(new ChunkBuffer());
		seg_buf_push_back(src_buf.get(), dest, *packet_bytes_written);

		bool rc = true;
		bool read_data;
		if (!T2::read_packet_header(p_t2,
			roundRes,
			tcp,
			pi,
			&read_data,
			src_buf.get(),
			&nb_bytes_read)) {
			rc = false;
		}
		if (rc) {

			// compare size of header
			if (numHeaderBytes != nb_bytes_read) {
				printf("encode_packet: round trip header bytes %u differs from original %u\n", (uint32_t)l_nb_bytes_read, (uint32_t)numHeaderBytes);
			}
			for (uint32_t bandno = 0; bandno < res->numbands; ++bandno) {
				auto band = res->bands + bandno;
				auto roundTripBand = roundRes->bands + bandno;
				if (!band->precincts)
					continue;
				for (uint64_t precno = 0; precno < band->numPrecincts; ++precno) {
					auto prec = band->precincts + precno;
					auto roundTripPrec = roundTripBand->precincts + precno;
					for (uint64_t cblkno = 0; cblkno < (uint64_t)prec->cw * prec->ch; ++cblkno) {
						auto originalCblk = prec->cblks.enc + cblkno;
						grk_layer *layer = originalCblk->layers + layno;
						if (!layer->numpasses)
							continue;

						// compare number of passes
						auto roundTripCblk = roundTripPrec->cblks.dec + cblkno;
						if (roundTripCblk->numPassesInPacket != layer->numpasses) {
							printf("encode_packet: round trip layer numpasses %d differs from original num passes %d at layer %d, component %d, band %d, precinct %d, resolution %d\n",
								roundTripCblk->numPassesInPacket,
								layer->numpasses,
								layno,
								compno,
								bandno,
								(uint32_t)precno,
								pi->resno);

						}
						// compare number of bit planes
						if (roundTripCblk->numbps != originalCblk->numbps) {
							printf("encode_packet: round trip numbps %d differs from original %d\n", roundTripCblk->numbps, originalCblk->numbps);
						}

						// compare number of length bits
						if (roundTripCblk->numlenbits != originalCblk->numlenbits) {
							printf("encode_packet: round trip numlenbits %u differs from original %u\n", roundTripCblk->numlenbits, originalCblk->numlenbits);
						}

						// compare inclusion
						if (roundTripCblk->included != originalCblk->included) {
							printf("encode_packet: round trip inclusion %d differs from original inclusion %d at layer %d, component %d, band %d, precinct %d, resolution %d\n",
								roundTripCblk->included,
								originalCblk->included,
								layno,
								compno,
								bandno,
								(uint32_t)precno,
								pi->resno);
						}

						// compare lengths
						if (roundTripCblk->packet_length_info->size() != originalCblk->packet_length_info->size()) {
							printf("encode_packet: round trip length size %d differs from original %d at layer %d, component %d, band %d, precinct %d, resolution %d\n",
								(uint32_t)roundTripCblk->packet_length_info->size(),
								(uint32_t)originalCblk->packet_length_info->size(),
								layno,
								compno,
								bandno,
								(uint32_t)precno,
								pi->resno);
						} else {
							for (uint32_t i = 0; i < roundTripCblk->packet_length_info->size(); ++i) {
								auto roundTrip = roundTripCblk->packet_length_info->operator[](i);
								auto original = originalCblk->packet_length_info->operator[](i);
								if (!(roundTrip ==original)) {
									printf("encode_packet: round trip length size %d differs from original %d at layer %d, component %d, band %d, precinct %d, resolution %d\n",
										roundTrip.len,
										original.len,
										layno,
										compno,
										bandno,
										(uint32_t)precno,
										pi->resno);
								}
							}

						
						}
					}
				}
			}
			/* we should read data for the packet */
			if (read_data) {
			 nb_bytes_read = 0;
				if (!T2::read_packet_data(roundRes,
					pi,
					src_buf.get(),
					&nb_bytes_read)) {
					rc = false;
				}
				else {
					if (originalDataBytes != nb_bytes_read) {
						printf("encode_packet: round trip data bytes %u differs from original %u\n", (uint32_t)l_nb_bytes_read, (uint32_t)originalDataBytes);
					}

					for (uint32_t bandno = 0; bandno < res->numbands; ++bandno) {
						auto band = res->bands + bandno;
						auto roundTripBand = roundRes->bands + bandno;
						if (!band->precincts)
							continue;
						for (uint64_t precno = 0; precno < band->numPrecincts; ++precno) {
							auto prec = band->precincts + precno;
							auto roundTripPrec = roundTripBand->precincts + precno;
							for (uint32_t cblkno = 0; cblkno < (uint64_t)prec->cw * prec->ch; ++cblkno) {
								auto originalCblk = prec->cblks.enc + cblkno;
								grk_layer *layer = originalCblk->layers + layno;
								if (!layer->numpasses)
									continue;

								// compare cumulative length
								uint32_t originalCumulativeLayerLength = 0;
								for (uint32_t i = 0; i <= layno; ++i) {
									auto lay = originalCblk->layers + i;
									if (lay->numpasses)
										originalCumulativeLayerLength += lay->len;
								}
								auto roundTripCblk = roundTripPrec->cblks.dec + cblkno;
								uint16_t roundTripTotalSegLen = min_buf_vec_get_len(&roundTripCblk->seg_buffers);
								if (roundTripTotalSegLen != originalCumulativeLayerLength) {
									printf("encode_packet: layer %d: round trip segment length %d differs from original %d\n", layno, roundTripTotalSegLen, originalCumulativeLayerLength);
								}

								// compare individual data points
								if (roundTripCblk->numSegments && roundTripTotalSegLen) {
									uint8_t* roundTripData = nullptr;
									bool needs_delete = false;
									/* if there is only one segment, then it is already contiguous, so no need to make a copy*/
									if (roundTripTotalSegLen == 1 && roundTripCblk->seg_buffers.get(0)) {
										roundTripData = ((grk_buf*)(roundTripCblk->seg_buffers.get(0)))->buf;
									}
									else {
										needs_delete = true;
										roundTripData = new uint8_t[roundTripTotalSegLen];
										min_buf_vec_copy_to_contiguous_buffer(&roundTripCblk->seg_buffers, roundTripData);
									}
									for (uint32_t i = 0; i < originalCumulativeLayerLength; ++i) {
										if (roundTripData[i] != originalCblk->data[i]) {
											printf("encode_packet: layer %d: round trip data %x differs from original %x\n", layno, roundTripData[i], originalCblk->data[i]);
										}
									}
									if (needs_delete)
										delete[] roundTripData;
								}

							}
						}
					}
				}
			}
		}
		else {
			GROK_ERROR("encode_packet: decompress packet failed");
		}
#endif
	return true;
}

bool T2::encode_packet_simulate(TileCodingParams *tcp, PacketIter *pi,
		uint32_t *packet_bytes_written, uint32_t max_bytes_available,
		PacketLengthMarkers *markers) {
	uint32_t compno = pi->compno;
	uint32_t resno = pi->resno;
	uint64_t precno = pi->precno;
	uint32_t layno = pi->layno;
	uint64_t nb_blocks;

	auto tile = tileProcessor->tile;
	auto tilec = tile->comps + compno;
	auto res = tilec->resolutions + resno;
	*packet_bytes_written = 0;

	if (tileProcessor->m_packetTracker.is_packet_encoded(compno, resno, precno,
			layno))
		return true;
	tileProcessor->m_packetTracker.packet_encoded(compno, resno, precno, layno);

#ifdef DEBUG_ENCODE_PACKETS
    GROK_INFO("simulate encode packet compono=%d, resno=%d, precno=%d, layno=%d",
             compno, resno, precno, layno);
#endif

	/* <SOP 0xff91> */
	if (tcp->csty & J2K_CP_CSTY_SOP) {
		max_bytes_available -= 6;
		*packet_bytes_written += 6;
	}
	/* </SOP> */

	if (!layno) {
		for (uint32_t bandno = 0; bandno < res->numbands; ++bandno) {
			auto band = res->bands + bandno;
			auto prc = band->precincts + precno;

			if (prc->incltree)
				prc->incltree->reset();
			if (prc->imsbtree)
				prc->imsbtree->reset();

			nb_blocks = (uint64_t)prc->cw * prc->ch;
			for (uint64_t cblkno = 0; cblkno < nb_blocks; ++cblkno) {
				auto cblk = prc->cblks.enc + cblkno;
				cblk->numPassesInPacket = 0;
				if (band->numbps < cblk->numbps) {
					GROK_WARN(
							"Code block %d bps greater than band bps. Skipping.",
							cblkno);
				} else {
					prc->imsbtree->setvalue(cblkno,
							(int64_t) (band->numbps - cblk->numbps));
				}
			}
		}
	}

	std::unique_ptr<BitIO> bio(new BitIO(0, max_bytes_available, true));
	bio->simulateOutput(true);
	/* Empty header bit */
	if (!bio->write(1, 1))
		return false;

	/* Writing Packet header */
	for (uint32_t bandno = 0; bandno < res->numbands; ++bandno) {
		auto band = res->bands + bandno;
		auto prc = band->precincts + precno;

		nb_blocks = (uint64_t)prc->cw * prc->ch;
		for (uint64_t cblkno = 0; cblkno < nb_blocks; ++cblkno) {
			auto cblk = prc->cblks.enc + cblkno;
			auto layer = cblk->layers + layno;
			if (!cblk->numPassesInPacket
					&& layer->numpasses) {
				prc->incltree->setvalue(cblkno, (int32_t) layno);
			}
		}
		for (uint64_t cblkno = 0; cblkno < nb_blocks; cblkno++) {
			auto cblk = prc->cblks.enc + cblkno;
			auto layer = cblk->layers + layno;
			uint32_t increment = 0;
			uint32_t nump = 0;
			uint32_t len = 0, passno;
			uint32_t nb_passes;

			/* cblk inclusion bits */
			if (!cblk->numPassesInPacket) {
				if (!prc->incltree->compress(bio.get(), cblkno,
						(int32_t) (layno + 1)))
					return false;
			} else {
				if (!bio->write(layer->numpasses != 0, 1))
					return false;
			}

			/* if cblk not included, go to the next cblk  */
			if (!layer->numpasses)
				continue;

			/* if first instance of cblk --> zero bit-planes information */
			if (!cblk->numPassesInPacket) {
				cblk->numlenbits = 3;
				if (!prc->imsbtree->compress(bio.get(), cblkno,
						tag_tree_uninitialized_node_value))
					return false;
			}

			/* number of coding passes included */
			bio->putnumpasses(layer->numpasses);
			nb_passes = cblk->numPassesInPacket
					+ layer->numpasses;
			/* computation of the increase of the length indicator and insertion in the header     */
			for (passno = cblk->numPassesInPacket;
					passno < nb_passes; ++passno) {
				auto pass =
						cblk->passes + cblk->numPassesInPacket + passno;
				++nump;
				len += pass->len;

				if (pass->term
						|| passno
								== (cblk->numPassesInPacket
										+ layer->numpasses) - 1) {
					increment = (uint32_t) std::max<int32_t>(
							(int32_t) increment,
							int_floorlog2(len) + 1
									- ((int32_t) cblk->numlenbits
											+ int_floorlog2(nump)));
					len = 0;
					nump = 0;
				}
			}
			bio->putcommacode((int32_t) increment);

			/* computation of the new Length indicator */
			cblk->numlenbits += increment;
			/* insertion of the codeword segment length */
			for (passno = cblk->numPassesInPacket;
					passno < nb_passes; ++passno) {
				auto pass =
						cblk->passes + cblk->numPassesInPacket + passno;
				nump++;
				len += pass->len;
				if (pass->term
						|| passno
								== (cblk->numPassesInPacket
										+ layer->numpasses) - 1) {
					if (!bio->write(len,
							cblk->numlenbits + (uint32_t) int_floorlog2(nump)))
						return false;
					len = 0;
					nump = 0;
				}
			}
		}
	}

	if (!bio->flush())
		return false;

	*packet_bytes_written += (uint32_t) bio->numbytes();
	max_bytes_available -= (uint32_t) bio->numbytes();

	/* <EPH 0xff92> */
	if (tcp->csty & J2K_CP_CSTY_EPH) {
		max_bytes_available -= 2;
		*packet_bytes_written += 2;
	}
	/* </EPH> */

	/* Writing the packet body */
	for (uint32_t bandno = 0; bandno < res->numbands; bandno++) {
		auto band = res->bands + bandno;
		auto prc = band->precincts + precno;

		nb_blocks = (uint64_t)prc->cw * prc->ch;
		for (uint64_t cblkno = 0; cblkno < nb_blocks; ++cblkno) {
			auto cblk = prc->cblks.enc + cblkno;
			auto layer = cblk->layers + layno;

			if (!layer->numpasses)
				continue;

			if (layer->len > max_bytes_available)
				return false;

			cblk->numPassesInPacket += layer->numpasses;
			*packet_bytes_written += layer->len;
			max_bytes_available -= layer->len;
		}
	}
	if (markers)
		markers->writeNext(*packet_bytes_written);

	return true;
}

}
