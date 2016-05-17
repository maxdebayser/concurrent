#ifndef WAITSTRATEGY_H
#define WAITSTRATEGY_H

#include "Sequence.h"
#include <mutex>
#include <chrono>
#include <thread>
#include <condition_variable>

#include "SequenceBarrier.h"
#include "Sleep.h"

namespace disruptor {

	class BlockingWaitStrategy {
	public:

		BlockingWaitStrategy(size_t timeout = 1) : m_timeoutMS(timeout) {}

		seq_t waitFor (
				seq_t sequence,
				Sequence* cursor,
				const MinimumReader& depedentSequence,
				SequenceBarrier& barrier)
		{
			seq_t availableSequence;
			if ((availableSequence = (*cursor)) < sequence) {

                std::unique_lock<std::mutex> lock(m_lock);

				while ((availableSequence = (*cursor)) < sequence) {
					barrier.checkAlert();
                    m_processorNotifyCondition.wait_for(lock, std::chrono::milliseconds(m_timeoutMS));
				}
			}
			while ((availableSequence = depedentSequence.value()) < sequence) {
				barrier.checkAlert();
			}
			return availableSequence;
		}

		void signalAllWhenBlocking() {
            //std::unique_lock<std::mutex> lock(m_lock);
            // The notifying thread does not need to hold the lock on the same mutex as the
            // one held by the waiting thread(s); in fact doing so is a pessimization, since
            // the notified thread would immediately block again, waiting for the notifying thread to release the lock.
            m_processorNotifyCondition.notify_all();
		}

	private:
        std::mutex m_lock;
        std::condition_variable m_processorNotifyCondition;
		size_t m_timeoutMS;
	};

	class SpinWaitStrategy {
	public:

		SpinWaitStrategy() {}

		seq_t waitFor (
				seq_t sequence,
				Sequence* /*cursor*/,
				const MinimumReader& depedentSequence,
				SequenceBarrier& barrier)
		{
			seq_t availableSequence;
			while ((availableSequence = depedentSequence.value()) < sequence) {
				barrier.checkAlert();
			}
			return availableSequence;
		}

		void signalAllWhenBlocking() {}

	private:
	};

	class SleepingWaitStrategy {
	public:

		SleepingWaitStrategy() {}

		seq_t waitFor (
				seq_t sequence,
				Sequence* /*cursor*/,
				const MinimumReader& depedentSequence,
				SequenceBarrier& barrier)
		{
			seq_t availableSequence;
			int counter = 200;

			while ((availableSequence = depedentSequence.value()) < sequence) {
				barrier.checkAlert();

				if (counter > 100) {
					--counter;
				} else if (counter > 0) {
					--counter;
                    std::this_thread::yield();
				} else {

				}
			}
			return availableSequence;
		}

		void signalAllWhenBlocking() {}

	private:
	};

	class YieldingWaitStrategy {
	public:

		YieldingWaitStrategy() {}

		seq_t waitFor (
				seq_t sequence,
				Sequence* /*cursor*/,
				const MinimumReader& depedentSequence,
				SequenceBarrier& barrier)
		{
			seq_t availableSequence;
			int counter = 100;

			while ((availableSequence = depedentSequence.value()) < sequence) {
				barrier.checkAlert();
                if (counter == 0) {
                    std::this_thread::yield();
				} else {
					SleepUtil::usleep(1);
					--counter;
				}
			}
			return availableSequence;
		}

		void signalAllWhenBlocking() {}

	private:
	};

}

#endif // WAITSTRATEGY_H
