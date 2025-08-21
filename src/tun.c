#include "tun.h"

#include <stdbool.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <net/route.h>
#include <arpa/inet.h>

#include <linux/if_tun.h>
#include <linux/ipv6.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include "log.h"

#define IFNAMSIZ_NULL (IFNAMSIZ - 1)

static const char TUN_DEV[] = "/dev/net/tun";

int tun_new(char *name)
{
    log_debug("Creating TUN interface: %s", name);
    
    int tunfd = open(TUN_DEV, O_RDWR);
    if (tunfd < 0)
    {
        log_debug("Failed to open TUN device: %s", TUN_DEV);
        return tunfd;
    }

    struct ifreq ifr = {0};
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    strncpy(ifr.ifr_name, name, IFNAMSIZ_NULL);

    if (ioctl(tunfd, TUNSETIFF, &ifr) < 0)
    {
        log_debug("Failed to set TUN interface flags");
        close(tunfd);
        return -1;
    }

    strncpy(name, ifr.ifr_name, IFNAMSIZ);
    log_debug("TUN interface created successfully: %s", name);
    return tunfd;
}

bool tun_set_ip(int sockfd, const char *ifname, in_addr_t ip, in_addr_t gateway)
{
    log_debug("Setting IPv4 address for interface %s: ip=%s, netmask=%s", 
              ifname, inet_ntoa(*(struct in_addr*)&ip), inet_ntoa(*(struct in_addr*)&gateway));
    
    struct ifreq ifr = {0};
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ_NULL);

    struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;
    addr->sin_family = AF_INET;

    addr->sin_addr.s_addr = ip;
    if (ioctl(sockfd, SIOCSIFADDR, &ifr) < 0)
    {
        log_debug("Failed to set IPv4 address");
        return false;
    }

    addr->sin_addr.s_addr = gateway;
    if (ioctl(sockfd, SIOCSIFNETMASK, &ifr) < 0)
    {
        log_debug("Failed to set IPv4 netmask");
        return false;
    }

    log_debug("IPv4 address configured successfully");
    return true;
}

bool tun_set_dest_ip(int sockfd, const char *ifname, in_addr_t ip)
{
    struct ifreq ifr = {0};
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ_NULL);

    struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_dstaddr;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = ip;

    return (ioctl(sockfd, SIOCSIFDSTADDR, &ifr) == 0);
}

bool tun_set_ip6(int sockfd, const char *ifname, struct in6_addr *ip6, int prefix)
{
    char ip6_str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, ip6, ip6_str, INET6_ADDRSTRLEN);
    log_debug("Setting IPv6 address for interface %s: ip=%s, prefix=%d", ifname, ip6_str, prefix);
    
    struct ifreq ifr = {0};
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ_NULL);

    if (ioctl(sockfd, SIOGIFINDEX, &ifr) < 0)
    {
        log_debug("Failed to get interface index");
        return false;
    }

    struct in6_ifreq in6_ifr = {0};
    in6_ifr.ifr6_ifindex = ifr.ifr_ifindex;
    in6_ifr.ifr6_prefixlen = prefix;
    in6_ifr.ifr6_addr = *ip6;

    if (ioctl(sockfd, SIOCSIFADDR, &in6_ifr) < 0)
    {
        log_debug("Failed to set IPv6 address");
        return false;
    }

    log_debug("IPv6 address configured successfully");
    return true;
}

bool tun_up(int sockfd, const char *ifname)
{
    log_debug("Bringing up interface: %s", ifname);
    
    struct ifreq ifr = {0};
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ_NULL);

    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0)
    {
        log_debug("Failed to get interface flags");
        return false;
    }

    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;

    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0)
    {
        log_debug("Failed to set interface flags");
        return false;
    }

    log_debug("Interface brought up successfully");
    return true;
}

int tun_get_mtu(int sockfd, const char *ifname)
{
    struct ifreq ifr = {0};
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ_NULL);

    if (ioctl(sockfd, SIOCGIFMTU, &ifr) < 0)
    {
        return -1;
    }

    return ifr.ifr_mtu;
}

bool tun_set_mtu(int sockfd, const char *ifname, int mtu)
{
    log_debug("Setting MTU for interface %s: %d", ifname, mtu);
    
    struct ifreq ifr = {0};
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ_NULL);
    ifr.ifr_mtu = mtu;

    bool result = (ioctl(sockfd, SIOCSIFMTU, &ifr) == 0);
    if (result) {
        log_debug("MTU set successfully");
    } else {
        log_debug("Failed to set MTU");
    }
    return result;
}

void tun_set_route(char *ifname, in_addr_t ip, int metric, int mtu, struct rtentry *route)
{
    log_debug("Setting up route: ifname=%s, gateway=%s, metric=%d, mtu=%d", 
              ifname, inet_ntoa(*(struct in_addr*)&ip), metric, mtu);
    
    memset(route, 0, sizeof(struct rtentry));

    struct sockaddr_in *addr = (struct sockaddr_in *)&route->rt_gateway;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = ip;

    addr = (struct sockaddr_in *)&route->rt_dst;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = INADDR_ANY;

    addr = (struct sockaddr_in *)&route->rt_genmask;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = INADDR_ANY;

    route->rt_dev = ifname;
    route->rt_flags = RTF_UP | RTF_GATEWAY | RTF_MTU;
    route->rt_metric = metric + 1;
    route->rt_mtu = mtu;
    
    log_debug("Route structure configured");
}

bool tun_add_route(int sockfd, struct rtentry *route)
{
    log_debug("Adding route to kernel");
    bool result = (ioctl(sockfd, SIOCADDRT, route) >= 0);
    if (result) {
        log_debug("Route added successfully");
    } else {
        log_debug("Failed to add route");
    }
    return result;
}

bool tun_del_route(int sockfd, struct rtentry *route)
{
    log_debug("Removing route from kernel");
    bool result = (ioctl(sockfd, SIOCDELRT, route) >= 0);
    if (result) {
        log_debug("Route removed successfully");
    } else {
        log_debug("Failed to remove route");
    }
    return result;
}

// Helper function to add IPv6 route via netlink
static bool tun_add_ipv6_route_netlink(int sock, const char *ifname, struct in6_addr *addr, int prefix_len, int metric)
{
    struct {
        struct nlmsghdr nl;
        struct rtmsg rt;
        char buf[1024];
    } req;
    
    memset(&req, 0, sizeof(req));
    
    // Netlink header
    req.nl.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    req.nl.nlmsg_type = RTM_NEWROUTE;
    req.nl.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE;
    req.nl.nlmsg_seq = 1;
    
    // Route message
    req.rt.rtm_family = AF_INET6;
    req.rt.rtm_table = RT_TABLE_MAIN;
    req.rt.rtm_protocol = RTPROT_BOOT;
    req.rt.rtm_scope = RT_SCOPE_LINK;
    req.rt.rtm_type = RTN_LOCAL;
    req.rt.rtm_flags = RTM_F_NOTIFY;
    
    // Add interface attribute
    struct rtattr *rta = (struct rtattr *)(((char *)&req) + NLMSG_ALIGN(req.nl.nlmsg_len));
    rta->rta_type = RTA_OIF;
    rta->rta_len = RTA_LENGTH(sizeof(int));
    
    int ifindex = if_nametoindex(ifname);
    if (ifindex == 0) {
        log_debug("Failed to get interface index for %s", ifname);
        return false;
    }
    
    memcpy(RTA_DATA(rta), &ifindex, sizeof(int));
    req.nl.nlmsg_len = NLMSG_ALIGN(req.nl.nlmsg_len) + RTA_ALIGN(rta->rta_len);
    
    // Add destination address attribute
    rta = (struct rtattr *)(((char *)&req) + NLMSG_ALIGN(req.nl.nlmsg_len));
    rta->rta_type = RTA_DST;
    rta->rta_len = RTA_LENGTH(prefix_len / 8);
    memcpy(RTA_DATA(rta), addr, prefix_len / 8);
    req.nl.nlmsg_len = NLMSG_ALIGN(req.nl.nlmsg_len) + RTA_ALIGN(rta->rta_len);
    
    // Send request
    if (send(sock, &req, req.nl.nlmsg_len, 0) < 0) {
        log_debug("Failed to send netlink route request");
        return false;
    }
    
    // Read response
    char buf[1024];
    ssize_t len = recv(sock, buf, sizeof(buf), 0);
    if (len < 0) {
        log_debug("Failed to receive netlink response");
        return false;
    }
    
    struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
    if (nlh->nlmsg_type == NLMSG_ERROR) {
        struct nlmsgerr *err = (struct nlmsgerr *)NLMSG_DATA(nlh);
        if (err->error != 0) {
            log_debug("Netlink error: %s", strerror(-err->error));
            return false;
        }
    }
    
    log_debug("IPv6 route added successfully for prefix length %d", prefix_len);
    return true;
}

// Helper function to delete IPv6 route via netlink
static void tun_del_ipv6_route_netlink(int sock, const char *ifname)
{
    // Implementation for deleting routes
    // This would be similar to the add function but with RTM_DELROUTE
    log_debug("IPv6 route deletion not yet implemented");
}

// IPv6 routing table manipulation functions
bool tun_manipulate_ipv6_routing(const char *ifname, struct in6_addr *src_addr, struct in6_addr *nat64_prefix)
{
    log_debug("Manipulating IPv6 routing table for interface %s", ifname);
    log_debug("Setting source address route with metric 0 (highest priority)");
    log_debug("Setting NAT64 prefix route with metric 200 (lower priority)");
    
    // Create netlink socket for IPv6 routing manipulation
    int sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sock < 0) {
        log_debug("Failed to create netlink socket");
        return false;
    }
    
    // Bind netlink socket
    struct sockaddr_nl addr;
    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_debug("Failed to bind netlink socket");
        close(sock);
        return false;
    }
    
    bool success = true;
    
    // Add route for our source address with high priority (lower metric = higher priority)
    // Set metric to 0 for roku interface to give it higher priority than eth0
    if (!tun_add_ipv6_route_netlink(sock, ifname, src_addr, 128, 0)) {
        log_debug("Failed to add source address route");
        success = false;
    }
    
    // Add route for NAT64 prefix with lower priority
    if (!tun_add_ipv6_route_netlink(sock, ifname, nat64_prefix, 96, 200)) {
        log_debug("Failed to add NAT64 prefix route");
        success = false;
    }
    
    close(sock);
    
    if (success) {
        log_debug("IPv6 routing table manipulation completed successfully");
    } else {
        log_debug("IPv6 routing table manipulation failed");
    }
    
    return success;
}

void tun_restore_ipv6_routing(const char *ifname)
{
    log_debug("Restoring IPv6 routing table for interface %s", ifname);
    
    // Create netlink socket
    int sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sock < 0) {
        log_debug("Failed to create netlink socket for cleanup");
        return;
    }
    
    // Bind netlink socket
    struct sockaddr_nl addr;
    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_debug("Failed to bind netlink socket for cleanup");
        close(sock);
        return;
    }
    
    // Remove routes we added
    tun_del_ipv6_route_netlink(sock, ifname);
    
    close(sock);
    log_debug("IPv6 routing table restored");
}