#include "NetworkView.h"
#include <LayoutBuilder.h>
#include <ColumnTypes.h> // For BStringColumn etc.
#include <Alert.h>       // For error reporting (temporary)
#include <NetworkInterface.h> // Modern Haiku way: BNetworkInterface
#include <NetServer.h> // For BNetworkRoster

#include <map> // Using std::map to store previous stats per interface

// Define column constants for BColumnListView
enum {
    kInterfaceNameColumn,
    kInterfaceTypeColumn,
    kInterfaceAddressColumn,
    kBytesSentColumn,
    kBytesRecvColumn,
    kPacketsSentColumn,
    kPacketsRecvColumn,
    kSendSpeedColumn,
    kRecvSpeedColumn,
    kSendErrorsColumn,
    kRecvErrorsColumn
};

// Structure to hold current and previous stats for speed calculation
struct InterfaceStatsRecord {
    uint64      bytesSent;
    uint64      bytesReceived;
    bigtime_t   lastUpdateTime;

    InterfaceStatsRecord() : bytesSent(0), bytesReceived(0), lastUpdateTime(0) {}
};

// Global map to store previous stats. A bit simplified, ideally part of the class
// or handled more robustly if interfaces change names or order.
static std::map<std::string, InterfaceStatsRecord> gPreviousStatsMap;


NetworkView::NetworkView(BRect frame)
    : BView(frame, "NetworkView", B_FOLLOW_ALL_SIDES, B_WILL_DRAW | B_PULSE_NEEDED),
      fSocket(-1)
{
    SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

    BBox* netBox = new BBox("NetworkInterfacesBox");
    netBox->SetLabel("Network Interfaces");

    BRect clvRect = netBox->Bounds();
    font_height fh;
    netBox->GetFontHeight(&fh);
    clvRect.top += fh.ascent + fh.descent + fh.leading + B_USE_DEFAULT_SPACING;
    clvRect.InsetBy(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING);


    fInterfaceListView = new BColumnListView(clvRect, "interface_clv",
                                             B_FOLLOW_ALL_SIDES,
                                             B_WILL_DRAW | B_NAVIGABLE,
                                             B_PLAIN_BORDER, true);

    fInterfaceListView->AddColumn(new BStringColumn("Name", 100, 50, 200, B_TRUNCATE_END), kInterfaceNameColumn);
    fInterfaceListView->AddColumn(new BStringColumn("Type", 80, 40, 150, B_TRUNCATE_END), kInterfaceTypeColumn);
    fInterfaceListView->AddColumn(new BStringColumn("Address", 120, 50, 300, B_TRUNCATE_END), kInterfaceAddressColumn);
    fInterfaceListView->AddColumn(new BStringColumn("Sent", 90, 50, 200, B_TRUNCATE_END, B_ALIGN_RIGHT), kBytesSentColumn);
    fInterfaceListView->AddColumn(new BStringColumn("Recv", 90, 50, 200, B_TRUNCATE_END, B_ALIGN_RIGHT), kBytesRecvColumn);
    fInterfaceListView->AddColumn(new BStringColumn("TX Speed", 90, 50, 150, B_TRUNCATE_END, B_ALIGN_RIGHT), kSendSpeedColumn);
    fInterfaceListView->AddColumn(new BStringColumn("RX Speed", 90, 50, 150, B_TRUNCATE_END, B_ALIGN_RIGHT), kRecvSpeedColumn);
    // fInterfaceListView->AddColumn(new BStringColumn("Pkt Sent", 80, 40, 150, B_TRUNCATE_END, B_ALIGN_RIGHT), kPacketsSentColumn);
    // fInterfaceListView->AddColumn(new BStringColumn("Pkt Recv", 80, 40, 150, B_TRUNCATE_END, B_ALIGN_RIGHT), kPacketsRecvColumn);
    // fInterfaceListView->AddColumn(new BStringColumn("TX Err", 60, 30, 100, B_TRUNCATE_END, B_ALIGN_RIGHT), kSendErrorsColumn);
    // fInterfaceListView->AddColumn(new BStringColumn("RX Err", 60, 30, 100, B_TRUNCATE_END, B_ALIGN_RIGHT), kRecvErrorsColumn);

    fInterfaceListView->SetSortColumn(fInterfaceListView->ColumnAt(kInterfaceNameColumn), true, true);

    BLayoutBuilder::Group<>(netBox, B_VERTICAL, 0)
        .SetInsets(B_USE_DEFAULT_SPACING, fh.ascent + fh.descent + fh.leading + B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
        .Add(fInterfaceListView);

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .SetInsets(0)
        .Add(netBox)
    .End();

    // Open a socket for ioctl calls (fallback or for certain info)
    // BNetworkInterface and BNetworkRoster are preferred for modern Haiku.
    fSocket = socket(AF_INET, SOCK_DGRAM, 0); // Or AF_LOCAL
    // if (fSocket < 0) {
    //     (new BAlert("NetworkView", "Failed to open socket for network stats.", "OK"))->Go(NULL);
    // }
}

NetworkView::~NetworkView()
{
    if (fSocket >= 0) {
        close(fSocket);
    }
    // fInterfaceListView is a child of netBox, auto-deleted.
}

void NetworkView::AttachedToWindow()
{
    UpdateData();
    BView::AttachedToWindow();
}

void NetworkView::Pulse()
{
    UpdateData();
}

BString NetworkView::FormatBytes(uint64 bytes) {
    BString str;
    double kb = bytes / 1024.0;
    double mb = kb / 1024.0;
    double gb = mb / 1024.0;

    if (gb >= 1.0) {
        str.SetToFormat("%.2f GiB", gb);
    } else if (mb >= 1.0) {
        str.SetToFormat("%.2f MiB", mb);
    } else if (kb >= 1.0) {
        str.SetToFormat("%.1f KiB", kb);
    } else {
        str.SetToFormat("%llu B", bytes);
    }
    return str;
}

BString NetworkView::FormatSpeed(uint64 bytesDelta, bigtime_t microSecondsDelta) {
    BString str;
    if (microSecondsDelta <= 0) return "0 B/s";

    double seconds = microSecondsDelta / 1000000.0;
    double speed = bytesDelta / seconds; // Bytes per second

    double kbs = speed / 1024.0;
    double mbs = kbs / 1024.0;

    if (mbs >= 1.0) {
        str.SetToFormat("%.2f MiB/s", mbs);
    } else if (kbs >= 1.0) {
        str.SetToFormat("%.2f KiB/s", kbs);
    } else {
        str.SetToFormat("%.0f B/s", speed);
    }
    return str;
}


const char* NetworkView::GetInterfaceType(uint8 ifType) {
    // From <net/if_types.h>
    switch (ifType) {
        case IFT_OTHER: return "Other";
        case IFT_ETHER: return "Ethernet";
        case IFT_ISO88025: return "TokenRing";
        case IFT_FDDI: return "FDDI";
        case IFT_PPP: return "PPP";
        case IFT_SLIP: return "SLIP";
        case IFT_LOOP: return "Loopback";
        case IFT_IEEE80211: return "WiFi";
        default: return "Unknown";
    }
}

void NetworkView::UpdateData()
{
    fLocker.Lock();

    // Clear existing rows (simple update strategy)
    while (BRow* row = fInterfaceListView->RowAt(0)) {
        fInterfaceListView->RemoveRow(row);
        delete row;
    }

    BNetworkRoster& roster = BNetworkRoster::Default();
    if (!roster.IsInstantiated()) {
        // Handle error: roster not available
        // (new BAlert("Network Error", "BNetworkRoster not available.", "OK"))->Go(NULL);
        fLocker.Unlock();
        return;
    }

    uint32 cookie = 0;
    BNetworkInterface interface;

    bigtime_t currentTime = system_time();

    while (roster.GetNextInterface(&cookie, interface) == B_OK) {
        BRow* row = new BRow();
        row->SetField(new BStringField(interface.Name()), kInterfaceNameColumn);

        // Get interface type (more reliable than flags for general type)
        // This requires ioctl or other means if BNetworkInterface doesn't expose it easily.
        // BNetworkInterface::Flags() can give IFF_LOOPBACK, IFF_POINTOPOINT.
        // For physical type like Ethernet/WiFi, ioctl(SIOCGIFMEDIA) or similar is often used.
        // Let's try to get it via ifreq for now if BNetworkInterface doesn't offer a direct type string.
        ifreq ifr;
        strncpy(ifr.ifr_name, interface.Name(), IFNAMSIZ -1);
        ifr.ifr_name[IFNAMSIZ-1] = '\0';

        // Type from BNetworkInterface (not directly available as string)
        // We can use ioctl for more details.
        // For now, a placeholder or deduce from flags.
        BString typeStr = "N/A";
        if (interface.Flags() & IFF_LOOPBACK) typeStr = "Loopback";
        else if (interface.Flags() & IFF_POINTOPOINT) typeStr = "Point-to-Point";
        // else could try SIOCGIFMEDIA or check common names like "eth", "wlan"
        // For now, let's use this basic deduction.
        // The GetInterfaceType function relies on if_data.ifi_type which comes from SIOCGIFDATA
        // or similar low-level calls. BNetworkInterface is higher level.
        // A more robust way is to use ifconfig output or the ioctl method.

        if (fSocket >= 0) {
            if (ioctl(fSocket, SIOCGIFTYPE, &ifr, sizeof(ifr)) == 0) {
                 typeStr = GetInterfaceType(ifr.ifr_type);
            } else if (ioctl(fSocket, SIOCGIFMEDIA, &ifr, sizeof(ifr)) == 0) { // Fallback using media
                ifmr ifmr;
                memcpy(ifmr.ifm_name, interface.Name(), IFNAMSIZ);
                if (ioctl(fSocket, SIOCGIFXMEDIA, &ifmr, sizeof(ifmr)) == 0) {
                    if (IFM_TYPE(ifmr.ifm_current) == IFM_ETHER) typeStr = "Ethernet";
                    else if (IFM_TYPE(ifmr.ifm_current) == IFM_IEEE80211) typeStr = "WiFi";
                    // Add more types based on IFM_TYPE definitions
                }
            }
        }
        row->SetField(new BStringField(typeStr), kInterfaceTypeColumn);


        // Get first usable IP address (IPv4 or IPv6)
        BString addressStr = "N/A";
        for (int32 i = 0; i < interface.CountAddresses(); i++) {
            BNetworkAddress addr = interface.AddressAt(i);
            if (addr.Family() == AF_INET || addr.Family() == AF_INET6) {
                if (!addr.IsLoopback() || (interface.Flags() & IFF_LOOPBACK) ) { // Show loopback addr for loopback iface
                     addressStr = addr.ToString();
                     break;
                }
            }
        }
        row->SetField(new BStringField(addressStr), kInterfaceAddressColumn);

        // Statistics using SIOCGIFSTATS (or BNetworkInterface if it provided them)
        // BNetworkInterface doesn't directly expose all raw counters like bytes/packets.
        // We must use ioctl(SIOCGIFSTATS) for that.
        uint64 bytesSent = 0, bytesRecv = 0, packetsSent = 0, packetsRecv = 0;
        uint64 sendErrors = 0, recvErrors = 0;
        BString sendSpeedStr = "0 B/s";
        BString recvSpeedStr = "0 B/s";

        if (fSocket >= 0) {
            strncpy(ifr.ifr_name, interface.Name(), IFNAMSIZ -1);
            ifr.ifr_name[IFNAMSIZ-1] = '\0';

            if (ioctl(fSocket, SIOCGIFSTATS, &ifr, sizeof(ifr)) == 0) {
                bytesSent = ifr.ifr_stats.ifi_obytes;
                bytesRecv = ifr.ifr_stats.ifi_ibytes;
                packetsSent = ifr.ifr_stats.ifi_opackets;
                packetsRecv = ifr.ifr_stats.ifi_ipackets;
                sendErrors = ifr.ifr_stats.ifi_oerrors;
                recvErrors = ifr.ifr_stats.ifi_ierrors;

                // Calculate speed
                std::string ifNameStd = interface.Name();
                if (gPreviousStatsMap.count(ifNameStd)) {
                    InterfaceStatsRecord& prev = gPreviousStatsMap[ifNameStd];
                    if (prev.lastUpdateTime > 0 && currentTime > prev.lastUpdateTime) {
                        uint64 deltaSent = (bytesSent >= prev.bytesSent) ? (bytesSent - prev.bytesSent) : 0;
                        uint64 deltaRecv = (bytesRecv >= prev.bytesReceived) ? (bytesRecv - prev.bytesReceived) : 0;
                        bigtime_t deltaTime = currentTime - prev.lastUpdateTime;

                        sendSpeedStr = FormatSpeed(deltaSent, deltaTime);
                        recvSpeedStr = FormatSpeed(deltaRecv, deltaTime);
                    }
                }
                // Update previous stats for next pulse
                gPreviousStatsMap[ifNameStd].bytesSent = bytesSent;
                gPreviousStatsMap[ifNameStd].bytesReceived = bytesRecv;
                gPreviousStatsMap[ifNameStd].lastUpdateTime = currentTime;

            } else {
                // perror("ioctl SIOCGIFSTATS"); // For debugging
            }
        }

        row->SetField(new BStringField(FormatBytes(bytesSent).String()), kBytesSentColumn);
        row->SetField(new BStringField(FormatBytes(bytesRecv).String()), kBytesRecvColumn);
        row->SetField(new BStringField(sendSpeedStr), kSendSpeedColumn);
        row->SetField(new BStringField(recvSpeedStr), kRecvSpeedColumn);
        // row->SetField(new BStringField(std::to_string(packetsSent).c_str()), kPacketsSentColumn);
        // row->SetField(new BStringField(std::to_string(packetsRecv).c_str()), kPacketsRecvColumn);
        // row->SetField(new BStringField(std::to_string(sendErrors).c_str()), kSendErrorsColumn);
        // row->SetField(new BStringField(std::to_string(recvErrors).c_str()), kRecvErrorsColumn);

        fInterfaceListView->AddRow(row);
    }

    fLocker.Unlock();
}
