	/*
	* Buddy Page Allocation Algorithm
	* SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (2)
	*/

	/*
	* STUDENT NUMBER: s1786987
	*/
	#include <infos/mm/page-allocator.h>
	#include <infos/mm/mm.h>
	#include <infos/kernel/kernel.h>
	#include <infos/kernel/log.h>
	#include <infos/util/math.h>
	#include <infos/util/printf.h>

	using namespace infos::kernel;
	using namespace infos::mm;
	using namespace infos::util;

	#define MAX_ORDER	17

	/**
	* A buddy page allocation algorithm.
	*/
	class BuddyPageAllocator : public PageAllocatorAlgorithm
	{
	private:
		/**
		* Returns the number of pages that comprise a 'block', in a given order.
		* @param order The order to base the calculation off of.
		* @return Returns the number of pages in a block, in the order.
		*/
		static inline constexpr uint64_t pages_per_block(int order)
		{
			/* The number of pages per block in a given order is simply 1, shifted left by the order number.
			* For example, in order-2, there are (1 << 2) == 4 pages in each block.
			*/
			return (1 << order);
		}

		/**
		* Returns TRUE if the supplied page descriptor is correctly aligned for the
		* given order.  Returns FALSE otherwise.
		* @param pgd The page descriptor to test alignment for.
		* @param order The order to use for calculations.
		*/
		static inline bool is_correct_alignment_for_order(const PageDescriptor *pgd, int order)
		{
			// Calculate the page-frame-number for the page descriptor, and return TRUE if
			// it divides evenly into the number pages in a block of the given order.
			return (sys.mm().pgalloc().pgd_to_pfn(pgd) % pages_per_block(order)) == 0;
		}

		/** Given a page descriptor, and an order, returns the buddy PGD.  The buddy could either be
		* to the left or the right of PGD, in the given order.
		* @param pgd The page descriptor to find the buddy for.
		* @param order The order in which the page descriptor lives.
		* @return Returns the buddy of the given page descriptor, in the given order.
		*/
		PageDescriptor *buddy_of(PageDescriptor *pgd, int order)
		{
			// (1) Make sure 'order' is within range
			if (order >= MAX_ORDER) {
				return NULL;
			}

			// (2) Check to make sure that PGD is correctly aligned in the order
			if (!is_correct_alignment_for_order(pgd, order)) {
				return NULL;
			}

			// (3) Calculate the page-frame-number of the buddy of this page.
			// * If the PFN is aligned to the next order, then the buddy is the next block in THIS order.
			// * If it's not aligned, then the buddy must be the previous block in THIS order.
			uint64_t buddy_pfn = is_correct_alignment_for_order(pgd, order + 1) ?
			sys.mm().pgalloc().pgd_to_pfn(pgd) + pages_per_block(order) :
			sys.mm().pgalloc().pgd_to_pfn(pgd) - pages_per_block(order);

			// (4) Return the page descriptor associated with the buddy page-frame-number.
			return sys.mm().pgalloc().pfn_to_pgd(buddy_pfn);
		}

		/**
		* Inserts a block into the free list of the given order.  The block is inserted in ascending order.
		* @param pgd The page descriptor of the block to insert.
		* @param order The order in which to insert the block.
		* @return Returns the slot (i.e. a pointer to the pointer that points to the block) that the block
		* was inserted into.
		*/
		PageDescriptor **insert_block(PageDescriptor *pgd, int order)
		{
			// Starting from the _free_area array, find the slot in which the page descriptor
			// should be inserted.
			PageDescriptor **slot = &_free_areas[order];

			// Iterate whilst there is a slot, and whilst the page descriptor pointer is numerically
			// greater than what the slot is pointing to.
			while (*slot && pgd > *slot) {
				slot = &(*slot)->next_free;
			}

			// Insert the page descriptor into the linked list.
			pgd->next_free = *slot;
			*slot = pgd;

			// Return the insert point (i.e. slot)
			return slot;
		}

		/**
		* Removes a block from the free list of the given order.  The block MUST be present in the free-list, otherwise
		* the system will panic.
		* @param pgd The page descriptor of the block to remove.
		* @param order The order in which to remove the block from.
		*/
		void remove_block(PageDescriptor *pgd, int order)
		{
			// Starting from the _free_area array, iterate until the block has been located in the linked-list.
			PageDescriptor **slot = &_free_areas[order];
			while (*slot && pgd != *slot) {
				slot = &(*slot)->next_free;
			}

			// Make sure the block actually exists.  Panic the system if it does not.
			assert(*slot == pgd);

			// Remove the block from the free list.
			*slot = pgd->next_free;
			pgd->next_free = NULL;
		}

		/**
		* Given a pointer to a block of free memory in the order "source_order", this function will
		* split the block in half, and insert it into the order below.
		* @param block_pointer A pointer to a pointer containing the beginning of a block of free memory.
		* @param source_order The order in which the block of free memory exists.  Naturally,
		* the split will insert the two new blocks into the order below.
		* @return Returns the left-hand-side of the new block.
		*/
		PageDescriptor *split_block(PageDescriptor **block_pointer, int source_order)
		{
			// Make sure there is an incoming pointer.
			assert(*block_pointer);

			// Make sure the block_pointer is correctly aligned.
			assert(is_correct_alignment_for_order(*block_pointer, source_order));

			if (source_order == 0){          //don't split 0
				syslog.messagef(LogLevel::DEBUG, " order=0, cannot split");
				return *block_pointer;
			}
			else if(source_order>0){          //source_order>0, must split_block
				//add the block to a smaller list and delete it from the original one;
				remove_block(*block_pointer, source_order);
				insert_block(*block_pointer, source_order-1);
				insert_block(*block_pointer+pages_per_block(source_order-1), source_order-1);
				return *block_pointer;
			}
			else
				return NULL;
			return NULL;
		}

		/**
		* Takes a block in the given source order, and merges it (and it's buddy) into the next order.
		* This function assumes both the source block and the buddy block are in the free list for the
		* source order.  If they aren't this function will panic the system.
		* @param block_pointer A pointer to a pointer containing a block in the pair to merge.
		* @param source_order The order in which the pair of blocks live.
		* @return Returns the new slot that points to the merged block.
		*/
		PageDescriptor **merge_block(PageDescriptor **block_pointer, int source_order)
		{
			assert(*block_pointer);

			// Make sure the area_pointer is correctly aligned.
			assert(is_correct_alignment_for_order(*block_pointer, source_order));

			PageDescriptor* start;
			if(source_order>MAX_ORDER-2)   //_free_areas from _free_areas[0] to _free_areas[16] so max order can be merged is 15
				return NULL;
			else{
				//find the left buddy
				if(*block_pointer<buddy_of(*block_pointer, source_order)){
					start = *block_pointer;
				}
				else{
					start = buddy_of(*block_pointer, source_order);
				}
				remove_block(start, source_order);
				remove_block(buddy_of(start, source_order), source_order);
			}

			syslog.messagef(LogLevel::DEBUG, "merge_block");
			return insert_block(start,source_order+1);
		}

	public:
		/**
		* Constructs a new instance of the Buddy Page Allocator.
		*/
		BuddyPageAllocator() {
			// Iterate over each free area, and clear it.
			for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
				_free_areas[i] = NULL;
			}
		}

		/**
		* Allocates 2^order number of contiguous pages
		* @param order The power of two, of the number of contiguous pages to allocate.
		* @return Returns a pointer to the first page descriptor for the newly allocated page range, or NULL if
		* allocation failed.
		*/
		PageDescriptor *alloc_pages(int order) override
		{
			//get the min order that has a free space
			int counter;
			for(counter=order;counter<MAX_ORDER;counter++){
				if(_free_areas[counter]){
					syslog.messagef(LogLevel::DEBUG, "get the order needed");
					break;
				}
			}
			if(counter==MAX_ORDER){
				syslog.messagef(LogLevel::DEBUG, "reach max order in alloc!");
				return NULL;
			}

			//split the first one until get the min page
			PageDescriptor* toSplit = _free_areas[counter];
			PageDescriptor* head = toSplit;
			for(;counter>order;counter--){
				head = split_block(&toSplit, counter);
				toSplit = head;
			}

			syslog.messagef(LogLevel::DEBUG, "Alloc");
			//remove the original largest one
			remove_block(head, counter);
			return head;
		}

		/**
		* Frees 2^order contiguous pages.
		* @param pgd A pointer to an array of page descriptors to be freed.
		* @param order The power of two number of contiguous pages to free.
		*/
		void free_pages(PageDescriptor *pgd, int order) override
		{
			// Make sure that the incoming page descriptor is correctly aligned
			// for the order on which it is being freed, for example, it is
			// illegal to free page 1 in order-1.
			assert(is_correct_alignment_for_order(pgd, order));

			PageDescriptor **freed = insert_block(pgd, order);
			for (;order < MAX_ORDER - 1;order++) {
				bool allFree = false;
				//test the buddy if free or not
				PageDescriptor* buddy = buddy_of(*freed,order);
				if((*freed)->next_free==buddy)
				allFree = true;
				else if(buddy->next_free ==*freed)
				allFree = true;

				if(allFree){
					freed = merge_block(freed, order);
					mm_log.messagef(LogLevel::DEBUG, "free_pages:merge");
			}
				else{
					break;
				}
			}
		}

		/**
		* Reserves a specific page, so that it cannot be allocated.
		* @param pgd The page descriptor of the page to reserve.
		* @return Returns TRUE if the reservation was successful, FALSE otherwise.
		*/
		bool reserve_page(PageDescriptor *pgd){
			//set the page found flag
			bool flag = false;

			if(pgd == NULL) 	//if pgd is a valid page or not
				return flag;
			else{
				//find the location among all the areas, try every !empty order
				for (int order = 0;order < MAX_ORDER;order++ ) {
					//if the list is not empty
					if ( _free_areas[order] ) {
						//search all the page in the area
						for (PageDescriptor* currentPage = _free_areas[order];currentPage!=NULL;currentPage = currentPage->next_free ) {
							//if the pgd is here
							if (pgd >= currentPage && pgd <= currentPage+pages_per_block(order)) {
								PageDescriptor* blockStart = currentPage;
								//split and find the page
								while (order) {
									blockStart = split_block(&blockStart, order--);
									//if the page is in the right side of the buddys, reset head
									if ( pgd >= blockStart + pages_per_block(order)) {
										blockStart = buddy_of(blockStart, order);
									}
								}
								//find the page, set flag and break
								flag = true;
								break;
							}
						}
					}
					//find the page and break
					if(flag){
						//make this page reserve by removing it
						remove_block(pgd, 0);
						break;
					}
				}
			}
			return flag;
		}

		/**
		* Initialises the allocation algorithm.
		* @return Returns TRUE if the algorithm was successfully initialised, FALSE otherwise.
		*/
		bool init(PageDescriptor *page_descriptors, uint64_t nr_page_descriptors) override
		{
			mm_log.messagef(LogLevel::DEBUG, "Buddy Allocator Initialising pd=%p, nr=0x%lx", page_descriptors, nr_page_descriptors);

			// Initialise the free area linked list for the maximum order
			// to initialise the allocation algorithm.

			//see if any page can be allocaed, else cannot init, return false
			if(nr_page_descriptors>0){
				PageDescriptor *page;
				uint64_t pageAvailable = nr_page_descriptors;
				uint64_t currentPage = 0;
				int currentUnit;
				for(int order = MAX_ORDER - 1;order >= 0 && pageAvailable > 0&& currentPage < nr_page_descriptors;order--){
					//get the page_descriptors for list
					_free_areas[order] = &page_descriptors[currentPage];
					page = _free_areas[order];
					//attach all pge to the list
					for(currentUnit = 0;currentUnit < (uint64_t)(pageAvailable / pages_per_block(order));currentUnit++){
						currentPage += pages_per_block(order);
						page->next_free = &page_descriptors[currentPage];
						page = page->next_free;
					}
					//page left which is smaller than an unit will be used for order-1 list
					pageAvailable %= pages_per_block(order);
					mm_log.messagef(LogLevel::DEBUG, " Initialising, %d page left for order %d", pageAvailable, order);
				}
				return true;
			}
			return false;
		}

		/**
		* Returns the friendly name of the allocation algorithm, for debugging and selection purposes.
		*/
		const char* name() const override { return "buddy"; }

		/**
		* Dumps out the current state of the buddy system
		*/
		void dump_state() const override
		{
			// Print out a header, so we can find the output in the logs.
			mm_log.messagef(LogLevel::DEBUG, "BUDDY STATE:");

			// Iterate over each free area.
			for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
				char buffer[256];
				snprintf(buffer, sizeof(buffer), "[%d] ", i);

				// Iterate over each block in the free area.
				PageDescriptor *pg = _free_areas[i];
				while (pg) {
					// Append the PFN of the free block to the output buffer.
					snprintf(buffer, sizeof(buffer), "%s%lx ", buffer, sys.mm().pgalloc().pgd_to_pfn(pg));
					pg = pg->next_free;
				}

				mm_log.messagef(LogLevel::DEBUG, "%s", buffer);
			}
		}


	private:
		PageDescriptor *_free_areas[MAX_ORDER];
	};

	/* --- DO NOT CHANGE ANYTHING BELOW THIS LINE --- */

	/*
	* Allocation algorithm registration framework
	*/
	RegisterPageAllocator(BuddyPageAllocator);
