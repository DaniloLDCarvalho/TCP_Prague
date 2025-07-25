/*
 * Copyright (c) 2008,2009 IITP RAS
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Kirill Andreev <andreev@iitp.ru>
 */

#include "hwmp-rtable.h"

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/object.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("HwmpRtable");

namespace dot11s
{

NS_OBJECT_ENSURE_REGISTERED(HwmpRtable);

TypeId
HwmpRtable::GetTypeId()
{
    static TypeId tid = TypeId("ns3::dot11s::HwmpRtable")
                            .SetParent<Object>()
                            .SetGroupName("Mesh")
                            .AddConstructor<HwmpRtable>();
    return tid;
}

HwmpRtable::HwmpRtable()
{
    DeleteProactivePath();
}

HwmpRtable::~HwmpRtable()
{
}

void
HwmpRtable::DoDispose()
{
    m_routes.clear();
}

void
HwmpRtable::AddReactivePath(Mac48Address destination,
                            Mac48Address retransmitter,
                            uint32_t interface,
                            uint32_t metric,
                            Time lifetime,
                            uint32_t seqnum)
{
    NS_LOG_FUNCTION(this << destination << retransmitter << interface << metric
                         << lifetime.GetSeconds() << seqnum);
    auto& route = m_routes[destination]; // find existing record or create new
    route.retransmitter = retransmitter;
    route.interface = interface;
    route.metric = metric;
    route.whenExpire = Simulator::Now() + lifetime;
    route.seqnum = seqnum;
}

void
HwmpRtable::AddProactivePath(uint32_t metric,
                             Mac48Address root,
                             Mac48Address retransmitter,
                             uint32_t interface,
                             Time lifetime,
                             uint32_t seqnum)
{
    NS_LOG_FUNCTION(this << metric << root << retransmitter << interface << lifetime << seqnum);
    m_root.root = root;
    m_root.retransmitter = retransmitter;
    m_root.metric = metric;
    m_root.whenExpire = Simulator::Now() + lifetime;
    m_root.seqnum = seqnum;
    m_root.interface = interface;
}

void
HwmpRtable::AddPrecursor(Mac48Address destination,
                         uint32_t precursorInterface,
                         Mac48Address precursorAddress,
                         Time lifetime)
{
    NS_LOG_FUNCTION(this << destination << precursorInterface << precursorAddress << lifetime);
    Precursor precursor;
    precursor.interface = precursorInterface;
    precursor.address = precursorAddress;
    precursor.whenExpire = Simulator::Now() + lifetime;
    auto i = m_routes.find(destination);
    if (i != m_routes.end())
    {
        bool should_add = true;
        for (unsigned int j = 0; j < i->second.precursors.size(); j++)
        {
            // NB: Only one active route may exist, so do not check
            // interface ID, just address
            if (i->second.precursors[j].address == precursorAddress)
            {
                should_add = false;
                i->second.precursors[j].whenExpire = precursor.whenExpire;
                break;
            }
        }
        if (should_add)
        {
            i->second.precursors.push_back(precursor);
        }
    }
}

void
HwmpRtable::DeleteProactivePath()
{
    NS_LOG_FUNCTION(this);
    m_root.precursors.clear();
    m_root.interface = INTERFACE_ANY;
    m_root.metric = MAX_METRIC;
    m_root.retransmitter = Mac48Address::GetBroadcast();
    m_root.seqnum = 0;
    m_root.whenExpire = Simulator::Now();
}

void
HwmpRtable::DeleteProactivePath(Mac48Address root)
{
    NS_LOG_FUNCTION(this << root);
    if (m_root.root == root)
    {
        DeleteProactivePath();
    }
}

void
HwmpRtable::DeleteReactivePath(Mac48Address destination)
{
    NS_LOG_FUNCTION(this << destination);
    auto i = m_routes.find(destination);
    if (i != m_routes.end())
    {
        m_routes.erase(i);
    }
}

HwmpRtable::LookupResult
HwmpRtable::LookupReactive(Mac48Address destination)
{
    NS_LOG_FUNCTION(this << destination);
    auto i = m_routes.find(destination);
    if (i == m_routes.end())
    {
        return LookupResult();
    }
    if ((i->second.whenExpire < Simulator::Now()) && (!i->second.whenExpire.IsZero()))
    {
        NS_LOG_DEBUG("Reactive route has expired, sorry.");
        return LookupResult();
    }
    return LookupReactiveExpired(destination);
}

HwmpRtable::LookupResult
HwmpRtable::LookupReactiveExpired(Mac48Address destination)
{
    NS_LOG_FUNCTION(this << destination);
    auto i = m_routes.find(destination);
    if (i == m_routes.end())
    {
        return LookupResult();
    }
    NS_LOG_DEBUG("Returning reactive route to " << destination);
    return LookupResult(i->second.retransmitter,
                        i->second.interface,
                        i->second.metric,
                        i->second.seqnum,
                        i->second.whenExpire - Simulator::Now());
}

HwmpRtable::LookupResult
HwmpRtable::LookupProactive()
{
    NS_LOG_FUNCTION(this);
    if (m_root.whenExpire < Simulator::Now())
    {
        NS_LOG_DEBUG("Proactive route has expired and will be deleted, sorry.");
        DeleteProactivePath();
    }
    return LookupProactiveExpired();
}

HwmpRtable::LookupResult
HwmpRtable::LookupProactiveExpired()
{
    NS_LOG_FUNCTION(this);
    NS_LOG_DEBUG("Returning proactive route to root");
    return LookupResult(m_root.retransmitter,
                        m_root.interface,
                        m_root.metric,
                        m_root.seqnum,
                        m_root.whenExpire - Simulator::Now());
}

std::vector<HwmpProtocol::FailedDestination>
HwmpRtable::GetUnreachableDestinations(Mac48Address peerAddress)
{
    NS_LOG_FUNCTION(this << peerAddress);
    HwmpProtocol::FailedDestination dst;
    std::vector<HwmpProtocol::FailedDestination> retval;
    for (auto i = m_routes.begin(); i != m_routes.end(); i++)
    {
        if (i->second.retransmitter == peerAddress)
        {
            dst.destination = i->first;
            i->second.seqnum++;
            dst.seqnum = i->second.seqnum;
            retval.push_back(dst);
        }
    }
    // Lookup a path to root
    if (m_root.retransmitter == peerAddress)
    {
        dst.destination = m_root.root;
        dst.seqnum = m_root.seqnum;
        retval.push_back(dst);
    }
    return retval;
}

HwmpRtable::PrecursorList
HwmpRtable::GetPrecursors(Mac48Address destination)
{
    NS_LOG_FUNCTION(this << destination);
    // We suppose that no duplicates here can be
    PrecursorList retval;
    auto route = m_routes.find(destination);
    if (route != m_routes.end())
    {
        for (auto i = route->second.precursors.begin(); i != route->second.precursors.end(); i++)
        {
            if (i->whenExpire > Simulator::Now())
            {
                retval.emplace_back(i->interface, i->address);
            }
        }
    }
    return retval;
}

bool
HwmpRtable::LookupResult::operator==(const HwmpRtable::LookupResult& o) const
{
    return (retransmitter == o.retransmitter && ifIndex == o.ifIndex && metric == o.metric &&
            seqnum == o.seqnum);
}

HwmpRtable::LookupResult::LookupResult(Mac48Address r, uint32_t i, uint32_t m, uint32_t s, Time l)
    : retransmitter(r),
      ifIndex(i),
      metric(m),
      seqnum(s),
      lifetime(l)
{
}

bool
HwmpRtable::LookupResult::IsValid() const
{
    return !(retransmitter == Mac48Address::GetBroadcast() && ifIndex == INTERFACE_ANY &&
             metric == MAX_METRIC && seqnum == 0);
}
} // namespace dot11s
} // namespace ns3
