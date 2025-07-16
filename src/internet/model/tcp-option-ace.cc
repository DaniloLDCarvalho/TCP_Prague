/*
 * Este ficheiro implementa a opção TCP para Accurate ECN (AccECN).
 */

#include "tcp-option-ace.h"
#include "ns3/log.h"
#include "ns3/assert.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TcpOptionAce");
NS_OBJECT_ENSURE_REGISTERED(TcpOptionAce);

TypeId
TcpOptionAce::GetTypeId()
{
    static TypeId tid = TypeId("ns3::TcpOptionAce")
                            .SetParent<TcpOption>()
                            .SetGroupName("Internet")
                            .AddConstructor<TcpOptionAce>();
    return tid;
}

TcpOptionAce::TcpOptionAce()
    : TcpOption(),
      m_ceBytes(0)
{
}

TcpOptionAce::~TcpOptionAce()
{
}

void
TcpOptionAce::SetCeBytes(uint32_t ceBytes)
{
    m_ceBytes = ceBytes;
}

uint32_t
TcpOptionAce::GetCeBytes() const
{
    return m_ceBytes;
}

uint8_t
TcpOptionAce::GetKind() const
{
    // Usamos o valor experimental 15 para a nossa opção ACE
    return 15;
}

uint32_t
TcpOptionAce::GetSerializedSize() const
{
    // Kind (1 byte) + Length (1 byte) + Value (4 bytes) = 6 bytes
    return 6;
}

void
TcpOptionAce::Serialize(Buffer::Iterator start) const
{
    start.WriteU8(GetKind());
    start.WriteU8(GetSerializedSize());
    start.WriteHtonU32(m_ceBytes);
}

uint32_t
TcpOptionAce::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    NS_ASSERT(i.ReadU8() == GetKind());
    uint8_t len = i.ReadU8();
    NS_ASSERT(len == GetSerializedSize());
    m_ceBytes = i.ReadNtohU32();
    return len;
}

void
TcpOptionAce::Print(std::ostream& os) const
{
    os << "ACE (CE-Bytes=" << m_ceBytes << ")";
}

} // namespace ns3
