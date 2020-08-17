#ifndef INCLUDE_FIRESTARTER_ENVIRONMENT_X86_PLATFORM_NEHALEMCONFIG_H
#define INCLUDE_FIRESTARTER_ENVIRONMENT_X86_PLATFORM_NEHALEMCONFIG_H

#include <firestarter/Environment/X86/Payload/SSE2Payload.hpp>
#include <firestarter/Environment/X86/Platform/X86PlatformConfig.hpp>

namespace firestarter::environment::x86::platform {
class NehalemConfig : public X86PlatformConfig {

public:
  NehalemConfig(llvm::StringMap<bool> *supportedFeatures, unsigned family,
                unsigned model, unsigned threads)
      : X86PlatformConfig("NHM_COREI", 6, {30, 37, 23}, {1, 2},
                          {32768, 262144, 1572864}, 104857600, family, model,
                          threads,
                          new payload::SSE2Payload(supportedFeatures)){};
  ~NehalemConfig(){};

  std::vector<std::pair<std::string, unsigned>> getDefaultPayloadSettings(void) override {
    return std::vector<std::pair<std::string, unsigned>>(
        {{"RAM_P", 1}, {"L1_LS", 70}, {"REG", 2}});
  }
};
} // namespace firestarter::environment::x86::platform

#endif
