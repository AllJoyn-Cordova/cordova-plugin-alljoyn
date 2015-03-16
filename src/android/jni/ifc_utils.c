/*
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. 
 * You may obtain a copy of the License at 
 *
 *     http://www.apache.org/licenses/LICENSE-2.0 
 *
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 * See the License for the specific language governing permissions and 
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/route.h>
#include <linux/wireless.h>

#include <stdio.h>
#include <string.h>

static int ifc_ctl_sock = -1;

static const char *ipaddr_to_string(uint32_t addr)
{
    struct in_addr in_addr;

    in_addr.s_addr = addr;
    return inet_ntoa(in_addr);
}

int ifc_init(void)
{
    if (ifc_ctl_sock == -1) {
        ifc_ctl_sock = socket(AF_INET, SOCK_DGRAM, 0);    
        if (ifc_ctl_sock < 0) {
            printf("socket() failed: %s\n", strerror(errno));
        }
    }
    return ifc_ctl_sock < 0 ? -1 : 0;
}

void ifc_close(void)
{
    if (ifc_ctl_sock != -1) {
        (void)close(ifc_ctl_sock);
        ifc_ctl_sock = -1;
    }
}

static void ifc_init_ifr(const char *name, struct ifreq *ifr)
{
    memset(ifr, 0, sizeof(struct ifreq));
    strncpy(ifr->ifr_name, name, IFNAMSIZ);
    ifr->ifr_name[IFNAMSIZ - 1] = 0;
}

int ifc_get_hwaddr(const char *name, void *ptr)
{
    int r;
    struct ifreq ifr;
    ifc_init_ifr(name, &ifr);

    r = ioctl(ifc_ctl_sock, SIOCGIFHWADDR, &ifr);
    if(r < 0) return -1;

    memcpy(ptr, &ifr.ifr_hwaddr.sa_data, 6);
    return 0;    
}

int ifc_get_ifindex(const char *name, int *if_indexp)
{
    int r;
    struct ifreq ifr;
    ifc_init_ifr(name, &ifr);

    r = ioctl(ifc_ctl_sock, SIOCGIFINDEX, &ifr);
    if(r < 0) return -1;

    *if_indexp = ifr.ifr_ifindex;
    return 0;    
}

static int ifc_set_flags(const char *name, unsigned set, unsigned clr)
{
    struct ifreq ifr;
    ifc_init_ifr(name, &ifr);

    if(ioctl(ifc_ctl_sock, SIOCGIFFLAGS, &ifr) < 0) return -1;
    ifr.ifr_flags = (ifr.ifr_flags & (~clr)) | set;
    return ioctl(ifc_ctl_sock, SIOCSIFFLAGS, &ifr);
}

int ifc_up(const char *name)
{
    return ifc_set_flags(name, IFF_UP, 0);
}

int ifc_down(const char *name)
{
    return ifc_set_flags(name, 0, IFF_UP);
}

static void init_sockaddr_in(struct sockaddr *sa, in_addr_t addr)
{
    struct sockaddr_in *sin = (struct sockaddr_in *) sa;
    sin->sin_family = AF_INET;
    sin->sin_port = 0;
    sin->sin_addr.s_addr = addr;
}

int ifc_set_addr(const char *name, in_addr_t addr)
{
    struct ifreq ifr;

    ifc_init_ifr(name, &ifr);
    init_sockaddr_in(&ifr.ifr_addr, addr);
    
    return ioctl(ifc_ctl_sock, SIOCSIFADDR, &ifr);
}

int ifc_set_mask(const char *name, in_addr_t mask)
{
    struct ifreq ifr;

    ifc_init_ifr(name, &ifr);
    init_sockaddr_in(&ifr.ifr_addr, mask);
    
    return ioctl(ifc_ctl_sock, SIOCSIFNETMASK, &ifr);
}

int ifc_get_info(const char *name, in_addr_t *addr, in_addr_t *mask, unsigned *flags)
{
    struct ifreq ifr;
    ifc_init_ifr(name, &ifr);

    if (addr != NULL) {
        if(ioctl(ifc_ctl_sock, SIOCGIFADDR, &ifr) < 0) {
            *addr = 0;
        } else {
            *addr = ((struct sockaddr_in*) &ifr.ifr_addr)->sin_addr.s_addr;
        }
    }
    
    if (mask != NULL) {
        if(ioctl(ifc_ctl_sock, SIOCGIFNETMASK, &ifr) < 0) {
            *mask = 0;
        } else {
            *mask = ((struct sockaddr_in*) &ifr.ifr_addr)->sin_addr.s_addr;
        }
    }

    if (flags != NULL) {
        if(ioctl(ifc_ctl_sock, SIOCGIFFLAGS, &ifr) < 0) {
            *flags = 0;
        } else {
            *flags = ifr.ifr_flags;
        }
    }

    return 0;
}


in_addr_t prefixLengthToIpv4Netmask(int prefix_length)
{
    in_addr_t mask = 0;
    // C99 (6.5.7): shifts of 32 bits have undefined results
    if (prefix_length <= 0 || prefix_length > 32) {
        return 0;
    }
    mask = ~mask << (32 - prefix_length);
    mask = htonl(mask);
    return mask;
}