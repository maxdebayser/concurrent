#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <vector>
#include <future>
#include "MessageQueue.h"
#include <memory>
#include <deque>
#include <exception/Exception.h>

class ThreadPool: public QObject
{
Q_OBJECT
public:

	class Task {
	public:
		virtual ~Task() {}
		virtual void run() = 0;
		virtual void cancel(ExceptionLib::ExceptionBase*) = 0;
	};
    typedef std::shared_ptr<Task> Work;

	template<class R, class C, class A1>
	struct MethodTask1Arg: public Task {
        std::promise<R> promise;
		typedef R (C::*memptr)(A1);

		C* object;
		memptr ptr;
		A1 a1;

		MethodTask1Arg(C* o, memptr p, A1 _a1) : object(o), ptr(p), a1(_a1) {}

		virtual void run() {
            promise.set_value((object->*ptr)(a1));
		}

        virtual void cancel(std::exception_ptr ex) {
            promise.set_exception(ex);
		}
	};

	template<class C, class A1>
	struct MethodTask1Arg<void, C, A1>: public Task {
        std::promise<void> promise;
		typedef void (C::*memptr)(A1);

		C* object;
		memptr ptr;
		A1 a1;

		MethodTask1Arg(C* o, memptr p, A1 _a1) : object(o), ptr(p), a1(_a1) {}

		virtual void run() {
			(object->*ptr)(a1);
            promise.set_value();
		}

        virtual void cancel(std::exception_ptr ex) {
            promise.set_exception(ex);
        }
	};

	template<class R, class C, class A1>
    std::future<R> run(C* object, R (C::*memptr)(A1), A1 a1) {
        std::shared_ptr<MethodTask1Arg<R,C,A1> > w(new MethodTask1Arg<R,C,A1>(object, memptr, a1));
        std::future<R> f = w->promise.get_future();
		m_work.push(w);
		return f;
	}

	ThreadPool(int size);

	~ThreadPool();

	void pushWork(Work w);

	void finish();

private:	

	typedef MessageQueue<std::deque<Work> > WQueue;

    class PoolThread {
	public:

		PoolThread();

		PoolThread(const PoolThread&);

		virtual void run();

		void startUp(ThreadPool* p);

		void finish();

        void start();

        void wait();

	private:
		ThreadPool* pool;
		volatile bool done;
        std::thread m_thread;
	};

	std::vector<PoolThread> m_threads;
	WQueue m_work;
};

#endif // THREADPOOL_H
