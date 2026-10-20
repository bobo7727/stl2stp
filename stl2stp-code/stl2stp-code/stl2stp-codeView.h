
// stl2stp-codeView.h : Cstl2stpcodeView 类的接口
//

#pragma once

class Cstl2stpcodeDoc;   // 前置声明（GetDocument 返回型；MainFrm.cpp 经本头使用视图需先于此可见）

class Cstl2stpcodeView : public CView
{
protected: // 仅从序列化创建
	Cstl2stpcodeView();
	DECLARE_DYNCREATE(Cstl2stpcodeView)

// 特性
public:
	Cstl2stpcodeDoc* GetDocument() const;

// 操作
public:
	// 【新 UI P2·SDI 化阶段 1.2】SDI 文档复用后的重取景入口：MRU/Ctrl+O/再次
	// 打开时复用同一文档，OnInitialUpdate 不再触发，由文档 OnOpenDocument 在
	// 载入成功后调用（FitViewToModel 同型）
	void RefitModel();

	// 【新 UI P4·P】视图管线就绪判定（OnInitialUpdate 已挂 V3d_View）：
	// 文档侧据以区分交互态载入（菜单/MRU/拖放，消息循环已运行——完整启用
	// 进度对话框）与启动期载入（ProcessShellCommand 栈内的命令行/文件关联
	// 路径，视图未初始化，不可泵消息——见 Doc.cpp LoadStlAndDisplay 取证注释）
	BOOL IsViewReady() const { return !m_hView.IsNull(); }

	// 【新 UI P4·G（用户指定）】视口背景随主题：深色主题=暗色垂直渐变（登记
	// 有意偏差，模型纯黄/DEFAULT 材质/网格线/三轴不随主题变=取证基线渲染外观
	// 不动）；浅色主题=原版取证基线渐变（与原版逐像素一致）。创建时与主框架
	// ApplyVisualTheme 切换时调用
	void ApplyViewTheme();

// 重写
public:
	virtual void OnDraw(CDC* pDC);  // 重写以绘制该视图
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void OnInitialUpdate();
protected:
	virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
	virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);

// 实现
public:
	virtual ~Cstl2stpcodeView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// OpenCASCADE 3D 视图（复刻原版 CVoxelModView 成员：+232 视图句柄，+240 窗口句柄）
	opencascade::handle<V3d_View>   m_hView;
	opencascade::handle<WNT_Window> m_hWin;

protected:

// 生成的消息映射函数
protected:
	afx_msg void OnFilePrintPreview();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);   // 原版 sub_140029280
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);     // 原版 sub_1400292D0
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);    // 原版 sub_1400293E0
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnDropFiles(HDROP hDropInfo);   // 拖放载入（原版拖放/文件关联同通道）
	// 【方案 A】OnChar('.') / OnKeyDown(VK_DELETE) 视口删除处理器已移除（WYSIWYG）
	afx_msg void OnImportStlModel();    // 原版 sub_140028E70 (ID 32773)
	afx_msg void OnViewFitall();        // 原版 sub_140029C30 (ID 32785)
	afx_msg void OnRefreshModel();      // 原版 sub_1400296D0 (ID 32778)
	afx_msg void OnViewProjBack();      // SetProj(4)
	afx_msg void OnViewProjTop();       // SetProj(2)
	afx_msg void OnViewProjFront();     // SetProj(1)
	afx_msg void OnViewProjRight();     // SetProj(0)
	afx_msg void OnViewProjBottom();    // SetProj(5)
	afx_msg void OnViewProjAxo();       // SetProj(19)
	afx_msg void OnViewProjLeft();      // SetProj(3)
	afx_msg void OnViewReset();         // 原版 sub_140029C70: Reset(true)
	afx_msg void OnHlrOff();            // 原版 sub_140029BB0
	afx_msg void OnHlrOn();             // 原版 sub_140029BD0
	afx_msg void OnUpdateHlrOff(CCmdUI* pCmdUI);  // 原版 sub_140029C80
	afx_msg void OnUpdateHlrOn(CCmdUI* pCmdUI);   // 原版 sub_140029CC0
	DECLARE_MESSAGE_MAP()

	// 鼠标交互状态（对应原版 CVoxelModView 成员偏移）
	CPoint m_ptRectStart;   // +248 矩形框选起点（左键按下时记录）
	CPoint m_ptRDown;      // +256 右键按下位置（用于右键未拖动时弹菜单）
	CPoint m_ptPanRef;     // +264 平移参考点
	CPoint m_ptCur;        // +288 鼠标当前点（操纵器变换用）
	BYTE   m_bComputed;    // +296 HLR 计算模式状态（0=关 1=开）
	// 【新 UI P4·H】本轮左键拖动是否为框选（Ctrl+左键）——区分于
	// 普通左键自由旋转；抬起时仅框选拖动需要擦除橡皮筋矩形
	BOOL   m_bSelectDrag = FALSE;

	// 框选拖拽橡皮筋矩形（原版经视图虚表槽位 +880 调用的自绘方法）
	void DrawDragRect(int x1, int y1, int x2, int y2, BOOL bDraw);
};

#ifndef _DEBUG  // stl2stp-codeView.cpp 中的调试版本
inline Cstl2stpcodeDoc* Cstl2stpcodeView::GetDocument() const
   { return reinterpret_cast<Cstl2stpcodeDoc*>(m_pDocument); }
#endif

