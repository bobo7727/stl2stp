#pragma once

// ToolStrip.h —— P4·C 工具行（设计稿 4.6）：44px 自绘工具条，替代 STL 命令栏
// (59450)+视图命令栏(59451) 两旧工具栏。原型规格（用户红圈指定）：深底延续
// 品牌行 #151A24，左起步骤条「①载入模型 › ②检查/简化 › ③导出」+ 按钮组
// 「打开 STL / 导出 STEP(主按钮) / 导出 IGES / 网格简化 | 适应 / 视图˅ / 网格 /
// 显示˅」。命令全部 PostMessage WM_COMMAND 走既有命令链（守卫/确认框等取证
// 基线不动）；下拉按钮弹程序化菜单（文案 ExportStrings newui::toolStrip）。
// 【基类与停靠教训，同品牌栏（AGENT.md 补充二十二）】停靠带区由基类决定——
// 必须 CMFCToolBar 一系；零按钮工具栏须 CalcFixedLayout 横返超宽（默认按按钮
// 内容算宽会塌成 0 宽）；CPane 系 NC 预留会吞掉整窗（WM_NCCALCSIZE 交回
// Default()）。状态（步骤条/按钮禁用/网格激活）由文档现读，WM_IDLEUPDATECMDUI
// 空闲触发重绘保证命令后即同步。

class Cstl2stpcodeDoc;

class CToolStrip : public CMFCToolBar
{
public:
	CToolStrip();
	virtual ~CToolStrip();

	// 工具行是固定 chrome——防关闭/防隐藏误失
	virtual BOOL CanBeClosed() const { return FALSE; }
	// 行高固定 44px（设计稿 4.6 规格；零按钮工具栏见类注释）
	virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz);
	// 【P4·D 修复：停靠状态腐蚀】固定 chrome 不参与停靠状态持久化。实测退出
	// 保存的 RectRecentDocked 是初始 DockPane 阶段的瞬态窄条（40px、挤进品牌
	// 栏行右侧余隙），下次启动经 CWinAppEx::LoadState→虚调用还原，整行塌缩
	// 为 9px 灰带（p4d 双通道取证）。本栏用户不可拖动、布局由 OnCreate 的
	// DockPane 顺序唯一决定，状态存取无价值——两函数置空杜绝整类腐蚀。
	virtual BOOL LoadState(LPCTSTR lpszProfileName = NULL, int nIndex = -1, UINT uiID = (UINT) -1) { return TRUE; }
	virtual BOOL SaveState(LPCTSTR lpszProfileName = NULL, int nIndex = -1, UINT uiID = (UINT) -1) { return TRUE; }

protected:
	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnMouseLeave();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	afx_msg LRESULT OnIdleUpdateCmdUI(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()

	enum { ICO_FOLDER, ICO_DOWN, ICO_SIMP, ICO_FIT, ICO_CUBE, ICO_GRID, ICO_EYE };
	enum { POP_NONE = 0, POP_VIEW, POP_DISPLAY };

	struct SBtn
	{
		UINT nCmd;                // 命令 ID（nPopup 非 0 时忽略）
		int  nPopup;              // POP_NONE / POP_VIEW / POP_DISPLAY
		int  nIcon;               // 图标类型
		const wchar_t* pszLabel;  // 文案（空 = 仅图标按钮）
		BOOL bPrimary;            // 主按钮：crAccent 实心圆角+白粗字
		BOOL bNeedsModel;         // 无模型时禁用并吞点击（菜单路径守卫保留）
		BOOL bAfterSep;           // 按钮前画 1px 竖分隔线
	};

	void DrawIcon(CDC* pDC, int nIcon, const CRect& rc, COLORREF cr);
	void DrawCheckGlyph(CDC* pDC, const CRect& rc, COLORREF cr);
	BOOL ShowPopup(int nPopup, const CPoint& pt);
	Cstl2stpcodeDoc* GetDoc();
	int  HitTest(const CPoint& pt) const;   // 返回按钮索引；-1 = 非按钮区

	CFont   m_font;             // 按钮/步骤条文字
	CFont   m_fontBold;         // 主按钮文字
	int     m_nHotItem = -1;   // hover 按钮索引
	BOOL    m_bTrack = FALSE;  // TrackMouseEvent 已登记
	CRect   m_rcSteps[3];      // 步骤条三段（仅绘制测量，不可点）
	CRect   m_rcBtns[8];       // 按钮热区（与 s_btns 一一对应）
	static const SBtn s_btns[];
};
