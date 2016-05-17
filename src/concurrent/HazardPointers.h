#ifndef HAZARDPOINTERS_H
#define HAZARDPOINTERS_H

#include <atomic>
#include <vector>
#include <deque>
#include "ThreadStorage.h"

/* Implementation of hazard pointers
 *
 * Michael, Maged M. "Hazard Pointers: Safe Memory Reclamation for Lock-Free Objects,"
 * IEEE Transactions on Parallel and Distributed Systems, pages 491-504. IEEE, 2004.
 *
 */

template<class T>
void standardDeleter(void * t) {
	delete reinterpret_cast<T*>(t);
}

typedef void (*deleter_t)(void*);

class HPRecord {
public:

	static HPRecord* acquire();

	template<class T>
    static HPRecord* acquire(std::atomic<T*>& stmPtr) {
		HPRecord* p = acquire();
		p->securePtr(stmPtr);
		return p;
	}

	// Apenas desmarca o hazardPointer
	void releasePtr();

	// Volta para o pool
	void releaseRecord();

	static void retireThread();

	template<class T>
    T* securePtr(std::atomic<T*>& stmPtr) {
		T* ptr;
		do {
			ptr = stmPtr;
			this->hazardPtr = ptr;
		} while (stmPtr != ptr);
		return ptr;
	}

	template<class T>
	static void retirePtr(T* ptr, deleter_t del = standardDeleter<T>) {

		if (ptr == NULL) {
			return;
		}

		RetiredPtrRecord ret = { ptr, del };

		rList().push_back(ret);

		if (rList().size() > RetiredLimit) {
			Scan(head());
		}
	}

    std::atomic<void*> hazardPtr;

private:
	HPRecord() {}
	HPRecord(const HPRecord&);
	HPRecord& operator=(const HPRecord&);

	struct RAII {
        std::atomic<HPRecord*> h;
		RAII() : h(NULL) {}
		~RAII() {
			HPRecord::cleanup();
		}
	};

	struct RetiredPtrRecord {
		void * ptr;
		deleter_t deleter;
	};

	typedef std::deque<RetiredPtrRecord> RetiredList;

    static std::atomic<HPRecord*>& head();

	static const size_t RetiredLimit;

    static std::atomic<int> s_listSize;

    static ThreadStorage<RetiredList> s_rList;

	static RetiredList& rList();

	static void Scan(HPRecord* rec);

	static void cleanup();

    static void cleanRecursive(std::atomic<HPRecord*>& rec);

    std::atomic<HPRecord*> m_next;

    std::atomic<int> m_active;

	RetiredList m_leftover;
};

#endif // HAZARDPOINTERS_H
