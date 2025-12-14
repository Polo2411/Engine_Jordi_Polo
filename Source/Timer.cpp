#include "Globals.h"
#include "Timer.h"

void Timer::start()
{
    running = true;
    startTime = Clock::now();
}

double Timer::stop()
{
    if (running)
    {
        auto now = Clock::now();
        accumulatedMs += std::chrono::duration<double, std::milli>(now - startTime).count();
        running = false;
    }
    return accumulatedMs;
}

double Timer::readMs() const
{
    if (!running)
        return accumulatedMs;

    auto now = Clock::now();
    double ms = accumulatedMs +
        std::chrono::duration<double, std::milli>(now - startTime).count();
    return ms;
}

double Timer::readSeconds() const
{
    return readMs() * 0.001;
}

void Timer::reset()
{
    accumulatedMs = 0.0;
    running = false;
}

void Timer::restart()
{
    reset();
    start();
}
