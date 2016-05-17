#include "DisruptorTest.h"

#include <thread>
#include <chrono>
#include "MessageQueue.h"
#include <deque>
#include <list>

#include "disruptor/RingBuffer.h"

using namespace disruptor;

#include <iostream>
using namespace std;

DisruptorTest::~DisruptorTest()
{
	HPRecord::retireThread();
}

struct SimpleWork {
	//char pad1[64];
	int id;
	//char pad2[64];

	SimpleWork& operator=(const SimpleWork& that) {
		id = that.id;
		return *this;
	}

	SimpleWork() : id(0) {}
	SimpleWork(int i) : id(i) {}

};


void DisruptorTest::test1P1C()
{
	static const int BUF = 11;
	std::cout << "hello " << (sizeof(Sequence)) << std::endl;

	//typedef RingBuffer<SimpleWork, BUF, SpinWaitStrategy, MultiProducerSequencer, MultiProducerPublisher> RingBuffer_t;
	typedef RingBuffer<SimpleWork, BUF, SpinWaitStrategy, SingleProducerSequencer, SingleProducerPublisher> RingBuffer_t;


	RingBuffer_t ring;

    shared_ptr<Sequence> gatingSequence(new Sequence);

	//Sequence gatingSequence;
	ring.addGatingSequence(gatingSequence);

	int paddedInt[100];
	int & lastId = paddedInt[50];
    lastId = 100*1000*1000;

    auto start = std::chrono::high_resolution_clock::now();


    thread producer([&](int count){
        std::cout << "run" << std::endl;

        while(count--) {

            ring.publishAndAssign(count);
            //std::cout << count << std::endl;
        }
        HPRecord::retireThread();
    }, lastId);

	int diff = 0;

	while (lastId > 0) {
		++diff;
		seq_t seq = gatingSequence->value() + diff;
		SimpleWork work = ring.get(seq);
        TS_ASSERT_EQUALS(work.id, lastId-1);
		lastId = work.id;
		if (diff == 1024) {
			gatingSequence->add(diff);
			diff =0 ;
			//gatingSequence.inc();
		}
	}
    producer.join();
	HPRecord::retireThread();

    TS_ASSERT_EQUALS(lastId, 0);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end-start;

    std::cout << (elapsed.count()) << " s" << std::endl;
}


void DisruptorTest::test1P1CBlockingQueue()
{
	std::cout << "hello 2"  << std::endl;

	// deque é muito mais rápido do que QList ou std::list.
	// É só substituir abaixo para comprovar

	MessageQueue<std::deque<SimpleWork> > queue;


	int paddedInt[100];
	int & lastId = paddedInt[50];
    lastId = 100*1000 * 1000;

    auto start = std::chrono::high_resolution_clock::now();

    thread producer([&](int count){
        std::cout << "run" << std::endl;

        while(count--) {

            SimpleWork work(count);
            queue.push(work);
            //std::cout << m_count << std::endl;
        }
    }, lastId);

	while (lastId > 0) {

		SimpleWork work =  queue.pop();
        TS_ASSERT_EQUALS(work.id, lastId-1);
		lastId = work.id;
	}
    producer.join();

    TS_ASSERT_EQUALS(lastId, 0);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end-start;

    std::cout << (elapsed.count()) << " s" << std::endl;
}
