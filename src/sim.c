#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "sim.h"
#include <math.h>
#include <string.h>

/*  "argc" holds the number of command-line arguments.
    "argv[]" holds the arguments themselves.

Example:
./sim 32 8192 4 262144 8 3 10 gcc_trace.txt
argc = 9
argv[0] = "./sim"
argv[1] = "32"
argv[2] = "8192"
... and so on
*/

#define DEBUG 0

cache_block **L1CacheBlock, **L2CacheBlock;
pfetch_block **pfetch = {0};
cache_addrs L1CacheAddrs, L2CacheAddrs;
cache_record L1CacheRecord = {0}, L2CacheRecord = {0};
cache_bits_records L1BitsRecords ={0} , L2BitsRecords={0};
cache_params_t temp_param = {0};
uint32_t temp_addr = 0, mem_fetch = 0;

void addressBitsSeprationFun(cache_params_t stCacheParams);
void seprateAddrsFun(uint32_t addr,cache_params_t params);
void displayFunc(uint32_t L1_Size,uint32_t L2_Size);
void sortingLru(uint32_t L1_Size,uint32_t L2_Size);
void sortingPrefetchLru();
void testdisplay();
int writeL1Cache();
int readL1Cache();
void updateLru(uint32_t uiIndexAddr, uint32_t index , char *cacheType);
int readL2Cache();
int L1WritebackToL2(uint32_t uiTagAddr, uint32_t uiIndexAddr,uint32_t uiOffsetAddr);

void prefetch_all(int n,cache_bits_records bit_records);
void prefetch_required(int p, int j, cache_bits_records bit);
void prefetch_all_req(int p, cache_bits_records bit);
void update_pfetch_lru(int i);
int find_block_pfetch(cache_addrs addr, cache_bits_records bit_records);

void displayPrefetch();


void addressBitsSeprationFun(cache_params_t stCacheParams)
{
	int i=0,j=0;

	L1BitsRecords.uiAssoc = stCacheParams.L1_ASSOC;
	// Finding Sets in L1 and finding Index,offset and Tag bits
	L1BitsRecords.uiSets = (stCacheParams.L1_SIZE / (stCacheParams.L1_ASSOC * stCacheParams.BLOCKSIZE));
	L1BitsRecords.uiIndexBits = log2(L1BitsRecords.uiSets);
	L1BitsRecords.uiBlockOffsetBits = log2(stCacheParams.BLOCKSIZE);
	L1BitsRecords.uitagBits = 32 - L1BitsRecords.uiIndexBits - L1BitsRecords.uiBlockOffsetBits;

	if(stCacheParams.L2_SIZE != 0)
	{
		L2BitsRecords.uiAssoc = stCacheParams.L2_ASSOC;
		// Finding Sets in L2 and finding Index,offset and Tag bits
		L2BitsRecords.uiSets = (stCacheParams.L2_SIZE / (stCacheParams.L2_ASSOC * stCacheParams.BLOCKSIZE));
		L2BitsRecords.uiIndexBits = log2(L2BitsRecords.uiSets);
		L2BitsRecords.uiBlockOffsetBits = log2(stCacheParams.BLOCKSIZE);
		L2BitsRecords.uitagBits = 32 - L2BitsRecords.uiIndexBits - L2BitsRecords.uiBlockOffsetBits;
	}

#if DEBUG
	printf("L1Sets =%d, L1IndexBit = %d ,L1BlockOffsetBit =%d, L1tagBit=%d\n",L1BitsRecords.uiSets,L1BitsRecords.uiIndexBits,L1BitsRecords.uiBlockOffsetBits,L1BitsRecords.uitagBits);
	printf("L2Sets =%d, L2IndexBit = %d ,L2BlockOffsetBit =%d, L2tagBit=%d\n",L2BitsRecords.uiSets,L2BitsRecords.uiIndexBits,L2BitsRecords.uiBlockOffsetBits,L2BitsRecords.uitagBits);
#endif

	////////////    Allocating Dynamic Memory for L1 and L2  Start  //////////////////////////
	L1CacheBlock = calloc (L1BitsRecords.uiSets,sizeof(cache_block *));   // Its allocate 4 byte according the number of set
	if(L1CacheBlock != NULL)
	{
		for(i=0;i<L1BitsRecords.uiSets;i++)
		{
			L1CacheBlock[i] = calloc (stCacheParams.L1_ASSOC,sizeof(cache_block));  // Its allocate Number of cache block byte according the number of Assoc in each set
		}

		// Setting LRU values
		for(i=0;i<L1BitsRecords.uiSets;i++)
		{
			for(j=0;j<stCacheParams.L1_ASSOC;j++)
			{
				L1CacheBlock[i][j].lru = j;
			}
		}
	}

	if(stCacheParams.L2_SIZE != 0)
	{
		L2CacheBlock = calloc (L2BitsRecords.uiSets,sizeof(cache_block));
		if(L2CacheBlock != NULL)
		{
			for(i=0;i<L2BitsRecords.uiSets;i++)
			{
				L2CacheBlock[i] =  calloc (stCacheParams.L2_ASSOC,sizeof(cache_block));  // Its allocate Number of cache block byte according the number of Assoc in each set
			}

			// Setting LRU values
			for(i=0;i<L2BitsRecords.uiSets;i++)
			{
				for(j=0;j<stCacheParams.L2_ASSOC;j++)
				{
					L2CacheBlock[i][j].lru = j;
				}
			}
		}
	}

	if (stCacheParams.PREF_N != 0)
	{
		pfetch = calloc(stCacheParams.PREF_N, sizeof(pfetch_block));

		for (i = 0; i < stCacheParams.PREF_N; i++)
		{
			pfetch[i] = calloc(stCacheParams.PREF_M, sizeof(pfetch_block));
		}
	}

	for(i=0;i<stCacheParams.PREF_N;i++)
	{
		pfetch[i]->lru = i;
	}
}

void updateLru(uint32_t uiIndexAddr, uint32_t index , char *cacheType)
{
	uint32_t tempLru = 0,j=0;	

	if(strcmp(cacheType,"L1") == 0)
	{
		//printf("updateLru L1\n");

		tempLru = L1CacheBlock[uiIndexAddr][index].lru;

		for(j=0;j<L1BitsRecords.uiAssoc;j++)
		{
			if(L1CacheBlock[uiIndexAddr][j].lru < tempLru)
			{
				L1CacheBlock[uiIndexAddr][j].lru += 1;
			}
		}
		L1CacheBlock[uiIndexAddr][index].lru = 0;
	}
	else if(strcmp(cacheType,"L2") == 0)
	{
		//printf("updateLru L2\n");

		tempLru = L2CacheBlock[uiIndexAddr][index].lru;

		for(j=0;j<L2BitsRecords.uiAssoc;j++)
		{
			if(L2CacheBlock[uiIndexAddr][j].lru < tempLru)
			{
				L2CacheBlock[uiIndexAddr][j].lru += 1;
			}
		}
		L2CacheBlock[uiIndexAddr][index].lru = 0;
	}
	else{}
}

void seprateAddrsFun(uint32_t addr,cache_params_t params)
{
	/// Seprating Address ////
	L1CacheAddrs.uiTagAddr = addr >> (L1BitsRecords.uiIndexBits + L1BitsRecords.uiBlockOffsetBits);
	L1CacheAddrs.uiIndexAddr = (addr >> L1BitsRecords.uiBlockOffsetBits) ^ (L1CacheAddrs.uiTagAddr << L1BitsRecords.uiIndexBits);
	L1CacheAddrs.uiOffsetAddr = addr ^ ((L1CacheAddrs.uiTagAddr << (L1BitsRecords.uiIndexBits + L1BitsRecords.uiBlockOffsetBits)) | ( L1CacheAddrs.uiIndexAddr << L1BitsRecords.uiBlockOffsetBits));
	//printf("L1 uiTagAddr =%x ,uiIndexAddr =%x and uiOffsetAddr =%x\n",L1CacheAddrs.uiTagAddr,L1CacheAddrs.uiIndexAddr, L1CacheAddrs.uiOffsetAddr);

	if(params.L2_SIZE != 0)
	{
		//printf("L2 Block is available\n");
		L2CacheAddrs.uiTagAddr = addr >> (L2BitsRecords.uiIndexBits + L2BitsRecords.uiBlockOffsetBits);
		L2CacheAddrs.uiIndexAddr = (addr >> L2BitsRecords.uiBlockOffsetBits) ^ (L2CacheAddrs.uiTagAddr << L2BitsRecords.uiIndexBits);
		L2CacheAddrs.uiOffsetAddr = addr ^ ((L2CacheAddrs.uiTagAddr << (L2BitsRecords.uiIndexBits + L2BitsRecords.uiBlockOffsetBits)) | ( L2CacheAddrs.uiIndexAddr << L2BitsRecords.uiBlockOffsetBits));
	}
	//printf("L2 uiTagAddr =%x ,uiIndexAddr =%x and uiOffsetAddr =%x\n\n\n",L2CacheAddrs.uiTagAddr,L2CacheAddrs.uiIndexAddr, L2CacheAddrs.uiOffsetAddr);

}

int readL2Cache()
{
	//printf("In Read L2\n");
	uint8_t flagSpaceNotAvailable = 1;
	uint32_t i=0,j=0;

	L2CacheRecord.reads += 1;

	/// Read Hit Scenario ///
	for(i=0;i<L2BitsRecords.uiAssoc;i++)
	{  
		if(L2CacheAddrs.uiTagAddr == L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].tag )//&& L2CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit == 1) // Read Hit
		{
			find_block_pfetch(L2CacheAddrs, L2BitsRecords);
			//printf("L2 Read hit\n");
			L2CacheRecord.readHits += 1;
			// LRU Counter Update; Not update Dirty and Valid bit
			updateLru(L2CacheAddrs.uiIndexAddr,i,"L2");
			break;
		}
	}

	/// Read Miss Scenario ///
	if(i == L2BitsRecords.uiAssoc)
	{
		L2CacheRecord.readMisses += 1;

		for(i=0;i<L2BitsRecords.uiAssoc;i++)
		{
			// 1. Checking in particular Index(set) valid bit is 0 means space is available
			if(L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].validBit == 0 )//&& L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].lru == (L2BitsRecords.uiAssoc -1))
			{
				flagSpaceNotAvailable = 0;
				//printf("L2 Read Miss v=0\n");

				if(temp_param.PREF_N != 0)
				{
					int n=0;
					int emptyStreamBuff = 0;

					//printf(" temp_param.PREF_N =%d\n",temp_param.PREF_N);

					for(n=0;n<temp_param.PREF_N;n++)
					{
						//printf("pfetch[n]->valid = %d and  emptyStreamBuff =%d\n",pfetch[n]->valid,emptyStreamBuff);

						if(pfetch[n]->valid != 0)
						{
							emptyStreamBuff = 1;
							break;
						}
						//printf("pfetch[n]->valid = %d and  emptyStreamBuff =%d\n",pfetch[n]->valid,emptyStreamBuff);
					}

					if(emptyStreamBuff == 0)  // All stream buffer is empty
					{
						//printf("Buffer Empty\n");

						if(pfetch[0]->valid == 0)
						{
							//printf("Buffer Empty and Valid 0\n");
							prefetch_all(0,L2BitsRecords);
							pfetch[0]->valid = 1;

							memoryTraffic +=1;
							L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].index = L2CacheAddrs.uiIndexAddr;
							L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].validBit = 1;
							L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
							L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].tag = L2CacheAddrs.uiTagAddr;
							L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].offset = L2CacheAddrs.uiOffsetAddr;
							updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU
							
							return 0;
						}	
					}
					else  // Atleast one will be full
					{
						//printf("Atleast one Buffer full\n");
						if(find_block_pfetch(L2CacheAddrs, L2BitsRecords))
						{
							//printf("Able to find block\n");

							L2CacheRecord.readMisses -= 1;
							
							L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].index = L2CacheAddrs.uiIndexAddr;
							L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].validBit = 1;
							L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
							L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].tag = L2CacheAddrs.uiTagAddr;
							L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].offset = L2CacheAddrs.uiOffsetAddr;

							updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU

							return 0;//break;
						}
						else
						{
							//printf("Not Able to find block\n");

							// Space is available
							for (j = 0; j < temp_param.PREF_N; j++)
							{
								if (pfetch[j]->valid == 0)
								{
									//printf("Space is available\n");
									prefetch_all(j, L2BitsRecords);
									pfetch[j]->valid = 1;
									//update_pfetch_lru(j);

									memoryTraffic +=1;
									L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].index = L2CacheAddrs.uiIndexAddr;
									L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].validBit = 1;
									L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
									L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].tag = L2CacheAddrs.uiTagAddr;
									L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].offset = L2CacheAddrs.uiOffsetAddr;

									updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU
									return 0;
								}
							}

							// Space is Not available
							for (j = 0; j < temp_param.PREF_N; j++)
							{
								//printf("Space is not available\n");
								if (pfetch[j]->lru == temp_param.PREF_N -1)
								{
									//printf("Space do empty through LRU pfetch[i]->lru  =%d\n",pfetch[j]->lru);
									prefetch_all(j, L2BitsRecords);
									//update_pfetch_lru(j);

									memoryTraffic +=1;
									L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].index = L2CacheAddrs.uiIndexAddr;
									L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].validBit = 1;
									L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
									L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].tag = L2CacheAddrs.uiTagAddr;
									L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].offset = L2CacheAddrs.uiOffsetAddr;

									updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU
									return 0;
								}
							}
						}
					}
				}

				memoryTraffic +=1;
				L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].index = L2CacheAddrs.uiIndexAddr;
				L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].validBit = 1;
				L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
				L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].tag = L2CacheAddrs.uiTagAddr;
				L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].offset = L2CacheAddrs.uiOffsetAddr;

				updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU
				
				break;
			}
		}

		if(flagSpaceNotAvailable)  // Space is not available in L2 Cache
		{
			for(i=0;i<L2BitsRecords.uiAssoc;i++)
			{
				// lru is 0 and D=0 so directly replace the block
				if(L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].lru == (L2BitsRecords.uiAssoc -1) && L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].dirtyBit == 0)
				{
					//printf("L2 Read Miss valid 1 and D =0\n"); 

					if(temp_param.PREF_N != 0)
					{
						int n=0;
						int emptyStreamBuff = 0;

						//printf(" temp_param.PREF_N =%d\n",temp_param.PREF_N);

						for(n=0;n<temp_param.PREF_N;n++)
						{
							//printf("pfetch[n]->valid = %d and  emptyStreamBuff =%d\n",pfetch[n]->valid,emptyStreamBuff);

							if(pfetch[n]->valid != 0)
							{
								emptyStreamBuff = 1;
								break;
							}
							//printf("pfetch[n]->valid = %d and  emptyStreamBuff =%d\n",pfetch[n]->valid,emptyStreamBuff);
						}

						if(emptyStreamBuff == 0)  // All stream buffer is empty
						{
							//printf("Buffer Empty\n");

							if(pfetch[0]->valid == 0)
							{
								//printf("Buffer Empty and Valid 0\n");
								prefetch_all(0,L2BitsRecords);
								pfetch[0]->valid = 1;

								memoryTraffic +=1;
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].index = L2CacheAddrs.uiIndexAddr;
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].validBit = 1;
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].tag = L2CacheAddrs.uiTagAddr;
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].offset = L2CacheAddrs.uiOffsetAddr;
								updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU
							
								return 0;
							}	
						}
						else  // Atleast one will be full
						{
							//printf("Atleast one Buffer full\n");
							if(find_block_pfetch(L2CacheAddrs, L2BitsRecords))
							{
								//printf("Able to find block\n");

								L2CacheRecord.readMisses -= 1;
							
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].index = L2CacheAddrs.uiIndexAddr;
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].validBit = 1;
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].tag = L2CacheAddrs.uiTagAddr;
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].offset = L2CacheAddrs.uiOffsetAddr;

								updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU

								return 0;//break;
							}
							else
							{
								//printf("Not Able to find block\n");

								// Space is available
								for (j = 0; j < temp_param.PREF_N; j++)
								{
									if (pfetch[j]->valid == 0)
									{
										//printf("Space is available\n");
										prefetch_all(j, L2BitsRecords);
										pfetch[j]->valid = 1;
										//update_pfetch_lru(j);		

										memoryTraffic +=1;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].index = L2CacheAddrs.uiIndexAddr;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].validBit = 1;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].tag = L2CacheAddrs.uiTagAddr;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].offset = L2CacheAddrs.uiOffsetAddr;

										updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU
										//printf("All Done\n");
										return 0;
									}
								}
							

								// Space is Not available
								for (j = 0; j < temp_param.PREF_N; j++)
								{
									//printf("Space is not available\n");
									if (pfetch[j]->lru == temp_param.PREF_N -1)
									{
										//printf("Space do empty through LRU pfetch[i]->lru  =%d\n",pfetch[j]->lru);
										prefetch_all(j, L2BitsRecords);
										//update_pfetch_lru(j);

										memoryTraffic +=1;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].index = L2CacheAddrs.uiIndexAddr;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].validBit = 1;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].tag = L2CacheAddrs.uiTagAddr;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].offset = L2CacheAddrs.uiOffsetAddr;

										updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU
										return 0;
									}
								}
							}
						}
					}

					memoryTraffic += 1;
					L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].index = L2CacheAddrs.uiIndexAddr;
					L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].validBit = 1;
					L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
					L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].tag = L2CacheAddrs.uiTagAddr;
					L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].offset = L2CacheAddrs.uiOffsetAddr;

					updateLru(L2CacheAddrs.uiIndexAddr,i,"L2");// update LRU
					break;
				}
				// lru is 0 and D=1 so clean and replace the block
				else if(L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].lru == (L2BitsRecords.uiAssoc - 1) && L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].dirtyBit == 1)
				{
					//printf("L2 Read Miss valid 1 and D =1\n");
					// For cleaning need to do memory write and after read data also doing memory operation so +2

					if(temp_param.PREF_N != 0)
					{
						int n=0;
						int emptyStreamBuff = 0;

						//printf(" temp_param.PREF_N =%d\n",temp_param.PREF_N);

						for(n=0;n<temp_param.PREF_N;n++)
						{
							//printf("pfetch[n]->valid = %d and  emptyStreamBuff =%d\n",pfetch[n]->valid,emptyStreamBuff);

							if(pfetch[n]->valid != 0)
							{
								emptyStreamBuff = 1;
								break;
							}
							//printf("pfetch[n]->valid = %d and  emptyStreamBuff =%d\n",pfetch[n]->valid,emptyStreamBuff);
						}

						if(emptyStreamBuff == 0)  // All stream buffer is empty
						{
							//printf("Buffer Empty\n");

							if(pfetch[0]->valid == 0)
							{
								//printf("Buffer Empty and Valid 0\n");
								prefetch_all(0,L2BitsRecords);
								pfetch[0]->valid = 1;

								L2CacheRecord.writebacks += 1;
								memoryTraffic +=2;
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].index = L2CacheAddrs.uiIndexAddr;
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].validBit = 1;
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].tag = L2CacheAddrs.uiTagAddr;
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].offset = L2CacheAddrs.uiOffsetAddr;
								updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU
							
								return 0;
							}	
						}
						else  // Atleast one will be full
						{
							//printf("Atleast one Buffer full\n");
							if(find_block_pfetch(L2CacheAddrs, L2BitsRecords))
							{
								//printf("Able to find block\n");

								L2CacheRecord.readMisses -= 1;
								L2CacheRecord.writebacks += 1;
								memoryTraffic +=1;  // check this once *********
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].index = L2CacheAddrs.uiIndexAddr;
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].validBit = 1;
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].tag = L2CacheAddrs.uiTagAddr;
								L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].offset = L2CacheAddrs.uiOffsetAddr;

								updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU

								return 0;//break;
							}
							else
							{
								//printf("Not Able to find block\n");

								// Space is available
								for (j = 0; j < temp_param.PREF_N; j++)
								{
									if (pfetch[j]->valid == 0)
									{
										//printf("Space is available\n");
										prefetch_all(j, L2BitsRecords);
										pfetch[j]->valid = 1;
										//update_pfetch_lru(j);

										L2CacheRecord.writebacks += 1;
										memoryTraffic +=2;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].index = L2CacheAddrs.uiIndexAddr;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].validBit = 1;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].tag = L2CacheAddrs.uiTagAddr;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].offset = L2CacheAddrs.uiOffsetAddr;

										updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU
										return 0;
									}
								}

								// Space is Not available
								for (j = 0; j < temp_param.PREF_N; j++)
								{
									//printf("Space is not available\n");
									if (pfetch[j]->lru == temp_param.PREF_N -1)
									{
										//printf("Space do empty through LRU pfetch[i]->lru  =%d\n",pfetch[j]->lru);
										prefetch_all(j, L2BitsRecords);
										//update_pfetch_lru(j);

										L2CacheRecord.writebacks += 1;
										memoryTraffic +=2;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].index = L2CacheAddrs.uiIndexAddr;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].validBit = 1;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].tag = L2CacheAddrs.uiTagAddr;
										L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].offset = L2CacheAddrs.uiOffsetAddr;

										updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU
										return 0;
									}
								}
							}
						}
					}

					L2CacheRecord.writebacks += 1;
					memoryTraffic += 2;
					L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].index = L2CacheAddrs.uiIndexAddr;
					L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].validBit = 1;
					L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
					L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].tag = L2CacheAddrs.uiTagAddr;
					L2CacheBlock[L2CacheAddrs.uiIndexAddr][i].offset = L2CacheAddrs.uiOffsetAddr;

					updateLru(L2CacheAddrs.uiIndexAddr,i,"L2");// update LRU
					break;
				}
				else{}
			}
			flagSpaceNotAvailable = 0;
		}
	}
	return 0;
}

// This method use when D=1 in L1 and need to clean it in L2. So block always be there in L2 just need top clean and update LRU
int L1WritebackToL2(uint32_t uiTagAddr, uint32_t uiIndexAddr,uint32_t uiOffsetAddr)
{
	//printf("L1 Writeback to L2\n");
	cache_addrs tempCacheAddrs = {0};
	uint32_t addr = 0;
	uint32_t i = 0,j=0;;
	uint8_t flagSpaceNotAvailable = 1;

	L2CacheRecord.writes += 1;

	//printf("L1 uiTagAddr=%x, Index =%x and Offset =%x\n",uiTagAddr,uiIndexAddr,uiOffsetAddr);

	addr = ((uiTagAddr << (L1BitsRecords.uiIndexBits + L1BitsRecords.uiBlockOffsetBits)) | (uiIndexAddr << L1BitsRecords.uiBlockOffsetBits) | uiOffsetAddr);

	//printf("After Combining addr = %x\n",addr);

	tempCacheAddrs.uiTagAddr = addr >> (L2BitsRecords.uiIndexBits + L2BitsRecords.uiBlockOffsetBits);
	tempCacheAddrs.uiIndexAddr = (addr >> L2BitsRecords.uiBlockOffsetBits) ^ (tempCacheAddrs.uiTagAddr << L2BitsRecords.uiIndexBits);
	tempCacheAddrs.uiOffsetAddr = addr ^ ((tempCacheAddrs.uiTagAddr << (L2BitsRecords.uiIndexBits + L2BitsRecords.uiBlockOffsetBits)) | (tempCacheAddrs.uiIndexAddr << L2BitsRecords.uiBlockOffsetBits));

	//printf("L2 uiTagAddr=%x, Index =%x and Offset =%x\n",tempCacheAddrs.uiTagAddr,tempCacheAddrs.uiIndexAddr,tempCacheAddrs.uiOffsetAddr);

	for(i=0;i<L2BitsRecords.uiAssoc;i++)
	{
		// check Tag and dirty bit is clear 
		//if(L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit == 0)
		if((L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].tag == tempCacheAddrs.uiTagAddr)) //&& (L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit == 1))
		{
			//printf("Before update Cache block Tag=%x , Index =%x, valid =%x, dirty =%x, lru =%x\n",L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].tag,L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].index,
			//L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit,L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit,L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].lru);
			find_block_pfetch(tempCacheAddrs, L2BitsRecords);

			L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit = 1;  //Set dirty bit and update LRU for L2
			updateLru(tempCacheAddrs.uiIndexAddr,i,"L2");

			//printf("After update Cache block Tag=%x , Index =%x, valid =%x, dirty =%x, lru =%x\n",L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].tag,L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].index,
			//L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit,L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit,L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].lru);

			break;
		}
	}
	/// Read Miss Scenario ///
	if(i == L2BitsRecords.uiAssoc)
	{
		L2CacheRecord.writeMisses += 1;

		for(i=0;i<L2BitsRecords.uiAssoc;i++)
		{
			// 1. Checking in particular Index(set) valid bit is 0 means space is available
			if(L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit == 0)
			{
				//printf("L2 Write Miss v=0\n");
				////////// Rohan Start /////////////////////

				if(temp_param.PREF_N != 0)
				{
					int n=0;
					int emptyStreamBuff = 0;

					//printf(" temp_param.PREF_N =%d\n",temp_param.PREF_N);

					for(n=0;n<temp_param.PREF_N;n++)
					{
						//printf("pfetch[n]->valid = %d and  emptyStreamBuff =%d\n",pfetch[n]->valid,emptyStreamBuff);

						if(pfetch[n]->valid != 0)
						{
							emptyStreamBuff = 1;
							break;
						}
						//printf("pfetch[n]->valid = %d and  emptyStreamBuff =%d\n",pfetch[n]->valid,emptyStreamBuff);
					}

					if(emptyStreamBuff == 0)  // All stream buffer is empty
					{
						//printf("Buffer Empty\n");

						if(pfetch[0]->valid == 0)
						{
							//printf("Buffer Empty and Valid 0\n");
							prefetch_all(0,L2BitsRecords);
							pfetch[0]->valid = 1;

							memoryTraffic +=1;
							
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].index = tempCacheAddrs.uiIndexAddr;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit = 1;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit = 0;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].tag = tempCacheAddrs.uiTagAddr;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].offset = tempCacheAddrs.uiOffsetAddr;

							updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU
							
							return 0;
						}	
					}
					else  // Atleast one will be full
					{
						//printf("Atleast one Buffer full\n");
						if(find_block_pfetch(L2CacheAddrs, L2BitsRecords))
						{
							//printf("Able to find block\n");

							L2CacheRecord.readMisses -= 1;
							
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].index = tempCacheAddrs.uiIndexAddr;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit = 1;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit = 0;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].tag = tempCacheAddrs.uiTagAddr;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].offset = tempCacheAddrs.uiOffsetAddr;


							updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU

							return 0;//break;
						}
						else
						{
							//printf("Not Able to find block\n");

							// Space is available
							for (j = 0; j < temp_param.PREF_N; j++)
							{
								if (pfetch[j]->valid == 0)
								{
									//printf("Space is available\n");
									prefetch_all(j, L2BitsRecords);
									pfetch[j]->valid = 1;
									//update_pfetch_lru(j);

									memoryTraffic +=1;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].index = tempCacheAddrs.uiIndexAddr;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit = 1;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit = 0;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].tag = tempCacheAddrs.uiTagAddr;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].offset = tempCacheAddrs.uiOffsetAddr;

									updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU
									return 0;
								}
							}

							// Space is Not available
							for (j = 0; j < temp_param.PREF_N; j++)
							{
								//printf("Space is not available\n");
								if (pfetch[j]->lru == temp_param.PREF_N -1)
								{
									//printf("Space do empty through LRU pfetch[i]->lru  =%d\n",pfetch[j]->lru);
									prefetch_all(j, L2BitsRecords);
									//update_pfetch_lru(j);

									memoryTraffic +=1;

									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].index = tempCacheAddrs.uiIndexAddr;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit = 1;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit = 0;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].tag = tempCacheAddrs.uiTagAddr;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].offset = tempCacheAddrs.uiOffsetAddr;

									updateLru(tempCacheAddrs.uiIndexAddr,i,"L2"); // update LRU
									return 0;
								}
							}
						}
					}
				}

				//////////////// Rohan End /////////////////////////

				memoryTraffic +=1;

				L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].index = tempCacheAddrs.uiIndexAddr;
				L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit = 1;
				L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit = 0;
				L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].tag = tempCacheAddrs.uiTagAddr;
				L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].offset = tempCacheAddrs.uiOffsetAddr;

				updateLru(tempCacheAddrs.uiIndexAddr,i,"L2"); // update LRU
				flagSpaceNotAvailable = 0;
				break;
			}
		}

		if(flagSpaceNotAvailable)  // Space is not available in L2 Cache
		{
			for(i=0;i<L2BitsRecords.uiAssoc;i++)
			{
				// lru is 0 and D=0 so directly replace the block
				if(L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].lru == (L2BitsRecords.uiAssoc - 1) && L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit == 0)
				{
					//printf("L2 Write Miss valid 1 and D =0\n"); 

					/// Start ///

									if(temp_param.PREF_N != 0)
				{
					int n=0;
					int emptyStreamBuff = 0;

					//printf(" temp_param.PREF_N =%d\n",temp_param.PREF_N);

					for(n=0;n<temp_param.PREF_N;n++)
					{
						//printf("pfetch[n]->valid = %d and  emptyStreamBuff =%d\n",pfetch[n]->valid,emptyStreamBuff);

						if(pfetch[n]->valid != 0)
						{
							emptyStreamBuff = 1;
							break;
						}
						//printf("pfetch[n]->valid = %d and  emptyStreamBuff =%d\n",pfetch[n]->valid,emptyStreamBuff);
					}

					if(emptyStreamBuff == 0)  // All stream buffer is empty
					{
						//printf("Buffer Empty\n");

						if(pfetch[0]->valid == 0)
						{
							//printf("Buffer Empty and Valid 0\n");
							prefetch_all(0,L2BitsRecords);
							pfetch[0]->valid = 1;

							memoryTraffic +=1;
							
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].index = tempCacheAddrs.uiIndexAddr;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit = 1;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit = 0;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].tag = tempCacheAddrs.uiTagAddr;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].offset = tempCacheAddrs.uiOffsetAddr;

							updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU
							
							return 0;
						}	
					}
					else  // Atleast one will be full
					{
						//printf("Atleast one Buffer full\n");
						if(find_block_pfetch(L2CacheAddrs, L2BitsRecords))
						{
							//printf("Able to find block\n");

							L2CacheRecord.readMisses -= 1;
							
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].index = tempCacheAddrs.uiIndexAddr;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit = 1;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit = 0;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].tag = tempCacheAddrs.uiTagAddr;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].offset = tempCacheAddrs.uiOffsetAddr;


							updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU

							return 0;//break;
						}
						else
						{
							//printf("Not Able to find block\n");

							// Space is available
							for (j = 0; j < temp_param.PREF_N; j++)
							{
								if (pfetch[j]->valid == 0)
								{
									//printf("Space is available\n");
									prefetch_all(j, L2BitsRecords);
									pfetch[j]->valid = 1;
									//update_pfetch_lru(j);

									memoryTraffic +=1;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].index = tempCacheAddrs.uiIndexAddr;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit = 1;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit = 0;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].tag = tempCacheAddrs.uiTagAddr;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].offset = tempCacheAddrs.uiOffsetAddr;

									updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU
									return 0;
								}
							}

							// Space is Not available
							for (j = 0; j < temp_param.PREF_N; j++)
							{
								//printf("Space is not available\n");
								if (pfetch[j]->lru == temp_param.PREF_N -1)
								{
									//printf("Space do empty through LRU pfetch[i]->lru  =%d\n",pfetch[j]->lru);
									prefetch_all(j, L2BitsRecords);
									//update_pfetch_lru(j);

									memoryTraffic +=1;

									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].index = tempCacheAddrs.uiIndexAddr;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit = 1;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit = 0;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].tag = tempCacheAddrs.uiTagAddr;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].offset = tempCacheAddrs.uiOffsetAddr;

									updateLru(tempCacheAddrs.uiIndexAddr,i,"L2"); // update LRU
									return 0;
								}
							}
						}
					}
				}

					/// End /////

					memoryTraffic += 1;
					L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].index = tempCacheAddrs.uiIndexAddr;
					L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit = 1;
					L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit = 0;
					L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].tag = tempCacheAddrs.uiTagAddr;
					L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].offset = tempCacheAddrs.uiOffsetAddr;

					updateLru(tempCacheAddrs.uiIndexAddr,i,"L2");// update LRU
					break;

				}
				// lru is 0 and D=1 so clean and replace the block
				else if(L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].lru == (L2BitsRecords.uiAssoc - 1) && L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit == 1)
				{
					//printf("L2 Write Miss valid 1 and D =1\n");
					// For cleaning need to do memory write and after read data also doing memory operation so +2


					//// Start ///
				if(temp_param.PREF_N != 0)
				{
					int n=0;
					int emptyStreamBuff = 0;

					//printf(" temp_param.PREF_N =%d\n",temp_param.PREF_N);

					for(n=0;n<temp_param.PREF_N;n++)
					{
						//printf("pfetch[n]->valid = %d and  emptyStreamBuff =%d\n",pfetch[n]->valid,emptyStreamBuff);

						if(pfetch[n]->valid != 0)
						{
							emptyStreamBuff = 1;
							break;
						}
						//printf("pfetch[n]->valid = %d and  emptyStreamBuff =%d\n",pfetch[n]->valid,emptyStreamBuff);
					}

					if(emptyStreamBuff == 0)  // All stream buffer is empty
					{
						//printf("Buffer Empty\n");

						if(pfetch[0]->valid == 0)
						{
							//printf("Buffer Empty and Valid 0\n");
							prefetch_all(0,L2BitsRecords);
							pfetch[0]->valid = 1;

							L2CacheRecord.writebacks += 1;
							memoryTraffic += 2;
							
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].index = tempCacheAddrs.uiIndexAddr;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit = 1;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit = 0;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].tag = tempCacheAddrs.uiTagAddr;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].offset = tempCacheAddrs.uiOffsetAddr;

							updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU
							
							return 0;
						}	
					}
					else  // Atleast one will be full
					{
						//printf("Atleast one Buffer full\n");
						if(find_block_pfetch(L2CacheAddrs, L2BitsRecords))
						{
							//printf("Able to find block\n");

							L2CacheRecord.readMisses -= 1;
							L2CacheRecord.writebacks += 1;
							memoryTraffic += 1;
							
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].index = tempCacheAddrs.uiIndexAddr;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit = 1;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit = 0;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].tag = tempCacheAddrs.uiTagAddr;
							L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].offset = tempCacheAddrs.uiOffsetAddr;


							updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU

							return 0;//break;
						}
						else
						{
							//printf("Not Able to find block\n");

							// Space is available
							for (j = 0; j < temp_param.PREF_N; j++)
							{
								if (pfetch[j]->valid == 0)
								{
									//printf("Space is available\n");
									prefetch_all(j, L2BitsRecords);
									pfetch[j]->valid = 1;
									//update_pfetch_lru(j);

									L2CacheRecord.writebacks += 1;
									memoryTraffic += 2;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].index = tempCacheAddrs.uiIndexAddr;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit = 1;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit = 0;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].tag = tempCacheAddrs.uiTagAddr;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].offset = tempCacheAddrs.uiOffsetAddr;

									updateLru(L2CacheAddrs.uiIndexAddr,i,"L2"); // update LRU
									return 0;
								}
							}

							// Space is Not available
							for (j = 0; j < temp_param.PREF_N; j++)
							{
								//printf("Space is not available\n");
								if (pfetch[j]->lru == temp_param.PREF_N -1)
								{
									//printf("Space do empty through LRU pfetch[i]->lru  =%d\n",pfetch[j]->lru);
									prefetch_all(j, L2BitsRecords);
									//update_pfetch_lru(j);

									L2CacheRecord.writebacks += 1;
									memoryTraffic += 2;

									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].index = tempCacheAddrs.uiIndexAddr;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit = 1;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit = 0;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].tag = tempCacheAddrs.uiTagAddr;
									L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].offset = tempCacheAddrs.uiOffsetAddr;

									updateLru(tempCacheAddrs.uiIndexAddr,i,"L2"); // update LRU
									return 0;
								}
							}
						}
					}
				}



					// End //

					L2CacheRecord.writebacks += 1;
					memoryTraffic += 2;
					L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].index = tempCacheAddrs.uiIndexAddr;
					L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].validBit = 1;
					L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].dirtyBit = 0;
					L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].tag = tempCacheAddrs.uiTagAddr;
					L2CacheBlock[tempCacheAddrs.uiIndexAddr][i].offset = tempCacheAddrs.uiOffsetAddr;

					updateLru(tempCacheAddrs.uiIndexAddr,i,"L2");// update LRU
					break;
				}
				else{}
			}
			flagSpaceNotAvailable = 0;
		}
	}
	return 0;
	//printf("Writeback Done\n");
}

void prefetch_all(int n,cache_bits_records bit_records)
{
	//printf("In prefetch All\n");

	int j,temp1, temp2, prefetch_addr, prefetch_tag, prefetch_index;

	prefetch_addr = temp_addr >> bit_records.uiBlockOffsetBits;

	for (j = 0; j < temp_param.PREF_M; j++)
	{
		//printf("Inside prefetch_all loop j = %d\n",j);

		prefetch_addr += 1;
		temp1 = temp2 = prefetch_addr;
		prefetch_tag = temp1 >> bit_records.uiIndexBits;
		prefetch_index = ((1 << bit_records.uiIndexBits) - 1) & (temp2);

		pfetch[n][j].sb_addr = prefetch_addr;
		pfetch[n][j].tag = prefetch_tag;
		pfetch[n][j].index = prefetch_index;
		mem_fetch += 1;

		//printf("pfetch[n][j].sb_addr =%x\n",pfetch[n][j].sb_addr);
	}

	update_pfetch_lru(n);

}

void prefetch_required(int p, int j, cache_bits_records bit)
{
	int  k, temp1, temp2,prefetch_addr, prefetch_tag, prefetch_index;
	int m = 1,i=0;

	// prefetch_addr= pfetch[p][j].sb_addr;
	prefetch_addr = pfetch[p][temp_param.PREF_M - 1].sb_addr;

	while (m <= (j + 1))
	{
		for (i = 0; i < temp_param.PREF_M - 1; i++)
		{
			pfetch[p][i].sb_addr = pfetch[p][i + 1].sb_addr;
			pfetch[p][i].tag = pfetch[p][i + 1].tag;
			pfetch[p][i].index = pfetch[p][i + 1].index;
		}
		m++;
	}

	k = temp_param.PREF_M - j - 1;

	for (k; k < temp_param.PREF_M; k++)
	{
		prefetch_addr += 1;
		temp1 = temp2 = prefetch_addr;
		prefetch_tag = temp1 >> bit.uiIndexBits;
		prefetch_index = ((1 << bit.uiIndexBits) - 1) & (temp2);

		pfetch[p][k].sb_addr = prefetch_addr;
		pfetch[p][k].tag = prefetch_tag;
		pfetch[p][k].index = prefetch_index;
		mem_fetch += 1;
	}
}

void prefetch_all_req(int p, cache_bits_records bit)
{
	int j,temp1, temp2, prefetch_tag, prefetch_index, prefetch_addr;

	prefetch_addr = pfetch[p][temp_param.PREF_M - 1].sb_addr;

	for (j = 0; j < temp_param.PREF_M; j++)
	{
		prefetch_addr += 1;
		temp1 = temp2 = prefetch_addr;
		prefetch_tag = temp1 >> bit.uiIndexBits;
		prefetch_index = ((1 << bit.uiIndexBits) - 1) & (temp2);

		pfetch[p][j].sb_addr = prefetch_addr;
		pfetch[p][j].tag = prefetch_tag;
		pfetch[p][j].index = prefetch_index;
		mem_fetch += 1;

		//printf("In prefetch_all_req add=%x\n",pfetch[p][j].sb_addr);
	}
}

void update_pfetch_lru(int i)
{
	//printf("In update_pfetch_lru i =%d\n ",i);

	uint32_t tempLru = 0,j=0;	

	tempLru = pfetch[i]->lru;

	for(j=0;j<temp_param.PREF_N;j++)
	{
		if(pfetch[j]->lru < tempLru)
		{
			pfetch[j]->lru += 1;
		}
	}
	pfetch[i]->lru = 0;

	//printf("After update_pfetch_lru\n");
}

int find_block_pfetch(cache_addrs addr, cache_bits_records bit_records)
{
	int i, j, k=0, found = 0;
	while (k < temp_param.PREF_N)
	{
		for (i = 0; i < temp_param.PREF_N; i++)
		{
			if (pfetch[i]->lru == k)
			{
				for (j = 0; j < temp_param.PREF_M; j++)
				{
					if (pfetch[i][j].index == addr.uiIndexAddr && (pfetch[i][j].tag == addr.uiTagAddr)) // block found in buffer
					{
						//printf("In find_block_pfetch its found addr.uiIndexAddr =%x and Tag=%x\n",addr.uiIndexAddr,addr.uiTagAddr);
						found = 1;

						if (j != temp_param.PREF_M - 1)
						{
							//printf("Not a last index\n");
							prefetch_required(i, j, bit_records);
						}
						else
						{
							//printf("Last Index\n");
							prefetch_all_req(i, bit_records);
						}
						//printf("Rohan Lru for block pfetch[i].lru=%d and Add=%x\n",pfetch[i]->lru,pfetch[i][j].sb_addr);
						update_pfetch_lru(i);
						//printf("After update Rohan Lru for block pfetch[i].lru=%d and Add=%x\n",pfetch[i]->lru,pfetch[i][j].sb_addr);
						return found;
					}
				}
				k++;
			}
		}
	}
	return found;
}


int readL1Cache()
{
	//printf("In read L1\n");

	uint8_t flagSpaceNotAvailable = 1;
	uint32_t i=0,j=0;

	/// Read Hit Scenario ///
	for(i=0;i<L1BitsRecords.uiAssoc;i++)
	{  
		//printf("Checking index =%d Tag =%x v=%d ,D = %d, LRU =%d\n",L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index,L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag,
		//L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit,L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit,L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].lru);

		if( L1CacheAddrs.uiTagAddr == L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag)// && L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit == 1) // Read Hit
		{
			//printf("L1 Read hit\n");

			find_block_pfetch(L1CacheAddrs, L1BitsRecords);
			L1CacheRecord.readHits += 1;
			// LRU Counter Update; Not update Dirty and Valid bit
			updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");

			break;
		}
	}
	/// Read Miss Scenario ///
	if(i == L1BitsRecords.uiAssoc)
	{
		L1CacheRecord.readMisses += 1;

		for(i=0;i<L1BitsRecords.uiAssoc;i++)
		{
			//printf("RM L1CacheAddrs.uiIndexAddr %x\n",L1CacheAddrs.uiIndexAddr);
			//printf("index =%d Tag =%x v=%d ,D = %d, LRU =%d\n",L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index,L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag,
			//	L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit,L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit,L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].lru);

			// 1. Checking in particular Index(set) valid bit is 0 means space is available
			if(L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].lru == (L1BitsRecords.uiAssoc -1) && L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit == 0)
			{
				flagSpaceNotAvailable = 0;
				//printf("L1 Read Miss v=0\n");

				if(temp_param.L2_SIZE != 0)
				{
					readL2Cache();
				}
				else if(temp_param.PREF_N != 0 && temp_param.L2_SIZE == 0)
				{

					int n=0;
					int emptyStreamBuff = 0;

					//printf(" temp_param.PREF_N =%d\n",temp_param.PREF_N);

					for(n=0;n<temp_param.PREF_N;n++)
					{
						//printf("pfetch[n]->valid = %d and  emptyStreamBuff =%d\n",pfetch[n]->valid,emptyStreamBuff);

						if(pfetch[n]->valid != 0)
						{
							emptyStreamBuff = 1;
							break;
						}
						//printf("pfetch[n]->valid = %d and  emptyStreamBuff =%d\n",pfetch[n]->valid,emptyStreamBuff);
					}

					if(emptyStreamBuff == 0)  // All stream buffer is empty
					{
						//printf("Buffer Empty\n");

						if(pfetch[0]->valid == 0)
						{
							//printf("Buffer Empty and Valid 0\n");
							prefetch_all(0,L1BitsRecords);
							pfetch[0]->valid = 1;

							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

							updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU

							memoryTraffic +=1;
							return 0;
						}	
					}
					else  // Atleast one will be full
					{
						//printf("Atleast one Buffer full\n");
						if(find_block_pfetch(L1CacheAddrs, L1BitsRecords))
						{
							//printf("Able to find block\n");

							L1CacheRecord.readMisses -= 1;
							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

							updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU

							return 0;//break;
						}
						else
						{
							//printf("Not Able to find block\n");

							// Space is available
							for (j = 0; j < temp_param.PREF_N; j++)
							{
								if (pfetch[j]->valid == 0)
								{
								//	printf("Space is available\n");
									prefetch_all(j, L1BitsRecords);
									pfetch[j]->valid = 1;
									//update_pfetch_lru(j);

									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

									updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
									memoryTraffic +=1;
									return 0;
								}
							}

							// Space is Not available
							for (j = 0; j < temp_param.PREF_N; j++)
							{
								//printf("Space is not available\n");
								if (pfetch[j]->lru == temp_param.PREF_N -1)
								{
									//printf("Space do empty through LRU pfetch[i]->lru  =%d\n",pfetch[j]->lru);
									prefetch_all(j, L1BitsRecords);
									//update_pfetch_lru(j);

									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

									updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
									memoryTraffic +=1;
									return 0;
								}
							}
						}
					}

				}
				else
				{
					memoryTraffic +=1;
				}

				L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
				L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
				L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
				L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
				L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

				updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU

				break;
			}
		}

		if(flagSpaceNotAvailable)  // Space is not available in Cache
		{
			for(i=0;i<L1BitsRecords.uiAssoc;i++)
			{
				// lru is 0 and D=0 so directly replace the block
				if(L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].lru == (L1BitsRecords.uiAssoc - 1) && L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit == 0)
				{
					//printf("L1 Read Miss valid 1 and D =0\n");

					if(temp_param.L2_SIZE != 0)
					{
						readL2Cache();

					}
					else if(temp_param.PREF_N != 0 && temp_param.L2_SIZE == 0)
					{

						int n=0;
						int emptyStreamBuff = 0;


						for(n=0;n<temp_param.PREF_N;n++)
						{
							if(pfetch[n]->valid != 0)
							{
								emptyStreamBuff = 1;
								break;
							}
						}

						if(emptyStreamBuff == 0)  // All stream buffer is empty
						{
							//printf("Buffer Empty\n");
							if(pfetch[0]->valid == 0)
							{
								//printf("Buffer Empty Valid 0\n");
								prefetch_all(0,L1BitsRecords);
								pfetch[0]->valid = 1;

								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

								updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
								memoryTraffic += 1;
								return 0;
							}	
						}
						else  // Atleast one will be full
						{
							//printf("Atleast one Buffer full\n");

							if(find_block_pfetch(L1CacheAddrs, L1BitsRecords))
							{
								//printf("Able to find block\n");
								L1CacheRecord.readMisses -= 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

								updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
								return 0;
							}
							else
							{
								//printf("Not Able to find block\n");
								// Space is available
								for (j = 0; j < temp_param.PREF_N; j++)
								{
									if (pfetch[j]->valid == 0)
									{
										//printf("Space is available\n");
										prefetch_all(j, L1BitsRecords);
										pfetch[j]->valid = 1;
										//update_pfetch_lru(j);

										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

										updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
										memoryTraffic += 1;
										return 0;
									}
								}

								// Space is Not available
								for (j = 0; j < temp_param.PREF_N; j++)
								{
									//printf("Space is not available\n");
									if (pfetch[j]->lru == temp_param.PREF_N -1)
									{
										//printf("Space do empty through LRU pfetch[i]->lru  =%d\n",pfetch[j]->lru);
										prefetch_all(j, L1BitsRecords);
										//update_pfetch_lru(j);

										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

										updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
										memoryTraffic += 1;
										return 0;
									}
								}
							}
						}
					}
					else
					{
						memoryTraffic += 1;
					}

					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

					updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU

					break;

				}
				// lru is 0 and D=1 so clean and replace the block
				else if(L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].lru == (L1BitsRecords.uiAssoc - 1) && L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit == 1)
				{
					//printf("L1 Read Miss valid 1 and D =1\n");
					// For cleaning need to do memory write and after read data also doing memory operation so +2

					L1CacheRecord.writebacks += 1;

					if(temp_param.L2_SIZE != 0)
					{
						L1WritebackToL2(L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag, L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index,L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset);
						readL2Cache();
					}
					else if(temp_param.PREF_N != 0 && temp_param.L2_SIZE == 0)
					{

						int n=0;
						int emptyStreamBuff = 0;

						for(n=0;n<temp_param.PREF_N;n++)
						{

							if(pfetch[n]->valid != 0)
							{
								emptyStreamBuff = 1;
								break;
							}
						}

						if(emptyStreamBuff == 0)  // All stream buffer is empty
						{
							//printf("Buffer Empty\n");
							if(pfetch[0]->valid == 0)
							{
								//printf("Buffer Empty Valid 0\n");
								prefetch_all(0,L1BitsRecords);
								pfetch[0]->valid = 1;

								L1CacheRecord.readMisses -= 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

								updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
								memoryTraffic += 2;
								return 0;
							}	
						}
						else  // Atleast one will be full
						{
							//printf("Atleast one Buffer full\n");
							if(find_block_pfetch(L1CacheAddrs, L1BitsRecords))
							{
								//printf("Able to find block\n");
								L1CacheRecord.readMisses -= 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

								updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU

								return 0;
							}
							else
							{
								//printf("Not Able to find block\n");
								// Space is available
								for (j = 0; j < temp_param.PREF_N; j++)
								{

									if (pfetch[j]->valid == 0)
									{
										//printf("Space is available\n");
										prefetch_all(j, L1BitsRecords);
										pfetch[j]->valid = 1;
										//update_pfetch_lru(j);

										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

										updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
										memoryTraffic += 2;
										return 0;
									}
								}

								// Space is Not available
								for (j = 0; j < temp_param.PREF_N; j++)
								{
									//printf("Space is not available\n");
									if (pfetch[j]->lru == temp_param.PREF_N -1)
									{
										//printf("Space do empty through LRU pfetch[i]->lru  =%d\n",pfetch[j]->lru);
										prefetch_all(j, L1BitsRecords);
										//update_pfetch_lru(j);

										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

										updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
										memoryTraffic += 2;
										return 0;
									}
								}
							}
						}
					}
					else
					{	
						memoryTraffic += 2;
					}

					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 0;
					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

					updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
					break;
				}
				else{}
			}
			flagSpaceNotAvailable = 0;
		}
	}
	return 0;
}

int writeL1Cache()
{
	//printf("In write L1\n");

	uint8_t flagSpaceNotAvailable = 1;
	uint32_t i=0,j=0;

	/// Write Hit Scenario ///
	for(i=0;i<L1BitsRecords.uiAssoc;i++)
	{
		if( L1CacheAddrs.uiTagAddr == L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag)// && L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit == 1) 
		{
			//printf("Write Hit\n");
			find_block_pfetch(L1CacheAddrs, L1BitsRecords);

			L1CacheRecord.writeHits += 1;

			L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 1;
			// LRU Counter Update;
			updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");
			break;
		}
	}

	/// Write Miss Scenario ///
	if(i == L1BitsRecords.uiAssoc)
	{
		L1CacheRecord.writeMisses += 1;

		for(i=0;i<L1BitsRecords.uiAssoc;i++)
		{
			// 1. Checking in particular Index(set) valid bit is 0 means space is available
			if(L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].lru == (L1BitsRecords.uiAssoc -1) && L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit == 0)
			{
				flagSpaceNotAvailable = 0;
				//printf(" L1 Write Miss V =0 \n");
				if(temp_param.L2_SIZE != 0)
				{
					readL2Cache();
				}
				else if(temp_param.PREF_N != 0 && temp_param.L2_SIZE == 0)
				{
					//printf("temp_param.PREF_N is not Zero\n");

					int n=0;
					int emptyStreamBuff = 0;

					for(n=0;n<temp_param.PREF_N;n++)
					{
						//printf("pfetch[n]->valid = %d and  emptyStreamBuff =%d\n",pfetch[n]->valid,emptyStreamBuff);
						if(pfetch[n]->valid != 0)
						{
							emptyStreamBuff = 1;
							break;
						}
					}

					if(emptyStreamBuff == 0)  // All stream buffer is empty
					{
						//printf("Buffer Empty\n");
						if(pfetch[0]->valid == 0)
						{
							//printf("Buffer Empty Valid 0\n");
							prefetch_all(0,L1BitsRecords);
							pfetch[0]->valid = 1;

							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 1;
							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;
							updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
							memoryTraffic += 1;

							return 0;
						}	

						//printf("pfetch[0]->valid = %d and  emptyStreamBuff =%d\n",pfetch[0]->valid,emptyStreamBuff);
					}
					else  // Atleast one will be full
					{
						//printf("Atleast one Buffer full\n");
						if(find_block_pfetch(L1CacheAddrs, L1BitsRecords))
						{
							//printf("Able to find block\n");
							L1CacheRecord.writeMisses -= 1;
							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 1;
							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
							L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;
							updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU

							return 0;
						}
						else
						{
							//printf("Not Able to find block\n");
							// Space is available
							for (j = 0; j < temp_param.PREF_N; j++)
							{
								if (pfetch[j]->valid == 0)
								{
									//printf("Space is available\n");
									prefetch_all(j, L1BitsRecords);
									pfetch[j]->valid = 1;
									//update_pfetch_lru(j);

									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 1;
									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

									updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
									memoryTraffic += 1;

									return 0;
								}
							}

							// Space is Not available
							for (j = 0; j < temp_param.PREF_N; j++)
							{
								//printf("Space is not available\n");
								if (pfetch[j]->lru == temp_param.PREF_N -1)
								{
									//printf("Space do empty through LRU pfetch[j]->lru  =%d\n",pfetch[j]->lru);
									prefetch_all(j, L1BitsRecords);
									//update_pfetch_lru(j);

									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 1;
									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
									L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

									updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
									memoryTraffic += 1;

									return 0;
								}
							}
						}
					}

				}
				else
				{
					memoryTraffic += 1;
				}

				L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
				L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
				L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 1;
				L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
				L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

				updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");   // update LRU


				break;
			}
		}

		if(flagSpaceNotAvailable)  // Space is not available in Cache
		{
			for(i=0;i<L1BitsRecords.uiAssoc;i++)
			{
				// lru is 0 and D=0 so directly replace the block
				if(L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].lru == (L1BitsRecords.uiAssoc -1) && L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit == 0)
				{
					//printf("L1 Write Miss V =1 D=0 \n");

					if(temp_param.L2_SIZE != 0)
					{
						readL2Cache();
					}
					else if(temp_param.PREF_N != 0 && temp_param.L2_SIZE == 0)
					{

						int n=0;
						int emptyStreamBuff = 0;

						for(n=0;n<temp_param.PREF_N;n++)
						{
							if(pfetch[n]->valid != 0)
							{
								emptyStreamBuff = 1;
								break;
							}
						}

						if(emptyStreamBuff == 0)  // All stream buffer is empty
						{
							//printf("Buffer Empty\n");
							if(pfetch[0]->valid == 0)
							{
								//printf("Buffer Empty Valid 0\n");
								prefetch_all(0,L1BitsRecords);
								pfetch[0]->valid = 1;

								L1CacheRecord.writeMisses -= 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

								updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
								memoryTraffic += 1;

								return 0;
							}	
						}
						else  // Atleast one will be full
						{
							//printf("Atleast one Buffer full\n");
							if(find_block_pfetch(L1CacheAddrs, L1BitsRecords))
							{
								//printf("Able to find block\n");
								L1CacheRecord.writeMisses -= 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

								updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
								return 0;
							}
							else
							{
								//printf("Not Able to find block\n");
								// Space is available
								for (j = 0; j < temp_param.PREF_N; j++)
								{
									if (pfetch[j]->valid == 0)
									{
										//printf("Space is available\n");
										prefetch_all(j, L1BitsRecords);
										pfetch[j]->valid = 1;
										//update_pfetch_lru(j);

										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 1;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

										updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
										memoryTraffic += 1;

										return 0;
									}
								}

								// Space is Not available
								for (j = 0; j < temp_param.PREF_N; j++)
								{
									//printf("Space is not available\n");
									if (pfetch[j]->lru == temp_param.PREF_N -1)
									{
										//printf("Space do empty through LRU pfetch[i]->lru  =%d\n",pfetch[j]->lru);
										prefetch_all(j, L1BitsRecords);
										//update_pfetch_lru(j);

										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 1;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

										updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
										memoryTraffic += 1;

										return 0;
									}
								}
							}
						}
					}
					else
					{
						memoryTraffic += 1;
					}

					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 1;
					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

					// update LRU
					updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");

					break;
				}
				// lru is 0 and D=1 so clean and replace the block
				else if(L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].lru == (L1BitsRecords.uiAssoc -1) && L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit == 1)
				{
					//printf("L1 Write Miss V =1 D=1 \n");
					L1CacheRecord.writebacks += 1;

					if(temp_param.L2_SIZE != 0)
					{
						L1WritebackToL2(L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag, L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index,L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset);
						readL2Cache();
					}
					else if(temp_param.PREF_N != 0 && temp_param.L2_SIZE == 0)
					{

						int n=0;
						int emptyStreamBuff = 0;

						for(n=0;n<temp_param.PREF_N;n++)
						{
							if(pfetch[n]->valid != 0)
							{
								emptyStreamBuff = 1;
								break;
							}
						}

						if(emptyStreamBuff == 0)  // All stream buffer is empty
						{
							//printf("Buffer Empty\n");
							if(pfetch[0]->valid == 0)
							{
								//printf("Buffer Empty Valid 0\n");
								prefetch_all(0,L1BitsRecords);
								pfetch[0]->valid = 1;

								L1CacheRecord.writeMisses -= 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

								updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
								memoryTraffic += 2;

								return 0;
							}	
						}
						else  // Atleast one will be full
						{
							//printf("Atleast one Buffer full\n");
							if(find_block_pfetch(L1CacheAddrs, L1BitsRecords))
							{
								//printf("Able to find block\n");
								L1CacheRecord.writeMisses -= 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 1;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
								L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

								updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU

								return 0;
							}
							else
							{
								//printf("Not Able to find block\n");
								// Space is available
								for (j = 0; j < temp_param.PREF_N; j++)
								{
									if (pfetch[j]->valid == 0)
									{
										//printf("Space is available\n");
										prefetch_all(j, L1BitsRecords);
										pfetch[j]->valid = 1;
										//update_pfetch_lru(j);

										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 1;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

										updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
										memoryTraffic += 2;
										return 0;
									}
								}

								// Space is Not available
								for (j = 0; j < temp_param.PREF_N; j++)
								{
									//printf("Space is not available\n");
									if (pfetch[j]->lru == temp_param.PREF_N -1)
									{
										//printf("Space do empty through LRU pfetch[i]->lru  =%d\n",pfetch[j]->lru);
										prefetch_all(j, L1BitsRecords);
										//update_pfetch_lru(j);

										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 1;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
										L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

										updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");// update LRU
										memoryTraffic += 2;
										return 0;
									}
								}
							}
						}
					}
					else
					{
						memoryTraffic += 2;
					}

					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].index = L1CacheAddrs.uiIndexAddr;
					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].validBit = 1;
					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].dirtyBit = 1;
					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].tag = L1CacheAddrs.uiTagAddr;
					L1CacheBlock[L1CacheAddrs.uiIndexAddr][i].offset = L1CacheAddrs.uiOffsetAddr;

					// update LRU
					updateLru(L1CacheAddrs.uiIndexAddr,i,"L1");
					break;
				}
				else{}
			}
			flagSpaceNotAvailable = 0;
		}
	}
	return 0;
}

int main (int argc, char *argv[]) 
{
	FILE *fp;			// File pointer.
	char *trace_file;		// This variable holds the trace file name.
	cache_params_t params;	// Look at the sim.h header file for the definition of struct cache_params_t.
	char rw;			// This variable holds the request's type (read or write) obtained from the trace.
	uint32_t addr;		// This variable holds the request's address obtained from the trace.
	// The header file <inttypes.h> above defines signed and unsigned integers of various sizes in a machine-agnostic way.  "uint32_t" is an unsigned integer of 32 bits.


	// Exit with an error if the number of command-line arguments is incorrect.
	if (argc != 9) {
		printf("Error: Expected 8 command-line arguments but was provided %d.\n", (argc - 1));
		exit(EXIT_FAILURE);
	}

	// "atoi()" (included by <stdlib.h>) converts a string (char *) to an integer (int).
	params.BLOCKSIZE = (uint32_t) atoi(argv[1]);
	params.L1_SIZE   = (uint32_t) atoi(argv[2]);
	params.L1_ASSOC  = (uint32_t) atoi(argv[3]);
	params.L2_SIZE   = (uint32_t) atoi(argv[4]);
	params.L2_ASSOC  = (uint32_t) atoi(argv[5]);
	params.PREF_N    = (uint32_t) atoi(argv[6]);
	params.PREF_M    = (uint32_t) atoi(argv[7]);
	trace_file       = argv[8];

	temp_param = params;

	// Open the trace file for reading.
	fp = fopen(trace_file, "r");
	if (fp == (FILE *) NULL) {
		// Exit with an error if file open failed.
		printf("Error: Unable to open file %s\n", trace_file);
		exit(EXIT_FAILURE);
	}

	// Print simulator configuration.
	printf("===== Simulator configuration =====\n");
	printf("BLOCKSIZE:  %u\n", params.BLOCKSIZE);
	printf("L1_SIZE:    %u\n", params.L1_SIZE);
	printf("L1_ASSOC:   %u\n", params.L1_ASSOC);
	printf("L2_SIZE:    %u\n", params.L2_SIZE);
	printf("L2_ASSOC:   %u\n", params.L2_ASSOC);
	printf("PREF_N:     %u\n", params.PREF_N);
	printf("PREF_M:     %u\n", params.PREF_M);
	printf("trace_file: %s\n", trace_file);
	//printf("===================================\n");
	printf("\n");

	addressBitsSeprationFun(params);

	// Read requests from the trace file and echo them back.
	while (fscanf(fp, "%c %x\n", &rw, &addr) == 2) 
	{	// Stay in the loop if fscanf() successfully parsed two tokens as specified.
		//printf("%c %x\n",rw,addr);
		temp_addr = addr;
		seprateAddrsFun(addr,params);


		if (rw == 'r')
		{
			//printf("r %x\n", addr);
			L1CacheRecord.reads += 1;
			readL1Cache();
			//displayPrefetch();
		} 
		else if (rw == 'w')
		{
			//printf("w %x\n", addr);
			L1CacheRecord.writes += 1;
			writeL1Cache();
			//displayPrefetch();
		}
		else 
		{
			printf("Error: Unknown request type %c.\n", rw);
			exit(EXIT_FAILURE);
		}
	}

	displayFunc(params.L1_SIZE,params.L2_SIZE);
	return(0);
}

void testdisplay()
{
	int i =0,j=0;

	for(i=0;i<L1BitsRecords.uiSets;i++)
	{
		//printf("Set index =%x\n",L1CacheBlock[i]);

		for(j=0;j<L1BitsRecords.uiAssoc;j++)
		{ 
			printf("index =%x Tag =%x v=%d ,D = %d, LRU =%d\n",L1CacheBlock[i][j].index,L1CacheBlock[i][j].tag,
					L1CacheBlock[i][j].validBit,L1CacheBlock[i][j].dirtyBit,L1CacheBlock[i][j].lru);
		}
	}

}

void sortingLru(uint32_t L1_Size,uint32_t L2_Size)
{
	uint32_t i =0,j=0,k=0;
	cache_block tempCacheBlock = {0}; 

	if(L1_Size != 0)
	{
		for (i = 0; i < L1BitsRecords.uiSets; i++) 
		{
			for(j=0;j<L1BitsRecords.uiAssoc;j++)
			{
				for (k = j + 1; k < L1BitsRecords.uiAssoc; k++)
				{
					if (L1CacheBlock[i][j].lru > L1CacheBlock[i][k].lru) 
					{
						tempCacheBlock =  L1CacheBlock[i][j];
						L1CacheBlock[i][j] = L1CacheBlock[i][k];
						L1CacheBlock[i][k] = tempCacheBlock;
					}
				}
			}
		}
	}

	memcpy(&tempCacheBlock,NULL,sizeof(cache_block));

	if(L2_Size != 0)
	{
		for (i = 0; i < L2BitsRecords.uiSets; i++) 
		{
			for(j=0;j<L2BitsRecords.uiAssoc;j++)
			{
				for (k = j + 1; k < L2BitsRecords.uiAssoc; k++)
				{
					if (L2CacheBlock[i][j].lru > L2CacheBlock[i][k].lru) 
					{
						tempCacheBlock =  L2CacheBlock[i][j];
						L2CacheBlock[i][j] = L2CacheBlock[i][k];
						L2CacheBlock[i][k] = tempCacheBlock;
					}
				}
			}
		}
	}

}

void sortingPrefetchLru()
{
	int i=0,j=0;
	pfetch_block *temp_Pblock = {0};
	if(temp_param.PREF_N != 0)
	{
		for(i=0;i<temp_param.PREF_N -1 ;i++)
		{
			for (j = 0; j < temp_param.PREF_N - 1 - i; j++)
			{
				if(pfetch[j]->lru > pfetch[j+1]->lru)
				{
					temp_Pblock = pfetch[j];
					pfetch[j] = pfetch[j+1];
					pfetch[j+1] = temp_Pblock;
				}
			}
		}
	}
}  

void displayPrefetch()
{
	int i = 0,j=0;
	if(temp_param.PREF_N !=0)
	{

		printf("\n");
		printf("===== Stream Buffer(s) contents =====\n");
		for (i = 0; i < temp_param.PREF_N; i++)
		{
			//printf("pfetch[i].lru =%d\n",pfetch[i]->lru);

			for (j = 0; j < temp_param.PREF_M; j++)
			{
				printf("%8x ", pfetch[i][j].sb_addr);
				printf("\n");
			}
		}
	}
}

void displayFunc(uint32_t L1_Size,uint32_t L2_Size)
{
	int i =0,j=0;

	sortingLru(L1_Size,L2_Size);
	printf("===== L1 contents =====\n");

	if(L1_Size != 0)
	{
		for(i=0;i<L1BitsRecords.uiSets;i++)
		{
			if (i <= 9)
				printf("set      %d: ", i);
			else if (i >= 10 && i <= 99)
				printf("set     %d: ", i);
			else if (i >= 100 && i <= 999)
				printf("set    %d: ", i);

			for(j=0;j<L1BitsRecords.uiAssoc;j++)
			{ 
				printf("%8x ",L1CacheBlock[i][j].tag);

				if(L1CacheBlock[i][j].dirtyBit == 1)
				{
					printf("D");
				}
				else
				{
					printf(" ");
				}
			}
			printf("\n");
		}
	}

	if (L2_Size != 0)
	{
		printf("\n===== L2 contents =====\n");

		for (i = 0; i < L2BitsRecords.uiSets; i++)
		{
			if (i <= 9)
				printf("set      %d: ", i);
			else if (i >= 10 && i <= 99)
				printf("set     %d: ", i);
			else if (i >= 100 && i <= 999)
				printf("set    %d: ", i);
			for (j = 0; j < L2BitsRecords.uiAssoc; j++)
			{
				printf("%8x ", L2CacheBlock[i][j].tag);

				if (L2CacheBlock[i][j].dirtyBit == 1)
				{
					printf("D");
				}
				else
				{
					printf(" ");
				}
			}
			printf("\n");
		}
	}

	if(temp_param.PREF_N !=0)
	{

		sortingPrefetchLru();

		printf("\n");
		printf("===== Stream Buffer(s) contents =====\n");
		for (i = 0; i < temp_param.PREF_N; i++)
		{
			for (j = 0; j < temp_param.PREF_M; j++)
			{
				printf("%8x ", pfetch[i][j].sb_addr);
			}
			printf("\n");
		}
	}

	L1CacheRecord.missRate = (float)(((float)L1CacheRecord.readMisses + (float)L1CacheRecord.writeMisses) / ((float)L1CacheRecord.reads + (float)L1CacheRecord.writes));

	if(temp_param.L2_SIZE !=0)
	{
		L2CacheRecord.missRate = (float)((float)L2CacheRecord.readMisses / (float)L2CacheRecord.reads);
	}
	else
	{
		L2CacheRecord.missRate = 0;
	}

	if (temp_param.L2_SIZE == 0)
	{
		memoryTraffic = L1CacheRecord.readMisses + L1CacheRecord.writeMisses + L1CacheRecord.writebacks;
	}
	else
	{
		memoryTraffic = L2CacheRecord.readMisses + L2CacheRecord.writeMisses + L2CacheRecord.writebacks;
	}

	printf("\n===== Measurements =====\n");
	printf("a. L1 reads:                   %d\n", L1CacheRecord.reads);
	printf("b. L1 read misses:             %d\n", L1CacheRecord.readMisses);
	printf("c. L1 writes:                  %d\n", L1CacheRecord.writes);
	printf("d. L1 write misses:            %d\n", L1CacheRecord.writeMisses);
	printf("e. L1 miss rate:               %.4f\n",L1CacheRecord.missRate);
	printf("f. L1 writebacks:              %d\n", L1CacheRecord.writebacks);
	if (temp_param.PREF_N != 0 && L2_Size == 0)
	{
		printf("g. L1 prefetches:              %d\n", mem_fetch);
	}
	else
	{
		printf("g. L1 prefetches:              %d\n", 0);
	}
	printf("h. L2 reads (demand):          %d\n", L2CacheRecord.reads);
	printf("i. L2 read misses (demand):    %d\n", L2CacheRecord.readMisses);
	printf("j. L2 reads (prefetch):        %d\n", 0);
	printf("k. L2 read misses (prefetch):  %d\n", 0);
	printf("l. L2 writes:                  %d\n", L2CacheRecord.writes);
	printf("m. L2 write misses:            %d\n", L2CacheRecord.writeMisses);
	printf("n. L2 miss rate:               %.4f\n", L2CacheRecord.missRate);
	printf("o. L2 writebacks:              %d\n", L2CacheRecord.writebacks);
	if (temp_param.PREF_N != 0 && L2_Size == 0)
    {
		printf("p. L2 prefetches:              %d\n", 0);
    }
	else
    {
		printf("p. L2 prefetches:              %d\n", mem_fetch);
	}
	printf("q. memory traffic:             %d\n", memoryTraffic+mem_fetch);
}

