#include "stdafx.h"

#include "ToolStrip.h"
#include "Resource.h"
#include "stl2stp-code.h"
#include "stl2stp-codeDoc.h"    // 步骤条/禁用态/网格激活态从文档现读
#include "ThemeTokens.h"        // P4·B 起的共享视觉令牌（浅/深双套）
#include "ExportStrings.h"      // newui::toolStrip 文案（开发约定：界面字符串入此）
#include <afxpriv.h>            // WM_IDLEUPDATECMDUI

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CToolStrip —— P4·C 工具行（规格=原型工具栏行：44px 深底 + 步骤条 + 按钮组）

BEGIN_MESSAGE_MAP(CToolStrip, CMFCToolBar)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_NCCALCSIZE()
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSELEAVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_MESSAGE(WM_IDLEUPDATECMDUI, &CToolStrip::OnIdleUpdateCmdUI)
END_MESSAGE_MAP()

// 按钮表：顺序即绘制顺序（与 m_rcBtns 一一对应）。
// 命令 ID 全部为原版语义：32773 导入STL / 32828 导出STEP / 32826 导出IGES /
// 32837 网格简化（弹原版 IDD 333 对话框，与菜单路径同语义）/ 32785 适应 /
// 32807 参考网格（激活态读文档 m_bGrid）；视图/效果族走下拉程序化菜单。
const CToolStrip::SBtn CToolStrip::s_btns[] =
{
	{ ID_IMPORT_STL_MODEL, POP_NONE,    ICO_FOLDER, newui::toolStrip::kOpenStl,    FALSE, FALSE, FALSE },
	{ ID_EXPORT_STEP,      POP_NONE,    ICO_DOWN,   newui::toolStrip::kExportStep, TRUE,  TRUE,  FALSE },
	{ ID_EXPORT_IGES,      POP_NONE,    ICO_DOWN,   newui::toolStrip::kExportIges, FALSE, TRUE,  FALSE },
	{ ID_MESH_SIMPLIFY,    POP_NONE,    ICO_SIMP,   newui::toolStrip::kSimplify,   FALSE, TRUE,  FALSE },
	{ ID_VIEW_FITALL,      POP_NONE,    ICO_FIT,    newui::toolStrip::kFit,        FALSE, TRUE,  TRUE  },
	{ 0,                   POP_VIEW,   ICO_CUBE,   newui::toolStrip::kView,        FALSE, FALSE, FALSE },
	{ ID_VIEW_GRID,        POP_NONE,    ICO_GRID,   L"",                           FALSE, FALSE, FALSE },
	{ 0,                   POP_DISPLAY, ICO_EYE,    newui::toolStrip::kDisplay,     FALSE, FALSE, FALSE },
};

CToolStrip::CToolStrip()
{
}

CToolStrip::~CToolStrip()
{
}

int CToolStrip::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMFCToolBar::OnCreate(lpCreateStruct) == -1)
		return -1;

	LOGFONT lf;
	::ZeroMemory(&lf, sizeof(lf));
	afxGlobalData.fontRegular.GetLogFont(&lf);
	m_font.CreateFontIndirect(&lf);
	LOGFONT lfBold = lf;
	lfBold.lfWeight = FW_BOLD;
	m_fontBold.CreateFontIndirect(&lfBold);
	// 状态同步双保险之二：500ms 重绘定时器。空闲消息（OnIdleUpdateCmdUI）
	// 在长时间渲染阻塞后不可靠，定时器保证任何文档状态变化在半秒内上屏
	SetTimer(1, 500, NULL);
	return 0;
}

void CToolStrip::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == 1)
		Invalidate(FALSE);
	CMFCToolBar::OnTimer(nIDEvent);
}

void CToolStrip::OnDestroy()
{
	KillTimer(1);
	CMFCToolBar::OnDestroy();
}

CSize CToolStrip::CalcFixedLayout(BOOL /*bStretch*/, BOOL bHorz)
{
	// 行高固定 44px（设计稿 4.6）。横向恒返超宽占满整行（零按钮工具栏按按钮
	// 内容算宽会塌成 0 宽，补充二十二取证）；纵向停靠不会发生（空白点击吞掉，
	// 禁拖浮动）。
	return bHorz ? CSize(100000, 44) : CSize(44, 100000);
}

void CToolStrip::OnSize(UINT nType, int cx, int cy)
{
	CMFCToolBar::OnSize(nType, cx, cy);
	Invalidate(FALSE);
}

// 品牌行同款：CPane 系非客户区预留会吞掉整窗（实测客户区可为负高度），
// 交回系统默认 NCCALCSIZE——无边框子窗口客户区=整窗，44px 全归自绘
void CToolStrip::OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp)
{
	UNREFERENCED_PARAMETER(bCalcValidRects);
	UNREFERENCED_PARAMETER(lpncsp);
	Default();
}

BOOL CToolStrip::OnEraseBkgnd(CDC* pDC)
{
	CRect rc;
	GetClientRect(rc);
	pDC->FillSolidRect(rc, TaskPal().crHeaderBg);
	return TRUE;
}

Cstl2stpcodeDoc* CToolStrip::GetDoc()
{
	CFrameWnd* pFrame = (CFrameWnd*)AfxGetMainWnd();
	if (pFrame == NULL)
		return NULL;
	return (Cstl2stpcodeDoc*)pFrame->GetActiveDocument();
}

LRESULT CToolStrip::OnIdleUpdateCmdUI(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	// 步骤条/按钮禁用态/网格激活态全部文档现读——空闲即重绘，任何命令
	//（载入/简化/导出/网格切换）执行后消息队列一空立刻反映
	Invalidate(FALSE);
	return 0;
}

void CToolStrip::OnPaint()
{
	CPaintDC dc(this);
	const STaskPal& pal = TaskPal();

	CRect rc;
	GetClientRect(rc);
	if (rc.IsRectEmpty() || rc.Height() < 10)
		return;
	const int H = rc.Height();
	dc.SetBkMode(TRANSPARENT);
	// 全幅自绘铺底：刷新一律 Invalidate(FALSE)（不触发 WM_ERASEBKGND），若只在
	// OnEraseBkgnd 铺底，状态切换的旧像素会永久残留——实测"当前步 accent 实心
	// 圆"变"完成绿环"后残留蓝底、网格 tile 去活后残留灰底、hover 消失后残留
	// 高亮圆角。OnPaint 每笔完整覆盖整条，杜绝残影（补充二十三取证）
	dc.FillSolidRect(rc, pal.crHeaderBg);

	Cstl2stpcodeDoc* pDoc = GetDoc();
	const BOOL bLoaded = (pDoc != NULL && pDoc->m_bLoaded != FALSE);

	// 步骤条状态：①载入 ②简化（m_hTriangOrig 非空，撤销即回退） ③已导出
	BOOL bDone[3] = { FALSE, FALSE, FALSE };
	bDone[0] = bLoaded;
	bDone[1] = (pDoc != NULL && !pDoc->m_hTriangOrig.IsNull());
	bDone[2] = (pDoc != NULL && pDoc->m_bExported != FALSE);
	int nCur = 0;
	while (nCur < 3 && bDone[nCur])
		++nCur;   // 3 = 全部完成
	// 完成步绿字（深 #38A96E / 浅 #2E9E64；原型实测 rgb(56,169,110)）
	const COLORREF crGreen =
		(theApp.GetInt(_T("DarkTheme"), 0) != 0) ? RGB(56, 169, 110) : RGB(46, 158, 100);

	// 1) 步骤条：20px 圆 + 文字；完成=绿描边绿✓，当前=accent 实心胶囊描边，
	//    未达=dim 描边
	static const wchar_t* const s_stepLbl[3] =
	{
		newui::toolStrip::kStep1, newui::toolStrip::kStep2, newui::toolStrip::kStep3
	};
	dc.SelectObject(&m_font);
	int x = 16;
	for (int i = 0; i < 3; ++i)
	{
		CRect rcC(x, 12, x + 20, 32);
		CString strLbl(s_stepLbl[i]);
		const CSize szLbl = dc.GetTextExtent(strLbl);
		CRect rcLbl(rcC.right + 6, 0, rcC.right + 6 + szLbl.cx + 10, H);
		m_rcSteps[i].SetRect(rcC.left - 3, 0, rcLbl.right, H);
		if (i == nCur)
		{
			// 当前步：圆角胶囊 1px accent 描边（原型当前步骤高亮）
			CPen penCur(PS_SOLID, 1, pal.crAccent);
			CBrush brNone;
			brNone.CreateStockObject(NULL_BRUSH);
			CPen* pP = dc.SelectObject(&penCur);
			CBrush* pB = dc.SelectObject(&brNone);
			CRect rcPill(m_rcSteps[i]);
			rcPill.DeflateRect(1, 4);
			dc.RoundRect(rcPill, CPoint(12, 12));
			dc.SelectObject(pB);
			dc.SelectObject(pP);
		}
		if (bDone[i])
		{
			CPen pen(PS_SOLID, 2, crGreen);
			CBrush brNone;
			brNone.CreateStockObject(NULL_BRUSH);
			CPen* pP = dc.SelectObject(&pen);
			CBrush* pB = dc.SelectObject(&brNone);
			dc.Ellipse(rcC);
			dc.SelectObject(pB);
			dc.SelectObject(pP);
			DrawCheckGlyph(&dc, rcC, crGreen);
		}
		else if (i == nCur)
		{
			CBrush br(pal.crAccent);
			CPen penNull(PS_NULL, 1, RGB(0, 0, 0));
			CPen* pP = dc.SelectObject(&penNull);
			CBrush* pB = dc.SelectObject(&br);
			dc.Ellipse(rcC);
			dc.SelectObject(pB);
			dc.SelectObject(pP);
			CString strN;
			strN.Format(_T("%d"), i + 1);
			dc.SetTextColor(RGB(255, 255, 255));
			dc.DrawText(strN, rcC, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		}
		else
		{
			CPen pen(PS_SOLID, 2, pal.crDim);
			CBrush brNone;
			brNone.CreateStockObject(NULL_BRUSH);
			CPen* pP = dc.SelectObject(&pen);
			CBrush* pB = dc.SelectObject(&brNone);
			dc.Ellipse(rcC);
			dc.SelectObject(pB);
			dc.SelectObject(pP);
			CString strN;
			strN.Format(_T("%d"), i + 1);
			dc.SetTextColor(pal.crDim);
			dc.DrawText(strN, rcC, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		}
		dc.SetTextColor(bDone[i] ? crGreen : ((i == nCur) ? pal.crText : pal.crFaint));
		dc.DrawText(strLbl, rcLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
		x = m_rcSteps[i].right + 4;
		if (i < 2)
		{
			dc.SetTextColor(pal.crFaint);
			dc.DrawText(_T("›"), CRect(x, 0, x + 12, H), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			x += 16;
		}
	}
	x += 14;

	// 2) 按钮组
	for (int b = 0; b < (int)_countof(s_btns); ++b)
	{
		const SBtn& btn = s_btns[b];
		if (btn.bAfterSep)
		{
			x += 12;
			dc.FillSolidRect(x, 13, 1, 18, pal.crHeaderLine);
			x += 13;
		}
		const BOOL bDisabled = (btn.bNeedsModel && !bLoaded);
		BOOL bActive = FALSE;
		if (btn.nCmd == ID_VIEW_GRID && pDoc != NULL)
			bActive = (pDoc->m_bGrid != 0);

		dc.SelectObject(btn.bPrimary ? &m_fontBold : &m_font);
		CString strLbl(btn.pszLabel);
		const BOOL bHasLabel = (btn.pszLabel[0] != 0);
		CSize szLbl(0, 0);
		if (bHasLabel)
			szLbl = dc.GetTextExtent(strLbl);
		const int nCaret = (btn.nPopup != POP_NONE) ? 12 : 0;
		const int cxBtn = 11 + 16 + (bHasLabel ? 6 + szLbl.cx : 0) + nCaret + 11;
		CRect rcB(x, 7, x + cxBtn, 37);
		m_rcBtns[b] = rcB;
		const CRect rcIcon(rcB.left + 11, 14, rcB.left + 11 + 16, 30);

		if (btn.bPrimary)
		{
			// 主按钮：accent 实心圆角 + 白粗字；无模型禁用态用弱化底
			CBrush br(bDisabled ? pal.crMenuHover : pal.crAccent);
			CPen penNull(PS_NULL, 1, RGB(0, 0, 0));
			CPen* pP = dc.SelectObject(&penNull);
			CBrush* pB = dc.SelectObject(&br);
			dc.RoundRect(rcB, CPoint(8, 8));
			dc.SelectObject(pB);
			dc.SelectObject(pP);
			DrawIcon(&dc, btn.nIcon, rcIcon, bDisabled ? pal.crDim : RGB(255, 255, 255));
			dc.SetTextColor(bDisabled ? pal.crFaint : RGB(255, 255, 255));
			// 【P4·O】文字画在图标右侧（与次按钮同布局）——此前 DT_CENTER 于
			// 整按钮矩形，居中文字与左侧 16px 图标重叠（用户反馈"下载箭头和
			// 文字重叠没有对齐"）；按钮宽式 11+16+6+szLbl+11 与此布局自洽
			if (bHasLabel)
			{
				CRect rcT(rcIcon.right + 6, 0, rcB.right - nCaret, H);
				dc.DrawText(strLbl, rcT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
			}
		}
		else
		{
			if (m_nHotItem == b && !bDisabled)
			{
				CBrush brHot(pal.crMenuHover);
				CPen penNull(PS_NULL, 1, RGB(0, 0, 0));
				CPen* pP = dc.SelectObject(&penNull);
				CBrush* pB = dc.SelectObject(&brHot);
				dc.RoundRect(rcB, CPoint(8, 8));
				dc.SelectObject(pB);
				dc.SelectObject(pP);
			}
			else if (bActive)
			{
				CBrush brOn(pal.crMenuHover);
				CPen penNull(PS_NULL, 1, RGB(0, 0, 0));
				CPen* pP = dc.SelectObject(&penNull);
				CBrush* pB = dc.SelectObject(&brOn);
				dc.RoundRect(rcB, CPoint(8, 8));
				dc.SelectObject(pB);
				dc.SelectObject(pP);
			}
			const COLORREF crContent =
				bDisabled ? pal.crFaint : (bActive ? pal.crAccent : pal.crDim);
			DrawIcon(&dc, btn.nIcon, rcIcon, crContent);
			if (bHasLabel)
			{
				CRect rcT(rcIcon.right + 6, 0, rcB.right - nCaret, H);
				dc.SetTextColor(crContent);
				dc.DrawText(strLbl, rcT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
			}
			if (nCaret > 0)
			{
				CRect rcCaret(rcB.right - nCaret, 0, rcB.right - 2, H);
				dc.SetTextColor(crContent);
				dc.DrawText(_T("˅"), rcCaret, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			}
		}
		x = rcB.right + 4;
	}

	// 3) 底部 1px 发丝线（与品牌行同令牌）
	dc.FillSolidRect(0, H - 1, rc.Width(), 1, pal.crHeaderLine);
}

// 16px 线描图标（2px 笔，品牌栏白色立方体同技法）
void CToolStrip::DrawIcon(CDC* pDC, int nIcon, const CRect& rc, COLORREF cr)
{
	const int l = rc.left, t = rc.top;
	CPen pen(PS_SOLID, 2, cr);
	CPen* pPenOld = pDC->SelectObject(&pen);
	switch (nIcon)
	{
	case ICO_FOLDER:
		pDC->MoveTo(l + 1, t + 5); pDC->LineTo(l + 1, t + 14); pDC->LineTo(l + 15, t + 14);
		pDC->LineTo(l + 15, t + 5); pDC->LineTo(l + 8, t + 5); pDC->LineTo(l + 6, t + 3);
		pDC->LineTo(l + 1, t + 3); pDC->LineTo(l + 1, t + 5);
		break;
	case ICO_DOWN:
		pDC->MoveTo(l + 8, t + 2); pDC->LineTo(l + 8, t + 9);
		pDC->MoveTo(l + 4, t + 6); pDC->LineTo(l + 8, t + 10); pDC->LineTo(l + 12, t + 6);
		pDC->MoveTo(l + 2, t + 13); pDC->LineTo(l + 2, t + 15); pDC->LineTo(l + 14, t + 15);
		pDC->LineTo(l + 14, t + 13);
		break;
	case ICO_SIMP:
		pDC->MoveTo(l + 3, t + 3); pDC->LineTo(l + 13, t + 3);
		pDC->MoveTo(l + 8, t + 3); pDC->LineTo(l + 8, t + 13);
		pDC->MoveTo(l + 5, t + 13); pDC->LineTo(l + 11, t + 13);
		break;
	case ICO_FIT:
		pDC->MoveTo(l + 1, t + 5); pDC->LineTo(l + 1, t + 1); pDC->LineTo(l + 5, t + 1);
		pDC->MoveTo(l + 11, t + 1); pDC->LineTo(l + 15, t + 1); pDC->LineTo(l + 15, t + 5);
		pDC->MoveTo(l + 15, t + 11); pDC->LineTo(l + 15, t + 15); pDC->LineTo(l + 11, t + 15);
		pDC->MoveTo(l + 5, t + 15); pDC->LineTo(l + 1, t + 15); pDC->LineTo(l + 1, t + 11);
		break;
	case ICO_CUBE:
		pDC->MoveTo(l + 8, t + 2); pDC->LineTo(l + 13, t + 5); pDC->LineTo(l + 8, t + 8);
		pDC->LineTo(l + 3, t + 5); pDC->LineTo(l + 8, t + 2);
		pDC->MoveTo(l + 13, t + 5); pDC->LineTo(l + 13, t + 11); pDC->LineTo(l + 8, t + 14);
		pDC->LineTo(l + 3, t + 11); pDC->LineTo(l + 3, t + 5);
		pDC->MoveTo(l + 8, t + 8); pDC->LineTo(l + 8, t + 14);
		break;
	case ICO_GRID:
		pDC->MoveTo(l + 6, t + 1); pDC->LineTo(l + 6, t + 15);
		pDC->MoveTo(l + 10, t + 1); pDC->LineTo(l + 10, t + 15);
		pDC->MoveTo(l + 1, t + 6); pDC->LineTo(l + 15, t + 6);
		pDC->MoveTo(l + 1, t + 10); pDC->LineTo(l + 15, t + 10);
		break;
	case ICO_EYE:
		{
			CBrush brNone;
			brNone.CreateStockObject(NULL_BRUSH);
			CBrush* pB = pDC->SelectObject(&brNone);
			pDC->Ellipse(l + 1, t + 4, l + 15, t + 12);
			pDC->SelectObject(pB);
			CBrush br(cr);
			pDC->SelectObject(&br);
			pDC->Ellipse(l + 6, t + 6, l + 10, t + 10);
			pDC->SelectObject(pB);
		}
		break;
	}
	pDC->SelectObject(pPenOld);
}

// 步骤条完成态 ✓（20px 圆内）
void CToolStrip::DrawCheckGlyph(CDC* pDC, const CRect& rc, COLORREF cr)
{
	CPen pen(PS_SOLID, 2, cr);
	CPen* pPenOld = pDC->SelectObject(&pen);
	pDC->MoveTo(rc.left + 4, rc.top + 10);
	pDC->LineTo(rc.left + 8, rc.top + 14);
	pDC->LineTo(rc.left + 15, rc.top + 5);
	pDC->SelectObject(pPenOld);
}

int CToolStrip::HitTest(const CPoint& pt) const
{
	for (int b = 0; b < (int)_countof(s_btns); ++b)
		if (m_rcBtns[b].PtInRect(pt))
			return b;
	return -1;
}

void CToolStrip::OnMouseMove(UINT nFlags, CPoint point)
{
	// 不调基类：CMFCToolBar 的按钮 hover 机制对零按钮工具行无意义
	if (!m_bTrack)
	{
		TRACKMOUSEEVENT tme;
		::ZeroMemory(&tme, sizeof(tme));
		tme.cbSize = sizeof(tme);
		tme.dwFlags = TME_LEAVE;
		tme.hwndTrack = GetSafeHwnd();
		if (::TrackMouseEvent(&tme))
			m_bTrack = TRUE;
	}
	const int nHit = HitTest(point);
	if (nHit != m_nHotItem)
	{
		m_nHotItem = nHit;
		Invalidate(FALSE);
	}
}

void CToolStrip::OnMouseLeave()
{
	m_bTrack = FALSE;
	if (m_nHotItem != -1)
	{
		m_nHotItem = -1;
		Invalidate(FALSE);
	}
}

void CToolStrip::OnLButtonDown(UINT nFlags, CPoint point)
{
	const int nHit = HitTest(point);
	if (nHit >= 0)
	{
		const SBtn& btn = s_btns[nHit];
		Cstl2stpcodeDoc* pDoc = GetDoc();
		BOOL bDisabled = (btn.bNeedsModel && (pDoc == NULL || pDoc->m_bLoaded == FALSE));
		if (!bDisabled)
		{
			CWnd* pMain = AfxGetMainWnd();
			if (btn.nPopup != POP_NONE)
			{
				CPoint pt(m_rcBtns[nHit].left, m_rcBtns[nHit].bottom);
				ShowPopup(btn.nPopup, pt);
			}
			else if (pMain != NULL)
			{
				// 命令照常 WM_COMMAND 进框架映射（守卫/确认框基线不动）
				pMain->PostMessage(WM_COMMAND, MAKEWPARAM(btn.nCmd, 0), 0);
			}
		}
		m_nHotItem = -1;
		Invalidate(FALSE);
		return;
	}
	// 非按钮区点击：停靠态吞掉（chrome 条防拖浮起）；浮动态走基类
	if (IsFloating())
	{
		CMFCToolBar::OnLButtonDown(nFlags, point);
	}
}

BOOL CToolStrip::ShowPopup(int nPopup, const CPoint& pt)
{
	CMenu menu;
	if (!menu.CreatePopupMenu())
		return FALSE;
	Cstl2stpcodeDoc* pDoc = GetDoc();
	if (nPopup == POP_VIEW)
	{
		menu.AppendMenu(MF_STRING, ID_VIEW_PROJ_FRONT, newui::toolStrip::kVFront);
		menu.AppendMenu(MF_STRING, ID_VIEW_PROJ_BACK,  newui::toolStrip::kVBack);
		menu.AppendMenu(MF_STRING, ID_VIEW_PROJ_LEFT,  newui::toolStrip::kVLeft);
		menu.AppendMenu(MF_STRING, ID_VIEW_PROJ_RIGHT, newui::toolStrip::kVRight);
		menu.AppendMenu(MF_STRING, ID_VIEW_PROJ_TOP,   newui::toolStrip::kVTop);
		menu.AppendMenu(MF_STRING, ID_VIEW_PROJ_BOTTOM, newui::toolStrip::kVBottom);
		menu.AppendMenu(MF_STRING, ID_VIEW_PROJ_AXO,   newui::toolStrip::kVAxo);
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, ID_VIEW_RESET,       newui::toolStrip::kVReset);
	}
	else
	{
		// 显示˅：效果族命令；复选态读文档渲染标志（复刻原版按钮状态语义）
		menu.AppendMenu(MF_STRING | ((pDoc != NULL && pDoc->m_bGrid != 0) ? MF_CHECKED : 0),
			ID_VIEW_GRID, newui::toolStrip::kDGrid);
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, ID_HLR_ON,  newui::toolStrip::kDHlrOn);
		menu.AppendMenu(MF_STRING, ID_HLR_OFF, newui::toolStrip::kDHlrOff);
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING | ((pDoc != NULL && pDoc->m_bRaytracing != 0) ? MF_CHECKED : 0),
			ID_VIEW_RAYTRACING, newui::toolStrip::kDRaytrace);
		menu.AppendMenu(MF_STRING | ((pDoc != NULL && pDoc->m_bShadow != 0) ? MF_CHECKED : 0),
			ID_VIEW_SHADOW, newui::toolStrip::kDShadow);
		menu.AppendMenu(MF_STRING | ((pDoc != NULL && pDoc->m_bReflection != 0) ? MF_CHECKED : 0),
			ID_VIEW_REFLECTION, newui::toolStrip::kDReflection);
		menu.AppendMenu(MF_STRING | ((pDoc != NULL && pDoc->m_bAntialiasing != 0) ? MF_CHECKED : 0),
			ID_VIEW_ANTIALIASING, newui::toolStrip::kDAntialias);
	}
	CWnd* pMain = AfxGetMainWnd();
	if (pMain == NULL)
		return FALSE;
	CPoint ptScreen(pt);
	ClientToScreen(&ptScreen);
	return menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_LEFTBUTTON,
		ptScreen.x, ptScreen.y, pMain, NULL);
}
