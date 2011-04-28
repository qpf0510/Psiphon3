/*
 * Copyright (c) 2011, Psiphon Inc.
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include "vpnmanager.h"
#include "webbrowser.h"
#include <algorithm>

VPNManager::VPNManager(void) :
    m_vpnState(VPN_STATE_STOPPED),
    m_userSignalledStop(false)
{
}

VPNManager::~VPNManager(void)
{
    Stop();
}

void VPNManager::Toggle()
{
    switch (m_vpnState)
    {
    case VPN_STATE_STOPPED:
        // The user clicked the button to start the VPN.
        // Clear this flag so we can do retries on failed connections.
        m_userSignalledStop = false;
        TryNextServer();
        break;

    default:
        // The user requested to stop the VPN by clicking the button.
        //
        // If a connection was in the INITIALIZING state, this flag
        // tells TryNextServer not to Establish, or to Stop if
        // Establish was already called.
        // NOTE that Stop is called here in case TryNextServer has
        // already returned (and the first callback notification has
        // not yet been received).
        //
        // If a connection was in the STARTING state, we will get a
        // "Connection Failed" notification.
        // This flag indicates that we should not retry when a failed
        // connection is signalled.
        m_userSignalledStop = true;
        Stop();
        break;
    }
}

void VPNManager::Stop(void)
{
    if (m_vpnConnection.Remove())
    {
        VPNStateChanged(VPN_STATE_STOPPED);
    }
}

void VPNManager::VPNStateChanged(VPNState newState)
{
    m_vpnState = newState;

    switch (m_vpnState)
    {
    case VPN_STATE_CONNECTED:
        OpenBrowser();
        break;

    case VPN_STATE_FAILED:
        // Either the user cancelled an in-progress connection,
        // or a connection actually failed.
        // Either way, we need to set the status to STOPPED,
        // so that another Toggle() will cause the VPN to start again.
        m_vpnState = VPN_STATE_STOPPED;

        if (!m_userSignalledStop)
        {
            // Connecting to the current server failed.
            m_vpnList.MarkCurrentServerFailed();
            TryNextServer();
        }
        break;

    default:
        // no default actions
        break;
    }
}

void VPNManager::TryNextServer(void)
{
    VPNStateChanged(VPN_STATE_INITIALIZING);

    // Try the next server in our list.
    ServerEntry serverEntry = m_vpnList.GetNextServer();

#ifdef _UNICODE
    wstring serverAddress(serverEntry.serverAddress.length(), L' ');
    std::copy(serverEntry.serverAddress.begin(), serverEntry.serverAddress.end(), serverAddress.begin());
#else
    string serverAddress = serverEntry.serverAddress;
#endif
    // TODO: do web request to get the PSK
    // TODO: if the web request is synchronous, do not continuously retry because
    //       that will prevent us from processing another toggle click.
    // NOTE: Toggle may have been clicked since the start of this function.
    //       If it was, don't Establish the VPN connection.
    if (!m_userSignalledStop)
    {
        m_vpnConnection.Establish(serverAddress, _T("1q2w3e4r!"));
    }

    // NOTE: Toggle may have been clicked during Establish.
    //       If it was, Stop the VPN.
    if (m_userSignalledStop)
    {
        Stop();
    }
}
