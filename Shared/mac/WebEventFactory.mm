/*
 * Copyright (C) 2010, 2011, 2013 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebEventFactory.h"

#if USE(APPKIT)

#import "WebKitSystemInterface.h"
#import <WebCore/KeyboardEvent.h>
#import <WebCore/NSMenuSPI.h>
#import <WebCore/PlatformEventFactoryMac.h>
#import <WebCore/Scrollbar.h>
#import <WebCore/WindowsKeyboardCodes.h>
#import <wtf/ASCIICType.h>


using namespace WebCore;

@interface NSEvent (WebNSEventDetails)
- (NSInteger)_scrollCount;
- (CGFloat)_unacceleratedScrollingDeltaX;
- (CGFloat)_unacceleratedScrollingDeltaY;
@end

namespace WebKit {

// FIXME: This is a huge copy/paste from WebCore/PlatformEventFactoryMac.mm. The code should be merged.

static WebMouseEvent::Button currentMouseButton()
{
    NSUInteger pressedMouseButtons = [NSEvent pressedMouseButtons];
    if (!pressedMouseButtons)
        return WebMouseEvent::NoButton;
    if (pressedMouseButtons == 1 << 0)
        return WebMouseEvent::LeftButton;
    if (pressedMouseButtons == 1 << 1)
        return WebMouseEvent::RightButton;
    return WebMouseEvent::MiddleButton;
}

static WebMouseEvent::Button mouseButtonForEvent(NSEvent *event)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    switch ([event type]) {
        case NSLeftMouseDown:
        case NSLeftMouseUp:
        case NSLeftMouseDragged:
            return WebMouseEvent::LeftButton;
        case NSRightMouseDown:
        case NSRightMouseUp:
        case NSRightMouseDragged:
            return WebMouseEvent::RightButton;
        case NSOtherMouseDown:
        case NSOtherMouseUp:
        case NSOtherMouseDragged:
            return WebMouseEvent::MiddleButton;
#if defined(__LP64__)
        case NSEventTypePressure:
#endif
        case NSMouseEntered:
        case NSMouseExited:
            return currentMouseButton();
        default:
            return WebMouseEvent::NoButton;
    }
#pragma clang diagnostic pop
}

static WebEvent::Type mouseEventTypeForEvent(NSEvent* event)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    switch ([event type]) {
        case NSLeftMouseDragged:
        case NSMouseEntered:
        case NSMouseExited:
        case NSMouseMoved:
        case NSOtherMouseDragged:
        case NSRightMouseDragged:
            return WebEvent::MouseMove;
        case NSLeftMouseDown:
        case NSRightMouseDown:
        case NSOtherMouseDown:
            return WebEvent::MouseDown;
        case NSLeftMouseUp:
        case NSRightMouseUp:
        case NSOtherMouseUp:
            return WebEvent::MouseUp;
        default:
            return WebEvent::MouseMove;
    }
#pragma clang diagnostic pop
}

static int clickCountForEvent(NSEvent *event)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    switch ([event type]) {
        case NSLeftMouseDown:
        case NSLeftMouseUp:
        case NSLeftMouseDragged:
        case NSRightMouseDown:
        case NSRightMouseUp:
        case NSRightMouseDragged:
        case NSOtherMouseDown:
        case NSOtherMouseUp:
        case NSOtherMouseDragged:
            return [event clickCount];
        default:
            return 0;
    }
#pragma clang diagnostic pop
}

static NSPoint globalPointForEvent(NSEvent *event)
{
    switch ([event type]) {
#if defined(__LP64__)
    case NSEventTypePressure:
#endif
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    case NSLeftMouseDown:
    case NSLeftMouseDragged:
    case NSLeftMouseUp:
    case NSMouseEntered:
    case NSMouseExited:
    case NSMouseMoved:
    case NSOtherMouseDown:
    case NSOtherMouseDragged:
    case NSOtherMouseUp:
    case NSRightMouseDown:
    case NSRightMouseDragged:
    case NSRightMouseUp:
    case NSScrollWheel:
#pragma clang diagnostic pop
        return WebCore::globalPoint([event locationInWindow], [event window]);
    default:
        return NSZeroPoint;
    }
}

static NSPoint pointForEvent(NSEvent *event, NSView *windowView)
{
    switch ([event type]) {
#if defined(__LP64__)
    case NSEventTypePressure:
#endif
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    case NSLeftMouseDown:
    case NSLeftMouseDragged:
    case NSLeftMouseUp:
    case NSMouseEntered:
    case NSMouseExited:
    case NSMouseMoved:
    case NSOtherMouseDown:
    case NSOtherMouseDragged:
    case NSOtherMouseUp:
    case NSRightMouseDown:
    case NSRightMouseDragged:
    case NSRightMouseUp:
    case NSScrollWheel: {
#pragma clang diagnostic pop
        // Note: This will have its origin at the bottom left of the window unless windowView is flipped.
        // In those cases, the Y coordinate gets flipped by Widget::convertFromContainingWindow.
        NSPoint location = [event locationInWindow];
        if (windowView)
            location = [windowView convertPoint:location fromView:nil];
        return location;
    }
    default:
        return NSZeroPoint;
    }
}

static WebWheelEvent::Phase phaseForEvent(NSEvent *event)
{
    uint32_t phase = WebWheelEvent::PhaseNone;
    if ([event phase] & NSEventPhaseBegan)
        phase |= WebWheelEvent::PhaseBegan;
    if ([event phase] & NSEventPhaseStationary)
        phase |= WebWheelEvent::PhaseStationary;
    if ([event phase] & NSEventPhaseChanged)
        phase |= WebWheelEvent::PhaseChanged;
    if ([event phase] & NSEventPhaseEnded)
        phase |= WebWheelEvent::PhaseEnded;
    if ([event phase] & NSEventPhaseCancelled)
        phase |= WebWheelEvent::PhaseCancelled;
    if ([event phase] & NSEventPhaseMayBegin)
        phase |= WebWheelEvent::PhaseMayBegin;

    return static_cast<WebWheelEvent::Phase>(phase);
}

static WebWheelEvent::Phase momentumPhaseForEvent(NSEvent *event)
{
    uint32_t phase = WebWheelEvent::PhaseNone; 

    if ([event momentumPhase] & NSEventPhaseBegan)
        phase |= WebWheelEvent::PhaseBegan;
    if ([event momentumPhase] & NSEventPhaseStationary)
        phase |= WebWheelEvent::PhaseStationary;
    if ([event momentumPhase] & NSEventPhaseChanged)
        phase |= WebWheelEvent::PhaseChanged;
    if ([event momentumPhase] & NSEventPhaseEnded)
        phase |= WebWheelEvent::PhaseEnded;
    if ([event momentumPhase] & NSEventPhaseCancelled)
        phase |= WebWheelEvent::PhaseCancelled;

    return static_cast<WebWheelEvent::Phase>(phase);
}

static inline String textFromEvent(NSEvent* event, bool replacesSoftSpace)
{
    if (replacesSoftSpace)
        return emptyString();
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if ([event type] == NSFlagsChanged)
#pragma clang diagnostic pop
        return emptyString();
    return String([event characters]);
}

static inline String unmodifiedTextFromEvent(NSEvent* event, bool replacesSoftSpace)
{
    if (replacesSoftSpace)
        return emptyString();
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if ([event type] == NSFlagsChanged)
#pragma clang diagnostic pop
        return emptyString();
    return String([event charactersIgnoringModifiers]);
}

static bool isKeypadEvent(NSEvent* event)
{
    // Check that this is the type of event that has a keyCode.
    switch ([event type]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        case NSKeyDown:
        case NSKeyUp:
        case NSFlagsChanged:
#pragma clang diagnostic pop
            break;
        default:
            return false;
    }

    switch ([event keyCode]) {
        case 71: // Clear
        case 81: // =
        case 75: // /
        case 67: // *
        case 78: // -
        case 69: // +
        case 76: // Enter
        case 65: // .
        case 82: // 0
        case 83: // 1
        case 84: // 2
        case 85: // 3
        case 86: // 4
        case 87: // 5
        case 88: // 6
        case 89: // 7
        case 91: // 8
        case 92: // 9
            return true;
     }
     
     return false;
}

static inline bool isKeyUpEvent(NSEvent *event)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if ([event type] != NSFlagsChanged)
        return [event type] == NSKeyUp;
    // FIXME: This logic fails if the user presses both Shift keys at once, for example:
    // we treat releasing one of them as keyDown.
    switch ([event keyCode]) {
        case 54: // Right Command
        case 55: // Left Command
            return ([event modifierFlags] & NSCommandKeyMask) == 0;
            
        case 57: // Capslock
            return ([event modifierFlags] & NSAlphaShiftKeyMask) == 0;
            
        case 56: // Left Shift
        case 60: // Right Shift
            return ([event modifierFlags] & NSShiftKeyMask) == 0;
            
        case 58: // Left Alt
        case 61: // Right Alt
            return ([event modifierFlags] & NSAlternateKeyMask) == 0;
            
        case 59: // Left Ctrl
        case 62: // Right Ctrl
            return ([event modifierFlags] & NSControlKeyMask) == 0;
            
        case 63: // Function
            return ([event modifierFlags] & NSFunctionKeyMask) == 0;
    }
#pragma clang diagnostic pop
    return false;
}

static inline WebEvent::Modifiers modifiersForEvent(NSEvent *event)
{
    unsigned modifiers = 0;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if ([event modifierFlags] & NSAlphaShiftKeyMask)
        modifiers |= WebEvent::CapsLockKey;
    if ([event modifierFlags] & NSShiftKeyMask)
        modifiers |= WebEvent::ShiftKey;
    if ([event modifierFlags] & NSControlKeyMask)
        modifiers |= WebEvent::ControlKey;
    if ([event modifierFlags] & NSAlternateKeyMask)
        modifiers |= WebEvent::AltKey;
    if ([event modifierFlags] & NSCommandKeyMask)
        modifiers |= WebEvent::MetaKey;
#pragma clang diagnostic pop
    return (WebEvent::Modifiers)modifiers;
}

static int typeForEvent(NSEvent *event)
{
    if ([NSMenu respondsToSelector:@selector(menuTypeForEvent:)])
        return static_cast<int>([NSMenu menuTypeForEvent:event]);
    
    if (mouseButtonForEvent(event) == WebMouseEvent::RightButton)
        return static_cast<int>(NSMenuTypeContextMenu);
    
    if (mouseButtonForEvent(event) == WebMouseEvent::LeftButton && (modifiersForEvent(event) & WebEvent::ControlKey))
        return static_cast<int>(NSMenuTypeContextMenu);
    
    return static_cast<int>(NSMenuTypeNone);
}

bool WebEventFactory::shouldBeHandledAsContextClick(const WebCore::PlatformMouseEvent& event)
{
    return (static_cast<NSMenuType>(event.menuTypeForEvent()) == NSMenuTypeContextMenu);
}

WebMouseEvent WebEventFactory::createWebMouseEvent(NSEvent *event, NSEvent *lastPressureEvent, NSView *windowView)
{
    NSPoint position = pointForEvent(event, windowView);
    NSPoint globalPosition = globalPointForEvent(event);

    WebEvent::Type type = mouseEventTypeForEvent(event);
#if defined(__LP64__)
    if ([event type] == NSEventTypePressure) {
        // Since AppKit doesn't send mouse events for force down or force up, we have to use the current pressure
        // event and lastPressureEvent to detect if this is MouseForceDown, MouseForceUp, or just MouseForceChanged.
        if (lastPressureEvent.stage == 1 && event.stage == 2)
            type = WebEvent::MouseForceDown;
        else if (lastPressureEvent.stage == 2 && event.stage == 1)
            type = WebEvent::MouseForceUp;
        else
            type = WebEvent::MouseForceChanged;
    }
#endif

    WebMouseEvent::Button button = mouseButtonForEvent(event);
    float deltaX = [event deltaX];
    float deltaY = [event deltaY];
    float deltaZ = [event deltaZ];
    int clickCount = clickCountForEvent(event);
    WebEvent::Modifiers modifiers = modifiersForEvent(event);
    double timestamp = eventTimeStampSince1970(event);
    int eventNumber = [event eventNumber];
    int menuTypeForEvent = typeForEvent(event);

    double force = 0;
#if defined(__LP64__)
    int stage = [event type] == NSEventTypePressure ? event.stage : lastPressureEvent.stage;
    double pressure = [event type] == NSEventTypePressure ? event.pressure : lastPressureEvent.pressure;
    force = pressure + stage;
#endif

    return WebMouseEvent(type, button, IntPoint(position), IntPoint(globalPosition), deltaX, deltaY, deltaZ, clickCount, modifiers, timestamp, force, WebMouseEvent::SyntheticClickType::NoTap, eventNumber, menuTypeForEvent);
}

WebWheelEvent WebEventFactory::createWebWheelEvent(NSEvent *event, NSView *windowView)
{
    NSPoint position = pointForEvent(event, windowView);
    NSPoint globalPosition = globalPointForEvent(event);

    BOOL continuous;
    float deltaX = 0;
    float deltaY = 0;
    float wheelTicksX = 0;
    float wheelTicksY = 0;

    WKGetWheelEventDeltas(event, &deltaX, &deltaY, &continuous);
    
    if (continuous) {
        // smooth scroll events
        wheelTicksX = deltaX / static_cast<float>(Scrollbar::pixelsPerLineStep());
        wheelTicksY = deltaY / static_cast<float>(Scrollbar::pixelsPerLineStep());
    } else {
        // plain old wheel events
        wheelTicksX = deltaX;
        wheelTicksY = deltaY;
        deltaX *= static_cast<float>(Scrollbar::pixelsPerLineStep());
        deltaY *= static_cast<float>(Scrollbar::pixelsPerLineStep());
    }

    WebWheelEvent::Granularity granularity  = WebWheelEvent::ScrollByPixelWheelEvent;
    bool directionInvertedFromDevice        = [event isDirectionInvertedFromDevice];
    WebWheelEvent::Phase phase              = phaseForEvent(event);
    WebWheelEvent::Phase momentumPhase      = momentumPhaseForEvent(event);
    bool hasPreciseScrollingDeltas          = continuous;

    uint32_t scrollCount;
    FloatSize unacceleratedScrollingDelta;

    static bool nsEventSupportsScrollCount = [NSEvent instancesRespondToSelector:@selector(_scrollCount)];
    if (nsEventSupportsScrollCount) {
        scrollCount = [event _scrollCount];
        unacceleratedScrollingDelta = FloatSize([event _unacceleratedScrollingDeltaX], [event _unacceleratedScrollingDeltaY]);
    } else {
        scrollCount = 0;
        unacceleratedScrollingDelta = FloatSize(deltaX, deltaY);
    }

    WebEvent::Modifiers modifiers           = modifiersForEvent(event);
    double timestamp                        = eventTimeStampSince1970(event);
    
    return WebWheelEvent(WebEvent::Wheel, IntPoint(position), IntPoint(globalPosition), FloatSize(deltaX, deltaY), FloatSize(wheelTicksX, wheelTicksY), granularity, directionInvertedFromDevice, phase, momentumPhase, hasPreciseScrollingDeltas, scrollCount, unacceleratedScrollingDelta, modifiers, timestamp);
}

WebKeyboardEvent WebEventFactory::createWebKeyboardEvent(NSEvent *event, bool handledByInputMethod, bool replacesSoftSpace, const Vector<WebCore::KeypressCommand>& commands)
{
    WebEvent::Type type             = isKeyUpEvent(event) ? WebEvent::KeyUp : WebEvent::KeyDown;
    String text                     = textFromEvent(event, replacesSoftSpace);
    String unmodifiedText           = unmodifiedTextFromEvent(event, replacesSoftSpace);
    String keyIdentifier            = keyIdentifierForKeyEvent(event);
    int windowsVirtualKeyCode       = windowsKeyCodeForKeyEvent(event);
    int nativeVirtualKeyCode        = [event keyCode];
    int macCharCode                 = WKGetNSEventKeyChar(event);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    bool autoRepeat                 = ([event type] != NSFlagsChanged) && [event isARepeat];
#pragma clang diagnostic pop
    bool isKeypad                   = isKeypadEvent(event);
    bool isSystemKey                = false; // SystemKey is always false on the Mac.
    WebEvent::Modifiers modifiers   = modifiersForEvent(event);
    double timestamp                = eventTimeStampSince1970(event);

    // Always use 13 for Enter/Return -- we don't want to use AppKit's different character for Enter.
    if (windowsVirtualKeyCode == VK_RETURN) {
        text = "\r";
        unmodifiedText = "\r";
    }

    // AppKit sets text to "\x7F" for backspace, but the correct KeyboardEvent character code is 8.
    if (windowsVirtualKeyCode == VK_BACK) {
        text = "\x8";
        unmodifiedText = "\x8";
    }

    // Always use 9 for Tab -- we don't want to use AppKit's different character for shift-tab.
    if (windowsVirtualKeyCode == VK_TAB) {
        text = "\x9";
        unmodifiedText = "\x9";
    }

    return WebKeyboardEvent(type, text, unmodifiedText, keyIdentifier, windowsVirtualKeyCode, nativeVirtualKeyCode, macCharCode, handledByInputMethod, commands, autoRepeat, isKeypad, isSystemKey, modifiers, timestamp);
}

} // namespace WebKit

#endif // USE(APPKIT)
