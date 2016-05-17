#ifndef SEQUENCE_H
#define SEQUENCE_H

#include <atomic>
#include <stdint.h>
#include <memory>

namespace disruptor {

	typedef size_t seq_t;

	class Sequence {
	public:

		static const seq_t INITIAL_CURSOR_VALUE = ~0;

        Sequence() : m_atomic(INITIAL_CURSOR_VALUE) {}

        Sequence(seq_t initialValue) : m_atomic(initialValue) {}

        Sequence(Sequence& other) : m_atomic(other.m_atomic.load()) {}

		seq_t value() const {
            return m_atomic;
		}

		operator seq_t() const {
			return value();
		}

		void changeValue(seq_t val) {
            m_atomic = val;
		}

		Sequence& operator=(seq_t val) {
			changeValue(val);
			return *this;
		}

		Sequence& operator=(const Sequence& other) {
			changeValue(other.value());
			return *this;
		}

		seq_t inc() {
            return m_atomic.fetch_add(1, std::memory_order_release) + 1;
		}

		seq_t add(seq_t increment) {
            return m_atomic.fetch_add(increment) + increment;
		}

		bool compareAndSet(seq_t expected, seq_t newVal) {
            return m_atomic.compare_exchange_weak(expected, newVal, std::memory_order_release, std::memory_order_relaxed);
        }

    private:
        std::atomic<seq_t> m_atomic;
	};


	struct SequenceArray {

        typedef std::shared_ptr<Sequence>* iterator;
        typedef std::shared_ptr<Sequence> const* const_iterator;

		const size_t size;

        std::shared_ptr<Sequence>& operator[](size_t pos) {
			return sequences[pos];
		}

        std::shared_ptr<Sequence> const& operator[](size_t pos) const {
			return sequences[pos];
		}

		iterator begin() { return sequences; }
        iterator end() { return begin() + size; }

		const_iterator begin() const { return sequences; }
		const_iterator end() const { return begin() + size; }

		static SequenceArray* build(size_t size) {
			const size_t alloc = sizeof(Sequence*)*size+sizeof(SequenceArray);
			char* buffer = new char[alloc];
			SequenceArray* ret = new(buffer) SequenceArray(size);
			return ret;
		}

		static void destroy(void* holderPtr) {
			SequenceArray* holder = reinterpret_cast<SequenceArray*>(holderPtr);
			holder->~SequenceArray();
			delete[] reinterpret_cast<char*>(holder);
		}

		template<class SequenceIterator>
		static SequenceArray* addSequence(const SequenceArray* orig, SequenceIterator begin, SequenceIterator end)
		{
			const size_t size = ((orig == NULL) ? 0 : orig->size) + (end - begin);
			SequenceArray* newArray = build(size);

			size_t i = 0;

			if (orig != NULL) {
				for (; i < orig->size; ++i) {
					(*newArray)[i] = (*orig)[i];
				}
			}

			for (SequenceIterator it = begin; i < size; ++it, ++i) {
				(*newArray)[i] = *it;
			}
			return newArray;
		}

		template<class SequenceIterator>
		static SequenceArray* build(SequenceIterator begin, SequenceIterator end)
		{
			const size_t size = end - begin;
			SequenceArray* newArray = build(size);

			size_t i = 0;
			for (SequenceIterator it = begin; i < size; ++it) {
				(*newArray)[i++] = *it;
			}
			return newArray;
		}

        static SequenceArray* removeSequence(const SequenceArray* orig, const std::shared_ptr<Sequence> toRemove)
		{
            const size_t removals = countMatching(orig, toRemove.get());

			const size_t size = orig->size - removals;
			SequenceArray* newArray = build(size);

			for (size_t i = 0, j = 0; i < orig->size; ++i) {
				if ((*orig)[i] == toRemove) {
					(*newArray)[j++] = (*orig)[i];
				}
			}
			return newArray;
		}

		static SequenceArray* copy(const SequenceArray* orig)
		{
			SequenceArray* newArray = build(orig->size);
			for (size_t i = 0; i < orig->size; ++i) {
				(*newArray)[i] = (*orig)[i];
			}
			return newArray;
		}

	private:

		static size_t countMatching(const SequenceArray* array, const Sequence* seq) {
			size_t count = 0;
			for (size_t i = 0; i < array->size; ++i) {
                if ((*array)[i].get() == seq) {
					++count;
				}
			}
			return count;
		}

        std::shared_ptr<Sequence> sequences[1];
		SequenceArray(size_t s) : size(s) {}
		~SequenceArray() {}
	};


	inline seq_t minimumSequence(const SequenceArray* sequences, seq_t minimum) {
		SequenceArray::const_iterator it = sequences->begin();
		SequenceArray::const_iterator end = sequences->end();
		for (; it != end; ++it) {
			seq_t value = (*it)->value();
			if (value == Sequence::INITIAL_CURSOR_VALUE) {
				return Sequence::INITIAL_CURSOR_VALUE;
			}
			minimum = std::min(minimum, value);
		}
		return minimum;
	}


	inline seq_t minimumSequence(const SequenceArray* sequences) {
		return minimumSequence(sequences, ~0-1);
	}

	struct MinimumReader {
		virtual ~MinimumReader() {}
		virtual seq_t value() const = 0;
	};

	struct OneSequenceReader: public MinimumReader {
		const Sequence* seq;
		OneSequenceReader(Sequence* s) : seq(s) {}
		seq_t value() const {
			return *seq;
		}
	};

	struct MultipleSequenceReader: public MinimumReader {
		const SequenceArray* seqs;
		MultipleSequenceReader(const SequenceArray* s) : seqs(s) {}
		seq_t value() const {
			return minimumSequence(seqs);
		}
	};

}

#endif // SEQUENCE_H
