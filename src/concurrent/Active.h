#ifndef ACTIVE_H
#define ACTIVE_H

#include <exception/Exception.h>

#include <deque>
#include <thread>
#include <memory>
#include <condition_variable>

#include "disruptor/RingBuffer.h"
#include "Semaphore.h"
#include <chrono>
#include <functional>

class WaitStrategy {
public:
	virtual ~WaitStrategy() {}

	virtual void wait() = 0;
	virtual void newData() = 0;
};

class MessageWaitStrategy:public WaitStrategy {
public:
	virtual ~MessageWaitStrategy() {}

	MessageWaitStrategy() : m_timeout (-1) {}
	MessageWaitStrategy(unsigned long timeoutMS) : m_timeout(timeoutMS) {}

	void wait() {
        m_semaphore.try_wait_for(1, std::chrono::milliseconds(m_timeout));
	}

	void newData() {
        m_semaphore.notify();
	}

private:
	int m_timeout;

    semaphore m_semaphore;
};


/** Implementa o padrão Active Object
 *
 *  Um active object é um objeto como outro qualquer só que
 *  ele possui uma thread interna que processa seus métodos.
 *  Como seus métodos são executados, os parâmetros são
 *  empacotados em mensagens que são colocadas numa fila.
 *  O active object possui um loop em que as mensagens são
 *  processadas uma a uma pela thread interna.
 *
 *  Essa implementação de ActiveObject pode ser configurada
 *  para usar uma fila padrão ou usar uma fila de prioridade.
 *  Usando a fila de prioridade as mensagens devem implementar
 *  um método de comparação que diz se sua prioridade é maior
 *  ou não que a de outra mensagem.
 */
template<class RBWaitStrategy = disruptor::SpinWaitStrategy, int QUEUE_LOG = 5>
class Active {
public:

	struct Message {
		virtual ~Message() {}
		virtual bool execute() = 0;
        std::weak_ptr<Message> weakThis;
	};

    typedef std::shared_ptr<Message> MsgPtr;

	typedef disruptor::RingBuffer<MsgPtr, QUEUE_LOG, RBWaitStrategy, disruptor::MultiProducerSequencer, disruptor::MultiProducerPublisher> RingBuffer_t;

	struct FinishMessage: public Message {
		FinishMessage(Active * a) : m_active(a) {}
		virtual bool execute() { m_active->m_done = true; return true; }
		Active* m_active;
	};

	Active(bool startNow = true)
		: m_queue()
		, m_gatingSequence(new disruptor::Sequence)
		, m_done(false)
		, m_waitStrategy(new MessageWaitStrategy)
	{
		m_queue.addGatingSequence(m_gatingSequence);
		if (startNow) {
			this->start();
		}
	}

    Active(std::shared_ptr<WaitStrategy> waitStrategy, bool startNow = true)
		: m_queue()
		, m_gatingSequence(new disruptor::Sequence)
		, m_done(false)
		, m_waitStrategy(waitStrategy)
	{
		m_queue.addGatingSequence(m_gatingSequence);
		if (startNow) {
			this->start();
		}
	}

    void start() {
        m_thread = std::thread(run);
    }

	~Active() {
		finish();
        m_thread.join(); // espera para sempre
	}

	void finish() {
		send(MsgPtr(new FinishMessage(this)));
	}

	void send(MsgPtr msg) {
        msg->weakThis = msg;
		m_queue.publishAndAssign(msg);
		m_waitStrategy->newData();
	}

	MsgPtr& getPreallocated(disruptor::seq_t& seq) {
		seq = m_queue.next();
		return m_queue.preallocated(seq);
	}

	void sendPreallocated(const MsgPtr& msg, disruptor::seq_t seq) {
        msg->weakThis = msg;
		m_queue.publish(seq);
		m_waitStrategy->newData();
	}

	MsgPtr& consumeMessage() {
		disruptor::seq_t seq = m_gatingSequence->value() + 1;
		return m_queue.get(seq); // isso bloqueia
	}

	MsgPtr& popMessage() {
		MsgPtr& ptr = consumeMessage();
		m_gatingSequence->inc();
		return ptr;
	}

	bool waitForMessage(int waitMS = -1) const {
		disruptor::seq_t seq = m_gatingSequence->value() + 1;

		while (waitMS-- > 0) {
			if (m_queue.available(seq)) {
				return true;
			}
			SleepUtil::msleep(1);
		}
		return m_queue.available(seq);
	}

	bool hasAvailableCapacity(int required) {
		return m_queue.hasAvailableCapacity(required);
	}

	bool done() const {
		return m_done;
	}

protected:

	RingBuffer_t m_queue;
    std::shared_ptr<disruptor::Sequence> m_gatingSequence;

private:
	volatile bool m_done;
    std::shared_ptr<WaitStrategy> m_waitStrategy;
    std::thread m_thread;

    std::function<void()> run = [&](){
        while (! m_done ) {

            while (waitForMessage(0)) {
                MsgPtr msg = popMessage();
                if (msg->execute()) {
                    break;
                }
            }

            m_waitStrategy->wait();
        }
    };
};



#endif // ACTIVE_H
