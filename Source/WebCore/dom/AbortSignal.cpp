/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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
#include "AbortSignal.h"

#include "AbortAlgorithm.h"
#include "DOMException.h"
#include "DOMTimer.h"
#include "Event.h"
#include "EventNames.h"
#include "JSDOMException.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/JSCast.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(AbortSignal);

Ref<AbortSignal> AbortSignal::create(ScriptExecutionContext* context)
{
    return adoptRef(*new AbortSignal(context));
}

// https://dom.spec.whatwg.org/#dom-abortsignal-abort
Ref<AbortSignal> AbortSignal::abort(JSDOMGlobalObject& globalObject, ScriptExecutionContext& context, JSC::JSValue reason)
{
    ASSERT(reason);
    if (reason.isUndefined())
        reason = toJS(&globalObject, &globalObject, DOMException::create(AbortError));
    return adoptRef(*new AbortSignal(&context, Aborted::Yes, reason));
}

// https://dom.spec.whatwg.org/#dom-abortsignal-timeout
Ref<AbortSignal> AbortSignal::timeout(ScriptExecutionContext& context, uint64_t milliseconds)
{
    auto signal = adoptRef(*new AbortSignal(&context));
    signal->setHasActiveTimeoutTimer(true);
    auto action = [signal](ScriptExecutionContext& context) mutable {
        signal->setHasActiveTimeoutTimer(false);

        auto* globalObject = JSC::jsCast<JSDOMGlobalObject*>(context.globalObject());
        if (!globalObject)
            return;

        auto& vm = globalObject->vm();
        Locker locker { vm.apiLock() };
        signal->signalAbort(toJS(globalObject, globalObject, DOMException::create(TimeoutError)));
    };
    DOMTimer::install(context, WTFMove(action), Seconds::fromMilliseconds(milliseconds), true);
    return signal;
}

AbortSignal::AbortSignal(ScriptExecutionContext* context, Aborted aborted, JSC::JSValue reason)
    : ContextDestructionObserver(context)
    , m_aborted(aborted == Aborted::Yes)
    , m_reason(reason)
{
    ASSERT(reason);
}

AbortSignal::~AbortSignal() = default;

// https://dom.spec.whatwg.org/#abortsignal-signal-abort
void AbortSignal::signalAbort(JSC::JSValue reason)
{
    // 1. If signal's aborted flag is set, then return.
    if (m_aborted)
        return;
    
    // 2. Set signal’s aborted flag.
    m_aborted = true;

    // FIXME: This code is wrong: we should emit a write-barrier. Otherwise, GC can collect it.
    // https://bugs.webkit.org/show_bug.cgi?id=236353
    ASSERT(reason);
    m_reason.setWeakly(reason);

    Ref protectedThis { *this };
    auto algorithms = std::exchange(m_algorithms, { });
    for (auto& algorithm : algorithms)
        algorithm();

    // 5. Fire an event named abort at signal.
    dispatchEvent(Event::create(eventNames().abortEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

// https://dom.spec.whatwg.org/#abortsignal-follow
void AbortSignal::signalFollow(AbortSignal& signal)
{
    if (aborted())
        return;

    if (signal.aborted()) {
        signalAbort(signal.reason().getValue());
        return;
    }

    ASSERT(!m_followingSignal);
    m_followingSignal = signal;
    signal.addAlgorithm([weakThis = WeakPtr { this }] {
        if (weakThis)
            weakThis->signalAbort(weakThis->m_followingSignal ? weakThis->m_followingSignal->reason().getValue() : JSC::jsUndefined());
    });
}

void AbortSignal::eventListenersDidChange()
{
    m_hasAbortEventListener = hasEventListeners(eventNames().abortEvent);
}

bool AbortSignal::whenSignalAborted(AbortSignal& signal, Ref<AbortAlgorithm>&& algorithm)
{
    if (signal.aborted()) {
        algorithm->handleEvent();
        return true;
    }
    signal.addAlgorithm([algorithm = WTFMove(algorithm)]() mutable {
        algorithm->handleEvent();
    });
    return false;
}

void AbortSignal::throwIfAborted(JSC::JSGlobalObject& lexicalGlobalObject)
{
    if (!aborted())
        return;

    auto& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwException(&lexicalGlobalObject, scope, m_reason.getValue());
}

} // namespace WebCore
