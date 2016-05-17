#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <vector>
#include <future>
#include "MessageQueue.h"
#include <memory>
#include <deque>
#include <exception/Exception.h>
#include <tuple>


class Task {
public:
    virtual ~Task() {}
    virtual void run() = 0;
    virtual void cancel(ExceptionLib::ExceptionBase*) = 0;
};
typedef std::shared_ptr<Task> Work;

template<class R>
struct LambdaTask: public Task {
    std::promise<R> promise;

    std::function<R()> m_f;

    LambdaTask(std::function<R()> f) : m_f(f) {}

    virtual void run() {
        promise.set_value(m_f());
    }

    virtual void cancel(std::exception_ptr ex) {
        promise.set_exception(ex);
    }
};

template<>
struct LambdaTask<void>: public Task {
    std::promise<void> promise;

    std::function<void()> m_f;

    LambdaTask(std::function<void()> f) : m_f(f) {}

    virtual void run() {
        m_f();
        promise.set_value();
    }

    virtual void cancel(std::exception_ptr ex) {
        promise.set_exception(ex);
    }
};


class ThreadPool
{
public:



	ThreadPool(int size);

	~ThreadPool();

	void pushWork(Work w);

	void finish();



    template<class R>
    std::future<R> run(std::function<R()> func) {
        std::shared_ptr<LambdaTask<R>> w(new LambdaTask<R>(func));
        std::future<R> f = w->promise.get_future();
        m_work.push(w);
        return f;
    }

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
