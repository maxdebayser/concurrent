#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include "Publisher.h"
#include "Sequence.h"
#include "SequenceBarrier.h"
#include "Sequencer.h"
#include "WaitStrategy.h"
#include <exception/Exception.h>
#include "HazardPointers.h"

#include <memory>

namespace disruptor {

	template<class T, size_t POW, class WaitStrategy_t, class Sequencer_t, template<class> class Publisher_Templ>
	class RingBuffer: public WaitStrategy_t, public Sequencer_t, public Publisher_Templ<RingBuffer<T, POW, WaitStrategy_t, Sequencer_t, Publisher_Templ> > {

		typedef RingBuffer<T, POW, WaitStrategy_t, Sequencer_t, Publisher_Templ> MyType;
		typedef Publisher_Templ<RingBuffer<T, POW, WaitStrategy_t, Sequencer_t, Publisher_Templ> > Publisher_t;

	public:

		// consumer
		T& get(seq_t sequence) {
			Publisher_t::ensureAvailable(sequence);
			return m_entries[sequence & indexMask];
		}

		// producer
		seq_t next() {
			HPRecord* rec = Sequencer_t::getHazardPointer();
			seq_t retVal =  Sequencer_t::next(rec->securePtr(m_gatingSequences));
			Sequencer_t::releaseHazardPointer(rec);
			return retVal;
		}

		void initaliseTo(seq_t sequence) {
			if (m_gatingSequences != NULL) {
				throw ExceptionLib::InvalidStateException("Can only initialise the cursor if not gating sequences have been added");
			}
			Sequencer_t::claim(sequence);
			Publisher_t::publish(sequence);
		}

        void addGatingSequence(std::shared_ptr<Sequence> s) {
			addGatingSequences(&s, (&s)+1);
		}

		seq_t minimumGatingSequence() const {
			if (m_gatingSequences == NULL) {
				return Sequence::INITIAL_CURSOR_VALUE;
			}
			return minimumSequence(m_gatingSequences);
		}

		template<class SequenceIterator>
		void addGatingSequences(SequenceIterator begin, SequenceIterator end) {

			SequenceArray* origArray = NULL;
			SequenceArray* newArray = NULL;
			HPRecord* rec = HPRecord::acquire();

			do {
				origArray = m_gatingSequences;
				rec->hazardPtr = origArray;

				if (m_gatingSequences != origArray) {
					continue;
				}

				if (newArray != NULL) {
					SequenceArray::destroy(newArray);
				}
				newArray = SequenceArray::addSequence(origArray, begin, end);

            } while (!m_gatingSequences.compare_exchange_weak(origArray, newArray, std::memory_order_release, std::memory_order_relaxed));

			rec->releaseRecord();
			HPRecord::retirePtr(origArray, SequenceArray::destroy);
		}

        void removeGatingSequence(std::shared_ptr<Sequence> toRemove) {

			SequenceArray* origArray = NULL;
			SequenceArray* newArray = NULL;
			HPRecord* rec = HPRecord::acquire();

			do {
				origArray = m_gatingSequences;
				rec->hazardPtr = origArray;

				if (m_gatingSequences != origArray) {
					continue;
				}

				if (newArray != NULL) {
					SequenceArray::destroy(newArray);
				}
				newArray = SequenceArray::removeSequence(origArray, toRemove);

            } while (!m_gatingSequences.compare_exchange_weak(origArray, newArray, std::memory_order_release, std::memory_order_relaxed));

			rec->releaseRecord();
			HPRecord::retirePtr(origArray, SequenceArray::destroy);
		}

		template<class SequenceIterator>
        std::shared_ptr<SequenceBarrier> newBarrier(SequenceIterator begin, SequenceIterator end) {
            return std::shared_ptr<SequenceBarrier>(new ProcessingSequenceBarrier<MyType>(this/*m_waitStrategy*/, Sequencer_t::cursor(), SequenceArray::build(begin, end)));
		}

		seq_t cursorVal() {
			return *Sequencer_t::cursor();
		}

		bool hasAvailableCapacity(size_t requiredCapacity) {
			HPRecord* rec = Sequencer_t::getHazardPointer();
			const bool retVal = Sequencer_t::hasAvailableCapacity(rec->securePtr(m_gatingSequences), requiredCapacity);
			Sequencer_t::releaseHazardPointer(rec);
			return retVal;
		}

		T& preallocated(seq_t sequence) {
			return m_entries[sequence & indexMask];
		}

		void publish(seq_t sequence) {
			Publisher_t::publish(sequence);
		}

		void publishAndAssign(const T& newVal) {
			const seq_t seq = next();
			preallocated(seq) = newVal;
			Publisher_t::publish(seq);
		}

		static const int bufferSize = (1 << POW);


		~RingBuffer() {
			if (m_gatingSequences != NULL) {

				SequenceArray* ptr = NULL;
				do {
					ptr = m_gatingSequences;
                } while (!m_gatingSequences.compare_exchange_weak(ptr, NULL, std::memory_order_release, std::memory_order_relaxed));

				HPRecord::retirePtr(ptr, SequenceArray::destroy);
			}
		}

		RingBuffer()
			: Sequencer_t(POW)
			, Publisher_t(POW)
			, m_gatingSequences(NULL)
		{
		}

	private:



		static const int indexMask = (1 << POW)-1;

		T m_entries[bufferSize];
        std::atomic<SequenceArray*> m_gatingSequences;
	};

}


#endif // RINGBUFFER_H
