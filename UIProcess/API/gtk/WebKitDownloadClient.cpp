/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitDownloadClient.h"

#include "APIDownloadClient.h"
#include "WebKitDownloadPrivate.h"
#include "WebKitURIResponsePrivate.h"
#include "WebKitWebContextPrivate.h"
#include "WebProcessPool.h"
#include <WebKit/WKString.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

using namespace WebCore;
using namespace WebKit;

class DownloadClient final : public API::DownloadClient {
public:
    explicit DownloadClient(WebKitWebContext* webContext)
        : m_webContext(webContext)
    {
    }

private:
    void didStart(WebProcessPool*, DownloadProxy* downloadProxy) override
    {
        GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(downloadProxy);
        webkitWebContextDownloadStarted(m_webContext, download.get());
    }

    void didReceiveResponse(WebProcessPool*, DownloadProxy* downloadProxy, const ResourceResponse& resourceResponse) override
    {
        GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(downloadProxy);
        if (webkitDownloadIsCancelled(download.get()))
            return;

        GRefPtr<WebKitURIResponse> response = adoptGRef(webkitURIResponseCreateForResourceResponse(resourceResponse));
        webkitDownloadSetResponse(download.get(), response.get());
    }

    void didReceiveData(WebProcessPool*, DownloadProxy* downloadProxy, uint64_t length) override
    {
        GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(downloadProxy);
        webkitDownloadNotifyProgress(download.get(), length);
    }

    String decideDestinationWithSuggestedFilename(WebProcessPool*, DownloadProxy* downloadProxy, const String& filename, bool& allowOverwrite) override
    {
        GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(downloadProxy);
        return String::fromUTF8(webkitDownloadDecideDestinationWithSuggestedFilename(download.get(), filename.utf8(), allowOverwrite));
    }

    void didCreateDestination(WebProcessPool*, DownloadProxy* downloadProxy, const String& path) override
    {
        GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(downloadProxy);
        webkitDownloadDestinationCreated(download.get(), path.utf8());
    }

    void didFail(WebProcessPool*, DownloadProxy* downloadProxy, const ResourceError& error) override
    {
        GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(downloadProxy);
        if (webkitDownloadIsCancelled(download.get())) {
            // Cancellation takes precedence over other errors.
            webkitDownloadCancelled(download.get());
        } else
            webkitDownloadFailed(download.get(), error);
        webkitWebContextRemoveDownload(downloadProxy);
    }

    void didCancel(WebProcessPool*, DownloadProxy* downloadProxy) override
    {
        GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(downloadProxy);
        webkitDownloadCancelled(download.get());
        webkitWebContextRemoveDownload(downloadProxy);
    }

    void didFinish(WebProcessPool*, DownloadProxy* downloadProxy) override
    {
        GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(downloadProxy);
        webkitDownloadFinished(download.get());
        webkitWebContextRemoveDownload(downloadProxy);
    }

    WebKitWebContext* m_webContext;
};

void attachDownloadClientToContext(WebKitWebContext* webContext)
{
    auto* processPool = webkitWebContextGetProcessPool(webContext);
    processPool->setDownloadClient(std::make_unique<DownloadClient>(webContext));
}
