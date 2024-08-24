/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/***
 * Copyright (c) 2017 Alexander Afanasyev
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation, either version
 * 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "simple-router.hpp"
#include "core/utils.hpp"

#include <fstream>

namespace simple_router {

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// IMPLEMENT THIS METHOD
void
SimpleRouter::handlePacket(const Buffer& packet, const std::string& inIface)
{
  std::cerr << "Got packet of size " << packet.size() << " on interface " << inIface << std::endl;

  const Interface* iface = findIfaceByName(inIface);
  if (iface == nullptr) {
    std::cerr << "Received packet, but interface is unknown, ignoring" << std::endl;
    return;
  }

  // FILL THIS IN
  // Get ethernet header
  struct ethernet_hdr* ethHdr = (struct ethernet_hdr*)packet.data();

  // Check that destination MAC is interface MAC
  std::string str_dhost = macToString(ethHdr->ether_dhost);
  if (str_dhost.compare(macToString(iface->addr)) != 0){
    std::cerr << "Invalid destination: " << str_dhost << std::endl;
    return;
  }
  
  // Check that type is IP
  if (ethHdr->ether_type != ntohs(0x0800)){
    std::cerr << "Invalid payload type" << std::endl;
    std::cerr << ethHdr->ether_type;
    return;
  }

  // Get IP header
  struct ip_hdr* ipHdr = (struct ip_hdr*)(packet.data() + sizeof(struct ethernet_hdr));


  //print_hdrs(packet);
  // Check checksum
  uint16_t sum = ipHdr->ip_sum;
  ipHdr->ip_sum = 0;
  uint16_t comp_sum = cksum(ipHdr, sizeof(struct ip_hdr));
  if (sum != comp_sum){
    std::cerr << "Invalid checksum" << std::endl;
    std::cerr << "Checksum: " << sum << std::endl;
    std::cerr << "Computed: " << comp_sum << std::endl;
    return;
  }

  // Decrement TTL
  ipHdr->ip_ttl -= 1;
  if (ipHdr->ip_ttl == 0){
    std::cerr << "TTL is 0" << std::endl;
    return;
  }

  // Compute new checksum
  ipHdr->ip_sum = cksum(ipHdr, sizeof(struct ip_hdr));

  // Check if destined for local interface
  std::cerr << "IP destination: " << ipToString(ipHdr->ip_dst) << std::endl;
  const Interface* destIface = findIfaceByIp(ipHdr->ip_dst);
  if (destIface == nullptr){
    //perform next hop lookup and send packet
    std::cerr << "Destined for other interface" << std::endl;
    // Get routing table entry
    RoutingTableEntry match = m_routingTable.lookup(ipHdr->ip_dst);
    std::cerr << "IP: " << ipToString(ipHdr->ip_dst) << std::endl;
    std::cerr << "Match: " << ipToString(match.gw) << std::endl;
    // Set IP dst
    ipHdr->ip_dst = match.gw;

    // redo cksum
    ipHdr->ip_sum = 0;
    ipHdr->ip_sum = cksum(ipHdr, sizeof(struct ip_hdr));

    std::shared_ptr<ArpEntry> arp_entry = m_arp.lookup(ipHdr->ip_dst);
    //Check if cache entry exists
    if (arp_entry != nullptr){
      // Set ethernet src as match.ifName mac
      Buffer src = findIfaceByName(match.ifName)->addr;
      std::copy(src.begin(), src.end(), ethHdr->ether_shost);
      // Set ethernet dest as cached_entry mac
      std::copy(arp_entry->mac.begin(), arp_entry->mac.end(), ethHdr->ether_dhost);
      // send packet
      std::cerr << "Sending packet to next hop from " << match.ifName << std::endl;
      //print_hdrs(packet);
      sendPacket(packet, match.ifName);
    } else {
      // Set ethernet src as match.ifName mac
      Buffer src = findIfaceByName(match.ifName)->addr;
      std::copy(src.begin(), src.end(), ethHdr->ether_shost);
      // Queue packet for arp request
      std::cerr << "Adding queue request for ip " << ipToString(ipHdr->ip_dst) << " and interface " << match.ifName << std::endl;
      m_arp.queueRequest(ipHdr->ip_dst, packet, match.ifName);
    }
  } else {
    // Return if protocol is not icmp
    if(ipHdr->ip_p != ip_protocol_icmp){
      std::cerr << "Protocol is not ICMP" << std::endl;
      return;
    }
    // Get icmp hdr
    struct icmp_hdr* icmpHdr = (struct icmp_hdr*)(packet.data() + sizeof(struct ethernet_hdr) + sizeof(struct ip_hdr));

    // If ICMP type field is not echo request, return
    if(icmpHdr->icmp_type != 8){
      std::cerr << "ICMP code was not echo request" << std::endl;
      return;
    }


    //Check checksum
    uint16_t icmp_sum = icmpHdr->icmp_sum;
    icmpHdr->icmp_sum = 0;
    uint16_t comp_icmp_sum = cksum(icmpHdr, packet.size() - sizeof(struct ethernet_hdr) - sizeof(struct ip_hdr));
    if(icmp_sum != comp_icmp_sum){
      std::cerr << "Invalid icmp checksum" << std::endl;
      std::cerr << "Checksum: " << icmp_sum << std::endl;
      std::cerr << "Computed: " << comp_icmp_sum << std::endl;
      return;
    }

    // Change code field to echo reply
    icmpHdr->icmp_type = 0;

    // Update checksum
    icmpHdr->icmp_sum = cksum(icmpHdr, packet.size() - sizeof(struct ethernet_hdr) - sizeof(struct ip_hdr));

    // Swap IP src and dst
    uint32_t temp_addr = ipHdr->ip_src;
    ipHdr->ip_src = ipHdr->ip_dst;
    ipHdr->ip_dst = temp_addr;

    // Set TTL to 64
    ipHdr->ip_ttl = 64;
    // Update IP cksum
    ipHdr->ip_sum = 0;
    ipHdr->ip_sum = cksum(ipHdr, sizeof(struct ip_hdr));

    // Swap eth host and dest
    uint8_t temp_host[ETHER_ADDR_LEN];
    std::copy(ethHdr->ether_dhost, ethHdr->ether_dhost + ETHER_ADDR_LEN, temp_host);
    std::copy(ethHdr->ether_shost, ethHdr->ether_shost + ETHER_ADDR_LEN, ethHdr->ether_dhost);
    std::copy(temp_host, temp_host + ETHER_ADDR_LEN, ethHdr->ether_shost);

    // Send packet
    sendPacket(packet, inIface);
    std::cerr << "Sending echo response packet from " << inIface << std::endl;
    //print_hdrs(packet);
  }
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// You should not need to touch the rest of this code.
SimpleRouter::SimpleRouter()
  : m_arp(*this)
{
}

void
SimpleRouter::sendPacket(const Buffer& packet, const std::string& outIface)
{
  m_pox->begin_sendPacket(packet, outIface);
}

bool
SimpleRouter::loadRoutingTable(const std::string& rtConfig)
{
  return m_routingTable.load(rtConfig);
}

void
SimpleRouter::loadIfconfig(const std::string& ifconfig)
{
  std::ifstream iff(ifconfig.c_str());
  std::string line;
  while (std::getline(iff, line)) {
    std::istringstream ifLine(line);
    std::string iface, ip;
    ifLine >> iface >> ip;

    in_addr ip_addr;
    if (inet_aton(ip.c_str(), &ip_addr) == 0) {
      throw std::runtime_error("Invalid IP address `" + ip + "` for interface `" + iface + "`");
    }

    m_ifNameToIpMap[iface] = ip_addr.s_addr;
  }
}

void
SimpleRouter::printIfaces(std::ostream& os)
{
  if (m_ifaces.empty()) {
    os << " Interface list empty " << std::endl;
    return;
  }

  for (const auto& iface : m_ifaces) {
    os << iface << "\n";
  }
  os.flush();
}

const Interface*
SimpleRouter::findIfaceByIp(uint32_t ip) const
{
  auto iface = std::find_if(m_ifaces.begin(), m_ifaces.end(), [ip] (const Interface& iface) {
      return iface.ip == ip;
    });

  if (iface == m_ifaces.end()) {
    return nullptr;
  }

  return &*iface;
}

const Interface*
SimpleRouter::findIfaceByMac(const Buffer& mac) const
{
  auto iface = std::find_if(m_ifaces.begin(), m_ifaces.end(), [mac] (const Interface& iface) {
      return iface.addr == mac;
    });

  if (iface == m_ifaces.end()) {
    return nullptr;
  }

  return &*iface;
}

void
SimpleRouter::reset(const pox::Ifaces& ports)
{
  std::cerr << "Resetting SimpleRouter with " << ports.size() << " ports" << std::endl;

  m_arp.clear();
  m_ifaces.clear();

  for (const auto& iface : ports) {
    auto ip = m_ifNameToIpMap.find(iface.name);
    if (ip == m_ifNameToIpMap.end()) {
      std::cerr << "IP_CONFIG missing information about interface `" + iface.name + "`. Skipping it" << std::endl;
      continue;
    }

    m_ifaces.insert(Interface(iface.name, iface.mac, ip->second));
  }

  printIfaces(std::cerr);
}

const Interface*
SimpleRouter::findIfaceByName(const std::string& name) const
{
  auto iface = std::find_if(m_ifaces.begin(), m_ifaces.end(), [name] (const Interface& iface) {
      return iface.name == name;
    });

  if (iface == m_ifaces.end()) {
    return nullptr;
  }

  return &*iface;
}


} // namespace simple_router {
