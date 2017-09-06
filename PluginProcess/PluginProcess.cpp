/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "PluginProcess.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "ArgumentCoders.h"
#include "Attachment.h"
#include "NetscapePlugin.h"
#include "NetscapePluginModule.h"
#include "PluginProcessConnectionMessages.h"
#include "PluginProcessCreationParameters.h"
#include "PluginProcessProxyMessages.h"
#include "WebProcessConnection.h"
#include <WebCore/MemoryPressureHandler.h>
#include <WebCore/NotImplemented.h>
#include <wtf/RunLoop.h>

#if PLATFORM(MAC)
#include <crt_externs.h>
#endif

using namespace WebCore;

namespace WebKit {

PluginProcess& PluginProcess::singleton()
{
    static NeverDestroyed<PluginProcess> pluginProcess;
    return pluginProcess;
}

PluginProcess::PluginProcess()
    : m_supportsAsynchronousPluginInitialization(false)
    , m_minimumLifetimeTimer(RunLoop::main(), this, &PluginProcess::minimumLifetimeTimerFired)
    , m_connectionActivity("PluginProcess connection activity.")
{
    NetscapePlugin::setSetExceptionFunction(WebProcessConnection::setGlobalException);
}

PluginProcess::~PluginProcess()
{
}

void PluginProcess::initializeProcess(const ChildProcessInitializationParameters& parameters)
{
    m_pluginPath = parameters.extraInitializationData.get("plugin-path");
    platformInitializeProcess(parameters);
}

void PluginProcess::removeWebProcessConnection(WebProcessConnection* webProcessConnection)
{
    size_t vectorIndex = m_webProcessConnections.find(webProcessConnection);
    ASSERT(vectorIndex != notFound);

    m_webProcessConnections.remove(vectorIndex);
    
    if (m_webProcessConnections.isEmpty() && m_pluginModule) {
        // Decrement the load count. This is balanced by a call to incrementLoadCount in createWebProcessConnection.
        m_pluginModule->decrementLoadCount();
    }        

    enableTermination();
}

NetscapePluginModule* PluginProcess::netscapePluginModule()
{
    if (!m_pluginModule) {
        ASSERT(!m_pluginPath.isNull());
        m_pluginModule = NetscapePluginModule::getOrCreate(m_pluginPath);

#if PLATFORM(MAC)
        if (m_pluginModule) {
            if (m_pluginModule->pluginQuirks().contains(PluginQuirks::PrognameShouldBeWebKitPluginHost))
                *const_cast<const char**>(_NSGetProgname()) = "WebKitPluginHost";
        }
#endif
    }

    return m_pluginModule.get();
}

bool PluginProcess::shouldTerminate()
{
    return m_webProcessConnections.isEmpty();
}

void PluginProcess::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    didReceivePluginProcessMessage(connection, decoder);
}

void PluginProcess::didClose(IPC::Connection&)
{
    // The UI process has crashed, just quit.
    // FIXME: If the plug-in is spinning in the main loop, we'll never get this message.
    stopRunLoop();
}

void PluginProcess::initializePluginProcess(PluginProcessCreationParameters&& parameters)
{
    ASSERT(!m_pluginModule);

    auto& memoryPressureHandler = MemoryPressureHandler::singleton();
#if OS(LINUX)
    if (parameters.memoryPressureMonitorHandle.fileDescriptor() != -1)
        memoryPressureHandler.setMemoryPressureMonitorHandle(parameters.memoryPressureMonitorHandle.releaseFileDescriptor());
#endif
    memoryPressureHandler.setLowMemoryHandler([this] (Critical, Synchronous) {
        if (shouldTerminate())
            terminate();
    });
    memoryPressureHandler.install();

    m_supportsAsynchronousPluginInitialization = parameters.supportsAsynchronousPluginInitialization;
    setMinimumLifetime(parameters.minimumLifetime);
    setTerminationTimeout(parameters.terminationTimeout);

    platformInitializePluginProcess(WTFMove(parameters));
}

void PluginProcess::createWebProcessConnection()
{
    bool didHaveAnyWebProcessConnections = !m_webProcessConnections.isEmpty();

#if USE(UNIX_DOMAIN_SOCKETS)
    IPC::Connection::SocketPair socketPair = IPC::Connection::createPlatformConnection();

    auto connection = WebProcessConnection::create(socketPair.server);
    m_webProcessConnections.append(WTFMove(connection));

    IPC::Attachment clientSocket(socketPair.client);
    parentProcessConnection()->send(Messages::PluginProcessProxy::DidCreateWebProcessConnection(clientSocket, m_supportsAsynchronousPluginInitialization), 0);
#elif OS(DARWIN)
    // Create the listening port.
    mach_port_t listeningPort;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);

    // Create a listening connection.
    auto connection = WebProcessConnection::create(IPC::Connection::Identifier(listeningPort));

    m_webProcessConnections.append(WTFMove(connection));

    IPC::Attachment clientPort(listeningPort, MACH_MSG_TYPE_MAKE_SEND);
    parentProcessConnection()->send(Messages::PluginProcessProxy::DidCreateWebProcessConnection(clientPort, m_supportsAsynchronousPluginInitialization), 0);
#else
    notImplemented();
#endif

    if (NetscapePluginModule* module = netscapePluginModule()) {
        if (!didHaveAnyWebProcessConnections) {
            // Increment the load count. This is matched by a call to decrementLoadCount in removeWebProcessConnection.
            // We do this so that the plug-in module's NP_Shutdown won't be called until right before exiting.
            module->incrementLoadCount();
        }
    }

    disableTermination();
}

void PluginProcess::getSitesWithData(uint64_t callbackID)
{
    Vector<String> sites;
    if (NetscapePluginModule* module = netscapePluginModule())
        sites = module->sitesWithData();

    parentProcessConnection()->send(Messages::PluginProcessProxy::DidGetSitesWithData(sites, callbackID), 0);
}

void PluginProcess::deleteWebsiteData(std::chrono::system_clock::time_point modifiedSince, uint64_t callbackID)
{
    if (auto* module = netscapePluginModule()) {
        auto currentTime = std::chrono::system_clock::now();

        if (currentTime > modifiedSince) {
            uint64_t maximumAge = std::chrono::duration_cast<std::chrono::seconds>(currentTime - modifiedSince).count();

            module->clearSiteData(String(), NP_CLEAR_ALL, maximumAge);
        }
    }

    parentProcessConnection()->send(Messages::PluginProcessProxy::DidDeleteWebsiteData(callbackID), 0);
}

void PluginProcess::deleteWebsiteDataForHostNames(const Vector<String>& hostNames, uint64_t callbackID)
{
    if (auto* module = netscapePluginModule()) {
        for (auto& hostName : hostNames)
            module->clearSiteData(hostName, NP_CLEAR_ALL, std::numeric_limits<uint64_t>::max());
    }

    parentProcessConnection()->send(Messages::PluginProcessProxy::DidDeleteWebsiteDataForHostNames(callbackID), 0);
}

void PluginProcess::setMinimumLifetime(double lifetime)
{
    if (lifetime <= 0.0)
        return;
    
    disableTermination();
    
    m_minimumLifetimeTimer.startOneShot(lifetime);
}

void PluginProcess::minimumLifetimeTimerFired()
{
    enableTermination();
}

#if !PLATFORM(COCOA)
void PluginProcess::initializeProcessName(const ChildProcessInitializationParameters&)
{
}

void PluginProcess::initializeSandbox(const ChildProcessInitializationParameters&, SandboxInitializationParameters&)
{
}
#endif

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)

