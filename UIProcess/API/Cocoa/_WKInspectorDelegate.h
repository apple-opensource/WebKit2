/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import <WebKit/WKFoundation.h>

@class _WKInspector;

@protocol _WKInspectorDelegate <NSObject>
@optional

/*! @abstract Called when the Browser domain is enabled for the associated _WKInspector.
    @param inspector the associated _WKInspector for which the Browser domain has been enabled.
 */
- (void)inspectorDidEnableBrowserDomain:(_WKInspector *)inspector;

/*! @abstract Called when the  Browser domain is disabled for the associated _WKInspector.
    @param inspector the associated _WKInspector for which the Browser domain has been disabled.
 */
- (void)inspectorDidDisableBrowserDomain:(_WKInspector *)inspector;

/*! @abstract Called when the _WKInspector requests to show a resource externally. This
    is used to display documentation pages and to show external URLs that are linkified.
    @param inspector the associated inspector for which an external navigation should be triggered.
    @param url The resource to be shown.
 */
- (void)inspector:(_WKInspector *)inspector openURLExternally:(NSURL *)url;

@end