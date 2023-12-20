#ifndef SIM_CACHE_H
#define SIM_CACHE_H

typedef struct {
   uint32_t BLOCKSIZE;
   uint32_t L1_SIZE;
   uint32_t L1_ASSOC;
   uint32_t L2_SIZE;
   uint32_t L2_ASSOC;
   uint32_t PREF_N;
   uint32_t PREF_M;
} cache_params_t;

typedef struct {
	uint32_t uiTagAddr;
	uint32_t uiIndexAddr;
	uint32_t uiOffsetAddr;
} cache_addrs;

typedef struct
{
	uint32_t uiSets;
	uint32_t uiAssoc;
	uint32_t uiIndexBits;
	uint32_t uiBlockOffsetBits;
	uint32_t uitagBits;
} cache_bits_records;

typedef struct
{
  uint32_t index;
  uint8_t validBit;
  uint8_t dirtyBit;
  uint32_t tag;
  uint32_t offset;
  uint32_t lru;
} cache_block;

typedef struct
{
  uint32_t reads;
  uint32_t readMisses;
  uint32_t readHits;
  uint32_t writes;
  uint32_t writeMisses;
  uint32_t writeHits;
  float missRate;
  uint32_t writebacks;
  
} cache_record;

typedef struct 
{
	uint32_t index ;
	uint32_t tag ;
	uint32_t sb_addr;
	uint32_t valid;
	uint32_t lru;
}pfetch_block;

static uint32_t memoryTraffic;

// Put additional data structures here as per your requirement.

#endif
