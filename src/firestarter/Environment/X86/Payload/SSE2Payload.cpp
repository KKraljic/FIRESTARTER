#include <firestarter/Environment/X86/Payload/SSE2Payload.hpp>

using namespace firestarter::environment::x86::payload;

int SSE2Payload::compilePayload(std::vector<std::pair<std::string, unsigned>> proportion,
                                std::list<unsigned> dataCacheBufferSize,
                                unsigned ramBufferSize, unsigned thread,
                                unsigned numberOfLines) {}

std::list<std::string> SSE2Payload::getAvailableInstructions(void) {}

void SSE2Payload::init(unsigned long long *memoryAddr,
                       unsigned long long bufferSize) {
  X86Payload::init(memoryAddr, bufferSize, 1.654738925401e-10,
                   1.654738925401e-15);
}
