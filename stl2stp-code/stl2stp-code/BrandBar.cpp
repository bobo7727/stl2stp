#include "stdafx.h"

#include "BrandBar.h"
#include "Resource.h"
#include "MainFrm.h"
#include "stl2stp-code.h"
#include "ThemeTokens.h"
#include "ExportStrings.h"      // newui::brand 文案（开发约定：界面字符串入此）

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CBrandBar —— P4·B 品牌标题栏（规格=原型头部：35px 深底品牌行）

BEGIN_MESSAGE_MAP(CBrandBar, CMFCToolBar)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_NCCALCSIZE()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSELEAVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()

CBrandBar::CBrandBar()
{
}

CBrandBar::~CBrandBar()
{
}

int CBrandBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMFCToolBar::OnCreate(lpCreateStruct) == -1)
		return -1;

	if (!m_menu.LoadMenu(IDR_stl2stpcodeTYPE))
	{
		TRACE0("品牌标题栏：未能加载主菜单\n");
		return -1;
	}

	LOGFONT lf;
	::ZeroMemory(&lf, sizeof(lf));
	afxGlobalData.fontRegular.GetLogFont(&lf);
	LOGFONT lfBold = lf;
	lfBold.lfWeight = FW_BOLD;
	m_fontName.CreateFontIndirect(&lfBold);
	m_fontMenu.CreateFontIndirect(&lf);
	return 0;
}

void CBrandBar::OnSize(UINT nType, int cx, int cy)
{
	CMFCToolBar::OnSize(nType, cx, cy);
	Invalidate(FALSE);
}

// 品牌行按纯客户端 chrome 条处理：CPane/CMFCToolBar 系的非客户区计算会为
// 停靠条预留"抓手+渐变带"，35px 窗口实测被吃到客户区 1910x-3——自绘 OnPaint
// 撞上"高度不足"早退，热区（菜单/主题/帮助）永远测不出，整条可见带全是
// 非客户区自带的 Office 渐变+拖拽点（补充二十二取证）。交回系统默认
// NCCALCSIZE：无边框子窗口 → 客户区=整窗，35px 全归自绘。
void CBrandBar::OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp)
{
	UNREFERENCED_PARAMETER(bCalcValidRects);
	UNREFERENCED_PARAMETER(lpncsp);
	Default();
}

BOOL CBrandBar::OnEraseBkgnd(CDC* pDC)
{
	CRect rc;
	GetClientRect(rc);
	pDC->FillSolidRect(rc, TaskPal().crHeaderBg);
	return TRUE;
}

void CBrandBar::OnPaint()
{
	CPaintDC dc(this);
	const STaskPal& pal = TaskPal();

	CRect rc;
	GetClientRect(rc);
	if (rc.IsRectEmpty() || rc.Height() < 10)
		return;
	const int H = rc.Height();
	dc.SetBkMode(TRANSPARENT);
	// 全幅自绘铺底（P4·C 补的课）：刷新一律 Invalidate(FALSE) 不触发
	// WM_ERASEBKGND，若只依赖 erase 铺底，菜单项 hover 高亮移开后会残留
	// 圆角底影——OnPaint 每笔完整覆盖整条，杜绝残影
	dc.FillSolidRect(rc, pal.crHeaderBg);

	// 1) logo：20x20 渐变圆角方块（#5487FF→#8870FF）+ 白色线描立方体
	CRect rcLogo(8, (H - 20) / 2, 28, (H - 20) / 2 + 20);
	DrawLogo(&dc, rcLogo);

	// 2) 应用名（加粗）
	dc.SelectObject(&m_fontName);
	dc.SetTextColor(pal.crText);
	int x = rcLogo.right + 10;
	const CSize szName = dc.GetTextExtent(newui::brand::kAppName);
	CRect rcName(x, 0, x + szName.cx + 8, H);
	dc.DrawText(newui::brand::kAppName, rcName, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
	x = rcName.left + szName.cx + 15;
	dc.SelectObject(&m_fontMenu);

	// 3)【P4·O】"v2 概念稿"徽章已按用户指定移除（2026-09-15，原绘制见
	//    补充三十五）——菜单项起点保持原徽章右侧同量级的间隙
	x += 28;

	// 4) 菜单项（热区随绘制测量；hover 圆角高亮）
	for (int i = 0; i < 4; ++i)
	{
		CString strM(newui::brand::kMenus[i]);
		const CSize szM = dc.GetTextExtent(strM);
		CRect rcM(x, 0, x + szM.cx + 18, H);
		m_rcMenus[i] = rcM;
		if (m_nHotItem == i)
		{
			CBrush brHot(pal.crMenuHover);
			CPen penNull(PS_NULL, 1, RGB(0, 0, 0));
			CPen* pP = dc.SelectObject(&penNull);
			CBrush* pB = dc.SelectObject(&brHot);
			dc.RoundRect(rcM, CPoint(6, 6));
			dc.SelectObject(pB);
			dc.SelectObject(pP);
		}
		dc.SetTextColor(pal.crDim);
		dc.DrawText(strM, rcM, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		x = rcM.right;
	}

	// 5) 右端图标按钮（P4·E 布局更新）：最右 138px = 自绘标题三键（最小化/
	//    最大化-还原/关闭，各 46px 满高热区），主题/帮助图标退到三键左侧
	m_rcCap[2].SetRect(rc.right - 46, 0, rc.right, H);
	m_rcCap[1].SetRect(rc.right - 92, 0, rc.right - 46, H);
	m_rcCap[0].SetRect(rc.right - 138, 0, rc.right - 92, H);
	m_rcHelp.SetRect(rc.right - 138 - 9 - 30, 0, rc.right - 138 - 9, H);
	m_rcTheme.SetRect(m_rcHelp.left - 34, 0, m_rcHelp.left - 4, H);
	const COLORREF crIcon = ((m_nHotItem == 4) || (m_nHotItem == 5)) ? pal.crAccent : pal.crDim;
	DrawSunGlyph(&dc, m_rcTheme, crIcon);
	DrawHelpGlyph(&dc, m_rcHelp, crIcon);
	DrawCaptionBtns(&dc, pal);

	// 6) 底部 1px 发丝线（原型 #1F2734 一系）
	dc.FillSolidRect(0, H - 1, rc.Width(), 1, pal.crHeaderLine);
}

// 渐变圆角方块：圆角区域剪裁内逐行插值 #5487FF(顶)→#8870FF(底)（免 GDI+/msimg32
// 依赖——GradientFill 需链接 Msimg32.lib，逐行 FillSolidRect 等价且无副作用）
void CBrandBar::DrawLogo(CDC* pDC, const CRect& rc)
{
	CRgn rgn;
	rgn.CreateRoundRectRgn(rc.left, rc.top, rc.right + 1, rc.bottom + 1, 6, 6);
	pDC->SelectClipRgn(&rgn);
	const int hgt = rc.Height();
	for (int y = 0; y < hgt; ++y)
	{
		const double t = (hgt > 1) ? (double)y / (hgt - 1) : 0.0;
		const BYTE r = (BYTE)(0x54 + (0x88 - 0x54) * t);
		const BYTE g = (BYTE)(0x87 + (0x70 - 0x87) * t);
		pDC->FillSolidRect(rc.left, rc.top + y, rc.Width(), 1, RGB(r, g, 0xFF));
	}
	// SelectClipRgn 返回区域类型码而非旧区域——CPaintDC 画布无先存自设剪裁，
	// 直接置 NULL 复位即可
	pDC->SelectClipRgn(NULL);

	// 白色线描立方体：顶面菱形 + 三竖棱 + 底面 V
	CPen pen(PS_SOLID, 2, RGB(240, 242, 255));
	CPen* pPenOld = pDC->SelectObject(&pen);
	const int cx = rc.left + rc.Width() / 2;
	const int cy = rc.top + rc.Height() / 2;
	pDC->MoveTo(cx, cy - 6); pDC->LineTo(cx + 5, cy - 3);
	pDC->LineTo(cx, cy);     pDC->LineTo(cx - 5, cy - 3);
	pDC->LineTo(cx, cy - 6);
	pDC->MoveTo(cx + 5, cy - 3); pDC->LineTo(cx + 5, cy + 3);
	pDC->LineTo(cx, cy + 6);     pDC->LineTo(cx - 5, cy + 3);
	pDC->MoveTo(cx, cy);         pDC->LineTo(cx, cy + 6);
	pDC->MoveTo(cx - 5, cy - 3); pDC->LineTo(cx - 5, cy + 3);
	pDC->SelectObject(pPenOld);
}

// 太阳图标（主题切换）：圆 + 8 条光线
void CBrandBar::DrawSunGlyph(CDC* pDC, const CRect& rc, COLORREF cr)
{
	const CPoint c(rc.left + rc.Width() / 2, rc.top + rc.Height() / 2);
	CPen pen(PS_SOLID, 2, cr);
	CPen* pPenOld = pDC->SelectObject(&pen);
	pDC->SelectStockObject(NULL_BRUSH);
	pDC->Ellipse(c.x - 4, c.y - 4, c.x + 5, c.y + 5);
	static const int s_dx[8] = { 7, 5, 0, -5, -7, -5, 0, 5 };
	static const int s_dy[8] = { 0, 5, 7, 5, 0, -5, -7, -5 };
	for (int a = 0; a < 8; ++a)
	{
		pDC->MoveTo(c.x + s_dx[a], c.y + s_dy[a]);
		pDC->LineTo(c.x + s_dx[a] * 9 / 7, c.y + s_dy[a] * 9 / 7);
	}
	pDC->SelectObject(pPenOld);
}

// 帮助图标：外圆 + 中央问号
void CBrandBar::DrawHelpGlyph(CDC* pDC, const CRect& rc, COLORREF cr)
{
	const CPoint c(rc.left + rc.Width() / 2, rc.top + rc.Height() / 2);
	CPen pen(PS_SOLID, 2, cr);
	CPen* pPenOld = pDC->SelectObject(&pen);
	pDC->SelectStockObject(NULL_BRUSH);
	pDC->Ellipse(c.x - 8, c.y - 8, c.x + 9, c.y + 9);
	pDC->SelectObject(pPenOld);
	pDC->SetBkMode(TRANSPARENT);
	pDC->SetTextColor(cr);
	CRect rcT(c.x - 8, c.y - 8, c.x + 9, c.y + 9);
	pDC->DrawText(_T("?"), rcT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// 【新 UI P4·E】自绘标题三键（系统标题栏退役后品牌行兼任窗口标题栏）。
// 线描字形与系统规格一致：最小化=底对齐横线、最大化=11px 方框描边、还原=
// 前后叠两个方框、关闭=X 叉。hover：最小化/最大化用菜单同款圆角色
// crMenuHover；关闭沿用 Windows 惯例红底 (232,17,35)+白字。深浅主题字形
// 颜色均取 crText（与主题/帮助图标的 crDim 区分——标题键是系统件）。
void CBrandBar::DrawCaptionBtns(CDC* pDC, const STaskPal& pal)
{
	for (int i = 0; i < 3; ++i)
	{
		const CRect rc(m_rcCap[i]);
		if (rc.IsRectEmpty())
			continue;
		const BOOL bHot = (m_nHotItem == 6 + i);
		if (bHot)
			pDC->FillSolidRect(rc, (i == 2) ? RGB(232, 17, 35) : pal.crMenuHover);

		const COLORREF crGlyph = ((i == 2) && bHot) ? RGB(255, 255, 255) : pal.crText;
		CPen pen(PS_SOLID, 1, crGlyph);
		CPen* pPenOld = pDC->SelectObject(&pen);
		const int cx = rc.left + rc.Width() / 2;
		const int cy = rc.top + rc.Height() / 2;
		switch (i)
		{
		case 0:   // 最小化：10px 底对齐横线
			pDC->MoveTo(cx - 5, cy + 4);
			pDC->LineTo(cx + 6, cy + 4);
			break;
		case 2:   // 关闭：X 叉
			pDC->MoveTo(cx - 5, cy - 5);
			pDC->LineTo(cx + 6, cy + 6);
			pDC->MoveTo(cx - 5, cy + 5);
			pDC->LineTo(cx + 6, cy - 6);
			break;
		default:  // 最大化/还原
			pDC->SelectStockObject(NULL_BRUSH);
			{
				CWnd* pMain = AfxGetMainWnd();
				if (pMain == NULL || !::IsZoomed(pMain->GetSafeHwnd()))
				{
					pDC->Rectangle(cx - 5, cy - 5, cx + 6, cy + 6);
				}
				else
				{
					// 还原态：后层方框（右下）+ 前层方框（左上，先填条底色遮断后层边）
					pDC->Rectangle(cx - 4, cy - 1, cx + 4, cy + 6);
					CRect rcTop(cx - 8, cy - 5, cx + 1, cy + 3);
					pDC->FillSolidRect(rcTop, pal.crHeaderBg);
					pDC->Rectangle(rcTop);
				}
			}
			break;
		}
		pDC->SelectObject(pPenOld);
	}
}

int CBrandBar::HitTest(const CPoint& pt) const
{
	for (int i = 0; i < 4; ++i)
		if (m_rcMenus[i].PtInRect(pt))
			return i;
	if (m_rcTheme.PtInRect(pt))
		return 4;
	if (m_rcHelp.PtInRect(pt))
		return 5;
	for (int i = 0; i < 3; ++i)
		if (m_rcCap[i].PtInRect(pt))
			return 6 + i;
	return -1;
}

CSize CBrandBar::CalcFixedLayout(BOOL /*bStretch*/, BOOL bHorz)
{
	// 行高固定 35px（零按钮工具栏；设计稿 4.5 规格）。停靠行按 CalcFixedLayout
	// 返回值布置条带：工具栏一系默认按按钮内容算宽，0 按钮会塌成 0 宽（首版
	// 实测 0x35，品牌行 OnPaint 直接空转）——横向恒返回超宽占满整行，停靠
	// 管理器裁到行宽；纵向停靠不会发生（控制栏带空白点击吞掉，禁拖）。
	return bHorz ? CSize(100000, 35) : CSize(35, 100000);
}

void CBrandBar::OnMouseMove(UINT nFlags, CPoint point)
{
	// 不调基类：CMFCToolBar 的按钮 hover 机制对零按钮品牌行无意义，
	// 悬停高亮自行管理
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

void CBrandBar::OnMouseLeave()
{
	m_bTrack = FALSE;
	if (m_nHotItem != -1)
	{
		m_nHotItem = -1;
		Invalidate(FALSE);
	}
}

void CBrandBar::OnLButtonDown(UINT nFlags, CPoint point)
{
	const int nHit = HitTest(point);
	CWnd* pMain = AfxGetMainWnd();

	if (nHit >= 0 && nHit <= 3)
	{
		// 菜单项：TrackPopupMenu 弹对应子菜单。owner=主框架 → WM_INITMENUPOPUP
		// 走 MFC 原生路由：MRU 占位自动刷新、菜单命令照常 WM_COMMAND 进框架映射。
		// 【有意偏差】弹出的是系统 #32768 原生菜单，配色随系统主题而非应用深浅
		// 主题（CMFC 自绘菜单样式不再适用）——AGENT.md 已登记。
		CMenu* pSub = m_menu.GetSubMenu(nHit);
		if (pSub != NULL && pMain != NULL)
		{
			CPoint pt(m_rcMenus[nHit].left, m_rcMenus[nHit].bottom);
			ClientToScreen(&pt);
			pSub->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_LEFTBUTTON,
				pt.x, pt.y, pMain, NULL);
		}
		m_nHotItem = -1;
		Invalidate(FALSE);
		return;
	}
	if (nHit == 4)
	{
		if (pMain != NULL)
			pMain->PostMessage(WM_COMMAND, MAKEWPARAM(ID_VIEW_DARKTHEME, 0), 0);
		return;
	}
	if (nHit == 5)
	{
		CMenu* pSub = m_menu.GetSubMenu(3);   // "帮助"子菜单
		if (pSub != NULL && pMain != NULL)
		{
			CPoint pt(m_rcHelp.left, m_rcHelp.bottom);
			ClientToScreen(&pt);
			pSub->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_LEFTBUTTON,
				pt.x, pt.y, pMain, NULL);
		}
		m_nHotItem = -1;
		Invalidate(FALSE);
		return;
	}
	// 【新 UI P4·E】自绘标题三键：转发系统 SC_ 命令（任务栏/动画/Alt+F4
	// 语义全走系统通道，最大化-还原按当前态切换）
	if (nHit >= 6 && nHit <= 8)
	{
		if (pMain != NULL)
		{
			if (nHit == 6)
				pMain->PostMessage(WM_SYSCOMMAND, SC_MINIMIZE, 0);
			else if (nHit == 7)
				pMain->PostMessage(WM_SYSCOMMAND,
					::IsZoomed(pMain->GetSafeHwnd()) ? SC_RESTORE : SC_MAXIMIZE, 0);
			else
				pMain->PostMessage(WM_SYSCOMMAND, SC_CLOSE, 0);
		}
		return;
	}
	// 【新 UI P4·E】品牌行兼任窗口标题栏：非热区（logo/应用名/空白带，徽章已移除 P4·O）
	// 左键按下转主窗口 HTCAPTION 非客户命中——DefWindowProc 进入系统原生
	// 移动循环（拖动/Aero 贴边/双击语义随系统）。浮动态仍走基类：浮窗标题
	// 需要工具栏自己的拖动/复位停靠能力。
	if (IsFloating())
	{
		CMFCToolBar::OnLButtonDown(nFlags, point);
		return;
	}
	if (pMain != NULL)
	{
		CPoint ptS(point);
		ClientToScreen(&ptS);
		pMain->PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(ptS.x, ptS.y));
	}
}

// 【新 UI P4·E】空白区双击 = 最大化/还原切换（Windows 标题栏惯例；工具栏
// 类经 afxglobals RegisterWindowClass 注册，类样式带 CS_DBLCLKS，真实
// 双击可到达）
void CBrandBar::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	const int nHit = HitTest(point);
	if (nHit == -1 && !IsFloating())
	{
		CWnd* pMain = AfxGetMainWnd();
		if (pMain != NULL)
			pMain->PostMessage(WM_SYSCOMMAND,
				::IsZoomed(pMain->GetSafeHwnd()) ? SC_RESTORE : SC_MAXIMIZE, 0);
		return;
	}
	CMFCToolBar::OnLButtonDblClk(nFlags, point);
}
