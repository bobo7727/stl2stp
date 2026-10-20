#pragma once

#include "ThemeTokens.h"   // STaskPal 令牌（DrawCaptionBtns 用）


// BrandBar.h —— P4·B 品牌标题栏（设计稿 4.5）：35px 品牌行替代 CMFCMenuBar
// 窗格行。自绘 logo/应用名/菜单项/主题·帮助图标（徽章已移除，P4·O）；菜单点击 TrackPopupMenu
// 弹 IDR_stl2stpcodeTYPE 子菜单（owner=主框架 → WM_INITMENUPOPUP 走 MFC 原生
// 路由：MRU 占位自动刷新、命令照常 WM_COMMAND 进框架映射；弹出为系统 #32768
// 原生菜单，配色随系统主题——有意偏差，AGENT.md 已登记）。
// 【实测教训，补充二十二】基类必须是 CMFCToolBar 一系：CDockablePane 停靠
// CBRS_TOP 时落进"窗格带"（永远在工具栏行之下，首版 29/29 像素验证后经整窗
// 截图复核发现）；CMFCMenuBar/CMFCToolBar 一系才停进"控制栏带"——标题栏正
// 下方，即原菜单栏行位置。类是带区的决定因素，与 DockPane 调用顺序无关。
// 系统标题栏保留（最小化/关闭/窗口动画所需；深色下已随 DWM 属性变深）。

class CBrandBar : public CMFCToolBar
{
public:
	CBrandBar();
	virtual ~CBrandBar();

	// 菜单入口唯一载体——防关闭/防隐藏误失
	virtual BOOL CanBeClosed() const { return FALSE; }
	// 品牌行是零按钮工具栏：CMFCToolBar 按按钮尺寸算行高，0 按钮会把行压扁，
	// 行高在此固定 35px（设计稿 4.5 规格）
	virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz);
	// 【P4·D 修复：停靠状态腐蚀】固定 chrome 不参与停靠状态持久化（同
	// CToolStrip）：保存的 RectRecentDocked 会把行宽腐蚀成与工具行同排的
	// 1880px，还原后右侧留 40px 灰缝；本行不可拖动、布局由 OnCreate 的
	// DockPane 顺序唯一决定——两函数置空杜绝整类腐蚀（见补充二十四）。
	virtual BOOL LoadState(LPCTSTR lpszProfileName = NULL, int nIndex = -1, UINT uiID = (UINT) -1) { return TRUE; }
	virtual BOOL SaveState(LPCTSTR lpszProfileName = NULL, int nIndex = -1, UINT uiID = (UINT) -1) { return TRUE; }

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp);
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnMouseLeave();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	DECLARE_MESSAGE_MAP()

	void DrawLogo(CDC* pDC, const CRect& rc);       // 渐变圆角方块+白线描立方体
	void DrawSunGlyph(CDC* pDC, const CRect& rc, COLORREF cr);
	void DrawHelpGlyph(CDC* pDC, const CRect& rc, COLORREF cr);
	void DrawCaptionBtns(CDC* pDC, const STaskPal& pal);   // P4·E：自绘三键
	int  HitTest(const CPoint& pt) const;           // -1 无 / 0..3 菜单 / 4 主题 / 5 帮助 / 6..8 标题键

	CMenu   m_menu;            // IDR_stl2stpcodeTYPE 全菜单（弹出子菜单用）
	CFont   m_fontName;        // 应用名（加粗）
	CFont   m_fontMenu;         // 菜单项/图标字（常规）
	int     m_nHotItem = -1;   // hover 热区索引
	BOOL    m_bTrack = FALSE;  // TrackMouseEvent 已登记
	CRect   m_rcMenus[4];      // 菜单项热区（OnPaint 时测量）
	CRect   m_rcTheme;          // 主题切换图标热区
	CRect   m_rcHelp;           // 帮助图标热区
	CRect   m_rcCap[3];         // P4·E：标题键热区（0=最小化 1=最大化/还原 2=关闭，各 46px）
};
