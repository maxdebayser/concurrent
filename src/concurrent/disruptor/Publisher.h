#ifndef PUBLISHER_H
#define PUBLISHER_H

#include <stdint.h>
#include "Sequence.h"
#include "Sequencer.h"
#include "WaitStrategy.h"
#include "Publisher.h"
#include <exception/svector.h>
#include <memory>
#include <atomic>

namespace disruptor {

	template<class SubClass>
	class SingleProducerPublisher
	{
	public:
		SingleProducerPublisher(int)
			: m_cursor(Sequence::INITIAL_CURSOR_VALUE)
		{}

		void publish(seq_t sequence) {
			m_cursor.changeValue(sequence);
		}

		bool available(seq_t sequence) const {
			seq_t cursor = m_cursor;
			return (cursor != Sequence::INITIAL_CURSOR_VALUE) && (sequence <= cursor);
		}

		void ensureAvailable(seq_t sequence) const {

			while (!available(sequence)) {
				SleepUtil::usleep(1);
			}
		}

		Sequence* cursor() {
			return &m_cursor;
		}

	private:
		Sequence m_cursor;
	};

	template<class SubClass>
	class MultiProducerPublisher {
	public:

		MultiProducerPublisher(int bufferSizeLog2)
			: m_availabilityFlags((1 << bufferSizeLog2), -1)
			, m_indexMask((1 << bufferSizeLog2)-1)
			, m_indexShift(bufferSizeLog2)
		{}

		void publish(seq_t sequence)
		{
			const seq_t index = calculateIndex(sequence);
			const int flag = calculateAvailabilityFlag(sequence);
			m_availabilityFlags[index] = flag;
			static_cast<SubClass*>(this)->signalAllWhenBlocking();
		}

		bool available(seq_t sequence) const
		{
			const seq_t index = calculateIndex(sequence);
			const int flag = calculateAvailabilityFlag(sequence);
			return m_availabilityFlags[index] == flag;
		}

		void ensureAvailable(seq_t sequence) const
		{
			const seq_t index = calculateIndex(sequence);
			const int flag = calculateAvailabilityFlag(sequence);

			while (m_availabilityFlags[index] != flag) {
				SleepUtil::usleep(1);
				//spin
	#ifdef DEBUG
				if (m_availabilityFlags[index] > flag) {
					throw ExceptionLib::InvalidStateException("m_availabilityFlags[index] > flag");
				}
	#endif
			}
		}

	private:

		int calculateAvailabilityFlag(seq_t sequence) const {
			return static_cast<int>(sequence >> m_indexShift);
		}

		seq_t calculateIndex(seq_t sequence) const {
			return static_cast<seq_t>(sequence & m_indexMask);
		}

        svector<std::atomic<int>> m_availabilityFlags;
		const int m_indexMask;
		const int m_indexShift;
	};

}

#endif // PUBLISHER_H
