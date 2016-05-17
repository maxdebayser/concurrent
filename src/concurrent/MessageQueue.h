#ifndef MESSAGEQUEUE_H
#define MESSAGEQUEUE_H

#include <mutex>
#include <condition_variable>
#include <exception/Exception.h>
#include <atomic>
#include <chrono>
#include <algorithm>

class InterruptedException : public ExceptionLib::Exception
{
public:

	//! Construtor com mensagem default
	InterruptedException()
		: ExceptionLib::ExceptionBase(this, false, "interrupted") {}

	//! Construtor de cópia
	InterruptedException(
			/*! [in] Objeto a ser copiado */
			const InterruptedException& that
			)
		: ExceptionLib::ExceptionBase(that)
		, ExceptionLib::Exception(that) {}

	//! Destrutor
	virtual ~InterruptedException() throw() {}

};

/** Fila de mensagens thread-safe */
template<class QueueType>
class MessageQueue
{
public:

	MessageQueue() : m_interrupted(0) {}

	typedef typename QueueType::value_type value_type;

	void push(const value_type& msg) {

        std::unique_lock<std::mutex> lock(m_mutex);

		m_queue.push_back(msg);

        m_cond.notify_one();
	}

	void push_front(const value_type& msg) {

        std::unique_lock<std::mutex> lock(m_mutex);

		m_queue.push_front(msg);

        m_cond.notify_one();
	}

	bool waitForMessage(int waitMS = -1) {
        std::unique_lock<std::mutex> lock(m_mutex);

		if (waitMS != 0) {
			if (m_queue.empty()) {
				if (waitMS < 0) {
                    m_cond.wait(lock);
				} else {
                    m_cond.wait_for(lock, std::chrono::milliseconds(waitMS));
				}
			}
		}
		return !m_queue.empty();
	}

	value_type pop() {
        std::unique_lock<std::mutex> lock(m_mutex);


		int prev = m_interrupted;

		while (m_queue.empty()) {
            m_cond.wait(lock);
			if (prev != m_interrupted) {
				throw InterruptedException();
			}
		}

		value_type v = m_queue.front();
		m_queue.pop_front();
		return v;
	}

	void interrupt() {
        std::unique_lock<std::mutex> lock(m_mutex);


		m_interrupted = true;

		m_queue.clear();

        m_cond.notify_all();
	}

	int size() {
        std::unique_lock<std::mutex> lock(m_mutex);
		return m_queue.size();
	}

private:

	QueueType m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::atomic<int> m_interrupted;
};

#endif // MESSAGEQUEUE_H
