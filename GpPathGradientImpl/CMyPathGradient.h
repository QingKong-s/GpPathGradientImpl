#pragma once
#include "eck/ECK.h"

struct CBmpDataWrapper
{
	void* m_pBits;
	int m_cx,
		m_cy,
		m_cbStride;

	constexpr BOOL IsCoordValid(int x, int y) const
	{
		return x >= 0 && x < m_cx && y >= 0 && y < m_cy;
	}

	constexpr DWORD& Pixel(int x, int y) const
	{
		return *(DWORD*)((BYTE*)m_pBits + (y * m_cbStride) + (x * sizeof(DWORD)));
	}
};

class CMyPathGradient
{
private:
	std::vector<GpPointF> m_vPt{};
	std::vector<ARGB> m_vColor{};

	GpPointF m_ptCenter{};
	ARGB m_crCenter{};

	EckInline void RasterizeTriangle(GpPointF* pt, ARGB* cr, const CBmpDataWrapper& bmp)
	{
		// 按Y坐标排序
		for (int i = 0; i < 2; ++i)
			for (int j = i + 1; j < 3; ++j)
				if (pt[i].Y > pt[j].Y)
				{
					std::swap(pt[i], pt[j]);
					std::swap(cr[i], cr[j]);
				}
		// 斜率的倒数
		float d0 = (pt[2].X - pt[1].X) / (pt[2].Y - pt[1].Y);
		float d1 = (pt[0].X - pt[2].X) / (pt[0].Y - pt[2].Y);
		float d2 = (pt[1].X - pt[0].X) / (pt[1].Y - pt[0].Y);
		for (int y = (int)pt[0].Y; y <= (int)pt[2].Y; ++y)
		{
			int xMin, xMax;
			// 计算相交的两边
			if (y < (int)pt[1].Y)// 与0-1相交
			{
				xMin = (int)((y - pt[0].Y) * d2 + pt[0].X);
				xMax = (int)((y - pt[0].Y) * d1 + pt[0].X);
				if (d1 == INFINITY)
					xMax = (int)pt[0].X;
				if (d2 == INFINITY)
					xMin = (int)pt[2].X;
			}
			else// 与1-2相交
			{
				xMin = (int)((y - pt[2].Y) * d0 + pt[2].X);
				xMax = (int)((y - pt[2].Y) * d1 + pt[2].X);
				if (d0 == INFINITY)
					xMin = (int)pt[1].X;
				if (d1 == INFINITY)
					xMax = (int)pt[2].X;
			}
			if (xMin > xMax)
				std::swap(xMin, xMax);

			for (int x = xMin; x <= xMax; ++x)
			{
				if (!bmp.IsCoordValid(x, y))
					continue;
				// 计算面积
				GpPointF vec0{ pt[0].X - x, pt[0].Y - y };
				GpPointF vec1{ pt[1].X - x, pt[1].Y - y };
				GpPointF vec2{ pt[2].X - x, pt[2].Y - y };

				float w0 = fabs(vec1.X * vec2.Y - vec1.Y * vec2.X);
				float w1 = fabs(vec0.X * vec2.Y - vec0.Y * vec2.X);
				float w2 = fabs(vec0.X * vec1.Y - vec0.Y * vec1.X);
				float wSum = w0 + w1 + w2;
				w0 /= wSum;
				w1 /= wSum;
				w2 /= wSum;

				const BYTE* pby0 = (const BYTE*)&cr[0];
				const BYTE* pby1 = (const BYTE*)&cr[1];
				const BYTE* pby2 = (const BYTE*)&cr[2];
				alignas(4) BYTE by3[4]
				{
					(BYTE)(w0 * pby0[0] + w1 * pby1[0] + w2 * pby2[0]),
					(BYTE)(w0 * pby0[1] + w1 * pby1[1] + w2 * pby2[1]),
					(BYTE)(w0 * pby0[2] + w1 * pby1[2] + w2 * pby2[2]),
					(BYTE)(w0 * pby0[3] + w1 * pby1[3] + w2 * pby2[3])
				};
				bmp.Pixel(x, y) = *(ARGB*)by3;
			}
		}
	}
public:
	// 创建自点集
	constexpr BOOL Create(const GpPointF* pPt, size_t cPt)
	{
		m_vPt.assign(pPt, pPt + cPt);
		m_vColor.resize(cPt);
		SetCenterPointAsMassCenter();
		return TRUE;
	}

	// 创建自路径
	BOOL Create(GpPath* pPath)
	{
		GpPathIterator* pIter{};
		GdipCreatePathIter(&pIter, pPath);
		BOOL bHasCurve;
		GdipPathIterHasCurve(pIter, &bHasCurve);
		int cPt;
		if (bHasCurve)// 若存在曲线则细分
		{
			GpPath* pNewPath;
			GdipClonePath(pPath, &pNewPath);
			GdipFlattenPath(pNewPath, nullptr, Gdiplus::FlatnessDefault);
			GdipDeletePathIter(pIter);
			GdipCreatePathIter(&pIter, pNewPath);

			GdipPathIterGetCount(pIter, &cPt);
			m_vPt.resize(cPt);
			m_vColor.resize(cPt);
			GdipGetPathPoints(pNewPath, m_vPt.data(), cPt);
			GdipDeletePath(pNewPath);
		}
		else
		{
			GdipPathIterGetCount(pIter, &cPt);
			m_vPt.resize(cPt);
			m_vColor.resize(cPt);
			GdipGetPathPoints(pPath, m_vPt.data(), cPt);
		}
		SetCenterPointAsMassCenter();
		GdipDeletePathIter(pIter);
		return TRUE;
	}

	/// <summary>
	/// 置周围颜色
	/// </summary>
	/// <param name="pColor">颜色数组</param>
	/// <param name="cColor">颜色数量，若小于当前点数量则空位使用最后一个颜色填充</param>
	void SetSurroundColor(const ARGB* pColor, size_t cColor)
	{
		if (cColor >= m_vColor.size())
			memcpy(m_vColor.data(), pColor, m_vColor.size() * sizeof(ARGB));
		else
		{
			memcpy(m_vColor.data(), pColor, cColor * sizeof(ARGB));
			for (size_t i = cColor; i < m_vColor.size(); i++)
				m_vColor[i] = pColor[cColor - 1];
		}
	}

	EckInline constexpr void SetCenterColor(ARGB cr)
	{
		m_crCenter = cr;
	}

	EckInline constexpr void SetCenterPoint(const GpPointF& pt)
	{
		m_ptCenter = pt;
	}

	constexpr void SetCenterPointAsMassCenter()
	{
		float xSum{}, ySum{};
		for (const auto& pt : m_vPt)
			xSum += pt.X, ySum += pt.Y;
		m_ptCenter.X = xSum / m_vPt.size();
		m_ptCenter.Y = ySum / m_vPt.size();
	}

	EckInline constexpr const auto& GetPoints() const { return m_vPt; }

	/// <summary>
	/// 光栅化
	/// </summary>
	/// <param name="pBits">位图数据，必须为32bppARGB</param>
	/// <param name="cx">宽度</param>
	/// <param name="cy">高度</param>
	/// <param name="cbStride">跨步</param>
	void Rasterize(void* pBits, int cx, int cy, UINT cbStride)
	{
		CBmpDataWrapper bmp{ pBits, cx, cy, cbStride };
		const auto& vPt = m_vPt;
		// 渐变三角信息
		GpPointF pt[3];
		ARGB cr[3];
		EckCounter(vPt.size(), i)
		{
			pt[0] = m_ptCenter;
			cr[0] = m_crCenter;
			// 顺次取三角形
			pt[1] = vPt[i];
			cr[1] = m_vColor[i];
			if (i == vPt.size() - 1)
			{
				pt[2] = vPt[0];
				cr[2] = m_vColor[0];
			}
			else ECKLIKELY
			{
				pt[2] = vPt[i + 1];
				cr[2] = m_vColor[i + 1];
			}
			RasterizeTriangle(pt, cr, bmp);
		}
	}
};