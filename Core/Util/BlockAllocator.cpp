#include "BlockAllocator.h"

// Slow freaking thing but works (eventually) :)

BlockAllocator::BlockAllocator()
{
	blocks.clear();
}

BlockAllocator::~BlockAllocator()
{
	Shutdown();
}

void BlockAllocator::Init(u32 _rangeStart, u32 _rangeSize)
{
	Shutdown();
	//Initial block, covering everything
	blocks.push_back(Block(_rangeStart, _rangeSize, false));
}


void BlockAllocator::Shutdown()
{
	blocks.clear();
}

u32 BlockAllocator::Alloc(u32 &size, bool fromEnd, const char *tag)
{
	//upalign size
	size = (size+15) & ~15;

	if (!fromEnd)
	{
		//Allocate from start of mem
		for (std::list<Block>::iterator iter = blocks.begin(); iter != blocks.end(); iter++)
		{
			BlockAllocator::Block &b = *iter;
			if (b.taken == false && b.size >= size)
			{
				if (b.size == size)
				{
					b.taken = true;
					b.SetTag(tag);
					return b.start;
				}
				else
				{
					blocks.insert(++iter, Block(b.start+size, b.size-size, false));
					b.taken = true;
					b.size = size;
					b.SetTag(tag);
					return b.start;
				}
				//Got one!
			}
		}
	}
	else
	{
		for (std::list<Block>::reverse_iterator iter = blocks.rbegin(); iter != blocks.rend(); ++iter)
		{
			std::list<Block>::reverse_iterator hey = iter;
			BlockAllocator::Block &b = *((++hey).base()); //yes, confusing syntax. reverse_iterators are confusing
			if (b.taken == false && b.size >= size)
			{
				if (b.size == size)
				{
					b.taken = true;
					b.SetTag(tag);
					return b.start;
				}
				else
				{
					blocks.insert(hey.base(), Block(b.start, b.size-size, false));
					b.taken = true;
					b.start += b.size-size;
					b.size = size;
					b.SetTag(tag);
					return b.start;
				}
				//Got one!
			}
		}
	}

	//Out of memory :(
	ListBlocks();
	ERROR_LOG(HLE, "Block Allocator failed to allocate %i (%08x) bytes of contiguous memory", size, size);
	return -1;
}

u32 BlockAllocator::AllocAt(u32 position, u32 size, const char *tag)
{
	//upalign size
	size=(size+15) & ~15;
	std::list<Block>::iterator iter = GetBlockIterFromAddress(position);
	if (iter != blocks.end())
	{
		Block &b = *iter;
		if (b.taken)
		{
			ERROR_LOG(HLE, "Block allocator AllocAt failed, block taken! %08x, %i", position, size);
			return -1;
		}
		else
		{
			//good to go
			if (b.start == position)
			{
				blocks.insert(++iter, Block(b.start + size, b.size - size, false));
				b.taken = true;
				b.size = size;
				b.SetTag(tag);
				return position;
			}
			else
			{
				int size1 = position - b.start;
				blocks.insert(iter, Block(b.start, size1, false));
				if (b.start + b.size > position + size)
				{
					iter++;
					blocks.insert(iter, Block(position + size, b.size - (size + size1), false));
				}
				b.taken = true;
				b.start = position;
				b.size = size;
				b.SetTag(tag);
				return position;
			}
		}
	}
	else
	{
		ERROR_LOG(HLE, "Block allocator AllocAt failed :( %08x, %i", position, size);
	}

	
	//Out of memory :(
	ListBlocks();
	ERROR_LOG(HLE, "Block Allocator failed to allocate %i bytes of contiguous memory", size);
	return -1;
}

void BlockAllocator::MergeFreeBlocks()
{
	bool found;
	do
	{
		found = false;
		std::list<Block>::iterator iter1, iter2;
		iter1 = blocks.begin();
		iter2 = blocks.begin();
		iter2++;
		while (iter2 != blocks.end())
		{
			BlockAllocator::Block &b1 = *iter1;
			BlockAllocator::Block &b2 = *iter2;
			
			if (b1.taken == false && b2.taken == false)
			{
				DEBUG_LOG(HLE, "Block Alloc found adjacent free blocks - merging");
				b1.size += b2.size;
				blocks.erase(iter2);
				found = true;
				break; //iterators now invalid - we have to restart our search
			}
			iter1++;
			iter2++;
		}
	} while (found);
}

void BlockAllocator::Free(u32 position)
{
	BlockAllocator::Block *b = GetBlockFromAddress(position);
	if (b)
	{
		b->taken = false;
		MergeFreeBlocks();
	}
	else
	{
		ERROR_LOG(HLE, "BlockAllocator : invalid free %08x", position);
	}
}

std::list<BlockAllocator::Block>::iterator BlockAllocator::GetBlockIterFromAddress(u32 addr)
{
	for (std::list<Block>::iterator iter = blocks.begin(); iter != blocks.end(); iter++)
	{
		BlockAllocator::Block &b = *iter;
		if (b.start <= addr && b.start+b.size > addr)
		{
			// Got one!
			return iter;
		}
	}
	return blocks.end();
}

BlockAllocator::Block *BlockAllocator::GetBlockFromAddress(u32 addr) 
{
	std::list<Block>::iterator iter = GetBlockIterFromAddress(addr);
	if (iter == blocks.end())
		return 0;
	else
		return &(*iter);
}


u32 BlockAllocator::GetBlockStartFromAddress(u32 addr) 
{
	Block *b = GetBlockFromAddress(addr);
	if (b)
		return b->start;
	else
		return -1;
}

u32 BlockAllocator::GetBlockSizeFromAddress(u32 addr) 
{
	Block *b = GetBlockFromAddress(addr);
	if (b)
		return b->size;
	else
		return -1;
}


void BlockAllocator::ListBlocks()
{
	INFO_LOG(HLE,"-----------");
	for (std::list<Block>::const_iterator iter = blocks.begin(); iter != blocks.end(); iter++)
	{
		const Block &b = *iter;
		INFO_LOG(HLE, "Block: %08x - %08x	size %08x	taken=%i	tag=%s", b.start, b.start+b.size, b.size, b.taken ? 1:0, b.tag);
	}
}

u32 BlockAllocator::GetLargestFreeBlockSize()
{
	u32 maxFreeBlock = 0;
	for (std::list<Block>::const_iterator iter = blocks.begin(); iter != blocks.end(); iter++)
	{
		const Block &b = *iter;
		if (!b.taken)
		{
			if (b.size > maxFreeBlock)
				maxFreeBlock = b.size;
		}
	}
	return maxFreeBlock;
}


u32 BlockAllocator::GetTotalFreeBytes()
{
	u32 sum = 0;
	for (std::list<Block>::const_iterator iter = blocks.begin(); iter != blocks.end(); iter++)
	{
		const Block &b = *iter;
		if (!b.taken)
		{
			sum += b.size;
		}
	}
	return sum;
}
