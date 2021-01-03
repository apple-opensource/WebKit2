/*
 * Copyright (C) 2017 Igalia S.L.
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

#pragma once

#include "WebEvent.h"
#include <wpe/wpe.h>

namespace WebKit {

class ScrollGestureController {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ScrollGestureController() = default;

    struct wpe_input_axis_event* axisEvent()
    {
#if WPE_CHECK_VERSION(1, 5, 0)
        return &m_axisEvent.base;
#else
        return &m_axisEvent;
#endif
    }

    WebWheelEvent::Phase phase() { return m_phase; }

    bool isHandling() const { return m_handling; }
    bool handleEvent(const struct wpe_input_touch_event_raw*);

private:
    struct {
        uint32_t time { 0 };
        int32_t x { 0 };
        int32_t y { 0 };
    } m_start;

    struct {
        int32_t x { 0 };
        int32_t y { 0 };
    } m_offset;

    bool m_handling { false };
#if WPE_CHECK_VERSION(1, 5, 0)
    struct wpe_input_axis_2d_event m_axisEvent;
#else
    struct wpe_input_axis_event m_axisEvent;
#endif
    WebWheelEvent::Phase m_phase { WebWheelEvent::Phase::PhaseNone };
};

} // namespace WebKit
