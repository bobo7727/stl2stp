#pragma once

// ExportStrings.h - message strings used by the IGES/STEP export handlers,
// byte-identical to the original STL2STP.exe rdata (decoded from the binary).
// This file is UTF-8 with BOM so MSVC parses the Chinese literals correctly.

namespace expstr
{
	// no model loaded (original word_140070EA8, wide)
	const wchar_t* const kNoModel = L"请导入STL模型！";

	// output pane: rebuild/export started (original unk_140071018, GBK bytes)
	const wchar_t* const kExporting = L"正在处理STL模型，请稍后...";

	// output pane: IGES done (original unk_140071088)
	const wchar_t* const kIgesDone = L"成功存为IGES模型！";

	// output pane: STEP done (original unk_140071190)
	const wchar_t* const kStepDone = L"成功存为STEP模型！";

	// >100000 triangles warning (original aSMrs at 0x140070EC0, wide, MB_YESNO)
	const wchar_t* const kWarnBigMesh =
		L"当前STL模型的面片数目较多，建议对其进行网格简化（点击工具栏中的黄色 J 按钮）后再转换。\n"
		L"是否继续（“是”：继续转换当前STL模型，“否”：停止转换）？";
}

// ==== 新 UI 增补字符串（有意偏差，无原版对应；见 ui-redesign\UI重设计方案.md）====
namespace newui
{
	// STL 载入失败细分原因（原版仅一句“读取STL模型文件失败！”）
	const wchar_t* const kErrFileNotFound =
		L"读取STL模型文件失败！\n文件不存在或无法访问：\n";
	const wchar_t* const kErrFileTooSmall =
		L"读取STL模型文件失败！\n文件过小，不是有效的二进制 STL 数据（至少 84 字节）：\n";
	const wchar_t* const kErrParseFail =
		L"读取STL模型文件失败！\n解析器报告：\n";
	const wchar_t* const kErrEmptyMesh =
		L"读取STL模型文件失败！\n文件中没有可用的三角面片：\n";

	// 撤销简化完成日志
	const wchar_t* const kSimplifyUndone =
		L"已撤销网格简化，恢复原始网格。";

	// 导出取消日志（新 UI：进度对话框“取消”按钮）
	const wchar_t* const kExportCanceled =
		L"已取消转换（未写出文件）。";

	// 大模型显示抽样日志（新 UI P2：仅显示端，转换结果不受影响；P4·J 抽样
	// 方式由步长抽样改为顶点聚类，文案同步）
	const wchar_t* const kDisplayDownsampled =
		L"模型面数较大，显示已做聚类抽样（不影响转换结果）。";

	// 【P4·K】截断 STL 容错载入日志（二进制 STL 头声明面数超出文件实际
	// 长度时，按实际长度载入可用面片）
	const wchar_t* const kTruncatedStl =
		L"文件不完整：头部声明 %d 面片，文件实际存有 %d 面片，已容错载入可用部分。";
}

// ==== 新 UI P2·2.1：右栏任务窗格（CTaskPane 三卡）文案（有意偏差，无原版对应）====
namespace newui { namespace task {

	// 分组框标题
	const wchar_t* const kGrpInfo   = L"模型信息";
	const wchar_t* const kGrpSimp   = L"网格简化";
	const wchar_t* const kGrpExport = L"导出";

	// 模型信息卡
	const wchar_t* const kLblFilename  = L"文件名：";
	const wchar_t* const kLblFilesize  = L"文件大小：";
	const wchar_t* const kLblSimpState = L"简化状态：";
	const wchar_t* const kLblV = L"顶点数：";
	const wchar_t* const kLblE = L"边数：";
	const wchar_t* const kLblF = L"面数：";
	// 包围盒 6 值标签（顺序 Xmin/Xmax/Ymin/Ymax/Zmin/Zmax；值保持 6 位小数，
	// 与原版属性窗格显示口径一致——取证基线保护条款）
	const wchar_t* const kBBoxLabels[6] =
		{ L"X最小值 ", L"X最大值 ", L"Y最小值 ", L"Y最大值 ", L"Z最小值 ", L"Z最大值 " };
	// 形态属性（2.2 自属性窗格迁入卡片）：
	// 材料/颜色名称与原属性网格一致（无/材料1..3 → DEFAULT/BRASS/STEEL/SILVER）；
	// 颜色“默认”= 纯黄（真实语义——旧网格的“默认”按钮是 3DFACE 外观色，偏差已预登记）
	const wchar_t* const kLblColor   = L"颜色";
	const wchar_t* const kLblMaterial = L"材料";
	const wchar_t* const kColorAuto  = L"默认";   // 颜色拾取“默认”项 = 纯黄
	const wchar_t* const kMatOptions[4] = { L"无", L"材料1", L"材料2", L"材料3" };
	const wchar_t* const kNoFile = L"—";
	const wchar_t* const kSimpNo  = L"未简化";
	const wchar_t* const kSimpYes = L"已简化（可撤销）";

	// 网格简化卡（滑块行 + 预计面数，滑块滚动时整行刷新）
	const wchar_t* const kLblRatio = L"保留比例（%）：";
	const wchar_t* const kEstimateFmt =
		L"预计简化后的三角面数：%d（当前 %d）";
	const wchar_t* const kEstimateNoModel =
		L"预计简化后的三角面数：—（未载入模型）";
	const wchar_t* const kBtnApply = L"应用简化";
	const wchar_t* const kBtnUndo  = L"撤销";
	const wchar_t* const kSimpHint =
		L"简化只改内存网格，可随时撤销；不影响原始 STL 文件。";

	// 导出卡（两按钮与工具栏/菜单同一 WM_COMMAND 链路；确认框等导出守卫原样保留）
	const wchar_t* const kBtnStep = L"导出 STEP";
	const wchar_t* const kBtnIges = L"导出 IGES";
	// >10 万面内联提示（advisory：仅提示，不替代原版 MB_YESNO 确认框）
	const wchar_t* const kBigHintFmt =
		L"当前 %d 面超过 10 万，直接导出较慢、文件较大，建议先简化。";
	const wchar_t* const kGotoSimp = L"前往简化";
} }

// ==== 新 UI P4·P：载入进度文案（有意偏差，无原版对应——原版载入为冻结式
//      沙漏，用户指定"设计转圈等待或进度条"）====
namespace newui { namespace load {
	const wchar_t* const kTitle     = L"正在载入 STL";
	const wchar_t* const kRead      = L"读取 STL 网格…";
	const wchar_t* const kCluster   = L"显示预处理（顶点聚类）…";
	const wchar_t* const kRender    = L"渲染首帧…";
	const wchar_t* const kCanceled   = L"载入已取消。";
} }

// ==== 新 UI P4·Q：网格简化进度文案（用户指定"应用简化"避免UI卡死——原版为
//      冻结式等待光标，登记有意偏差）====
namespace newui { namespace simpl {
	const wchar_t* const kTitle      = L"正在简化网格";
	const wchar_t* const kCollapsing = L"QEM 边折叠简化…";
	const wchar_t* const kCanceled   = L"简化已取消。";
} }

// ==== 新 UI P4·V：STEP 预览文案（用户指定"把 STEP 预览做进应用里"——
//      原版无 STEP 读入能力，登记有意偏差）====
namespace newui { namespace stepprev {
	// 载入进度对话框（读取阶段无进度回调接口，静态文案；转换阶段接进度指示器）
	const wchar_t* const kTitle    = L"正在载入 STEP 预览";
	const wchar_t* const kRead     = L"读取 STEP 文件…";
	const wchar_t* const kTransfer = L"转换 B-Rep 实体…";
	const wchar_t* const kRender   = L"渲染首帧…";
	// 输出窗格日志（STL 路径的取证文案"成功加载STL模型。"不动）
	const wchar_t* const kLoaded   = L"成功载入STEP预览（仅预览，不可简化/导出）。";
	const wchar_t* const kCanceled = L"载入已取消。";
	// 失败细分（与 STL 载入失败同型）
	const wchar_t* const kErrRead    = L"读取STEP模型文件失败！\n解析器报告：\n";
	const wchar_t* const kErrNoShape = L"读取STEP模型文件失败！\n文件中没有可转换的实体：\n";
	// 纯预览守卫（网格语义操作对 B-Rep 预览拒绝）
	const wchar_t* const kNoSimplify = L"当前为 STEP 预览（B-Rep 实体），网格简化仅适用于 STL 网格模型。";
	const wchar_t* const kNoExport   = L"当前为 STEP 预览（B-Rep 实体），仅支持预览查看；请载入 STL 网格模型后再导出。";
	// 任务窗格（简化状态行 / 预计面数行）
	const wchar_t* const kSimpState = L"STEP 预览（无网格）";
	const wchar_t* const kEstimate  = L"STEP 预览为 B-Rep 实体，无网格可简化。";
} }

// ==== 新 UI P4·B：品牌标题栏文案（设计稿 4.5；有意偏差，无原版对应）====
// 【P4·O】kBadge（"v2 概念稿"徽章）已按用户指定移除（2026-09-15，见补充三十五）
namespace newui { namespace brand {
	const wchar_t* const kAppName = L"STL→STEP 转换器";
	// 菜单项顺序须与 IDR_stl2stpcodeTYPE 弹出子菜单序一致（0 文件/1 视图/2 工具/3 帮助）
	const wchar_t* const kMenus[4] = { L"文件(F)", L"视图(V)", L"工具(T)", L"帮助(H)" };
} }

// ==== 新 UI P4·C：工具行文案（设计稿 4.6；有意偏差，无原版对应——原版为
//      纯图标工具栏无文字标签；下拉菜单为程序化构建，命令 ID 均原版语义）====
namespace newui { namespace toolStrip {
	const wchar_t* const kStep1 = L"载入模型";
	const wchar_t* const kStep2 = L"检查 / 简化";
	const wchar_t* const kStep3 = L"导出";
	const wchar_t* const kOpenStl     = L"打开 STL";
	const wchar_t* const kExportStep  = L"导出 STEP";
	const wchar_t* const kExportIges  = L"导出 IGES";
	const wchar_t* const kSimplify    = L"网格简化";
	const wchar_t* const kFit         = L"适应";
	const wchar_t* const kView        = L"视图";
	const wchar_t* const kDisplay     = L"显示";
	// 视图˅ 下拉（六视图 SetProj 语义 + 复位）
	const wchar_t* const kVFront = L"前视图";
	const wchar_t* const kVBack  = L"后视图";
	const wchar_t* const kVLeft  = L"左视图";
	const wchar_t* const kVRight = L"右视图";
	const wchar_t* const kVTop   = L"俯视图";
	const wchar_t* const kVBottom = L"仰视图";
	const wchar_t* const kVAxo   = L"轴测视图";
	const wchar_t* const kVReset = L"复位视图";
	// 显示˅ 下拉（效果族命令，复选态读文档渲染标志 m_bGrid/m_bRaytracing 等）
	const wchar_t* const kDGrid       = L"参考网格";
	const wchar_t* const kDHlrOn      = L"计算模式：开";
	const wchar_t* const kDHlrOff     = L"计算模式：关";
	const wchar_t* const kDRaytrace    = L"光线跟踪";
	const wchar_t* const kDShadow      = L"阴影";
	const wchar_t* const kDReflection  = L"反射";
	const wchar_t* const kDAntialias   = L"反走样";
} }
