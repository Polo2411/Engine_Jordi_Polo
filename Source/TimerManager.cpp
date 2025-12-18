#include "Globals.h"
#include "TimeManager.h"

bool TimeManager::init()
{
    appTimer.restart();
    frameCount = 0;
    historyIndex = 0;
    frameMsHistory.fill(0.0f);
    firstFrame = true;

    realTimeSinceStartupSeconds = 0.0;
    realDeltaTimeSeconds = 0.0;
    gameTimeSeconds = 0.0;
    gameDeltaTimeSeconds = 0.0;
    fps = 0.0f;
    avgFrameMs = 0.0f;

    return true;
}

void TimeManager::update()
{
    // Real time
    realTimeSinceStartupSeconds = appTimer.readSeconds();

    static double lastRealTime = 0.0;

    if (firstFrame)
    {
        realDeltaTimeSeconds = 0.0;
        firstFrame = false;
    }
    else
    {
        realDeltaTimeSeconds = realTimeSinceStartupSeconds - lastRealTime;
    }

    lastRealTime = realTimeSinceStartupSeconds;

    // dt of the game
    if (paused)
        gameDeltaTimeSeconds = 0.0;
    else
        gameDeltaTimeSeconds = realDeltaTimeSeconds * timeScale;

    gameTimeSeconds += gameDeltaTimeSeconds;

    // Save In historial (for ImGui::PlotLines)
    float frameMs = static_cast<float>(realDeltaTimeSeconds * 1000.0);
    frameMsHistory[historyIndex] = frameMs;
    historyIndex = (historyIndex + 1) % kHistorySize;

    // Average
    float sum = 0.0f;
    size_t cnt = 0;
    for (float v : frameMsHistory)
    {
        if (v > 0.0f)
        {
            sum += v;
            ++cnt;
        }
    }

    if (cnt > 0)
    {
        avgFrameMs = sum / cnt;
        fps = (avgFrameMs > 0.0f) ? (1000.0f / avgFrameMs) : 0.0f;
    }
    else
    {
        avgFrameMs = 0.0f;
        fps = 0.0f;
    }

    ++frameCount;
}

void TimeManager::setTimeScale(float scale)
{
    // Limits
    if (scale < 0.0f) scale = 0.0f;
    if (scale > 4.0f) scale = 4.0f;
    timeScale = scale;
}

void TimeManager::setPaused(bool p)
{
    paused = p;
}

void TimeManager::togglePaused()
{
    paused = !paused;
}
