
// stl2stp-code.cpp : 定义应用程序的类行为。
//

#include "stdafx.h"
#include "afxwinappex.h"
#include "afxdialogex.h"
#include "stl2stp-code.h"
#include "MainFrm.h"

#include "stl2stp-codeDoc.h"
#include "stl2stp-codeView.h"
// 【新 UI P2·SDI 化阶段 1.2】ChildFrm.h 引用随 CChildFrame 整体移除（原版子框架
// 取证记录保留在 AGENT.md；MDI 架构让位于单文档，见设计定稿 4.2.2）


// Cstl2stpcodeApp

BEGIN_MESSAGE_MAP(Cstl2stpcodeApp, CWinAppEx)
	ON_COMMAND(ID_APP_ABOUT, &Cstl2stpcodeApp::OnAppAbout)
	// 基于文件的标准文档命令
	ON_COMMAND(ID_FILE_NEW, &CWinAppEx::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, &CWinAppEx::OnFileOpen)
	// 标准打印设置命令
	ON_COMMAND(ID_FILE_PRINT_SETUP, &CWinAppEx::OnFilePrintSetup)
END_MESSAGE_MAP()


// Cstl2stpcodeApp 构造

Cstl2stpcodeApp::Cstl2stpcodeApp()
{
	m_bHiColorIcons = TRUE;

	// 支持重新启动管理器
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_ALL_ASPECTS;
#ifdef _MANAGED
	// 如果应用程序是利用公共语言运行时支持(/clr)构建的，则: 
	//     1) 必须有此附加设置，“重新启动管理器”支持才能正常工作。
	//     2) 在您的项目中，您必须按照生成顺序向 System.Windows.Forms 添加引用。
	System::Windows::Forms::Application::SetUnhandledExceptionMode(System::Windows::Forms::UnhandledExceptionMode::ThrowException);
#endif

	// 复刻原版 STL2STP 的 AppID（原版 CVoxelModApp 中为
	// SetAppID(L"VoxelMod.AppID.NoVersion")）。
	// 注意：本重制版不与原版共用 AppID，避免任务栏分组/跳转列表冲突。
	SetAppID(_T("STL2STP-Code.AppID.NoVersion"));

	// 创建全局 OpenGL 图形驱动（复刻原版 CVoxelModApp 构造函数中的做法：
	// OpenGl_GraphicDriver(NULL DisplayConnection, true)）
	m_hGraphicDriver = new OpenGl_GraphicDriver(
		opencascade::handle<Aspect_DisplayConnection>(), true);

	// TODO: 在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中
}

// 唯一的一个 Cstl2stpcodeApp 对象

Cstl2stpcodeApp theApp;


// Cstl2stpcodeApp 初始化

BOOL Cstl2stpcodeApp::InitInstance()
{
	// 如果一个运行在 Windows XP 上的应用程序清单指定要
	// 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
	//则需要 InitCommonControlsEx()。  否则，将无法创建窗口。
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// 将它设置为包括所有要在应用程序中使用的
	// 公共控件类。
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinAppEx::InitInstance();


	// 初始化 OLE 库
	if (!AfxOleInit())
	{
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}

	AfxEnableControlContainer();

	EnableTaskbarInteraction(FALSE);  // 原版 InitInstance 中传 0

	// 使用 RichEdit 控件需要 AfxInitRichEdit2()	
	// AfxInitRichEdit2();

	// 标准初始化
	// 注册表键：原版为 "STL2STP"，但本重制版必须使用独立键名——
	// 两者若共用 HKCU\Software\STL2STP，本程序会加载原版保存的界面
	// 停靠布局状态，导致菜单栏/工具栏被错误还原到窗口侧边（实测复现）。
	// 标题栏文本/图标等可见要素仍与原版一致（见资源字符串 IDR_MAINFRAME）。
	SetRegistryKey(_T("STL2STP-Code"));
	// 新 UI 有意偏差：启用最近文件列表（原版为 0）——菜单栏恢复可见后，
	// “文件→最近的文件”成为高频入口；见 ui-redesign\UI重设计方案.md A7
	LoadStdProfileSettings(16);

	// 【新 UI P2·SDI 化阶段 1.1】工作区布局版本迁移：
	// 停靠状态按控件 ID 存于 HKCU\...\STL2STP\Workspace，帧结构/窗格集合变化后
	// 旧记录可能把工具栏/窗格还原到错误形态（AGENT.md 验收补充三/五两次事故
	// 同源）。版本不匹配时清一次 Workspace 节；DarkTheme/ApplicationLook 两个
	// 设置同在此节内，删除前读出、删除后回写；MRU（Recent File List）与
	// LastExportDir（Settings 节）不在 Workspace 内，不受影响。
	// 每次帧结构/窗格集合变化递增 kLayoutVersion（设计定稿 4.2.8）。
	{
		const int kLayoutVersion = 8;   // 8 = 工具行替代 STL/视图两工具栏（P4·C，设计稿 4.6）；7 = 品牌标题栏改基类 CMFCToolBar（CDockablePane 版落带区错误，补充二十二）；6 = 品牌标题栏替代菜单栏窗格行（P4·B，4.5）；5 = 任务窗格加宽+内底自绘（P4·A，4.4）；4 = 属性窗格退役（2.2）；3 = SDI 切换（1.2）；2 = 视图命令栏迁主框架（1.1）
		if (GetProfileInt(_T("Workspace"), _T("LayoutVersion"), 0) != kLayoutVersion)
		{
			const int nDarkTheme = GetProfileInt(_T("Workspace"), _T("DarkTheme"), 0);
			const int nAppLook   = GetProfileInt(_T("Workspace"), _T("ApplicationLook"), 0);
			{
				HKEY hKeyApp = NULL;
				if (RegOpenKeyEx(HKEY_CURRENT_USER,
						_T("Software\\STL2STP-Code\\STL2STP"), 0, KEY_ALL_ACCESS, &hKeyApp) == ERROR_SUCCESS)
				{
					RegDeleteTree(hKeyApp, _T("Workspace"));
					RegCloseKey(hKeyApp);
				}
			}
			WriteProfileInt(_T("Workspace"), _T("LayoutVersion"), kLayoutVersion);
			if (nDarkTheme != 0)
				WriteProfileInt(_T("Workspace"), _T("DarkTheme"), nDarkTheme);
			if (nAppLook != 0)
				WriteProfileInt(_T("Workspace"), _T("ApplicationLook"), nAppLook);
		}
	}


	InitContextMenuManager();

	InitKeyboardManager();

	InitTooltipManager();
	CMFCToolTipInfo ttParams;
	ttParams.m_bVislManagerTheme = TRUE;
	theApp.GetTooltipManager()->SetTooltipParams(AFX_TOOLTIP_TYPE_ALL,
		RUNTIME_CLASS(CMFCToolTipCtrl), &ttParams);

	// 【新 UI P2·SDI 化阶段 1.2】MDI→SDI：文档模板改单文档——主框架即文档框架
	//（CMainFrame 由模板经 ProcessShellCommand 内 LoadFrame(IDR_stl2stpcodeTYPE)
	// 创建；此前手工 new CMainFrame + LoadFrame(IDR_MAINFRAME) 的 MDI 创建块随之
	// 取消。加速键表已在 .rc 为 IDR_stl2stpcodeTYPE 补齐（130 原无——MDI 时代由
	// 主框架 128 承载，SDI 不补则 Ctrl+N/O/S 全部失效）。设计定稿 4.2.2(a)(d)。
	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(IDR_stl2stpcodeTYPE,
		RUNTIME_CLASS(Cstl2stpcodeDoc),
		RUNTIME_CLASS(CMainFrame), // SDI：主框架即文档框架
		RUNTIME_CLASS(Cstl2stpcodeView));
	if (!pDocTemplate)
		return FALSE;
	AddDocTemplate(pDocTemplate);


	// 分析标准 shell 命令、DDE、打开文件操作的命令行
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);



	// 调度在命令行中指定的命令。SDI 下 FileNew/FileOpen 由模板创建主框架+文档+视图。
	// 如果用 /RegServer、/Register、/Unregserver 或 /Unregister 启动应用程序，则返回 FALSE。
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;
	// 主窗口已初始化，因此显示它并对其进行更新（模板创建后经 m_pMainWnd 取回）
	m_pMainWnd->ShowWindow(m_nCmdShow);
	m_pMainWnd->UpdateWindow();

	return TRUE;
}

int Cstl2stpcodeApp::ExitInstance()
{
	//TODO: 处理可能已添加的附加资源
	AfxOleTerm(FALSE);

	return CWinAppEx::ExitInstance();
}

// Cstl2stpcodeApp 消息处理程序


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// 用于运行对话框的应用程序命令
void Cstl2stpcodeApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

// Cstl2stpcodeApp 自定义加载/保存方法

void Cstl2stpcodeApp::PreLoadState()
{
	BOOL bNameValid;
	CString strName;
	bNameValid = strName.LoadString(IDS_EDIT_MENU);
	ASSERT(bNameValid);
	GetContextMenuManager()->AddMenu(strName, IDR_POPUP_EDIT);
	bNameValid = strName.LoadString(IDS_EXPLORER);
	ASSERT(bNameValid);
	GetContextMenuManager()->AddMenu(strName, IDR_POPUP_EXPLORER);
}

void Cstl2stpcodeApp::LoadCustomState()
{
}

void Cstl2stpcodeApp::SaveCustomState()
{
}

// Cstl2stpcodeApp 消息处理程序



