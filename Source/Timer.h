#pragma once

#include <chrono>

class Timer
{
public:
    Timer() = default;

    
    void start();

    
    double stop();

    
    double readMs() const;

    
    double readSeconds() const;

    
    void reset();

    // reset + start
    void restart();

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    TimePoint startTime{};
    double    accumulatedMs = 0.0;
    bool      running = false;
};
