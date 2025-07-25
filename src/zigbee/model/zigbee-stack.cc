/*
 * Copyright (c) 2024 Tokushima University, Japan
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Alberto Gallegos Ramonet <alramonet@is.tokushima-u.ac.jp>
 */

#include "zigbee-stack.h"

#include "ns3/channel.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"

using namespace ns3::lrwpan;

namespace ns3
{
namespace zigbee
{
NS_LOG_COMPONENT_DEFINE("ZigbeeStack");
NS_OBJECT_ENSURE_REGISTERED(ZigbeeStack);

TypeId
ZigbeeStack::GetTypeId()
{
    static TypeId tid = TypeId("ns3::zigbee::ZigbeeStack")
                            .SetParent<Object>()
                            .SetGroupName("Zigbee")
                            .AddConstructor<ZigbeeStack>();
    return tid;
}

ZigbeeStack::ZigbeeStack()
{
    NS_LOG_FUNCTION(this);

    m_nwk = CreateObject<zigbee::ZigbeeNwk>();
    m_aps = CreateObject<zigbee::ZigbeeAps>();

    m_nwkOnly = false;
}

ZigbeeStack::~ZigbeeStack()
{
    NS_LOG_FUNCTION(this);
}

void
ZigbeeStack::DoDispose()
{
    NS_LOG_FUNCTION(this);

    m_netDevice = nullptr;
    m_node = nullptr;
    m_aps = nullptr;
    m_nwk = nullptr;
    m_mac = nullptr;
    Object::DoDispose();
}

void
ZigbeeStack::DoInitialize()
{
    NS_LOG_FUNCTION(this);

    // AggregateObject(m_aps);

    NS_ABORT_MSG_UNLESS(m_netDevice,
                        "Invalid NetDevice found when attempting to install ZigbeeStack");

    // Make sure the NetDevice is previously initialized
    // before using ZigbeeStack (PHY and MAC are initialized)
    m_netDevice->Initialize();

    m_mac = m_netDevice->GetObject<lrwpan::LrWpanMacBase>();
    NS_ABORT_MSG_UNLESS(m_mac,
                        "Invalid LrWpanMacBase found in this NetDevice, cannot use ZigbeeStack");

    m_nwk->Initialize();
    AggregateObject(m_nwk);

    // Set NWK callback hooks with the MAC
    m_nwk->SetMac(m_mac);
    m_mac->SetMcpsDataIndicationCallback(MakeCallback(&ZigbeeNwk::McpsDataIndication, m_nwk));
    m_mac->SetMlmeOrphanIndicationCallback(MakeCallback(&ZigbeeNwk::MlmeOrphanIndication, m_nwk));
    m_mac->SetMlmeCommStatusIndicationCallback(
        MakeCallback(&ZigbeeNwk::MlmeCommStatusIndication, m_nwk));
    m_mac->SetMlmeBeaconNotifyIndicationCallback(
        MakeCallback(&ZigbeeNwk::MlmeBeaconNotifyIndication, m_nwk));
    m_mac->SetMlmeAssociateIndicationCallback(
        MakeCallback(&ZigbeeNwk::MlmeAssociateIndication, m_nwk));
    m_mac->SetMcpsDataConfirmCallback(MakeCallback(&ZigbeeNwk::McpsDataConfirm, m_nwk));
    m_mac->SetMlmeScanConfirmCallback(MakeCallback(&ZigbeeNwk::MlmeScanConfirm, m_nwk));
    m_mac->SetMlmeStartConfirmCallback(MakeCallback(&ZigbeeNwk::MlmeStartConfirm, m_nwk));
    m_mac->SetMlmeSetConfirmCallback(MakeCallback(&ZigbeeNwk::MlmeSetConfirm, m_nwk));
    m_mac->SetMlmeGetConfirmCallback(MakeCallback(&ZigbeeNwk::MlmeGetConfirm, m_nwk));
    m_mac->SetMlmeAssociateConfirmCallback(MakeCallback(&ZigbeeNwk::MlmeAssociateConfirm, m_nwk));
    // TODO: complete other callback hooks with the MAC

    if (!m_nwkOnly)
    {
        // Set APS callback hooks with NWK (i.e., NLDE primitives only)
        m_nwk->SetNldeDataConfirmCallback(MakeCallback(&ZigbeeAps::NldeDataConfirm, m_aps));
        m_nwk->SetNldeDataIndicationCallback(MakeCallback(&ZigbeeAps::NldeDataIndication, m_aps));

        m_aps->Initialize();
        m_aps->SetNwk(m_nwk);
        AggregateObject(m_aps);
    }

    // Obtain Extended address as soon as NWK is set to begin operations
    m_mac->MlmeGetRequest(MacPibAttributeIdentifier::macExtendedAddress);

    Object::DoInitialize();
}

Ptr<Channel>
ZigbeeStack::GetChannel() const
{
    return m_netDevice->GetChannel();
}

Ptr<Node>
ZigbeeStack::GetNode() const
{
    return m_node;
}

Ptr<NetDevice>
ZigbeeStack::GetNetDevice() const
{
    return m_netDevice;
}

void
ZigbeeStack::SetNetDevice(Ptr<NetDevice> netDevice)
{
    NS_LOG_FUNCTION(this << netDevice);
    m_netDevice = netDevice;
    m_node = m_netDevice->GetNode();
}

void
ZigbeeStack::SetOnlyNwkLayer()
{
    m_nwkOnly = true;
}

Ptr<zigbee::ZigbeeNwk>
ZigbeeStack::GetNwk() const
{
    return m_nwk;
}

void
ZigbeeStack::SetNwk(Ptr<zigbee::ZigbeeNwk> nwk)
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_IF(ZigbeeStack::IsInitialized(), "NWK layer cannot be set after initialization");
    m_nwk = nwk;
}

Ptr<zigbee::ZigbeeAps>
ZigbeeStack::GetAps() const
{
    return m_aps;
}

void
ZigbeeStack::SetAps(Ptr<zigbee::ZigbeeAps> aps)
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_IF(ZigbeeStack::IsInitialized(), "APS layer cannot be set after initialization");
    m_aps = aps;
}

} // namespace zigbee
} // namespace ns3
