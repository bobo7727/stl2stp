
#pragma once

#include "ViewTree.h"

class CFileView : public CDockablePane
{
// 构造
public:
	CFileView();

	void AdjustLayout();
	void OnChangeVisualStyle();
	void ApplyTheme(BOOL bDark);   // 新 UI P4·D：树控件色随主题（深色换肤/浅色还原系统默认）；深浅态由调用方显式传入——切换早于注册表落盘，现读注册表会滞后一拍

// 特性
public:
	// 新 UI：列出当前打开的全部文档（原版仅一个静态“模型”树节点）。
	// pExclude = 待删除文档（OnCloseDocument 在 delete this 之前刷新时排除自身）。
	void RefreshFileList(CDocument* pExclude = NULL);

protected:

	CViewTree m_wndFileView;
	CImageList m_FileViewImages;

protected:

// 实现
public:
	virtual ~CFileView();

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);   // 新 UI P4·D：窗格底随主题令牌
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnProperties();
	afx_msg void OnFileOpen();
	afx_msg void OnFileOpenWith();
	afx_msg void OnDummyCompile();
	afx_msg void OnEditCut();
	afx_msg void OnEditCopy();
	afx_msg void OnEditClear();
	afx_msg void OnPaint();
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnTreeSelChanged(NMHDR* pNMHDR, LRESULT* pResult);   // 新 UI：点击列表项激活对应文档

	DECLARE_MESSAGE_MAP()
};

