#include "IrSenderSimulator.h"

void IrSenderSimulator::send(const IrSequence& irSequence, frequency_t frequency) {
    stream.print(F("IrSenderSimulator: "));
    boolean printedSomething = IrSignal::dumpFrequency(stream, frequency);
    if (printedSomething)
        stream.print(' ');
    irSequence.dump(stream, true);
}
