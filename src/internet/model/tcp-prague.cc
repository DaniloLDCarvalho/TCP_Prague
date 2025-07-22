/*
 * Copyright (c) 2017 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Shravya K.S. <shravya.ks0@gmail.com>
 *
 */

#include "tcp-prague.h"

#include "tcp-socket-state.h"

#include "ns3/abort.h"
#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TcpPrague");

NS_OBJECT_ENSURE_REGISTERED(TcpPrague);

TypeId
TcpPrague::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TcpPrague")
            .SetParent<TcpLinuxReno>()
            .AddConstructor<TcpPrague>()
            .SetGroupName("Internet")
            .AddAttribute("PragueShiftG",
                          "Parameter G for updating Prague_alpha",
                          DoubleValue(0.0625),
                          MakeDoubleAccessor(&TcpPrague::m_g),
                          MakeDoubleChecker<double>(0, 1))
            .AddAttribute("PragueAlphaOnInit",
                          "Initial alpha value",
                          DoubleValue(1.0),
                          MakeDoubleAccessor(&TcpPrague::InitializePragueAlpha),
                          MakeDoubleChecker<double>(0, 1))
            .AddAttribute("UseEct0",
                          "Set to true to use ECT(0) for L4S ECN. If false (default), uses ECT(1)",
                          BooleanValue(false),
                          MakeBooleanAccessor(&TcpPrague::m_useEct0),
                          MakeBooleanChecker())
            .AddTraceSource("CongestionEstimate",
                            "Update sender-side congestion estimate state",
                            MakeTraceSourceAccessor(&TcpPrague::m_traceCongestionEstimate),
                            "ns3::TcpPrague::CongestionEstimateTracedCallback");
    return tid;
}

std::string
TcpPrague::GetName() const
{
    return "TcpPrague";
}

TcpPrague::TcpPrague()
    : TcpLinuxReno(),
      m_ackedBytesEcn(0),
      m_ackedBytesTotal(0),
      m_priorRcvNxt(SequenceNumber32(0)),
      m_priorRcvNxtFlag(false),
      m_nextSeq(SequenceNumber32(0)),
      m_nextSeqFlag(false),
      m_ceState(false),
      m_delayedAckReserved(false),
      m_initialized(false),
      m_baseRtt(Time(0)),
      m_inClassicFallback(false)
{
    NS_LOG_FUNCTION(this);
}

TcpPrague::TcpPrague(const TcpPrague& sock)
    : TcpLinuxReno(sock),
      m_ackedBytesEcn(sock.m_ackedBytesEcn),
      m_ackedBytesTotal(sock.m_ackedBytesTotal),
      m_priorRcvNxt(sock.m_priorRcvNxt),
      m_priorRcvNxtFlag(sock.m_priorRcvNxtFlag),
      m_alpha(sock.m_alpha),
      m_nextSeq(sock.m_nextSeq),
      m_nextSeqFlag(sock.m_nextSeqFlag),
      m_ceState(sock.m_ceState),
      m_delayedAckReserved(sock.m_delayedAckReserved),
      m_g(sock.m_g),
      m_useEct0(sock.m_useEct0),
      m_initialized(sock.m_initialized)
{
    NS_LOG_FUNCTION(this);
}

TcpPrague::~TcpPrague()
{
    NS_LOG_FUNCTION(this);
}

Ptr<TcpCongestionOps>
TcpPrague::Fork()
{
    NS_LOG_FUNCTION(this);
    return CopyObject<TcpPrague>(this);
}

void
TcpPrague::Init(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);
    NS_LOG_INFO(this << "Enabling PragueEcn for Prague");
    tcb->m_useEcn = TcpSocketState::On;
    tcb->m_ecnMode = TcpSocketState::DctcpEcn;
    tcb->m_ectCodePoint = m_useEct0 ? TcpSocketState::Ect0 : TcpSocketState::Ect1;
    SetSuppressIncreaseIfCwndLimited(false);
    m_initialized = true;
}

// Step 9, Section 3.3 of RFC 8257.  GetSsThresh() is called upon
// entering the CWR state, and then later, when CWR is exited,
// cwnd is set to ssthresh (this value).  bytesInFlight is ignored.
// Em src/internet/model/tcp-prague.cc

uint32_t
TcpPrague::GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
    NS_LOG_FUNCTION(this << tcb << bytesInFlight);
    // Comportamento do Reno
    return std::max(tcb->m_segmentSize * 2, bytesInFlight / 2);
}

void
TcpPrague::PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked << rtt);

    if (tcb->m_congState == TcpSocketState::CA_RECOVERY || tcb->m_congState == TcpSocketState::CA_LOSS)
    {
        return; 
    }

    // Lógica da Feature 4: Detetar aumento de RTT (mantém-se)
    if (!rtt.IsZero())
    {
        if (m_baseRtt.IsZero() || rtt < m_baseRtt) m_baseRtt = rtt;
        if (tcb->m_srtt > (m_baseRtt + MilliSeconds(3)))
        {
            if (!m_inClassicFallback)
            {
                NS_LOG_INFO("Entering Classic Fallback Mode. BaseRTT: " << m_baseRtt << ", SRTT: " << tcb->m_srtt);
                m_inClassicFallback = true;
            }
        }
        else
        {
            if (m_inClassicFallback)
            {
                NS_LOG_INFO("Exiting Classic Fallback Mode. BaseRTT: " << m_baseRtt << ", SRTT: " << tcb->m_srtt);
                m_inClassicFallback = false;
            }
        }
    }

    // =======================================================
    // LÓGICA DA FEATURE 2 (Emissor): Ler e usar o valor do AccECN
    // =======================================================
    
    // 1. Ler o valor da "caixa de correio" em TcpSocketState
    uint32_t ceBytes = tcb->m_aceCeBytes;
    if (ceBytes > 0)
    {
        NS_LOG_INFO("AccECN: Processing ACE value from TCB. CE bytes = " << ceBytes);
    }

    // 2. Usar o valor preciso para calcular a fração de congestionamento
    double bytesEcnFraction = 0.0;
    if (tcb->m_lastAckedSackedBytes > 0)
    {
        bytesEcnFraction = static_cast<double>(ceBytes) / tcb->m_lastAckedSackedBytes;
    }
    
    // 3. Atualizar o alpha com a fração precisa
    m_alpha = (1.0 - m_g) * m_alpha + m_g * bytesEcnFraction;
    NS_LOG_INFO("AccECN: Updated alpha = " << m_alpha);

    // 4. Resetar o valor na "caixa de correio" para não ser usado novamente
    tcb->m_aceCeBytes = 0;

    // Aumento da janela
    if (tcb->m_congState == TcpSocketState::CA_OPEN)
    {
        double adder = static_cast<double>(tcb->m_segmentSize * tcb->m_segmentSize) / tcb->m_cWnd.Get();
        adder = std::max(1.0, adder);
        tcb->m_cWnd = tcb->m_cWnd.Get() + static_cast<uint32_t>(adder);
    }
}

void
TcpPrague::InitializePragueAlpha(double alpha)
{
    NS_LOG_FUNCTION(this << alpha);
    NS_ABORT_MSG_IF(m_initialized, "Prague has already been initialized");
    m_alpha = alpha;
}

void
TcpPrague::Reset(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);
    m_nextSeq = tcb->m_nextTxSequence;
    m_ackedBytesEcn = 0;
    m_ackedBytesTotal = 0;
}

void
TcpPrague::CeState0to1(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);
    if (!m_ceState && m_delayedAckReserved && m_priorRcvNxtFlag)
    {
        SequenceNumber32 tmpRcvNxt;
        /* Save current NextRxSequence. */
        tmpRcvNxt = tcb->m_rxBuffer->NextRxSequence();

        /* Generate previous ACK without ECE */
        tcb->m_rxBuffer->SetNextRxSequence(m_priorRcvNxt);
        tcb->m_sendEmptyPacketCallback(TcpHeader::ACK);

        /* Recover current RcvNxt. */
        tcb->m_rxBuffer->SetNextRxSequence(tmpRcvNxt);
    }

    if (!m_priorRcvNxtFlag)
    {
        m_priorRcvNxtFlag = true;
    }
    m_priorRcvNxt = tcb->m_rxBuffer->NextRxSequence();
    m_ceState = true;
    tcb->m_ecnState = TcpSocketState::ECN_CE_RCVD;
}

void
TcpPrague::CeState1to0(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);
    if (m_ceState && m_delayedAckReserved && m_priorRcvNxtFlag)
    {
        SequenceNumber32 tmpRcvNxt;
        /* Save current NextRxSequence. */
        tmpRcvNxt = tcb->m_rxBuffer->NextRxSequence();

        /* Generate previous ACK with ECE */
        tcb->m_rxBuffer->SetNextRxSequence(m_priorRcvNxt);
        tcb->m_sendEmptyPacketCallback(TcpHeader::ACK | TcpHeader::ECE);

        /* Recover current RcvNxt. */
        tcb->m_rxBuffer->SetNextRxSequence(tmpRcvNxt);
    }

    if (!m_priorRcvNxtFlag)
    {
        m_priorRcvNxtFlag = true;
    }
    m_priorRcvNxt = tcb->m_rxBuffer->NextRxSequence();
    m_ceState = false;

    if (tcb->m_ecnState.Get() == TcpSocketState::ECN_CE_RCVD ||
        tcb->m_ecnState.Get() == TcpSocketState::ECN_SENDING_ECE)
    {
        tcb->m_ecnState = TcpSocketState::ECN_IDLE;
    }
}

void
TcpPrague::UpdateAckReserved(Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCAEvent_t event)
{
    NS_LOG_FUNCTION(this << tcb << event);
    switch (event)
    {
    case TcpSocketState::CA_EVENT_DELAYED_ACK:
        if (!m_delayedAckReserved)
        {
            m_delayedAckReserved = true;
        }
        break;
    case TcpSocketState::CA_EVENT_NON_DELAYED_ACK:
        if (m_delayedAckReserved)
        {
            m_delayedAckReserved = false;
        }
        break;
    default:
        /* Don't care for the rest. */
        break;
    }
}

void
TcpPrague::CwndEvent(Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCAEvent_t event)
{
    NS_LOG_FUNCTION(this << tcb << event);

    switch (event)
    {
    // CASO DE PERDA (3 ACKs duplicados)
    case TcpSocketState::CA_EVENT_LOSS:
    {
        if (tcb->m_congState == TcpSocketState::CA_CWR)
        {
            //Redução mais suave (2+alpha)/4
            double reductionFactor = (2.0 + m_alpha) / 4.0;
            tcb->m_ssThresh = std::max(static_cast<uint32_t>(tcb->m_segmentSize * 2), (uint32_t)(tcb->m_cWnd.Get() * reductionFactor));
            NS_LOG_INFO("Smart Fallback on Loss (CWR state): alpha=" << m_alpha << ", reduction=" << reductionFactor);
        }
        else
        {
            //Fallback para o comportamento do Reno (metade)
            tcb->m_ssThresh = std::max(static_cast<uint32_t>(tcb->m_segmentSize * 2), tcb->m_cWnd.Get() / 2);
            NS_LOG_INFO("Simple Fallback on Loss: ssthresh=" << tcb->m_ssThresh.Get());
        }
        // Em ambos os casos, a cwnd é reduzida para o novo ssthresh.
        tcb->m_cWnd = tcb->m_ssThresh.Get();
        break;
    }

    // CASOS DE ECN
    case TcpSocketState::CA_EVENT_ECN_IS_CE:
    {
        if (m_inClassicFallback)
        {
            // MODO FALLBACK: Reage à marcação ECN de forma agressiva, como se fosse uma perda.
            NS_LOG_INFO("Classic Fallback: Applying harsh reduction due to ECN mark.");
            // Reutilizamos a mesma lógica da Feature 3 para consistência.
            tcb->m_ssThresh = std::max(static_cast<uint32_t>(tcb->m_segmentSize * 2), tcb->m_cWnd.Get() / 2);
            tcb->m_cWnd = tcb->m_ssThresh.Get();
        }
        else
        {
            // MODO NORMAL: Reage à marcação ECN de forma suave.
            CeState0to1(tcb);
        }
        break; 
    }

    case TcpSocketState::CA_EVENT_ECN_NO_CE:
        CeState1to0(tcb);
        break;
    
    // Outros casos
    case TcpSocketState::CA_EVENT_DELAYED_ACK:
    case TcpSocketState::CA_EVENT_NON_DELAYED_ACK:
        UpdateAckReserved(tcb, event);
        break;
    default:
        break;
    }
}

} // namespace ns3
