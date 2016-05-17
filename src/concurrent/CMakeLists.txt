set(HEADERS
    Active.h
    HazardPointers.h
    MessageQueue.h
    Semaphore.h
    Sleep.h
    ThreadPool.h
    ThreadStorage.h

    disruptor/Publisher.h
    disruptor/RingBuffer.h
    disruptor/SequenceBarrier.h
    disruptor/Sequence.h
    disruptor/Sequencer.h
    disruptor/WaitStrategy.h
)


set(SOURCES
    HazardPointers.cpp
    Sleep.cpp
    ThreadPool.cpp
    ThreadStorage.cpp
)


SET(LIBS
)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fPIC")
SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fPIC")

add_library(concurrent STATIC ${HEADERS} ${SOURCES})
