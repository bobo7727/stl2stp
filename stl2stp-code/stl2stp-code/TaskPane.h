#pragma once

// TaskPane.h —— 新 UI P2·2.1：右栏"任务"窗格（三卡：模型信息 / 网格简化 / 导出）
// 设计定稿见 ui-redesign\UI重设计方案.md 4.2.3；P3 视觉精修（卡片容器 OnPaint 自绘
// + 令牌配色 + 静态文本 WM_CTLCOLOR 着色）见 4.3。所有控件保持真实 HWND——
// WM_COMMAND/BM_CLICK/WM_GETTEXT/EnableWindow 无头验收链路零改动。
// 2.1 与"属性"窗格并存：控件 ID 取独立新值 ID_VIEW_TASKPANE=185（同 ID 会互相
// 覆盖停靠状态——AGENT.md 验收补充五教训）；窗格内容高于客户区时启用纵向滚动。
// 取证基线保护：边数显示沿用 V+F-2 公式输出（UpdateModelStats 同源）、包围盒
// 6 位小数、>10 万面提示为 advisory（不替代原版确认框）、导出按钮与工具栏/菜单/
// 无头验收同一 WM_COMMAND 链路。

class Cstl2stpcodeDoc;

// P4·4.4(4)：材料下拉整体换肤（CBS_OWNERDRAWFIXED——闭态显示区与弹出列表
// item 全走 DrawItem，销掉 3.3 的 P-R6 闭态偏差）；CB_*/键盘自动化链路不变。
class CTaskCombo : public CComboBox
{
public:
	BOOL CreateCombo(const CRect& rc, CWnd* pParent, UINT nID);

protected:
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
};

// P3·3.2：扁平按钮（BS_OWNERDRAW 三样式，设计定稿 4.3.2）——仍是真实 HWND，
// BM_CLICK/WM_GETTEXT/EnableWindow 与原 CButton 完全一致（无头验收链路不变）。
class CFlatButton : public CButton
{
public:
	enum EStyle { ePrimary, eSub, eLink };

	BOOL CreateFlat(const wchar_t* lpszCaption, const CRect& rc, CWnd* pParent,
		UINT nID, EStyle eStyle);

protected:
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnMouseLeave();

	EStyle m_eStyle = eSub;
	BOOL   m_bHover = FALSE;
	BOOL   m_bTrack = FALSE;    // TrackMouseEvent 已登记（WM_MOUSELEAVE 复位）

	DECLARE_MESSAGE_MAP()
};

class CTaskPane : public CDockablePane
{
public:
	CTaskPane();
	virtual ~CTaskPane();

// 操作
public:
	void AdjustLayout();

	// 模型信息卡 9 值入口（与属性窗格同源——CMainFrame::UpdateModelStats 双写）
	void UpdateModelStats(long nVertices, long nEdges, long nFaces,
		double dXMin, double dXMax, double dYMin, double dYMax,
		double dZMin, double dZMax);
	// 文件名/大小/简化状态/按钮可用性/大模型提示——从文档拉取刷新（pDoc=NULL 清空）
	void RefreshFromDocument(Cstl2stpcodeDoc* pDoc);
	// 未载入模型：数值清零、提示隐藏、按钮禁用（新建/载入失败/清除时调用）
	void ClearInfo();

// 实现
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnDestroy();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);   // P3·3.1：三卡容器绘制（挂 erase 而非 OnPaint，见 .cpp 注释）
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);   // P3·3.1：静态着色
	afx_msg void OnTaskApply();
	afx_msg void OnTaskUndo();
	afx_msg void OnTaskExportStep();
	afx_msg void OnTaskExportIges();
	afx_msg void OnTaskGotoSimp();
	afx_msg void OnTaskColor();      // 2.2：颜色按钮选中（CMFCColorButton 经 BN_CLICKED）
	afx_msg void OnTaskMaterial();   // 2.2：材料下拉 CBN_SELCHANGE
	afx_msg void OnSliderCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);   // P3·3.2：滑块自绘
	DECLARE_MESSAGE_MAP()

	// MFC 空闲命令 UI 走查：CDockablePane::OnUpdateCmdUI 会调
	// UpdateDialogControls(pTarget, bDisableIfNoHndler)——窗格内按钮的命令 ID
	// （IDC_TASK_*）在框架路由层无处理器，会被“无处理器即禁用”逻辑清成禁用
	//（BM_CLICK 仍可达，但鼠标不可点——runF 实测发现的真缺陷）。本窗格按钮
	// 可用性由 RefreshFromDocument/ClearInfo 显式管理，覆盖为直通（防再犯：
	// 机制定位见 afxdockablepane.cpp:2767 + wincore.cpp:4541）。
	virtual void OnUpdateCmdUI(class CFrameWnd* pTarget, int bDisableIfNoHndler);

	// 三卡标题（P3·3.1：BS_GROUPBOX 退役；卡片容器圆角矩形由 OnPaint 绘制）
	CStatic  m_stTitleInfo;
	CStatic  m_stTitleSimp;
	CStatic  m_stTitleExport;
	// 模型信息卡（12 行静态）
	CStatic  m_stFilename;
	CStatic  m_stFilesize;
	CStatic  m_stSimpState;
	CStatic  m_stV;
	CStatic  m_stE;
	CStatic  m_stF;
	CStatic  m_stXMin;
	CStatic  m_stXMax;
	CStatic  m_stYMin;
	CStatic  m_stYMax;
	CStatic  m_stZMin;
	CStatic  m_stZMax;
	// 形态属性（2.2 自属性窗格迁入模型信息卡；Doc API 与 P1 用户实测链路零改动）
	CStatic        m_stLblColor;
	CMFCColorButton m_btnColor;      // 颜色（“默认”=纯黄；选中后经 BN_CLICKED 通知）
	CStatic        m_stLblMaterial;
	CTaskCombo      m_cboMaterial;    // 无/材料1..3 → DEFAULT/BRASS/STEEL/SILVER（P4 自绘换肤）
	// 网格简化卡
	CStatic  m_stRatio;        // 保留比例 % 值（滑块联动）
	CSliderCtrl m_slider;      // 保留比例 1-100（默认 60）
	CStatic  m_stEstimate;     // 预计简化后的三角面数（round(比例×F)，与 IDD 333 估计同式）
	CFlatButton m_btnApply;
	CFlatButton m_btnUndo;
	CStatic  m_stSimpHint;     // 简化卡说明行（内存替换/可撤销语义）
	// 导出卡
	CFlatButton m_btnExportStep;
	CFlatButton m_btnExportIges;
	CStatic  m_stBigHint;      // >10 万面内联提示（advisory）
	CFlatButton m_btnGotoSimp;

	CFont    m_fontTitle;      // 卡片标题/预计面数（9pt 加粗，4.3.2 着色 textDim/accent）
	CFont    m_fontValue;      // V/E/F 三行大数字（13pt 加粗；文本串不变，保断言链）
	int      m_nCurrentFaces = 0;   // 当前面数（预计面数联动基数，与统计同源）
	BOOL     m_bHasModel = FALSE;
	BOOL     m_bStepPreview = FALSE;   // 【P4·V】STEP 预览态（简化/导出输入件禁用依据）
	int      m_nScrollPos = 0;      // 纵向滚动偏移（内容超高时）
	int      m_nContentH = 0;      // 最近一次布局的内容总高（滚动范围用）
	CRect    m_rcCard[3];     // 三卡容器（内容坐标系；绘制与子件定位统一减滚动偏移）
	CRect    m_rcWarn;        // >10 万面 warn 内衬盒（隐藏时置空）

	void     CreateLabel(CStatic& st, const wchar_t* lpszText, UINT nID);
	void     DrawCards(CDC* pDC);     // P3·3.1：卡片容器+warn 内衬盒（OnEraseBkgnd 调）
	void     UpdateEstimate();
	void     UpdateScrollRange();
	Cstl2stpcodeDoc* GetActiveDoc() const;
};
