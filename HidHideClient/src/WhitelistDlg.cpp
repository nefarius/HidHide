// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// WhitelistDlg.cpp
#include "stdafx.h"
#include "WhitelistDlg.h"
#include "HidHideClientDlg.h"
#include "Utils.h"
#include "Volume.h"
#include "Logging.h"

// Define user-message for processing device interface arrivals
constexpr auto WM_USER_CM_NOTIFICATION_REFRESH{ WM_USER + 1 };

namespace
{
    // Capture the list box content as a set of paths
    HidHide::FullImageNames ListBoxToPathSet(_In_ CListBox const& listBox)
    {
        TRACE_ALWAYS(L"");
        HidHide::FullImageNames result;

        for (int index{}, size(listBox.GetCount()); (index < size); index++)
        {
            CString value;
            listBox.GetText(index, value);
            result.emplace(value.GetBuffer());
        }

        return (result);
    }
}

IMPLEMENT_DYNAMIC(CWhitelistDlg, CDialogEx)

#pragma warning(push)
#pragma warning(disable: 26454 28213) // Warnings caused by Microsoft MFC macros
BEGIN_MESSAGE_MAP(CWhitelistDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_WHITELIST_DELETE,  &CWhitelistDlg::OnBnClickedButtonWhitelistDelete)
    ON_BN_CLICKED(IDC_BUTTON_WHITELIST_INSERT,  &CWhitelistDlg::OnBnClickedButtonWhitelistInsert)
    ON_BN_CLICKED(IDC_CHECK_WHITELIST_INVERSE,  &CWhitelistDlg::OnBnClickedCheckInverse)
    ON_MESSAGE(WM_USER_CM_NOTIFICATION_REFRESH, &CWhitelistDlg::OnUserMessageRefresh)
    ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()
#pragma warning(pop)

_Use_decl_annotations_
CWhitelistDlg::CWhitelistDlg(CHidHideClientDlg& hidHideClientDlg, CWnd* pParent)
    : CDialogEx(IDD_DIALOG_WHITELIST, pParent)
    , HidHide::IDropTarget()
    , m_HidHideClientDlg{ hidHideClientDlg }
    , m_DropTargetFullImageNames{}
    , m_Whitelist{}
    , m_Guidance{}
    , m_Insert{}
    , m_Delete{}
    , m_Inverse{}
{
    TRACE_ALWAYS(L"");
}

CWhitelistDlg::~CWhitelistDlg()
{
    TRACE_ALWAYS(L"");
}

HidHide::FilterDriverProxy& CWhitelistDlg::FilterDriverProxy() noexcept
{
    return (m_HidHideClientDlg.FilterDriverProxy());
}

_Use_decl_annotations_
DROPEFFECT CWhitelistDlg::OnDragEnter(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(pWnd);
    UNREFERENCED_PARAMETER(point);

    // Flush the previous list
    m_DropTargetFullImageNames.clear();

    // Only accept one or more application files (.exe, .com, .bin) located on a volume
    for (auto const& fullyQualifiedFileName : HidHide::DragTargetFileNames(pDataObject))
    {
        if (HidHide::FileIsAnApplication(fullyQualifiedFileName))
        {
            if (auto const fullImageName{ HidHide::FileNameToFullImageName(fullyQualifiedFileName) }; !fullImageName.empty())
            {
                // Criteria met proceed to next one
                m_DropTargetFullImageNames.emplace(fullImageName);
                continue;
            }
        }

        // Criteria not met then flush the result and quit the loop
        m_DropTargetFullImageNames.clear();
        break;
    }

    // Change mouse shape when a copy operation is initiated (left mouse button with or without control pressed) and the critiria are met
    return ((HidHide::DragTargetCopyOperation(dwKeyState) && (!m_DropTargetFullImageNames.empty())) ? DROPEFFECT_COPY : DROPEFFECT_NONE);
}

_Use_decl_annotations_
DROPEFFECT CWhitelistDlg::OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
{
    TRACE_PERFORMANCE(L"");
    UNREFERENCED_PARAMETER(pWnd);
    UNREFERENCED_PARAMETER(pDataObject);
    UNREFERENCED_PARAMETER(dwKeyState);

    // Change mouse shape when a copy operation is initiated (left mouse button with or without control pressed) and the critiria are met and the mouse hovers above the whitelist
    return ((HidHide::DragTargetCopyOperation(dwKeyState) && (!m_DropTargetFullImageNames.empty()) && (MousePointerAtWhitelist(point))) ? DROPEFFECT_COPY : DROPEFFECT_NONE);
}

_Use_decl_annotations_
DROPEFFECT CWhitelistDlg::OnDropEx(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropDefault, DROPEFFECT dropList, CPoint point)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(pWnd);
    UNREFERENCED_PARAMETER(pDataObject);
    UNREFERENCED_PARAMETER(dropDefault);
    UNREFERENCED_PARAMETER(dropList);

    if ((m_DropTargetFullImageNames.empty()) || (!MousePointerAtWhitelist(point))) return (DROPEFFECT_NONE);

    // Process all dropped file names and keep track if they are new to the white list
    auto dirty{ false };
    auto whitelist{ FilterDriverProxy().GetWhitelist() };
    for (auto const& fullImageName : m_DropTargetFullImageNames)
    {
        if (whitelist.emplace(fullImageName).second) dirty = true;
    }

    // When there are new entries then update the whitelist accordingly and refresh the screen
    if (dirty)
    {
        FilterDriverProxy().SetWhitelist(whitelist);
        Refresh();
    }

    return (DROPEFFECT_COPY);
}

_Use_decl_annotations_
bool CWhitelistDlg::MousePointerAtWhitelist(CPoint point)
{
    TRACE_PERFORMANCE(L"");
    CRect rect{};
    GetDlgItem(IDC_LIST_WHITELIST)->GetWindowRect(&rect);
    GetParent()->ScreenToClient(&rect);
    return (FALSE != rect.PtInRect(point));
}

_Use_decl_annotations_
void CWhitelistDlg::DoDataExchange(CDataExchange* pDX)
{
    TRACE_ALWAYS(L"");
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_WHITELIST,            m_Whitelist);
    DDX_Control(pDX, IDC_STATIC_WHITELIST_GUIDANCE, m_Guidance);
    DDX_Control(pDX, IDC_BUTTON_WHITELIST_INSERT,   m_Insert);
    DDX_Control(pDX, IDC_BUTTON_WHITELIST_DELETE,   m_Delete);
    DDX_Control(pDX, IDC_CHECK_WHITELIST_INVERSE,   m_Inverse);
}

BOOL CWhitelistDlg::OnInitDialog()
{
    TRACE_ALWAYS(L"");
    CDialogEx::OnInitDialog();

    // Apply the labels from the string table
    m_Guidance.SetWindowTextW(HidHide::StringTable(IDS_STATIC_WHITELIST_GUIDANCE).c_str());
    m_Insert.SetWindowTextW(HidHide::StringTable(IDS_BUTTON_WHITELIST_INSERT).c_str());
    m_Delete.SetWindowTextW(HidHide::StringTable(IDS_BUTTON_WHITELIST_DELETE).c_str());
    m_Inverse.SetWindowTextW(HidHide::StringTable(IDS_CHECK_WHITELIST_INVERSE).c_str());

    // Reflect the current state in the check-box
    m_Inverse.SetCheck(FilterDriverProxy().GetInverse() ? BST_CHECKED : BST_UNCHECKED);

    return (TRUE);
}

_Use_decl_annotations_
void CWhitelistDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
    TRACE_ALWAYS(L"");
    CDialogEx::OnShowWindow(bShow, nStatus);
    Refresh();
}

void CWhitelistDlg::Refresh()
{
    TRACE_ALWAYS(L"");
    PostMessageW(WM_USER_CM_NOTIFICATION_REFRESH, 0, NULL);
}

_Use_decl_annotations_
LRESULT CWhitelistDlg::OnUserMessageRefresh(WPARAM wParam, LPARAM lParam)
{
    TRACE_ALWAYS(L"");
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    m_Whitelist.UpdateData(FALSE);
    m_Whitelist.ResetContent();
    m_Whitelist.SetSel(-1, FALSE);
    for (auto const& fullImageName : FilterDriverProxy().GetWhitelist())
    {
        auto const index{ m_Whitelist.AddString(fullImageName.c_str()) };
        if ((LB_ERR == index) || (LB_ERRSPACE == index)) THROW_WIN32(ERROR_INVALID_PARAMETER);

        // Mask the entry in the list when the file doesn't exist anymore
        auto const exists{ std::filesystem::exists(HidHide::FullImageNameToFileName(fullImageName)) };
        if (!exists) m_Whitelist.SetSel(index, TRUE);
    }
    m_Whitelist.SetFocus();
    m_Whitelist.UpdateData(TRUE);
    return (0);
}

void CWhitelistDlg::OnBnClickedButtonWhitelistInsert()
{
    TRACE_ALWAYS(L"");

    // Clear current selection
    if (LB_ERR == m_Whitelist.SetSel(-1, FALSE)) THROW_WIN32(ERROR_INVALID_PARAMETER);

    // Ask the user to pin-point a specific file
    CFileDialog fileDlg(TRUE, L"exe", nullptr, (OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST), HidHide::StringTable(IDS_STATIC_FILE_OPEN_FILTER).c_str());
    auto const title{ HidHide::StringTable(IDS_DIALOG_FILE_OPEN) };
    fileDlg.m_pOFN->lpstrTitle = title.c_str();
    if (IDOK == fileDlg.DoModal())
    {
        // Convert the file name into a full image name
        CString fullImageName{ HidHide::FileNameToFullImageName(fileDlg.GetPathName().GetString()).c_str() };

        // Avoid duplicate entries
        for (int index{}, size(m_Whitelist.GetCount()); (index < size); index++)
        {
            CString value;
            m_Whitelist.GetText(index, value);
            if (fullImageName == value) return;
        }

        // No duplicates so add it
        if (auto const result{ m_Whitelist.AddString(fullImageName) }; (LB_ERR == result) || (LB_ERRSPACE == result)) THROW_WIN32(ERROR_INVALID_PARAMETER);

        FilterDriverProxy().SetWhitelist(ListBoxToPathSet(m_Whitelist));
        Refresh();
    }
}

void CWhitelistDlg::OnBnClickedButtonWhitelistDelete()
{
    TRACE_ALWAYS(L"");

    // Get the array of selected items
    auto const size{ m_Whitelist.GetSelCount() };
    CArray<int, int> itemsSelected;
    itemsSelected.SetSize(size);
    if (LB_ERR == m_Whitelist.GetSelItems(size, itemsSelected.GetData())) THROW_WIN32(ERROR_INVALID_PARAMETER);

    // Iterate the array of selected items and remove the entries marked
    for (int index{ size }; (index > 0);)
    {
        index--;
        CString value;
        if (LB_ERR == m_Whitelist.DeleteString(itemsSelected[index])) THROW_WIN32(ERROR_INVALID_PARAMETER);
    }

    FilterDriverProxy().SetWhitelist(ListBoxToPathSet(m_Whitelist));
    Refresh();
}

void CWhitelistDlg::OnBnClickedCheckInverse()
{
    TRACE_ALWAYS(L"");
    FilterDriverProxy().SetInverse(0 != (m_Inverse.GetCheck() & BST_CHECKED));
}
