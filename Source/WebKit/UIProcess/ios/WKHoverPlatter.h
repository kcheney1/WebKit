/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)

#import "NativeWebMouseEvent.h"

namespace WebCore {
class FloatRect;
struct InteractionRegion;
}

namespace WebKit {
struct InteractionInformationAtPosition;
struct InteractionInformationRequest;
}

@class UIView;
@class WKHoverPlatter;

@protocol WKHoverPlatterDelegate

@required
- (void)positionInformationForHoverPlatter:(WKHoverPlatter *)hoverPlatter withRequest:(WebKit::InteractionInformationRequest&)request completionHandler:(void (^)(WebKit::InteractionInformationAtPosition))completionHandler;

- (void)interactionRegionsForHoverPlatter:(WKHoverPlatter *)hoverPlatter inRect:(WebCore::FloatRect)rectInContentCoordinates completionHandler:(void (^)(Vector<WebCore::InteractionRegion>))completionHandler;

@end

@interface WKHoverPlatter : NSObject

- (instancetype)initWithView:(UIView *)view delegate:(id <WKHoverPlatterDelegate>)delegate;

- (void)didReceiveMouseEvent:(const WebKit::NativeWebMouseEvent&)event;
- (void)didLongPressAtPoint:(WebCore::FloatPoint)point;
- (void)didSingleTapAtPoint:(WebCore::FloatPoint)point;
- (void)didDoubleTapAtPoint:(WebCore::FloatPoint)point;

- (WebCore::FloatPoint)adjustedPointForPoint:(WebCore::FloatPoint)point;
- (WebKit::NativeWebMouseEvent)adjustedEventForEvent:(const WebKit::NativeWebMouseEvent&)event;

- (void)dismissPlatterWithAnimation:(BOOL)withAnimation;

- (void)invalidate;

- (BOOL)isVisible;

@end

#endif // HAVE(UIKIT_WITH_MOUSE_SUPPORT)
