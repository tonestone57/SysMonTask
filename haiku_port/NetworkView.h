#ifndef NETWORKVIEW_H
#define NETWORKVIEW_H

#include <View.h>
#include <StringView.h>
#include <Box.h>
#include <ColumnListView.h>
#include <Locker.h>

// For network interface stats
#include <sys/socket.h> // For AF_LINK, sockaddr_dl
#include <net/if.h>     // For ifreq, IFNAMSIZ
#include <net/if_dl.h>  // For sockaddr_dl
#include <net/if_media.h> // For ifmediareq
#include <net/if_types.h> // For IFT_*
#include <arpa/inet.h>  // For inet_ntop
#include <stdio.h>      // For snprintf
#include <string.h>     // For strerror
#include <unistd.h>     // For close() on socket
#include <sys/ioctl.h>  // For ioctl commands like SIOCGIFSTATS, SIOCGIFCONF


struct NetworkInterfaceInfo {
    BString     name;
    BString     type; // e.g., Ethernet, WiFi
    BString     address; // IPv4 or IPv6
    uint64      bytesSent;
    uint64      bytesReceived;
    uint64      packetsSent;
    uint64      packetsReceived;
    uint64      sendErrors;
    uint64      recvErrors;
    // Store previous values to calculate speed
    bigtime_t   lastUpdateTime;
    uint64      lastBytesSent;
    uint64      lastBytesReceived;
    BString     sendSpeed; // Formatted string (e.g., "KB/s")
    BString     recvSpeed; // Formatted string
};


class NetworkView : public BView {
public:
    NetworkView(BRect frame);
    ~NetworkView();

    virtual void AttachedToWindow();
    virtual void Pulse();

private:
    void UpdateData();
    BString FormatBytes(uint64 bytes);
    BString FormatSpeed(uint64 bytes, bigtime_t microSeconds);
    const char* GetInterfaceType(uint8 ifType);

    BColumnListView*    fInterfaceListView;
    // We'll need a way to store info for multiple interfaces if present
    // A BList of NetworkInterfaceInfo structs, or manage rows in CLV directly

    // Map interface name to its struct to preserve speed calculation data
    // This is simpler than managing BList indices if interfaces can appear/disappear
    // However, Haiku's interface list is usually stable.
    // For now, let's re-fetch and update all. A map might be useful for persistent stats.
    // BMap<BString, NetworkInterfaceInfo> fInterfaceStatsMap;

    // To keep track of previous stats for speed calculation for each interface
    // A BList of NetworkInterfaceInfo, assuming interface order from SIOCGIFCONF is stable.
    // Or, better, use a map from interface name to its previous state.
    // For now, let's rebuild the list each time and calculate speed based on the last Pulse.
    // This requires storing the previous stats *within* the BRow or a parallel structure.

    BLocker fLocker;
    int fSocket; // Socket for ioctl calls
};

#endif // NETWORKVIEW_H
