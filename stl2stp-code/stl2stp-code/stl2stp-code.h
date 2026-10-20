
// stl2stp-code.h : stl2stp-code 应用程序的主头文件
//
#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含“stdafx.h”以生成 PCH 文件"
#endif

#include "resource.h"       // 主符号


// Cstl2stpcodeApp:
// 有关此类的实现，请参阅 stl2stp-code.cpp
//

class Cstl2stpcodeApp : public CWinAppEx
{
public:
	Cstl2stpcodeApp();


// 重写
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// 实现
	UINT  m_nAppLook;
	BOOL  m_bHiColorIcons;

	// 全局 OpenCASCADE OpenGL 图形驱动（在应用程序对象构造时创建，
	// 与原版 STL2STP 的 CVoxelModApp 行为一致）
	opencascade::handle<Graphic3d_GraphicDriver> m_hGraphicDriver;

	virtual void PreLoadState();
	virtual void LoadCustomState();
	virtual void SaveCustomState();

	afx_msg void OnAppAbout();
	DECLARE_MESSAGE_MAP()
};

extern Cstl2stpcodeApp theApp;
