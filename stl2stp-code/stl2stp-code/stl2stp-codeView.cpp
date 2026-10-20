
// stl2stp-codeView.cpp : Cstl2stpcodeView ���ʵ��
//

#include "stdafx.h"
// SHARED_HANDLERS ������ʵ��Ԥ��������ͼ������ɸѡ�������
// ATL ��Ŀ�н��ж��壬�����������Ŀ�����ĵ����롣
#ifndef SHARED_HANDLERS
#include "stl2stp-code.h"
#endif

#include "stl2stp-codeDoc.h"
#include "stl2stp-codeView.h"
#include "MainFrm.h"   // 新 UI 增补：视角名称写入状态栏
#include <Bnd_Box.hxx>   // FitAll(Bnd_Box) 显式包围盒适配（MeshVS 结构盒在 master 下退化）
#include <BRepBndLib.hxx>   // 【P4·V】STEP 预览取景：B-Rep 实体包围盒

// 新 UI 增补：视角名称写入状态栏窗格（ID_INDICATOR_VIEW_DIR）——
// 原版状态栏仅有 CAP/NUM/SCRL，不承载任何模型/视图信息
static void SetViewDirStatus(LPCTSTR lpszName)
{
	CMainFrame* pMain = (CMainFrame*)AfxGetMainWnd();
	if (pMain != NULL)
		pMain->SetStatusViewDir(lpszName);
}

#ifdef _DEBUG
// ע�⣺OCCT ������Զ��� operator new���� MFC ���Է�������DEBUG_NEW�������з��գ�
// ά�ֽ��� DEBUG_NEW��Ǩ���Ծ� ABI ���������ر������
//#define new DEBUG_NEW
#endif


// Cstl2stpcodeView

IMPLEMENT_DYNCREATE(Cstl2stpcodeView, CView)

BEGIN_MESSAGE_MAP(Cstl2stpcodeView, CView)
	// ��׼��ӡ����
	ON_COMMAND(ID_FILE_PRINT, &CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, &CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, &Cstl2stpcodeView::OnFilePrintPreview)
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSEWHEEL()
	// 【方案 A】WM_CHAR / WM_KEYDOWN 映射已移除（'.' 与 Delete 删除选中模型不被导出尊重，WYSIWYG）
	ON_WM_DROPFILES()
	// �������ԭ�� CVoxelModView ��Ϣӳ�� 0x140072100..0x1400723C0��
	ON_COMMAND(ID_IMPORT_STL_MODEL, &Cstl2stpcodeView::OnImportStlModel)
	ON_COMMAND(ID_VIEW_FITALL, &Cstl2stpcodeView::OnViewFitall)
	ON_COMMAND(ID_REFRESH_MODEL, &Cstl2stpcodeView::OnRefreshModel)
	ON_COMMAND(ID_VIEW_PROJ_BACK, &Cstl2stpcodeView::OnViewProjBack)
	ON_COMMAND(ID_VIEW_PROJ_TOP, &Cstl2stpcodeView::OnViewProjTop)
	ON_COMMAND(ID_VIEW_PROJ_FRONT, &Cstl2stpcodeView::OnViewProjFront)
	ON_COMMAND(ID_VIEW_PROJ_RIGHT, &Cstl2stpcodeView::OnViewProjRight)
	ON_COMMAND(ID_VIEW_PROJ_BOTTOM, &Cstl2stpcodeView::OnViewProjBottom)
	ON_COMMAND(ID_VIEW_PROJ_AXO, &Cstl2stpcodeView::OnViewProjAxo)
	ON_COMMAND(ID_VIEW_PROJ_LEFT, &Cstl2stpcodeView::OnViewProjLeft)
	ON_COMMAND(ID_VIEW_RESET, &Cstl2stpcodeView::OnViewReset)
	ON_COMMAND(ID_HLR_OFF, &Cstl2stpcodeView::OnHlrOff)
	ON_COMMAND(ID_HLR_ON, &Cstl2stpcodeView::OnHlrOn)
	ON_UPDATE_COMMAND_UI(ID_HLR_OFF, &Cstl2stpcodeView::OnUpdateHlrOff)
	ON_UPDATE_COMMAND_UI(ID_HLR_ON, &Cstl2stpcodeView::OnUpdateHlrOn)
END_MESSAGE_MAP()

// Cstl2stpcodeView ����/����

Cstl2stpcodeView::Cstl2stpcodeView()
{
	// HLR ����ģʽ״̬��ԭ�� view+296 ��ֵΪ 0���رգ�
	m_bComputed = 0;
}

Cstl2stpcodeView::~Cstl2stpcodeView()
{
}

BOOL Cstl2stpcodeView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: �ڴ˴�ͨ���޸�
	//  CREATESTRUCT cs ���޸Ĵ��������ʽ

	return CView::PreCreateWindow(cs);
}

// Cstl2stpcodeView ����

void Cstl2stpcodeView::OnDraw(CDC* /*pDC*/)
{
	Cstl2stpcodeDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;

	// ����ԭ�� 0x1400296D0�����Ѽ���ģ����ˢ�� OCCT ��ͼ
	// ��ԭ��˴�ͬʱ�ؽ� B-Rep���˴���ˢ����ʾ��B-Rep �ؽ���� STEP ����ʵ�֣�
	if (!m_hView.IsNull())
		m_hView->V3d_View::Update();
}

// Cstl2stpcodeView ��ʼ�� ���� ����ԭ�� sub_140028CD0��OnInitialUpdate��

// 前置声明：OnInitialUpdate 的"载入即取景"在本函数定义之前使用
static void FitViewToModel(Cstl2stpcodeDoc* pDoc, const opencascade::handle<V3d_View>& hView);

void Cstl2stpcodeView::OnInitialUpdate()
{
	CView::OnInitialUpdate();

	// P4·P 取证：视图初始化打点（extern 链接文档侧日志出口，环境变量门控）
	extern void StatsDebugLog(const char* fmt, ...);
	StatsDebugLog("P4P: view OnInitialUpdate enter");

	Cstl2stpcodeDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (pDoc == NULL || pDoc->m_hViewer.IsNull())
		return;

	// ���ĵ��� Viewer �ϴ�������ͼ��Ӧ�� V3d_View
	m_hView = pDoc->m_hViewer->CreateView();
	StatsDebugLog("P4P: view CreateView done");

	// �����󶨵�����ͼ���ڵ� WNT_Window��ԭ�湹����� (HWND, NameOfColor 2=MATRAGRAY)��
	m_hWin = new WNT_Window((Aspect_Handle)GetSafeHwnd());

	// �ҽӴ��ڣ�ԭ�� SetWindow(win, NULL)���°�ڶ���Ϊ��Ⱦ�����ľ����Ĭ�Ͽգ�
	m_hView->SetWindow(m_hWin);

	// ���������ã�ԭ�� ZBufferTriedronSetup(412, 229, 22, 0.8, 0.05, 12)��
	// ��ԭ���ͼȡɫ��֤ X/Y/Z �ἴ����/��/�����°�Ĭ��ֵһ�£���ȫ��Ĭ�ϲ�����
	m_hView->ZBufferTriedronSetup();

	// ��ʾ�����ᣨԭ�� TriedronDisplay(λ��6=���½�, ��ɫ516=��, ����0.1, ZBUFFER)��
	m_hView->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_WHITE,
		0.1, V3d_ZBUFFER);

	// 取证（2026-09）：原版初始视图为透视投影——其网格行明显透视收敛
	// （同尺寸窗口实测网格点距：原版底行 8px → 顶行 5px 递减；复刻版正交投影恒定 6px）。
	// OCCT master 默认相机是正交，这里显式切到透视以对齐原版初始观感。
	// 排障教训（2026-09-08）：直接改 Camera() 后必须 ZFitAll() 重算深度范围——
	// 透视投影沿用正交的 ZNear/ZFar 会把结构整体裁掉（网格/三轴走特殊管线不受
	// 影响，表现为“模型不渲染”，网格点距正常）。改动曾一度无 ZFitAll，导致所有
	// 载入路径的模型均不可见（用户实测报告）。
	if (!m_hView->Camera().IsNull())
	{
		opencascade::handle<Graphic3d_Camera> hCam = m_hView->Camera();
		hCam->SetProjectionType(Graphic3d_Camera::Projection_Perspective);
		// 对齐原版的相机远近：原版透视梯度更平缓——网格点距实测约为复刻版
		// 默认的 0.54 倍（复刻默认 12px vs 原版约 6.5px），初始距离放大同比例。
		hCam->SetDistance(hCam->Distance() * 1.85);
		// 直接改 Camera() 后必须 ZFitAll() 重算深度范围（排障教训：透视投影
		// 沿用正交的 ZNear/ZFar 会把结构裁掉；网格/三轴走特殊管线不受影响，
		// 表现为"模型不渲染"而网格点距正常）。
		m_hView->ZFitAll();
	}

	// 【新 UI P4·G（用户指定）】视口背景随主题：深色=暗色渐变、浅色=原版取证
	// 基线渐变（浅色模式与原版逐像素一致；模型/网格线/三轴渲染外观不动）
	ApplyViewTheme();

	// 载入即取景（原版用户实测：拖入模型后以合适视角直接可见）。
	// 命令行/文件关联路径的载入发生在视图创建之前（MFC：OnOpenDocument 先于
	// OnInitialUpdate），文档侧无法 FitAll——统一在视图初始化时对已载入模型
	// 取景；导入/拖放路径在各自命令处理器中取景（FitViewToModel 同型）。
	if (pDoc != NULL && pDoc->m_bLoaded && !m_hView.IsNull())
		FitViewToModel(pDoc, m_hView);
	StatsDebugLog("P4P: view initial fit done");

	// 原版支持拖放/文件关联载入（exe 导入表含 DragAcceptFiles，文件枚举走 MFC 内部）。
	// 视图窗口注册接受拖放——否则把 STL 拖进窗口无任何反应（2026-09-08 用户实测缺口）。
	DragAcceptFiles(TRUE);
	StatsDebugLog("P4P: view OnInitialUpdate exit");

	// �òο�����ɼ�������ԭ��Ŀɼ�Ч������
	// ����ֵ��0,0/10,10/1000���� ActivateGrid �����ĵ����������ã�
	// �°� V3d_Viewer::ActivateGrid �ڲ������ Grid->Display()���������ֶ�ˢ�¡�

	// �����д򿪵��ĵ����� FitAll��ԭ���ͼʵ�⣺��ͼͣ���� Viewer Ĭ�������
	// 1 ��λ�����岻�ɼ�������=���䱳��+�������FitAll ����ԭ�桰����STLģ�͡�
	// �������� sub_14003����·����ͨ�� TKV3d ������е� FitAll(double,bool) ���ã���
}


// Cstl2stpcodeView ��ӡ


// 【新 UI P4·G（用户指定）】视口背景随主题。深色主题=暗色垂直渐变（登记
// 有意偏差：原版 STL2STP 无深色主题，浅色模式保持取证基线即可）；浅色主题
// =原版取证基线渐变（顶 (180,200,255)/底 (180,180,180)，sub_14000C3E5 取证）。
// 模型纯黄+DEFAULT 材质、网格线 GRAY50/70、白色三轴不随主题变——取证基线
// 的"渲染外观"仅背景随主题，其余不动。master 渲染管线输出端做 sRGB 编码
//（见 Doc.cpp OnNewDocument 补偿注释），故传线性值 = sRGBDecode(目标显示值)：
// 深色目标 顶 (44,58,88) → (0.02518, 0.04230, 0.09755)、
// 底 (18,23,34) → (0.00605, 0.00857, 0.01599)——与应用深色族令牌同系，
// 顶偏蓝保留纵深。浅色两组值与 Doc.cpp 基线常量逐位一致。
void Cstl2stpcodeView::ApplyViewTheme()
{
	if (m_hView.IsNull())
		return;
	if (theApp.GetInt(_T("DarkTheme"), 0) != 0)
	{
		Quantity_Color aTop(0.02518, 0.04230, 0.09755, Quantity_TOC_RGB);
		Quantity_Color aBot(0.00605, 0.00857, 0.01599, Quantity_TOC_RGB);
		m_hView->SetBgGradientColors(aTop, aBot, Aspect_GFM_VER);
	}
	else
	{
		Quantity_Color aTop(0.45647, 0.57757, 1.0, Quantity_TOC_RGB);
		Quantity_Color aBot(0.45647, 0.45647, 0.45647, Quantity_TOC_RGB);
		m_hView->SetBgGradientColors(aTop, aBot, Aspect_GFM_VER);
	}
	m_hView->Redraw();
}

void Cstl2stpcodeView::OnFilePrintPreview()
{
#ifndef SHARED_HANDLERS
	AFXPrintPreview(this);
#endif
}

BOOL Cstl2stpcodeView::OnPreparePrinting(CPrintInfo* pInfo)
{
	// Ĭ��׼��
	return DoPreparePrinting(pInfo);
}

void Cstl2stpcodeView::OnBeginPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: ��Ӷ���Ĵ�ӡǰ���еĳ�ʼ������
}

void Cstl2stpcodeView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: ��Ӵ�ӡ����е��������
}

// �Ҽ����£�����ԭ�� sub_140029280����¼���㣻Shift+�Ҽ�������ת��
void Cstl2stpcodeView::OnRButtonDown(UINT nFlags, CPoint point)
{
	m_ptRDown = point;    // ԭ�� view+256
	m_ptPanRef = point;    // ԭ�� view+264

	if ((nFlags & MK_SHIFT) && !m_hView.IsNull())
		m_hView->StartRotation(point.x, point.y, 0.0);

	CView::OnRButtonDown(nFlags, point);
}

// �Ҽ�̧�𣨸���ԭ�� sub_1400292D0��δ�϶�ʱ�ڹ�괦�������������˵� 119��
void Cstl2stpcodeView::OnRButtonUp(UINT nFlags, CPoint point)
{
	if (point == m_ptRDown)
	{
		ClientToScreen(&point);
		theApp.GetContextMenuManager()->ShowPopupMenu(
			IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
	}
}


// Cstl2stpcodeView ���

#ifdef _DEBUG
void Cstl2stpcodeView::AssertValid() const
{
	CView::AssertValid();
}

void Cstl2stpcodeView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

Cstl2stpcodeDoc* Cstl2stpcodeView::GetDocument() const // �ǵ��԰汾��������
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(Cstl2stpcodeDoc)));
	return (Cstl2stpcodeDoc*)m_pDocument;
}
#endif //_DEBUG


// Cstl2stpcodeView ��Ϣ�������

// �������������� TRUE ���� MFC �ð�ɫ��������������˸��OpenGL ���ڱ�׼������
BOOL Cstl2stpcodeView::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE;
}

// ��С�仯������ԭ�� sub_140029AB0����Ĭ�ϴ����� MustBeResized��
void Cstl2stpcodeView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);

	if (!m_hView.IsNull())
		m_hView->MustBeResized();
}

// ������£�����ԭ�� sub_1400290A0����¼��㣻�������Ѹ����Ҽ���ʱ StartTransform��
void Cstl2stpcodeView::OnLButtonDown(UINT nFlags, CPoint point)
{
	m_ptRectStart = point;   // 原版 view+248
	m_ptCur = point;         // 原版 view+288
	m_bSelectDrag = FALSE;   // P4·H：仅 Ctrl+左键拖动会置位（框选）

	// 【新 UI P4·H（用户指定"随鼠标拖动自由转动"）】左键拖动=自由旋转
	//（原型规格"左键旋转/右键平移"）：按下即锚定旋转起点；原版左键拖动的
	// 框选语义移至 Ctrl+左键（Shift/Ctrl+右键旋转/平移、右键单击菜单全保留）。
	if ((nFlags & MK_CONTROL) == 0 && !m_hView.IsNull())
		m_hView->StartRotation(point.x, point.y, 0.0);

	// 【方案 A】原操纵器 StartTransform 调用已移除（WYSIWYG）

	CView::OnLButtonDown(nFlags, point);
}

// ����ƶ�������ԭ�� sub_1400293E0 ��������֧�߼���
void Cstl2stpcodeView::OnMouseMove(UINT nFlags, CPoint point)
{
	m_ptCur = point;   // ԭ�� view+288

	Cstl2stpcodeDoc* pDoc = GetDocument();
	if (pDoc == NULL || pDoc->m_hAISCtx.IsNull())
	{
		CView::OnMouseMove(nFlags, point);
		return;
	}

	if (nFlags & MK_LBUTTON)
	{
		// 【新 UI P4·H】左键拖动双语义：Ctrl+左键=框选（原版左键语义移此，
		// Shift 再按=加选 XOR）；普通左键=自由旋转（原型规格）
		if (nFlags & MK_CONTROL)
		{
			m_bSelectDrag = TRUE;
			if (nFlags & MK_SHIFT)
			{
				pDoc->m_hAISCtx->SelectRectangle(
					NCollection_Vec2<int>(m_ptRectStart.x, m_ptRectStart.y),
					NCollection_Vec2<int>(point.x, point.y), m_hView,
					AIS_SelectionScheme_XOR);
				m_hView->Update();
			}
			else
			{
				pDoc->m_hAISCtx->SelectRectangle(
					NCollection_Vec2<int>(m_ptRectStart.x, m_ptRectStart.y),
					NCollection_Vec2<int>(point.x, point.y), m_hView,
					AIS_SelectionScheme_Replace);
				m_hView->Update();
			}

			// 框选拖动橡皮筋矩形（原版经视图绘图设备位 +880 调用）
			DrawDragRect(m_ptRectStart.x, m_ptRectStart.y, point.x, point.y, TRUE);
		}
		else if (!m_hView.IsNull())
		{
			m_hView->Rotation(point.x, point.y);
		}
	}
	else if (nFlags & MK_RBUTTON)
	{
		if ((nFlags & MK_SHIFT) && !m_hView.IsNull())
		{
			// Shift+右键拖动=旋转（原版语义保留）
			m_hView->Rotation(point.x, point.y);
		}
		else if (!m_hView.IsNull())
		{
			// 【新 UI P4·H】右键拖动=平移（原型规格；原 Ctrl+右键平移语义
			// 保留且与之同式）。未拖动单击仍弹编辑菜单（OnRButtonUp 判位移）
			m_hView->Pan(point.x - m_ptPanRef.x, m_ptPanRef.y - point.y, 1.0, true);
			m_ptPanRef = point;
		}
	}
	else
	{
		// �ް�����������⣨MoveTo��
		pDoc->m_hAISCtx->MoveTo(point.x, point.y, m_hView, true);
	}

	CView::OnMouseMove(nFlags, point);
}

// ���̧�𣨸���ԭ�� sub_140029140���϶���ͣ�ò�����ģʽ���������Σ�
//   �����Select/ShiftSelect ��ͣ�ò�����ģʽ��
void Cstl2stpcodeView::OnLButtonUp(UINT nFlags, CPoint point)
{
	Cstl2stpcodeDoc* pDoc = GetDocument();

	if (m_bSelectDrag)
	{
		// Ctrl+框选拖动结束：仅擦除橡皮筋矩形（P4·H；原操纵器停用调用已移除，方案 A）
		DrawDragRect(m_ptRectStart.x, m_ptRectStart.y, point.x, point.y, FALSE);
	}
	else if (point == m_ptRectStart && pDoc != NULL && !pDoc->m_hAISCtx.IsNull())
	{
		// 未拖动单击：拾取选择（Shift 为加选）；P4·H：旋转拖动松开不算单击
		// δ�϶������ʰȡ��Shift Ϊ��ѡ�����°� SelectDetected �����Զ�ˢ�£�����ʽ Update
		if (nFlags & MK_SHIFT)
			pDoc->m_hAISCtx->SelectDetected(AIS_SelectionScheme_XOR);
		else
			pDoc->m_hAISCtx->SelectDetected(AIS_SelectionScheme_Replace);
		m_hView->Update();
	}

	m_bSelectDrag = FALSE;             // P4·H：复位（普通左键拖动=旋转，无需收尾）
	m_ptRectStart = CPoint(0, 0);   // ԭ�潫 +248 ����

	CView::OnLButtonUp(nFlags, point);
}

// ��ѡ��ק��Ƥ����Σ�ԭ�澭��ͼ����λ +880 ���ã�
//   DrawFocusRect Ϊ XOR ���ƣ�ͬһ�����ٻ�һ�μ�������
void Cstl2stpcodeView::DrawDragRect(int x1, int y1, int x2, int y2, BOOL /*bDraw*/)
{
	CRect rect(x1, y1, x2, y2);
	rect.NormalizeRect();
	if (rect.IsRectEmpty())
		return;

	CClientDC dc(this);
	dc.DrawFocusRect(&rect);
}

// �������ţ�����ԭ�� sub_140029370��zDelta ���� -120��SetZoom(z/360+1, true)��
BOOL Cstl2stpcodeView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	short z = zDelta;
	if (z < -120)
		z = -120;

	if (!m_hView.IsNull())
		m_hView->SetZoom(z / 360.0 + 1.0, true);

	return CView::OnMouseWheel(nFlags, zDelta, pt);
}

// ������STLģ�͡����� ���� ����ԭ�� sub_140028E70 ����������
// 用三角网格节点直接构造包围盒驱动 FitAll 的 Bnd_Box 重载。
// OCCT master 下对含 MeshVS_Mesh 的场景直接 FitAll(0.01, true)（走结构包围盒
// 计算）会得到退化盒，相机变为无效后整个视图停止渲染——网格点/三轴/模型全部
// 消失只剩渐变背景（2026-09-08 导入按钮路径实测）。原版（OCCT 7.0）无此问题；
// master 的等价路径 = 用模型自身的 Bnd_Box 显式适配。
static void FitViewToModel(Cstl2stpcodeDoc* pDoc, const opencascade::handle<V3d_View>& hView)
{
	if (pDoc == NULL || hView.IsNull())
		return;
	// 【P4·V】STEP 预览态：包围盒取 B-Rep 实体（BRepBndLib 几何盒，无网格
	// 节点可遍历）；取景参数与网格路径一致（0.2 边距 = P4·I 口径）
	if (pDoc->IsStepPreview())
	{
		if (pDoc->m_rebuiltShape.IsNull())
			return;
		Bnd_Box aBoxStep;
		BRepBndLib::Add(pDoc->m_rebuiltShape, aBoxStep);
		if (!aBoxStep.IsVoid())
		{
			hView->FitAll(aBoxStep, 0.2, true);
			hView->ZFitAll(1.0);   // 深度范围重算（与网格路径同式）
		}
		return;
	}
	if (pDoc->m_hTriang.IsNull())
		return;
	Bnd_Box aBox;
	const int nNodes = pDoc->m_hTriang->NbNodes();
	for (int i = 1; i <= nNodes; ++i)
		aBox.Add(pDoc->m_hTriang->Node(i));
	if (!aBox.IsVoid())
	{
		// 【P4·I 优化（用户指定"模型默认显示在屏幕中间"）】取景边距 0.01→0.2：
		// OCCT FitAll(Bnd_Box) 本就把包围盒几何中心对到屏幕中心（Graphic3d_Camera::
		// FitMinMax 源核实），但 0.01 让模型占满视口 99%——niuniu 实测顶余 80px、
		// 底仅余 8px，贴边无"居中感"。0.2 = 模型占视口约 83%，四周留 ~8% 呼吸边。
		// 原版"适应"命令的 0.01（sub_140029C30 取证）为当前口径，放宽登记有意
		// 偏差；载入即取景本身已是复刻版增强，边距为其参数。
		hView->FitAll(aBox, 0.2, true);
		hView->ZFitAll(1.0);   // 装载/重建后重算深度范围（透视下尤其必要）
	}
}

void Cstl2stpcodeView::OnImportStlModel()
{
	CFileDialog dlg(TRUE, NULL, NULL,
		OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		// 【P4·V】过滤器新增 STEP 条目（用户指定"把 STEP 预览做进应用里"；P4·R
		// 复原的 STL 文案保持首项不变——扩展登记有意偏差）
		_T("STL 文件 (*.stl)|*.stl|STEP 文件 (*.step;*.stp)|*.step;*.stp|所有文件 (*.*)|*.*||"));
	dlg.m_ofn.lpstrTitle = _T("打开模型文件");

	// ��ʼĿ¼����ȡ�������� CSF_OCCTDataPath �µ� stl ��Ŀ¼����ԭ��һ�£�
	CString strInitialDir;
	strInitialDir.GetEnvironmentVariable(_T("CSF_OCCTDataPath"));
	if (!strInitialDir.IsEmpty())
		dlg.m_ofn.lpstrInitialDir = (strInitialDir + _T("\\stl")).GetString();

	CString strFile;
	TCHAR* pBuffer = strFile.GetBuffer(MAX_PATH);
	dlg.m_ofn.lpstrFile = pBuffer;

	INT_PTR nResult = dlg.DoModal();
	strFile.ReleaseBuffer(-1);

	if (nResult != IDOK)
		return;

	Cstl2stpcodeDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (pDoc == NULL)
		return;

	CWaitCursor wait;   // ԭ���ڴ˴����õȴ���� IDC_WAIT

	// ԭ�����̣�doc->OnOpenDocument(path) �� doc->LoadStlAndDisplay(path) �� FitAll
	if (!pDoc->OnOpenDocument(strFile))
		return;

	if (!m_hView.IsNull())
		FitViewToModel(pDoc, m_hView);

	// ԭ���ڴ˴��ָ���� IDC_ARROW
}

// ����Ӧ��ͼ���������ԭ�� sub_140029C30��FitAll(0.01, true) + ZFitAll(1.0)��
// 拖放载入：复刻原版拖放交互（与导入按钮同型——OnOpenDocument + FitAll；
// 原版走 MFC ON_WM_DROPFILES，DragQueryFile 在 MFC 内部调用，exe 导入表只见 DragAcceptFiles）
void Cstl2stpcodeView::OnDropFiles(HDROP hDropInfo)
{
	Cstl2stpcodeDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (pDoc == NULL)
	{
		DragFinish(hDropInfo);
		return;
	}

	CString strFile;
	const UINT nCount = ::DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0);
	if (nCount > 0)
	{
		TCHAR* pBuffer = strFile.GetBuffer(MAX_PATH);
		::DragQueryFile(hDropInfo, 0, pBuffer, MAX_PATH);
		strFile.ReleaseBuffer();
	}
	DragFinish(hDropInfo);

	if (strFile.IsEmpty())
		return;

	CWaitCursor wait;
	if (pDoc->OnOpenDocument(strFile))
	{
		if (!m_hView.IsNull())
			FitViewToModel(pDoc, m_hView);
	}
}

void Cstl2stpcodeView::OnViewFitall()
{
	if (!m_hView.IsNull())
	{
		FitViewToModel(GetDocument(), m_hView);
		m_hView->ZFitAll(1.0);
	}
}

// 【新 UI P2·SDI 化阶段 1.2】SDI 文档复用后的重取景公开入口（文档侧
// RefitActiveView 调用）：MRU/Ctrl+O/再次打开路径在视图已存在时复用同一文档，
// OnInitialUpdate 不再触发，由文档在载入成功后取景（FitViewToModel 同型）。
void Cstl2stpcodeView::RefitModel()
{
	if (!m_hView.IsNull())
	{
		FitViewToModel(GetDocument(), m_hView);
		m_hView->ZFitAll(1.0);
	}
}

// ��ˢ����ʾ���������ԭ�� sub_1400296D0��
//   �Ѽ���ģ��ʱ�ؽ� BRep �� V3d_View::Update��BRep �ؽ����������̱���
void Cstl2stpcodeView::OnRefreshModel()
{
	Cstl2stpcodeDoc* pDoc = GetDocument();
	if (pDoc == NULL || !pDoc->IsModelLoaded())
		return;

	// original sub_1400296D0 rebuilds the shape from the mesh first
	// (sub_14000CE80 chain), then updates the view
	pDoc->RebuildShapeFromMesh();

	if (!m_hView.IsNull())
		m_hView->Update();
}

// ==== ��ͼ�������ԭ��Ϊֱ�ӵ��� V3d_View::SetProj �İ�װ������ ====

// 【新 UI P4·S（用户反馈：点前视图等标准视图后模型出画）】master 的
// V3d_View::SetProj(orientation)/Reset 把相机中心重设到世界原点、视点落在
// "到原点等距"的标准轴上（V3d_View.cxx:1314-1324，保留的仅是相对原点的
// 平移）——偏离原点的模型（悟空.stl 包围盒中心 (169,110,48)，离原点
// ~206 单位）整体出画。P2 时代六视图对照用的是近原点小模型未暴露。
// 修复口径：投影切换后重跑与载入一致的取景（FitViewToModel：包围盒
// FitAll 边距 0.2 + ZFitAll），保证模型居中可见。登记有意偏差：缩放随
// 取景复位到适配档（不保留切换前的距离）；无模型时自动跳过。
void Cstl2stpcodeView::OnViewProjBack()   // sub_140029AE0: SetProj(4)
{
	if (!m_hView.IsNull())
	{
		m_hView->SetProj((V3d_TypeOfOrientation)4);
		FitViewToModel(GetDocument(), m_hView);   // 【P4·S】重取景防出画（见本组头部注释）
	}
	SetViewDirStatus(_T("后视图"));
}

void Cstl2stpcodeView::OnViewProjTop()    // sub_140029B40: SetProj(2)
{
	if (!m_hView.IsNull())
	{
		m_hView->SetProj((V3d_TypeOfOrientation)2);
		FitViewToModel(GetDocument(), m_hView);
	}
	SetViewDirStatus(_T("俯视图"));
}

void Cstl2stpcodeView::OnViewProjFront()  // sub_140029B00: SetProj(1)
{
	if (!m_hView.IsNull())
	{
		m_hView->SetProj((V3d_TypeOfOrientation)1);
		FitViewToModel(GetDocument(), m_hView);
	}
	SetViewDirStatus(_T("前视图"));
}

void Cstl2stpcodeView::OnViewProjRight()  // sub_140029B60: SetProj(0)
{
	if (!m_hView.IsNull())
	{
		m_hView->SetProj((V3d_TypeOfOrientation)0);
		FitViewToModel(GetDocument(), m_hView);
	}
	SetViewDirStatus(_T("右视图"));
}

void Cstl2stpcodeView::OnViewProjBottom() // sub_140029B20: SetProj(5)
{
	if (!m_hView.IsNull())
	{
		m_hView->SetProj((V3d_TypeOfOrientation)5);
		FitViewToModel(GetDocument(), m_hView);
	}
	SetViewDirStatus(_T("仰视图"));
}

void Cstl2stpcodeView::OnViewProjAxo()    // sub_140029B90: SetProj(19)
{
	if (!m_hView.IsNull())
	{
		m_hView->SetProj((V3d_TypeOfOrientation)19);
		FitViewToModel(GetDocument(), m_hView);
	}
	SetViewDirStatus(_T("轴测视图"));
}

void Cstl2stpcodeView::OnViewProjLeft()   // sub_140029B70: SetProj(3)
{
	if (!m_hView.IsNull())
	{
		m_hView->SetProj((V3d_TypeOfOrientation)3);
		FitViewToModel(GetDocument(), m_hView);
	}
	SetViewDirStatus(_T("左视图"));
}

// ��ͼ��λ��ԭ�� sub_140029C70��Reset(true)��
void Cstl2stpcodeView::OnViewReset()
{
	if (!m_hView.IsNull())
	{
		m_hView->Reset(true);
		// 【P4·S】Reset 同样回到原点中心的默认视点——重取景与载入态一致
		FitViewToModel(GetDocument(), m_hView);
	}
	SetViewDirStatus(_T("复位视图"));
}

// ==== HLR������������������ģʽ ====

// �رռ���ģʽ������ԭ�� sub_140029BB0��
void Cstl2stpcodeView::OnHlrOff()
{
	m_bComputed = 0;
	if (!m_hView.IsNull())
		m_hView->SetComputedMode(false);
}

// �������ģʽ������ԭ�� sub_140029BD0���ȴ���� �� SetComputedMode(true) �� �ָ���꣩
void Cstl2stpcodeView::OnHlrOn()
{
	SetCursor(LoadCursor(NULL, IDC_WAIT));
	m_bComputed = 1;
	if (!m_hView.IsNull())
		m_hView->SetComputedMode(true);
	SetCursor(LoadCursor(NULL, IDC_ARROW));
}

// HLR ��ť��ѡ/����״̬������ԭ�� sub_140029C80 / sub_140029CC0��
//   ����ť�������ã����ء����ڿ���ʱ���ã����������ڹر�ʱ���ã�
void Cstl2stpcodeView::OnUpdateHlrOff(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_bComputed == 0);
	pCmdUI->Enable(m_bComputed != 0);
}

void Cstl2stpcodeView::OnUpdateHlrOn(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_bComputed != 0);
	pCmdUI->Enable(m_bComputed == 0);
}

// 銆愭柟妗?A锛圵YSIWYG锛夈€戝師鏂囦欢灏鹃儴鐨?DeleteSelectedObjects 闈欐€佸嚱鏁颁笌 OnChar('.')/
// OnKeyDown(VK_DELETE) 瑙嗗彛鍒犻櫎澶勭悊鍣ㄥ凡绉婚櫎鈥斺€旇鍙ｅ垹闄や笉琚鍑哄皧閲嶏紙瀵煎嚭浠嶇敤鍘熷缃戞牸锛夈€俙r
// 鍘熷疄鐜拌褰曡 AGENT.md 楠屾敹琛ュ厖鍗併€俙r
