
#pragma once

/////////////////////////////////////////////////////////////////////////////
// COutputList 窗口

class COutputList : public CListBox
{
// 构造
public:
	COutputList();

// 实现
public:
	virtual ~COutputList();

protected:
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnEditCopy();
	afx_msg void OnEditClear();
	afx_msg void OnViewOutput();
	afx_msg HBRUSH CtlColor(CDC* pDC, UINT nCtlColor);   // 新 UI P4·D：深色列表换肤（CTLCOLOR 反射；两参签名）

	DECLARE_MESSAGE_MAP()
};

class COutputWnd : public CDockablePane
{
// 构造
public:
	COutputWnd();

	void UpdateFonts();

// 操作
public:
	// 向“生成”页列表追加一行并滚动到底（复刻原版 LB_ADDSTRING + WM_VSCROLL SB_BOTTOM）
	void AddBuildLine(LPCTSTR lpszText);

	// 导出流程日志：复刻原版的原始消息序列（LB_ADDSTRING + LB_SETCURSEL(7)，
	// 对应原版 mainframe+58576 的列表框），随后 UpdateWindow 刷新窗格（+41840）
	void OriginalLogLine(const wchar_t* lpszText)
	{
		if (m_wndOutputBuild.GetSafeHwnd() != NULL)
		{
			::SendMessage(m_wndOutputBuild.GetSafeHwnd(), LB_ADDSTRING, 0, (LPARAM)lpszText);
			::SendMessage(m_wndOutputBuild.GetSafeHwnd(), LB_SETCURSEL, 7, 0);
		}
		UpdateWindow();
	}

// 特性
protected:
	CMFCTabCtrl	m_wndTabs;

	COutputList m_wndOutputBuild;
	COutputList m_wndOutputDebug;
	COutputList m_wndOutputFind;

protected:
	void FillBuildWindow();
	void FillDebugWindow();
	void FillFindWindow();

	void AdjustHorzScroll(CListBox& wndListBox);

// 实现
public:
	virtual ~COutputWnd();

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);   // 新 UI P4·D：窗格底随主题令牌

	DECLARE_MESSAGE_MAP()
};

