#include "stdafx.h"
#include <afxcolorbutton.h>   // 2.2：模型信息卡颜色按钮（CMFCColorButton）

#include "TaskPane.h"
#include "Resource.h"
#include "MainFrm.h"
#include "stl2stp-code.h"
#include "stl2stp-codeDoc.h"    // 新 UI P2·2.1：任务卡从文档拉取状态
#include "ExportStrings.h"      // 三卡文案（newui::task，开发约定：界面字符串入此）
#include "ThemeTokens.h"        // P4·B：视觉令牌共享单元（4.3 定义迁出，BrandBar 同用）

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


/////////////////////////////////////////////////////////////////////////////
// CFlatButton —— P3·3.2 扁平按钮（设计定稿 4.3.2；令牌取本文件匿名命名空间）

BEGIN_MESSAGE_MAP(CFlatButton, CButton)
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSELEAVE()
END_MESSAGE_MAP()

BOOL CFlatButton::CreateFlat(const wchar_t* lpszCaption, const CRect& rc, CWnd* pParent,
	UINT nID, EStyle eStyle)
{
	m_eStyle = eStyle;
	return Create(lpszCaption, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
		rc, pParent, nID);
}

void CFlatButton::OnMouseMove(UINT nFlags, CPoint point)
{
	CButton::OnMouseMove(nFlags, point);
	if (!m_bTrack)
	{
		// hover 经 TrackMouseEvent（WM_MOUSELEAVE 复位）；无头 BM_CLICK 不依赖 hover
		TRACKMOUSEEVENT tme;
		::ZeroMemory(&tme, sizeof(tme));
		tme.cbSize = sizeof(tme);
		tme.dwFlags = TME_LEAVE;
		tme.hwndTrack = GetSafeHwnd();
		if (::TrackMouseEvent(&tme))
			m_bTrack = TRUE;
		m_bHover = TRUE;
		Invalidate(FALSE);
	}
}

void CFlatButton::OnMouseLeave()
{
	m_bTrack = FALSE;
	m_bHover = FALSE;
	Invalidate(FALSE);
	CButton::OnMouseLeave();
}

void CFlatButton::DrawItem(LPDRAWITEMSTRUCT pDIS)
{
	CDC* pDC = CDC::FromHandle(pDIS->hDC);
	CRect rc(pDIS->rcItem);
	const STaskPal& pal = TaskPal();
	const BOOL bEnabled = (pDIS->itemState & ODS_DISABLED) == 0;
	const BOOL bPressed = (pDIS->itemState & ODS_SELECTED) != 0;
	const BOOL bHover = m_bHover && bEnabled;

	// 配色（4.3.2）：primary=accent 底白字（hover/按压 accentStrong）；
	// sub=卡片底 +1px 描边（hover 描边/文字转 accent）；link=无描边文字链接；
	// 禁用统一褪色（底转 border-soft、文字 textFaint——原型 opacity .4 的近似）
	COLORREF crFill, crEdge, crText;
	switch (m_eStyle)
	{
	case ePrimary:
		crFill = bEnabled ? ((bHover || bPressed) ? pal.crAccentStrong : pal.crAccent) : pal.crBorder;
		crEdge = crFill;
		crText = bEnabled ? RGB(255, 255, 255) : pal.crFaint;
		break;
	case eLink:
		crFill = pal.crCard;
		crEdge = crFill;
		crText = bEnabled ? (bHover ? pal.crAccentStrong : pal.crAccent) : pal.crFaint;
		break;
	case eSub:
	default:
		crFill = pal.crCard;
		crEdge = bEnabled ? (bHover ? pal.crAccent : pal.crBorder) : pal.crBorder;
		crText = bEnabled ? (bHover ? pal.crAccent : pal.crText) : pal.crFaint;
		break;
	}
	CString strText;
	GetWindowText(strText);
	CPen pen(PS_SOLID, 1, crEdge);
	CBrush br(crFill);
	CPen* pPenOld = pDC->SelectObject(&pen);
	CBrush* pBrOld = pDC->SelectObject(&br);
	pDC->RoundRect(rc, CPoint(6, 6));
	pDC->SetBkMode(TRANSPARENT);
	pDC->SetTextColor(crText);
	pDC->DrawText(strText, rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
	pDC->SelectObject(pBrOld);
	pDC->SelectObject(pPenOld);
}

/////////////////////////////////////////////////////////////////////////////
// CTaskCombo —— P4·4.4(4)：材料下拉换肤（闭态显示区与弹出列表全走 DrawItem）

BOOL CTaskCombo::CreateCombo(const CRect& rc, CWnd* pParent, UINT nID)
{
	// 【P4·F 修复（用户反馈）】CBS_HASSTRINGS 必须有：无此样式的 owner-draw
	// 组合框 AddString 存入的是字符串指针（当 itemData），闭态 DrawItem 的
	// GetLBText 取回指针字节当文字——材料下拉显示"‖翱"状乱码即此（P4·A 换肤
	// 引入；枚举级断言只查了选中索引，未覆盖闭态文字）。
	return Create(WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED |
		CBS_HASSTRINGS | WS_VSCROLL, rc, pParent, nID);
}

void CTaskCombo::MeasureItem(LPMEASUREITEMSTRUCT pMIS)
{
	pMIS->itemHeight = 20;
}

void CTaskCombo::DrawItem(LPDRAWITEMSTRUCT pDIS)
{
	CDC* pDC = CDC::FromHandle(pDIS->hDC);
	CRect rc(pDIS->rcItem);
	const STaskPal& pal = TaskPal();
	const BOOL bEnabled = (pDIS->itemState & ODS_DISABLED) == 0;
	const BOOL bSelected = (pDIS->itemState & ODS_SELECTED) != 0;   // 弹出列表 hover/选中
	const BOOL bEdit = (pDIS->itemState & ODS_COMBOBOXEDIT) != 0;  // 闭态显示区
	CString str;
	if (pDIS->itemID != (UINT)-1)
		GetLBText((int)pDIS->itemID, str);
	pDC->FillSolidRect(rc, (bSelected && !bEdit && bEnabled) ? pal.crBorder : pal.crCard);
	CRect rcText(rc);
	rcText.left += 8;
	pDC->SetBkMode(TRANSPARENT);
	pDC->SetTextColor(bEnabled ? pal.crText : pal.crFaint);
	pDC->DrawText(str, rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

/////////////////////////////////////////////////////////////////////////////
// CTaskPane

CTaskPane::CTaskPane()
{
}

CTaskPane::~CTaskPane()
{
}

BEGIN_MESSAGE_MAP(CTaskPane, CDockablePane)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	ON_WM_DESTROY()
	ON_WM_ERASEBKGND()
	ON_WM_CTLCOLOR()
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_TASK_RATIO, &CTaskPane::OnSliderCustomDraw)
	ON_BN_CLICKED(IDC_TASK_APPLY, &CTaskPane::OnTaskApply)
	ON_BN_CLICKED(IDC_TASK_UNDO, &CTaskPane::OnTaskUndo)
	ON_BN_CLICKED(IDC_TASK_EXPORT_STEP, &CTaskPane::OnTaskExportStep)
	ON_BN_CLICKED(IDC_TASK_EXPORT_IGES, &CTaskPane::OnTaskExportIges)
	ON_BN_CLICKED(IDC_TASK_GOTO_SIMP, &CTaskPane::OnTaskGotoSimp)
	ON_BN_CLICKED(IDC_TASK_COLORBTN, &CTaskPane::OnTaskColor)
	ON_CBN_SELCHANGE(IDC_TASK_MATCBO, &CTaskPane::OnTaskMaterial)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTaskPane 消息处理程序

int CTaskPane::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect rectDummy;
	rectDummy.SetRectEmpty();

	// P3·3.1：分组框（BS_GROUPBOX）退役——卡片容器改 OnPaint 圆角绘制，标题改
	// 静态文本（同 ID 1030-1032、同文案 kGrp*；WM_GETTEXT 断言链不变）
	CreateLabel(m_stTitleInfo,   newui::task::kGrpInfo,   IDC_TASK_INFO_BOX);
	CreateLabel(m_stTitleSimp,   newui::task::kGrpSimp,   IDC_TASK_SIMP_BOX);
	CreateLabel(m_stTitleExport, newui::task::kGrpExport, IDC_TASK_EXPORT_BOX);

	DWORD dwStStyle = WS_CHILD | WS_VISIBLE | SS_LEFT;
	CreateLabel(m_stFilename,  newui::task::kLblFilename,  IDC_TASK_FILENAME);
	CreateLabel(m_stFilesize,  newui::task::kLblFilesize,  IDC_TASK_FILESIZE);
	CreateLabel(m_stSimpState, newui::task::kLblSimpState, IDC_TASK_SIMPSTATE);
	CreateLabel(m_stV, newui::task::kLblV, IDC_TASK_V);
	CreateLabel(m_stE, newui::task::kLblE, IDC_TASK_E);
	CreateLabel(m_stF, newui::task::kLblF, IDC_TASK_F);
	CreateLabel(m_stXMin, newui::task::kBBoxLabels[0], IDC_TASK_XMIN);
	CreateLabel(m_stXMax, newui::task::kBBoxLabels[1], IDC_TASK_XMAX);
	CreateLabel(m_stYMin, newui::task::kBBoxLabels[2], IDC_TASK_YMIN);
	CreateLabel(m_stYMax, newui::task::kBBoxLabels[3], IDC_TASK_YMAX);
	CreateLabel(m_stZMin, newui::task::kBBoxLabels[4], IDC_TASK_ZMIN);
	CreateLabel(m_stZMax, newui::task::kBBoxLabels[5], IDC_TASK_ZMAX);

	// 形态属性行（2.2 自属性窗格迁入）：颜色按钮 + 材料下拉。
	// 颜色“默认”= 纯黄（真实语义——取证基线默认色；旧属性网格的“默认”按钮
	// 是 3DFACE 外观色，偏差已预登记设计定稿 4.2.11）。CMFCColorButton 选中后经
	// WM_COMMAND(BN_CLICKED) 通知窗格（afxcolorbutton.cpp UpdateColor 源核实）。
	CreateLabel(m_stLblColor, newui::task::kLblColor, IDC_TASK_LBL_COLOR);
	if (!m_btnColor.Create(newui::task::kLblColor, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		rectDummy, this, IDC_TASK_COLORBTN))
	{
		TRACE0("未能创建任务窗格颜色按钮\n");
		return -1;
	}
	m_btnColor.EnableAutomaticButton(newui::task::kColorAuto, RGB(255, 255, 0), TRUE);
	m_btnColor.SetColor(RGB(255, 255, 0));
	m_btnColor.m_bTransparent = TRUE;   // P4：按钮面跟随父底色（深色卡面上不再白底）
	CreateLabel(m_stLblMaterial, newui::task::kLblMaterial, IDC_TASK_LBL_MAT);
	if (!m_cboMaterial.CreateCombo(CRect(0, 0, 0, 200), this, IDC_TASK_MATCBO))
	{
		TRACE0("未能创建任务窗格材料下拉\n");
		return -1;
	}
	for (int i = 0; i < 4; ++i)
		m_cboMaterial.AddString(newui::task::kMatOptions[i]);
	m_cboMaterial.SetCurSel(0);

	// 网格简化卡：滑块 + 比例值 + 预计面数 + 两按钮 + 说明行
	if (!m_stRatio.Create(newui::task::kLblRatio, dwStStyle, rectDummy, this, IDC_TASK_RATIOLBL) ||
		!m_stEstimate.Create(newui::task::kEstimateNoModel, dwStStyle, rectDummy, this, IDC_TASK_ESTIMATE))
	{
		TRACE0("未能创建任务窗格简化卡静态控件\n");
		return -1;
	}
	if (!m_slider.Create(WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS | TBS_BOTTOM,
		rectDummy, this, IDC_TASK_RATIO))
	{
		TRACE0("未能创建任务窗格简化比例滑块\n");
		return -1;
	}
	m_slider.SetRange(1, 100);
	m_slider.SetPos(60);
	m_slider.SetTicFreq(10);
	CreateLabel(m_stSimpHint, newui::task::kSimpHint, IDC_TASK_SIMPHINT);

	// P3·3.2：扁平按钮（primary=accent 底白字 / sub=卡片底描边 / link=文字链接）。
	// BS_OWNERDRAW 仍是真实 HWND——BM_CLICK/WM_GETTEXT/EnableWindow 与原 CButton
	// 完全一致（无头验收链路不变，风险 P-R3）。
	if (!m_btnApply.CreateFlat(newui::task::kBtnApply, rectDummy, this, IDC_TASK_APPLY, CFlatButton::ePrimary) ||
		!m_btnUndo.CreateFlat(newui::task::kBtnUndo, rectDummy, this, IDC_TASK_UNDO, CFlatButton::eSub))
	{
		TRACE0("未能创建任务窗格简化卡按钮\n");
		return -1;
	}

	// 导出卡：两按钮 + 大模型提示（默认隐藏）+ 前往简化
	if (!m_btnExportStep.CreateFlat(newui::task::kBtnStep, rectDummy, this, IDC_TASK_EXPORT_STEP, CFlatButton::ePrimary) ||
		!m_btnExportIges.CreateFlat(newui::task::kBtnIges, rectDummy, this, IDC_TASK_EXPORT_IGES, CFlatButton::eSub))
	{
		TRACE0("未能创建任务窗格导出卡按钮\n");
		return -1;
	}
	if (!m_stBigHint.Create(newui::task::kBigHintFmt, dwStStyle, rectDummy, this, IDC_TASK_BIGHINT) ||
		!m_btnGotoSimp.CreateFlat(newui::task::kGotoSimp, rectDummy, this, IDC_TASK_GOTO_SIMP, CFlatButton::eLink))
	{
		TRACE0("未能创建任务窗格导出卡提示\n");
		return -1;
	}
	m_stBigHint.ShowWindow(SW_HIDE);
	m_btnGotoSimp.ShowWindow(SW_HIDE);

	// P3·4.3 字体：卡片标题/预计面数 9pt 加粗（OnCtlColor 按 ID 着 textDim/accent），
	// V/E/F 大数字 13pt 加粗（文本串不变——保 WM_GETTEXT 断言链）
	{
		LOGFONT lf;
		::ZeroMemory(&lf, sizeof(lf));
		afxGlobalData.fontRegular.GetLogFont(&lf);
		lf.lfWeight = FW_BOLD;
		if (m_fontTitle.CreateFontIndirect(&lf))
		{
			m_stTitleInfo.SetFont(&m_fontTitle);
			m_stTitleSimp.SetFont(&m_fontTitle);
			m_stTitleExport.SetFont(&m_fontTitle);
			m_stEstimate.SetFont(&m_fontTitle);
		}
		HDC hdc = ::GetDC(NULL);
		lf.lfHeight = -::MulDiv(::GetDeviceCaps(hdc, LOGPIXELSY), 13, 72);
		::ReleaseDC(NULL, hdc);
		if (m_fontValue.CreateFontIndirect(&lf))
		{
			m_stV.SetFont(&m_fontValue);
			m_stE.SetFont(&m_fontValue);
			m_stF.SetFont(&m_fontValue);
		}
	}

	ClearInfo();      // 初始态：未载入模型（数值清零、按钮禁用、提示隐藏）
	AdjustLayout();
	return 0;
}

void CTaskPane::CreateLabel(CStatic& st, const wchar_t* lpszText, UINT nID)
{
	if (!st.Create(lpszText, WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(0, 0, 0, 0), this, nID))
		TRACE0("未能创建任务窗格静态控件\n");
}

void CTaskPane::OnDestroy()
{
	CDockablePane::OnDestroy();
}

// 空闲命令 UI 走查直通（不调基类的 UpdateDialogControls——见头文件注释：
// 按钮可用性由 RefreshFromDocument/ClearInfo 显式管理，避免无处理器误禁）
void CTaskPane::OnUpdateCmdUI(CFrameWnd* /*pTarget*/, int /*bDisableIfNoHndler*/)
{
}

void CTaskPane::OnSize(UINT nType, int cx, int cy)
{
	CDockablePane::OnSize(nType, cx, cy);
	AdjustLayout();
}

// 滑块滚动：刷新比例值与预计面数（round(比例×当前面数)，与 IDD 333 估计同一公式）
void CTaskPane::OnHScroll(UINT /*nSBCode*/, UINT /*nPos*/, CScrollBar* pScrollBar)
{
	if (pScrollBar == (CScrollBar*)&m_slider)
		UpdateEstimate();
}

void CTaskPane::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* /*pScrollBar*/)
{
	CRect rc;
	GetClientRect(rc);
	const int nPage = (rc.Height() > 16) ? rc.Height() - 16 : 16;
	int nNewPos = m_nScrollPos;
	switch (nSBCode)
	{
	case SB_LINEUP:   nNewPos -= 16; break;
	case SB_LINEDOWN: nNewPos += 16; break;
	case SB_PAGEUP:   nNewPos -= nPage; break;
	case SB_PAGEDOWN:  nNewPos += nPage; break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION: nNewPos = (int)nPos; break;
	default: return;
	}
	const int nMax = (m_nContentH > rc.Height()) ? (m_nContentH - rc.Height()) : 0;
	if (nNewPos < 0) nNewPos = 0;
	if (nNewPos > nMax) nNewPos = nMax;
	if (nNewPos == m_nScrollPos)
		return;
	m_nScrollPos = nNewPos;
	SetScrollPos(SB_VERT, m_nScrollPos);
	AdjustLayout();
}

// 布局（P3·4.3 重排）：三卡纵向排布，全部按"内容坐标系"（y 从 0 起）计算——
// 卡片矩形存 m_rcCard[3]/m_rcWarn，子件先按内容坐标定位，再统一减滚动偏移
// （每轮先回内容坐标再平移，幂等；滚动与卡片绘制不脱节）。
// 行高：信息行 16 / V·E·F 22（13pt）/ 按钮 28；内容超高启用纵向滚动。
void CTaskPane::AdjustLayout()
{
	if (GetSafeHwnd() == NULL)
		return;

	CRect rc;
	GetClientRect(rc);
	if (rc.IsRectEmpty())
		return;
	const int cxW = rc.Width();

	const BOOL bBigHint = (m_stBigHint.GetSafeHwnd() != NULL &&
		(m_stBigHint.GetStyle() & WS_VISIBLE));

	// 几何常量：卡边距 8 / 圆角 8 / 内边距 12 / 卡间距 10（设计定稿 4.3.2）
	const int M = 8, PAD = 12, GAP = 10;
	const int xL = M, xR = cxW - M;
	const int cL = xL + PAD, cR = xR - PAD;

	// ---- 卡 1：模型信息（标题 + 3 信息行 + V/E/F 大数字 + 包围盒 6 行 + 形态行）----
	int y = 0;
	m_stTitleInfo.MoveWindow(CRect(cL, y + 10, cL + 140, y + 28));
	int yr = y + 38;
	CRect rcRow(cL, yr, cR, yr + 16);
	m_stFilename.MoveWindow(rcRow);   rcRow.OffsetRect(0, 16);
	m_stFilesize.MoveWindow(rcRow);  rcRow.OffsetRect(0, 16);
	m_stSimpState.MoveWindow(rcRow); rcRow.OffsetRect(0, 16);
	// 【P4·F 修复（用户反馈）】V/E/F 行用 13pt 加粗大数字（P3·4.3），行高约
	// 22px——沿用 9pt 的 16px 窗口会把字形底部裁掉（顶点数/边数/面数三行
	// "排版混乱+数字被遮挡"），行窗口加高 24px、行距 22→26。
	rcRow.bottom = rcRow.top + 24;
	m_stV.MoveWindow(rcRow);         rcRow.OffsetRect(0, 26);
	m_stE.MoveWindow(rcRow);         rcRow.OffsetRect(0, 26);
	m_stF.MoveWindow(rcRow);
	yr += 3 * 16 + 2 * 26 + 24 + 6;
	rcRow.SetRect(cL, yr, cR, yr + 16);
	m_stXMin.MoveWindow(rcRow);     rcRow.OffsetRect(0, 16);
	m_stXMax.MoveWindow(rcRow);     rcRow.OffsetRect(0, 16);
	m_stYMin.MoveWindow(rcRow);     rcRow.OffsetRect(0, 16);
	m_stYMax.MoveWindow(rcRow);     rcRow.OffsetRect(0, 16);
	m_stZMin.MoveWindow(rcRow);     rcRow.OffsetRect(0, 16);
	m_stZMax.MoveWindow(rcRow);
	yr += 6 * 16 + 4;
	// 形态属性行（2.2 迁自属性窗格）——P3·3.3 拆两行（原型 formrow 结构），
	// 同时修复 3.1 重排把下拉挤到 38px 宽的问题（现为内容满宽 ~146px）
	m_stLblColor.MoveWindow(CRect(cL, yr + 5, cL + 30, yr + 21));
	m_btnColor.MoveWindow(CRect(cL + 34, yr + 1, cL + 102, yr + 25));
	const int ym = yr + 30;
	m_stLblMaterial.MoveWindow(CRect(cL, ym + 5, cL + 30, ym + 21));
	m_cboMaterial.MoveWindow(CRect(cL + 34, ym, cR, ym + 200));
	const int h1 = ym + 28 + PAD;
	m_rcCard[0].SetRect(xL, y, xR, h1);

	// ---- 卡 2：网格简化（标题 / 滑块+比例 / 预计 / 两按钮 / 说明）----
	int y2 = h1 + GAP;
	m_stTitleSimp.MoveWindow(CRect(cL, y2 + 10, cL + 140, y2 + 28));
	int ys = y2 + 38;
	m_slider.MoveWindow(CRect(cL, ys, cR - 72, ys + 28));
	m_stRatio.MoveWindow(CRect(cR - 68, ys + 6, cR, ys + 22));
	ys += 34;
	m_stEstimate.MoveWindow(CRect(cL, ys, cR, ys + 32));
	ys += 38;
	m_btnApply.MoveWindow(CRect(cL, ys, cL + 88, ys + 28));
	m_btnUndo.MoveWindow(CRect(cL + 96, ys, cL + 168, ys + 28));
	ys += 28 + 8;
	m_stSimpHint.MoveWindow(CRect(cL, ys, cR, ys + 30));
	const int h2 = ys + 30 + PAD - y2;
	m_rcCard[1].SetRect(xL, y2, xR, y2 + h2);

	// ---- 卡 3：导出（标题 / 两按钮 / 可选 warn 内衬盒 + 前往）----
	int y3 = y2 + h2 + GAP;
	m_stTitleExport.MoveWindow(CRect(cL, y3 + 10, cL + 140, y3 + 28));
	int ye = y3 + 38;
	const int wBtn = (cR - cL - 8) / 2;   // 导出两钮平分内容宽（窄窗格自适应）
	m_btnExportStep.MoveWindow(CRect(cL, ye, cL + wBtn, ye + 28));
	m_btnExportIges.MoveWindow(CRect(cL + wBtn + 8, ye, cR, ye + 28));
	ye += 28;
	if (bBigHint)
	{
		const int yb = ye + 10;
		m_stBigHint.MoveWindow(CRect(cL + 4, yb, cR - 4, yb + 48));
		m_btnGotoSimp.MoveWindow(CRect(cL + 8, yb + 52, cL + 80, yb + 76));
		m_rcWarn.SetRect(xL + 4, yb - 6, xR - 4, yb + 76 + 6);
	}
	else
	{
		m_rcWarn.SetRectEmpty();
	}
	const int h3 = (bBigHint ? ye + 10 + 76 + PAD : ye + PAD) - y3;
	m_rcCard[2].SetRect(xL, y3, xR, y3 + h3);

	m_nContentH = y3 + h3 + M;

	// 滚动偏移：子件自内容坐标整体下移 dy（内容坐标→窗口坐标）
	if (m_nScrollPos != 0)
	{
		const int dy = -m_nScrollPos;
		for (CWnd* pChild = GetWindow(GW_CHILD); pChild != NULL;
			pChild = pChild->GetWindow(GW_HWNDNEXT))
		{
			CRect rcw;
			pChild->GetWindowRect(rcw);
			ScreenToClient(rcw);
			rcw.OffsetRect(0, dy);
			pChild->MoveWindow(rcw);
		}
	}

	UpdateScrollRange();
	Invalidate();   // 卡片容器随布局重画（绘制挂 OnEraseBkgnd——须带擦除重发）
}

void CTaskPane::UpdateScrollRange()
{
	CRect rc;
	GetClientRect(rc);
	const int nMax = (m_nContentH > rc.Height()) ? (m_nContentH - rc.Height()) : 0;
	if (m_nScrollPos > nMax)
	{
		m_nScrollPos = nMax;
		AdjustLayout();
		return;
	}
	SCROLLINFO si;
	::ZeroMemory(&si, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
	si.nMin = 0;
	si.nMax = m_nContentH > 0 ? m_nContentH - 1 : 0;
	si.nPage = rc.Height();
	si.nPos = m_nScrollPos;
	SetScrollInfo(SB_VERT, &si, TRUE);
	ShowScrollBar(SB_VERT, m_nContentH > rc.Height());
}

// ==== P3·4.3 视觉：卡片容器绘制 + 静态文本着色 ====

// 三卡容器：圆角矩形（cardBg 填充 + cardBorder 描边，r=8）+ >10 万面 warn 内衬盒。
// 挂 OnEraseBkgnd（基类先填窗格底色再叠画卡片）：防闪烁常规位，屏幕与取证两条
// 路径等价（runA/B/C 实测：PrintWindow(PW_RENDERFULLCONTENT) 与屏幕 BitBlt 对本
// 窗格逐位一致；早前"OnPaint 不进位图"推断系误诊——真实原因是验证环境处于深色
// 主题（Workspace\DarkTheme=1，ObsidianBlack 窗格内底为白、停靠标题条为灰），
// 浅色断言跑在深色环境上。取证前先 reg query 主题状态，勿凭像素反推主题）。
BOOL CTaskPane::OnEraseBkgnd(CDC* pDC)
{
	// P4·4.4(1)：深色主题自填窗格底（原型 #11151D 一系）——基类 ObsidianBlack 下
	// 的窗格内底是纯白，深色卡片会晾成"白纸贴深卡"+底部白洞（图片分析员实测：
	// 卡下 210px 白）；浅色主题维持基类默认（与文件/输出窗格一致）。
	if (theApp.GetInt(_T("DarkTheme"), 0) != 0)
	{
		CRect rc;
		GetClientRect(rc);
		pDC->FillSolidRect(rc, TaskPal().crPaneBg);
	}
	else
	{
		CDockablePane::OnEraseBkgnd(pDC);   // 浅色：CMFCVisualManager 默认窗格底
	}
	DrawCards(pDC);
	return TRUE;
}

void CTaskPane::DrawCards(CDC* pDC)
{
	const STaskPal& pal = TaskPal();

	CRect rcClient;
	GetClientRect(rcClient);
	const int dy = -m_nScrollPos;

	CPen penBorder(PS_SOLID, 1, pal.crBorder);
	CBrush brCard(pal.crCard);
	CBrush brWarn(pal.crWarnSoft);
	CPen* pPenOld = pDC->SelectObject(&penBorder);
	CBrush* pBrOld = pDC->SelectObject(&brCard);
	for (int i = 0; i < 3; ++i)
	{
		CRect rc = m_rcCard[i];
		rc.OffsetRect(0, dy);
		if (rc.top >= rcClient.bottom || rc.bottom <= rcClient.top)
			continue;
		pDC->RoundRect(rc, CPoint(8, 8));
	}
	if (!m_rcWarn.IsRectEmpty())
	{
		CRect rc = m_rcWarn;
		rc.OffsetRect(0, dy);
		if (rc.top < rcClient.bottom && rc.bottom > rcClient.top)
		{
			pDC->SelectObject(&brWarn);
			pDC->RoundRect(rc, CPoint(6, 6));
		}
	}
	pDC->SelectObject(pBrOld);
	pDC->SelectObject(pPenOld);
}

// 静态文本：按 ID 令牌着色（标题 textDim / 正文 textMain / 预计面数 accent /
// 说明行 textFaint / 大模型提示 warn），背景=卡片表面同色实底刷（每笔现读主题）。
HBRUSH CTaskPane::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	if (nCtlColor == CTLCOLOR_LISTBOX)
	{
		// P3·3.3：材料下拉（CBS_DROPDOWNLIST 闭态与弹出列表同走 CTLCOLORLISTBOX）
		// ——深色下闭态/列表深色；浅色下白底深字与原生默认等价
		const STaskPal& pal = TaskPal();
		pDC->SetTextColor(pal.crText);
		pDC->SetBkColor(pal.crCard);
		return TaskCardBrush();
	}
	if (nCtlColor == CTLCOLOR_STATIC)
	{
		const STaskPal& pal = TaskPal();
		COLORREF crText = pal.crText;
		switch (pWnd->GetDlgCtrlID())
		{
		case IDC_TASK_INFO_BOX:
		case IDC_TASK_SIMP_BOX:
		case IDC_TASK_EXPORT_BOX: crText = pal.crDim;    break;
		case IDC_TASK_ESTIMATE:    crText = pal.crAccent; break;
		case IDC_TASK_SIMPHINT:    crText = pal.crFaint;  break;
		case IDC_TASK_BIGHINT:     crText = pal.crWarn;   break;
		}
		pDC->SetTextColor(crText);
		pDC->SetBkColor(pal.crCard);
		return TaskCardBrush();
	}
	return CDockablePane::OnCtlColor(pDC, pWnd, nCtlColor);
}

// P3·3.2 滑块自绘（设计定稿 4.3.2）：轨道圆角 4px——thumb 左侧 accent（已保留
// 比例）、右侧 border-soft；thumb 圆角 accent。刻度（TBCD_TICS）走系统默认。
// 常量已按 SDK commctrl.h 核实（P-R5，D:\Windows Kits\10.0.18362.0 头 2687/2719-2721）：
// TBCD_TICS=1 / TBCD_THUMB=2 / TBCD_CHANNEL=3，TBM_GETTHUMBRECT=WM_USER+25。
void CTaskPane::OnSliderCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMCUSTOMDRAW pCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
	*pResult = CDRF_DODEFAULT;
	if (pCD->dwDrawStage == CDDS_PREPAINT)
	{
		*pResult = CDRF_NOTIFYITEMDRAW;
		return;
	}
	if (pCD->dwDrawStage != CDDS_ITEMPREPAINT)
		return;

	CDC* pDC = CDC::FromHandle(pCD->hdc);
	CRect rc(pCD->rc);
	const STaskPal& pal = TaskPal();
	const BOOL bEnabled = m_slider.IsWindowEnabled();
	CPen penNull(PS_NULL, 1, RGB(0, 0, 0));
	CPen* pPenOld = pDC->SelectObject(&penNull);

	if (pCD->dwItemSpec == TBCD_CHANNEL)
	{
		CRect rcThumb;
		::ZeroMemory(&rcThumb, sizeof(rcThumb));
		m_slider.SendMessage(TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>((LPRECT)&rcThumb));
		CBrush brBase(pal.crBorder);
		CBrush* pBrOld = pDC->SelectObject(&brBase);
		pDC->RoundRect(rc, CPoint(4, 4));
		if (bEnabled && !rcThumb.IsRectEmpty())
		{
			// 已保留部分：thumb 中心左侧填 accent
			CRect rcLeft(rc);
			rcLeft.right = rcThumb.CenterPoint().x;
			if (rcLeft.right > rc.right)
				rcLeft.right = rc.right;
			if (rcLeft.right >= rc.left)
			{
				CBrush brFill(pal.crAccent);
				pDC->SelectObject(&brFill);
				pDC->RoundRect(rcLeft, CPoint(4, 4));
			}
		}
		pDC->SelectObject(pBrOld);
		*pResult = CDRF_SKIPDEFAULT;
	}
	else if (pCD->dwItemSpec == TBCD_THUMB)
	{
		CBrush brThumb(bEnabled ? pal.crAccent : pal.crBorder);
		CBrush* pBrOld = pDC->SelectObject(&brThumb);
		pDC->RoundRect(rc, CPoint(4, 4));
		pDC->SelectObject(pBrOld);
		*pResult = CDRF_SKIPDEFAULT;
	}
	pDC->SelectObject(pPenOld);
}

// ==== 数据入口 ====

// 9 值与属性窗格同源（Doc::UpdateModelStatsFromMesh → CMainFrame 双写转发）。
// 边数保持 V+F-2 公式输出（取证基线）；包围盒 6 位小数（与原版显示口径一致）。
void CTaskPane::UpdateModelStats(long nVertices, long nEdges, long nFaces,
	double dXMin, double dXMax, double dYMin, double dYMax,
	double dZMin, double dZMax)
{
	if (GetSafeHwnd() == NULL)
		return;
	m_nCurrentFaces = (int)nFaces;   // 预计面数联动基数（与统计同源）
	CString str;
	str.Format(_T("%s%ld"), newui::task::kLblV, nVertices);
	m_stV.SetWindowText(str);
	str.Format(_T("%s%ld"), newui::task::kLblE, nEdges);
	m_stE.SetWindowText(str);
	str.Format(_T("%s%ld"), newui::task::kLblF, nFaces);
	m_stF.SetWindowText(str);
	str.Format(_T("%s%.6f"), newui::task::kBBoxLabels[0], dXMin);
	m_stXMin.SetWindowText(str);
	str.Format(_T("%s%.6f"), newui::task::kBBoxLabels[1], dXMax);
	m_stXMax.SetWindowText(str);
	str.Format(_T("%s%.6f"), newui::task::kBBoxLabels[2], dYMin);
	m_stYMin.SetWindowText(str);
	str.Format(_T("%s%.6f"), newui::task::kBBoxLabels[3], dYMax);
	m_stYMax.SetWindowText(str);
	str.Format(_T("%s%.6f"), newui::task::kBBoxLabels[4], dZMin);
	m_stZMin.SetWindowText(str);
	str.Format(_T("%s%.6f"), newui::task::kBBoxLabels[5], dZMax);
	m_stZMax.SetWindowText(str);
	UpdateEstimate();
}

void CTaskPane::RefreshFromDocument(Cstl2stpcodeDoc* pDoc)
{
	if (GetSafeHwnd() == NULL)
		return;
	if (pDoc == NULL || !pDoc->IsModelLoaded())
	{
		ClearInfo();
		return;
	}

	m_bHasModel = TRUE;
	// 【P4·V】STEP 预览态：仅显示类输入件可用（颜色/材料作用于当前 AIS 显示）；
	// 简化/导出按网格语义禁用（文档命令处理器另有守卫，双保险）
	const BOOL bStep = pDoc->IsStepPreview();
	m_bStepPreview = bStep;
	m_nCurrentFaces = pDoc->GetTriangleCount();

	CString strName = pDoc->GetPathName();
	if (!strName.IsEmpty())
		strName = strName.Mid(strName.ReverseFind(_T('\\')) + 1);
	CString strText;
	strText.Format(_T("%s%s"), newui::task::kLblFilename,
		strName.IsEmpty() ? newui::task::kNoFile : (LPCTSTR)strName);
	m_stFilename.SetWindowText(strText);

	// 文件大小：B/KB/MB 自适应
	CString strSize;
	const ULONGLONG ull = pDoc->m_ullFileSize;
	if (ull >= 1024ull * 1024ull)
		strSize.Format(_T("%.2f MB"), ull / (1024.0 * 1024.0));
	else if (ull >= 1024ull)
		strSize.Format(_T("%.1f KB"), ull / 1024.0);
	else
		strSize.Format(_T("%I64u B"), ull);
	strText.Format(_T("%s%s"), newui::task::kLblFilesize, (LPCTSTR)strSize);
	m_stFilesize.SetWindowText(strText);

	// 简化状态（m_hTriangOrig 非空 = 存在撤销快照）；STEP 预览无网格语义，显示预览态
	const BOOL bSimplified = !pDoc->m_hTriangOrig.IsNull();
	if (bStep)
		strText.Format(_T("%s%s"), newui::task::kLblSimpState, newui::stepprev::kSimpState);
	else
		strText.Format(_T("%s%s"), newui::task::kLblSimpState,
			bSimplified ? newui::task::kSimpYes : newui::task::kSimpNo);
	m_stSimpState.SetWindowText(strText);

	// 形态属性（2.2 迁自属性窗格）：显示文档当前颜色/材料
	//（Quantity_Color 分量 0..1 → 8bit；SetColor 不发通知，仅显示态）
	m_btnColor.SetColor(RGB(
		(BYTE)(pDoc->m_clrModel.Red()   * 255.0 + 0.5),
		(BYTE)(pDoc->m_clrModel.Green() * 255.0 + 0.5),
		(BYTE)(pDoc->m_clrModel.Blue()  * 255.0 + 0.5)));
	m_cboMaterial.SetCurSel(pDoc->m_nMaterialIndex);

	// 【P4·V】STEP 预览态：简化/导出输入件禁用（纯预览语义）；颜色/材料保留
	//（作用于当前 AIS 显示，预览外观可调）
	m_btnApply.EnableWindow(!bStep);
	m_btnUndo.EnableWindow(bSimplified && !bStep);
	m_btnExportStep.EnableWindow(!bStep);
	m_btnExportIges.EnableWindow(!bStep);
	m_btnColor.EnableWindow(TRUE);
	m_cboMaterial.EnableWindow(TRUE);
	m_slider.EnableWindow(!bStep);     // 3.2：与按钮同批管理（空文档统一禁用）

	// 大模型内联提示（advisory；阈值与导出确认框一致：100000 = 0x186A0，
	// 原版取证确认框行为不变，此提示仅引导简化，不替代任何确认）
	const BOOL bBig = (m_nCurrentFaces > 100000);
	if (bBig)
	{
		strText.Format(newui::task::kBigHintFmt, m_nCurrentFaces);
		m_stBigHint.SetWindowText(strText);
	}
	m_stBigHint.ShowWindow(bBig ? SW_SHOW : SW_HIDE);
	m_btnGotoSimp.ShowWindow(bBig ? SW_SHOW : SW_HIDE);

	UpdateEstimate();
	AdjustLayout();
}

void CTaskPane::ClearInfo()
{
	if (GetSafeHwnd() == NULL)
		return;
	m_bHasModel = FALSE;
	m_nCurrentFaces = 0;
	m_bStepPreview = FALSE;   // 【P4·V】预览态复位
	CString strText;
	m_stFilename.SetWindowText(CString(newui::task::kLblFilename) + newui::task::kNoFile);
	m_stFilesize.SetWindowText(CString(newui::task::kLblFilesize) + newui::task::kNoFile);
	m_stSimpState.SetWindowText(CString(newui::task::kLblSimpState) + newui::task::kNoFile);
	UpdateModelStats(0, 0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	m_btnApply.EnableWindow(FALSE);
	m_btnUndo.EnableWindow(FALSE);
	m_btnExportStep.EnableWindow(FALSE);
	m_btnExportIges.EnableWindow(FALSE);
	m_slider.EnableWindow(FALSE);    // 3.2：P2 起漏项——空文档输入件与按钮统一禁用
	// 形态属性复位（对齐 DeleteContents 的文档复位：纯黄 + DEFAULT）
	m_btnColor.SetColor(RGB(255, 255, 0));
	m_cboMaterial.SetCurSel(0);
	m_btnColor.EnableWindow(FALSE);
	m_cboMaterial.EnableWindow(FALSE);
	m_stBigHint.ShowWindow(SW_HIDE);
	m_btnGotoSimp.ShowWindow(SW_HIDE);
	AdjustLayout();
}

// 预计面数 = round(保留比例 × 当前面数)——与 IDD 333 对话框估计（补充八）同一公式
void CTaskPane::UpdateEstimate()
{
	if (GetSafeHwnd() == NULL)
		return;
	if (!m_bHasModel || m_nCurrentFaces <= 0)
	{
		m_stRatio.SetWindowText(CString(newui::task::kLblRatio) + _T(" —"));
		// 【P4·V】STEP 预览无网格可简化（m_nCurrentFaces=0 命中本分支）
		m_stEstimate.SetWindowText(m_bStepPreview
			? newui::stepprev::kEstimate : newui::task::kEstimateNoModel);
		return;
	}
	const int nPct = m_slider.GetPos();
	CString strRatio;
	strRatio.Format(_T("%s%d"), newui::task::kLblRatio, nPct);
	m_stRatio.SetWindowText(strRatio);
	const int nEstimate = static_cast<int>(m_nCurrentFaces * (nPct / 100.0) + 0.5);
	CString strEst;
	strEst.Format(newui::task::kEstimateFmt, nEstimate, m_nCurrentFaces);
	m_stEstimate.SetWindowText(strEst);
}

// ==== 按钮动作 ====

Cstl2stpcodeDoc* CTaskPane::GetActiveDoc() const
{
	// SDI：主框架 GetActiveDocument 直接可用（补充十四；MDI 陷阱已消失）
	CFrameWnd* pMain = (CFrameWnd*)AfxGetMainWnd();
	if (pMain == NULL)
		return NULL;
	return DYNAMIC_DOWNCAST(Cstl2stpcodeDoc, pMain->GetActiveDocument());
}

// 应用简化：直接调文档共享实现（无 IDD 333 对话框——滑块+预计面数+撤销已覆盖
// 其信息职责；对话框入口 32837 原样保留，两入口并存，设计定稿 4.2.3(c)）
void CTaskPane::OnTaskApply()
{
	Cstl2stpcodeDoc* pDoc = GetActiveDoc();
	if (pDoc == NULL)
		return;
	const int nPct = m_slider.GetPos();
	pDoc->SimplifyByRatio(nPct / 100.0);
}

// 撤销上次简化：菜单 32838 与本按钮共用文档共享实现
void CTaskPane::OnTaskUndo()
{
	Cstl2stpcodeDoc* pDoc = GetActiveDoc();
	if (pDoc == NULL)
		return;
	pDoc->UndoLastSimplify();
}

// 导出按钮：与工具栏/菜单/无头验收脚本同一 WM_COMMAND 链路（主框架路由到文档
// 处理器——kNoModel 守卫、>10 万面确认框、进度对话框、写盘全部原样保留）
void CTaskPane::OnTaskExportStep()
{
	CWnd* pMain = AfxGetMainWnd();
	if (pMain != NULL)
		pMain->PostMessage(WM_COMMAND, MAKEWPARAM(ID_EXPORT_STEP, 0), 0);
}

void CTaskPane::OnTaskExportIges()
{
	CWnd* pMain = AfxGetMainWnd();
	if (pMain != NULL)
		pMain->PostMessage(WM_COMMAND, MAKEWPARAM(ID_EXPORT_IGES, 0), 0);
}

// 前往简化：提示条为 advisory——把输入焦点交给简化卡滑块
void CTaskPane::OnTaskGotoSimp()
{
	if (m_slider.GetSafeHwnd() != NULL)
		m_slider.SetFocus();
}

// ==== 形态属性（2.2 迁自属性窗格；Doc API 与 P1 用户实测链路零改动）====

// 颜色按钮选中：CMFCColorButton 经 WM_COMMAND(BN_CLICKED) 通知（选中具体色与
// “默认”项同路径——afxcolorbutton.cpp UpdateColor 源核实）。“默认”= 纯黄。
void CTaskPane::OnTaskColor()
{
	Cstl2stpcodeDoc* pDoc = GetActiveDoc();
	if (pDoc == NULL || !pDoc->IsModelLoaded())
		return;
	pDoc->SetModelDisplayColor(m_btnColor.GetColor());
}

// 材料下拉：无/材料1/材料2/材料3 → DEFAULT/BRASS/STEEL/SILVER
//（与原属性网格 AddOption 映射一致，见 Doc.cpp s_aMaterialEnums）
void CTaskPane::OnTaskMaterial()
{
	Cstl2stpcodeDoc* pDoc = GetActiveDoc();
	if (pDoc == NULL || !pDoc->IsModelLoaded())
		return;
	pDoc->SetModelDisplayMaterial(m_cboMaterial.GetCurSel());
}
