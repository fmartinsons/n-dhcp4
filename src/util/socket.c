/*
 * Socket Utilities
 */

#include <assert.h>
#include <errno.h>
#include <net/if.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include "util/socket.h"

/**
 * socket_SIOCGIFNAME() - resolve an ifindex to an ifname
 * @socket:                     socket to operate on
 * @ifindex:                    index of network interface to resolve
 * @ifname:                     buffer to store resolved name
 *
 * This uses the SIOCGIFNAME ioctl to resolve an ifindex to an ifname. The
 * buffer provided in @ifnamep must be at least IFNAMSIZ bytes in size. The
 * maximum ifname length is IFNAMSIZ-1, and this function always
 * zero-terminates the result.
 *
 * This function is similar to if_indextoname(3) provided by glibc, but it
 * allows to specify the target socket explicitly. This allows the caller to
 * control the target network-namespace, rather than relying on the network
 * namespace of the running process.
 *
 * Return: 0 on success, negative kernel error code on failure.
 */
int socket_SIOCGIFNAME(int socket, int ifindex, char (*ifnamep)[IFNAMSIZ]) {
        struct ifreq req = { .ifr_ifindex = ifindex };
        int r;

        r = ioctl(socket, SIOCGIFNAME, &req);
        if (r < 0)
                return -errno;

        /*
         * The linux kernel guarantees that an interface name is always
         * zero-terminated, and it always fully fits into IFNAMSIZ bytes,
         * including the zero-terminator.
         */
        memcpy(ifnamep, req.ifr_name, IFNAMSIZ);
        return 0;
}

/**
 * socket_bind_if() - bind socket to a network interface
 * @socket:                     socket to operate on
 * @ifindex:                    index of network interface to bind to, or 0
 *
 * This binds the socket given via @socket to the network interface specified
 * via @ifindex. It uses the underlying SO_BINDTODEVICE ioctl of the linux
 * kernel. However, if available, if prefers the newer SO_BINDTOIF ioctl, which
 * avoids resolving the interface name temporarily, and thus does not suffer
 * from a race-condition.
 *
 * Return: 0 on success, negative error code on failure.
 */
int socket_bind_if(int socket, int ifindex) {
        char ifname[IFNAMSIZ] = {};
        int r;

        assert(ifindex >= 0);

        /*
         * We first try the newer SO_BINDTOIF. If it is not available on the
         * running kernel, we fall back to SO_BINDTODEVICE. This, however,
         * requires us to first resolve the ifindex to an ifname. Note that
         * this is racy, since the device name might theoretically change
         * asynchronously.
         *
         * Using 0 as ifindex will remove the device-binding. For SO_BINDTOIF
         * we simply pass-through the 0 to the kernel, which recognizes this
         * correctly. For SO_BINDTODEVICE we pass the empty string, which the
         * kernel recognizes as a request to remove the binding.
         */

#if 0 /* SO_BINDTOIF is not yet upstream, so disable for now */
#ifdef SO_BINDTOIF
        r = setsockopt(socket,
                       SOL_SOCKET,
                       SO_BINDTOIF,
                       &ifindex,
                       sizeof(ifindex));
        if (r >= 0)
                return 0;
        else if (errno != ENOPROTOOPT)
                return -errno;
#endif /* SO_BINDTOIF */
#endif

        if (ifindex > 0) {
                r = socket_SIOCGIFNAME(socket, ifindex, &ifname);
                if (r)
                        return r;
        }

        r = setsockopt(socket,
                       SOL_SOCKET,
                       SO_BINDTODEVICE,
                       ifname,
                       strlen(ifname));
        if (r < 0)
                return -errno;

        return 0;
}