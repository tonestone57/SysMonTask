#include "SysInfoView.h"
#include <kernel/OS.h>      // For system_info, get_system_info, get_cpu_info
#include <Screen.h>         // For BScreen, accelerant_device_info
#include <GraphicsDefs.h>   // For accelerant_device_info
#include <VolumeRoster.h>   // For BVolumeRoster
#include <Volume.h>         // For BVolume
#include <fs_info.h>        // For fs_info
#include <driver_settings.h> // For parsing CPU features (if available this way)
#include <stdio.h>          // For snprintf
#include <time.h>           // For ctime, strftime for build date
#include <TextView.h>       // For BTextView
#include <String.h>
#include <Alignment.h>
#include <SpaceLayoutItem.h>
#include <Font.h>

// Helper to add a labeled string view to a grid layout
static void AddInfoRow(BGridLayout* grid, int32& row, const char* labelText, BStringView*& valueView) {
    BStringView* labelView = new BStringView(NULL, labelText); // Name can be null for labels not needing lookup
    labelView->SetAlignment(B_ALIGN_RIGHT);
    grid->AddView(labelView, 0, row);
    valueView = new BStringView(NULL, "N/A"); // Value placeholder
    grid->AddView(valueView, 1, row);
    grid->AddItem(BSpaceLayoutItem::CreateHorizontalStrut(5), 2, row); // Spacer
    row++;
}


SysInfoView::SysInfoView(BRect frame)
    : BView(frame, "SysInfoView", B_FOLLOW_ALL_SIDES, B_WILL_DRAW),
      fDiskInfoTextView(NULL), fDiskInfoScrollView(NULL), fMainSectionsBox(NULL)
{
    SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    CreateLayout();
}

SysInfoView::~SysInfoView()
{
    // Child views are automatically deleted by parent
}

void SysInfoView::CreateLayout()
{
    fMainSectionsBox = new BBox(Bounds(), "mainSysInfoBox", B_FOLLOW_ALL_SIDES, B_WILL_DRAW | B_FRAME_EVENTS, B_PLAIN_BORDER);
    fMainSectionsBox->SetLabel("System Information");

    // --- OS Section ---
    BBox* osBox = new BBox("OSInfo");
    osBox->SetLabel("Operating System");
    BGridLayout* osGrid = new BGridLayout(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING);
    osGrid->SetInsets(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING);
    int32 row = 0;
    AddInfoRow(osGrid, row, "Kernel Name:", fKernelNameValue);
    AddInfoRow(osGrid, row, "Kernel Version:", fKernelVersionValue);
    AddInfoRow(osGrid, row, "Build Date/Time:", fKernelBuildValue);
    AddInfoRow(osGrid, row, "CPU Architecture:", fCPUArchValue);
    AddInfoRow(osGrid, row, "System Uptime:", fUptimeValue);
    osBox->AddChild(osGrid->View());

    // --- CPU Section ---
    BBox* cpuBox = new BBox("CPUInfo");
    cpuBox->SetLabel("Processor");
    BGridLayout* cpuGrid = new BGridLayout(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING);
    cpuGrid->SetInsets(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING);
    row = 0;
    AddInfoRow(cpuGrid, row, "Model:", fCPUModelValue);
    AddInfoRow(cpuGrid, row, "Cores:", fCPUCoresValue);
    AddInfoRow(cpuGrid, row, "Clock Speed:", fCPUClockSpeedValue);
    AddInfoRow(cpuGrid, row, "L1 Cache (I/D):", fL1CacheValue);
    AddInfoRow(cpuGrid, row, "L2 Cache:", fL2CacheValue);
    AddInfoRow(cpuGrid, row, "L3 Cache:", fL3CacheValue);
    AddInfoRow(cpuGrid, row, "Features:", fCPUFeaturesValue);
    fCPUFeaturesValue->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET)); // Allow to wrap if needed
    cpuBox->AddChild(cpuGrid->View());

    // --- Graphics Section ---
    BBox* graphicsBox = new BBox("GraphicsInfo");
    graphicsBox->SetLabel("Graphics");
    BGridLayout* graphicsGrid = new BGridLayout(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING);
    graphicsGrid->SetInsets(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING);
    row = 0;
    AddInfoRow(graphicsGrid, row, "GPU Type:", fGPUTypeValue);
    AddInfoRow(graphicsGrid, row, "Driver:", fGPUDriverValue);
    AddInfoRow(graphicsGrid, row, "VRAM:", fGPUVRAMValue);
    AddInfoRow(graphicsGrid, row, "Resolution:", fScreenResolutionValue);
    graphicsBox->AddChild(graphicsGrid->View());

    // --- Memory Section ---
    BBox* memoryBox = new BBox("MemoryInfo");
    memoryBox->SetLabel("Memory");
    BGridLayout* memoryGrid = new BGridLayout(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING);
    memoryGrid->SetInsets(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING);
    row = 0;
    AddInfoRow(memoryGrid, row, "Total RAM:", fTotalRAMValue);
    memoryBox->AddChild(memoryGrid->View());

    // --- Disk Section ---
    BBox* diskBox = new BBox("DiskInfo");
    diskBox->SetLabel("Disk Volumes");
    fDiskInfoTextView = new BTextView("diskInfoTextView");
    fDiskInfoTextView->SetWordWrap(false);
    fDiskInfoTextView->MakeEditable(false);
    fDiskInfoTextView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR)); // Match background
    fDiskInfoScrollView = new BScrollView("diskInfoScroller", fDiskInfoTextView, B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP_BOTTOM, 0, false, true, B_PLAIN_BORDER); // Horizontal scrollbar off
    fDiskInfoScrollView->SetExplicitMinSize(BSize(0, 80)); // Give it some min height

    BLayoutBuilder::Group<>(diskBox, B_VERTICAL, 0)
        .SetInsets(B_USE_DEFAULT_SPACING) // Insets for the box content
        .Add(fDiskInfoScrollView);


    // Add all sections to the main box's layout manager
    // Using a BGroupLayout for the fMainSectionsBox to hold other boxes vertically
    BGroupLayout* mainGroupLayout = new BGroupLayout(B_VERTICAL, B_USE_DEFAULT_SPACING);
    fMainSectionsBox->SetLayout(mainGroupLayout); // Set layout for the main box
    mainGroupLayout->SetInsets(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING);
    // Add a B_USE_WINDOW_INSETS like value for the top if BBox label is drawn
    font_height fh;
    fMainSectionsBox->GetFontHeight(&fh);
    mainGroupLayout->SetInsets(B_USE_DEFAULT_SPACING, fh.ascent + fh.descent + fh.leading + B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING);


    mainGroupLayout->AddView(osBox);
    mainGroupLayout->AddView(cpuBox);
    mainGroupLayout->AddView(graphicsBox);
    mainGroupLayout->AddView(memoryBox);
    mainGroupLayout->AddView(diskBox);
    mainGroupLayout->AddGlue(); // Push all content to the top

    // The SysInfoView itself will contain a ScrollView that contains fMainSectionsBox
    // This allows the entire content to be scrollable if it exceeds the view height.
    BScrollView* viewScroller = new BScrollView("sysInfoScroller", fMainSectionsBox, B_FOLLOW_ALL_SIDES, 0, false, true, B_NO_BORDER);

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .SetInsets(0) // No insets for the top BView, scroller handles it.
        .Add(viewScroller)
    .End();
}


void SysInfoView::AttachedToWindow()
{
    BView::AttachedToWindow();
    LoadData(); // Load data when view is attached
}

// Helper Formatters
BString SysInfoView::FormatBytes(uint64 bytes, int precision) {
    BString str;
    double kb = bytes / 1024.0;
    double mb = kb / 1024.0;
    double gb = mb / 1024.0;

    if (gb >= 1.0) {
        str.SetToFormat("%.*f GiB", precision, gb);
    } else if (mb >= 1.0) {
        str.SetToFormat("%.*f MiB", precision, mb);
    } else if (kb >= 1.0) {
        str.SetToFormat("%.*f KiB", precision, kb);
    } else {
        str.SetToFormat("%llu Bytes", bytes);
    }
    return str;
}

BString SysInfoView::FormatHertz(uint64 hertz) {
    BString str;
    double ghz = hertz / 1000000000.0;
    double mhz = hertz / 1000000.0;
    double khz = hertz / 1000.0;

    if (ghz >= 1.0) {
        str.SetToFormat("%.2f GHz", ghz);
    } else if (mhz >= 1.0) {
        str.SetToFormat("%.0f MHz", mhz);
    } else if (khz >= 1.0) {
        str.SetToFormat("%.0f KHz", khz);
    } else {
        str.SetToFormat("%llu Hz", hertz);
    }
    return str;
}

BString SysInfoView::FormatUptime(bigtime_t bootTime) {
    bigtime_t now = system_time();
    bigtime_t uptimeMicros = now - bootTime;
    uint32 seconds = uptimeMicros / 1000000;

    uint32 days = seconds / (24 * 3600);
    seconds %= (24 * 3600);
    uint32 hours = seconds / 3600;
    seconds %= 3600;
    uint32 minutes = seconds / 60;
    seconds %= 60;

    BString uptimeStr;
    if (days > 0) {
        uptimeStr.SetToFormat("%u days, %02u:%02u:%02u", days, hours, minutes, seconds);
    } else {
        uptimeStr.SetToFormat("%02u:%02u:%02u", hours, minutes, seconds);
    }
    return uptimeStr;
}

BString SysInfoView::GetCPUFeatureFlags() {
    BString featuresStr;
    uint32 features = 0;
    uint32 extendedFeatures = 0;

    if (get_cpu_features(&features) != B_OK) {
        // featuresStr += "Features N/A; ";
    }
    if (get_extended_cpu_features(&extendedFeatures) != B_OK) {
        // featuresStr += "Ext Features N/A";
    }

    // Check for common SIMD features first
    if (features & B_CPU_FEATURE_MMX) featuresStr += "MMX ";
    if (features & B_CPU_FEATURE_SSE) featuresStr += "SSE ";
    if (features & B_CPU_FEATURE_SSE2) featuresStr += "SSE2 ";
    if (extendedFeatures & B_CPU_EXT_FEATURE_SSE3) featuresStr += "SSE3 "; // B_CPU_FEATURE_SSE3 in some headers
    if (extendedFeatures & B_CPU_EXT_FEATURE_SSSE3) featuresStr += "SSSE3 ";
    if (extendedFeatures & B_CPU_EXT_FEATURE_SSE4_1) featuresStr += "SSE4.1 ";
    if (extendedFeatures & B_CPU_EXT_FEATURE_SSE4_2) featuresStr += "SSE4.2 ";
    if (extendedFeatures & B_CPU_EXT_FEATURE_AVX) featuresStr += "AVX ";
    if (extendedFeatures & B_CPU_EXT_FEATURE_AVX2) featuresStr += "AVX2 ";
    // Add more common features as desired
    // if (features & B_CPU_FEATURE_FPU) featuresStr += "FPU ";
    // if (features & B_CPU_FEATURE_TSC) featuresStr += "TSC ";
    // if (features & B_CPU_FEATURE_CMOV) featuresStr += "CMOV ";
    // if (extendedFeatures & B_CPU_EXT_FEATURE_ABM) featuresStr += "ABM "; // (LZCNT)
    // if (extendedFeatures & B_CPU_EXT_FEATURE_AES) featuresStr += "AES ";
    // if (extendedFeatures & B_CPU_EXT_FEATURE_RDRAND) featuresStr += "RDRAND ";


    if (featuresStr.Length() == 0) {
        featuresStr = "N/A or no common features detected";
    }
    return featuresStr.Trim();
}


void SysInfoView::LoadData() {
    system_info sysInfo;
    if (get_system_info(&sysInfo) == B_OK) {
        // --- OS Info ---
        fKernelNameValue->SetText(sysInfo.kernel_name);

        BString kernelVer;
        kernelVer.SetToFormat("%s (API %lu)", sysInfo.kernel_version, sysInfo.abi);
        fKernelVersionValue->SetText(kernelVer);

        char buildDateTime[100];
        struct tm build_tm;
        localtime_r(&sysInfo.kernel_build_time, &build_tm); // kernel_build_time is time_t
        strftime(buildDateTime, sizeof(buildDateTime), "%Y-%m-%d %H:%M:%S", &build_tm);
        fKernelBuildValue->SetText(buildDateTime);

        const char* arch = "Unknown";
        #if __x86_64__
            arch = "x86_64";
        #elif __INTEL__ // For x86 (32-bit)
            arch = "x86 (32-bit)";
        #elif __POWERPC__
            arch = "PowerPC";
        #elif __ARM__
            arch = "ARM";
        #elif __M68K__
            arch = "m68k";
        #endif
        // sysInfo.architecture might also provide a string on some Haiku versions/builds
        fCPUArchValue->SetText(arch);

        fUptimeValue->SetText(FormatUptime(sysInfo.boot_time).String());

        // --- CPU Info ---
        // Note: cpu_info is an array. For simplicity, showing info for the first CPU.
        // A more complex UI could list all CPUs if heterogeneous or desired.
        if (sysInfo.cpu_count > 0) {
            cpu_info cpuInfo = sysInfo.cpu_infos[0]; // Info for the first core/package

            // Construct a CPU model string if Haiku doesn't provide one directly
            // Some Haiku versions might populate cpu_info.model_name or similar.
            // If not, we try to get it from kernel args or a known location if any.
            // For now, using sysInfo.cpu_type which is often a generic type.
            // The actual "brand string" might not be easily available without parsing /dev/cpu/*
            // or specific low-level mechanisms not in system_info.
            // The kernel might store the brand string in sysInfo.cpu_type or similar.
            // For now, we use what's available in system_info.
            BString cpuModelString;
            // cpu_info.model_name is not standard in BeOS R5 system_info, might be Haiku specific.
            // Check if sysInfo.cpu_type itself is descriptive enough
            if (strlen(sysInfo.cpu_type) > 0 ) { // cpu_type is char array in system_info
                 cpuModelString = sysInfo.cpu_type;
            } else {
                cpuModelString = "N/A"; // Fallback
            }
            fCPUModelValue->SetText(cpuModelString);


            BString coreStr;
            coreStr << sysInfo.cpu_count;
            fCPUCoresValue->SetText(coreStr);
            fCPUClockSpeedValue->SetText(FormatHertz(sysInfo.cpu_clock_speed).String());

            // Cache sizes (L1 is often per core, L2/L3 can be shared or per core)
            // Summing for simplicity if per-core details are too verbose for this view
            uint64 totalL1i = 0, totalL1d = 0, totalL2 = 0, totalL3 = 0;
            for (uint32 i = 0; i < sysInfo.cpu_count; ++i) {
                totalL1i += sysInfo.cpu_infos[i].l1_cache_size; // L1I often not in cpu_info directly, might be combined
                totalL1d += sysInfo.cpu_infos[i].l1_cache_size; // Assuming l1_cache_size is L1D or combined L1
                                                                // Haiku's cpu_info has l1_icache_size, l1_dcache_size, l2_cache_size, l3_cache_size
                                                                // but these might not be in the older BeOS compatible system_info struct directly.
                                                                // Let's assume system_info is extended or use get_cpu_info if needed
            }
            // For a more accurate modern Haiku approach:
            // get_cpu_info(B_CPU_TCB, 0, &cpuSpecificInfo) might be better.
            // For now, stick to what's in system_info.cpu_infos[0] for simplicity
            // if these fields (l1_icache_size etc.) are indeed Haiku extensions to cpu_info.
            // The BeBook cpu_info has:
            // uint32	l1_cache_size;	/* size of L1 cache */
            // uint32	l2_cache_size;	/* size of L2 cache */
            // uint32	l3_cache_size;	/* size of L3 cache */
            // Haiku likely extended this with separate I/D cache sizes.
            // Let's assume sysInfo.cpu_infos[0] has the extended fields for now.
            // If not, these will be 0.
            BString l1Str, l2Str, l3Str;
            if (sysInfo.cpu_infos[0].l1_icache_size > 0 || sysInfo.cpu_infos[0].l1_dcache_size > 0) {
                 l1Str.SetToFormat("%s I / %s D (per core avg)",
                                FormatBytes(sysInfo.cpu_infos[0].l1_icache_size,0).String(),
                                FormatBytes(sysInfo.cpu_infos[0].l1_dcache_size,0).String());
            } else if (sysInfo.cpu_infos[0].l1_cache_size > 0) { // Fallback to combined L1
                 l1Str.SetToFormat("%s (per core avg)", FormatBytes(sysInfo.cpu_infos[0].l1_cache_size,0).String());
            } else {
                l1Str = "N/A";
            }

            if(sysInfo.cpu_infos[0].l2_cache_size > 0)
                l2Str.SetToFormat("%s", FormatBytes(sysInfo.cpu_infos[0].l2_cache_size,0).String());
            else l2Str = "N/A";

            if(sysInfo.cpu_infos[0].l3_cache_size > 0)
                l3Str.SetToFormat("%s", FormatBytes(sysInfo.cpu_infos[0].l3_cache_size,0).String());
            else l3Str = "N/A";

            fL1CacheValue->SetText(l1Str);
            fL2CacheValue->SetText(l2Str);
            fL3CacheValue->SetText(l3Str);

            fCPUFeaturesValue->SetText(GetCPUFeatureFlags().String());
        } else {
            fCPUModelValue->SetText("N/A");
            fCPUCoresValue->SetText("N/A");
            fCPUClockSpeedValue->SetText("N/A");
            fL1CacheValue->SetText("N/A");
            fL2CacheValue->SetText("N/A");
            fL3CacheValue->SetText("N/A");
            fCPUFeaturesValue->SetText("N/A");
        }

        // --- Memory Info ---
        fTotalRAMValue->SetText(FormatBytes( (uint64)sysInfo.max_pages * B_PAGE_SIZE ).String());

    } else {
        // Handle error getting system_info
        const char* errorMsg = "Error fetching system info";
        fKernelNameValue->SetText(errorMsg);
        // ... set all other fields to error or N/A
    }

    // --- Graphics Info ---
    BScreen screen(B_MAIN_SCREEN_ID);
    if (screen.IsValid()) {
        accelerant_device_info deviceInfo;
        if (screen.GetDeviceInfo(&deviceInfo) == B_OK) {
            fGPUTypeValue->SetText(deviceInfo.name);
            BString driverVerStr;
            driverVerStr << deviceInfo.version; // Version is uint32
            fGPUDriverValue->SetText(driverVerStr);
            fGPUVRAMValue->SetText(FormatBytes(deviceInfo.memory).String());
        } else {
            fGPUTypeValue->SetText("Error getting GPU info");
            fGPUDriverValue->SetText("N/A");
            fGPUVRAMValue->SetText("N/A");
        }
        BRect frame = screen.Frame();
        BString resStr;
        resStr.SetToFormat("%.0fx%.0f", frame.Width() + 1, frame.Height() + 1);
        fScreenResolutionValue->SetText(resStr);
    } else {
        fGPUTypeValue->SetText("Error: Invalid screen object");
        fGPUDriverValue->SetText("N/A");
        fGPUVRAMValue->SetText("N/A");
        fScreenResolutionValue->SetText("N/A");
    }

    // --- Disk Info ---
    BString diskTextData;
    BVolumeRoster volRoster;
    BVolume volume;
    volRoster.Rewind();
    int diskCount = 0;
    while (volRoster.GetNextVolume(&volume) == B_OK) {
        if (!volume.KnowsCapacity()) continue; // Skip virtual/non-capacity volumes
        diskCount++;
        fs_info fsInfo;
        if (fs_stat_dev(volume.Device(), &fsInfo) == B_OK) {
            if (diskTextData.Length() > 0) diskTextData << "\n---\n"; // Separator
            diskTextData << "Volume Name: " << fsInfo.volume_name << "\n";

            BDirectory rootDir;
            volume.GetRootDirectory(&rootDir);
            BEntry entry;
            rootDir.GetEntry(&entry);
            BPath path;
            entry.GetPath(&path);
            diskTextData << "Mount Point: " << path.Path() << "\n";

            diskTextData << "File System: " << fsInfo.fsh_name << "\n";
            diskTextData << "Total Size: " << FormatBytes(fsInfo.total_blocks * fsInfo.block_size).String() << "\n";
            diskTextData << "Free Size: " << FormatBytes(fsInfo.free_blocks * fsInfo.block_size).String();
            // Determining physical type (USB, SATA, HDD/SSD) is complex and generally
            // not available from BVolume/fs_info directly. Would require deeper device querying.
        }
    }
    if (diskCount == 0) {
        diskTextData = "No disk volumes found or accessible.";
    }
    fDiskInfoTextView->SetText(diskTextData.String());
}

// Dummy main for syntax checking if compiled standalone (remove for project)
// int main() { return 0; }
