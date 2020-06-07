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
 */

#include "Tier1.h"
#include "T1Decoder.h"
#include "T1Encoder.h"

namespace grk {

void Tier1::encodeCodeblocks(TileCodingParams *tcp,
							grk_tile *tile,
							const double *mct_norms,
							uint32_t mct_numcomps,
							bool doRateControl) {

	uint32_t compno, resno, bandno;
	uint64_t precno;
	tile->distotile = 0;
	std::vector<encodeBlockInfo*> blocks;
	uint32_t maxCblkW = 0;
	uint32_t maxCblkH = 0;

	for (compno = 0; compno < tile->numcomps; ++compno) {
		auto tilec = tile->comps + compno;
		auto tccp = tcp->tccps + compno;
		for (resno = 0; resno < tilec->numresolutions; ++resno) {
			auto res = &tilec->resolutions[resno];
			for (bandno = 0; bandno < res->numbands; ++bandno) {
				auto band = &res->bands[bandno];
				for (precno = 0; precno < (uint64_t)res->pw * res->ph; ++precno) {
					auto prc = &band->precincts[precno];
					int64_t cblkno;
					int32_t bandOdd = band->bandno & 1;
					int32_t bandModTwo = band->bandno & 2;

					for (cblkno = 0; cblkno < (int64_t) prc->cw * prc->ch;
							++cblkno) {
						auto cblk = prc->enc + cblkno;
						int32_t x = (int32_t)(cblk->x0 - band->x0);
						int32_t y = (int32_t)(cblk->y0 - band->y0);
						if (bandOdd) {
							grk_resolution *pres = &tilec->resolutions[resno
									- 1];
							x += pres->x1 - pres->x0;
						}
						if (bandModTwo) {
							grk_resolution *pres = &tilec->resolutions[resno
									- 1];
							y += pres->y1 - pres->y0;
						}

						maxCblkW = std::max<uint32_t>(maxCblkW,
								(uint32_t) (1 << tccp->cblkw));
						maxCblkH = std::max<uint32_t>(maxCblkH,
								(uint32_t) (1 << tccp->cblkh));
						auto block = new encodeBlockInfo();
						block->compno = compno;
						block->bandno = band->bandno;
						block->cblk = cblk;
						block->cblk_sty = tccp->cblk_sty;
						block->qmfbid = tccp->qmfbid;
						block->resno = resno;
						block->inv_step = (int32_t)band->inv_step;
						block->inv_step_ht = 1.0f/band->stepsize;
						block->stepsize = band->stepsize;
						block->x = (uint32_t)x;
						block->y = (uint32_t)y;
						block->mct_norms = mct_norms;
						block->mct_numcomps = mct_numcomps;
						block->tiledp = tilec->buf->get_ptr( resno,
								bandno, (uint32_t) x, (uint32_t) y);
						block->k_msbs = (uint8_t)(band->numbps - cblk->numbps);
						blocks.push_back(block);

					}
				}
			}
		}
	}
	T1Encoder encoder(tcp, tile, maxCblkW, maxCblkH, doRateControl);
	encoder.compress(&blocks);
}

bool Tier1::prepareDecodeCodeblocks(TileComponent *tilec, TileComponentCodingParams *tccp,
		std::vector<decodeBlockInfo*> *blocks) {
	if (!tilec->buf->alloc_component_data_decode()) {
		GROK_ERROR( "Not enough memory for tile data");
		return false;
	}
	for (uint32_t resno = 0; resno < tilec->minimum_num_resolutions; ++resno) {
		grk_resolution *res = &tilec->resolutions[resno];
		for (uint32_t bandno = 0; bandno < res->numbands; ++bandno) {
			grk_band *GRK_RESTRICT band = res->bands + bandno;
			for (uint64_t precno = 0; precno < (uint64_t)res->pw * res->ph; ++precno) {
				auto precinct = band->precincts + precno;
				if (!tilec->is_subband_area_of_interest(resno,
												bandno,
												precinct->x0,
												precinct->y0,
												precinct->x1,
												precinct->y1)){

					continue;
				}
				for (uint64_t cblkno = 0;
						cblkno < (uint64_t) precinct->cw * precinct->ch;
						++cblkno) {
					grk_rect cblk_rect;
					auto cblk = precinct->dec + cblkno;
					if (tilec->is_subband_area_of_interest(resno,
													bandno,
													cblk->x0,
													cblk->y0,
													cblk->x1,
													cblk->y1)){


						/* get code block offset relative to band*/
						int32_t x = (int32_t)cblk->x0 - (int32_t)band->x0;
						int32_t y = (int32_t)cblk->y0 - (int32_t)band->y0;

						/* add band offset relative to previous resolution */
						if (band->bandno & 1) {
							auto pres = &tilec->resolutions[resno - 1];
							x += pres->x1 - pres->x0;
						}
						if (band->bandno & 2) {
							auto pres = &tilec->resolutions[resno - 1];
							y += pres->y1 - pres->y0;
						}
						assert(x >= 0);
						assert(y >= 0);

						auto block = new decodeBlockInfo();
						block->bandno = band->bandno;
						block->cblk = cblk;
						block->cblk_sty = tccp->cblk_sty;
						block->qmfbid = tccp->qmfbid;
						block->resno = resno;
						block->roishift = tccp->roishift;
						block->stepsize = band->stepsize;
						block->tilec = tilec;
						block->x = (uint32_t)x;
						block->y = (uint32_t)y;
						block->tiledp = tilec->buf->get_ptr( resno, bandno,
								(uint32_t) x, (uint32_t) y);
						block->k_msbs = (uint8_t)(band->numbps - cblk->numbps);
						blocks->push_back(block);
					}

				}
			}
		}
	}
	return true;
}


bool Tier1::decodeCodeblocks(TileCodingParams *tcp,
		                    uint16_t blockw, uint16_t blockh,
		                    std::vector<decodeBlockInfo*> *blocks) {
	T1Decoder decoder(tcp, blockw, blockh);
	return decoder.decompress(blocks);
}

}
