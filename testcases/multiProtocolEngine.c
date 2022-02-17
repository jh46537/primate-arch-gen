#include <stdint.h>
#define ETHERNET (uint8_t) 0x80
#define IPV4  (uint8_t) 0x40
#define CONTROL_PLANE 255
#define INVALID_ADDRESS 255

typedef struct {
  uint8_t version;
  uint16_t length;
  uint8_t ttl;
  uint16_t chksum;
  uint32_t srcAddr;
  uint32_t dstAddr;
} IPv4Header_t;

typedef struct {
  uint8_t l3Type;
} EthernetHeader_t;

typedef struct {
 uint8_t l2Protocol;
 uint8_t outPort;
 EthernetHeader_t  eth;
 IPv4Header_t  ipv4;
} NP_EthIPv4Header_t;

#pragma primate blue match 1 1
void Output(NP_EthIPv4Header_t *output);

#pragma primate blue match 1 1
void ipv4Lookup1(uint32_t *addr, uint8_t *port);

#pragma primate blue match 1 1
void ipv4Lookup2(uint32_t *addr, uint8_t *port);

#pragma primate blue match 1 1
void qosCount(uint8_t *port, uint8_t *qcOutput);

void multiProtocolEngine(NP_EthIPv4Header_t input) {
  if (input.l2Protocol == ETHERNET && input.eth.l3Type == IPV4 
    && input.ipv4.length >= 20 && input.ipv4.version == 4) {
    uint8_t outPort, srcLookupResult;
    ipv4Lookup1(&input.ipv4.dstAddr, &outPort);
    ipv4Lookup2(&input.ipv4.srcAddr, &srcLookupResult);
    if (srcLookupResult != INVALID_ADDRESS && outPort != INVALID_ADDRESS) {
      input.outPort = outPort;
      int qcOutput;
      qosCount(&outPort, &qcOutput);
      if (input.ipv4.ttl > 1) {
        input.ipv4.ttl --;
        input.ipv4.chksum += 0x80;
        Output(&input);
      } else {
        input.outPort = CONTROL_PLANE;
        Output(&input);
      }
    } else {
      input.outPort = CONTROL_PLANE;
      Output(&input);
    }
  } else {
    input.outPort = CONTROL_PLANE;
    Output(&input);
  }
}
