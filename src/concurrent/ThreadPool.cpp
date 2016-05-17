#include "ThreadPool.h"

using namespace ExceptionLib;

ThreadPool::ThreadPool(int size) : m_threads(size) {
	for (int i = 0; i < size; ++i) {
		m_threads[i].startUp(this);
	}
}


ThreadPool::~ThreadPool() {
	finish();
}


void ThreadPool::pushWork(Work w) {
	m_work.push(w);
}


void ThreadPool::finish() {
	m_work.interrupt();
	for (size_t i = 0; i < m_threads.size(); ++i) {
		m_threads[i].finish();
	}
}

ThreadPool::PoolThread::PoolThread() : pool(NULL), done(false) {}

ThreadPool::PoolThread::PoolThread(const PoolThread&) : pool(NULL), done(false) {}

void ThreadPool::PoolThread::run() {

	while(!done) {

		try {
			Work w = pool->m_work.pop();
			try {
				w->run();
			} catch(const Exception& ex) {
				w->cancel(ex.clone());
			} catch(const std::exception& ex) {
				w->cancel(new Exception(ex.what()));
			} catch(...) {
				w->cancel(new Exception("unhandled exception of unknown type"));
				throw;
			}
		} catch (const InterruptedException&) {
			return;
		}
	}
}


void ThreadPool::PoolThread::startUp(ThreadPool* p) {
	pool = p;
	this->start();
}


void ThreadPool::PoolThread::finish() {
	done = true;
	wait();
}

void ThreadPool::PoolThread::start()
{
    m_thread = std::thread([&](){ run(); });
}

void ThreadPool::PoolThread::wait()
{
    m_thread.join();
}
