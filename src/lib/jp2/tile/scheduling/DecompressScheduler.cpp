/*
 *    Copyright (C) 2016-2022 Grok Image Compression Inc.
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
#include "grk_includes.h"

namespace grk
{
const uint8_t gain_b[4] = {0, 1, 1, 2};

DecompressScheduler::DecompressScheduler() : success(true) {}

void DecompressScheduler::prepareScheduleDecompress(TileComponent* tilec,
													TileComponentCodingParams* tccp, uint8_t prec)
{
	bool wholeTileDecoding = tilec->isWholeTileDecoding();
	for(uint8_t resno = 0; resno <= tilec->highestResolutionDecompressed; ++resno)
	{
		ResDecompressBlocks resBlocks;
		auto res = tilec->tileCompResolution + resno;
		for(uint8_t bandIndex = 0; bandIndex < res->numTileBandWindows; ++bandIndex)
		{
			auto band = res->tileBand + bandIndex;
			auto paddedBandWindow =
				tilec->getBuffer()->getBandWindowPadded(resno, band->orientation);
			for(auto precinct : band->precincts)
			{
				if(!wholeTileDecoding && !paddedBandWindow->nonEmptyIntersection(precinct))
					continue;
				for(uint64_t cblkno = 0; cblkno < precinct->getNumCblks(); ++cblkno)
				{
					auto cblkBounds = precinct->getCodeBlockBounds(cblkno);
					if(wholeTileDecoding || paddedBandWindow->nonEmptyIntersection(&cblkBounds))
					{
						auto cblk = precinct->getDecompressedBlockPtr(cblkno);
						auto block = new DecompressBlockExec();
						block->x = cblk->x0;
						block->y = cblk->y0;
						block->tilec = tilec;
						block->bandIndex = bandIndex;
						block->bandNumbps = band->numbps;
						block->bandOrientation = band->orientation;
						block->cblk = cblk;
						block->cblk_sty = tccp->cblk_sty;
						block->qmfbid = tccp->qmfbid;
						block->resno = resno;
						block->roishift = tccp->roishift;
						block->stepsize = band->stepsize;
						block->k_msbs = (uint8_t)(band->numbps - cblk->numbps);
						block->R_b = prec + gain_b[band->orientation];
						resBlocks.push_back(block);
					}
				}
			}
		}
		if(!resBlocks.empty())
			blocks.push_back(resBlocks);
	}
}
bool DecompressScheduler::scheduleDecompress(TileComponent* tilec, TileCodingParams* tcp,
											 TileComponentCodingParams* tccp, uint8_t prec)
{
	prepareScheduleDecompress(tilec, tccp, prec);
	// nominal code block dimensions
	uint16_t codeblock_width = (uint16_t)(tccp->cblkw ? (uint32_t)1 << tccp->cblkw : 0);
	uint16_t codeblock_height = (uint16_t)(tccp->cblkh ? (uint32_t)1 << tccp->cblkh : 0);
	for(auto i = 0U; i < ExecSingleton::get()->num_workers(); ++i)
		t1Implementations.push_back(
			T1Factory::makeT1(false, tcp, codeblock_width, codeblock_height));

	return decompress();
}
bool DecompressScheduler::decompress()
{
	if(!blocks.size())
		return true;
	size_t num_threads = ExecSingleton::get()->num_workers();
	success = true;
	if(num_threads == 1)
	{
		for(auto& resBlocks : blocks)
		{
			for(auto& block : resBlocks)
			{
				if(!success)
				{
					delete block;
				}
				else
				{
					auto impl = t1Implementations[(size_t)0];
					if(!decompressBlock(impl, block))
						success = false;
				}
			}
		}

		return success;
	}

	// create one tf::Taskflow per resolution, and create one single
	// tf::Taskflow object composedFlow, composed of all resolution flows
	auto resFlow = new tf::Taskflow[blocks.size()];
	tf::Taskflow composedFlow;
	auto resBlockTasks = new tf::Task*[blocks.size()];
	auto resTasks = new tf::Task[blocks.size()];
	size_t resno = 0;
	for(auto& resBlocks : blocks)
	{
		auto resTaskArray = new tf::Task[resBlocks.size()];
		for(size_t blockno = 0; blockno < resBlocks.size(); ++blockno)
			resTaskArray[blockno] = resFlow[resno].placeholder();
		resBlockTasks[resno] = resTaskArray;
		resTasks[resno] = composedFlow.composed_of(resFlow[resno]).name("module");
		resno++;
	}
	resno = 0;
	for(auto& resBlocks : blocks)
	{
		size_t blockno = 0;
		for(auto& block : resBlocks)
		{
			resBlockTasks[resno][blockno++].work([this, block] {
				if(!success)
				{
					delete block;
				}
				else
				{
					auto threadnum = ExecSingleton::get()->this_worker_id();
					auto impl = t1Implementations[(size_t)threadnum];
					if(!decompressBlock(impl, block))
						success = false;
				}
			});
		}
		resno++;
	}
	ExecSingleton::get()->run(composedFlow).wait();

	for(size_t resno = 0; resno < blocks.size(); ++resno)
		delete[] resBlockTasks[resno];
	delete[] resBlockTasks;
	delete[] resTasks;
	delete[] resFlow;

	return success;
}
bool DecompressScheduler::decompressBlock(T1Interface* impl, DecompressBlockExec* block)
{
	try
	{
		bool rc = block->open(impl);
		delete block;
		return rc;
	}
	catch(std::runtime_error& rerr)
	{
		delete block;
		GRK_ERROR(rerr.what());
		return false;
	}

	return true;
}

} // namespace grk