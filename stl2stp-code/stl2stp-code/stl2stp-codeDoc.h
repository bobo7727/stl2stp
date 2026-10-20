
// stl2stp-codeDoc.h : Cstl2stpcodeDoc 类的接口
//


#pragma once

#include <TopoDS_Shape.hxx>
#include <Quantity_Color.hxx>
#include <AIS_Shape.hxx>    // 新 UI：m_hModelPrs 颜色/材质在位更新（stdafx 未包含此头）
#include <AIS_Triangulation.hxx>   // 新 UI P4·L：全量网格+平滑法线显示基元（派生类挂网格拾取）

class CExportProgressDlg;   // 新 UI：导出进度对话框（实现见 stl2stp-codeDoc.cpp）


class Cstl2stpcodeDoc : public CDocument
{
protected: // 仅从序列化创建
	Cstl2stpcodeDoc();
	DECLARE_DYNCREATE(Cstl2stpcodeDoc)

// 特性
public:
	// 加载 STL 文件并放入 AIS 场景（复刻原版 CVoxelModDoc::sub_14000C830）
	BOOL LoadStlAndDisplay(LPCTSTR lpszPath);
	BOOL IsModelLoaded() const { return m_bLoaded; }
	// 【新 UI P4·V】STEP 纯预览（用户指定"把 STEP 预览做进应用里"；原版无 STEP
	// 读入能力，登记有意偏差）：.step/.stp 经 OnOpenDocument 分流读入 B-Rep 实体
	//（STEPControl_Reader），仅显示预览——简化/导出按网格语义拒绝（各入口守卫），
	// m_rebuiltShape 直接承载 STEP 实体（无网格→实体重建环节）
	BOOL LoadStepPreview(LPCTSTR lpszPath);
	BOOL IsStepPreview() const { return m_bStepPreview; }
	// 【新 UI P2·SDI 化阶段 1.2】文档复用后的重取景：SDI 下 MRU/Ctrl+O/导入等
	// 再次载入复用同一文档/视图（OnInitialUpdate 不再触发），载入成功后由此
	// 对已存在视图重取景（内部调 Cstl2stpcodeView::RefitModel，设计定稿 4.2.2(e)1）
	BOOL RefitActiveView();
	// 【新 UI P2·2.1】网格简化/撤销共享实现：IDD 333 对话框入口（菜单与工具栏
	// 32837/32838）与右栏"任务"窗格滑块/按钮入口共用（设计定稿 4.2.3(c)）
	BOOL SimplifyByRatio(double dRatio);   // 返回 FALSE = 未执行（未载入/越界/无变化）
	BOOL UndoLastSimplify();               // 无快照时返回 FALSE
	// 新 UI：当前三角面数（“文件”窗格列表显示用）
	int  GetTriangleCount() const { return m_hTriang.IsNull() ? 0 : m_hTriang->NbTriangles(); }
	// 新 UI：形态属性真生效（原版颜色/材料属性可编辑但无任何效果）——
	// 作用于当前显示的 AIS_Shape 并持久到下次重建显示
	void SetModelDisplayColor(COLORREF clr);
	void SetModelDisplayMaterial(int nIndex);   // 0=无(DEFAULT) 1..3=材质枚举

// 重写
public:
	virtual BOOL OnNewDocument();
	virtual BOOL OnOpenDocument(LPCTSTR lpszPath);
	// 【新 UI P2·SDI 化阶段 1.2】SDI 复用语义的文档复位：MDI"新建"=新文档对象
	//（构造函数复位）；SDI 复用同一对象，必须在此显式清网格/载入标志/场景，
	// 否则"新建"后旧网格残留、导出守卫失效（CDocument::OnNewDocument 内部先
	// 调本函数，OnOpenDocument 顶部亦显式调用——新建/打开两路全覆盖）
	virtual void DeleteContents();
	virtual void OnCloseDocument();
	virtual void Serialize(CArchive& ar);
#ifdef SHARED_HANDLERS
	virtual void InitializeSearchContent();
	virtual void OnDrawThumbnail(CDC& dc, LPRECT lprcBounds);
#endif // SHARED_HANDLERS

// 实现
public:
	virtual ~Cstl2stpcodeDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// OpenCASCADE 场景对象（官方 API，正常构造；
	// 原版对应 CVoxelModDoc 成员：viewer +384 / ctx +392 / driver +400 /
	// MeshVS +448 / 网格 +480 / 操纵器 +632）
	opencascade::handle<Graphic3d_GraphicDriver>   m_hGraphicDriver;
	opencascade::handle<V3d_Viewer>                m_hViewer;
	opencascade::handle<AIS_InteractiveContext>    m_hAISCtx;
	opencascade::handle<MeshVS_Mesh>               m_hMeshVS;
	// STL 三角网格（原版 StlMesh_Mesh +480；新版 OCCT 已移除 StlMesh，
	// 改用 Poly_Triangulation）
	opencascade::handle<Poly_Triangulation>        m_hTriang;
	// 简化前原始网格备份（新 UI 增补：撤销“网格简化”用；首次简化时备份）
	opencascade::handle<Poly_Triangulation>        m_hTriangOrig;
	// 显示专用降采样网格（新 UI P2：大模型渲染降采样）——空 = 未降采样。
	// 超过阈值时按步长抽样三角面仅供显示；导出/统计/简化仍用完整 m_hTriang
	opencascade::handle<Poly_Triangulation>        m_hTriangDisplay;
	// 【方案 A（WYSIWYG）】原版操纵器成员 m_hManipulator / m_bManipActive 已移除——
	// 视口姿态编辑不被导出尊重（导出仍用原始网格），转换器定位下该功能误导用户；
	// 原实现记录见 AGENT.md 验收补充十。
	BOOL          m_bLoaded;     // +488 是否已加载模型
	// 【P4·V】STEP 纯预览态：TRUE = 当前文档载入的是 STEP B-Rep 实体（仅预览，
	// 简化/导出拒绝；统计走 TopExp 拓扑计数而非网格 V+F-2）
	BOOL          m_bStepPreview;
	ULONGLONG     m_ullFileSize; // +600 文件大小(字节)

	// 渲染/效果状态（复刻原版 doc+624..628 字节）
	BYTE m_bRaytracing;     // +624 光线跟踪
	BYTE m_bShadow;         // +625 阴影
	BYTE m_bReflection;     // +626 反射
	BYTE m_bAntialiasing;   // +627 反走样
	BYTE m_bGrid;           // +628 参考网格

	// 将效果状态写入活动视图的渲染参数并刷新（复刻原版 sub_14000E680）
	void ApplyRenderingParams();

	// Rebuilt solid from the STL mesh (original doc+416: {TShape, location, orientation})
	TopoDS_Shape m_rebuiltShape;
	// 当前显示的模型基元（新 UI：颜色/材料属性在位更新用；原版每次重建新句柄。
	// P4·L 默认 AIS_Triangulation（网格直显），注册表 DisplayPrimitive=0 时为
	// AIS_Shape 重建体——两基类的 SetColor/SetMaterial 同走 AIS 基类接口）
	opencascade::handle<AIS_InteractiveObject> m_hModelPrs;
	// 模型显示颜色与材质索引（新 UI；默认纯黄+DEFAULT 保持取证基线）
	Quantity_Color m_clrModel;
	int m_nMaterialIndex;
	// 导出进行中标志（新 UI：进度泵消息期间的防重入守卫）
	BOOL m_bExporting;
	// 本次载入后是否已成功导出（新 UI P4·C：工具行步骤条第 ③ 步状态；
	// DeleteContents 复位、STEP/IGES 任一导出成功置位，撤销简化不回退）
	BOOL m_bExported;
	// mesh -> TopoDS solid (original sub_14000CE80 + sub_140005A50 + sub_14000D240)
	// 新 UI：返回 FALSE 表示被用户取消（进度对话框取消或阶段失败）
	// 新 UI P2：hSource 指定源网格（默认完整网格；显示降采样路径传抽样网格），
	// pResult 指定输出形状（默认 &m_rebuiltShape 导出形状；显示路径传局部形状，
	// 避免抽样形状污染导出）
	// 新 UI P4·L：bAttachDisplayTri=TRUE 时逐面挂载携带平滑法线的 3 节点三角化
	//（显示路径 Gouraud 着色用；导出路径保持 FALSE——实体结构/STEP 输出不变）
	BOOL RebuildShapeFromMesh(CExportProgressDlg* pProgress = NULL,
		const opencascade::handle<Poly_Triangulation>& hSource
			= opencascade::handle<Poly_Triangulation>(),
		TopoDS_Shape* pResult = NULL,
		BOOL bAttachDisplayTri = FALSE);
	// 大模型显示降采样（新 UI P2）：超过阈值时按步长抽样出 m_hTriangDisplay
	//【P4·P】pLoadProgress 非空时聚类阶段泵消息更新载入进度对话框
	void BuildDisplayTriangulation(CExportProgressDlg* pLoadProgress = NULL);
	// 模型显示：默认 AIS_Triangulation 直显（全量网格+平滑法线，P4·L），
	// 回退时 AIS_Shape 重建体（显示降采样+平面着色）。载入与网格简化后共用
	//（MeshVS 显示在本机 GL 软回退下无像素，见 Doc.cpp 注释）
	//【P4·P】载入路径传进度对话框（聚类/重建/渲染阶段反馈），简化重显传 NULL
	void DisplayModelShape(CExportProgressDlg* pLoadProgress = NULL);
	// 【新 UI P4·L】AIS_Triangulation 直显路径（机制见 Doc.cpp 实现注释）；
	// 返回 FALSE = 开关关闭（注册表 Workspace\DisplayPrimitive=0）或网格为空，
	// 交回旧重建体路径
	BOOL DisplayTriangulationPrs();
	// 属性窗格模型统计（顶点/边/面/包围盒）：载入与网格简化后刷新
	void UpdateModelStatsFromMesh();
	// 【P4·V】STEP 预览统计：TopExp 去重拓扑计数 + BRepBndLib 包围盒
	//（有意偏差：B-Rep 无网格语义，边数为真实拓扑边数，非 V+F-2 欧拉推算）
	void UpdateModelStatsFromShape();
	// output pane log (original: LB_ADDSTRING + LB_SETCURSEL(7) to mainframe listbox)
	void LogToOutputPane(const wchar_t* msg);

protected:

// 生成的消息映射函数
protected:
	// 效果命令（复刻原版 sub_14000E5F0/E620/E640/E660/E7E0）
	afx_msg void OnViewRaytracing();
	afx_msg void OnViewShadow();
	afx_msg void OnViewReflection();
	afx_msg void OnViewAntialiasing();
	afx_msg void OnViewGrid();
	afx_msg void OnExportIges();          // sub_14000E950
	afx_msg void OnExportStep();          // sub_14000F5B0
	afx_msg void OnMeshSimplify();        // sub_14000FE20（OpenMesh 算法待实现）
	afx_msg void OnMeshSimplifyUndo();    // 撤销上次简化（新 UI 增补，无原版对应）
	afx_msg void OnUpdateMeshSimplifyUndo(CCmdUI* pCmdUI);
	// 4 个效果按钮共用（复刻原版 sub_14000E760）
	afx_msg void OnUpdateViewEffect(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewGrid(CCmdUI* pCmdUI);   // sub_14000E810
	DECLARE_MESSAGE_MAP()

#ifdef SHARED_HANDLERS
	// 用于为搜索处理程序设置搜索内容的 Helper 函数
	void SetSearchContent(const CString& value);
#endif // SHARED_HANDLERS
};
