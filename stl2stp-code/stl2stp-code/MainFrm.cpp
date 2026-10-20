
// MainFrm.cpp : CMainFrame 类的实现
//

#include "stdafx.h"
#include "stl2stp-code.h"
#include "stl2stp-codeView.h"   // 新 UI P4·G：主题切换联动视口背景渐变
#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CMainFrame

// 【新 UI P2·SDI 化阶段 1.2】DECLARE_DYNAMIC → DYNCREATE（SDI 模板经
// CRuntimeClass::CreateObject 创建主框架，见 MainFrm.h 修正注释）
IMPLEMENT_DYNCREATE(CMainFrame, CFrameWndEx)

const int  iMaxUserToolbars = 10;
const UINT uiFirstUserToolBarId = AFX_IDW_CONTROLBAR_FIRST + 40;
const UINT uiLastUserToolBarId = uiFirstUserToolBarId + iMaxUserToolbars - 1;

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWndEx)
	ON_WM_CREATE()
	ON_COMMAND(ID_VIEW_CUSTOMIZE, &CMainFrame::OnViewCustomize)
	// 1.2：ON_COMMAND(ID_WINDOW_MANAGER)（窗口管理器）已移除——SDI 单文档无多窗口可管
	ON_REGISTERED_MESSAGE(AFX_WM_CREATETOOLBAR, &CMainFrame::OnToolbarCreateNew)
	ON_COMMAND_RANGE(ID_VIEW_APPLOOK_WIN_2000, ID_VIEW_APPLOOK_WINDOWS_7, &CMainFrame::OnApplicationLook)
	ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_APPLOOK_WIN_2000, ID_VIEW_APPLOOK_WINDOWS_7, &CMainFrame::OnUpdateApplicationLook)
	ON_COMMAND(ID_VIEW_DARKTHEME, &CMainFrame::OnViewDarkTheme)
	ON_UPDATE_COMMAND_UI(ID_VIEW_DARKTHEME, &CMainFrame::OnUpdateViewDarkTheme)
	ON_WM_SETTINGCHANGE()
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,               // 状态行指示器
	ID_INDICATOR_MODEL_INFO,    // 当前模型名（新 UI 增补）
	ID_INDICATOR_MODEL_STATS,   // V/E/F 统计（新 UI 增补）
	ID_INDICATOR_VIEW_DIR,     // 当前视角（新 UI 增补）
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
};

// CMainFrame 构造/析构

CMainFrame::CMainFrame()
{
	// TODO: 在此添加成员初始化代码
	theApp.m_nAppLook = theApp.GetInt(_T("ApplicationLook"), ID_VIEW_APPLOOK_VS_2008);
}

CMainFrame::~CMainFrame()
{
}

// P4·4.4(3)：系统标题栏跟随主题（DWM 沉浸式深标题栏）。运行时 GetProcAddress
// 调 DwmSetWindowAttribute（免加 dwmapi.lib 依赖）；属性 20 于 Win10 1903+，
// 失败回落 19（1809+）。注：曾因"整窗军蓝"误判回退——后查明是桌面全屏覆盖
// 窗口污染了屏幕采样（PrintWindow 复核软件渲染正常），本功能无实害，恢复。
static void SetCaptionDark(HWND hWnd, BOOL bDark)
{
	typedef HRESULT(WINAPI* PFN_DwmSetWindowAttribute)(HWND, DWORD, const void*, DWORD);
	HMODULE hDwm = ::GetModuleHandle(_T("dwmapi.dll"));
	if (hDwm == NULL)
		return;
	PFN_DwmSetWindowAttribute pfn = (PFN_DwmSetWindowAttribute)::GetProcAddress(hDwm, "DwmSetWindowAttribute");
	if (pfn == NULL)
		return;
	BOOL bVal = bDark;
	if (FAILED(pfn(hWnd, 20, &bVal, sizeof(bVal))))
		pfn(hWnd, 19, &bVal, sizeof(bVal));
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWndEx::OnCreate(lpCreateStruct) == -1)
		return -1;

	// 视觉主题（新 UI P2 增补）：浅色=原版 Windows 原生样式（取证基线，默认）；
	// 深色=Office2007 ObsidianBlack。注册表 DarkTheme 记忆用户选择。
	m_bDarkTheme = (theApp.GetInt(_T("DarkTheme"), 0) != 0);
	if (m_bDarkTheme)
	{
		CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_ObsidianBlack);
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerOffice2007));
	}
	else
	{
		// 复刻原版 STL2STP：经典 MDI（无标签页组）+ Windows 原生视觉样式
		// （原版 OnCreate 首行：CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));）
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));
	}
	SetCaptionDark(m_hWnd, m_bDarkTheme);   // P4：系统标题栏跟随主题

	BOOL bNameValid;

	// 【新 UI P4·B】品牌标题栏（设计稿 4.5）：35px 自绘品牌行替代 CMFCMenuBar
	// 窗格行——logo、应用名、徽章、菜单项、主题与帮助图标全在 CBrandBar 内自绘，
	// 菜单经 TrackPopupMenu(owner=主框架) 弹 IDR_stl2stpcodeTYPE 子菜单：
	// WM_INITMENUPOPUP 走 MFC 原生路由（MRU 占位自动刷新），菜单命令照常
	// WM_COMMAND 进框架映射。基类取 CMFCToolBar——实测 CDockablePane 停
	// CBRS_TOP 落在"窗格带"（工具栏行之下），只有工具栏一系才停进标题栏正
	// 下方的"控制栏带"（原菜单栏行位置，补充二十二取证）。CMFCMenuBar 不再
	// 创建后，框架系统菜单栏失去原本由其顺带摘除的机制，在此显式 SetMenu(NULL)
	// （文档模板 HMENU 为共享资源，仅摘除显示、不销毁）。旧停靠残留由
	// kLayoutVersion 6 升 7 迁移清理（stl2stp-code.cpp InitInstance）。
	SetMenu(NULL);
	if (!m_wndBrandBar.CreateEx(this, TBSTYLE_FLAT,
			WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_TOP | CBRS_ALIGN_ANY,
			CRect(0, 0, 0, 35), ID_VIEW_BRANDBAR))
	{
		TRACE0("未能创建品牌标题栏\n");
		return -1;
	}

	// 【新 UI P4·C】工具行（设计稿 4.6）：44px 自绘工具条替代 STL 命令栏(59450)
	// +视图命令栏(59451)——步骤条（1 载入模型 / 2 检查-简化 / 3 导出）+ 按钮组
	// （打开 STL / 导出 STEP 主按钮 / 导出 IGES / 网格简化 | 适应 / 视图下拉 /
	// 网格 / 显示下拉）。命令照常 WM_COMMAND 走既有命令链（32773/32828/32826/
	// 32837/32785/32807，守卫/确认框取证基线不动），下拉弹程序化菜单。两旧
	// 工具栏成员保留不再创建。基类同品牌栏取 CMFCToolBar 一系——带区归属/
	// 零按钮行宽/NC 吞噬三教训见补充二十二；新建/导入按钮的工具栏面随之
	// 消失（文件菜单仍全量可达，有意偏差登记）。
	if (!m_wndToolStrip.CreateEx(this, TBSTYLE_FLAT,
			WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_TOP | CBRS_ALIGN_ANY,
			CRect(0, 0, 0, 44), ID_VIEW_TOOLSTRIP))
	{
		TRACE0("未能创建工具行\n");
		return -1;
	}

	CString strCustomize;
	bNameValid = strCustomize.LoadString(IDS_TOOLBAR_CUSTOMIZE);
	ASSERT(bNameValid);

	if (!m_wndStatusBar.Create(this))
	{
		TRACE0("未能创建状态栏\n");
		return -1;
	}
	m_wndStatusBar.SetIndicators(indicators, sizeof(indicators)/sizeof(UINT));

	// 状态栏自定义窗格接线（新 UI）：模型名 / V·E·F 统计 / 当前视角
	m_nPaneModel   = m_wndStatusBar.CommandToIndex(ID_INDICATOR_MODEL_INFO);
	m_nPaneStats   = m_wndStatusBar.CommandToIndex(ID_INDICATOR_MODEL_STATS);
	m_nPaneViewDir = m_wndStatusBar.CommandToIndex(ID_INDICATOR_VIEW_DIR);
	if (m_nPaneModel   >= 0) m_wndStatusBar.SetPaneWidth(m_nPaneModel, 300);
	if (m_nPaneStats   >= 0) m_wndStatusBar.SetPaneWidth(m_nPaneStats, 250);
	if (m_nPaneViewDir >= 0) m_wndStatusBar.SetPaneWidth(m_nPaneViewDir, 90);

	// 【新 UI 有意偏差，P4·B/C 更新】品牌标题栏行 + 工具行替代原版
	// "隐藏菜单栏+22px 空条带+双工具栏"整组顶部带；行序 [品牌标题栏][工具行]
	// （控制栏带按 DockPane 顺序，先停者在上——P4·B 实测取证，补充二十二）。
	m_wndBrandBar.EnableDocking(CBRS_ALIGN_ANY);
	m_wndToolStrip.EnableDocking(CBRS_ALIGN_ANY);
	EnableDocking(CBRS_ALIGN_ANY);
	DockPane(&m_wndBrandBar);
	DockPane(&m_wndToolStrip);

	// 启用 Visual Studio 2005 样式停靠窗口行为
	CDockingManager::SetDockingMode(DT_SMART);
	// 启用 Visual Studio 2005 样式停靠窗口自动隐藏行为
	EnableAutoHidePanes(CBRS_ALIGN_ANY);

	// 加载菜单项图像(不在任何标准工具栏上): 
	CMFCToolBar::AddToolBarForImageCollection(IDR_MENU_IMAGES, theApp.m_bHiColorIcons ? IDB_MENU_IMAGES_24 : 0);

	// 创建停靠窗口（原版仅有：模型/输出/属性 三个停靠窗格，无类视图）
	if (!CreateDockingWindows())
	{
		TRACE0("未能创建停靠窗口\n");
		return -1;
	}

	m_wndFileView.EnableDocking(CBRS_ALIGN_ANY);
	DockPane(&m_wndFileView);
	m_wndOutput.EnableDocking(CBRS_ALIGN_ANY);
	DockPane(&m_wndOutput);
	// 2.2：属性窗格 EnableDocking/DockPane 已随退役移除——右栏仅任务窗格
	// 新 UI P2·2.1：右栏“任务”窗格（三卡）。控件 ID 取独立新值 185（定稿原为
	// 复用 150——并存期同 ID 会互相覆盖停靠状态，验收补充五教训）
	m_wndTaskPane.EnableDocking(CBRS_ALIGN_ANY);
	DockPane(&m_wndTaskPane);

	// 1.2：EnableWindowsDialog（窗口管理器对话框，CMDIFrameWndEx 专属）已移除——
	// CFrameWndEx 无此 API，SDI 单文档亦无窗口列表可管

	// 启用工具栏和停靠窗口菜单替换
	EnablePaneMenu(TRUE, ID_VIEW_CUSTOMIZE, strCustomize, ID_VIEW_TOOLBAR);

	// 启用快速(按住 Alt 拖动)工具栏自定义
	// （原版为 CMFCToolBar::m_bAltCustomization = TRUE）
	CMFCToolBar::EnableQuickCustomization();

	// 注意：原版 OnCreate 末尾没有 ModifyStyle(0, FWS_PREFIXTITLE)，
	// 标题栏保持 MFC 默认逻辑（子窗口最大化时 “文档 - 应用”，
	// 还原时 “应用 - [文档]”），此处保持一致，不修改。
	//（1.2 注：以上为 MDI 时代取证注释，SDI 下标题恒为 “应用 - [文档]” 型。）

	// 【新 UI P2·SDI 化阶段 1.2】主框架经文档模板 LoadFrame(IDR_stl2stpcodeTYPE=130)
	// 创建，默认取文档图标；显式设置主程序图标保持与 MDI 时代（LoadFrame(128)）一致。
	HICON hIconBig = (HICON) ::LoadImage(::AfxGetResourceHandle(),
		MAKEINTRESOURCE(IDR_MAINFRAME), IMAGE_ICON,
		::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), 0);
	if (hIconBig != NULL)
		SetIcon(hIconBig, TRUE);
	HICON hIconSmall = (HICON) ::LoadImage(::AfxGetResourceHandle(),
		MAKEINTRESOURCE(IDR_MAINFRAME), IMAGE_ICON,
		::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), 0);
	if (hIconSmall != NULL)
		SetIcon(hIconSmall, FALSE);

	// 【新 UI P4·E】系统原生标题栏退役（用户红圈指定）：品牌标题栏升为窗口
	// 标题栏。PreCreateWindow 的 cs.style 清位实测未生效（SDI 装配链经文档
	// 模板 LoadFrame→Create 传入的 dwDefaultStyle 又是全 WS_OVERLAPPEDWINDOW，
	// 创建后的运行态仍带 WS_CAPTION）——改在 OnCreate 末尾直接 ModifyStyle
	// 摘除，SWP_FRAMECHANGED 触发非客户区重算，客户区上扩到窗顶。保留
	// WS_THICKFRAME：六向缩放边框与最大化窗口离屏边框仍由系统管理。
	// 标题三键（最小化/最大化-还原/关闭）由 CBrandBar 自绘承担，空白区左键
	// 转 HTCAPTION 拖动、双击切换最大化（Windows 标题栏惯例）。SetCaptionDark
	// 的 DWM 深色属性保留——无边框后仍给残存细边框着色。
	ModifyStyle(WS_CAPTION, 0, SWP_FRAMECHANGED);

	return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFrameWndEx::PreCreateWindow(cs) )
		return FALSE;
	// TODO: 在此处通过修改
	//  CREATESTRUCT cs 来修改窗口类或样式
	// 【新 UI P4·E】注：曾在此 cs.style &= ~WS_CAPTION 摘系统标题栏，实测对
	// SDI 文档模板装配链不生效（运行态仍带 WS_CAPTION），摘除改在 OnCreate
	// 末尾 ModifyStyle 执行——见该处注释。

	return TRUE;
}

// 向“输出”窗口的生成页追加一行日志
void CMainFrame::LogMessage(LPCTSTR lpszText)
{
	m_wndOutput.AddBuildLine(lpszText);
}

// ==== 状态栏真实信息（新 UI 增补）====

void CMainFrame::SetStatusModelInfo(LPCTSTR lpszText)
{
	if (m_nPaneModel >= 0)
		m_wndStatusBar.SetPaneText(m_nPaneModel, lpszText);
}

void CMainFrame::SetStatusModelStats(LPCTSTR lpszText)
{
	if (m_nPaneStats >= 0)
		m_wndStatusBar.SetPaneText(m_nPaneStats, lpszText);
}

void CMainFrame::SetStatusViewDir(LPCTSTR lpszText)
{
	if (m_nPaneViewDir >= 0)
		m_wndStatusBar.SetPaneText(m_nPaneViewDir, lpszText);
}

// 新建空文档：清空属性统计与状态栏信息
// （原版怪癖“新建不清空上次载入的统计”为有意偏差，见 ui-redesign\UI重设计方案.md A6）
void CMainFrame::ClearModelInfo()
{
	// 2.2：属性窗格退役——任务窗格为唯一数值/形态显示位
	m_wndTaskPane.ClearInfo();
	SetStatusModelInfo(_T("未载入模型"));
	SetStatusModelStats(_T("V 0 · E 0 · F 0"));
}

BOOL CMainFrame::CreateDockingWindows()
{
	BOOL bNameValid;

	// 创建文件视图（原版标题为“模型”）
	CString strFileView;
	bNameValid = strFileView.LoadString(IDS_FILE_VIEW);
	ASSERT(bNameValid);
	if (!m_wndFileView.Create(strFileView, this, CRect(0, 0, 200, 200), TRUE, ID_VIEW_FILEVIEW, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_LEFT| CBRS_FLOAT_MULTI))
	{
		TRACE0("未能创建“文件视图”窗口\n");
		return FALSE; // 未能创建
	}

	// 创建输出窗口
	CString strOutputWnd;
	bNameValid = strOutputWnd.LoadString(IDS_OUTPUT_WND);
	ASSERT(bNameValid);
	if (!m_wndOutput.Create(strOutputWnd, this, CRect(0, 0, 100, 100), TRUE, ID_VIEW_OUTPUTWND, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_BOTTOM | CBRS_FLOAT_MULTI))
	{
		TRACE0("未能创建输出窗口\n");
		return FALSE; // 未能创建
	}

	// 【新 UI P2·2.2】“属性”窗格（CMFCPropertyGridCtrl）已退役——模型信息九项与
	// 形态属性（颜色/材料）由右栏“任务”窗格承载（同源数值 + 真实语义控件）；
	// 退役实现与 AFX_WM_PROPERTY_CHANGED 接线记录见验收补充九/十一。
	// 任务窗格：控件 ID 185，右栏唯一窗格
	CString strTaskWnd;
	bNameValid = strTaskWnd.LoadString(IDS_TASK_WND);
	ASSERT(bNameValid);
	if (!m_wndTaskPane.Create(strTaskWnd, this, CRect(0, 0, 320, 240), TRUE, ID_VIEW_TASKPANE, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_RIGHT | CBRS_FLOAT_MULTI))
	{
		TRACE0("未能创建“任务”窗口\n");
		return FALSE;
	}

	SetDockingWindowIcons(theApp.m_bHiColorIcons);
	return TRUE;
}

void CMainFrame::SetDockingWindowIcons(BOOL bHiColorIcons)
{
	HICON hFileViewIcon = (HICON) ::LoadImage(::AfxGetResourceHandle(), MAKEINTRESOURCE(bHiColorIcons ? IDI_FILE_VIEW_HC : IDI_FILE_VIEW), IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), 0);
	m_wndFileView.SetIcon(hFileViewIcon, FALSE);

	HICON hOutputBarIcon = (HICON) ::LoadImage(::AfxGetResourceHandle(), MAKEINTRESOURCE(bHiColorIcons ? IDI_OUTPUT_WND_HC : IDI_OUTPUT_WND), IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), 0);
	m_wndOutput.SetIcon(hOutputBarIcon, FALSE);

	// 新 UI P2·2.1/2.2：任务窗格图标（右栏唯一窗格，承接属性窗格图标）
	HICON hTaskBarIcon = (HICON) ::LoadImage(::AfxGetResourceHandle(), MAKEINTRESOURCE(bHiColorIcons ? IDI_PROPERTIES_WND_HC : IDI_PROPERTIES_WND), IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), 0);
	m_wndTaskPane.SetIcon(hTaskBarIcon, FALSE);

	// 1.2：UpdateMDITabbedBarsIcons()（MDI 标签子框架图标同步）已移除——SDI 无子框架
}

// CMainFrame 诊断

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CFrameWndEx::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWndEx::Dump(dc);
}
#endif //_DEBUG


// CMainFrame 消息处理程序

// 1.2：OnWindowManager/ShowWindowsDialog（窗口管理器对话框）已移除——
// CMDIFrameWndEx 专属命令，SDI 单文档无窗口列表可管（ID_WINDOW_MANAGER 的
// 命令 ID 保留在 resource.h 作取证）

void CMainFrame::OnViewCustomize()
{
	CMFCToolBarsCustomizeDialog* pDlgCust = new CMFCToolBarsCustomizeDialog(this, TRUE /* 扫描菜单*/);
	pDlgCust->EnableUserDefinedToolbars();
	pDlgCust->Create();
}

LRESULT CMainFrame::OnToolbarCreateNew(WPARAM wp,LPARAM lp)
{
	LRESULT lres = CFrameWndEx::OnToolbarCreateNew(wp,lp);
	if (lres == 0)
	{
		return 0;
	}

	CMFCToolBar* pUserToolbar = (CMFCToolBar*)lres;
	ASSERT_VALID(pUserToolbar);

	BOOL bNameValid;
	CString strCustomize;
	bNameValid = strCustomize.LoadString(IDS_TOOLBAR_CUSTOMIZE);
	ASSERT(bNameValid);

	pUserToolbar->EnableCustomizeButton(TRUE, ID_VIEW_CUSTOMIZE, strCustomize);
	return lres;
}

void CMainFrame::OnApplicationLook(UINT id)
{
	CWaitCursor wait;

	theApp.m_nAppLook = id;

	switch (theApp.m_nAppLook)
	{
	case ID_VIEW_APPLOOK_WIN_2000:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManager));
		break;

	case ID_VIEW_APPLOOK_OFF_XP:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerOfficeXP));
		break;

	case ID_VIEW_APPLOOK_WIN_XP:
		CMFCVisualManagerWindows::m_b3DTabsXPTheme = TRUE;
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));
		break;

	case ID_VIEW_APPLOOK_OFF_2003:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerOffice2003));
		CDockingManager::SetDockingMode(DT_SMART);
		break;

	case ID_VIEW_APPLOOK_VS_2005:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerVS2005));
		CDockingManager::SetDockingMode(DT_SMART);
		break;

	case ID_VIEW_APPLOOK_VS_2008:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerVS2008));
		CDockingManager::SetDockingMode(DT_SMART);
		break;

	case ID_VIEW_APPLOOK_WINDOWS_7:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows7));
		CDockingManager::SetDockingMode(DT_SMART);
		break;

	default:
		switch (theApp.m_nAppLook)
		{
		case ID_VIEW_APPLOOK_OFF_2007_BLUE:
			CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_LunaBlue);
			break;

		case ID_VIEW_APPLOOK_OFF_2007_BLACK:
			CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_ObsidianBlack);
			break;

		case ID_VIEW_APPLOOK_OFF_2007_SILVER:
			CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_Silver);
			break;

		case ID_VIEW_APPLOOK_OFF_2007_AQUA:
			CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_Aqua);
			break;
		}

		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerOffice2007));
		CDockingManager::SetDockingMode(DT_SMART);
	}

	// P4·4.4(5) 双主题轴防呆：外观轴切换时同步深色主题——除 ObsidianBlack 外
	// 全为浅色系；不同步会出现"壳层浅/任务卡深"混态（落盘先于重绘，4.3 时序教训）
	m_bDarkTheme = (theApp.m_nAppLook == ID_VIEW_APPLOOK_OFF_2007_BLACK);
	theApp.WriteInt(_T("DarkTheme"), m_bDarkTheme ? 1 : 0);

	// P4·T 补缺：外观轴切浅色时任务窗格自读注册表会随重绘正确换肤，但
	// 文件树色是 TVM 状态设置、视口背景须 GL 重绘——两者与深色主题轴的
	// ApplyVisualTheme 一样须显式重设（落盘先行，p3s33/p4d 时序教训同源）
	m_wndFileView.ApplyTheme(m_bDarkTheme);
	{
		Cstl2stpcodeView* pView = DYNAMIC_DOWNCAST(Cstl2stpcodeView, GetActiveView());
		if (pView != NULL)
			pView->ApplyViewTheme();
	}
	m_wndOutput.UpdateFonts();
	RedrawWindow(NULL, NULL, RDW_ALLCHILDREN | RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME | RDW_ERASE);
	RedrawOwnedPopups();   // P4·T：浮动窗格宿主不在框架子窗树内，须单独补绘

	theApp.WriteInt(_T("ApplicationLook"), theApp.m_nAppLook);
}

void CMainFrame::OnUpdateApplicationLook(CCmdUI* pCmdUI)
{
	pCmdUI->SetRadio(theApp.m_nAppLook == pCmdUI->m_nID);
}

// ==== 深/浅主题切换（新 UI P2 增补）====

void CMainFrame::OnViewDarkTheme()
{
	ApplyVisualTheme(!m_bDarkTheme, TRUE);
}

void CMainFrame::OnUpdateViewDarkTheme(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_bDarkTheme);
}

// 深色=CMFCVisualManagerOffice2007(ObsidianBlack)（SetStyle 必须在 SetDefaultManager 之前，
// 与 OnApplicationLook 同型）；浅色=原版取证基线 CMFCVisualManagerWindows。
void CMainFrame::ApplyVisualTheme(BOOL bDark, BOOL bSave)
{
	m_bDarkTheme = bDark;
	SetCaptionDark(GetSafeHwnd(), bDark);   // P4：系统标题栏跟随主题
	if (bDark)
	{
		CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_ObsidianBlack);
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerOffice2007));
	}
	else
	{
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));
	}
	CDockingManager::SetDockingMode(DT_SMART);
	m_wndOutput.UpdateFonts();
	// P3·3.3：落盘必须在 RedrawWindow 之前——任务窗格视觉令牌是无状态现读注册表
	//（TaskPane.cpp TaskPal/TaskCardBrush），先重绘后落盘会把窗格甩在上一主题
	//（p3s33 切换往返实测：暗卡片残留 91178px、浅色慢一拍）。
	if (bSave)
		theApp.WriteInt(_T("DarkTheme"), bDark ? 1 : 0);
	// P4·D：树控件色是状态设置（TVM），随切换显式重设；状态经 bDark 显式传入，
	// 但令牌调色板经 TaskPal() 现读注册表——必须在落盘之后调用，否则深色回切
	// 时读到旧值把树底设成浅色 crPaneBg（p4d 往返实测：切回深色树底 (238,241,246)）。
	m_wndFileView.ApplyTheme(bDark);
	// P4·G：视口背景随主题（深=暗色渐变/浅=原版取证基线渐变；模型/网格线/三轴
	// 渲染外观不动）。同样现读注册表，必须排在落盘之后；显式 Redraw 使切换即时
	// 可见（软渲染阻塞期照旧，验取证须像素轮询）。
	{
		Cstl2stpcodeView* pView = DYNAMIC_DOWNCAST(Cstl2stpcodeView, GetActiveView());
		if (pView != NULL)
			pView->ApplyViewTheme();
	}
	RedrawWindow(NULL, NULL, RDW_ALLCHILDREN | RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME | RDW_ERASE);
	// P4·T（用户反馈：切亮色主题时任务窗格背景不刷新）：浮动窗格的宿主
	// （CMiniFrameWnd）是独立顶层窗口，上面的框架级 RedrawWindow 只作用于
	// 框架子窗树、够不到它——拖出停靠的任务窗格滞留旧主题（实测暗底
	// (27,34,48) 残留）。补一次 owner 为主框架的顶层弹窗重绘
	RedrawOwnedPopups();
}

// P4·T：重绘主框架拥有的所有顶层弹窗（浮动停靠窗格的迷你框架宿主）。
// 浮动窗格通常 0-2 个，桌面顶层窗全量遍历一次的开销可忽略
void CMainFrame::RedrawOwnedPopups()
{
	const HWND hwndThis = GetSafeHwnd();
	for (HWND h = ::GetWindow(::GetDesktopWindow(), GW_CHILD); h != NULL;
		h = ::GetWindow(h, GW_HWNDNEXT))
	{
		if (::GetWindow(h, GW_OWNER) == hwndThis)
			::RedrawWindow(h, NULL, NULL, RDW_ALLCHILDREN | RDW_INVALIDATE |
				RDW_UPDATENOW | RDW_FRAME | RDW_ERASE);
	}
}


BOOL CMainFrame::LoadFrame(UINT nIDResource, DWORD dwDefaultStyle, CWnd* pParentWnd, CCreateContext* pContext) 
{
	// 基类将执行真正的工作

	if (!CFrameWndEx::LoadFrame(nIDResource, dwDefaultStyle, pParentWnd, pContext))
	{
		return FALSE;
	}


	// 为所有用户工具栏启用自定义按钮
	BOOL bNameValid;
	CString strCustomize;
	bNameValid = strCustomize.LoadString(IDS_TOOLBAR_CUSTOMIZE);
	ASSERT(bNameValid);

	for (int i = 0; i < iMaxUserToolbars; i ++)
	{
		CMFCToolBar* pUserToolbar = GetUserToolBarByIndex(i);
		if (pUserToolbar != NULL)
		{
			pUserToolbar->EnableCustomizeButton(TRUE, ID_VIEW_CUSTOMIZE, strCustomize);
		}
	}

	return TRUE;
}


void CMainFrame::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
	CFrameWndEx::OnSettingChange(uFlags, lpszSection);
	m_wndOutput.UpdateFonts();
}
