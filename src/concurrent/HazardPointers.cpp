#include "HazardPointers.h"
#include <algorithm>
#include <iostream>

const size_t HPRecord::RetiredLimit = 20;

HPRecord* HPRecord::acquire() {

	HPRecord* p = head();

	for (; p; p = p->m_next) {

        int expected = 0;
        if (p->m_active == 1 || !p->m_active.compare_exchange_weak(expected, 1, std::memory_order_release, std::memory_order_relaxed)) {
			continue;
		}
		return p;
	}

    s_listSize.fetch_add(1, std::memory_order_release);

	HPRecord* rec = new HPRecord;
	rec->m_active = 1;
	rec->hazardPtr = NULL;

	HPRecord* old = head();

	do {
		old = head();
		rec->m_next = old;
    } while (!head().compare_exchange_weak(old, rec, std::memory_order_release, std::memory_order_relaxed));

	// Herda o leftover de outra thread
	while (!rec->m_leftover.empty()) {
		rList().push_back(rec->m_leftover.front());
		rec->m_leftover.pop_front();
	}

	return rec;
}


void HPRecord::releasePtr() {
	hazardPtr = NULL;
}

void HPRecord::releaseRecord() {
	hazardPtr = NULL;
	m_active = 0;
}

void HPRecord::retireThread() {

    if (s_rList.hasLocalData() && !s_rList.localData().empty()) {
		HPRecord* rec = acquire();

		RetiredList::iterator it = rList().begin();
		RetiredList::iterator end = rList().end();

		for (; it != end; ++it) {
			rec->m_leftover.push_back(*it);
		}
        s_rList.emplaceLocalData();
		rec->releaseRecord();
	}
}

std::atomic<HPRecord*>& HPRecord::head() {
	static RAII r;
	return r.h;
}

std::atomic<int> HPRecord::s_listSize;

HPRecord::RetiredList& HPRecord::rList() {

	if (!s_rList.hasLocalData()) {
        s_rList.emplaceLocalData();
	}
    return s_rList.localData();
}

void HPRecord::Scan(HPRecord* rec) {

	std::vector<volatile void*> hp(s_listSize);

	while (rec) {
		volatile void* ptr = rec->hazardPtr;
		if (ptr) {
			hp.push_back(ptr);
		}
		rec = rec->m_next;
	}
	std::sort(hp.begin(), hp.end());

	RetiredList& retList = rList();
	RetiredList::iterator it = retList.begin();

	while (it != retList.end()) {
		if (!std::binary_search(hp.begin(), hp.end(), it->ptr)) {
			it->deleter(it->ptr);
			if (&*it != &retList.back()) {
				*it = retList.back();
			}
			retList.pop_back();
		} else {
			++it;
		}
	}
}

void HPRecord::cleanup() {

    if (s_rList.hasLocalData()) {

        cleanRecursive(head());
        Scan(head());

        if (!s_rList.localData().empty()) {
            std::cout << "Some hazard pointers are still active. The referenced objects will leak" << std::endl;
        }
        s_rList.emplaceLocalData();
    } // otherwise the thread local data was already destroyed on shutdown
}

void HPRecord::cleanRecursive(std::atomic<HPRecord*>& rec) {

	if (rec == NULL) {
		return;
	}

    if (rec.load()->m_active == 0) {

        s_listSize.fetch_sub(1, std::memory_order_release);

		// Remove the node
		HPRecord* n = NULL;
		HPRecord* h = NULL;

		do {
			h = rec;
			n = h->m_next;
        } while (!rec.compare_exchange_weak(h, n, std::memory_order_release, std::memory_order_relaxed));

		// Clean the node, acquiring all leftover nodes

		while (!h->m_leftover.empty()) {
			rList().push_back(h->m_leftover.front());
			h->m_leftover.pop_front();
		}

		delete h;

		// recurse
		cleanRecursive(rec);

	} else {
		// skip the node with a warning
		std::cout << "Found an active HPRecord" << std::endl;

        cleanRecursive(rec.load()->m_next);
	}
}

ThreadStorage<HPRecord::RetiredList> HPRecord::s_rList;
