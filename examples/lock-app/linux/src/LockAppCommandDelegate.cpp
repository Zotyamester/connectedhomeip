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

static Messaging::ExchangeManager * gXMgr;
static unsigned long long gTargetNodeId = 0;

void HandleDeviceConnected(void * context, Messaging::ExchangeManager & exchangeMgr, const SessionHandle & sessionHandle)
{
    ChipLogProgress(DataManagement, "Lock App: Connection established!");

    auto onSuccess = [](const ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
        ChipLogProgress(NotSpecified, "Lock App: Read attribute successful!");
    };
    auto onFailure = [](const ConcreteDataAttributePath * attributePath, CHIP_ERROR error) {
        ChipLogError(NotSpecified, "Lock App: Read attribute failed: %" CHIP_ERROR_FORMAT, error.Format());
    };

    [[maybe_unused]] auto x =
        Controller::ReadAttribute<Clusters::OnOff::Attributes::OnOff::TypeInfo>(gXMgr, sessionHandle, 0x01, onSuccess, onFailure);
    [[maybe_unused]] auto y = Controller::ReadAttribute<Clusters::LevelControl::Attributes::CurrentLevel::TypeInfo>(
        gXMgr, sessionHandle, 0x01, onSuccess, onFailure);
    [[maybe_unused]] auto z = Controller::ReadAttribute<Clusters::ColorControl::Attributes::CurrentHue::TypeInfo>(
        gXMgr, sessionHandle, 0x01, onSuccess, onFailure);
    [[maybe_unused]] auto w = Controller::ReadAttribute<Clusters::DoorLock::Attributes::LockState::TypeInfo>(
        gXMgr, sessionHandle, 0x01, onSuccess, onFailure);
    // ...
}

void HandleDeviceConnectionFailure(void * context, const ScopedNodeId & peeerId, CHIP_ERROR err)
{
    ChipLogError(NotSpecified, "Lock App: Connection failed: %" CHIP_ERROR_FORMAT, err.Format());
}

Callback::Callback<OnDeviceConnected> gOnConnectedCallback(HandleDeviceConnected, NULL);
Callback::Callback<OnDeviceConnectionFailure> gOnConnectionFailureCallback(HandleDeviceConnectionFailure, NULL);

void LockAppCommandDelegate::OnEventCommandReceived(const char * json)
{
    // Command format:
    // { "Cmd": "SetDoorState", "Params": { "EndpointId": 1, "DoorState": 2} }
    Json::Reader reader;
    Json::Value value;
    if (!reader.parse(json, value))
    {
        ChipLogError(NotSpecified, "Lock App: Error parsing JSON with error %s:", reader.getFormattedErrorMessages().c_str());
        return;
    }

    if (value.empty() || !value.isObject())
    {
        ChipLogError(NotSpecified, "Lock App: Invalid JSON command received");
        return;
    }

    if (!value.isMember("Cmd") || !value["Cmd"].isString())
    {
        ChipLogError(NotSpecified, "Lock App: Invalid JSON command received: command name is missing");
        return;
    }
    auto commandName = value["Cmd"].asString();

    Json::Value params = Json::objectValue;
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

    if (!params.isMember("NodeId"))
    {
        ChipLogError(NotSpecified, "Lock App: Invalid JSON command received: no Node ID is specified");
        return;
    }
    gTargetNodeId = strtoull(params["NodeId"].asString().c_str(), nullptr, 16);

    // Now we can try to execute a command
    if (commandName == "RunScan")
    {
        auto error = DeviceLayer::PlatformMgr().ScheduleWork([](intptr_t arg) {
            gXMgr = InteractionModelEngine::GetInstance()->GetExchangeManager();
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
