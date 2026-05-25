/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "LockAppCommandDelegate.h"
#include <lib/support/BytesToHex.h>
#include <platform/PlatformManager.h>

#include <LockManager.h>
#include <string>
#include <utility>

#include <app/EventManagement.h>
#include <app/InteractionModelEngine.h>
#include <app/server/Server.h>
#include <controller/InvokeInteraction.h>
#include <controller/ReadInteraction.h>
#include <messaging/ExchangeContext.h>
#include <messaging/ExchangeMgr.h>

using namespace ::chip;
using namespace ::chip::app;

// Global exchange manager used for initiating interactions
static Messaging::ExchangeManager * gExchangeManager;
// Target Node ID for the scan operation
static unsigned long long gTargetNodeId = 0;

/**
 * @brief Callback handler for when a device connection is successfully established.
 *
 * This function is triggered when a CASE session is established. It attempts
 * to read several attributes from the connected device.
 *
 * @param context Application-specific context (unused).
 * @param exchangeMgr The exchange manager used for the session.
 * @param sessionHandle The established session handle to the peer.
 */
void HandleDeviceConnected(void * context, Messaging::ExchangeManager & exchangeMgr, const SessionHandle & sessionHandle)
{
    ChipLogProgress(DataManagement, "Lock App: Connection established!");

    // Define success and failure callbacks for the read attribute interaction
    auto onSuccess = [](const ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
        ChipLogProgress(NotSpecified, "Lock App: Read attribute successful!");
    };
    auto onFailure = [](const ConcreteDataAttributePath * attributePath, CHIP_ERROR error) {
        ChipLogError(NotSpecified, "Lock App: Read attribute failed: %" CHIP_ERROR_FORMAT, error.Format());
    };

    // Attempt to read the OnOff attribute
    [[maybe_unused]] auto readOnOffStatus = Controller::ReadAttribute<Clusters::OnOff::Attributes::OnOff::TypeInfo>(
        gExchangeManager, sessionHandle, 0x01, onSuccess, onFailure);

    // Attempt to read the CurrentLevel attribute
    [[maybe_unused]] auto readCurrentLevelStatus =
        Controller::ReadAttribute<Clusters::LevelControl::Attributes::CurrentLevel::TypeInfo>(gExchangeManager, sessionHandle, 0x01,
                                                                                              onSuccess, onFailure);

    // Attempt to read the CurrentHue attribute
    [[maybe_unused]] auto readCurrentHueStatus =
        Controller::ReadAttribute<Clusters::ColorControl::Attributes::CurrentHue::TypeInfo>(gExchangeManager, sessionHandle, 0x01,
                                                                                            onSuccess, onFailure);

    // Attempt to read the LockState attribute
    [[maybe_unused]] auto readLockStateStatus = Controller::ReadAttribute<Clusters::DoorLock::Attributes::LockState::TypeInfo>(
        gExchangeManager, sessionHandle, 0x01, onSuccess, onFailure);
    // ...
}

/**
 * @brief Callback handler for when a device connection fails.
 *
 * @param context Application-specific context (unused).
 * @param peerId The Node ID of the peer we failed to connect to.
 * @param err The error that caused the connection to fail.
 */
void HandleDeviceConnectionFailure(void * context, const ScopedNodeId & peerId, CHIP_ERROR err)
{
    ChipLogError(NotSpecified, "Lock App: Connection failed: %" CHIP_ERROR_FORMAT, err.Format());
}

Callback::Callback<OnDeviceConnected> gOnConnectedCallback(HandleDeviceConnected, NULL);
Callback::Callback<OnDeviceConnectionFailure> gOnConnectionFailureCallback(HandleDeviceConnectionFailure, NULL);

/**
 * @brief Handles incoming event commands in JSON format.
 *
 * Parses the provided JSON string, extracts the command and its parameters,
 * and schedules the appropriate action (e.g., establishing a session to a target node).
 *
 * @param json The JSON formatted command string.
 */
void LockAppCommandDelegate::OnEventCommandReceived(const char * json)
{
    // Expected Command format:
    // { "Cmd": "RunScan", "Params": { "NodeId": "1234ABCD" } }
    Json::Reader reader;
    Json::Value value;

    // Parse the JSON string
    if (!reader.parse(json, value))
    {
        ChipLogError(NotSpecified, "Lock App: Error parsing JSON with error %s:", reader.getFormattedErrorMessages().c_str());
        return;
    }

    // Validate the root JSON object
    if (value.empty() || !value.isObject())
    {
        ChipLogError(NotSpecified, "Lock App: Invalid JSON command received");
        return;
    }

    // Extract and validate the command name
    if (!value.isMember("Cmd") || !value["Cmd"].isString())
    {
        ChipLogError(NotSpecified, "Lock App: Invalid JSON command received: command name is missing");
        return;
    }
    auto commandName = value["Cmd"].asString();

    Json::Value params = Json::objectValue;

    // Extract and validate the parameters
    if (!value.isMember("Params"))
    {
        ChipLogError(NotSpecified, "Lock App: Invalid JSON command received: no parameters are specified");
        return;
    }

    if (!value["Params"].isObject())
    {
        ChipLogError(NotSpecified, "Lock App: Invalid JSON command received: specified parameters are incorrect");
        return;
    }
    params = value["Params"];

    // Extract the Node ID from the parameters
    if (!params.isMember("NodeId"))
    {
        ChipLogError(NotSpecified, "Lock App: Invalid JSON command received: no Node ID is specified");
        return;
    }

    // Convert the Node ID from string to an unsigned long long
    gTargetNodeId = strtoull(params["NodeId"].asString().c_str(), nullptr, 16);

    // Now we can try to execute the command based on the command name
    if (commandName == "RunScan")
    {
        // Schedule the connection attempt on the Matter event loop
        auto error = DeviceLayer::PlatformMgr().ScheduleWork([](intptr_t arg) {
            gExchangeManager = InteractionModelEngine::GetInstance()->GetExchangeManager();
            Server::GetInstance().GetCASESessionManager()->FindOrEstablishSession(
                ScopedNodeId(gTargetNodeId, 0x1), &gOnConnectedCallback, &gOnConnectionFailureCallback);
        });

        if (error != CHIP_NO_ERROR)
        {
            ChipLogError(NotSpecified, "Lock App: Failed to schedule scanning: %" CHIP_ERROR_FORMAT, error.Format());
        }
    }
    else
    {
        ChipLogError(NotSpecified, "Lock App: Unable to execute command \"%s\": command not supported", commandName.c_str());
    }

    ChipLogProgress(NotSpecified, "Lock App: Exiting command handler");
}
