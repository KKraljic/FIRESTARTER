#ifndef INCLUDE_FIRESTARTER_ENVIRONMENT_X86_PAYLOAD_PAYLOAD_H
#define INCLUDE_FIRESTARTER_ENVIRONMENT_X86_PAYLOAD_PAYLOAD_H

#include <firestarter/Environment/Payload/Payload.hpp>
#include <firestarter/Logging/Log.hpp>
#include <firestarter/ThreadData.hpp>

#include <llvm/ADT/StringMap.h>

#include <asmjit/x86.h>

#define INIT_BLOCKSIZE 1024

namespace firestarter::environment::x86::payload {

class X86Payload : public environment::payload::Payload {
private:
  // we can use this to check, if our platform support this payload
  llvm::StringMap<bool> *_supportedFeatures;
  std::list<std::string> featureRequests;

protected:
  //  asmjit::CodeHolder code;
  asmjit::JitRuntime rt;
  // typedef int (*LoadFunction)(firestarter::ThreadData *);
  typedef unsigned long long (*LoadFunction)(unsigned long long *,
                                             volatile unsigned long long *,
                                             unsigned long long);
  LoadFunction loadFunction = nullptr;

  llvm::StringMap<bool> *const &supportedFeatures = _supportedFeatures;

public:
  X86Payload(llvm::StringMap<bool> *supportedFeatures,
             std::initializer_list<std::string> featureRequests,
             std::string name)
      : Payload(name), _supportedFeatures(supportedFeatures),
        featureRequests(featureRequests){};
  ~X86Payload(){};

  bool isAvailable(void) override {
    bool available = true;

    for (std::string feature : featureRequests) {
      available &= (*supportedFeatures)[feature];
    }

    return available;
  };

    // A generic implemenation for all x86 payloads
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
  void init(unsigned long long *memoryAddr, unsigned long long bufferSize,
            double firstValue, double lastValue);
#pragma GCC diagnostic pop
#pragma clang diagnostic pop
  // use cpuid and usleep as low load
  void lowLoadFunction(volatile unsigned long long *addrHigh,
                       unsigned long long period) override;

  unsigned long long highLoadFunction(unsigned long long *addrMem,
                                      volatile unsigned long long *addrHigh,
                                      unsigned long long iterations) override;
};

} // namespace firestarter::environment::x86::payload

#endif
