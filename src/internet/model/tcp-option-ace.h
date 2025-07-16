/*
 * Este ficheiro define a opção TCP para Accurate ECN (AccECN).
 */

#ifndef TCP_OPTION_ACE_H
#define TCP_OPTION_ACE_H

#include "tcp-option.h"

namespace ns3
{

/**
 * @ingroup tcp
 * @brief A Opção TCP para Accurate ECN (AccECN), usada para transportar
 * a contagem de bytes que sofreram Congestion Experience (CE).
 */
class TcpOptionAce : public TcpOption
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    TcpOptionAce();
    ~TcpOptionAce() override;

    /**
     * @brief Define a contagem de bytes marcados com CE.
     * @param ceBytes O número de bytes a ser enviado na opção.
     */
    void SetCeBytes(uint32_t ceBytes);

    /**
     * @brief Obtém a contagem de bytes marcados com CE.
     * @return O número de bytes lido da opção.
     */
    uint32_t GetCeBytes() const;

    // Funções virtuais herdadas da classe base TcpOption
    void Print(std::ostream& os) const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    uint8_t GetKind() const override;
    uint32_t GetSerializedSize() const override;

  private:
    uint32_t m_ceBytes; //!< Contagem de bytes marcados com CE.
};

} // namespace ns3

#endif /* TCP_OPTION_ACE_H */
