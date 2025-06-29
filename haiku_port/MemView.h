#ifndef MEMVIEW_H
#define MEMVIEW_H

#include <View.h>
#include <StringView.h>
#include <Box.h>
#include <GridLayout.h>
#include <GroupLayout.h>
#include <Locker.h>
#include <kernel/OS.h> // For system_info, get_system_info

#define HISTORY_SIZE 100

// Forward declaration for GraphView
class GraphView;

class MemView : public BView {
public:
    MemView(BRect frame);
    ~MemView();

    virtual void AttachedToWindow();
    virtual void Pulse();

private:
    void UpdateData();
    BString FormatBytes(uint64 bytes);

    BStringView* fTotalMemLabel;
    BStringView* fTotalMemValue;
    BStringView* fUsedMemLabel;
    BStringView* fUsedMemValue;
    BStringView* fFreeMemLabel;
    BStringView* fFreeMemValue;
    BStringView* fCachedMemLabel;    // File system cache
    BStringView* fCachedMemValue;
    // BStringView* fBufferedMemLabel; // Block cache - might combine with cached or omit for simplicity
    // BStringView* fBufferedMemValue;


    GraphView* fCacheGraphView;
    float fCacheHistory[HISTORY_SIZE];
    int fCacheHistoryIndex;
    uint64 fTotalSystemMemory; // To calculate percentage for graph if needed

    BLocker fLocker;
};


// Simple generic GraphView class
class GraphView : public BView {
public:
    GraphView(BRect frame, const char* name, const float* historyData, const int* historyIndex, const uint64* totalValue);
    virtual void Draw(BRect updateRect);
    virtual void AttachedToWindow(); // To set a pulse if needed, or initial draw

private:
    const float* fHistoryData; // Pointer to the actual history data in MemView
    const int* fHistoryIndex;  // Pointer to the current index
    const uint64* fTotalValue; // Pointer to total memory for percentage calculation
    rgb_color fGraphColor;
};


#endif // MEMVIEW_H
