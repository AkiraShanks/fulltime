#include <afxwin.h>
#include <afxdlgs.h>
#include <windows.h>
#include <shellapi.h>
#include <random>
#include <algorithm>
#include <cmath>
#include "resource.h"

#define ID_TIMER_MOVE 1001
#define HOTKEY_ON 4001
#define HOTKEY_OFF 4002

class CMainDlg : public CDialog
{
public:
    CMainDlg() : CDialog(IDD_MAIN), m_running(false), m_trayAdded(false) {}

protected:
    bool m_running;
    bool m_trayAdded;
    NOTIFYICONDATAW m_nid{};
    POINT m_target{ 0, 0 };
    bool m_hasTarget = false;

    BOOL OnInitDialog() override
    {
        CDialog::OnInitDialog();
        SetWindowTextW(L"Explore");
        HICON icon = (HICON)::LoadImageW(AfxGetInstanceHandle(),
            MAKEINTRESOURCEW(IDI_ICON1), IMAGE_ICON, 32, 32, LR_DEFAULTSIZE);
        SetIcon(icon, TRUE);
        SetIcon(icon, FALSE);
        RegisterHotKey(m_hWnd, HOTKEY_ON, MOD_ALT | MOD_NOREPEAT, 'Q');
        RegisterHotKey(m_hWnd, HOTKEY_OFF, MOD_ALT | MOD_NOREPEAT, 'W');
        AddTrayIcon();
        ShowWindow(SW_HIDE);
        return TRUE;
    }

    void AddTrayIcon()
    {
        m_nid.cbSize = sizeof(NOTIFYICONDATAW);
        m_nid.hWnd = m_hWnd;
        m_nid.uID = 1;
        m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        m_nid.uCallbackMessage = WM_TRAYICON;
        m_nid.hIcon = (HICON)::LoadImageW(AfxGetInstanceHandle(),
            MAKEINTRESOURCEW(IDI_ICON1), IMAGE_ICON, 16, 16, LR_DEFAULTSIZE);
        wcscpy_s(m_nid.szTip, L"Explore - Alt+Q On / Alt+W Off");
        m_trayAdded = Shell_NotifyIconW(NIM_ADD, &m_nid) == TRUE;
    }

    void RemoveTrayIcon()
    {
        if (m_trayAdded)
        {
            Shell_NotifyIconW(NIM_DELETE, &m_nid);
            m_trayAdded = false;
        }
    }

    void SetDisplayAwake(bool awake)
    {
        SetThreadExecutionState(awake
            ? (ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED)
            : ES_CONTINUOUS);
    }

    void ChooseNewTarget()
    {
        RECT r{};
        if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &r, 0)) return;

        static std::mt19937 gen{ std::random_device{}() };
        std::uniform_int_distribution<int> dx(r.left + 25, r.right - 25);
        std::uniform_int_distribution<int> dy(r.top + 25, r.bottom - 25);

        m_target.x = dx(gen);
        m_target.y = dy(gen);
        m_hasTarget = true;
    }

    void MoveMouseContinuously()
    {
        POINT current{};
        if (!GetCursorPos(&current)) return;

        if (!m_hasTarget)
            ChooseNewTarget();

        int vx = m_target.x - current.x;
        int vy = m_target.y - current.y;
        double distance = std::sqrt(static_cast<double>(vx * vx + vy * vy));

        if (distance < 8.0)
        {
            ChooseNewTarget();
            return;
        }

        static std::mt19937 gen{ std::random_device{}() };
        std::uniform_int_distribution<int> stepDist(2, 7);
        std::uniform_int_distribution<int> jitterDist(-2, 2);

        double step = static_cast<double>(stepDist(gen));
        int nextX = current.x + static_cast<int>((vx / distance) * step) + jitterDist(gen);
        int nextY = current.y + static_cast<int>((vy / distance) * step) + jitterDist(gen);

        SetCursorPos(nextX, nextY);
    }

    void StartContinuousTimer()
    {
        // Frequent small movements keep the pointer moving continuously.
        // No long wait or blocking Sleep call is used.
        SetTimer(ID_TIMER_MOVE, 30, nullptr);
    }

    void StartMover()
    {
        if (m_running) return;
        m_running = true;
        SetDisplayAwake(true);
        m_hasTarget = false;
        ChooseNewTarget();
        StartContinuousTimer();
    }

    void StopMover()
    {
        if (!m_running) return;
        KillTimer(ID_TIMER_MOVE);
        m_running = false;
        SetDisplayAwake(false);
    }

    void ShowDialog()
    {
        ShowWindow(SW_SHOW);
        SetForegroundWindow();
    }

    void ShowTrayMenu()
    {
        POINT p{};
        GetCursorPos(&p);
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, ID_TRAY_SHOW, L"Show Explore");
        AppendMenuW(menu, MF_STRING, ID_TRAY_ON, L"Turn On (Alt+Q)");
        AppendMenuW(menu, MF_STRING, ID_TRAY_OFF, L"Turn Off (Alt+W)");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");
        SetForegroundWindow();
        TrackPopupMenu(menu, TPM_RIGHTBUTTON, p.x, p.y, 0, m_hWnd, nullptr);
        DestroyMenu(menu);
    }

    afx_msg void OnTimer(UINT_PTR id)
    {
        if (id == ID_TIMER_MOVE && m_running)
        {
            MoveMouseContinuously();
        }
        CDialog::OnTimer(id);
    }

    afx_msg void OnHotKey(UINT nHotKeyId, UINT, UINT)
    {
        if (nHotKeyId == HOTKEY_ON) StartMover();
        if (nHotKeyId == HOTKEY_OFF) StopMover();
    }

    afx_msg LRESULT OnTrayIcon(WPARAM, LPARAM event)
    {
        if (event == WM_RBUTTONUP) ShowTrayMenu();
        if (event == WM_LBUTTONDBLCLK) ShowDialog();
        return 0;
    }

    afx_msg void OnTrayCommand(UINT id)
    {
        if (id == ID_TRAY_SHOW) ShowDialog();
        if (id == ID_TRAY_ON) StartMover();
        if (id == ID_TRAY_OFF) StopMover();
        if (id == ID_TRAY_EXIT) OnCancel();
    }

    afx_msg void OnTurnOn() { StartMover(); }
    afx_msg void OnTurnOff() { StopMover(); }

    void OnCancel() override
    {
        StopMover();
        UnregisterHotKey(m_hWnd, HOTKEY_ON);
        UnregisterHotKey(m_hWnd, HOTKEY_OFF);
        RemoveTrayIcon();
        CDialog::OnCancel();
    }

    DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CMainDlg, CDialog)
    ON_WM_TIMER()
    ON_WM_HOTKEY()
    ON_COMMAND(ID_BUTTON_ON, &CMainDlg::OnTurnOn)
    ON_COMMAND(ID_BUTTON_OFF, &CMainDlg::OnTurnOff)
    ON_COMMAND_RANGE(ID_TRAY_SHOW, ID_TRAY_EXIT, &CMainDlg::OnTrayCommand)
    ON_MESSAGE(WM_TRAYICON, &CMainDlg::OnTrayIcon)
END_MESSAGE_MAP()

class CExploreApp : public CWinApp
{
public:
    BOOL InitInstance() override
    {
        CWinApp::InitInstance();
        CMainDlg dlg;
        m_pMainWnd = &dlg;
        dlg.DoModal();
        return FALSE;
    }
};

CExploreApp theApp;
