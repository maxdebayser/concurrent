#ifndef DISRUPTORTEST_H
#define DISRUPTORTEST_H

#include <cxxtest/TestSuite.h>


class DisruptorTest : public CxxTest::TestSuite {
public:
	bool isManual() const { return true; }

	~DisruptorTest();

	void test1P1C();

	void test1P1CBlockingQueue();
};


#endif // DISRUPTORTEST_H
