#pragma once

#include <chrono>

class Timer
{
public:
    Timer() = default;

    // Reinicia el temporizador y lo pone en marcha
    void start();

    // Para el timer y devuelve el tiempo transcurrido en ms
    double stop();

    // Lee el tiempo en ms SIN parar el timer
    double readMs() const;

    // Lee el tiempo en segundos SIN parar el timer
    double readSeconds() const;

    // Pone el tiempo a 0 y NO lo arranca
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
