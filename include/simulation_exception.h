#ifndef AILIFE_SIMULATION_EXCEPTION_H
#define AILIFE_SIMULATION_EXCEPTION_H

#include <stdexcept>
#include <string>

class SimulationException : public std::runtime_error {
  public:
    explicit SimulationException(const std::string& message) : std::runtime_error(message) {}
};

class LLMException : public SimulationException {
  public:
    using SimulationException::SimulationException;
};

class LLMRateLimitException : public LLMException {
  public:
    using LLMException::LLMException;
};

class LLMTimeoutException : public LLMException {
  public:
    using LLMException::LLMException;
};

class LLMInvalidResponseException : public LLMException {
  public:
    using LLMException::LLMException;
};

class IPCException : public SimulationException {
  public:
    using SimulationException::SimulationException;
};

class CommandParseException : public IPCException {
  public:
    using IPCException::IPCException;
};

#endif // AILIFE_SIMULATION_EXCEPTION_H
