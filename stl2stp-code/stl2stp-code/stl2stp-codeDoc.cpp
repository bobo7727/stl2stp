
// stl2stp-codeDoc.cpp : Cstl2stpcodeDoc ���ʵ��
//

#include "stdafx.h"
// SHARED_HANDLERS ������ʵ��Ԥ��������ͼ������ɸѡ�������
// ATL ��Ŀ�н��ж��壬�����������Ŀ�����ĵ����롣
#ifndef SHARED_HANDLERS
#include "stl2stp-code.h"
#endif

#include "stl2stp-codeDoc.h"

#include "stl2stp-code.h"
#include "MainFrm.h"
#include "stl2stp-codeView.h"   // ApplyRenderingParams ��Ҫ���ʸ���ͼ�� V3d_View

#include <propkey.h>
#include "ExportStrings.h"   // 原版导出/载入消息字符串（UTF-8 BOM）
#include "MeshSimplify.h"   // QEM 边折叠网格简化（32837 复刻）
#include <vector>
#include <unordered_map>
#include <set>
#include <map>
#include <tuple>
#include <cfloat>

// �����ؽ����裨TopoDS/BRep��
#include <gp.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <Precision.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopAbs_Orientation.hxx>
// ���ݽ���
#include <IGESControl_Controller.hxx>
#include <IGESControl_Writer.hxx>
#include <IGESData_IGESModel.hxx>
#include <IGESData_GlobalSection.hxx>
#include <STEPControl_Writer.hxx>
// 【P4·V】STEP 预览读入（master 签名已核对：ReadFile 无进度重载、
// TransferRoots(Message_ProgressRange)、OneShape——见 XSControl_Reader.hxx）
#include <STEPControl_Reader.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
// master 无 TopTools_IndexedMapOfShape 别名头（Deprecated 除外），统计用
// 类型化集合 NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>
#include <NCollection_IndexedMap.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <StepData_StepModel.hxx>
#include <APIHeaderSection_MakeHeader.hxx>
#include <Interface_Static.hxx>
// 2026-09 Ǩ�Ʋ���
#include <MeshVS_Drawer.hxx>
#include <AIS_Shape.hxx>
#include <AIS_Triangulation.hxx>
#include <Graphic3d_MaterialAspect.hxx>
#include <Graphic3d_CLight.hxx>
#include <Select3D_SensitiveTriangulation.hxx>

#include <SelectMgr_EntityOwner.hxx>
#include <AIS_SelectionScheme.hxx>
#include <Message_ProgressIndicator.hxx>
#include <Message_ProgressScope.hxx>
#include <Message_ProgressRange.hxx>

// Cstl2stpcodeDoc

IMPLEMENT_DYNCREATE(Cstl2stpcodeDoc, CDocument)

BEGIN_MESSAGE_MAP(Cstl2stpcodeDoc, CDocument)
	// Ч���������������ԭ�� doc ��Ϣӳ�� 0x140070c70..0x140070e30��
	ON_COMMAND(ID_VIEW_RAYTRACING, &Cstl2stpcodeDoc::OnViewRaytracing)
	ON_COMMAND(ID_VIEW_SHADOW, &Cstl2stpcodeDoc::OnViewShadow)
	ON_COMMAND(ID_VIEW_REFLECTION, &Cstl2stpcodeDoc::OnViewReflection)
	ON_COMMAND(ID_VIEW_ANTIALIASING, &Cstl2stpcodeDoc::OnViewAntialiasing)
	ON_COMMAND(ID_VIEW_GRID, &Cstl2stpcodeDoc::OnViewGrid)
	ON_COMMAND(ID_EXPORT_IGES, &Cstl2stpcodeDoc::OnExportIges)
	ON_COMMAND(ID_EXPORT_STEP, &Cstl2stpcodeDoc::OnExportStep)
	ON_COMMAND(ID_MESH_SIMPLIFY, &Cstl2stpcodeDoc::OnMeshSimplify)
	ON_COMMAND(ID_MESH_SIMPLIFY_UNDO, &Cstl2stpcodeDoc::OnMeshSimplifyUndo)
	ON_UPDATE_COMMAND_UI(ID_MESH_SIMPLIFY_UNDO, &Cstl2stpcodeDoc::OnUpdateMeshSimplifyUndo)
	ON_UPDATE_COMMAND_UI(ID_VIEW_RAYTRACING, &Cstl2stpcodeDoc::OnUpdateViewEffect)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHADOW, &Cstl2stpcodeDoc::OnUpdateViewEffect)
	ON_UPDATE_COMMAND_UI(ID_VIEW_REFLECTION, &Cstl2stpcodeDoc::OnUpdateViewEffect)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ANTIALIASING, &Cstl2stpcodeDoc::OnUpdateViewEffect)
	ON_UPDATE_COMMAND_UI(ID_VIEW_GRID, &Cstl2stpcodeDoc::OnUpdateViewGrid)
END_MESSAGE_MAP()


// ==== Poly_Triangulation �� MeshVS ����Դ������ ====
// ԭ��ʹ�� XSDRAWSTLVRML_DataSource(StlMesh_Mesh)���°� OCCT ��ɾ���������࣬
// ��������Ϊ�ٷ� MeshVS_DataSource �ӿڵĵȼ�ʵ�֣��ڵ� 1..NbNodes��
// ������ 1..NbTriangles��GetGeom ���� 9 ������ X1,Y1,Z1,...����
class CStlTriangulationDataSource : public MeshVS_DataSource
{
public:
	explicit CStlTriangulationDataSource(const opencascade::handle<Poly_Triangulation>& theTriang)
		: myTriang(theTriang)
	{
		for (int i = 1; i <= myTriang->NbNodes(); ++i)
			myNodes.Add(i);
		for (int i = 1; i <= myTriang->NbTriangles(); ++i)
			myElements.Add(i);
	}

	bool GetGeom(const int ID, const bool IsElement,
	             NCollection_Array1<double>& Coords,
	             int& NbNodes, MeshVS_EntityType& Type) const override
	{
		if (IsElement)
		{
			if (ID < 1 || ID > myTriang->NbTriangles())
				return false;
			const Poly_Triangle aTri = myTriang->Triangle(ID);
			const int nBase = Coords.Lower();
			for (int k = 0; k < 3; ++k)
			{
				const gp_Pnt aPnt = myTriang->Node(aTri.Value(k + 1));
				Coords[nBase + k * 3 + 0] = aPnt.X();
				Coords[nBase + k * 3 + 1] = aPnt.Y();
				Coords[nBase + k * 3 + 2] = aPnt.Z();
			}
			NbNodes = 3;
			Type = MeshVS_ET_Face;
			return true;
		}
		if (ID < 1 || ID > myTriang->NbNodes())
			return false;
		const gp_Pnt aPnt = myTriang->Node(ID);
		const int nBase = Coords.Lower();
		Coords[nBase + 0] = aPnt.X();
		Coords[nBase + 1] = aPnt.Y();
		Coords[nBase + 2] = aPnt.Z();
		NbNodes = 1;
		Type = MeshVS_ET_Node;
		return true;
	}

	bool GetGeomType(const int, const bool, MeshVS_EntityType& Type) const override
	{
		Type = MeshVS_ET_Face;   // �ڵ�/��Ԫ������ GetGeom ��ϸ�֣��˴�����ȫĬ��
		return true;
	}

	void* GetAddr(const int, const bool) const override { return NULL; }

	bool GetNodesByElement(const int ID, NCollection_Array1<int>& NodeIDs, int& NbNodes) const override
	{
		if (ID < 1 || ID > myTriang->NbTriangles())
			return false;
		const Poly_Triangle aTri = myTriang->Triangle(ID);
		const int nBase = NodeIDs.Lower();
		for (int k = 0; k < 3; ++k)
			NodeIDs[nBase + k] = aTri.Value(k + 1);
		NbNodes = 3;
		return true;
	}

	const TColStd_PackedMapOfInteger& GetAllNodes() const override { return myNodes; }
	const TColStd_PackedMapOfInteger& GetAllElements() const override { return myElements; }

	// �淨�ߣ�������Ƭ�ɽڵ��������ԭ�� XSDRAWSTLVRML_DataSource ͬ��ʵ�֣�
	bool GetNormal(const int Id, const int, double& nx, double& ny, double& nz) const override
	{
		if (Id < 1 || Id > myTriang->NbTriangles())
			return false;
		const Poly_Triangle aTri = myTriang->Triangle(Id);
		const gp_Pnt aP1 = myTriang->Node(aTri.Value(1));
		const gp_Pnt aP2 = myTriang->Node(aTri.Value(2));
		const gp_Pnt aP3 = myTriang->Node(aTri.Value(3));
		gp_Vec aV1(aP1, aP2), aV2(aP1, aP3);
		gp_Vec aN = aV1.Crossed(aV2);
		const double aLen = aN.Magnitude();
		if (aLen <= gp::Resolution())
			return false;
		nx = aN.X() / aLen; ny = aN.Y() / aLen; nz = aN.Z() / aLen;
		return true;
	}

private:
	opencascade::handle<Poly_Triangulation> myTriang;
	TColStd_PackedMapOfInteger myNodes;
	TColStd_PackedMapOfInteger myElements;
};


// Cstl2stpcodeDoc ����/����

// 前置声明：RebuildLog 定义在文件下方（仅环境变量 STL2STP_DEBUG_LOG 存在时
// 写盘，取证调试不受影响）——载入早期阶段（文档构造/打开）也要打点
static void RebuildLog(const char* fmt, ...);

Cstl2stpcodeDoc::Cstl2stpcodeDoc()
{
	RebuildLog("P4P: doc ctor enter");
	// ����ԭ�� CVoxelModDoc ���캯����sub_14000C140���ĳ�ʼ�����У�
	m_bLoaded = FALSE;
	m_bStepPreview = FALSE;   // 【P4·V】STEP 纯预览态（DeleteContents 亦复位）
	m_ullFileSize = 0;
	// 【方案 A】原操纵器（m_hManipulator 创建 / m_bManipActive 标志）已移除——WYSIWYG

	// 新 UI：形态属性（颜色/材质）初值——纯黄 + DEFAULT 材质保持取证基线
	m_clrModel = Quantity_Color(1.0, 1.0, 0.0, Quantity_TOC_RGB);
	m_nMaterialIndex = 0;
	m_bExporting = FALSE;
	m_bExported = FALSE;   // P4·C：步骤条"已导出"态（DeleteContents 亦复位）

	// Ч��״̬��ֵ��ԭ�棺*(DWORD*)(doc+624)=256 �� {0,1,0,0}��*(BYTE*)(doc+628)=1��
	m_bRaytracing = 0;      // ���߸��ٹ�
	m_bShadow = 1;          // ��Ӱ��
	m_bReflection = 0;      // �����
	m_bAntialiasing = 0;    // ��������
	m_bGrid = 1;            // �ο�����

	// ȡӦ�ó����ȫ��ͼ������
	Cstl2stpcodeApp* pApp = (Cstl2stpcodeApp*)AfxGetApp();
	m_hGraphicDriver = pApp->m_hGraphicDriver;

	// ���� V3d_Viewer��ԭ�����������"��ά��ͼ"���ӿ� 1000.0��
	// V3d_XposYnegZpos(19)��GRAY30 ������ZBUFFER��GOURAUD��ASAP��˫����+������
	// �°湹�캯��ֻ�� driver�����������Ĭ�����ýӿڡ���
	if (!m_hGraphicDriver.IsNull())
	{
		m_hViewer = new V3d_Viewer(m_hGraphicDriver);
		m_hViewer->SetDefaultViewSize(1000.0);
		m_hViewer->SetDefaultViewProj(V3d_XposYnegZpos);
		Quantity_Color aBgColor(Quantity_NOC_GRAY30);
		m_hViewer->SetDefaultBackgroundColor(aBgColor);
		m_hViewer->SetDefaultVisualization(V3d_ZBUFFER);
		m_hViewer->SetDefaultShadingModel(Graphic3d_TOSM_VERTEX);   // ԭ�� GOURAUD

		m_hViewer->SetDefaultLights();
		m_hViewer->SetLightOn();

		// 【新 UI P4·L】模型光照 sRGB 补偿：master 输出端把点亮结果做 sRGB
		// 编码（背景/网格已按各自链路补偿，见上下注释；模型侧此前未补偿）。
		// 逐像素取证（补充三十二）：无补偿时 43.2% 模型像素的光照斜坡被输出
		// 端 255 通道硬钳成纯黄平顶（细节全失），原版零钳制、斜坡自然收敛到
		// L=161；光照斜坡随强度线性缩放（0.70 档钳制区收缩为前者的严格子集）。
		// 按原版斜坡顶端反推（k≈0.65 处斜坡顶=158≈原版 161），阶梯实测定档
		// 0.50：钳制占比 1.0→43.1% / 0.7→23.3% / 0.6→16.8% / 0.5→9.3%（原版
		// 零钳制；暗部环境光底不随强度缩放，真实全分布中位低于掩膜中位）。
		// 对默认两灯（headlight 方向光 + ambient）按注册表 ModelLightScale
		// （百分比整数，默认 50）缩放强度。
		{
			const double dLightScale = AfxGetApp()->GetProfileInt(
				_T("Workspace"), _T("ModelLightScale"), 50) / 100.0;
			if (dLightScale > 0.0 && dLightScale < 1.0)
			{
				for (NCollection_List<opencascade::handle<Graphic3d_CLight>>::Iterator
					aLightIt(m_hViewer->ActiveLights()); aLightIt.More(); aLightIt.Next())
					aLightIt.Value()->SetIntensity(
						(float)(aLightIt.Value()->Intensity() * dLightScale));
			}
		}

		// ���䱳����ԭ��� viewer ���ý��䱳������
		// SetColors(ǳ��(0.706,0.784,1.0), ��(0.706,0.706,0.706), DIAG1)��
		// 渐变背景取证更新（sub_14000C3E5，2026-09-09）：SetColors(蓝(0.706,0.784,1.0),
		// 灰(0.706,0.706,0.706), Aspect_GFM_VER=2)——垂直渐变，蓝色在顶部（实测原版像素）。
		// master 渲染管线在输出端做 sRGB 编码（实测：传 0.706 显示 218=encode(0.706)），
		// 原版 7.0 固定管线无编码（直接 180）。为让最终像素一致，此处传线性值
		// = sRGBDecode(目标显示值)：顶 (180,200,255)→(0.4565,0.5776,1.0)，
		// 底 (180,180,180)→0.4565。
		Quantity_Color aGrad1(0.45647, 0.57757, 1.0, Quantity_TOC_RGB);
		Quantity_Color aGrad2(0.45647, 0.45647, 0.45647, Quantity_TOC_RGB);
		m_hViewer->SetDefaultBgGradientColors(aGrad1, aGrad2, Aspect_GFM_VER);

		// ����ο�����ԭ�� ActivateGrid(Rectangular, Points)��
		// ����ֵԭ��(0,0)������(10,10)����Χ 1000 ��ԭ������ͼ���������ã�
		// 参考网格取证更新（sub_14000C3F7，2026-09-09）：原版 ActivateGrid 实参
		// (Rectangular=0, Lines=0)——线模式而非点；步距/尺寸走 OCCT 默认（步距10，
		// 尺寸=默认视面 1000 的一半）；旋转角 0。网格颜色原版未设 = 默认
		// GRAY50(0.5)/GRAY70(0.7)（原版 7.0 固定管线直接显示 127/178）。
		// master 网格链路把传入值按线性值原样输出（实测默认 0.5 显示为 54），
		// 故按目标显示值直接传：主线 126/255=0.494、每10条 178/255=0.7。
		m_hViewer->SetRectangularGridValues(0.0, 0.0, 10.0, 10.0, 0.0);
		m_hViewer->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);
		{
			opencascade::handle<V3d_RectangularGrid> hGrid =
				opencascade::handle<V3d_RectangularGrid>::DownCast(m_hViewer->Grid());
			if (!hGrid.IsNull())
				hGrid->SetColors(Quantity_Color(0.494, 0.494, 0.494, Quantity_TOC_RGB),
				                 Quantity_Color(0.7, 0.7, 0.7, Quantity_TOC_RGB));
		}

		// �������������ģ�ȫ����ʾģʽ��Ϊ 1
		m_hAISCtx = new AIS_InteractiveContext(m_hViewer);
		if (!m_hAISCtx.IsNull())
			m_hAISCtx->SetDisplayMode(1, true);
	}

	// ���� MeshVS_Mesh ��ʾ����ԭ����� false��
	m_hMeshVS = new MeshVS_Mesh(false);

	// ������������ԭ�����ĵ����캯���д�����
	// 【方案 A（WYSIWYG）】原版此处创建 AIS_Manipulator 的语句已移除——
	// 视口姿态编辑不被导出尊重（导出用原始网格），见 AGENT.md 验收补充十
	RebuildLog("P4P: doc ctor exit, viewer=%d ctx=%d",
		!m_hViewer.IsNull() ? 1 : 0, !m_hAISCtx.IsNull() ? 1 : 0);
}

Cstl2stpcodeDoc::~Cstl2stpcodeDoc()
{
}

BOOL Cstl2stpcodeDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// 新 UI 有意偏差：原版怪癖“新建空文档不清空上次载入的统计”（AGENT.md 取证）——
	// 残留上一个模型的属性/状态栏数据会误导用户。新建即清空属性窗格统计与状态栏。
	CMainFrame* pMain = (CMainFrame*)AfxGetMainWnd();
	if (pMain != NULL)
	{
		pMain->ClearModelInfo();
		pMain->RefreshFileList();   // 新 UI：“文件”窗格登记新建文档
	}

	return TRUE;
}

// Cstl2stpcodeDoc ���ĵ�

// 【P4·V】STEP 预览文件判定（.step/.stp，大小写不敏感）——OnOpenDocument 按
// 扩展名分流：STEP 走纯预览读入链，其余（含 .stl）走原 STL 载入链不动
static bool IsStepFile(LPCTSTR lpszPath)
{
	if (lpszPath == NULL)
		return false;
	CString strPath(lpszPath);
	strPath.MakeLower();
	return strPath.Right(5) == _T(".step") || strPath.Right(4) == _T(".stp");
}

BOOL Cstl2stpcodeDoc::OnOpenDocument(LPCTSTR lpszPath)
{
	// ����Ĭ�����л���ֱ�Ӽ��� STL ����ʾ����ԭ����Ϊһ�£�
	DeleteContents();
	// 【新 UI P2·SDI 化阶段 1.2】SDI 文档复用：载入成功后对已存在视图显式重取景
	//（MDI 时代 MRU/再次打开新建子框架，OnInitialUpdate 自动取景；SDI 复用同一
	// 视图，无人再调 FitViewToModel——不补此步则换载第二个文件时模型停在旧相机）
	RebuildLog("P4P: OnOpenDocument enter");
	// 【P4·V】STEP 分流：.step/.stp 走纯预览读入链（不进网格重建/简化/导出语义）
	const bool bStep = IsStepFile(lpszPath);
	BOOL bOK = bStep ? LoadStepPreview(lpszPath) : LoadStlAndDisplay(lpszPath);
	RebuildLog("P4V: load dispatch ret=%d step=%d", (int)bOK, (int)bStep);
	if (bOK)
		RefitActiveView();
	RebuildLog("P4P: OnOpenDocument ret=%d", (int)bOK);
	return bOK;
}

// 【新 UI P2·SDI 化阶段 1.2】文档内容复位（SDI 复用语义，设计定稿 4.2.2(e)2）：
// MDI 下“新建”= 新 Cstl2stpcodeDoc 对象（构造函数复位）；SDI 复用同一文档对象，
// 构造函数不再执行——不在此显式复位则“新建”后旧网格残留、导出守卫失效。
// CDocument::OnNewDocument 内部先调 DeleteContents，OnOpenDocument 顶部亦显式
// 调用——新建/打开两路全覆盖。
void Cstl2stpcodeDoc::DeleteContents()
{
	m_bLoaded = FALSE;
	m_bStepPreview = FALSE;   // 【P4·V】STEP 预览态随文档复位（新建/打开两路全覆盖）
	m_ullFileSize = 0;
	m_hTriang.Nullify();
	m_hTriangOrig.Nullify();
	m_hTriangDisplay.Nullify();
	m_bExported = FALSE;   // P4·C：步骤条"已导出"随文档复位（新建/打开两路共用）
	m_hModelPrs.Nullify();
	m_rebuiltShape.Nullify();
	// 形态属性回取证默认（对齐 MDI 新建=构造函数初值：纯黄 + DEFAULT 材质）
	m_clrModel = Quantity_Color(1.0, 1.0, 0.0, Quantity_TOC_RGB);
	m_nMaterialIndex = 0;
	if (!m_hAISCtx.IsNull())
		m_hAISCtx->RemoveAll(true);
	CDocument::DeleteContents();
}

// ���� STL ������ AIS ���� ���� ����ԭ�� sub_14000C830 ����������
// 【P4·K（用户反馈：悟空.stl 载入失败弹“没有可用的三角面片”）】实际根因是
// 文件名编码而非截断：OCCT master 全链路把 char* 路径按 UTF-8 解码
// （OSD_OpenStream 经 TCollection_ExtendedString(theName, true) 转 UTF-16 宽
// 字符打开，见 OSD_OpenFile.hxx），CT2A 产 CP_ACP 的 GBK 字节，非 ASCII 文件名
// 被误读成乱码宽字符路径 → 打开失败 → RWStl 返回空 → 误报“没有可用的三角
// 面片”。该文件格式完好：头声明 1,266,126 面（0x1351CE）恰等于文件实有容量
// (63,306,384-84)/50（十六进制复核 2026-09-15；此前“声明 1,271,502 面、截断”
// 的取证结论有误）；同数据 ASCII 名 wukong_copy.stl 载入正常（t+80s 模型入帧）、
// 原中文名零 CPU 瞬间失败——双实验互证。修复 = 传给 OCCT 的路径一律转 UTF-8。
static CStringA ToUtf8(LPCTSTR lpszText)
{
	CStringA strUtf8;
	if (lpszText == NULL || *lpszText == 0)
		return strUtf8;
	const int cb = WideCharToMultiByte(CP_UTF8, 0, lpszText, -1, NULL, 0, NULL, NULL);
	if (cb <= 1)
		return strUtf8;
	LPSTR pszBuf = strUtf8.GetBufferSetLength(cb - 1);
	WideCharToMultiByte(CP_UTF8, 0, lpszText, -1, pszBuf, cb, NULL, NULL);
	strUtf8.ReleaseBufferSetLength(cb - 1);
	return strUtf8;
}

// 截断探测：读二进制 STL 80 字节头的面数声明，与 (文件长度-84)/50 实有容量
// 比较，容量 < 声明即文件不完整。ASCII 文件域（"solid" 开头）无二进制面数
// 语义，返回 false。
static bool ProbeStlCapacity(LPCTSTR lpszPath, int& nDeclared, ULONGLONG& ullCapacity)
{
	nDeclared = 0;
	ullCapacity = 0;
	try
	{
		CFile aFile(lpszPath, CFile::modeRead | CFile::shareDenyNone);
		const ULONGLONG ullSize = aFile.GetLength();
		if (ullSize < 84)
			return false;
		char aHeader[84];
		if (aFile.Read(aHeader, 84) != 84)
			return false;
		if (memcmp(aHeader, "solid", 5) == 0)
			return false;
		memcpy(&nDeclared, aHeader + 80, 4);
		ullCapacity = (ullSize - 84) / 50;
		return true;
	}
	catch (CFileException*)
	{
		return false;
	}
}

// 二进制 STL 截断容错兜底读取。RWStl master 对截断文件本就按实有数据部分
// 载入（ReadFile 有意忽略读取失败、返回已读面片，见 RWStl.cxx:156-158），
// 仅当截断点落在首个读取块（80 面片 4000 字节）内时收集不到任何面片、
// 返回空——本兜底为该极端场景按实有容量载入可用面片。非截断或不可解析
// 仍返回 null 交回原失败路径（细分提示不变）。顶点按 float32 位型精确去重
// （STL 存 float32，与 RWStl 同语义——共享顶点是 RebuildShapeFromMesh 缝合
// 质量的前提）。
static opencascade::handle<Poly_Triangulation> ReadStlTolerant(LPCTSTR lpszPath,
	int& nDeclared, int& nLoaded)
{
	nDeclared = 0;
	nLoaded = 0;
	try
	{
		CFile aFile(lpszPath, CFile::modeRead | CFile::shareDenyNone);
		const ULONGLONG ullSize = aFile.GetLength();
		if (ullSize < 84)
			return NULL;
		char aHeader[80];
		if (aFile.Read(aHeader, 80) != 80)
			return NULL;
		bool bAsciiMagic = memcmp(aHeader, "solid", 5) == 0;
		for (int i = 0; i < 74 && !bAsciiMagic; ++i)
			bAsciiMagic = bAsciiMagic || memcmp(aHeader + i, "facet", 5) == 0;
		if (bAsciiMagic)
			return NULL;   // ASCII 文件域（RWStl 负责），不接管
		unsigned int nCount = 0;
		if (aFile.Read(&nCount, 4) != 4)
			return NULL;
		nDeclared = (int)nCount;
		const ULONGLONG ullAvail = (ullSize - 84) / 50;
		if (ullAvail >= (ULONGLONG)nCount)
			return NULL;   // 长度足够——非截断问题，交回原失败路径
		const int nUse = (int)ullAvail;
		std::vector<char> aBuf((size_t)nUse * 50);
		if (aFile.Read(aBuf.data(), (UINT)aBuf.size()) != aBuf.size())
			return NULL;

		struct FSHash
		{
			size_t operator()(const std::tuple<float, float, float>& k) const
			{
				return std::hash<float>()(std::get<0>(k))
					^ (std::hash<float>()(std::get<1>(k)) << 1)
					^ (std::hash<float>()(std::get<2>(k)) << 2);
			}
		};
		std::unordered_map<std::tuple<float, float, float>, int, FSHash> aWeld;
		std::vector<gp_Pnt> aNodes;
		std::vector<Poly_Triangle> aTris;
		aTris.reserve(nUse);
		for (int i = 0; i < nUse; ++i)
		{
			const float* pV = (const float*)(aBuf.data() + (size_t)i * 50 + 12);  // 跳过法线
			int anIdx[3];
			for (int k = 0; k < 3; ++k)
			{
				const float x = pV[k * 3], y = pV[k * 3 + 1], z = pV[k * 3 + 2];
				auto key = std::make_tuple(x, y, z);
				auto it = aWeld.find(key);
				if (it == aWeld.end())
				{
					aNodes.push_back(gp_Pnt(x, y, z));
					it = aWeld.emplace(key, (int)aNodes.size()).first;
				}
				anIdx[k] = it->second;
			}
			if (anIdx[0] != anIdx[1] && anIdx[1] != anIdx[2] && anIdx[0] != anIdx[2])
				aTris.push_back(Poly_Triangle(anIdx[0], anIdx[1], anIdx[2]));
		}
		nLoaded = (int)aTris.size();
		if (aTris.empty())
			return NULL;
		opencascade::handle<Poly_Triangulation> hT =
			new Poly_Triangulation((int)aNodes.size(), (int)aTris.size(), false, false);
		for (size_t k = 0; k < aNodes.size(); ++k)
			hT->SetNode((int)(k + 1), aNodes[k]);
		for (size_t k = 0; k < aTris.size(); ++k)
			hT->SetTriangle((int)(k + 1), aTris[k]);
		return hT;
	}
	catch (CFileException*)
	{
		return NULL;
	}
	catch (...)
	{
		return NULL;
	}
}

// 新 UI 增补：导出/载入共用进度对话框（IDD_EXPORT_PROGRESS）——替代冻结式
// 等待。无模式对话框：流程分阶段调用 SetStage()（更新进度条/阶段文字并泵
// 消息），用户可随时"取消"（OnCancel 仅置标志，由调用流程在阶段边界退出）。
// 【新 UI P4·P】载入路径共用本资源：SetTitle() 换标题；SetRemap() 把子流程
// 的内部百分比（如重建 2..88）映射到本对话框的区间刻度；OnInitDialog 居中。
class CExportProgressDlg : public CDialog
{
public:
	enum { IDD = IDD_EXPORT_PROGRESS };
	CExportProgressDlg(CWnd* pParent = NULL) : CDialog(IDD, pParent) {}
	BOOL IsCanceled() const { return m_bCancel; }
	// 【P4·P】可选标题（默认保持资源标题"正在转换"）
	void SetTitle(LPCTSTR lpsz) { m_strTitle = lpsz; }
	// 【P4·P】窗口百分比重映射：nSpan>0 时 nPos = nBase + nPercent*nSpan/100，
	// nSpan<=0 时 nPos = nPercent 原样（导出路径默认恒等映射，行为不变）
	void SetRemap(int nBase, int nSpan) { m_nBase = nBase; m_nSpan = nSpan; }
	void SetStage(int nPercent, LPCTSTR lpszStage)
	{
		if (GetSafeHwnd() == NULL)
			return;
		const int nPos = m_nSpan > 0 ? m_nBase + nPercent * m_nSpan / 100 : nPercent;
		if (m_wndBar.GetSafeHwnd() != NULL)
			m_wndBar.SetPos(nPos);
		if (lpszStage != NULL)
			SetDlgItemText(IDC_EXPORT_STAGE, lpszStage);
		Pump();
	}
	// 非阻塞消息泵：处理对话框重绘/按钮点击等挂起消息后立即返回
	void Pump()
	{
		MSG msg;
		while (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!AfxGetThread()->PumpMessage())
				break;
		}
	}
protected:
	virtual void DoDataExchange(CDataExchange* pDX)
	{
		CDialog::DoDataExchange(pDX);
		DDX_Control(pDX, IDC_EXPORT_PROGRESS_BAR, m_wndBar);
	}
	virtual BOOL OnInitDialog()
	{
		CDialog::OnInitDialog();
		// 【P4·P 取证】对话框窗口拓扑（隐藏/父链谜团定位，环境变量门控）
		{
			CWnd* pDlgParent = GetParent();
			RebuildLog("P4P: dlg OnInitDialog hwnd=%p vis=%d parent=%p style=%08x",
				(void*)GetSafeHwnd(), (int)IsWindowVisible(), (void*)(pDlgParent != NULL ? pDlgParent->GetSafeHwnd() : NULL),
				GetSafeHwnd() != NULL ? (unsigned)GetStyle() : 0u);
		}
		m_wndBar.SetRange(0, 100);
		if (!m_strTitle.IsEmpty())
			SetWindowText(m_strTitle);
		CenterWindow();   // 【P4·P】居中于父窗口（原未定位）
		return TRUE;
	}
	virtual void OnCancel() { m_bCancel = TRUE; }   // 无模式：仅置取消标志，不销毁窗口
	CProgressCtrl m_wndBar;
	CString m_strTitle;
	int m_nBase = 0, m_nSpan = 100;   // 【P4·P】重映射窗口（默认恒等）
	BOOL m_bCancel = FALSE;
};

// 【新 UI P4·P】载入进度指示器：把 OCCT 进度系统的 Show/UserBreak 回调接到
// 载入进度对话框——RWStl 二进制读取逐面片推进（内部 ~1% 节流调 Show），
// 每次回调更新进度条并泵消息，载入期间界面不再冻结；取消即 UserBreak →
// RWStl 读循环按已读数据返回（ReadFile 有意忽略失败标志，RWStl.cxx:156-158），
// 由调用侧以 IsCanceled() 判定后走干净取消路径（部分数据不采纳）
class CLoadProgressIndicator : public Message_ProgressIndicator
{
public:
	CLoadProgressIndicator(CExportProgressDlg* pDlg, int nBase, int nSpan)
		: m_pDlg(pDlg), m_nBase(nBase), m_nSpan(nSpan) {}
protected:
	bool UserBreak() override
	{
		return m_pDlg != NULL && m_pDlg->IsCanceled();
	}
	void Show(const Message_ProgressScope& /*theScope*/, const bool /*isForce*/) override
	{
		if (m_pDlg == NULL)
			return;
		const double dPos = GetPosition();   // 0..1（指示器节流后的总进度）
		m_pDlg->SetStage(m_nBase + (int)(dPos * m_nSpan), NULL);
	}
private:
	CExportProgressDlg* m_pDlg;
	int m_nBase, m_nSpan;
};

// 载入 STL 并显示 AIS（对应原版 sub_14000C830 载入管线）
BOOL Cstl2stpcodeDoc::LoadStlAndDisplay(LPCTSTR lpszPath)
{
	if (m_hAISCtx.IsNull() || m_hMeshVS.IsNull())
		return FALSE;

	if (lpszPath == NULL || *lpszPath == 0)
		return FALSE;

	RebuildLog("P4P: load enter");

	// 1) ��ճ���
	m_hAISCtx->RemoveAll(true);

	// 2) ��¼�ļ�·�����С
	SetPathName(lpszPath, TRUE);
	{
		WIN32_FILE_ATTRIBUTE_DATA fad;
		if (GetFileAttributesEx(lpszPath, GetFileExInfoStandard, &fad) != 0)
		{
			ULARGE_INTEGER ul;
			ul.HighPart = fad.nFileSizeHigh;
			ul.LowPart = fad.nFileSizeLow;
			m_ullFileSize = ul.QuadPart;
		}
	}

	// 【新 UI P4·P】载入进度对话框：读取/聚类/重建/渲染全程进度反馈
	//（用户指定"设计转圈等待或进度条"——原版载入为冻结式沙漏）；主框架
	// 失能防泵消息重入（同导出路径），取消在阶段边界干净退出（部分数据
	// 不采纳），各失败/取消出口统一经 closeLoadProgress 收尾
	CExportProgressDlg aLoad;
	CWnd* pShellFrame = AfxGetMainWnd();

	// 3) 读取 STL：RWStl::ReadFile。
	// 【新 UI 增补】载入失败按原因细分提示（文件不存在 / 过小非二进制 STL /
	// 解析器异常 / 无面片），并复位载入状态、清空属性与状态栏——
	// 原版仅一句“读取STL模型文件失败！”，见 ui-redesign\UI重设计方案.md B5。
	{
		CString strPath(lpszPath);
		auto closeLoadProgress = [&]()
		{
			// 对称收尾：仅在曾创建对话框（即曾失能主框架）时恢复/销毁——
			// 启动期静默载入不发任何窗口消息（见 bShellLoading 取证注释）
			if (aLoad.GetSafeHwnd() != NULL)
			{
				if (pShellFrame != NULL)
					pShellFrame->EnableWindow(TRUE);
				aLoad.DestroyWindow();
			}
		};
		auto failLoad = [&](const CString& strMsg) -> BOOL
		{
			m_bLoaded = FALSE;
			m_hTriang.Nullify();
			CMainFrame* pMainFrm = (CMainFrame*)AfxGetMainWnd();
			if (pMainFrm != NULL)
				pMainFrm->ClearModelInfo();
			closeLoadProgress();
			AfxMessageBox(strMsg, MB_ICONERROR);
			return FALSE;
		};
		// 【P4·P】取消路径：与失败同位复位，但不弹错误框，写输出窗格提示
		auto canceledLoad = [&]() -> BOOL
		{
			m_bLoaded = FALSE;
			m_hTriang.Nullify();
			CMainFrame* pMainFrm = (CMainFrame*)AfxGetMainWnd();
			if (pMainFrm != NULL)
				pMainFrm->ClearModelInfo();
			closeLoadProgress();
			LogToOutputPane(newui::load::kCanceled);
			return FALSE;
		};
		if (GetFileAttributes(strPath) == INVALID_FILE_ATTRIBUTES)
			return failLoad(CString(newui::kErrFileNotFound) + strPath);
		if (m_ullFileSize < 84)   // 二进制 STL 至少 80 字节头 + 4 字节面片计数
			return failLoad(CString(newui::kErrFileTooSmall) + strPath);

		// 【P4·P】读取阶段上线：标题/主框架失能/RWStl 进度指示器——
		// OCCT 进度系统逐面片推进（内部 ~1% 节流），每次 Show 回调更新
		// 进度条并泵消息，载入期间界面不再冻结。
		// 【P4·P 崩溃取证定案（2026-09-15 阶段日志）】启动期载入（InitInstance
		// 的 ProcessShellCommand 栈内：命令行/文件关联）不能建对话框、不能泵
		// 消息。原判据“主框架不可见”必然误判——本应用主框架先于文档创建并已
		// 显示（日志实证 frame 非空、style=150FC000 含 WS_VISIBLE），首个
		// SetStage 的泵消息把挂起的初始化消息派发给 OnInitialUpdate 尚未运行
		// 的半初始化视图，确定性崩于建框与读取之间（mfc140u.dll 0x24298c AV，
		// 多轮同偏移）。判据改为“视图管线就绪”：首视图 V3d_View 已挂 = 消息
		// 循环已运行的交互态（菜单/MRU/拖放），完整启用进度 UI；启动期保持
		// 静默载入（P4·P 前行为，原版同为冻结式无反馈）。SetStage/取消检查
		// 在对话框未创建时为安全空操作
		BOOL bShellLoading = TRUE;
		{
			POSITION pos = GetFirstViewPosition();
			Cstl2stpcodeView* pFirstView = (pos != NULL)
				? DYNAMIC_DOWNCAST(Cstl2stpcodeView, GetNextView(pos)) : NULL;
			bShellLoading = (pFirstView == NULL) ||
				pFirstView->IsViewReady() == FALSE;
		}
		RebuildLog("P4P: shellguard frame=%p style=%08x bShell=%d", (void*)pShellFrame,
			(pShellFrame != NULL && pShellFrame->GetSafeHwnd() != NULL)
				? (unsigned)pShellFrame->GetStyle() : 0u, (int)bShellLoading);
		opencascade::handle<CLoadProgressIndicator> hLoadInd;
		if (!bShellLoading)
		{
			aLoad.SetTitle(newui::load::kTitle);
			const BOOL bDlgCreated = aLoad.Create(CExportProgressDlg::IDD, AfxGetMainWnd());
			RebuildLog("P4P: dialog create=%d", (int)bDlgCreated);
			// 【P4·P 修复】模板无 WS_VISIBLE，必须显式显示（与导出路径
			// SW_SHOWNOACTIVATE 一致，不抢焦点）——漏此步对话框全程不可见
			//（v2/v3 验证"对话框未出现"之谜即此，屏幕与窗口枚举均无）
			if (bDlgCreated)
				aLoad.ShowWindow(SW_SHOWNOACTIVATE);
			if (pShellFrame != NULL)
				pShellFrame->EnableWindow(FALSE);
			aLoad.SetStage(3, newui::load::kRead);
			hLoadInd = new CLoadProgressIndicator(&aLoad, 5, 50);
		}
		const CStringA strPathUtf8 = ToUtf8(lpszPath);   // 【P4·K】OCCT 期望 UTF-8 路径
		RebuildLog("P4P: read begin");
		try
		{
			m_hTriang = RWStl::ReadFile((const char*)strPathUtf8,
				hLoadInd.IsNull() ? Message_ProgressRange() : hLoadInd->Start());
		}
		catch (const Standard_Failure& e)
		{
			CString strReason(e.what() != NULL
				? CA2T(e.what()) : _T("(无异常消息)"));
			return failLoad(CString(newui::kErrParseFail) + strReason
				+ _T("\n文件：") + strPath);
		}
		catch (...)
		{
			return failLoad(CString(newui::kErrParseFail) + _T("(未知异常)")
				+ _T("\n文件：") + strPath);
		}
		RebuildLog("P4P: read done, tri=%d",
			m_hTriang.IsNull() ? -1 : m_hTriang->NbTriangles());
		// 【P4·P】取消即 UserBreak——RWStl 按已读数据返回（部分数据不采纳），
		// 与失败同位复位干净退出
		if (aLoad.IsCanceled())
			return canceledLoad();

		if (m_hTriang.IsNull() || m_hTriang->NbTriangles() == 0)
		{
			// 【P4·K】RWStl 完全拒载的极端场景兜底（截断落在首个读取块内、
			// 收集不到任何面片）：按实有容量容错载入；截断警告由下方统一探测写日志
			int nDeclared = 0, nLoaded = 0;
			opencascade::handle<Poly_Triangulation> hT =
				ReadStlTolerant(strPath, nDeclared, nLoaded);
			if (!hT.IsNull())
				m_hTriang = hT;
		}
		if (m_hTriang.IsNull() || m_hTriang->NbTriangles() == 0)
			return failLoad(CString(newui::kErrEmptyMesh) + strPath);

		// 【P4·K】截断警告：RWStl master 对截断文件按实有数据静默部分载入，
		// 探测到头声明面数超出文件容量时写一条“文件不完整”提示到输出窗格
			{
				int nDeclared = 0;
				ULONGLONG ullCapacity = 0;
				if (ProbeStlCapacity(strPath, nDeclared, ullCapacity)
					&& nDeclared > 0 && ullCapacity < (ULONGLONG)nDeclared)
				{
					CString strNote;
					strNote.Format(newui::kTruncatedStl, nDeclared, (int)ullCapacity);
					LogToOutputPane(strNote);
				}
			}

	// 4)+5)+6) 显示：重建体走 AIS_Shape（环境取证 2026-09-08）。
	// 本机显示适配器为 Microsoft 基本显示适配器（另有 Citrix 虚拟适配器），
	// OpenGL 只能取得 GDI Generic 1.1.0 软回退上下文；OCCT master 的固定管线
	// 回退画不出 MeshVS 的图元阵列（Compute 正常完成但无像素输出，平滑/平面
	// 着色皆然），而普通 AIS_Shape 实测可正常渲染。原版基于 OCCT 7.0 的完整
	// 固定管线，同一环境用 MeshVS 显示正常；master 无等价回退，故模型显示改走
	// AIS_Shape（重建体），颜色/材质按 DisplayModelShape 中的取证注释对齐原版。
	// （在有真实 GPU 驱动的机器上可改回 MeshVS 显示路径。）
	// 【P4·P】聚类/重建/渲染经同一进度对话框反馈；重建阶段取消（Step
	// 阶段边界退出）与读取取消同位复位干净退出
		RebuildLog("P4P: display begin");
		DisplayModelShape(&aLoad);
		RebuildLog("P4P: display end");
		if (aLoad.IsCanceled())
			return canceledLoad();
	}   // 读取+显示块（strPath/aLoad 阶段作用域）
	RebuildLog("P4P: display block closed");

	m_bLoaded = TRUE;
	RebuildLog("P4P: m_bLoaded set");

	// 载入成功日志（sub_14000CC55 取证 2026-09-09）：字符串 RVA 0x140070E90
	// = "成功加载STL模型。"（GBK）与文件路径拼接后经 LB_ADDSTRING(0x180) +
	// WM_VSCROLL(SB_BOTTOM) 写入“生成”页列表框——AddBuildLine 即此序列。
	{
		CMainFrame* pMain = (CMainFrame*)AfxGetMainWnd();
		if (pMain != NULL)
		{
			pMain->LogMessage(CString(_T("成功加载STL模型。")) + CString(lpszPath));
			// 新 UI 增补：状态栏“模型”窗格显示文件名（取路径末段）
			CString strPath(lpszPath);
			pMain->SetStatusModelInfo(strPath.Mid(strPath.ReverseFind(_T('\\')) + 1));
		}
	}
	RebuildLog("P4P: log+status done");

	// 原版行为：载入（处理）后填充属性窗格的模型统计
	UpdateModelStatsFromMesh();

	// 新 UI：刷新“文件”窗格列表（新载入文件的名称与面数）
	CMainFrame* pMainList = (CMainFrame*)AfxGetMainWnd();
	if (pMainList != NULL)
	{
		pMainList->RefreshFileList();
		// 新 UI P2·2.1：任务窗格刷新（文件名/大小/简化状态/预计面数基数/大模型提示）
		pMainList->UpdateTaskPane(this);
	}
	RebuildLog("P4P: panes refreshed, load complete");

	// 【P4·P】收尾：进度条到 100、主框架恢复、对话框销毁（对称防护：
	// 启动期静默载入对话框未创建，全部跳过——SetStage 本身即空操作）
	aLoad.SetStage(100, NULL);
	if (aLoad.GetSafeHwnd() != NULL)
	{
		if (pShellFrame != NULL)
			pShellFrame->EnableWindow(TRUE);
		aLoad.DestroyWindow();
	}
	return TRUE;
}

// 新 UI 增补："材料"属性选项 → OCCT 材质枚举（索引 0="无"保持 DEFAULT，取证基线）；
// 【P4·V】定义前移至首次引用（STEP 预览载入）之前
static const Graphic3d_NameOfMaterial s_aMaterialEnums[4] =
{ Graphic3d_NOM_DEFAULT, Graphic3d_NOM_BRASS, Graphic3d_NOM_STEEL, Graphic3d_NOM_SILVER };

// 【P4·V】STEP 纯预览载入（用户指定"把 STEP 预览做进应用里"；原版无 STEP
// 读入能力，登记有意偏差）。管线：STEPControl_Reader::ReadFile（本 master 无
// 进度重载，静态文案）→ TransferRoots（Message_ProgressRange 进度接进度指示
// 器）→ OneShape → AIS_Shape 直显（B-Rep 实体保留自动重三角化——面无预挂
// 网格，须由显示管线按需细剖）。外观沿用取证基线（纯黄 + DEFAULT 材质）。
// 纯预览语义：简化/导出按网格语义拒绝（OnMeshSimplify/SimplifyByRatio/
// OnExportStep/OnExportIges 的 stepprev 守卫）；进度/取消/启动期静默契约与
// STL 载入（P4·P）完全同型。
BOOL Cstl2stpcodeDoc::LoadStepPreview(LPCTSTR lpszPath)
{
	if (m_hAISCtx.IsNull())
		return FALSE;
	if (lpszPath == NULL || *lpszPath == 0)
		return FALSE;

	RebuildLog("P4V: step load enter");
	m_hAISCtx->RemoveAll(true);
	SetPathName(lpszPath, TRUE);
	{
		WIN32_FILE_ATTRIBUTE_DATA fad;
		if (GetFileAttributesEx(lpszPath, GetFileExInfoStandard, &fad) != 0)
		{
			ULARGE_INTEGER ul;
			ul.HighPart = fad.nFileSizeHigh;
			ul.LowPart = fad.nFileSizeLow;
			m_ullFileSize = ul.QuadPart;
		}
	}

	// 进度对话框与 STL 载入同契约（P4·P）：启动期（视图管线未就绪）静默载入，
	// 不建对话框/不泵消息；交互态全程反馈、可取消
	CExportProgressDlg aLoad;
	CWnd* pShellFrame = AfxGetMainWnd();
	BOOL bShellLoading = TRUE;
	{
		POSITION pos = GetFirstViewPosition();
		Cstl2stpcodeView* pFirstView = (pos != NULL)
			? DYNAMIC_DOWNCAST(Cstl2stpcodeView, GetNextView(pos)) : NULL;
		bShellLoading = (pFirstView == NULL) ||
			pFirstView->IsViewReady() == FALSE;
	}
	RebuildLog("P4V: shellguard bShell=%d", (int)bShellLoading);

	CString strPath(lpszPath);
	auto closeLoadProgress = [&]()
	{
		if (aLoad.GetSafeHwnd() != NULL)
		{
			if (pShellFrame != NULL)
				pShellFrame->EnableWindow(TRUE);
			aLoad.DestroyWindow();
		}
	};
	auto failLoad = [&](const CString& strMsg) -> BOOL
	{
		m_bLoaded = FALSE;
		m_bStepPreview = FALSE;
		m_rebuiltShape.Nullify();
		CMainFrame* pMainFrm = (CMainFrame*)AfxGetMainWnd();
		if (pMainFrm != NULL)
			pMainFrm->ClearModelInfo();
		closeLoadProgress();
		AfxMessageBox(strMsg, MB_ICONERROR);
		return FALSE;
	};
	if (GetFileAttributes(strPath) == INVALID_FILE_ATTRIBUTES)
		return failLoad(CString(newui::kErrFileNotFound) + strPath);

	opencascade::handle<CLoadProgressIndicator> hXferInd;
	if (!bShellLoading)
	{
		aLoad.SetTitle(newui::stepprev::kTitle);
		const BOOL bDlgCreated = aLoad.Create(CExportProgressDlg::IDD, AfxGetMainWnd());
		// 模板无 WS_VISIBLE，必须显式显示（P4·P 教训，同导出路径不抢焦点）
		if (bDlgCreated)
			aLoad.ShowWindow(SW_SHOWNOACTIVATE);
		if (pShellFrame != NULL)
			pShellFrame->EnableWindow(FALSE);
		aLoad.SetStage(8, newui::stepprev::kRead);   // 读取阶段静态文案（ReadFile 无进度回调）
		hXferInd = new CLoadProgressIndicator(&aLoad, 15, 75);   // 转换 15..90%
	}
	const CStringA strPathUtf8 = ToUtf8(lpszPath);   // 【P4·K】OCCT 期望 UTF-8 路径
	RebuildLog("P4V: read begin");
	STEPControl_Reader aReader;
	IFSelect_ReturnStatus eReadStat = IFSelect_RetVoid;
	try
	{
		eReadStat = aReader.ReadFile((const char*)strPathUtf8);
	}
	catch (const Standard_Failure& e)
	{
		CString strReason(e.what() != NULL ? CA2T(e.what()) : _T("(无异常消息)"));
		return failLoad(CString(newui::stepprev::kErrRead) + strReason
			+ _T("\n文件：") + strPath);
	}
	catch (...)
	{
		return failLoad(CString(newui::stepprev::kErrRead) + _T("(未知异常)")
			+ _T("\n文件：") + strPath);
	}
	RebuildLog("P4V: read done, stat=%d roots=%d", (int)eReadStat,
		aReader.NbRootsForTransfer());
	if (aLoad.IsCanceled())
	{
		m_bLoaded = FALSE;
		m_bStepPreview = FALSE;
		m_rebuiltShape.Nullify();
		closeLoadProgress();
		LogToOutputPane(newui::stepprev::kCanceled);
		return FALSE;
	}
	if (eReadStat != IFSelect_RetDone)
	{
		CString strMsg;
		strMsg.Format(_T("%s(读取状态码 %d)\n文件：%s"),
			newui::stepprev::kErrRead, (int)eReadStat, (LPCTSTR)strPath);
		return failLoad(strMsg);
	}

	RebuildLog("P4V: transfer begin");
	int nTransferred = 0;
	try
	{
		nTransferred = aReader.TransferRoots(
			hXferInd.IsNull() ? Message_ProgressRange() : hXferInd->Start());
	}
	catch (const Standard_Failure& e)
	{
		CString strReason(e.what() != NULL ? CA2T(e.what()) : _T("(无异常消息)"));
		return failLoad(CString(newui::stepprev::kErrRead) + strReason
			+ _T("\n文件：") + strPath);
	}
	catch (...)
	{
		return failLoad(CString(newui::stepprev::kErrRead) + _T("(未知异常)")
			+ _T("\n文件：") + strPath);
	}
	RebuildLog("P4V: transfer done, roots=%d shapes=%d", nTransferred,
		aReader.NbShapes());
	if (aLoad.IsCanceled())
	{
		m_bLoaded = FALSE;
		m_bStepPreview = FALSE;
		m_rebuiltShape.Nullify();
		closeLoadProgress();
		LogToOutputPane(newui::stepprev::kCanceled);
		return FALSE;
	}

	TopoDS_Shape aShape = aReader.OneShape();
	if (aShape.IsNull())
		return failLoad(CString(newui::stepprev::kErrNoShape) + strPath);

	// 渲染阶段：B-Rep 实体直接 AIS_Shape 显示（外观基线：纯黄 + DEFAULT 材质）
	if (!bShellLoading)
		aLoad.SetStage(95, newui::stepprev::kRender);
	m_rebuiltShape = aShape;    // 预览实体直接承载（无网格→实体重建环节）
	m_bStepPreview = TRUE;
	opencascade::handle<AIS_Shape> hPrs = new AIS_Shape(aShape);
	if (m_nMaterialIndex > 0)
		hPrs->SetMaterial(Graphic3d_MaterialAspect(s_aMaterialEnums[m_nMaterialIndex]));
	else
		hPrs->SetMaterial(Graphic3d_MaterialAspect());
	hPrs->SetColor(m_clrModel);
	m_hAISCtx->Display(hPrs, true);
	m_hModelPrs = hPrs;
	RebuildLog("P4V: ctx Display done");

	m_bLoaded = TRUE;
	RebuildLog("P4V: m_bLoaded set");

	// 载入成功日志（新 UI 增补文案；STL 路径取证文案"成功加载STL模型。"不动）
	{
		CMainFrame* pMain = (CMainFrame*)AfxGetMainWnd();
		if (pMain != NULL)
		{
			pMain->LogMessage(CString(newui::stepprev::kLoaded) + CString(lpszPath));
			pMain->SetStatusModelInfo(strPath.Mid(strPath.ReverseFind(_T('\\')) + 1));
		}
	}

	UpdateModelStatsFromShape();

	// 新 UI：刷新"文件"窗格列表与任务窗格（与 STL 载入同链路）
	CMainFrame* pMainList = (CMainFrame*)AfxGetMainWnd();
	if (pMainList != NULL)
	{
		pMainList->RefreshFileList();
		pMainList->UpdateTaskPane(this);
	}
	RebuildLog("P4V: panes refreshed, load complete");

	aLoad.SetStage(100, NULL);
	closeLoadProgress();
	return TRUE;
}

// 原版行为：载入/网格简化后填充属性窗格的模型统计——
// 顶点数=去重坐标数、边数=去重边数、面数=三角片数、包围盒=节点极值。
// （原版取自网格处理结果：test_cube 实测 顶点14/边24/面12，V-E+F=2 闭合流形）
void Cstl2stpcodeDoc::UpdateModelStatsFromMesh()
{
	if (m_hTriang.IsNull())
		return;
	const int nNodeCount = m_hTriang->NbNodes();
	std::map<std::tuple<double, double, double>, int> aPosToId;
	std::vector<int> anUniqueIdx(nNodeCount + 1, 0);
	double aBbMin[3] = { DBL_MAX, DBL_MAX, DBL_MAX };
	double aBbMax[3] = { -DBL_MAX, -DBL_MAX, -DBL_MAX };
	for (int i = 1; i <= nNodeCount; ++i)
	{
		const gp_Pnt aPnt = m_hTriang->Node(i);
		const auto aKey = std::make_tuple(aPnt.X(), aPnt.Y(), aPnt.Z());
		const auto aRes = aPosToId.emplace(aKey, static_cast<int>(aPosToId.size()) + 1);
		anUniqueIdx[i] = aRes.first->second;
		if (aPnt.X() < aBbMin[0]) aBbMin[0] = aPnt.X();
		if (aPnt.X() > aBbMax[0]) aBbMax[0] = aPnt.X();
		if (aPnt.Y() < aBbMin[1]) aBbMin[1] = aPnt.Y();
		if (aPnt.Y() > aBbMax[1]) aBbMax[1] = aPnt.Y();
		if (aPnt.Z() < aBbMin[2]) aBbMin[2] = aPnt.Z();
		if (aPnt.Z() > aBbMax[2]) aBbMax[2] = aPnt.Z();
	}
	// 边数 = 顶点数 + 面数 - 2（原版 0x140008900 取证 2026-09-09：三个属性分别取
	// A=StlMesh 顶点数、B=StlMesh 面数，边数用 lea eax,[rbx+rdi-2] 即 A+B-2——
	// 按单闭合壳欧拉公式推算，并非实际去重边数。niuniu 实测原版显示
	// 顶点 8658 / 边 25964 / 面 17308：8658+17308-2=25964，而实际去重边 25962，
	// 差 2 = 模型含两个闭合壳（牛+底板））。
	const long nVertexCount = (long)aPosToId.size();
	const long nFaceCount = (long)m_hTriang->NbTriangles();
	const long nEdgeCount = nVertexCount + nFaceCount - 2;
	{
		extern void StatsDebugLog(const char* fmt, ...);
		StatsDebugLog("stats: V=%ld E=%ld F=%ld bbox=[%.6f,%.6f][%.6f,%.6f][%.6f,%.6f]",
			nVertexCount, nEdgeCount, nFaceCount,
			aBbMin[0], aBbMax[0], aBbMin[1], aBbMax[1], aBbMin[2], aBbMax[2]);
	}
	CMainFrame* pMain = (CMainFrame*)AfxGetMainWnd();
	if (pMain != NULL)
	{
		// 坐标按原版 0x140008a20 传裸 double（VT_R8 同型赋值），勿传字符串
		pMain->UpdateModelStats(nVertexCount, nEdgeCount, nFaceCount,
			aBbMin[0], aBbMax[0], aBbMin[1], aBbMax[1], aBbMin[2], aBbMax[2]);
		// 新 UI 增补：状态栏 V/E/F 统计窗格
		CString strStats;
		strStats.Format(_T("V %ld · E %ld · F %ld"), nVertexCount, nEdgeCount, nFaceCount);
		pMain->SetStatusModelStats(strStats);
	}
}

// 【P4·V】STEP 预览统计（有意偏差，无原版对应）：B-Rep 实体无网格语义——
// 顶点/边/面 = TopExp 拓扑子形去重计数（IndexedMap），包围盒 = BRepBndLib
// 几何精确盒（非节点极值）；边数为真实拓扑边数，不再用 V+F-2 闭合网格
// 欧拉推算口径（见 UpdateModelStatsFromMesh 的取证注释）。
void Cstl2stpcodeDoc::UpdateModelStatsFromShape()
{
	if (m_rebuiltShape.IsNull())
		return;
	NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> aVerts, aEdges, aFaces;
	TopExp::MapShapes(m_rebuiltShape, TopAbs_VERTEX, aVerts);
	TopExp::MapShapes(m_rebuiltShape, TopAbs_EDGE, aEdges);
	TopExp::MapShapes(m_rebuiltShape, TopAbs_FACE, aFaces);
	Bnd_Box aBox;
	BRepBndLib::Add(m_rebuiltShape, aBox);
	double aBbMin[3] = { 0.0, 0.0, 0.0 }, aBbMax[3] = { 0.0, 0.0, 0.0 };
	if (!aBox.IsVoid())
		aBox.Get(aBbMin[0], aBbMin[1], aBbMin[2], aBbMax[0], aBbMax[1], aBbMax[2]);
	const long nVertexCount = (long)aVerts.Extent();
	const long nEdgeCount = (long)aEdges.Extent();
	const long nFaceCount = (long)aFaces.Extent();
	{
		extern void StatsDebugLog(const char* fmt, ...);
		StatsDebugLog("stepstats: V=%ld E=%ld F=%ld bbox=[%.6f,%.6f][%.6f,%.6f][%.6f,%.6f]",
			nVertexCount, nEdgeCount, nFaceCount,
			aBbMin[0], aBbMax[0], aBbMin[1], aBbMax[1], aBbMin[2], aBbMax[2]);
	}
	CMainFrame* pMain = (CMainFrame*)AfxGetMainWnd();
	if (pMain != NULL)
	{
		// 与网格统计同通道（坐标按原版 VT_R8 同型传裸 double）
		pMain->UpdateModelStats(nVertexCount, nEdgeCount, nFaceCount,
			aBbMin[0], aBbMax[0], aBbMin[1], aBbMax[1], aBbMin[2], aBbMax[2]);
		CString strStats;
		strStats.Format(_T("V %ld · E %ld · F %ld"), nVertexCount, nEdgeCount, nFaceCount);
		pMain->SetStatusModelStats(strStats);
	}
}

// Cstl2stpcodeDoc ���л�

void Cstl2stpcodeDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: �ڴ���Ӵ洢����
	}
	else
	{
		// TODO: �ڴ���Ӽ��ش���
	}
}

#ifdef SHARED_HANDLERS

// ����ͼ��֧��
void Cstl2stpcodeDoc::OnDrawThumbnail(CDC& dc, LPRECT lprcBounds)
{
	// �޸Ĵ˴����Ի����ĵ�����
	dc.FillSolidRect(lprcBounds, RGB(255, 255, 255));

	CString strText = _T("TODO: implement thumbnail drawing here");
	LOGFONT lf;

	CFont* pDefaultGUIFont = CFont::FromHandle((HFONT) GetStockObject(DEFAULT_GUI_FONT));
	pDefaultGUIFont->GetLogFont(&lf);
	lf.lfHeight = 36;

	CFont fontDraw;
	fontDraw.CreateFontIndirect(&lf);

	CFont* pOldFont = dc.SelectObject(&fontDraw);
	dc.DrawText(strText, lprcBounds, DT_CENTER | DT_WORDBREAK);
	dc.SelectObject(pOldFont);
}

// ������������֧��
void Cstl2stpcodeDoc::InitializeSearchContent()
{
	CString strSearchContent;
	// ���ĵ����������������ݡ�
	// ���ݲ���Ӧ��";"�ָ�

	SetSearchContent(strSearchContent);
}

void Cstl2stpcodeDoc::SetSearchContent(const CString& value)
{
	if (value.IsEmpty())
	{
		RemoveChunk(PKEY_Search_Contents.fmtid, PKEY_Search_Contents.pid);
	}
	else
	{
		CMFCFilterChunkValueImpl *pChunk = NULL;
		ATLTRY(pChunk = new CMFCFilterChunkValueImpl);
		if (pChunk != NULL)
		{
			pChunk->SetTextValue(PKEY_Search_Contents, value, CHUNK_TEXT);
			SetChunkValue(pChunk);
		}
	}
}

#endif // SHARED_HANDLERS

// Cstl2stpcodeDoc ���

#ifdef _DEBUG
void Cstl2stpcodeDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void Cstl2stpcodeDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG


// Cstl2stpcodeDoc ����

// ==== ��ȾЧ���������ԭ�� doc ��Ϣӳ�䣩 ====

// ��Ч��״̬д����ͼ����Ⱦ����������ԭ�� sub_14000E680 �����ֽ���Ϊ��
void Cstl2stpcodeDoc::ApplyRenderingParams()
{
	if (m_hAISCtx.IsNull() || m_hViewer.IsNull())
		return;

	// ԭ����� viewer+536 ��ͼ����ȡ���ͼ���°�ֱ���û��ͼ
	opencascade::handle<V3d_View> hView = m_hAISCtx->LastActiveView();
	if (hView.IsNull())
		return;

	Graphic3d_RenderingParams& rp = hView->ChangeRenderingParams();
	rp.Method = m_bRaytracing ? Graphic3d_RM_RAYTRACING : Graphic3d_RM_RASTERIZATION;
	rp.IsShadowEnabled = (m_bShadow != 0);
	rp.IsReflectionEnabled = (m_bReflection != 0);
	rp.IsAntialiasingEnabled = (m_bAntialiasing != 0);

	m_hAISCtx->UpdateCurrentViewer();
}

// ���߸��٣�����ԭ�� sub_14000E5F0����ת 624���ر�ʱ�������� ��Ӱ/����/��������
void Cstl2stpcodeDoc::OnViewRaytracing()
{
	m_bRaytracing = !m_bRaytracing;
	if (!m_bRaytracing)
	{
		m_bShadow = 0;
		m_bReflection = 0;
		m_bAntialiasing = 0;
	}
	ApplyRenderingParams();
}

// ���䣨����ԭ�� sub_14000E620����ת 626��
void Cstl2stpcodeDoc::OnViewReflection()
{
	m_bReflection = !m_bReflection;
	ApplyRenderingParams();
}

// ��Ӱ������ԭ�� sub_14000E640����ת 625��
void Cstl2stpcodeDoc::OnViewShadow()
{
	m_bShadow = !m_bShadow;
	ApplyRenderingParams();
}

// ������������ԭ�� sub_14000E660����ת 627��
void Cstl2stpcodeDoc::OnViewAntialiasing()
{
	m_bAntialiasing = !m_bAntialiasing;
	ApplyRenderingParams();
}

// �ο����񣨸���ԭ�� sub_14000E7E0����ת 628 �� ActivateGrid/DeactivateGrid��
void Cstl2stpcodeDoc::OnViewGrid()
{
	m_bGrid = !m_bGrid;
	if (m_hViewer.IsNull())
		return;

	if (m_bGrid)
		m_hViewer->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);
	else
		m_hViewer->DeactivateGrid();

	// 【新 UI 修复】master 的 Activate/DeactivateGrid 不触发网格重绘
	// （View.cpp OnInitialUpdate 同因注释：新版 V3d_Viewer::ActivateGrid
	// 内部不再 Grid->Display）。开关后显式刷新当前视图，复刻原版
	// 固定管线“点击即变”——缺此步时点击按钮无视觉反应，须晃动窗口才生效。
	if (!m_hAISCtx.IsNull())
		m_hAISCtx->UpdateCurrentViewer();
}

// 4 ��Ч����ť���õĹ�ѡ״̬������ԭ�� sub_14000E760����
//   ���߸��� �� SetCheck(624)����Ӱ/����/������ �� �� Enable(624) �� SetCheck(����״̬)
void Cstl2stpcodeDoc::OnUpdateViewEffect(CCmdUI* pCmdUI)
{
	if (pCmdUI->m_nID == ID_VIEW_RAYTRACING)
	{
		pCmdUI->SetCheck(m_bRaytracing);
		return;
	}

	// ԭ���ڷǹ��߸���ģʽ�½��� ��Ӱ/����/������ ��ť
	pCmdUI->Enable(m_bRaytracing);

	if (pCmdUI->m_nID == ID_VIEW_SHADOW)
		pCmdUI->SetCheck(m_bShadow);
	else if (pCmdUI->m_nID == ID_VIEW_REFLECTION)
		pCmdUI->SetCheck(m_bReflection);
	else
		pCmdUI->SetCheck(m_bAntialiasing);
}

// �ο�����ť��ѡ״̬������ԭ�� sub_14000E810��
void Cstl2stpcodeDoc::OnUpdateViewGrid(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_bGrid);
}

// ==== ���������� ====

// ���Ӳ�����������ԭ�� sub_14000E830��
//   ȡ��ǰѡ�ж��� �� Attach(OptionsForAttach{1,0,1}=Ĭ��ֵ) �� �ü����־��
//   ԭ���� InitCurrent/Current���ֲ������ģ����°��� SelectedOwner ���������
// ==== Export / Rebuild (Plan A: manipulator attach/detach commands removed, WYSIWYG) ====
// temporary rebuild debug log (plain CRT file IO, works in Release)
// 【新 UI 有意偏差】默认关闭：此前硬编码 Z:\ 绝对路径、每次重建都写盘。
// 现改为环境变量 STL2STP_DEBUG_LOG 存在时才启用（取证调试不受影响）。
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
static void RebuildLog(const char* fmt, ...)
{
    static int s_nEnabled = -1;
    if (s_nEnabled < 0)
        s_nEnabled = (::GetEnvironmentVariableA("STL2STP_DEBUG_LOG", NULL, 0) != 0) ? 1 : 0;
    if (!s_nEnabled)
        return;
    FILE* f = NULL;
    if (fopen_s(&f, "Z:\\Ashare\\stl2stp-pro\\stl2stp-code\\out\\rebuild_debug.log", "a") != 0 || f == NULL)
        return;
    va_list ap; va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fclose(f);
}

// 属性窗格统计的调试出口（与 RebuildLog 同文件）
void StatsDebugLog(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    char buf[512];
    vsnprintf_s(buf, _TRUNCATE, fmt, ap);
    va_end(ap);
    RebuildLog("%s", buf);
}

// 新 UI 增补：”材料”属性选项 → OCCT 材质枚举表 s_aMaterialEnums 定义已前移
//（P4·V：STEP 预览载入先引用；索引 0=”无”保持 DEFAULT，取证基线不变）

// 新 UI 增补：导出进度对话框类定义已前移至 LoadStlAndDisplay 之前
//（P4·P：载入路径共用，见上方扩展版 CExportProgressDlg）。

// 【新 UI P2：大模型渲染降采样】按步长抽样三角面构造显示专用网格：
// 完整网格超过阈值时每 stride 个面取 1 个（stride = ⌈面数/上限⌉），节点重新
// 编号只保留被抽中面引用的节点（O(F) 即时完成，不同于简化算法不改变文档网格）。
// 未超阈值时 m_hTriangDisplay 置空（显示直接用完整网格）。导出/统计/简化始终
// 用 m_hTriang，转换结果不受此影响（有意偏差，见 ui-redesign\UI重设计方案.md P2）
void Cstl2stpcodeDoc::BuildDisplayTriangulation(CExportProgressDlg* pLoadProgress)
{
	m_hTriangDisplay.Nullify();
	if (m_hTriang.IsNull())
		return;

	const int nFull = m_hTriang->NbTriangles();
	// 【新 UI P4·L】显示面数预算注册表可调（Workspace\DisplayBudget）：
	// P4·J 时代 60000 对 >100 万面模型是 21:1 聚类，细节凸包化（五官/手指
	// 不可辨）；实测阶梯（悟空 1.27M 面）：150k 细节完成度 56%（载入 52s）、
	// 500k 完成度 72%（载入 179s，与原版载入同模型同量级）——定档 500000
	//（图像分析员逐区梯度量化，见补充三十二）
	const int kMaxDisplayTriangles =
		AfxGetApp()->GetProfileInt(_T("Workspace"), _T("DisplayBudget"), 500000);
	if (nFull <= kMaxDisplayTriangles)
		return;

	// 【P4·P】聚类阶段：需聚类时上阶段文字；每轮粗化泵一次消息保持界面响应
	if (pLoadProgress != NULL)
		pLoadProgress->SetStage(56, newui::load::kCluster);

	// 【P4·J 修复（用户反馈：10.5 万面模型满屏黑斑/轮廓断裂）】顶点聚类替代
	// 按步长抽样。原实现逐 N 取三角形——孔洞在整面均匀散布，深色背景从孔洞
	// 透出即"满屏黑斑"，细特征（冠刺/薄壁）被抽稀成断裂毛刺（150_stls
	// obj_1_...A_A.stl 105546 面实测，原版全量显示光滑无斑）。顶点聚类：
	// 顶点吸附粗栅格并焊接，仅退化三角形被丢弃——表面闭合无孔洞，面数按
	// 栅格粗化逐轮收缩进入预算（每轮一次线性扫描，10 万面亚秒级，显示端
	// 专用；统计/导出仍走 m_hTriang 全量网格，行为不变）。
	double x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0;
	{
		Bnd_Box aBox;
		const int nNodes = m_hTriang->NbNodes();
		for (int i = 1; i <= nNodes; ++i)
			aBox.Add(m_hTriang->Node(i));
		aBox.Get(x0, y0, z0, x1, y1, z1);
	}
	const double aSpan = std::max(std::max(x1 - x0, y1 - y0), z1 - z0);
	if (aSpan <= 0.0)
		return;

	std::vector<gp_Pnt> aNodes;
	std::vector<Poly_Triangle> aTris;
	std::unordered_map<uint64_t, int> aWeld;   // 栅格坐标键 → 新节点 1 基索引
	double aCell = aSpan / 512.0;             // 起始细栅格：先尽量保真再逐轮粗化
	int nStrideLog = 0;
	for (int nAttempt = 0; nAttempt < 24; ++nAttempt, aCell *= 1.5)
	{
		if (pLoadProgress != NULL)
			pLoadProgress->SetStage(56, NULL);   // 每轮粗化泵消息（界面保持响应）
		aWeld.clear();
		aNodes.clear();
		aTris.clear();
		auto aWeldNode = [&](const gp_Pnt& aP) -> int
		{
			const uint64_t nIx = (uint64_t)std::max<int>(0,
				(int)std::floor((aP.X() - x0) / aCell) & 0x1FFFFF);
			const uint64_t nIy = (uint64_t)std::max<int>(0,
				(int)std::floor((aP.Y() - y0) / aCell) & 0x1FFFFF);
			const uint64_t nIz = (uint64_t)std::max<int>(0,
				(int)std::floor((aP.Z() - z0) / aCell) & 0x1FFFFF);
			const uint64_t nKey = (nIx << 42) | (nIy << 21) | nIz;
			auto it = aWeld.find(nKey);
			if (it == aWeld.end())
			{
				aNodes.push_back(aP);
				it = aWeld.emplace(nKey, (int)aNodes.size()).first;
			}
			return it->second;
		};
	for (int t = 1; t <= nFull; ++t)
	{
		const Poly_Triangle aTri = m_hTriang->Triangle(t);
		const int nA = aWeldNode(m_hTriang->Node(aTri.Value(1)));
		const int nB = aWeldNode(m_hTriang->Node(aTri.Value(2)));
		const int nC = aWeldNode(m_hTriang->Node(aTri.Value(3)));
		if (nA != nB && nB != nC && nA != nC)   // 栅格内塌缩的退化面丢弃
			aTris.push_back(Poly_Triangle(nA, nB, nC));
	}
		nStrideLog = nAttempt;
		if ((int)aTris.size() <= kMaxDisplayTriangles || aTris.empty())
			break;
	}

	if (aTris.empty())
		return;   // 全部塌缩（极端畸形网格）——退回全量显示走原渲染路径

	opencascade::handle<Poly_Triangulation> hDisplay =
		new Poly_Triangulation((int)aNodes.size(), (int)aTris.size(), false, false);
	for (size_t k = 0; k < aNodes.size(); ++k)
		hDisplay->SetNode((int)(k + 1), aNodes[k]);
	for (size_t k = 0; k < aTris.size(); ++k)
		hDisplay->SetTriangle((int)(k + 1), aTris[k]);
	m_hTriangDisplay = hDisplay;
	RebuildLog("display clustered: %d -> %d faces (grid pass %d)",
		nFull, (int)aTris.size(), nStrideLog + 1);
}

// 【新 UI P4·L】AIS_Triangulation 的 ComputeSelection 在 master 是空桩
//（AIS_Triangulation.cxx:188），直接用会拾取不中模型——派生类补挂
// Select3D_SensitiveTriangulation（网格级拾取、整模型高亮；原版 MeshVS
// 拾取同为网格级语义）。
class CModelTriangPrs : public AIS_Triangulation
{
public:
	CModelTriangPrs(const opencascade::handle<Poly_Triangulation>& hTri)
		: AIS_Triangulation(hTri) {}
protected:
	void ComputeSelection(const opencascade::handle<SelectMgr_Selection>& hSel,
		const int nMode) override
	{
		(void)nMode;
		opencascade::handle<SelectMgr_EntityOwner> hOwner =
			new SelectMgr_EntityOwner(this);
		hSel->Add(new Select3D_SensitiveTriangulation(hOwner,
			GetTriangulation(), TopLoc_Location()));
	}
};

// 【新 UI P4·L】AIS_Triangulation 显示路径：全量网格 + 平滑顶点法线。
// 法线 = Poly_Triangulation::ComputeNormals（三角形法线按面积权重累加到
// 3 顶点后归一化——与原版 MeshVS 平滑着色同机制）；AIS_Triangulation::
// Compute 读节点法线构造带法线索引数组（Gouraud），图元数组与 AIS_Shape
// 着色同构（Graphic3d_ArrayOfTriangles+法线）。几何全量——免显示降采样
// 与 BRep 缝合，细节不再损失。返回 FALSE = 开关关闭（注册表
// Workspace\DisplayPrimitive=0 强制回退）或网格为空，交回旧重建体路径。
BOOL Cstl2stpcodeDoc::DisplayTriangulationPrs()
{
	if (m_hAISCtx.IsNull())
		return FALSE;
	if (m_hTriang.IsNull() || m_hTriang->NbTriangles() == 0)
		return FALSE;
	// 实验定性（2026-09-15，悟空 1.27M 面 + niuniu 17k 面双测）：本机 GDI
	// Generic 1.1 下 AIS_Triangulation 与 MeshVS 同族无像素输出（Compute 正常
	// 完成、零光栅化 CPU、小模型同症）——默认关闭，GPU 机器可置 1 复用
	if (AfxGetApp()->GetProfileInt(_T("Workspace"), _T("DisplayPrimitive"), 0) == 0)
		return FALSE;
	if (!m_hTriang->HasNormals())
		m_hTriang->ComputeNormals();   // 幂等：已有法线不重算
	opencascade::handle<CModelTriangPrs> hPrs = new CModelTriangPrs(m_hTriang);
	// AIS_Triangulation 仅实现 mode 0（着色三角）；上下文全局模式为 1
	//（AIS_Shape 语义），必须对象级覆写为 0 否则空白
	hPrs->SetDisplayMode(0);
	// 颜色/材质沿用形态属性成员（默认纯黄 + DEFAULT 材质 = 取证基线不变）
	if (m_nMaterialIndex > 0)
		hPrs->SetMaterial(Graphic3d_MaterialAspect(s_aMaterialEnums[m_nMaterialIndex]));
	else
		hPrs->SetMaterial(Graphic3d_MaterialAspect());
	hPrs->SetColor(m_clrModel);
	m_hAISCtx->Display(hPrs, true);
	m_hModelPrs = hPrs;
	return TRUE;
}

void Cstl2stpcodeDoc::DisplayModelShape(CExportProgressDlg* pLoadProgress)
{
	// 模型显示（2026-09-08 环境取证）：本机 GL 为 GDI Generic 1.1 软回退，
	// MeshVS 图元阵列在此管线无像素输出，改用重建体的 AIS_Shape 显示（实测可渲染）。
	// 外观对齐原版（sub_14000C830 取证 2026-09-09 + 原版实机像素核验）：
	//   内面色 = Quantity_Color(Quantity_NameOfColor(0x1FE))，原版 TKernel 7.0
	//   颜色表 510 = RGB(1,1,0) 纯黄（ffff00）。master 同枚举值 510 = WHEAT2，
	//   不能按枚举取值，必须显式 RGB。
	//   材质 = 原版 MeshVS 未设 FrontMaterial 的回退 Graphic3d_MaterialAspect()
	//   即 Graphic3d_NameOfMaterial_DEFAULT（环境 0.15 / 漫反射 0.65 / 无高光）。
	if (m_hAISCtx.IsNull())
		return;

	RebuildLog("P4P: DisplayModelShape enter");
	m_hAISCtx->RemoveAll(true);

	// 【新 UI P4·L】AIS_Triangulation 直显（全量+平滑法线）在本机 GDI
	// Generic 1.1 下与 MeshVS 同族无像素输出（实验见 DisplayTriangulationPrs
	// 注释），GPU 机器可经注册表 DisplayPrimitive=1 启用；默认走下方重建体路径
	if (DisplayTriangulationPrs())
		return;

	// 【新 UI P2：大模型渲染降采样】超阈值时显示形状走抽样网格（重建耗时与
	// 渲染负载按比例下降，写局部形状不触碰 m_rebuiltShape）；导出路径仍走完整网格
	BuildDisplayTriangulation(pLoadProgress);
	if (!m_hTriangDisplay.IsNull())
		LogToOutputPane(newui::kDisplayDownsampled);
	const opencascade::handle<Poly_Triangulation> hDisplayMesh =
		m_hTriangDisplay.IsNull() ? m_hTriang : m_hTriangDisplay;
	// 【新 UI P4·L】显示网格预置平滑顶点法线（面积加权，与原版 MeshVS 平滑
	// 着色同机制）——重建体逐面携带后着色按已有法线 Gouraud 插值
	if (!hDisplayMesh->HasNormals())
		hDisplayMesh->ComputeNormals();
	TopoDS_Shape aDisplayShape;
	// 【P4·P】重建阶段：内部 2..88 的阶段百分比经 SetRemap 映射到本对话框
	// 的 58..95 区间；重建阶段取消（Step 边界）由调用侧统一判定
	if (pLoadProgress != NULL)
		pLoadProgress->SetRemap(58, 37);
	if (!RebuildShapeFromMesh(pLoadProgress, hDisplayMesh, &aDisplayShape, TRUE) ||
		aDisplayShape.IsNull())
		return;
	if (pLoadProgress != NULL)
		pLoadProgress->SetRemap(0, 100);   // 复位恒等映射（渲染阶段直接用绝对值）

	opencascade::handle<AIS_Shape> hPrs = new AIS_Shape(aDisplayShape);
	// 【新 UI P4·L】关闭本对象的自动重三角化：显示体已自带携带平滑法线的
	// 逐面三角化，BRepMesh 重网格化会以逐面平面法线覆盖（平台着色回退）；
	// 且 IsTessellated 的边多边形校验（BRepTools::Triangulation 要求每边挂
	// PolygonOnTriangulation）不满足必触发重网格，故必须显式关闭
	hPrs->Attributes()->SetAutoTriangulation(FALSE);
	// 【新 UI】颜色/材质初值来自形态属性成员（默认纯黄 + DEFAULT 材质 = 取证基线
	// 不变；用户在属性窗格改色/改材质后立即生效，并延续到后续重建显示）
	if (m_nMaterialIndex > 0)
		hPrs->SetMaterial(Graphic3d_MaterialAspect(s_aMaterialEnums[m_nMaterialIndex]));
	else
		hPrs->SetMaterial(Graphic3d_MaterialAspect());
	hPrs->SetColor(m_clrModel);
	// 【P4·P】渲染阶段（AIS 显示 + 首帧）
	if (pLoadProgress != NULL)
		pLoadProgress->SetStage(95, newui::load::kRender);
	m_hAISCtx->Display(hPrs, true);
	m_hModelPrs = hPrs;
	RebuildLog("P4P: ctx Display done");
}

BOOL Cstl2stpcodeDoc::RebuildShapeFromMesh(CExportProgressDlg* pProgress,
	const opencascade::handle<Poly_Triangulation>& hSource, TopoDS_Shape* pResult,
	BOOL bAttachDisplayTri)
{
	RebuildLog("=== rebuild start ===");
	// 新 UI P2：源网格/输出形状可注入——显示降采样路径传抽样网格与局部形状；
	// 导出路径默认完整网格写 m_rebuiltShape（导出实体结构不受显示降采样影响）
	opencascade::handle<Poly_Triangulation> hMesh = hSource.IsNull() ? m_hTriang : hSource;
	if (pResult == NULL)
		pResult = &m_rebuiltShape;
	if (hMesh.IsNull())
	{
		RebuildLog("no triangulation, bail");
		return FALSE;
	}

	// 进度步进：无进度对话框（如显示路径 DisplayModelShape）时零开销直通；
	// 有进度对话框时更新阶段文字/百分比并泵消息（对话框可重绘、可取消）
	auto Step = [pProgress](int nPct, const wchar_t* lpszStage) -> BOOL
	{
		if (pProgress == NULL)
			return TRUE;
		pProgress->SetStage(nPct, lpszStage);
		return !pProgress->IsCanceled();
	};

	BRep_Builder aBuilder;

	// -- 阶段 1：每个网格顶点建一个 TopoDS_Vertex（复刻原版 sub_14000D240 调用类）
	const int nNodes = hMesh->NbNodes();
	std::vector<TopoDS_Vertex> vertices(nNodes);
	for (int i = 1; i <= nNodes; ++i)
	{
		const gp_Pnt aPoint = hMesh->Node(i);
		if (i <= 3)
			RebuildLog("vertex %d: %g %g %g", i - 1, aPoint.X(), aPoint.Y(), aPoint.Z());
		BRepBuilderAPI_MakeVertex aVertexTool(aPoint);
		vertices[i - 1] = aVertexTool.Vertex();
		if ((i & 4095) == 0 &&
			!Step(2 + i * 23 / nNodes, L"重建 B-Rep 实体：构建顶点…"))
			return FALSE;
	}
	RebuildLog("vertices built: %d", nNodes);

	// -- 阶段 2：去重无向边 + 三角形带符号索引（复刻原版 sub_140005A50）：
	//    边 {a,b} 以一个方向存入哈希，同向查询得 +idx，反向查询得 -idx。
	const int nTri = hMesh->NbTriangles();
	std::vector<unsigned __int64> edges;
	std::vector<int> triangles;
	triangles.reserve((size_t)nTri * 3);
	std::unordered_map<unsigned __int64, int> edgeLookup;   // 去重查表
	edgeLookup.reserve((size_t)nTri * 3);
	for (int t = 1; t <= nTri; ++t)
	{
		const Poly_Triangle aTri = hMesh->Triangle(t);
		const int nodeIdx[3] = { aTri.Value(1), aTri.Value(2), aTri.Value(3) };
		if (t <= 2)
			RebuildLog("tri %d: (%d,%d,%d)", t - 1, nodeIdx[0], nodeIdx[1], nodeIdx[2]);
		for (int e = 0; e < 3; ++e)
		{
			const unsigned int a = (unsigned int)nodeIdx[e];
			const unsigned int b = (unsigned int)nodeIdx[(e + 1) % 3];
			int nSignedIdx = 0;
			auto itFwd = edgeLookup.find(((unsigned __int64)b << 32) | a);
			if (itFwd != edgeLookup.end())
				nSignedIdx = itFwd->second;                    // 已存 (a,b) → +idx
			else
			{
				auto itRev = edgeLookup.find(((unsigned __int64)a << 32) | b);
				if (itRev != edgeLookup.end())
					nSignedIdx = -(itRev->second);             // 已存 (b,a) → -idx
			}
			if (nSignedIdx == 0)
			{
				edges.push_back(((unsigned __int64)b << 32) | (unsigned __int64)a);
				const int nNewIdx = (int)edges.size();   // 1 基
				edgeLookup[((unsigned __int64)b << 32) | a] = nNewIdx;
				nSignedIdx = nNewIdx;
			}
			triangles.push_back(nSignedIdx);
		}
		if ((t & 4095) == 0 &&
			!Step(25 + t * 25 / nTri, L"重建 B-Rep 实体：去重有向边…"))
			return FALSE;
	}
	RebuildLog("dedup: triangles=%d edges=%d", nTri, (int)edges.size());

	// -- 阶段 3：每条去重边建一个 TopoDS_Edge（复刻原版 sub_14000D240 调用类）
	std::vector<TopoDS_Edge> edgeShapes;
	edgeShapes.reserve(edges.size());
	for (size_t k = 0; k < edges.size(); ++k)
	{
		const unsigned int a = (unsigned int)(edges[k] & 0xFFFFFFFFui64);
		const unsigned int b = (unsigned int)(edges[k] >> 32);
		if (k < 3)
			RebuildLog("edge %d: (%u,%u)", (int)k, a, b);
		BRepBuilderAPI_MakeEdge aEdgeTool(vertices[a - 1], vertices[b - 1]);
		edgeShapes.push_back(aEdgeTool.Edge());
		if ((k & 4095) == 0 &&
			!Step(50 + (int)k * 10 / (int)edges.size(), L"重建 B-Rep 实体：构建棱边…"))
			return FALSE;
	}
	RebuildLog("edges built: %d", (int)edgeShapes.size());

	// -- 阶段 4：每三角面 MakeWire(e1,e2,e3) + MakeFace(wire) → 平面。
	//    与原版一致：只调整第 1 条边方向（逆时针时反向），面建好后仅保留符号。
	std::vector<TopoDS_Face> faceShapes;
	faceShapes.reserve(nTri);
	for (size_t t = 0; t + 2 < triangles.size(); t += 3)
	{
		const int e1 = triangles[t], e2 = triangles[t + 1], e3 = triangles[t + 2];
		const int i1 = e1 < 0 ? -e1 : e1, i2 = e2 < 0 ? -e2 : e2, i3 = e3 < 0 ? -e3 : e3;
		TopoDS_Edge aE1 = edgeShapes[i1 - 1];
		const TopoDS_Edge& aE2 = edgeShapes[i2 - 1];
		const TopoDS_Edge& aE3 = edgeShapes[i3 - 1];
		if (e1 < 0)
			aE1.Reverse();
		BRepBuilderAPI_MakeWire aMakeWire(aE1, aE2, aE3);
		BRepBuilderAPI_MakeFace aMakeFace(aMakeWire.Wire());
		TopoDS_Face aFace = aMakeFace.Face();
		// 【新 UI P4·L】显示路径逐面挂载携带平滑法线的 3 节点三角化：
		// 着色时 StdPrs_ShadedShape::fillTriangles → BRepLib::ComputeNormals
		// 见 HasNormals 短路直接采用（Poly 面积加权平滑 = Gouraud 插值，
		// 对齐原版 MeshVS 平滑观感，取代逐面平面平台）。节点序 = 原始三角
		// 形循环，面方向翻转由 fillTriangles 的 REVERSED 法线反转逻辑处理。
		// 导出路径不挂（bAttachDisplayTri=FALSE）——实体结构/STEP 输出不变
		if (bAttachDisplayTri && hMesh->HasNormals())
		{
			const Poly_Triangle aTriSrc = hMesh->Triangle((int)(t / 3) + 1);
			const int anN[3] = { aTriSrc.Value(1), aTriSrc.Value(2), aTriSrc.Value(3) };
			opencascade::handle<Poly_Triangulation> hFaceTri =
				new Poly_Triangulation(3, 1, false, true);
			// 【P4·M 负结果记录】曾对"针状碎片"（最小高<0.15×最长边）替换
			// 自身几何法线试图消暗噪点——实测更糟（封闭暗孔 1→107、亮黄
			// -12%：焊接缝细三角的平滑法线跨缝平均后部分受光，恰是其在亮；
			// 暗噪点主体是明暗终止线（terminator）穿越折皱过渡带的固有点划，
			// 原版同款同密度）。丢弃碎片方案同样劣化（暗孔 1→675 背景裂缝）。
			// 结论：保持平滑法线，噪点与原版对齐属取证基线，不再外科手术。
			for (int k = 0; k < 3; ++k)
			{
				hFaceTri->SetNode(k + 1, hMesh->Node(anN[k]));
				hFaceTri->SetNormal(k + 1, hMesh->Normal(anN[k]));
			}
			hFaceTri->SetTriangle(1, Poly_Triangle(1, 2, 3));
			aBuilder.UpdateFace(aFace, hFaceTri);
		}
		faceShapes.push_back(aFace);
		if ((faceShapes.size() & 2047) == 0 &&
			!Step(60 + (int)faceShapes.size() * 25 / nTri, L"重建 B-Rep 实体：构建面片…"))
			return FALSE;
	}
	RebuildLog("faces built: %d", (int)faceShapes.size());

	// -- 阶段 5/6：壳 → 体（原版手工构造 TShell/TSolid，新版直接用 BRep_Builder 接口）
	if (!Step(88, L"组装壳与体…"))
		return FALSE;

	TopoDS_Shell aShell;
	aBuilder.MakeShell(aShell);
	for (size_t k = 0; k < faceShapes.size(); ++k)
		aBuilder.Add(aShell, faceShapes[k]);
	BRepTools::Update(aShell);
	RebuildLog("shell built, closed=%d", BRep_Tool::IsClosed(aShell) ? 1 : 0);

	TopoDS_Solid aSolid;
	aBuilder.MakeSolid(aSolid);
	aBuilder.Add(aSolid, aShell);
	RebuildLog("solid built");

	*pResult = aSolid;
	RebuildLog("=== rebuild stored, done ===");
	return TRUE;
}

// 输出窗格日志（复刻原版 sub_14000E950 / sub_14000F5B0 中的
// SendMessage(LB_ADDSTRING) + SendMessage(LB_SETCURSEL,7) + UpdateWindow 序列）
void Cstl2stpcodeDoc::LogToOutputPane(const wchar_t* msg)
{
	CMainFrame* pMain = (CMainFrame*)AfxGetMainWnd();
	if (pMain == NULL)
		return;
	pMain->LogExportLine(msg);
}

// ���� IGES������ԭ�� sub_14000E950��
void Cstl2stpcodeDoc::OnExportIges()
{
	// 【P4·V】STEP 预览守卫：纯预览不可导出（B-Rep 无网格转换语义）
	if (m_bStepPreview)
	{
		RebuildLog("P4V: export iges refused (step preview)");
		AfxMessageBox(newui::stepprev::kNoExport);
		return;
	}
	// δ����ģ�ͣ�ԭ�浯�� word_140070EA8
	if (!m_bLoaded)
	{
		AfxMessageBox(expstr::kNoModel);
		return;
	}
	// ԭ���ȡ StlMesh ��Ƭ����mesh+16�������� 100000 (0x186A0) ʱ MB_YESNO ȷ��
	if (m_hTriang->NbTriangles() > 100000)
	{
		if (AfxMessageBox(expstr::kWarnBigMesh, MB_YESNO) == IDNO)
			return;
	}

	CFileDialog aDlg(FALSE, _T("*.iges"), NULL, 6,
	                 _T("IGES Files (*.iges )|*.iges;|IGES Files (*.igs )| *.igs;||"),
	                 NULL, 0, TRUE);
	{
		// 新 UI 增补：初始目录优先取上次导出目录（注册表记忆）；
		// 回退到原版取证路径 CSF_OCCTDataPath（普通用户机器通常无此变量）
		CString strInitialDir = AfxGetApp()->GetProfileString(_T("Settings"), _T("LastExportDir"));
		if (strInitialDir.IsEmpty())
		{
			CString strEnv;
			strEnv.GetEnvironmentVariable(_T("CSF_OCCTDataPath"));
			if (!strEnv.IsEmpty())
				strInitialDir = strEnv + _T("\\iges");
		}
		if (!strInitialDir.IsEmpty())
			aDlg.GetOFN().lpstrInitialDir = strInitialDir;
	}
	if (aDlg.DoModal() != IDOK)
		return;

	// 新 UI 增补：记忆本次导出目录
	{
		CString strSelPath = aDlg.GetPathName();
		const int nSlash = strSelPath.ReverseFind(_T('\\'));
		if (nSlash > 0)
			AfxGetApp()->WriteProfileString(_T("Settings"), _T("LastExportDir"), strSelPath.Left(nSlash));
	}

	// �ȴ���� + �������"���ڴ���STLģ�ͣ����Ժ�..."
	SetCursor(LoadCursor(NULL, IDC_WAIT));
	LogToOutputPane(expstr::kExporting);

	// 【新 UI】进度对话框替代冻结式等待光标：分阶段进度 + 可取消。
	// 转换期间禁用主框架交互（进度对话框泵消息，防止导出流程被重入打断）。
	if (m_bExporting)
	{
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		return;
	}
	m_bExporting = TRUE;
	CExportProgressDlg aProgress;
	aProgress.Create(CExportProgressDlg::IDD, AfxGetMainWnd());
	aProgress.CenterWindow(AfxGetMainWnd());
	aProgress.ShowWindow(SW_SHOWNOACTIVATE);
	CWnd* pShellFrame = AfxGetMainWnd();
	if (pShellFrame != NULL)
		pShellFrame->EnableWindow(FALSE);

	BOOL bOK = RebuildShapeFromMesh(&aProgress) &&
		!m_rebuiltShape.IsNull() && !aProgress.IsCanceled();
	if (!bOK)
	{
		if (pShellFrame != NULL)
			pShellFrame->EnableWindow(TRUE);
		aProgress.DestroyWindow();
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		m_bExporting = FALSE;
		LogToOutputPane(newui::kExportCanceled);
		return;
	}
	aProgress.SetStage(90, _T("写入文件（此步耗时较长，请稍候）…"));

	// �ȴ������ؽ� TopoDS ʵ�壨ԭ�� sub_14000CE80��
	const CStringA strPathUtf8 = ToUtf8(aDlg.GetPathName());   // 【P4·K】OCCT 期望 UTF-8 路径
	const CStringA strNameUtf8 = ToUtf8(aDlg.GetFileName());
	TCollection_AsciiString aPath((const char*)strPathUtf8);
	TCollection_AsciiString aName((const char*)strNameUtf8);

	IGESControl_Controller::Init();
	// ԭ���ȡ XSTEP.iges.writebrep.mode / XSTEP.iges.unit���°��������
	int nWriteBrepMode = Interface_Static::IVal("write.brep.mode");
	const char* szUnit = Interface_Static::CVal("write.iges.unit");

	IGESControl_Writer aWriter(szUnit, nWriteBrepMode);

	opencascade::handle<TCollection_HAsciiString> hAuthor    = new TCollection_HAsciiString("VINCENT");
	opencascade::handle<TCollection_HAsciiString> hCompany   = new TCollection_HAsciiString("VINCENT");
	opencascade::handle<TCollection_HAsciiString> hFileName  = new TCollection_HAsciiString(aName.ToCString());
	opencascade::handle<TCollection_HAsciiString> hProcessor = new TCollection_HAsciiString("STL2STP IGES processor");

	// ����ԭ�棺���� GlobalSection��sub_14000F0C0������ 6 ���ֶΣ�д��ģ��
	IGESData_GlobalSection aSectionCopy(aWriter.Model()->GlobalSection());
	aSectionCopy.SetAuthorName(hAuthor);
	aSectionCopy.SetCompanyName(hCompany);
	aSectionCopy.SetFileName(hFileName);
	aSectionCopy.SetInterfaceVersion(hProcessor);
	aSectionCopy.SetSendName(hProcessor);
	aSectionCopy.SetSystemId(hProcessor);
	aWriter.Model()->SetGlobalSection(aSectionCopy);

	aWriter.AddShape(m_rebuiltShape);
	aWriter.ComputeModel();
	aWriter.Write(aPath.ToCString(), false);

	LogToOutputPane(expstr::kIgesDone);
	m_bExported = TRUE;    // P4·C：步骤条第 ③ 步完成

	// 【新 UI】转换完成：恢复主框架交互、关闭进度对话框
	if (pShellFrame != NULL)
		pShellFrame->EnableWindow(TRUE);
	aProgress.DestroyWindow();
	SetCursor(LoadCursor(NULL, IDC_ARROW));
	m_bExporting = FALSE;
}

// ���� STEP������ԭ�� sub_14000F5B0��
void Cstl2stpcodeDoc::OnExportStep()
{
	// 【P4·V】STEP 预览守卫：纯预览不可导出（B-Rep 无网格转换语义）
	if (m_bStepPreview)
	{
		RebuildLog("P4V: export step refused (step preview)");
		AfxMessageBox(newui::stepprev::kNoExport);
		return;
	}
	if (!m_bLoaded)
	{
		AfxMessageBox(expstr::kNoModel);
		return;
	}
	if (m_hTriang->NbTriangles() > 100000)
	{
		if (AfxMessageBox(expstr::kWarnBigMesh, MB_YESNO) == IDNO)
			return;
	}

	CFileDialog aDlg(FALSE, _T("*.step"), NULL, 6,
	                 _T("STEP Files (*.step )|*.step;|STEP Files (*.stp )| *.stp;||"),
	                 NULL, 0, TRUE);
	{
		// 新 UI 增补：初始目录优先取上次导出目录（注册表记忆）；
		// 回退到原版取证路径 CSF_OCCTDataPath（普通用户机器通常无此变量）
		CString strInitialDir = AfxGetApp()->GetProfileString(_T("Settings"), _T("LastExportDir"));
		if (strInitialDir.IsEmpty())
		{
			CString strEnv;
			strEnv.GetEnvironmentVariable(_T("CSF_OCCTDataPath"));
			if (!strEnv.IsEmpty())
				strInitialDir = strEnv + _T("\\step");
		}
		if (!strInitialDir.IsEmpty())
			aDlg.GetOFN().lpstrInitialDir = strInitialDir;
	}
	if (aDlg.DoModal() != IDOK)
		return;

	// 新 UI 增补：记忆本次导出目录
	{
		CString strSelPath = aDlg.GetPathName();
		const int nSlash = strSelPath.ReverseFind(_T('\\'));
		if (nSlash > 0)
			AfxGetApp()->WriteProfileString(_T("Settings"), _T("LastExportDir"), strSelPath.Left(nSlash));
	}

	SetCursor(LoadCursor(NULL, IDC_WAIT));
	LogToOutputPane(expstr::kExporting);

	// 【新 UI】进度对话框替代冻结式等待光标：分阶段进度 + 可取消。
	// 转换期间禁用主框架交互（进度对话框泵消息，防止导出流程被重入打断）。
	if (m_bExporting)
	{
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		return;
	}
	m_bExporting = TRUE;
	CExportProgressDlg aProgress;
	aProgress.Create(CExportProgressDlg::IDD, AfxGetMainWnd());
	aProgress.CenterWindow(AfxGetMainWnd());
	aProgress.ShowWindow(SW_SHOWNOACTIVATE);
	CWnd* pShellFrame = AfxGetMainWnd();
	if (pShellFrame != NULL)
		pShellFrame->EnableWindow(FALSE);

	BOOL bOK = RebuildShapeFromMesh(&aProgress) &&
		!m_rebuiltShape.IsNull() && !aProgress.IsCanceled();
	if (!bOK)
	{
		if (pShellFrame != NULL)
			pShellFrame->EnableWindow(TRUE);
		aProgress.DestroyWindow();
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		m_bExporting = FALSE;
		LogToOutputPane(newui::kExportCanceled);
		return;
	}
	aProgress.SetStage(90, _T("写入文件（此步耗时较长，请稍候）…"));

	const CStringA strNameUtf8 = ToUtf8(aDlg.GetFileName());   // 【P4·K】OCCT 头部名称按 UTF-8 语义写入
	TCollection_AsciiString aName((const char*)strNameUtf8);

	STEPControl_Writer aWriter;
	Interface_Static::SetCVal("write.step.product.name", "STL2STP 1.0.0");
	aWriter.Transfer(m_rebuiltShape, STEPControl_AsIs, true);

	opencascade::handle<StepData_StepModel> hModel = aWriter.Model(false);
	APIHeaderSection_MakeHeader aHeader(hModel);

	opencascade::handle<TCollection_HAsciiString> hName       = new TCollection_HAsciiString(aName.ToCString());
	opencascade::handle<TCollection_HAsciiString> hAuthor     = new TCollection_HAsciiString("VINCENT");
	opencascade::handle<TCollection_HAsciiString> hOrg        = new TCollection_HAsciiString("VINCENT");
	opencascade::handle<TCollection_HAsciiString> hOriginating = new TCollection_HAsciiString("STL2STP");
	opencascade::handle<TCollection_HAsciiString> hDescription = new TCollection_HAsciiString("STL TO STEP FILE");
	opencascade::handle<TCollection_HAsciiString> hPreproc     = new TCollection_HAsciiString("STL2STP STEP processor");

	aHeader.SetName(hName);
	aHeader.SetAuthorValue(1, hAuthor);
	aHeader.SetOrganizationValue(1, hOrg);
	aHeader.SetOriginatingSystem(hOriginating);
	aHeader.SetDescriptionValue(1, hDescription);
	aHeader.SetPreprocessorVersion(hPreproc);

	const CStringA strWriteUtf8 = ToUtf8(aDlg.GetPathName());   // 【P4·K】OCCT 期望 UTF-8 路径
	aWriter.Write((const char*)strWriteUtf8);

	LogToOutputPane(expstr::kStepDone);
	m_bExported = TRUE;    // P4·C：步骤条第 ③ 步完成

	// 【新 UI】转换完成：恢复主框架交互、关闭进度对话框
	if (pShellFrame != NULL)
		pShellFrame->EnableWindow(TRUE);
	aProgress.DestroyWindow();
	SetCursor(LoadCursor(NULL, IDC_ARROW));
	m_bExporting = FALSE;
}

// ����ԭ�� CMeshSimpDialog����Դ 333��������򻯱�������Ի���
// ���㷨���壨ԭ�� sub_14000FE20��OpenMesh ʵ�֣����������̱���
// 【新 UI 增补】构造时传入当前面数；输入比例时经 IDC_SIMP_ESTIMATE 实时联动
// “预计简化后的三角面数”。简化结果在 OnMeshSimplify 中备份原始网格，
// 可经菜单“工具→撤销上次简化”还原（原版简化不可逆）。
class CMeshSimpDlg : public CDialog
{
public:
	CMeshSimpDlg(int nCurrentFaces = 0, CWnd* pParent = NULL)
		: CDialog(IDD_MESH_SIMPLIFY, pParent), m_nCurrentFaces(nCurrentFaces) {}
	enum { IDD = IDD_MESH_SIMPLIFY };
	double m_dRatio = 0.0;
protected:
	virtual void DoDataExchange(CDataExchange* pDX)
	{
		CDialog::DoDataExchange(pDX);
		DDX_Text(pDX, IDC_SIMP_RATIO, m_dRatio);
	}
	virtual BOOL OnInitDialog()
	{
		CDialog::OnInitDialog();
		UpdateEstimate();
		return TRUE;
	}
	afx_msg void OnEnChangeRatio() { UpdateEstimate(); }
	void UpdateEstimate()
	{
		CString strText;
		GetDlgItemText(IDC_SIMP_RATIO, strText);
		strText.Trim();
		const double dRatio = _tstof(strText);
		if (strText.IsEmpty() || dRatio < 0.0 || dRatio > 1.0)
		{
			SetDlgItemText(IDC_SIMP_ESTIMATE,
				_T("预计简化后的三角面数：请输入 0-1 之间的比例"));
			return;
		}
		CString strEstimate;
		strEstimate.Format(_T("预计简化后的三角面数：%d（当前 %d）"),
			static_cast<int>(dRatio * m_nCurrentFaces + 0.5), m_nCurrentFaces);
		SetDlgItemText(IDC_SIMP_ESTIMATE, strEstimate);
	}
	int m_nCurrentFaces;
	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CMeshSimpDlg, CDialog)
	ON_EN_CHANGE(IDC_SIMP_RATIO, &CMeshSimpDlg::OnEnChangeRatio)
END_MESSAGE_MAP()

void Cstl2stpcodeDoc::OnMeshSimplify()
{
	// 【P4·V】STEP 预览守卫：纯预览不可简化（QEM 作用于三角网格）
	if (m_bStepPreview)
	{
		RebuildLog("P4V: simplify refused (step preview)");
		AfxMessageBox(newui::stepprev::kNoSimplify);
		return;
	}
	// 守卫与导出命令一致：未载入模型时提示（原版行为）
	if (!m_bLoaded)
	{
		AfxMessageBox(expstr::kNoModel);
		return;
	}

	// 复刻原版 CMeshSimpDialog（资源 333）：输入简化比例 0-1，越小面片越少
	// 【新 UI 增补】对话框显示实时预计面数；原始网格备份至 m_hTriangOrig 供撤销。
	// 【新 UI P2·2.1】简化执行体抽为共享实现 SimplifyByRatio——右栏"任务"窗格
	// 滑块入口与本对话框入口共用（原对话框入口原样保留，两入口并存）。
	CMeshSimpDlg aDlg(static_cast<int>(m_hTriang->NbTriangles()));
	if (aDlg.DoModal() != IDOK)
		return;
	SimplifyByRatio(aDlg.m_dRatio);
}

// 【新 UI P2·2.1】网格简化共享实现（IDD 333 对话框入口与任务卡滑块入口共用，
// 设计定稿 4.2.3(c)）。含守卫与全刷新链路；返回 FALSE = 未执行
//（未载入 / 比例越界 / 无变化）。
BOOL Cstl2stpcodeDoc::SimplifyByRatio(double dRatio)
{
	// 【P4·V】STEP 预览守卫：纯预览不可简化（任务窗格按钮已禁用，此处防
	// 菜单/快捷键/加速键直达路径）
	if (m_bStepPreview)
	{
		RebuildLog("P4V: simplify-by-ratio refused (step preview)");
		AfxMessageBox(newui::stepprev::kNoSimplify);
		return FALSE;
	}
	if (!m_bLoaded)
	{
		AfxMessageBox(expstr::kNoModel);
		return FALSE;
	}
	if (dRatio < 0.0 || dRatio > 1.0)
		return FALSE;   // 范围外：不处理（对话框文案已声明输入范围 0-1）

	// 原版 sub_14000FE20：OpenMesh 简化 → 重建；此处为等价的 QEM 边折叠实现，
	// 简化结果直接替换文档网格（不回写 STL 文件——原版写盘行为未取证，
	// 内存替换对视图/属性/导出链路的可见效果等价）。
	// 【新 UI P4·Q（用户指定"避免UI卡死"）】简化全程进度对话框替代冻结式
	// 等待光标：QEM 折叠按已折叠面数推进（5..55%），显示重建/渲染复用
	// 载入路径的 DisplayModelShape(pProgress) 反馈（56..95%），收尾 100%。
	// 取消仅在折叠阶段生效（彼时网格尚未替换，取消 = 原样返回干净退出）；
	// 重建开始后取消不再采纳（网格已替换，须走完显示链保持状态一致）。
	LogToOutputPane(expstr::kExporting);
	RebuildLog("P4Q: simplify begin ratio=%.3f srcTri=%d",
		dRatio, m_hTriang->NbTriangles());
	CExportProgressDlg aSimp;
	aSimp.SetTitle(newui::simpl::kTitle);
	const BOOL bDlgCreated = aSimp.Create(CExportProgressDlg::IDD, AfxGetMainWnd());
	if (bDlgCreated)
	{
		// 模板无 WS_VISIBLE，须显式显示（同导出/载入路径，见补充三十六）
		aSimp.ShowWindow(SW_SHOWNOACTIVATE);
		if (AfxGetMainWnd() != NULL)
			AfxGetMainWnd()->EnableWindow(FALSE);   // 防简化期间重复触发
		aSimp.SetStage(3, newui::simpl::kCollapsing);
	}
	auto closeSimpProgress = [&]()
	{
		aSimp.SetStage(100, NULL);
		if (aSimp.GetSafeHwnd() != NULL)
		{
			if (AfxGetMainWnd() != NULL)
				AfxGetMainWnd()->EnableWindow(TRUE);
			aSimp.DestroyWindow();
		}
	};

	opencascade::handle<Poly_Triangulation> hNew =
		SimplifyTriangulation(m_hTriang, dRatio,
			[&aSimp](int nPercent) -> bool {
				aSimp.SetStage(5 + nPercent * 50 / 100, NULL);   // QEM 5..55%
				return !aSimp.IsCanceled();
			});
	if (hNew.IsNull())
	{
		// 折叠阶段取消：网格未替换，原样收尾（不弹错误框，输出窗格提示）
		RebuildLog("P4Q: canceled");
		closeSimpProgress();
		LogToOutputPane(newui::simpl::kCanceled);
		return FALSE;
	}
	if (hNew->NbTriangles() == 0 ||
		hNew->NbTriangles() == m_hTriang->NbTriangles())
	{
		closeSimpProgress();
		return FALSE;   // 无变化
	}
	RebuildLog("P4Q: qem done tri=%d", hNew->NbTriangles());

	// 新 UI 增补：首次简化前备份原始网格（撤销入口：工具→撤销上次简化/任务卡）
	if (m_hTriangOrig.IsNull())
		m_hTriangOrig = m_hTriang;

	// 换网格数据并刷新显示（与载入路径同一套 AIS_Shape 显示流程，
	// 经同一进度对话框反馈重建/渲染 56..95%）
	m_hTriang = hNew;
	DisplayModelShape(&aSimp);
	closeSimpProgress();
	RebuildLog("P4Q: simplify complete");

	// 原版行为：简化（处理）后刷新属性窗格统计
	UpdateModelStatsFromMesh();

	// 新 UI P2·2.1：任务窗格状态刷新（简化状态/撤销可用/预计面数基数/大模型提示）
	CMainFrame* pMainTask = (CMainFrame*)AfxGetMainWnd();
	if (pMainTask != NULL)
		pMainTask->UpdateTaskPane(this);
	return TRUE;
}

// 【新 UI P2·2.1】撤销共享实现（菜单 32838 入口与任务卡按钮共用）
BOOL Cstl2stpcodeDoc::UndoLastSimplify()
{
	if (m_hTriangOrig.IsNull())
		return FALSE;
	m_hTriang = m_hTriangOrig;
	m_hTriangOrig.Nullify();

	DisplayModelShape();
	UpdateModelStatsFromMesh();

	LogToOutputPane(newui::kSimplifyUndone);

	// 新 UI P2·2.1：任务窗格状态刷新（还原后简化状态/撤销可用性复位）
	CMainFrame* pMainTask = (CMainFrame*)AfxGetMainWnd();
	if (pMainTask != NULL)
		pMainTask->UpdateTaskPane(this);
	return TRUE;
}

// 撤销上次网格简化（新 UI 增补，无原版对应）：还原备份的原始网格，
// 重走显示/统计刷新链路。载入新文件或再次简化不叠加多级撤销（单级快照）。
void Cstl2stpcodeDoc::OnMeshSimplifyUndo()
{
	UndoLastSimplify();
}

// “撤销上次简化”可用性：需已载入模型且存在撤销快照
void Cstl2stpcodeDoc::OnUpdateMeshSimplifyUndo(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_bLoaded && !m_hTriangOrig.IsNull());
}

// 新 UI：文档销毁前刷新“文件”窗格——基类 OnCloseDocument 末尾 delete this，
// 列表项保存的文档指针必须在删除前失效（刷新时排除自身）
void Cstl2stpcodeDoc::OnCloseDocument()
{
	CMainFrame* pMain = (CMainFrame*)AfxGetMainWnd();
	if (pMain != NULL)
		pMain->RefreshFileList(this);
	CDocument::OnCloseDocument();
}

// 新 UI：形态属性真生效——更新当前显示模型颜色（原版颜色属性可编辑但无任何效果）
void Cstl2stpcodeDoc::SetModelDisplayColor(COLORREF clr)
{
	// 2.2：门控日志——卡片颜色按钮自动化验证路由（STL2STP_DEBUG_LOG）
	StatsDebugLog("card form: color RGB(%d,%d,%d)",
		(int)GetRValue(clr), (int)GetGValue(clr), (int)GetBValue(clr));
	m_clrModel = Quantity_Color(GetRValue(clr) / 255.0,
		GetGValue(clr) / 255.0, GetBValue(clr) / 255.0, Quantity_TOC_RGB);
	if (!m_hModelPrs.IsNull() && !m_hAISCtx.IsNull())
	{
		m_hModelPrs->SetColor(m_clrModel);
		m_hAISCtx->UpdateCurrentViewer();
	}
}

// 新 UI：形态属性真生效——材料索引 0=无(DEFAULT) 1=材料1(BRASS) 2=材料2(STEEL) 3=材料3(SILVER)
void Cstl2stpcodeDoc::SetModelDisplayMaterial(int nIndex)
{
	if (nIndex < 0 || nIndex > 3)
		return;
	// 2.2：门控日志——卡片材料下拉自动化验证路由
	StatsDebugLog("card form: material index %d", nIndex);
	m_nMaterialIndex = nIndex;
	if (!m_hModelPrs.IsNull() && !m_hAISCtx.IsNull())
	{
		m_hModelPrs->SetMaterial(nIndex > 0
			? Graphic3d_MaterialAspect(s_aMaterialEnums[nIndex])
			: Graphic3d_MaterialAspect());
		m_hAISCtx->UpdateCurrentViewer();
	}
}

// 【新 UI P2·SDI 化阶段 1.2】文档复用后的重取景（见 OnOpenDocument 注释）：
// 找到文档已存在的视图，经视图公开入口 RefitModel 重取景（FitViewToModel 同型）。
BOOL Cstl2stpcodeDoc::RefitActiveView()
{
	POSITION pos = GetFirstViewPosition();
	if (pos == NULL)
		return FALSE;
	Cstl2stpcodeView* pView = DYNAMIC_DOWNCAST(Cstl2stpcodeView, GetNextView(pos));
	if (pView == NULL)
		return FALSE;
	pView->RefitModel();
	return TRUE;
}
