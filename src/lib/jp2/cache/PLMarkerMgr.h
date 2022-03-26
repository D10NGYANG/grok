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
 */

#pragma once
#include <vector>
#include <map>

//#define DEBUG_PLT

namespace grk
{
// raw markers - PL is stored using comma code
typedef std::vector<grkBufferU8*> PL_MARKER;
typedef std::map<uint32_t, PL_MARKER*> PL_MARKERS;

struct PLMarkerMgr
{
	PLMarkerMgr(void);
	~PLMarkerMgr(void);

	//////////////////////////////////////////
	// compress
	void pushInit(bool isFinal);
	void pushPL(uint32_t len);
	bool write(void);
	uint32_t getTotalBytesWritten(void);
	/////////////////////////////////////////

	/////////////////////////////////////////////
	// decompress
	PLMarkerMgr(IBufferedStream* strm);
	bool readPLT(uint8_t* headerData, uint16_t header_size);
	bool readPLM(uint8_t* headerData, uint16_t header_size);
	void rewind(void);
	uint32_t pop(void);
	uint64_t pop(uint64_t numPackets);
	////////////////////////////////////////////
  private:
	void clearMarkers(void);
	bool findMarker(uint32_t index, bool compress);
	grkBufferU8* addNewMarker(uint8_t *data, uint16_t len);
	PL_MARKERS *rawMarkers_;
	PL_MARKERS::iterator currMarkerIter_;

	////////////////////////////////
	// compress
	uint32_t totalBytesWritten_;
	bool isFinal_;
	IBufferedStream* stream_;
	////////////////////////////////

	//////////////////////////
	// decompress
	bool readNextByte(uint8_t Iplm, uint32_t *packetLength);
	bool sequential_;
	uint32_t packetLen_;
	uint32_t currMarkerBufIndex_;
	grkBufferU8 *currMarkerBuf_;
	///////////////////////////////
};

} // namespace grk