#ifndef SEQUEENCEBARRIER_H
#define SEQUEENCEBARRIER_H

#include "Sequence.h"
#include <exception/Exception.h>


namespace disruptor {

	class WaitStrategy;

	class AlertException: public ExceptionLib::Exception {
	public:
		AlertException() : ExceptionBase(this, false, "AlertException", NULL) {}

		AlertException(QString errorMsg, const ExceptionBase* nested = NULL, bool trace = false )
			: ExceptionBase(this, trace, errorMsg.toStdString(), nested) {}

		AlertException(const AlertException& that) : ExceptionBase(that), Exception(that) {}
		virtual ~AlertException() throw () {}
	};

	class SequenceBarrier {
	public:
		virtual ~SequenceBarrier() {}

		virtual seq_t waitFor(seq_t) = 0;

		virtual seq_t cursor() const = 0;

		virtual bool alerted() const = 0;

		virtual void alert() = 0;

		virtual void clearAlert() = 0;

		virtual void checkAlert() const = 0;

	};

	template<class WaitStrategy_t>
	class ProcessingSequenceBarrier: public SequenceBarrier {
	public:

		ProcessingSequenceBarrier(
				WaitStrategy_t* waitStrategy,
				Sequence* cursorSequence,
				SequenceArray* dependentSequences)
			: m_waitStrategy(waitStrategy)
			, m_dependentSequences(dependentSequences)
			, m_alerted(false)
			, m_cursorSequence(cursorSequence)
		{

		}

		~ProcessingSequenceBarrier()
		{
			if (m_dependentSequences != NULL) {
				SequenceArray::destroy(m_dependentSequences);
				m_dependentSequences = NULL;
			}
		}

		seq_t waitFor(seq_t sequence)
		{
			checkAlert();
			return m_waitStrategy->waitFor(sequence, m_cursorSequence, MultipleSequenceReader(m_dependentSequences), *this);
		}

		seq_t cursor() const
		{
			return MultipleSequenceReader(m_dependentSequences).value();
		}

		bool alerted() const
		{
			return m_alerted;
		}

		void alert()
		{
			m_alerted = true;
			m_waitStrategy->signalAllWhenBlocking();
		}

		void clearAlert()
		{
			m_alerted = false;
		}

		void checkAlert() const
		{
			if (m_alerted) {
				throw AlertException();
			}
		}

	private:
		WaitStrategy_t* m_waitStrategy;
		SequenceArray* m_dependentSequences;
		volatile bool m_alerted;
		Sequence* m_cursorSequence;
	};
}

#endif // SEQUEENCEBARRIER_H
