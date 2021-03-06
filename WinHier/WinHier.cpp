// WinHier.cpp
// Copyright (C) 2018-2019 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>.
// This file is public domain software.

#include "targetver.h"
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_WCSTOK
#define MWINDOWTREE_ADJUST_OWNER
#include <shlwapi.h>
#include "MWindowTreeView.hpp"
#include "MResizable.hpp"
#include "ConstantsDB.hpp"
#include "MSelectWnd.hpp"
#include "resource.h"

typedef BOOL (WINAPI *ISWOW64PROCESS)(HANDLE, PBOOL);

BOOL IsWow64(HANDLE hProcess)
{
    HINSTANCE hKernel32 = GetModuleHandleA("kernel32");
    ISWOW64PROCESS pIsWow64Process;
    pIsWow64Process = (ISWOW64PROCESS)GetProcAddress(hKernel32, "IsWow64Process");
    if (pIsWow64Process)
    {
        BOOL ret = FALSE;
        (*pIsWow64Process)(hProcess, &ret);
        return ret;
    }
    return FALSE;
}

BOOL EnableProcessPriviledge(LPCTSTR pszSE_)
{
    BOOL f;
    HANDLE hProcess;
    HANDLE hToken;
    LUID luid;
    TOKEN_PRIVILEGES tp;
    
    f = FALSE;
    hProcess = GetCurrentProcess();
    if (OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES, &hToken))
    {
        if (LookupPrivilegeValue(NULL, pszSE_, &luid))
        {
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            tp.Privileges[0].Luid = luid;
            f = AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL);
        }
        CloseHandle(hToken);
    }
    return f;
}

BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    MWindowTreeView::node_type *node = (MWindowTreeView::node_type *)lParam;

    TCHAR szText[128];

    if (node->m_type == MWindowTreeNode::WINDOW)
    {
        StringCbPrintf(szText, sizeof(szText), TEXT("%p"), node->m_hwndTarget);
        SetDlgItemText(hwnd, edt1, szText);

        StringCbPrintf(szText, sizeof(szText), TEXT("%s"), node->m_szClass);
        SetDlgItemText(hwnd, edt2, szText);

        StringCbPrintf(szText, sizeof(szText), TEXT("%s"), node->m_szText);
        SetDlgItemText(hwnd, edt3, szText);

        DWORD style = node->m_style;
        std::wstring strStyle = mstr_hex(node->m_style);
        std::wstring bits = g_db.DumpBitField(node->m_szClass, L"STYLE", style);
        strStyle += L" (";
        strStyle += bits;
        strStyle += L")";
        SetDlgItemText(hwnd, edt4, strStyle.c_str());

        DWORD exstyle = node->m_exstyle;
        std::wstring strExStyle = mstr_hex(node->m_exstyle);
        strExStyle += L" (";
        strExStyle += g_db.DumpBitField(L"EXSTYLE", exstyle);
        strExStyle += L")";
        SetDlgItemText(hwnd, edt5, strExStyle.c_str());

        StringCbPrintf(szText, sizeof(szText), TEXT("%p"), node->m_hwndOwner);
        SetDlgItemText(hwnd, edt6, szText);

        StringCbPrintf(szText, sizeof(szText), TEXT("%p"), node->m_hwndParent);
        SetDlgItemText(hwnd, edt7, szText);

        StringCbPrintf(szText, sizeof(szText), TEXT("%p"), node->m_hwndFirstChild);
        SetDlgItemText(hwnd, edt8, szText);

        StringCbPrintf(szText, sizeof(szText), TEXT("%p"), node->m_hwndLastChild);
        SetDlgItemText(hwnd, edt9, szText);

        StringCbPrintf(szText, sizeof(szText), TEXT("(%ld, %ld) - (%ld, %ld)"),
                       node->m_rcWnd.left,
                       node->m_rcWnd.top,
                       node->m_rcWnd.right,
                       node->m_rcWnd.bottom);
        SetDlgItemText(hwnd, edt11, szText);

        StringCbPrintf(szText, sizeof(szText), TEXT("(%ld, %ld) - (%ld, %ld)"),
                       node->m_rcClient.left,
                       node->m_rcClient.top,
                       node->m_rcClient.right,
                       node->m_rcClient.bottom);
        SetDlgItemText(hwnd, edt12, szText);
    }

    StringCbPrintf(szText, sizeof(szText), TEXT("%p"), node->m_id);
    SetDlgItemText(hwnd, edt10, szText);

    return TRUE;
}

void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
    case IDOK:
    case IDCANCEL:
        EndDialog(hwnd, id);
        break;
    }
}

INT_PTR CALLBACK
PropDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
    }
    return 0;
}

class WinHier : public MDialogBase
{
public:
    HINSTANCE   m_hInst;        // the instance handle
    HICON       m_hIcon;        // the main icon
    HICON       m_hIconSm;      // the small icon
    HWND                m_hwndSelected;
    MWindowTreeView     m_ctl1;
    MResizable          m_resizable;
    SELECTWNDDATA       m_selection;
    MSelectWndIconWnd   m_ico1;

    WinHier(HINSTANCE hInst)
        : MDialogBase(IDD_MAIN), m_hInst(hInst),
          m_hIcon(NULL), m_hIconSm(NULL), m_hwndSelected(NULL),
          m_ico1(m_selection)
    {
        m_hIcon = LoadIconDx(IDI_MAIN);
        m_hIconSm = LoadSmallIconDx(IDI_MAIN);
    }

    virtual ~WinHier()
    {
        DestroyIcon(m_hIcon);
        DestroyIcon(m_hIconSm);
    }

    BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        EnableProcessPriviledge(SE_DEBUG_NAME);

        SubclassChildDx(m_ctl1, ctl1);

        WCHAR *pch, szPath[MAX_PATH];
        GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath));
        pch = PathFindFileName(szPath);
        if (pch && *pch)
            *pch = 0;
        PathAppend(szPath, L"Constants.txt");

        if (!g_db.LoadFromFile(szPath))
        {
            if (pch && *pch)
                *pch = 0;
            PathAppend(szPath, L"..\\Constants.txt");
            if (!g_db.LoadFromFile(szPath))
            {
                if (pch && *pch)
                    *pch = 0;
                PathAppend(szPath, L"..\\..\\Constants.txt");
                if (!g_db.LoadFromFile(szPath))
                {
                    MessageBoxW(NULL, L"Unable to load Constants.txt file", NULL,
                                MB_ICONERROR);
                    DestroyWindow(hwnd);
                    return TRUE;
                }
            }
        }

        if (HINSTANCE hinstUXTheme = LoadLibrary(TEXT("UXTHEME")))
        {
            typedef HRESULT (WINAPI *SETWINDOWTHEME)(HWND, LPCWSTR, LPCWSTR);
            SETWINDOWTHEME pSetWindowTheme =
                (SETWINDOWTHEME)GetProcAddress(hinstUXTheme, "SetWindowTheme");

            if (pSetWindowTheme)
            {
                // apply Explorer's visual style
                (*pSetWindowTheme)(m_ctl1, L"Explorer", NULL);
            }

            FreeLibrary(hinstUXTheme);
        }

        SendDlgItemMessage(hwnd, cmb1, CB_ADDSTRING, 0, (LPARAM)TEXT("Desktop-Tree"));
        SendDlgItemMessage(hwnd, cmb1, CB_ADDSTRING, 0, (LPARAM)TEXT("Process-Window"));
        SendDlgItemMessage(hwnd, cmb1, CB_ADDSTRING, 0, (LPARAM)TEXT("Process-Thread"));
        SendDlgItemMessage(hwnd, cmb1, CB_SETCURSEL, 0, 0);

        SendMessageDx(WM_SETICON, ICON_BIG, LPARAM(m_hIcon));
        SendMessageDx(WM_SETICON, ICON_SMALL, LPARAM(m_hIconSm));

        SubclassChildDx(m_ico1, ico1);
        m_ico1.SetTargetIcon(m_ico1);

        // adjust size
        RECT rc;
        SetRect(&rc, 0, 0, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
        AdjustWindowRectEx(&rc, GetWindowLong(m_ico1, GWL_STYLE),
            FALSE, GetWindowLong(m_ico1, GWL_EXSTYLE));
        SetWindowPos(m_ico1, NULL, 0, 0,
            rc.right - rc.left, rc.bottom - rc.top,
            SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

        m_resizable.OnParentCreate(hwnd);

        m_resizable.SetLayoutAnchor(ctl1, mzcLA_TOP_LEFT, mzcLA_BOTTOM_RIGHT);
        m_resizable.SetLayoutAnchor(psh1, mzcLA_BOTTOM_LEFT);
        m_resizable.SetLayoutAnchor(edt1, mzcLA_BOTTOM_LEFT);
        m_resizable.SetLayoutAnchor(stc1, mzcLA_BOTTOM_LEFT);
        m_resizable.SetLayoutAnchor(ico1, mzcLA_BOTTOM_LEFT);
        m_resizable.SetLayoutAnchor(stc2, mzcLA_BOTTOM_RIGHT);
        m_resizable.SetLayoutAnchor(stc3, mzcLA_BOTTOM_RIGHT);
        m_resizable.SetLayoutAnchor(edt2, mzcLA_BOTTOM_RIGHT);
        m_resizable.SetLayoutAnchor(psh2, mzcLA_BOTTOM_RIGHT);
        m_resizable.SetLayoutAnchor(psh3, mzcLA_BOTTOM_RIGHT);
        m_resizable.SetLayoutAnchor(cmb1, mzcLA_BOTTOM_RIGHT);
        m_resizable.SetLayoutAnchor(IDOK, mzcLA_BOTTOM_RIGHT);
        m_resizable.SetLayoutAnchor(IDCANCEL, mzcLA_BOTTOM_RIGHT);

        m_ctl1.set_style(MWTVS_DESKTOPTREE);
        m_ctl1.refresh();

        SetFocus(GetDlgItem(hwnd, psh1));
        return TRUE;
    }

    void OnSize(HWND hwnd, UINT state, int cx, int cy)
    {
        m_resizable.OnSize();
    }

    void OnTimer(HWND hwnd, UINT id)
    {
        KillTimer(hwnd, id);
        m_ctl1.refresh();
    }

    void OnPsh1(HWND hwnd)
    {
        INT i = (INT)SendDlgItemMessage(hwnd, cmb1, CB_GETCURSEL, 0, 0);
        if (i == CB_ERR)
            return;

        switch (i)
        {
        case 0:
            m_ctl1.set_style(MWTVS_DESKTOPTREE);
            break;
        case 1:
            m_ctl1.set_style(MWTVS_PROCESSWINDOW);
            break;
        case 2:
            m_ctl1.set_style(MWTVS_PROCESSTHREAD);
            break;
        }

        UINT uSeconds = GetDlgItemInt(hwnd, edt1, NULL, FALSE);
        SetTimer(hwnd, 999, 1000 * uSeconds, NULL);
    }

    void OnPsh2(HWND hwnd)
    {
        WCHAR szText[128];
        GetDlgItemTextW(hwnd, edt2, szText, ARRAYSIZE(szText));

        mstr_trim(szText, L" \t");

        std::vector<HTREEITEM> found;
        m_ctl1.find_text(found, szText);
        if (found.empty())
            return;

        HTREEITEM hItem = m_ctl1.GetSelectedItem();
        HTREEITEM hThisOne = NULL;
        for (size_t i = 1; i < found.size(); ++i)
        {
            if (found[i] == NULL || found[i] == hItem)
            {
                hThisOne = found[i - 1];
                break;
            }
        }

        if (hThisOne == NULL)
            hThisOne = found[found.size() - 1];

        m_ctl1.SelectItem(hThisOne);
        SetFocus(m_ctl1);
    }

    void OnPsh3(HWND hwnd)
    {
        WCHAR szText[128];
        GetDlgItemTextW(hwnd, edt2, szText, ARRAYSIZE(szText));

        mstr_trim(szText, L" \t");

        std::vector<HTREEITEM> found;
        m_ctl1.find_text(found, szText);
        if (found.empty())
            return;

        HTREEITEM hItem = m_ctl1.GetSelectedItem();
        HTREEITEM hThisOne = NULL;
        for (size_t i = 0; i < found.size() - 1; ++i)
        {
            if (found[i] == NULL || found[i] == hItem)
            {
                hThisOne = found[i + 1];
                break;
            }
        }

        if (hThisOne == NULL)
            hThisOne = found[0];

        m_ctl1.SelectItem(hThisOne);
        SetFocus(m_ctl1);
    }

    void OnCmb1(HWND hwnd)
    {
        INT i = (INT)SendDlgItemMessage(hwnd, cmb1, CB_GETCURSEL, 0, 0);
        if (i == CB_ERR)
            return;

        switch (i)
        {
        case 0:
            m_ctl1.set_style(MWTVS_DESKTOPTREE);
            break;
        case 1:
            m_ctl1.set_style(MWTVS_PROCESSWINDOW);
            break;
        case 2:
            m_ctl1.set_style(MWTVS_PROCESSTHREAD);
            break;
        }
        m_ctl1.refresh();
    }

    void OnProp(HWND hwnd)
    {
        HTREEITEM hItem = TreeView_GetSelection(m_ctl1);
        MWindowTreeView::node_type *node = m_ctl1.NodeFromItem(hItem);
        if (node)
        {
            DialogBoxParam(m_hInst, MAKEINTRESOURCE(IDD_PROPERTIES), hwnd,
                           PropDialogProc, (LPARAM)node);
        }
    }

    void OnMessages(HWND hwnd)
    {
        HTREEITEM hItem = TreeView_GetSelection(m_ctl1);
        HWND hwndTarget = m_ctl1.WindowFromItem(hItem);
        if (IsWindow(hwndTarget))
        {
            HINSTANCE hinst = GetWindowInstance(hwndTarget);
            DWORD pid = 0;
            GetWindowThreadProcessId(hwndTarget, &pid);

            TCHAR szPath[MAX_PATH], *pch;
            GetModuleFileName(hinst, szPath, ARRAYSIZE(szPath));
            pch = PathFindFileName(szPath);
            *pch = 0;

            BOOL bIsWow64 = FALSE;
            if (HANDLE hProcess = OpenProcess(GENERIC_READ, TRUE, pid))
            {
                if (IsWow64(hProcess))
                {
                    bIsWow64 = TRUE;
                }
                CloseHandle(hProcess);
            }

            DWORD dwBinType;
            if (bIsWow64)
            {
                PathAppend(szPath, TEXT("MsgGetter32.exe"));
            }
            else if (GetBinaryType(szPath, &dwBinType) && dwBinType == SCS_32BIT_BINARY)
            {
                PathAppend(szPath, TEXT("MsgGetter32.exe"));
            }
            else
            {
                PathAppend(szPath, TEXT("MsgGetter64.exe"));
            }

            TCHAR szText[64];
            StringCbPrintf(szText, sizeof(szText), TEXT("0x%p"), hwndTarget);

            ShellExecute(hwnd, NULL, szPath, szText, NULL, SW_SHOWNORMAL);
        }
    }

    void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        switch (id)
        {
        case IDCANCEL:
            KillTimer(hwnd, 999);
            EndDialog(IDCANCEL);
            break;
        case psh1:
            OnPsh1(hwnd);
            break;
        case psh2:
            OnPsh2(hwnd);
            break;
        case psh3:
            OnPsh3(hwnd);
            break;
        case cmb1:
            switch (codeNotify)
            {
            case CBN_SELENDOK:
                OnCmb1(hwnd);
                break;
            }
            break;
        case ID_PROP:
            OnProp(hwnd);
            break;
        case ID_MESSAGES:
            OnMessages(hwnd);
            break;
        case ID_WINDOW_CHOOSED:
            m_ctl1.refresh();
            m_ctl1.select_hwnd(m_ico1.GetSelectedWindow());
            break;
        }
    }

    void OnContextMenu(HWND hwnd, HWND hwndContext, UINT xPos, UINT yPos)
    {
        TV_HITTESTINFO hittest;
        hittest.pt.x = xPos;
        hittest.pt.y = yPos;
        ScreenToClient(m_ctl1, &hittest.pt);
        TreeView_HitTest(m_ctl1, &hittest);
        TreeView_SelectItem(m_ctl1, hittest.hItem);

        HMENU hMenu = LoadMenu(m_hInst, MAKEINTRESOURCE(1));
        HMENU hSubMenu = GetSubMenu(hMenu, 0);

        SetForegroundWindow(hwnd);
        INT nCmd = TrackPopupMenu(hSubMenu, TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_RETURNCMD,
            xPos, yPos, 0, hwnd, NULL);
        DestroyMenu(hMenu);
        PostMessage(hwnd, WM_COMMAND, nCmd, 0);
    }

    virtual INT_PTR CALLBACK
    DialogProcDx(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
        HANDLE_MSG(hwnd, WM_INITDIALOG, OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
        HANDLE_MSG(hwnd, WM_SIZE, OnSize);
        HANDLE_MSG(hwnd, WM_TIMER, OnTimer);
        HANDLE_MSG(hwnd, WM_CONTEXTMENU, OnContextMenu);
        default:
            return DefaultProcDx();
        }
    }

    BOOL StartDx(INT nCmdShow)
    {
        return TRUE;
    }

    INT RunDx()
    {
        DialogBoxDx(NULL);
        return 0;
    }
};

//////////////////////////////////////////////////////////////////////////////
// Win32 main function

extern "C"
INT APIENTRY WinMain(
    HINSTANCE   hInstance,
    HINSTANCE   hPrevInstance,
    LPSTR       lpCmdLine,
    INT         nCmdShow)
{
    int ret = -1;

    {
        WinHier app(hInstance);

        ::InitCommonControls();

        if (app.StartDx(nCmdShow))
        {
            ret = app.RunDx();
        }
    }

#if (WINVER >= 0x0500)
    HANDLE hProcess = GetCurrentProcess();
    DebugPrintDx(TEXT("Count of GDI objects: %ld\n"),
                 GetGuiResources(hProcess, GR_GDIOBJECTS));
    DebugPrintDx(TEXT("Count of USER objects: %ld\n"),
                 GetGuiResources(hProcess, GR_USEROBJECTS));
#endif

#if defined(_MSC_VER) && !defined(NDEBUG)
    // for detecting memory leak (MSVC only)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    return ret;
}

//////////////////////////////////////////////////////////////////////////////
