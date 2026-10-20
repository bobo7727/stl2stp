#pragma once

// MeshSimplify.h - 基于 QEM（二次误差度量）的边折叠网格简化。
//
// 复刻原版 STL2STP 的 32837（网格简化，对话框 IDD=333 输入简化比例）：
// 原版 sub_14000FE20 用 OpenMesh decimater 实现；OpenCASCADE master 没有等价
// 算法（仅有 Poly_MergeNodesTool 节点合并），此处按同一语义自实现：
//   - theKeepRatio 为保留面片比例，范围 [0,1]；越小 → 简化后面片越少
//     （与原版对话框文案"简化比例越小，得到的STL模型中的面片数量越少"一致）；
//   - 目标面数 = round(theKeepRatio * 原面数)，边折叠按 QEM 误差从小到大贪心进行；
//   - 每次折叠做法向翻转校验（拒绝翻转边，中点回退仍翻转则跳过该边）。
//   - theProgress（可选）：折叠循环进度回调，入参为完成百分比 0-100
//     （按已折叠面数/目标折叠数计）；返回 false 请求中止，函数返回空句柄
//     （区别于 theSource 原样返回的"无需简化"，调用侧据此走取消路径，
//     原网格不受影响）。回调仅在百分比变化时触发（~1% 粒度）。

#include <Poly_Triangulation.hxx>
#include <functional>

opencascade::handle<Poly_Triangulation> SimplifyTriangulation(
	const opencascade::handle<Poly_Triangulation>& theSource,
	double theKeepRatio,
	const std::function<bool(int nPercent)>& theProgress
		= std::function<bool(int)>());
