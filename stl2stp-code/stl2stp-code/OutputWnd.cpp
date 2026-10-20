
#include "stdafx.h"

#include "OutputWnd.h"
#include "Resource.h"
#include "MainFrm.h"
#include "stl2stp-code.h"     // 新 UI P4·D：theApp（主题探测）
#include "ThemeTokens.h"      // 新 UI P4·D：窗格/列表换肤令牌（与任务窗格同源）

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COutputBar

COutputWnd::COutputWnd()
{
}

COutputWnd::~COutputWnd()
{
}

BEGIN_MESSAGE_MAP(COutputWnd, CDockablePane)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

int COutputWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect rectDummy;
	rectDummy.SetRectEmpty();

	// ����ѡ�����: 
	if (!m_wndTabs.Create(CMFCTabCtrl::STYLE_FLAT, rectDummy, this, 1))
	{
		TRACE0("δ�ܴ������ѡ�����\n");
		return -1;      // δ�ܴ���
	}

	// ����ԭ�棺�������ֻ��һ�������ɡ��б���ѡ���ԭ������ʱö�ٽ���
	// һ�� ListBox �Ӵ��ڣ�����Ҳֻ��ʾһ��͹��ѡ�������ʼΪ�ա�
	const DWORD dwStyle = LBS_NOINTEGRALHEIGHT | WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL;

	if (!m_wndOutputBuild.Create(dwStyle, rectDummy, &m_wndTabs, 2))
	{
		TRACE0("δ�ܴ����������\n");
		return -1;      // δ�ܴ���
	}

	UpdateFonts();

	CString strTabName;
	BOOL bNameValid;

	// ���б���ڸ��ӵ�ѡ�:
	bNameValid = strTabName.LoadString(IDS_BUILD_TAB);
	ASSERT(bNameValid);
	m_wndTabs.AddTab(&m_wndOutputBuild, strTabName, (UINT)0);

	FillBuildWindow();

	return 0;
}

void COutputWnd::OnSize(UINT nType, int cx, int cy)
{
	CDockablePane::OnSize(nType, cx, cy);

	// ѡ��ؼ�Ӧ��������������: 
	m_wndTabs.SetWindowPos (NULL, -1, -1, cx, cy, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
}

// 新 UI P4·D：窗格底随主题令牌（深 #111522 / 浅 #EEF1F6，与任务/文件窗格
// 同源）；页签控件覆盖大半客户区，此底供边缘与切换间隙。每笔现读，主题
// 切换的全局 RedrawWindow 即生效
BOOL COutputWnd::OnEraseBkgnd(CDC* pDC)
{
	CRect rc;
	GetClientRect(rc);
	pDC->FillSolidRect(rc, TaskPal().crPaneBg);
	return TRUE;
}

void COutputWnd::AdjustHorzScroll(CListBox& wndListBox)
{
	CClientDC dc(this);
	CFont* pOldFont = dc.SelectObject(&afxGlobalData.fontRegular);

	int cxExtentMax = 0;

	for (int i = 0; i < wndListBox.GetCount(); i ++)
	{
		CString strItem;
		wndListBox.GetText(i, strItem);

		cxExtentMax = std::max(cxExtentMax, (int)dc.GetTextExtent(strItem).cx);
	}

	wndListBox.SetHorizontalExtent(cxExtentMax);
	dc.SelectObject(pOldFont);
}

// ����ԭ�棺���������б��ʼΪ�գ���ͼ���գ�ԭ�����ʱ���κ�����У�
void COutputWnd::FillBuildWindow()
{
}

void COutputWnd::FillDebugWindow()
{
}

void COutputWnd::FillFindWindow()
{
}

void COutputWnd::UpdateFonts()
{
	// 复刻原版只创建“生成”一个列表（Debug/Find 列表框不创建）——模板代码
	// 给三个列表都 SetFont，MFC Debug 断言 IsWindow 在未创建窗口上触发
	// （afxwin2.inl:43 CWnd::SetFont；Release 无断言故长期未察觉，
	// 2026-09-10 用户在 VS2019 F5 调试运行命中）。必须逐个检查句柄。
	if (m_wndOutputBuild.GetSafeHwnd() != NULL)
		m_wndOutputBuild.SetFont(&afxGlobalData.fontRegular);
	if (m_wndOutputDebug.GetSafeHwnd() != NULL)
		m_wndOutputDebug.SetFont(&afxGlobalData.fontRegular);
	if (m_wndOutputFind.GetSafeHwnd() != NULL)
		m_wndOutputFind.SetFont(&afxGlobalData.fontRegular);
}

// �����ɡ�ҳ�б�׷��һ�в��������ף�����ԭ��� LB_ADDSTRING + WM_VSCROLL/SB_BOTTOM��
void COutputWnd::AddBuildLine(LPCTSTR lpszText)
{
	if (m_wndOutputBuild.GetSafeHwnd() == NULL)
		return;
	m_wndOutputBuild.AddString(lpszText);
	m_wndOutputBuild.SendMessage(WM_VSCROLL, SB_BOTTOM, 0);
}

/////////////////////////////////////////////////////////////////////////////
// COutputList1

COutputList::COutputList()
{
}

COutputList::~COutputList()
{
}

BEGIN_MESSAGE_MAP(COutputList, CListBox)
	ON_WM_CONTEXTMENU()
	ON_WM_CTLCOLOR_REFLECT()
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_COMMAND(ID_EDIT_CLEAR, OnEditClear)
	ON_COMMAND(ID_VIEW_OUTPUTWND, OnViewOutput)
	ON_WM_WINDOWPOSCHANGING()
END_MESSAGE_MAP()
/////////////////////////////////////////////////////////////////////////////
// COutputList ��Ϣ�������

/////////////////////////////////////////////////////////////////////////////
// COutputList 消息处理程序

// 新 UI P4·D：深色主题下输出列表换肤。列表父是页签控件（CMFCTabCtrl），
// 走 CTLCOLOR 反射（ON_WM_CTLCOLOR_REFLECT）命中；文字色/底色每笔现读令牌，
// 主题切换的全局 RedrawWindow 即生效，无需切换管线。浅色返回 NULL 走系统
// 默认（白底黑字，取证基线外观不变）。
HBRUSH COutputList::CtlColor(CDC* pDC, UINT /*nCtlColor*/)
{
	if (theApp.GetInt(_T("DarkTheme"), 0) == 0)
		return NULL;
	const STaskPal& pal = TaskPal();
	pDC->SetTextColor(pal.crText);
	pDC->SetBkColor(pal.crPaneBg);
	return TaskPaneBgBrush();
}

void COutputList::OnContextMenu(CWnd* /*pWnd*/, CPoint point)
{
	CMenu menu;
	menu.LoadMenu(IDR_OUTPUT_POPUP);

	CMenu* pSumMenu = menu.GetSubMenu(0);

	if (AfxGetMainWnd()->IsKindOf(RUNTIME_CLASS(CFrameWndEx)))
	{
		CMFCPopupMenu* pPopupMenu = new CMFCPopupMenu;

		if (!pPopupMenu->Create(this, point.x, point.y, (HMENU)pSumMenu->m_hMenu, FALSE, TRUE))
			return;

		((CFrameWndEx*)AfxGetMainWnd())->OnShowPopupMenu(pPopupMenu);
		UpdateDialogControls(this, FALSE);
	}

	SetFocus();
}

// 复制全部日志到剪贴板（新 UI：原版此处为占位消息框"复制...."）
void COutputList::OnEditCopy()
{
	CString strAll;
	const int nCount = GetCount();
	for (int i = 0; i < nCount; ++i)
	{
		CString strLine;
		GetText(i, strLine);
		strAll += strLine;
		strAll += _T("\r\n");
	}
	if (strAll.IsEmpty())
		return;

	if (!OpenClipboard())
		return;
	if (EmptyClipboard())
	{
		const SIZE_T cbBytes = (strAll.GetLength() + 1) * sizeof(TCHAR);
		if (HGLOBAL hData = GlobalAlloc(GMEM_MOVEABLE, cbBytes))
		{
			if (void* pBytes = GlobalLock(hData))
			{
				memcpy(pBytes, (LPCTSTR)strAll, cbBytes);
				GlobalUnlock(hData);
				SetClipboardData(CF_UNICODETEXT, hData);
			}
			else
				GlobalFree(hData);
		}
	}
	CloseClipboard();
}

// 清空日志列表（新 UI：原版此处为占位消息框"清除...."）
void COutputList::OnEditClear()
{
	ResetContent();
}

void COutputList::OnViewOutput()
{
	CDockablePane* pParentBar = DYNAMIC_DOWNCAST(CDockablePane, GetOwner());
	CFrameWndEx* pMainFrame = DYNAMIC_DOWNCAST(CFrameWndEx, GetTopLevelFrame());

	if (pMainFrame != NULL && pParentBar != NULL)
	{
		pMainFrame->SetFocus();
		pMainFrame->ShowPane(pParentBar, FALSE, FALSE, FALSE);
		pMainFrame->RecalcLayout();

	}
}
