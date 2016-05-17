# concurrent
C++11 Disruptor pattern and other concurrency tools

This repository contains an implementation in C++11 of the disruptor pattern published by LMAX.
The original LMAX version is written in java and can be found here https://github.com/LMAX-Exchange/disruptor.
It was also the subject of an article by Martin Fowler http://martinfowler.com/articles/lmax.html.

I had this implementation using Qt for threading and synchronization sitting around for a few years
and now that C++11 threading support seems to be available in all major compilers, I decided to port
it. Together with the disruptor, there are some other concurrency tools, like an implementation
of the Active Object pattern implemented using the disruptor. 
