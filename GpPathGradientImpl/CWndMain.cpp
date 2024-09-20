#include "pch.h"

#include "CWndMain.h"
#include "CMyPathGradient.h"

void CWndMain::UpdateDpi(int iDpi)
{
	const int iDpiOld = m_iDpi;
	UpdateDpiInit(iDpi);

	// TODO: 更新字体大小
	HFONT hFont = eck::ReCreateFontForDpiChanged(m_hFont, iDpi, iDpiOld);
	eck::SetFontForWndAndCtrl(HWnd, hFont, FALSE);
	std::swap(m_hFont, hFont);
	DeleteObject(hFont);

	UpdateFixedUISize();
}

void CWndMain::UpdateFixedUISize()
{
	// TODO: 更新固定大小的控件

}

void CWndMain::ClearRes()
{
	DeleteObject(m_hFont);
}

GpPointF ptSimple[]
{
	{0, 0},
	{160, 0},
	{160, 200},
	{80, 150},
	{0, 200},
};

constexpr ARGB crSimple[]
{
	0xFFFF0000,
	0xFF00FF00,
	0xFF00FF00,
	0xFF0000FF,
	0xFFFF0000,
};

GpPointF ptStar[]
{
	{75, 0},
	{100, 50},
	{150, 50},
	{112, 75},
	{150, 150},
	{75, 100},
	{0, 150},
	{37, 75},
	{0, 50},
	{50, 50},
};

constexpr ARGB crStar[]
{
	eck::ColorrefToARGB(RGB(0, 0, 0)),
	eck::ColorrefToARGB(RGB(0, 255, 0)),
	eck::ColorrefToARGB(RGB(0, 0, 255)),
	eck::ColorrefToARGB(RGB(255, 255, 255)),
	eck::ColorrefToARGB(RGB(0, 0, 0)),
	eck::ColorrefToARGB(RGB(0, 255, 0)),
	eck::ColorrefToARGB(RGB(0, 0, 255)),
	eck::ColorrefToARGB(RGB(255, 255, 255)),
	eck::ColorrefToARGB(RGB(0, 0, 0)),
	eck::ColorrefToARGB(RGB(0, 255, 0)),
};

BOOL CWndMain::OnCreate(HWND hWnd, CREATESTRUCT* lpCreateStruct)
{
	eck::GetThreadCtx()->UpdateDefColor();

	UpdateDpiInit(eck::GetDpi(hWnd));
	m_hFont = eck::EzFont(L"微软雅黑", 9);
	constexpr auto cx = 900, cy = 700;

	m_DP.Create(nullptr, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,
		0, 0, cx, cy, hWnd, 0);

	GpBitmap* pBitmap;
	GdipCreateBitmapFromScan0(cx, cy, 0, PixelFormat32bppARGB, nullptr, &pBitmap);

	GpGraphics* pGraphics;
	GdipGetImageGraphicsContext(pBitmap, &pGraphics);
	GpSolidFill* pBrush;
	GdipCreateSolidFill(0xFFFFFFFF, &pBrush);
	GdipFillRectangle(pGraphics, pBrush, 0, 0, cx, cy);
	GdipDeleteBrush(pBrush);
	GdipDeleteGraphics(pGraphics);

	Gdiplus::BitmapData Data;
	const GpRect rc{ 0,0,cx,cy };
	GdipBitmapLockBits(pBitmap, &rc, Gdiplus::ImageLockModeRead | Gdiplus::ImageLockModeWrite,
		PixelFormat32bppARGB, &Data);

	// 绘制简单点集
	{
		for (auto& e : ptSimple)
		{
			e.X *= 2.f;
			e.Y *= 2.f;
		}
		CMyPathGradient pg{};
		pg.Create(ptSimple, ARRAYSIZE(ptSimple));
		pg.SetSurroundColor(crSimple, ARRAYSIZE(crSimple));
		pg.SetCenterColor(0xFFFFFFFF);
		pg.Rasterize(Data.Scan0, cx, cy, Data.Stride);
	}
	// 绘制五角星
	{
		for (auto& e : ptStar)
		{
			e.X *= 3;
			e.Y *= 3;

			e.X += 350;
		}
		CMyPathGradient pg{};
		pg.Create(ptStar, ARRAYSIZE(ptStar));
		pg.SetSurroundColor(crStar, ARRAYSIZE(crStar));
		pg.SetCenterColor(0xFFFF0000);
		pg.Rasterize(Data.Scan0, cx, cy, Data.Stride);
	}
	// 绘制椭圆
	{
		GpPath* pPath;
		GdipCreatePath(Gdiplus::FillModeAlternate, &pPath);
		GdipAddPathEllipse(pPath, 0, 450, 300, 200);
		constexpr ARGB cr{ 0xFF0000FF };
		CMyPathGradient pg{};
		pg.Create(pPath);
		pg.SetSurroundColor(&cr, 1);
		pg.SetCenterColor(0xFFFF0000);
		pg.Rasterize(Data.Scan0, cx, cy, Data.Stride);
	}
	GdipBitmapUnlockBits(pBitmap, &Data);

	const auto hDC = m_DP.GetHDC();

	GdipCreateFromHDC(hDC, &pGraphics);
	GdipDrawImageI(pGraphics, pBitmap, 0, 0);
	GdipDeleteGraphics(pGraphics);
	m_DP.Redraw();

	eck::SetFontForWndAndCtrl(hWnd, m_hFont);
	return TRUE;
}

CWndMain::~CWndMain()
{

}

LRESULT CWndMain::OnMsg(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
		return HANDLE_WM_CREATE(hWnd, wParam, lParam, OnCreate);
	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
			return 0;
		}
	}
	break;
	case WM_DESTROY:
		ClearRes();
		PostQuitMessage(0);
		return 0;
	case WM_SETTINGCHANGE:
	{
		if (eck::IsColorSchemeChangeMessage(lParam))
		{
			const auto ptc = eck::GetThreadCtx();
			ptc->UpdateDefColor();
			ptc->SetNcDarkModeForAllTopWnd(ShouldAppsUseDarkMode());
			ptc->SendThemeChangedToAllTopWindow();
			return 0;
		}
	}
	break;
	case WM_DPICHANGED:
		eck::MsgOnDpiChanged(hWnd, lParam);
		UpdateDpi(HIWORD(wParam));
		return 0;
	}
	return CForm::OnMsg(hWnd, uMsg, wParam, lParam);
}

BOOL CWndMain::PreTranslateMessage(const MSG& Msg)
{
	return CForm::PreTranslateMessage(Msg);
}

HWND CWndMain::Create(PCWSTR pszText, DWORD dwStyle, DWORD dwExStyle,
	int x, int y, int cx, int cy, HWND hParent, HMENU hMenu, PCVOID pData)
{
	IntCreate(dwExStyle, WCN_MAIN, pszText, dwStyle,
		x, y, cx, cy, hParent, hMenu, eck::g_hInstance, NULL);
	return NULL;
}