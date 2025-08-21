#ifndef ROKU_ADDR_H
#define ROKU_ADDR_H

#include <stdbool.h>
#include <netinet/in.h>

// Check if an IPv6 address matches our source address (exact match for /128)
#define ADDR_MATCH_SRC(a, b) (memcmp(&a, &b, sizeof(struct in6_addr)) == 0)
// Check if an IPv6 address matches our destination prefix (for NAT64)
#define ADDR_MATCH_PREFIX(a, b) (a.s6_addr32[0] == b.s6_addr32[0] && a.s6_addr32[1] == b.s6_addr32[1])
// Validate that the source address is a full /128 address
#define ADDR_VALID_ADDR(a) (1) // Always valid for /128
// Validate that the destination prefix is a valid /96 prefix
#define ADDR_VALID_PREFIX(a) (a.s6_addr32[3] == 0)

bool addr_6to4(struct in6_addr *ip6, in_addr_t *ip, bool pseudo);
void addr_4to6(in_addr_t ip, struct in6_addr *ip6, struct in6_addr *src_addr);
bool addr_validate(in_addr_t ip);

#endif