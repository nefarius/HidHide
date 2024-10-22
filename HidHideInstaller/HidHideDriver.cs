using System;

using Nefarius.Utilities.DeviceManagement.PnP;

namespace Nefarius.HidHide.Setup;

public static class HidHideDriver
{
    public const string HidHideHardwareId = @"Root\HidHide";

    public static Guid HidHideInterfaceGuid = new("{0C320FF7-BD9B-42B6-BDAF-49FEB9C91649}");

    public const string HidHideClassName = "System";

    public static Guid HidHideClassGuid = DeviceClassIds.System;

    public const string HidHideServiceName = "HidHide";
}