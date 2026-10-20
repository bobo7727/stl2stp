
// MainFrm.h : CMainFrame 类的接口
//

#pragma once
#include "FileView.h"
#include "ClassView.h"
#include "OutputWnd.h"
// 【新 UI P2·2.2】PropertiesWnd.h 引用随 CPropertiesWnd 整体退役——模型信息/
// 形态属性（颜色/材料）已迁入右栏“任务”窗格（CTaskPane），右栏唯一窗格；
// 退役前实现（CMFCPropertyGridCtrl + AFX_WM_PROPERTY_CHANGED 接线）记录见
// AGENT.md 验收补充九/十一，源码可从阶段 tar 备份恢复。
#include "TaskPane.h"      // 新 UI P2·2.1：右栏"任务"窗格（模型信息+简化+导出三卡）
#include "BrandBar.h"       // P4·B：品牌标题栏（35px 品牌行，替代 CMFCMenuBar 窗格行）
#include "ToolStrip.h"      // P4·C：工具行（44px 步骤条+命令按钮组，替代两旧工具栏）

// 原版(MFC100)菜单栏复刻说明：原版运行时菜单栏为 0 宽 x 22 高（窗口枚举实测 0x22，
// 标题'菜单栏'）——菜单按钮在内部但不可见，SC_KEYMENU 无响应。
// 【新 UI 有意偏差（2026-09-10）】菜单栏恢复为可见：原版"隐藏菜单栏"使全部命令
// 失去可发现性入口（只能靠记工具栏图标），见 ui-redesign\UI重设计方案.md 缺陷 A1。
// 原 CMainFrameEmptyToolBar（22px 空条带）随之移除。
class CMainFrameMenuBar : public CMFCMenuBar
{
};

// 【新 UI P2·SDI 化阶段 1.2】CMainFrame 基类 CMDIFrameWndEx → CFrameWndEx
//（单文档主框架即文档框架；MDI 专属 API EnableWindowsDialog/MDIGetActive/
// UpdateMDITabbedBarsIcons 已移除，其余停靠/工具栏 API 在 CFrameWndEx 同名具备，
// 实现期已对照 atlmfc afxframewndex.h 核实。设计定稿 4.2.2(b)）
// 【1.2 构建修正】SDI 下文档模板经 CRuntimeClass::CreateObject() 创建主框架
//（MDI 时代手工 new CMainFrame，DECLARE_DYNAMIC 即可；SDI 必须 DYNCREATE，
// 否则 CreateNewFrame 得到 NULL → 启动弹“创建空文档失败”）
class CMainFrame : public CFrameWndEx
{
	DECLARE_DYNCREATE(CMainFrame)
public:
	CMainFrame();

// 特性
public:

// 操作
public:
	// 向“输出”窗口的生成页追加一行日志（复刻原版 STL2STP 的日志行为）
	void LogMessage(LPCTSTR lpszText);
	// 原版行为：STL 载入后填充属性窗格的模型统计（转发到“属性”窗格；
	// 坐标按原版 0x140008a20 传裸 double，详见 PropertiesWnd.h 注释）
	void UpdateModelStats(long nVertices, long nEdges, long nFaces,
		double dXMin, double dXMax, double dYMin, double dYMax,
		double dZMin, double dZMax)
	{
		// 2.2：属性窗格退役——数值显示唯一入口为任务窗格（同源 9 值）
		m_wndTaskPane.UpdateModelStats(nVertices, nEdges, nFaces,
			dXMin, dXMax, dYMin, dYMax, dZMin, dZMax);
	}
// 重写
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL LoadFrame(UINT nIDResource, DWORD dwDefaultStyle = WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, CWnd* pParentWnd = NULL, CCreateContext* pContext = NULL);

// 实现
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
public:
	// 导出流程日志：直通输出窗格的原始消息序列（LB_ADDSTRING + LB_SETCURSEL(7)）
	void LogExportLine(const wchar_t* lpszText) { m_wndOutput.OriginalLogLine(lpszText); }

	// ==== 状态栏真实信息（新 UI 增补，替代原版仅 CAP/NUM/SCRL）====
	void SetStatusModelInfo(LPCTSTR lpszText);   // 窗格1：当前模型文件名
	void SetStatusModelStats(LPCTSTR lpszText);  // 窗格2：V/E/F 统计
	void SetStatusViewDir(LPCTSTR lpszText);     // 窗格3：当前视角名称
	// 新建空文档时清空属性统计与状态栏（原版怪癖"新建不清空上次统计"为有意偏差）
	void ClearModelInfo();
	// 新 UI：“文件”窗格列表刷新（载入/新建/关闭文档时调用；pExclude=待删除文档）
	void RefreshFileList(class CDocument* pExclude = NULL)
	{
		m_wndFileView.RefreshFileList(pExclude);
	}
	// 新 UI P2·2.1：右栏“任务”窗格状态刷新（文件名/大小/简化状态/撤销可用/
	// 大模型提示；载入、简化、撤销、新建等文档状态变化点调用）
	void UpdateTaskPane(class Cstl2stpcodeDoc* pDoc)
	{
		m_wndTaskPane.RefreshFromDocument(pDoc);
	}

	// 新 UI P2 增补：深/浅主题切换（浅色=原版 Windows 原生样式取证基线；
	// 深色=Office2007 ObsidianBlack）；bSave=是否持久化注册表
	void ApplyVisualTheme(BOOL bDark, BOOL bSave);
	// P4·T：补一次主框架拥有的顶层弹窗（浮动窗格宿主）重绘——切主题滞留修复
	void RedrawOwnedPopups();
	BOOL m_bDarkTheme = FALSE;

protected:  // 控件条嵌入成员
	// 【P4·B】CMFCMenuBar 窗格行退役（设计稿 4.5）——菜单由品牌标题栏 CBrandBar
	// 承载（TrackPopupMenu 弹原菜单，MRU/命令路由走 MFC 原生链）；m_wndMenuBar
	// 成员保留但 OnCreate 不再创建
	CMainFrameMenuBar  m_wndMenuBar;
	CBrandBar          m_wndBrandBar;      // P4·B：品牌标题栏（菜单/主题/帮助入口）
	// 【P4·C】STL 命令栏(59450)+视图命令栏(59451)退役——工具行 CToolStrip 承载
	// 同命令面（32773/32828/32826/32837/32785/32807 + 六视图/效果族下拉），两
	// 成员保留但 OnCreate 不再创建；P1/P2 工具栏复刻记录留档（原版工具栏资源
	// IDR_MAINFRAME_256=129 / IDR_VIEW_TOOLBAR），需要时可自阶段 tar 恢复创建代码
	CMFCToolBar     m_wndStlBar;      // （P4·C 退役）STL 命令工具栏
	CMFCToolBar     m_wndViewBar;    // （P4·C 退役）视图命令工具栏
	CToolStrip         m_wndToolStrip;    // P4·C：工具行（步骤条+命令按钮组）
	CMFCStatusBar     m_wndStatusBar;
	CFileView         m_wndFileView;
	CClassView        m_wndClassView;
	COutputWnd        m_wndOutput;
	CTaskPane         m_wndTaskPane;    // 新 UI P2·2.1/2.2：右栏"任务"窗格（属性窗格退役后为右栏唯一窗格）

	// 状态栏自定义窗格索引（OnCreate 中由 CommandToIndex 填充；-1 表示未创建）
	int m_nPaneModel  = -1;
	int m_nPaneStats  = -1;
	int m_nPaneViewDir = -1;

// 生成的消息映射函数
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnViewCustomize();   // 1.2：OnWindowManager（窗口管理器，MDI 专属）已移除
	afx_msg LRESULT OnToolbarCreateNew(WPARAM wp, LPARAM lp);
	afx_msg void OnApplicationLook(UINT id);
	afx_msg void OnUpdateApplicationLook(CCmdUI* pCmdUI);
	afx_msg void OnViewDarkTheme();                 // 新 UI P2：深/浅主题切换
	afx_msg void OnUpdateViewDarkTheme(CCmdUI* pCmdUI);
	afx_msg void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);
	DECLARE_MESSAGE_MAP()

	BOOL CreateDockingWindows();
	void SetDockingWindowIcons(BOOL bHiColorIcons);
};


