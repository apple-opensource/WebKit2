/*
 * Copyright (C) 2014 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CompositingRunLoop.h"

#if USE(COORDINATED_GRAPHICS_THREADED)

#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>

namespace WebKit {

CompositingRunLoop::CompositingRunLoop(std::function<void ()>&& updateFunction)
    : m_runLoop(RunLoop::current())
    , m_updateTimer(m_runLoop, this, &CompositingRunLoop::updateTimerFired)
    , m_updateFunction(WTFMove(updateFunction))
{
}

void CompositingRunLoop::performTask(Function<void ()>&& function)
{
    ASSERT(isMainThread());
    m_runLoop.dispatch(WTFMove(function));
}

void CompositingRunLoop::performTaskSync(Function<void ()>&& function)
{
    ASSERT(isMainThread());
    LockHolder locker(m_dispatchSyncConditionMutex);
    m_runLoop.dispatch([this, function = WTFMove(function)] {
        LockHolder locker(m_dispatchSyncConditionMutex);
        function();
        m_dispatchSyncCondition.notifyOne();
    });
    m_dispatchSyncCondition.wait(m_dispatchSyncConditionMutex);
}

void CompositingRunLoop::startUpdateTimer(UpdateTiming timing)
{
    if (m_updateTimer.isActive())
        return;

    const static double targetFPS = 60;
    double nextUpdateTime = 0;
    if (timing == WaitUntilNextFrame)
        nextUpdateTime = std::max((1 / targetFPS) - (monotonicallyIncreasingTime() - m_lastUpdateTime), 0.0);

    m_updateTimer.startOneShot(nextUpdateTime);
}

void CompositingRunLoop::stopUpdateTimer()
{
    m_updateTimer.stop();
}

void CompositingRunLoop::updateTimerFired()
{
    m_updateFunction();
    m_lastUpdateTime = monotonicallyIncreasingTime();
}

void CompositingRunLoop::run()
{
    m_runLoop.run();
}

void CompositingRunLoop::stop()
{
    m_updateTimer.stop();
    m_runLoop.stop();
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS_THREADED)
