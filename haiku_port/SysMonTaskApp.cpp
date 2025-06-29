// Placeholder for the main Haiku application class
// We will start implementing this based on Haiku's Application Kit

#include <Application.h>
#include <Window.h>
#include <View.h>
#include <Alert.h>
#include <stdio.h>
#include <LayoutBuilder.h> // For BLayoutBuilder
#include <TabView.h>      // For BTabView

#include "CPUView.h" // Include the new CPUView header
#include "MemView.h" // Include the new MemView header
#include "DiskView.h" // Include the new DiskView header
#include "NetworkView.h" // Include the new NetworkView header
#include "ProcessView.h" // Include the new ProcessView header
#include "GPUView.h"     // Include the new GPUView header
#include "SysInfoView.h" // Include the new SysInfoView header

// Forward declaration
class MainWindow;

class SysMonTaskApp : public BApplication {
public:
    SysMonTaskApp();
    virtual void ReadyToRun();

private:
    MainWindow* mainWindow;
};

class MainWindow : public BWindow {
public:
    MainWindow(BRect frame);
    virtual bool QuitRequested();
};

MainWindow::MainWindow(BRect frame)
    : BWindow(frame, "SysMonTask Haiku", B_TITLED_WINDOW, B_QUIT_ON_WINDOW_CLOSE) {

    // Create a TabView
    BTabView* tabView = new BTabView("tab_view", B_WIDTH_FROM_LABEL);

    // Create CPU View
    BRect tabFrame = tabView->Bounds();
    // Adjust tabFrame for the actual tab content area (it's smaller than Bounds())
    // This might require getting the container view of the tabview or adjusting after adding a tab.
    // For now, let's assume Bounds() is close enough for initial placement.
    // A better way is to let the TabView manage the frame of its children.

    CPUView* cpuView = new CPUView(tabFrame); // Pass the tab's content area rect
    BTab* cpuTab = new BTab(cpuView);
    tabView->AddTab(cpuView, cpuTab);
    cpuTab->SetLabel("CPU"); // Set label after adding if view is passed directly

    // Create Memory View
    MemView* memoryView = new MemView(tabFrame);
    BTab* memTab = new BTab(memoryView);
    tabView->AddTab(memoryView, memTab);
    memTab->SetLabel("Memory");

    // Create Disk View
    DiskView* diskView = new DiskView(tabFrame);
    BTab* diskTab = new BTab(diskView);
    tabView->AddTab(diskView, diskTab);
    diskTab->SetLabel("Disks");

    // Create Network View
    NetworkView* networkView = new NetworkView(tabFrame);
    BTab* networkTab = new BTab(networkView);
    tabView->AddTab(networkView, networkTab);
    networkTab->SetLabel("Network");

    // Create Process View
    ProcessView* processView = new ProcessView(tabFrame);
    BTab* processTab = new BTab(processView);
    tabView->AddTab(processView, processTab);
    processTab->SetLabel("Processes");

    // Create GPU View
    GPUView* gpuView = new GPUView(tabFrame);
    BTab* gpuTab = new BTab(gpuView);
    tabView->AddTab(gpuView, gpuTab);
    gpuTab->SetLabel("GPU");

    // Create System Info View
    SysInfoView* sysInfoView = new SysInfoView(tabFrame);
    BTab* sysInfoTab = new BTab(sysInfoView);
    tabView->AddTab(sysInfoView, sysInfoTab);
    sysInfoTab->SetLabel("System Info");

    // Use BLayoutBuilder to add the tabView to the window
    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .SetInsets(0) // No insets for the main layout containing the tab view
        .Add(tabView)
    .End();

    // Set PulseRate for the window, so views with B_PULSE_NEEDED will get Pulse() calls
    // Pulse rate in microseconds. 1,000,000 microseconds = 1 second.
    SetPulseRate(1000000); // Pulse once per second, adjust as needed
}

bool MainWindow::QuitRequested() {
    be_app->PostMessage(B_QUIT_REQUESTED);
    return true;
}

SysMonTaskApp::SysMonTaskApp()
    : BApplication("application/x-vnd.SysMonTaskHaiku") {
    // Constructor
}

void SysMonTaskApp::ReadyToRun() {
    BRect windowRect(50, 50, 850, 650); // Adjust size as needed
    mainWindow = new MainWindow(windowRect);
    mainWindow->Show();
}

int main() {
    SysMonTaskApp app;
    app.Run();
    return 0;
}
