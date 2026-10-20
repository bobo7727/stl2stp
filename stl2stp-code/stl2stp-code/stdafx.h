
// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件

#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // 从 Windows 头中排除极少使用的资料
#endif

#include "targetver.h"

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // 某些 CString 构造函数将是显式的

// 关闭 MFC 对某些常见但经常可放心忽略的警告消息的隐藏
#define _AFX_ALL_WARNINGS

#include <afxwin.h>         // MFC 核心组件和标准组件
#include <afxext.h>         // MFC 扩展


#include <afxdisp.h>        // MFC 自动化类



#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h>           // MFC 对 Internet Explorer 4 公共控件的支持
#endif
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>             // MFC 对 Windows 公共控件的支持
#endif // _AFX_NO_AFXCMN_SUPPORT

#include <afxcontrolbars.h>     // 功能区和控件条的 MFC 支持

// MFC/GDI+ 头文件依赖 windows.h 的 min/max 宏，而 OCCT 头文件使用 std::min/std::max，
// 两者冲突。顺序：先让 MFC 头完整展开（保留宏），随后取消宏定义再引入 OCCT 头。
#undef min
#undef max

#include <algorithm>

// OpenCASCADE（最新版 master，源码 ..\OCCT-master，由 CMake 构建后提供 inc/ 与 TK*.lib。
// 不再使用手写 ABI 层 —— 所有对象按官方 API 正常构造）。
#include <Standard_Handle.hxx>
#include <TCollection_AsciiString.hxx>
#include <TCollection_HAsciiString.hxx>
#include <Quantity_Color.hxx>
#include <Graphic3d_GraphicDriver.hxx>
#include <Graphic3d_RenderingParams.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_Grid.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <WNT_Window.hxx>
#include <V3d_Viewer.hxx>
#include <V3d_RectangularGrid.hxx>
#include <V3d_View.hxx>
#include <AIS_InteractiveContext.hxx>
// 【方案 A】AIS_Manipulator.hxx 已移除（操纵器功能整体移除，WYSIWYG——
// 视口姿态编辑不被导出尊重；恢复方法见 AGENT.md 验收补充十）
#include <MeshVS_Mesh.hxx>
#include <MeshVS_DataSource.hxx>
#include <MeshVS_MeshPrsBuilder.hxx>
#include <MeshVS_DrawerAttribute.hxx>
#include <Poly_Triangulation.hxx>
#include <RWStl.hxx>
#include <TopoDS_Shape.hxx>









#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif


