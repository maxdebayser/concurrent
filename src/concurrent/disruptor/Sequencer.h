#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <exception/svector.h>
#include <exception/Exception.h>
#include "Sequence.h"
#include "Sequencer.h"
#include "WaitStrategy.h"
#include "Sleep.h"
#include "HazardPointers.h"

namespace disruptor {

	class InsufficientCapacityException: public ExceptionLib::Exception {
	public:
		InsufficientCapacityException() : ExceptionBase(this, false, "InsufficientCapacityException", NULL) {}

		InsufficientCapacityException(QString errorMsg, const ExceptionBase* nested = NULL, bool trace = false )
			: ExceptionBase(this, trace, errorMsg.toStdString(), nested) {}

		InsufficientCapacityException(const InsufficientCapacityException& that) : ExceptionBase(that), Exception(that) {}
		virtual ~InsufficientCapacityException() throw () {}
	};

	class SingleProducerSequencer {
	public:
		SingleProducerSequencer(size_t bufferSizeLog)
			: m_bufferSize((1 << bufferSizeLog))
			, m_minGatingSequence(Sequence::INITIAL_CURSOR_VALUE)
			, m_nextValue(Sequence::INITIAL_CURSOR_VALUE)
			, m_hazardPtr(NULL)
		{}

		~SingleProducerSequencer() {
			m_hazardPtr->releaseRecord();
			m_hazardPtr = NULL;
		}

		HPRecord* getHazardPointer() {
			if (m_hazardPtr == NULL) {
				m_hazardPtr = HPRecord::acquire();
			}
			return m_hazardPtr;
		}

		void releaseHazardPointer(HPRecord* rec) {
			rec->releasePtr();
		}

		int bufferSize() const
		{
			return m_bufferSize;
		}

		bool hasAvailableCapacity(
				SequenceArray*  gatingSequences, int requiredCapacity) const
		{
			const seq_t targetSequence = m_nextValue + requiredCapacity;

			if (targetSequence == m_bufferSize) {
				if (m_minGatingSequence == Sequence::INITIAL_CURSOR_VALUE) {
					seq_t minSequence = minimumSequence(gatingSequences, m_nextValue);
					m_minGatingSequence = minSequence;
					if (minSequence == Sequence::INITIAL_CURSOR_VALUE)	{
						return false;
					}
				}
			}
			if (targetSequence > m_bufferSize) {
				const seq_t wrapPoint = targetSequence - m_bufferSize;
				if (wrapPoint > m_minGatingSequence) {
					const seq_t minSequence = minimumSequence(gatingSequences, m_nextValue);
					m_minGatingSequence = minSequence;

					if (wrapPoint > minSequence) {
						return false;
					}
				}
			}
			return true;
		}


		seq_t next(SequenceArray*  gatingSequences)
		{
			seq_t nextSequence = m_nextValue +1;

			if (nextSequence > m_bufferSize) {
				const seq_t wrapPoint = nextSequence - m_bufferSize;
				// Wrap point é onde o contador do producer vai bater no de um consumer
				if (wrapPoint > m_minGatingSequence) {
					seq_t minSequence;
					while(wrapPoint > (minSequence = minimumSequence(gatingSequences, m_nextValue))) {
						// TODO: Use waitStrategy to spin?
						SleepUtil::usleep(1);
					}
					m_minGatingSequence = minSequence;
				}
			} else
			if (nextSequence == m_bufferSize) {
				if (m_minGatingSequence == Sequence::INITIAL_CURSOR_VALUE) {
					seq_t minSequence;
					while((minSequence = minimumSequence(gatingSequences, m_nextValue)) == Sequence::INITIAL_CURSOR_VALUE) {
						// TODO: Use waitStrategy to spin?
						SleepUtil::usleep(1);
					}
					m_minGatingSequence = minSequence;
				}
			}

			m_nextValue = nextSequence;
			return nextSequence;
		}


		seq_t tryNext(SequenceArray* gatingSequences)
		{
			if (!hasAvailableCapacity(gatingSequences, 1)) {
				throw InsufficientCapacityException();
			}
			return ++m_nextValue;
		}


		size_t remainingCapacity(SequenceArray*  gatingSequences) const
		{
			seq_t consumed = minimumSequence(gatingSequences, m_nextValue);
			seq_t produced = (m_nextValue == Sequence::INITIAL_CURSOR_VALUE) ? 0 : m_nextValue;
			return m_bufferSize - (produced - consumed);
		}


		void claim(seq_t sequence)
		{
			m_nextValue = sequence;
		}


		seq_t wrapPoint()
		{
			return m_minGatingSequence;
		}

	private:
		const size_t m_bufferSize;
		mutable seq_t m_minGatingSequence;
		seq_t m_nextValue;
		HPRecord* m_hazardPtr;
	};


	class MultiProducerSequencer {
	public:

		MultiProducerSequencer(size_t bufferSizeLog)
			: m_bufferSize((1 << bufferSizeLog))
			, m_cursor(Sequence::INITIAL_CURSOR_VALUE)
			, m_wrapPointCache(Sequence::INITIAL_CURSOR_VALUE)
		{}

		~MultiProducerSequencer() {	}

		HPRecord* getHazardPointer() {
			return HPRecord::acquire();
		}

		void releaseHazardPointer(HPRecord* rec) {
			rec->releaseRecord();
		}

		int bufferSize() const {
			return m_bufferSize;
		}

		bool hasAvailableCapacity(SequenceArray* gatingSequences, int requiredCapacity) const
		{
			const seq_t desiredSequence = m_cursor + requiredCapacity;
			if (desiredSequence > m_wrapPointCache) {
				seq_t wrapPoint = minimumSequence(gatingSequences, m_cursor) + bufferSize();
				m_wrapPointCache = wrapPoint;
				if (desiredSequence > wrapPoint) {
					return false;
				}
			}
			return true;
		}

		seq_t next(SequenceArray* gatingSequences)
		{
			seq_t current;
			seq_t next;

			do {

				current = m_cursor;
				next = current + 1;

				if (next == m_bufferSize && m_wrapPointCache == Sequence::INITIAL_CURSOR_VALUE) {

					seq_t minSeq = minimumSequence(gatingSequences, m_cursor);

					if (minSeq == Sequence::INITIAL_CURSOR_VALUE) {
						SleepUtil::usleep(1);
						continue;
					}

					seq_t wrapPoint = minSeq + m_bufferSize;
					m_wrapPointCache = wrapPoint;
				}

				if (next > m_wrapPointCache) {

					seq_t wrapPoint = minimumSequence(gatingSequences, m_cursor) + m_bufferSize;
					m_wrapPointCache = wrapPoint;

					if (next > wrapPoint) {
						SleepUtil::usleep(1);
						continue;
					}
				} else if (m_cursor.compareAndSet(current, next)) {
					break;
				}
			} while (true);
			return next;
		}

		seq_t tryNext(SequenceArray* gatingSequences)
		{
			seq_t current;
			seq_t next;

			do {

				current = m_cursor;
				next = current + 1;

				if (next == m_bufferSize && m_wrapPointCache == Sequence::INITIAL_CURSOR_VALUE) {
					seq_t wrapPoint = minimumSequence(gatingSequences, m_cursor) + m_bufferSize;
					m_wrapPointCache = wrapPoint;

					if (wrapPoint == Sequence::INITIAL_CURSOR_VALUE) {
						throw InsufficientCapacityException();
					}
				}

				if (next > m_wrapPointCache) {

					seq_t wrapPoint = minimumSequence(gatingSequences, m_cursor) + m_bufferSize;
					m_wrapPointCache = wrapPoint;

					if (next > wrapPoint) {
						throw InsufficientCapacityException();
					}
				}
			} while (!m_cursor.compareAndSet(current, next));
			return next;
		}

		size_t remainingCapacity(SequenceArray* gatingSequences) const
		{
			const seq_t consumed = minimumSequence(gatingSequences, m_cursor);
			const seq_t produced = m_cursor;
			return m_bufferSize - (produced - consumed);
		}

		void claim(seq_t sequence)
		{
			m_cursor = sequence;
		}

		seq_t wrapPoint()
		{
			return m_wrapPointCache;
		}

		Sequence* cursor()
		{
			return &m_cursor;
		}

	private:
		const size_t m_bufferSize;
		Sequence m_cursor;
		mutable Sequence m_wrapPointCache;
	};
}

#endif // SEQUENCER_H
