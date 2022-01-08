#pragma once

#include <vector>
#include <mutex>
#include <atomic>

namespace grk {

struct Strip {
	Strip(GrkImage *outputImage, uint16_t id, uint32_t tileHeight);
	~Strip(void);
	GrkImage* stripImg;
	std::atomic<uint32_t> tileCounter;
};

class StripCache {
public:
	StripCache(void);
	virtual ~StripCache();

	void init( uint16_t tgrid_w,
				uint32_t th,
			  uint16_t tgrid_h,
			  GrkImage *outputImg,
			  void* serialize_d,
			  grk_serialize_pixels serializeBufferCb);
	bool composite(GrkImage *tileImage);
private:
	grk_serialize_buf getBuffer(uint64_t len);
	void putBuffer(grk_serialize_buf b);
	std::vector<grk_serialize_buf> bufCache;

	Strip **strips;
	uint16_t m_tgrid_w;
	uint32_t m_y0;
	uint32_t m_th;
	uint16_t m_tgrid_h;
	uint64_t m_packedRowBytes;

	mutable std::mutex bufCacheMutex;

	void* serialize_data;
	grk_serialize_pixels serializeBufferCallback;
};

}