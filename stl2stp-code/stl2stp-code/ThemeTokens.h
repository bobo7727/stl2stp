#pragma once

// ThemeTokens.h —— P4·B（设计稿 4.5.4）：视觉令牌共享单元。4.3 起令牌位于
// TaskPane.cpp 匿名命名空间，品牌标题栏（BrandBar）加入后提升为独立翻译单元。
// 主题探测与 MainFrm.cpp:66 同调用 theApp.GetInt(_T("DarkTheme"))——
// CWinAppEx::GetInt 经 m_strRegSection 默认读 Workspace 节（afxwinappex.cpp
// 源核实，无 SetRegistryBase 调用）；切换时 ApplyVisualTheme 落盘先于全局
// 重绘（4.3 时序修复），各消费者无状态现读即同步。

struct STaskPal
{
	// 卡片/文本/强调（4.3.1 定稿表）
	COLORREF crCard, crBorder, crText, crDim, crFaint, crAccent, crAccentStrong, crWarn, crWarnSoft, crPaneBg;
	// P4·B 品牌标题栏头部令牌（4.5.4）
	COLORREF crHeaderBg, crHeaderLine, crMenuHover;
};

const STaskPal& TaskPal();
HBRUSH TaskCardBrush();     // 静态自擦实底刷（与卡片表面同色；按主题各缓存一支）
HBRUSH TaskPaneBgBrush();   // P4·D：窗格底实底刷（与 crPaneBg 同色；列表 CTLCOLOR 返回用）
