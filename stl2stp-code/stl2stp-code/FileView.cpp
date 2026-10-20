
#include "stdafx.h"
#include "mainfrm.h"
#include "FileView.h"
#include "Resource.h"
#include "stl2stp-code.h"
#include "stl2stp-codeDoc.h"   // 新 UI：文件列表显示面数/激活跳转
#include "ThemeTokens.h"       // 新 UI P4·D：窗格/树换肤令牌（与任务窗格同源）

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CFileView

CFileView::CFileView()
{
}

CFileView::~CFileView()
{
}

BEGIN_MESSAGE_MAP(CFileView, CDockablePane)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_PROPERTIES, OnProperties)
	ON_COMMAND(ID_OPEN, OnFileOpen)
	ON_COMMAND(ID_OPEN_WITH, OnFileOpenWith)
	ON_COMMAND(ID_DUMMY_COMPILE, OnDummyCompile)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_COMMAND(ID_EDIT_CLEAR, OnEditClear)
	ON_NOTIFY_REFLECT(TVN_SELCHANGED, &CFileView::OnTreeSelChanged)
	ON_WM_PAINT()
	ON_WM_SETFOCUS()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWorkspaceBar 消息处理程序

int CFileView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect rectDummy;
	rectDummy.SetRectEmpty();

	// 创建视图:
	const DWORD dwViewStyle = WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS;

	if (!m_wndFileView.Create(dwViewStyle, rectDummy, this, 4))
	{
		TRACE0("未能创建文件视图\n");
		return -1;      // 未能创建
	}

	// 加载视图图像:
	m_FileViewImages.Create(IDB_FILE_VIEW, 16, 0, RGB(255, 0, 255));
	m_wndFileView.SetImageList(&m_FileViewImages, TVSIL_NORMAL);

	OnChangeVisualStyle();

	// 新 UI：移除模板残留的单按钮窗格工具栏（"属性"按钮只弹占位消息框）。
	// 原版"模型"静态节点改为动态打开文件列表，文档载入/新建/关闭时刷新
	RefreshFileList();
	AdjustLayout();
	ApplyTheme(theApp.GetInt(_T("DarkTheme"), 0) != 0);   // 新 UI P4·D：初始主题（创建时注册表已定，含迁移保留的偏好）

	return 0;
}

void CFileView::OnSize(UINT nType, int cx, int cy)
{
	CDockablePane::OnSize(nType, cx, cy);
	AdjustLayout();
}

// 新 UI P4·D：树控件色随主题。树色经 TVM 消息设置（不走 CTLCOLOR），是状态
// 而非每笔现读——主题切换必须由主框架 ApplyVisualTheme 显式重设（全局
// RedrawWindow 不会让树重查状态色）。深色=crPaneBg 底+crText 字；
// 浅色还原 (COLORREF)-1=系统默认（取证基线外观不变）。注：TVM_SETBKCOLOR/
// SETTEXTCOLOR 的还原哨兵文档明确是 -1（CLR_DEFAULT=0xFF000000 的低 24 位
// 是纯黑，首测实测把树底设成黑色）。
void CFileView::ApplyTheme(BOOL bDark)
{
	if (m_wndFileView.GetSafeHwnd() == NULL)
		return;
	if (bDark)
	{
		const STaskPal& pal = TaskPal();
		m_wndFileView.SetBkColor(pal.crPaneBg);
		m_wndFileView.SetTextColor(pal.crText);
	}
	else
	{
		m_wndFileView.SetBkColor((COLORREF)-1);
		m_wndFileView.SetTextColor((COLORREF)-1);
	}
}

// 新 UI P4·D：窗格底随主题令牌（深 #111522 / 浅 #EEF1F6，与任务窗格同源）；
// 每笔现读，主题切换的全局 RedrawWindow 即生效
BOOL CFileView::OnEraseBkgnd(CDC* pDC)
{
	CRect rc;
	GetClientRect(rc);
	pDC->FillSolidRect(rc, TaskPal().crPaneBg);
	return TRUE;
}

// 新 UI：列出全部打开文档（替代原版静态"模型"树节点）。
// 项文本 = 文件名（空文档用标题）+ 面数副标；活动文档加 [当前] 前缀；
// ItemData 保存 CDocument*（点击激活跳转用）。pExclude 用于文档销毁前刷新。
void CFileView::RefreshFileList(CDocument* pExclude)
{
	if (m_wndFileView.GetSafeHwnd() == NULL)
		return;
	m_wndFileView.DeleteAllItems();

	CWinApp* pApp = AfxGetApp();
	// 【新 UI P2·SDI 化阶段 1.2】单文档：活动文档经主框架 GetActiveDocument 直接
	// 获取（SDI 时 OnActivateView 把视图设进主框架的 m_pViewActive）；MDI 时代的
	// MDIGetActive 链路随多文档架构取消（设计定稿 4.2.2(c)）
	CFrameWnd* pMain = DYNAMIC_DOWNCAST(CFrameWnd, AfxGetMainWnd());
	CDocument* pActiveDoc = NULL;
	if (pMain != NULL)
		pActiveDoc = pMain->GetActiveDocument();

	POSITION posT = pApp->GetFirstDocTemplatePosition();
	while (posT != NULL)
	{
		CDocTemplate* pTpl = pApp->GetNextDocTemplate(posT);
		POSITION posD = pTpl->GetFirstDocPosition();
		while (posD != NULL)
		{
			CDocument* pDoc = pTpl->GetNextDoc(posD);
			if (pDoc == NULL || pDoc == pExclude)
				continue;

			CString strName = pDoc->GetPathName();
			if (!strName.IsEmpty())
				strName = strName.Mid(strName.ReverseFind(_T('\\')) + 1);
			if (strName.IsEmpty())
				strName = pDoc->GetTitle();   // 空文档显示默认标题（My1 等，原版 docName）

			CString strSub;
			Cstl2stpcodeDoc* pOur = DYNAMIC_DOWNCAST(Cstl2stpcodeDoc, pDoc);
			if (pOur != NULL && pOur->IsModelLoaded())
			{
				// 【P4·V】STEP 预览无三角网格（0 面有误导），显示预览态
				if (pOur->IsStepPreview())
					strSub = _T("（STEP 预览）");
				else
				{
					CString strFaces;
					strFaces.Format(_T("%d 面"), pOur->GetTriangleCount());
					strSub = _T("（") + strFaces + _T("）");
				}
			}
			else
				strSub = _T("（空文档）");

			const BOOL bActive = (pDoc == pActiveDoc);
			CString strItem = (bActive ? _T("[当前] ") : _T("")) + strName + _T("  ") + strSub;
			HTREEITEM hItem = m_wndFileView.InsertItem(strItem, 0, 0);
			m_wndFileView.SetItemData(hItem, (DWORD_PTR)pDoc);
		}
	}
}

// 新 UI：点击列表项——MDI 时代经 MDIActivate 激活对应文档的子框架。
// 【新 UI P2·SDI 化阶段 1.2】单文档：列表项即当前文档，无需激活跳转
//（MDIActivate 路径随多文档架构取消，设计定稿 4.2.2(c)）
void CFileView::OnTreeSelChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMTREEVIEW pNM = (LPNMTREEVIEW)pNMHDR;
	HTREEITEM hItem = pNM->itemNew.hItem;
	if (hItem != NULL)
	{
		// 单文档：仅保留选中态（ItemData 仍保存文档指针，"属性"右键查询用）
	}
	*pResult = 0;
}

void CFileView::OnContextMenu(CWnd* pWnd, CPoint point)
{
	CTreeCtrl* pWndTree = (CTreeCtrl*) &m_wndFileView;
	ASSERT_VALID(pWndTree);

	if (pWnd != pWndTree)
	{
		CDockablePane::OnContextMenu(pWnd, point);
		return;
	}

	if (point != CPoint(-1, -1))
	{
		// 选择已单击的项: 
		CPoint ptTree = point;
		pWndTree->ScreenToClient(&ptTree);

		UINT flags = 0;
		HTREEITEM hTreeItem = pWndTree->HitTest(ptTree, &flags);
		if (hTreeItem != NULL)
		{
			pWndTree->SelectItem(hTreeItem);
		}
	}

	pWndTree->SetFocus();
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EXPLORER, point.x, point.y, this, TRUE);
}

void CFileView::AdjustLayout()
{
	if (GetSafeHwnd() == NULL)
	{
		return;
	}

	CRect rectClient;
	GetClientRect(rectClient);

	// 新 UI：窗格工具栏已移除，树视图占满整个客户区
	m_wndFileView.SetWindowPos(NULL, rectClient.left + 1, rectClient.top + 1, rectClient.Width() - 2, rectClient.Height() - 2, SWP_NOACTIVATE | SWP_NOZORDER);
}

void CFileView::OnProperties()
{
	// 新 UI：原版此处为占位消息框"属性...."；改为显示选中文档的简要信息
	HTREEITEM hItem = m_wndFileView.GetSelectedItem();
	if (hItem == NULL)
		return;
	CDocument* pDoc = (CDocument*)m_wndFileView.GetItemData(hItem);
	if (pDoc == NULL)
		return;

	CString strInfo;
	Cstl2stpcodeDoc* pOur = DYNAMIC_DOWNCAST(Cstl2stpcodeDoc, pDoc);
	if (pOur != NULL && pOur->IsModelLoaded())
		strInfo.Format(_T("文件：%s\n三角面数：%d"),
			(LPCTSTR)pDoc->GetPathName(), pOur->GetTriangleCount());
	else
		strInfo.Format(_T("文档：%s（未载入模型）"), (LPCTSTR)pDoc->GetTitle());
	AfxMessageBox(strInfo);
}

void CFileView::OnFileOpen()
{
	// TODO: 在此处添加命令处理程序代码
}

void CFileView::OnFileOpenWith()
{
	// TODO: 在此处添加命令处理程序代码
}

void CFileView::OnDummyCompile()
{
	// TODO: 在此处添加命令处理程序代码
}

void CFileView::OnEditCut()
{
	// TODO: 在此处添加命令处理程序代码
}

void CFileView::OnEditCopy()
{
	// TODO: 在此处添加命令处理程序代码
}

void CFileView::OnEditClear()
{
	// TODO: 在此处添加命令处理程序代码
}

void CFileView::OnPaint()
{
	CPaintDC dc(this); // 用于绘制的设备上下文

	CRect rectTree;
	m_wndFileView.GetWindowRect(rectTree);
	ScreenToClient(rectTree);

	rectTree.InflateRect(1, 1);
	// 新 UI P4·D：深色下树外框换发丝线色；浅色保持系统阴影色（取证基线）
	const COLORREF crBorder = (theApp.GetInt(_T("DarkTheme"), 0) != 0)
		? TaskPal().crHeaderLine : ::GetSysColor(COLOR_3DSHADOW);
	dc.Draw3dRect(rectTree, crBorder, crBorder);
}

void CFileView::OnSetFocus(CWnd* pOldWnd)
{
	CDockablePane::OnSetFocus(pOldWnd);

	m_wndFileView.SetFocus();
}

void CFileView::OnChangeVisualStyle()
{
	// 新 UI：窗格工具栏已移除，仅维护树视图图像列表
	m_FileViewImages.DeleteImageList();

	UINT uiBmpId = theApp.m_bHiColorIcons ? IDB_FILE_VIEW_24 : IDB_FILE_VIEW;

	CBitmap bmp;
	if (!bmp.LoadBitmap(uiBmpId))
	{
		TRACE(_T("无法加载位图: %x\n"), uiBmpId);
		ASSERT(FALSE);
		return;
	}

	BITMAP bmpObj;
	bmp.GetBitmap(&bmpObj);

	UINT nFlags = ILC_MASK;

	nFlags |= (theApp.m_bHiColorIcons) ? ILC_COLOR24 : ILC_COLOR4;

	m_FileViewImages.Create(16, bmpObj.bmHeight, nFlags, 0, 0);
	m_FileViewImages.Add(&bmp, RGB(255, 0, 255));

	m_wndFileView.SetImageList(&m_FileViewImages, TVSIL_NORMAL);
}


