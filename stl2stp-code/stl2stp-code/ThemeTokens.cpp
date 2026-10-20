#include "stdafx.h"
#include "ThemeTokens.h"
#include "stl2stp-code.h"

// ==== 视觉令牌双套（4.3.1 + 4.5.4）====
const STaskPal s_palLight = {
	RGB(255, 255, 255), RGB(212, 218, 229), RGB(27, 36, 51),   RGB(87, 98, 122),
	RGB(139, 149, 169), RGB(47, 111, 228),  RGB(31, 92, 214),  RGB(192, 124, 16),
	RGB(249, 242, 231), RGB(238, 241, 246),
	RGB(255, 255, 255), RGB(226, 230, 238), RGB(239, 243, 250) };
const STaskPal s_palDark = {
	RGB(27, 34, 48),    RGB(38, 46, 61),   RGB(231, 236, 244), RGB(154, 165, 184),
	RGB(95, 106, 125),  RGB(79, 140, 255), RGB(56, 120, 240),  RGB(232, 163, 61),
	RGB(58, 53, 50),    RGB(17, 21, 30),
	RGB(21, 26, 36),    RGB(31, 39, 52),   RGB(38, 48, 70) };

const STaskPal& TaskPal()
{
	return (theApp.GetInt(_T("DarkTheme"), 0) != 0) ? s_palDark : s_palLight;
}

// 静态自擦实底刷（与卡片表面同色）——按主题各缓存一支，进程生命周期内复用
HBRUSH s_brCard[2] = { NULL, NULL };
HBRUSH TaskCardBrush()
{
	const int i = (theApp.GetInt(_T("DarkTheme"), 0) != 0) ? 1 : 0;
	if (s_brCard[i] == NULL)
		s_brCard[i] = ::CreateSolidBrush(i != 0 ? s_palDark.crCard : s_palLight.crCard);
	return s_brCard[i];
}

// P4·D：窗格底实底刷（与 crPaneBg 同色）——文件/输出窗格的列表 CTLCOLOR
// 返回用；按主题各缓存一支，与 TaskCardBrush 同模式
HBRUSH s_brPaneBg[2] = { NULL, NULL };
HBRUSH TaskPaneBgBrush()
{
	const int i = (theApp.GetInt(_T("DarkTheme"), 0) != 0) ? 1 : 0;
	if (s_brPaneBg[i] == NULL)
		s_brPaneBg[i] = ::CreateSolidBrush(i != 0 ? s_palDark.crPaneBg : s_palLight.crPaneBg);
	return s_brPaneBg[i];
}
