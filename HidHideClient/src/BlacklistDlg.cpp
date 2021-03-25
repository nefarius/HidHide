// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// BlacklistDlg.cpp
#include "stdafx.h"
#include "BlacklistDlg.h"
#include "HidHideApi.h"
#include "Logging.h"

// Define user-message for processing device interface arrivals
constexpr auto WM_USER_CM_NOTIFICATION_REFRESH{ WM_USER + 1 };

// The actual bit pattern for checking the check-boxes
constexpr auto LVIS_STATE_CHECKBOX_MASK      { 0x3000 };
constexpr auto LVIS_STATE_CHECKBOX_UNCHECKED { 0x1000 };
constexpr auto LVIS_STATE_CHECKBOX_CHECKED   { 0x2000 };

// The icon for a certain state
enum class ICON_LOCK
{
    BLANK,
    OFF,
    ON
};

namespace
{
    DWORD CALLBACK OnCmNotificationCallbackStatic(_In_ HCMNOTIFICATION cmNotification, _In_ PVOID context, _In_ CM_NOTIFY_ACTION cmNotifyAction, _In_ PCM_NOTIFY_EVENT_DATA cmNotifyEventData, _In_ DWORD cmNotifyEventDataSize)
    {
        TRACE_ALWAYS(L"");
        return (static_cast<CBlacklistDlg*>(context)->OnCmNotificationCallback(cmNotification, cmNotifyAction, cmNotifyEventData, cmNotifyEventDataSize));
    }
}

IMPLEMENT_DYNAMIC(CBlacklistDlg, CDialogEx)

#pragma warning(push)
#pragma warning(disable: 26454 28213) // Warnings caused by Microsoft MFC macros
BEGIN_MESSAGE_MAP(CBlacklistDlg, CDialogEx)
    ON_NOTIFY(TVN_ITEMCHANGED, IDC_TREE_BLACKLIST, &CBlacklistDlg::OnTvnItemChangedTreeBlacklist)
    ON_BN_CLICKED(IDC_CHECK_BLACKLIST_FILTER,      &CBlacklistDlg::OnBnClickedCheckFilter)
    ON_BN_CLICKED(IDC_CHECK_BLACKLIST_GAMING,      &CBlacklistDlg::OnBnClickedCheckGaming)
    ON_BN_CLICKED(IDC_CHECK_BLACKLIST_ENABLE,      &CBlacklistDlg::OnBnClickedCheckEnable)
    ON_MESSAGE(WM_USER_CM_NOTIFICATION_REFRESH,    &CBlacklistDlg::OnUserMessageRefresh)
    ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()
#pragma warning(pop)

_Use_decl_annotations_
CBlacklistDlg::CBlacklistDlg(CWnd* pParent)
    : CDialogEx(IDD_DIALOG_BLACKLIST, pParent)
    , m_BlacklistItemData{}
    , m_Blacklist{}
    , m_CmNotificationHandle{}
    , m_LockBlank{}
    , m_LockOff{}
    , m_LockOn{}
    , m_ImageList{}
    , m_Filter{}
    , m_Gaming{}
    , m_Enable{}
    , m_Guidance{}
{
    TRACE_ALWAYS(L"");
}

CBlacklistDlg::~CBlacklistDlg()
{
    TRACE_ALWAYS(L"");

    // Unsubscribe from HID device arrival
    ::CM_Unregister_Notification(m_CmNotificationHandle);
}

_Use_decl_annotations_
DWORD CBlacklistDlg::OnCmNotificationCallback(HCMNOTIFICATION cmNotification, CM_NOTIFY_ACTION cmNotifyAction, PCM_NOTIFY_EVENT_DATA cmNotifyEventData, DWORD cmNotifyEventDataSize)
{
    UNREFERENCED_PARAMETER(cmNotification);
    UNREFERENCED_PARAMETER(cmNotifyEventData);
    UNREFERENCED_PARAMETER(cmNotifyEventDataSize);

    // Only act on new device arrivals
    if (CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL == cmNotifyAction)
    {
        TRACE_ALWAYS(L"");
        Refresh();
    }

    return (0);
}

_Use_decl_annotations_
void CBlacklistDlg::DoDataExchange(CDataExchange* pDX)
{
    TRACE_ALWAYS(L"");
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TREE_BLACKLIST,            m_Blacklist);
    DDX_Control(pDX, IDC_CHECK_BLACKLIST_FILTER,    m_Filter);
    DDX_Control(pDX, IDC_CHECK_BLACKLIST_GAMING,    m_Gaming);
    DDX_Control(pDX, IDC_CHECK_BLACKLIST_ENABLE,    m_Enable);
    DDX_Control(pDX, IDC_STATIC_BLACKLIST_GUIDANCE, m_Guidance);
}

BOOL CBlacklistDlg::OnInitDialog()
{
    TRACE_ALWAYS(L"");
    CDialogEx::OnInitDialog();

    // Apply the labels from the string table
    m_Filter.SetWindowTextW  (HidHide::StringTable(IDS_CHECK_BLACKLIST_FILTER).c_str());
    m_Gaming.SetWindowTextW  (HidHide::StringTable(IDS_CHECK_BLACKLIST_GAMING).c_str());
    m_Enable.SetWindowTextW  (HidHide::StringTable(IDS_CHECK_BLACKLIST_ENABLE).c_str());
    m_Guidance.SetWindowTextW(HidHide::StringTable(IDS_STATIC_BLACKLIST_GUIDANCE).c_str());

    // Reflect the current Active state in the check-box
    m_Filter.SetCheck(BST_CHECKED);
    m_Gaming.SetCheck(BST_CHECKED);
    m_Enable.SetCheck(HidHide::GetActive() ? BST_CHECKED : BST_UNCHECKED);

    // Prepare list icons
    if (nullptr == (m_LockBlank = ::LoadIconW(AfxGetApp()->m_hInstance, MAKEINTRESOURCEW(IDI_ICON_BLACKLIST_LOCK_BLANK)))) THROW_WIN32_LAST_ERROR;
    if (nullptr == (m_LockOff   = ::LoadIconW(AfxGetApp()->m_hInstance, MAKEINTRESOURCEW(IDI_ICON_BLACKLIST_LOCK_OFF))))   THROW_WIN32_LAST_ERROR;
    if (nullptr == (m_LockOn    = ::LoadIconW(AfxGetApp()->m_hInstance, MAKEINTRESOURCEW(IDI_ICON_BLACKLIST_LOCK_ON))))    THROW_WIN32_LAST_ERROR;
    if (FALSE == m_ImageList.Create(16, 16, (ILC_COLOR8 | ILC_MASK), 2, 2)) THROW_WIN32(ERROR_INVALID_PARAMETER);
    if (-1 == m_ImageList.Add(m_LockBlank)) THROW_WIN32(ERROR_INVALID_PARAMETER);
    if (-1 == m_ImageList.Add(m_LockOff))   THROW_WIN32(ERROR_INVALID_PARAMETER);
    if (-1 == m_ImageList.Add(m_LockOn))    THROW_WIN32(ERROR_INVALID_PARAMETER);
    m_Blacklist.SetImageList(&m_ImageList, TVSIL_NORMAL);

    // All set ... now subscribe to HID device arrival ... as this may trigger window messages too
    CM_NOTIFY_FILTER cmNotifyFilter{ sizeof(CM_NOTIFY_FILTER), CM_NOTIFY_FILTER_FLAG_ALL_INTERFACE_CLASSES, CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE, 0, 0 };
    if (auto const result{ ::CM_Register_Notification(&cmNotifyFilter, this, &OnCmNotificationCallbackStatic, &m_CmNotificationHandle) }; (CR_SUCCESS != result)) THROW_CONFIGRET(result);

    return (TRUE);
}

_Use_decl_annotations_
void CBlacklistDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
    TRACE_ALWAYS(L"");
    CDialogEx::OnShowWindow(bShow, nStatus);
    Refresh();
}

void CBlacklistDlg::Refresh()
{
    TRACE_ALWAYS(L"");
    PostMessageW(WM_USER_CM_NOTIFICATION_REFRESH, 0, NULL);
}

_Use_decl_annotations_
LRESULT CBlacklistDlg::OnUserMessageRefresh(WPARAM wParam, LPARAM lParam)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    // As the strings are referenced be sure to delete the list entries first
    m_Blacklist.UpdateData(FALSE);
    if (FALSE == m_Blacklist.DeleteAllItems()) THROW_WIN32(ERROR_INVALID_PARAMETER);

    // Get the black-listed devices
    auto const hidDeviceInstancePathsBlacklisted{ HidHide::GetBlacklist() };

    // Get the human interface devices and their associated model information
    m_BlacklistItemData = { HidHide::GetDescriptionToHidDeviceInstancePathsWithModelInfo() };

    // Fill the tree
    for (auto const& topLevelEntry : m_BlacklistItemData)
    {
        // Get the device instance path of its base container id (if present)
        auto const deviceInstancePathBaseContainer{ (topLevelEntry.second.empty() ? L"" : topLevelEntry.second.at(0).deviceInstancePathBaseContainer) };

        // Is the top-level entry on the black-list ?
        auto const topLevelEntryBlacklisted{ std::end(hidDeviceInstancePathsBlacklisted) != std::find(std::begin(hidDeviceInstancePathsBlacklisted), std::end(hidDeviceInstancePathsBlacklisted), deviceInstancePathBaseContainer) };

        // Is any of the child entries on the black-list ?
        auto const anyChildEntryBlacklisted{ std::end(topLevelEntry.second) != std::find_if(std::begin(topLevelEntry.second), std::end(topLevelEntry.second), [hidDeviceInstancePathsBlacklisted](HidHide::HidDeviceInstancePathWithModelInfo const& value)
        {
            return (std::end(hidDeviceInstancePathsBlacklisted) != std::find(std::begin(hidDeviceInstancePathsBlacklisted), std::end(hidDeviceInstancePathsBlacklisted), value.deviceInstancePath));
        }) };

        // Are all child entries on the black-list ?
        auto const allChildEntryBlacklisted{ std::end(topLevelEntry.second) == std::find_if_not(std::begin(topLevelEntry.second), std::end(topLevelEntry.second), [hidDeviceInstancePathsBlacklisted](HidHide::HidDeviceInstancePathWithModelInfo const& value)
        {
            return (std::end(hidDeviceInstancePathsBlacklisted) != std::find(std::begin(hidDeviceInstancePathsBlacklisted), std::end(hidDeviceInstancePathsBlacklisted), value.deviceInstancePath));
        }) };

        // Apply the filters only when the entry isn't black-listed
        if ((!topLevelEntryBlacklisted) && (!anyChildEntryBlacklisted))
        {
            // Skip the entry when gaming-only is selected and its not a gaming device
            if ((0 != (m_Gaming.GetCheck() & BST_CHECKED)) && (std::end(topLevelEntry.second) == std::find_if(std::begin(topLevelEntry.second), std::end(topLevelEntry.second), [](HidHide::HidDeviceInstancePathWithModelInfo const& value) { return (value.gamingDevice); })))
            {
                continue;
            }
            // Skip the entry when present-only is selected and the device isn't present
            if ((0 != (m_Filter.GetCheck() & BST_CHECKED)) && (std::end(topLevelEntry.second) == std::find_if(std::begin(topLevelEntry.second), std::end(topLevelEntry.second), [](HidHide::HidDeviceInstancePathWithModelInfo const& value) { return (value.present); })))
            {
                continue;
            }
        }

        // Add the top-level entry
        TVINSERTSTRUCTW tvInsert;
        tvInsert.item.mask             = (TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_STATE);
        tvInsert.hParent               = nullptr;
        tvInsert.hInsertAfter          = TVI_LAST;
        tvInsert.itemex.state          = ((topLevelEntryBlacklisted || allChildEntryBlacklisted) ? LVIS_STATE_CHECKBOX_CHECKED : LVIS_STATE_CHECKBOX_UNCHECKED);
        tvInsert.itemex.stateMask      = TVIS_USERMASK;
        tvInsert.itemex.iImage         = static_cast<int>((topLevelEntryBlacklisted || anyChildEntryBlacklisted) ? ICON_LOCK::ON : ICON_LOCK::OFF);
        tvInsert.itemex.iSelectedImage = static_cast<int>((topLevelEntryBlacklisted || anyChildEntryBlacklisted) ? ICON_LOCK::ON : ICON_LOCK::OFF);
        tvInsert.item.pszText          = const_cast<LPWSTR>(topLevelEntry.first.c_str());
        auto const hParent{ m_Blacklist.InsertItem(&tvInsert) };
        if (nullptr == hParent) THROW_WIN32(ERROR_INVALID_PARAMETER);

        // Add its children
        for (auto const& child : topLevelEntry.second)
        {
            auto const childEntryBlacklisted{ (std::end(hidDeviceInstancePathsBlacklisted) != std::find(std::begin(hidDeviceInstancePathsBlacklisted), std::end(hidDeviceInstancePathsBlacklisted), child.deviceInstancePath)) };
            tvInsert.hParent               = hParent;
            tvInsert.itemex.state          = ((topLevelEntryBlacklisted | childEntryBlacklisted) ? LVIS_STATE_CHECKBOX_CHECKED : LVIS_STATE_CHECKBOX_UNCHECKED);
            tvInsert.itemex.stateMask      = TVIS_USERMASK;
            tvInsert.itemex.iImage         = static_cast<int>(ICON_LOCK::BLANK);
            tvInsert.itemex.iSelectedImage = static_cast<int>(ICON_LOCK::BLANK);
            tvInsert.item.pszText = const_cast<LPWSTR>(child.usage.c_str());
            auto const hItem{ m_Blacklist.InsertItem(&tvInsert) };
            if (nullptr == hItem) THROW_WIN32(ERROR_INVALID_PARAMETER);
            if (FALSE == m_Blacklist.SetItemData(hItem, reinterpret_cast<DWORD_PTR>(&child))) THROW_WIN32(ERROR_INVALID_PARAMETER);
        }
    }

    m_Blacklist.SortChildren(TVI_ROOT);
    m_Blacklist.SelectItem(m_Blacklist.GetFirstVisibleItem());
    m_Blacklist.SetFocus();
    m_Blacklist.UpdateData(TRUE);

    return (0);
}

_Use_decl_annotations_
void CBlacklistDlg::OnTvnItemChangedTreeBlacklist(NMHDR* pNMHDR, LRESULT* pResult)
{
    TRACE_ALWAYS(L"");
    auto const& pNMTVItemChange{ *reinterpret_cast<NMTVITEMCHANGE*>(pNMHDR) };

    // Filter-out any event that doesn't relate to changes
    if (auto const key{ (pNMTVItemChange.uStateOld << 16) + pNMTVItemChange.uStateNew }; (0x10022002 != key) && (0x10002000 != key) && (0x10602060 != key) && (0x10622062 != key) && (0x20021002 != key) && (0x20001000 != key) && (0x20601060 != key) && (0x20621062 != key)) return;
    auto const checked { (LVIS_STATE_CHECKBOX_CHECKED == (LVIS_STATE_CHECKBOX_MASK & pNMTVItemChange.uStateNew)) };
    auto const hParent { m_Blacklist.GetParentItem(pNMTVItemChange.hItem) };
    auto const topLevel{ (nullptr == hParent) };

    // A top-level change is also applied to all of its children
    if (topLevel)
    {
        if (FALSE == m_Blacklist.SetItemImage(pNMTVItemChange.hItem, static_cast<int>(checked ? ICON_LOCK::ON : ICON_LOCK::OFF), static_cast<int>(checked ? ICON_LOCK::ON : ICON_LOCK::OFF))) THROW_WIN32(ERROR_INVALID_PARAMETER);
        for (auto hChild{ m_Blacklist.GetChildItem(pNMTVItemChange.hItem) }; (nullptr != hChild); hChild = m_Blacklist.GetNextSiblingItem(hChild))
        {
            if (FALSE == m_Blacklist.SetItemState(hChild, ((checked ? LVIS_STATE_CHECKBOX_CHECKED : LVIS_STATE_CHECKBOX_UNCHECKED) | (~LVIS_STATE_CHECKBOX_MASK & m_Blacklist.GetItemState(hChild, TVIS_USERMASK))), TVIS_USERMASK)) THROW_WIN32(ERROR_INVALID_PARAMETER);
        }
    }

    // When the child-level is checked and all other child-levels are also checked then check the top-level parent
    if ((!topLevel) && (checked))
    {
        auto allChecked{ true };
        for (auto hChild{ m_Blacklist.GetChildItem(hParent) }; (nullptr != hChild); hChild = m_Blacklist.GetNextSiblingItem(hChild))
        {
            if (LVIS_STATE_CHECKBOX_CHECKED != (LVIS_STATE_CHECKBOX_MASK & m_Blacklist.GetItemState(hChild, TVIS_USERMASK)))
            {
                allChecked = false;
                break;
            }
        }
        if ((allChecked) && (FALSE == m_Blacklist.SetItemImage(hParent, static_cast<int>(ICON_LOCK::ON), static_cast<int>(ICON_LOCK::ON)))) THROW_WIN32(ERROR_INVALID_PARAMETER);
        if ((allChecked) && (FALSE == m_Blacklist.SetItemState(hParent, (LVIS_STATE_CHECKBOX_CHECKED | (~LVIS_STATE_CHECKBOX_MASK & m_Blacklist.GetItemState(hParent, TVIS_USERMASK))), TVIS_USERMASK))) THROW_WIN32(ERROR_INVALID_PARAMETER);
    }

    // When the child-level is unchecked then uncheck the top-level parent
    if ((!topLevel) && (!checked))
    {
        if (FALSE == m_Blacklist.SetItemImage(hParent, static_cast<int>(ICON_LOCK::OFF), static_cast<int>(ICON_LOCK::OFF))) THROW_WIN32(ERROR_INVALID_PARAMETER);
        if (FALSE == m_Blacklist.SetItemState(hParent, (LVIS_STATE_CHECKBOX_UNCHECKED | (~LVIS_STATE_CHECKBOX_MASK & m_Blacklist.GetItemState(hParent, TVIS_USERMASK))), TVIS_USERMASK)) THROW_WIN32(ERROR_INVALID_PARAMETER);
    }

    // Construct the new black-list for the filter driver
    HidHide::HidDeviceInstancePaths hidDeviceInstancePaths;
    for (auto hItem{ m_Blacklist.GetRootItem() }; (nullptr != hItem); hItem = m_Blacklist.GetNextItem(hItem, TVGN_NEXT))
    {
        // When we have a top-level entry and explore the option for blocking the base container device
        // This is only valid when (a) there is a base container and (b) all of its child devices are human interface devices and (c) the filter driver is installed on its stack
        if (LVIS_STATE_CHECKBOX_CHECKED == (LVIS_STATE_CHECKBOX_MASK & m_Blacklist.GetItemState(hItem, TVIS_USERMASK)))
        {
            // Get the base container device details (just take it from any of the child devices)
            // Note that the class guid will be GUID_NULL when this is a stand-alone device
            std::wstring deviceInstancePathBaseContainer;
            GUID         deviceInstancePathBaseContainerClassGuid{};
            size_t       deviceInstancePathBaseContainerDeviceCount{};
            if (auto const hFirstChild{ m_Blacklist.GetChildItem(hItem) }; (nullptr != hFirstChild))
            {
                auto const firstChilditemData{ reinterpret_cast<HidHide::HidDeviceInstancePathWithModelInfo*>(m_Blacklist.GetItemData(hFirstChild)) };
                if (nullptr == firstChilditemData) THROW_WIN32(ERROR_INVALID_PARAMETER);
                deviceInstancePathBaseContainer            = firstChilditemData->deviceInstancePathBaseContainer;
                deviceInstancePathBaseContainerClassGuid   = firstChilditemData->deviceInstancePathBaseContainerClassGuid;
                deviceInstancePathBaseContainerDeviceCount = firstChilditemData->deviceInstancePathBaseContainerDeviceCount;
            }

            // When there is a base container device then can be block it or is it also providing non human interface devices?
            // Note that the result will be false when this is a stand-alone device
            auto theBaseContainerDeviceCanBeBlocked{ false };
            if ((GUID_DEVCLASS_HIDCLASS == deviceInstancePathBaseContainerClassGuid) || (GUID_DEVCLASS_XUSBCLASS == deviceInstancePathBaseContainerClassGuid))
            {
                size_t humanInterfaceDeviceCount{};
                for (auto hChild{ m_Blacklist.GetChildItem(hItem) }; (nullptr != hChild); hChild = m_Blacklist.GetNextSiblingItem(hChild)) humanInterfaceDeviceCount++;
                theBaseContainerDeviceCanBeBlocked = (humanInterfaceDeviceCount == deviceInstancePathBaseContainerDeviceCount);
            }

            // When we can block the base container device then do so
            if (theBaseContainerDeviceCanBeBlocked)
            {
                hidDeviceInstancePaths.emplace_back(deviceInstancePathBaseContainer);
            }
        }

        // Block the individual child devices
        for (auto hChild{ m_Blacklist.GetChildItem(hItem) }; (nullptr != hChild); hChild = m_Blacklist.GetNextSiblingItem(hChild))
        {
            if (LVIS_STATE_CHECKBOX_CHECKED == (LVIS_STATE_CHECKBOX_MASK & m_Blacklist.GetItemState(hChild, TVIS_USERMASK)))
            {
                auto const childItemData{ reinterpret_cast<HidHide::HidDeviceInstancePathWithModelInfo*>(m_Blacklist.GetItemData(hChild)) };
                if (nullptr == childItemData) THROW_WIN32(ERROR_INVALID_PARAMETER);
                hidDeviceInstancePaths.emplace_back(childItemData->deviceInstancePath);
            }
        }
    }

    // Forward the new selection to the filter driver
    HidHide::SetBlacklist(hidDeviceInstancePaths);
    *pResult = 0;
}

void CBlacklistDlg::OnBnClickedCheckFilter()
{
    TRACE_ALWAYS(L"");
    Refresh();
}

void CBlacklistDlg::OnBnClickedCheckGaming()
{
    TRACE_ALWAYS(L"");
    Refresh();
}

void CBlacklistDlg::OnBnClickedCheckEnable()
{
    TRACE_ALWAYS(L"");
    HidHide::SetActive(0 != (m_Enable.GetCheck() & BST_CHECKED));
}
