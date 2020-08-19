#include <firestarter/Environment/X86/X86Environment.hpp>
#include <firestarter/Logging/Log.hpp>

#include <cstdio>

using namespace firestarter::environment::x86;

void X86Environment::evaluateFunctions(void) {
  for (auto ctor : this->platformConfigsCtor) {
    // add asmjit for model and family detection
    this->platformConfigs.push_back(
        ctor(&this->cpuFeatures, this->cpuInfo.familyId(),
             this->cpuInfo.modelId(), this->getNumberOfThreadsPerCore()));
  }

  for (auto ctor : this->fallbackPlatformConfigsCtor) {
    this->fallbackPlatformConfigs.push_back(
        ctor(&this->cpuFeatures, this->cpuInfo.familyId(),
             this->cpuInfo.modelId(), this->getNumberOfThreadsPerCore()));
  }
}

int X86Environment::selectFunction(unsigned functionId,
                                   bool allowUnavailablePayload) {
  unsigned id = 1;
  std::string defaultPayloadName("");

  // if functionId is 0 get the default or fallback
  for (auto config : this->platformConfigs) {
    for (auto const &[thread, functionName] : config->getThreadMap()) {
      // the selected function
      if (id == functionId) {
        if (!config->isAvailable()) {
          log::error() << "Error: Function " << functionId << " (\""
                       << functionName << "\") requires "
                       << config->payload->name
                       << ", which is not supported by the processor.";
          if (!allowUnavailablePayload) {
            return EXIT_FAILURE;
          }
        }
        // found function
        config->printCodePathSummary(thread);
        this->_selectedConfig =
            new ::firestarter::environment::platform::Config(config, thread);
        return EXIT_SUCCESS;
      }
      // default function
      if (0 == functionId && config->isDefault()) {
        if (thread == this->getNumberOfThreadsPerCore()) {
          config->printCodePathSummary(thread);
          this->_selectedConfig =
              new ::firestarter::environment::platform::Config(config, thread);
          return EXIT_SUCCESS;
        } else {
          defaultPayloadName = config->payload->name;
        }
      }
      id++;
    }
  }

  // no default found
  // use fallback
  if (0 == functionId) {
    if (!defaultPayloadName.empty()) {
      // default payload available, but number of threads per core is not
      // supported
      log::warn() << "Warning: no " << defaultPayloadName << " code path for "
                  << this->getNumberOfThreadsPerCore() << " threads per code!";
    }
    log::warn() << "Warning: " << this->vendor << " " << this->getModel()
                << " is not supported by this version of FIRESTARTER!\n"
                << "Check project website for updates.";

    // loop over available implementation and check if they are marked as
    // fallback
    for (auto config : this->fallbackPlatformConfigs) {
      if (config->isAvailable()) {
        auto selectedThread = 0;
        auto selectedFunctionName = std::string("");
        for (auto const &[thread, functionName] : config->getThreadMap()) {
          if (thread == this->getNumberOfThreadsPerCore()) {
            selectedThread = thread;
            selectedFunctionName = functionName;
          }
        }
        if (selectedThread == 0) {
          selectedThread = config->getThreadMap().begin()->first;
          selectedFunctionName = config->getThreadMap().begin()->second;
        }
        this->_selectedConfig =
            new ::firestarter::environment::platform::Config(config, selectedThread);
        log::warn() << "Warning: using function " << selectedFunctionName
                    << " as fallback.\n"
                    << "You can use the parameter --function to try other "
                       "functions.";
        return EXIT_SUCCESS;
      }
    }

    // no fallback found
    log::error() << "Error: No fallback implementation found for available ISA "
                    "extensions.";
    return EXIT_FAILURE;
  }

  log::error() << "Error: unknown function id: " << functionId
               << ", see --avail for available ids";
  return EXIT_FAILURE;
}

void X86Environment::printFunctionSummary() {
  log::info() << " available load-functions:\n"
              << "  ID   | NAME                           | available on this "
                 "system | payload default setting\n"
              << "  "
                 "-------------------------------------------------------------"
                 "-------------------------------------------------------------"
                 "-----------------------------";

  unsigned id = 1;

  for (auto const &config : this->platformConfigs) {
    for (auto const &[thread, functionName] : config->getThreadMap()) {
      const char *available = config->isAvailable() ? "yes" : "no";
      const char *fmt = "  %4u | %-30s | %-24s | %s";
      int sz =
          std::snprintf(nullptr, 0, fmt, id, functionName.c_str(), available,
                        config->getDefaultPayloadSettingsString().c_str());
      std::vector<char> buf(sz + 1);
      std::snprintf(&buf[0], buf.size(), fmt, id, functionName.c_str(),
                    available,
                    config->getDefaultPayloadSettingsString().c_str());
      log::info() << std::string(&buf[0]);
      id++;
    }
  }
}
