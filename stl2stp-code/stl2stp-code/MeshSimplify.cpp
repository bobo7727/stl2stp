// MeshSimplify.cpp - QEM 边折叠网格简化（细节与语义见 MeshSimplify.h）。

#include "stdafx.h"
#include "MeshSimplify.h"

#include <vector>
#include <set>
#include <map>
#include <tuple>
#include <queue>
#include <cmath>
#include <limits>
#include <algorithm>

namespace
{
	// 对称 4x4 二次误差矩阵（QEM），按 10 元素存储：
	// q11 q12 q13 q14 / q12 q22 q23 q24 / q13 q23 q33 q34 / q14 q24 q34 q44
	struct Quadric
	{
		double v[10];
		Quadric() { for (int i = 0; i < 10; ++i) v[i] = 0.0; }
		Quadric& operator+=(const Quadric& r)
		{
			for (int i = 0; i < 10; ++i) v[i] += r.v[i];
			return *this;
		}
	};

	// 由三角片平面 (a,b,c,d)：ax+by+cz+d=0 构造平面二次误差矩阵
	Quadric MakePlaneQuadric(double a, double b, double c, double d)
	{
		Quadric q;
		q.v[0] = a * a; q.v[1] = a * b; q.v[2] = a * c; q.v[3] = a * d;
		q.v[4] = b * b; q.v[5] = b * c; q.v[6] = b * d;
		q.v[7] = c * c; q.v[8] = c * d;
		q.v[9] = d * d;
		return q;
	}

	// 误差 = (x,y,z,1) Q (x,y,z,1)^T
	double EvalQuadric(const Quadric& q, double x, double y, double z)
	{
		const double w[4] = { x, y, z, 1.0 };
		const double r0 = q.v[0]*w[0] + q.v[1]*w[1] + q.v[2]*w[2] + q.v[3]*w[3];
		const double r1 = q.v[1]*w[0] + q.v[4]*w[1] + q.v[5]*w[2] + q.v[6]*w[3];
		const double r2 = q.v[2]*w[0] + q.v[5]*w[1] + q.v[7]*w[2] + q.v[8]*w[3];
		const double r3 = q.v[3]*w[0] + q.v[6]*w[1] + q.v[8]*w[2] + q.v[9]*w[3];
		return w[0]*r0 + w[1]*r1 + w[2]*r2 + w[3]*r3;
	}

	struct Vec3
	{
		double x, y, z;
	};

	struct Candidate
	{
		double err;
		int a, b;
		bool operator>(const Candidate& r) const { return err > r.err; }
	};
}

opencascade::handle<Poly_Triangulation> SimplifyTriangulation(
	const opencascade::handle<Poly_Triangulation>& theSource,
	double theKeepRatio,
	const std::function<bool(int nPercent)>& theProgress)
{
	if (theSource.IsNull())
		return theSource;
	if (theKeepRatio < 0.0 || theKeepRatio > 1.0)
		return theSource;

	const int nSrcNodes = theSource->NbNodes();
	const int nSrcTris = theSource->NbTriangles();
	if (nSrcTris <= 0)
		return theSource;

	// ---- 1) 坐标去重，拓扑重建（与属性统计同一语义） ----
	std::map<std::tuple<double, double, double>, int> aPosToId;
	std::vector<int> anNodeToVert(nSrcNodes + 1, 0);
	std::vector<Vec3> aVerts;
	aVerts.push_back(Vec3{ 0, 0, 0 });   // 占位，顶点从 1 开始
	for (int i = 1; i <= nSrcNodes; ++i)
	{
		const gp_Pnt aP = theSource->Node(i);
		const auto aKey = std::make_tuple(aP.X(), aP.Y(), aP.Z());
		const auto aRes = aPosToId.emplace(aKey, static_cast<int>(aPosToId.size()) + 1);
		anNodeToVert[i] = aRes.first->second;
		if (aRes.second)
			aVerts.push_back(Vec3{ aP.X(), aP.Y(), aP.Z() });
	}

	struct Face { int v[3]; bool alive; };
	std::vector<Face> aFaces;
	aFaces.reserve(nSrcTris);
	for (int t = 1; t <= nSrcTris; ++t)
	{
		const Poly_Triangle aTri = theSource->Triangle(t);
		const int v1 = anNodeToVert[aTri.Value(1)];
		const int v2 = anNodeToVert[aTri.Value(2)];
		const int v3 = anNodeToVert[aTri.Value(3)];
		if (v1 == v2 || v2 == v3 || v3 == v1)
			continue;   // 退化三角
		aFaces.push_back(Face{ { v1, v2, v3 }, true });
	}
	const int nFaces = static_cast<int>(aFaces.size());
	if (nFaces == 0)
		return theSource;

	const int nTarget = static_cast<int>(theKeepRatio * nFaces + 0.5);
	if (nTarget >= nFaces)
		return theSource;   // 无需简化

	// ---- 2) 邻接与初始二次误差 ----
	const int nVerts = static_cast<int>(aVerts.size()) - 1;
	std::vector<std::set<int> > aVertFaces(nVerts + 1);
	std::vector<Quadric> aQ(nVerts + 1);
	for (int f = 0; f < nFaces; ++f)
	{
		const int v1 = aFaces[f].v[0], v2 = aFaces[f].v[1], v3 = aFaces[f].v[2];
		aVertFaces[v1].insert(f);
		aVertFaces[v2].insert(f);
		aVertFaces[v3].insert(f);
	}
	for (int f = 0; f < nFaces; ++f)
	{
		const Vec3& p1 = aVerts[aFaces[f].v[0]];
		const Vec3& p2 = aVerts[aFaces[f].v[1]];
		const Vec3& p3 = aVerts[aFaces[f].v[2]];
		const double ax = p2.x - p1.x, ay = p2.y - p1.y, az = p2.z - p1.z;
		const double bx = p3.x - p1.x, by = p3.y - p1.y, bz = p3.z - p1.z;
		double a = ay * bz - az * by, b = az * bx - ax * bz, c = ax * by - ay * bx;
		const double len = std::sqrt(a * a + b * b + c * c);
		if (len <= 0.0)
			continue;
		a /= len; b /= len; c /= len;
		const double d = -(a * p1.x + b * p1.y + c * p1.z);
		const Quadric fq = MakePlaneQuadric(a, b, c, d);
		aQ[aFaces[f].v[0]] += fq;
		aQ[aFaces[f].v[1]] += fq;
		aQ[aFaces[f].v[2]] += fq;
	}

	// ---- 3) 边折叠贪心（惰性优先队列，小根堆） ----
	auto EdgeExists = [&aFaces](int va, int vb, const std::set<int>& fa) {
		for (int f : fa)
		{
			if (!aFaces[f].alive) continue;
			const int w0 = aFaces[f].v[0], w1 = aFaces[f].v[1], w2 = aFaces[f].v[2];
			if ((w0 == va || w1 == va || w2 == va) && (w0 == vb || w1 == vb || w2 == vb))
				return true;
		}
		return false;
	};

	// 计算 (a,b) 的 QEM 最优折叠位置与误差：3x3 解失败或解超界时回退
	// 中点/端点，取误差最小者
	auto SolveCollapse = [&aVerts, &aQ](int a, int b, Vec3& outPos, double& outErr) {
		Quadric q = aQ[a];
		q += aQ[b];
		const double a11 = q.v[0], a12 = q.v[1], a13 = q.v[2];
		const double a22 = q.v[4], a23 = q.v[5], a33 = q.v[7];
		const double b1 = -q.v[3], b2 = -q.v[6], b3 = -q.v[8];
		const double det =
			a11 * (a22 * a33 - a23 * a23) -
			a12 * (a12 * a33 - a23 * a13) +
			a13 * (a12 * a23 - a22 * a13);
		double bestErr = std::numeric_limits<double>::max();
		Vec3 best{ (aVerts[a].x + aVerts[b].x) * 0.5,
		          (aVerts[a].y + aVerts[b].y) * 0.5,
		          (aVerts[a].z + aVerts[b].z) * 0.5 };
		if (std::fabs(det) > 1e-12)
		{
			const double idet = 1.0 / det;
			Vec3 pos;
			pos.x = idet * (b1 * (a22 * a33 - a23 * a23)
				- a12 * (b2 * a33 - a23 * b3)
				+ a13 * (b2 * a23 - a22 * b3));
			pos.y = idet * (a11 * (b2 * a33 - a23 * b3)
				- b1 * (a12 * a33 - a13 * a23)
				+ a13 * (a12 * b3 - b2 * a13));
			pos.z = idet * (a11 * (a22 * b3 - b2 * a23)
				- a12 * (a12 * b3 - b2 * a13)
				+ b1 * (a12 * a23 - a22 * a13));
			// 解须落在包围范围内，否则弃用（防数值爆炸）
			const double dmax = std::max(std::fabs(aVerts[a].x - aVerts[b].x),
				std::max(std::fabs(aVerts[a].y - aVerts[b].y),
					std::fabs(aVerts[a].z - aVerts[b].z)));
			if (std::fabs(pos.x - best.x) < dmax * 3.0 + 1.0 &&
				std::fabs(pos.y - best.y) < dmax * 3.0 + 1.0 &&
				std::fabs(pos.z - best.z) < dmax * 3.0 + 1.0)
			{
				const double e = EvalQuadric(q, pos.x, pos.y, pos.z);
				if (e < bestErr) { bestErr = e; best = pos; }
			}
		}
		const Vec3 mid = best;
		{
			const double em = EvalQuadric(q, mid.x, mid.y, mid.z);
			if (em < bestErr) { bestErr = em; best = mid; }
			const double ea = EvalQuadric(q, aVerts[a].x, aVerts[a].y, aVerts[a].z);
			if (ea < bestErr) { bestErr = ea; best = aVerts[a]; }
			const double eb = EvalQuadric(q, aVerts[b].x, aVerts[b].y, aVerts[b].z);
			if (eb < bestErr) { bestErr = eb; best = aVerts[b]; }
		}
		outPos = best;
		outErr = (bestErr > 0.0 ? bestErr : 0.0);
	};

	// 折叠合法性：受影响的相邻面（公共面除外）不允许法向翻转。
	// 旧法向取自折叠前的原顶点位置，新法向把 va/vb 替换为折叠点后计算，
	// 两者点积 <= 0 视为翻转/退化，拒绝折叠。（三种顶点位置统一比较）
	auto CollapseOK = [&aFaces, &aVerts](int va, int vb, const Vec3& pos,
		const std::set<int>& fa, const std::set<int>& fb) {
		std::set<int> all;
		for (int f : fa) if (aFaces[f].alive) all.insert(f);
		for (int f : fb) if (aFaces[f].alive) all.insert(f);
		for (int f : all)
		{
			const int w0 = aFaces[f].v[0], w1 = aFaces[f].v[1], w2 = aFaces[f].v[2];
			const bool hasA = (w0 == va || w1 == va || w2 == va);
			const bool hasB = (w0 == vb || w1 == vb || w2 == vb);
			if (hasA && hasB)
				continue;   // 公共面将被删除
			if (!hasA && !hasB)
				continue;
			// 旧法向（折叠前）
			const Vec3& A0 = aVerts[w0];
			const Vec3& B0 = aVerts[w1];
			const Vec3& C0 = aVerts[w2];
			const double oX = (B0.y - A0.y) * (C0.z - A0.z) - (B0.z - A0.z) * (C0.y - A0.y);
			const double oY = (B0.z - A0.z) * (C0.x - A0.x) - (B0.x - A0.x) * (C0.z - A0.z);
			const double oZ = (B0.x - A0.x) * (C0.y - A0.y) - (B0.y - A0.y) * (C0.x - A0.x);
			const double oLen2 = oX * oX + oY * oY + oZ * oZ;
			if (oLen2 <= 0.0)
				continue;   // 原本就是退化面，不再约束
			// 新法向（va/vb → pos）
			Vec3 N0 = A0, N1 = B0, N2 = C0;
			if (w0 == va || w0 == vb) N0 = pos;
			if (w1 == va || w1 == vb) N1 = pos;
			if (w2 == va || w2 == vb) N2 = pos;
			const double nX = (N1.y - N0.y) * (N2.z - N0.z) - (N1.z - N0.z) * (N2.y - N0.y);
			const double nY = (N1.z - N0.z) * (N2.x - N0.x) - (N1.x - N0.x) * (N2.z - N0.z);
			const double nZ = (N1.x - N0.x) * (N2.y - N0.y) - (N1.y - N0.y) * (N2.x - N0.x);
			if (oX * nX + oY * nY + oZ * nZ <= 0.0)
				return false;
		}
		return true;
	};

	std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate> > aHeap;
	auto PushEdgesAround = [&](int v) {
		for (int f : aVertFaces[v])
		{
			if (!aFaces[f].alive) continue;
			const int w0 = aFaces[f].v[0], w1 = aFaces[f].v[1], w2 = aFaces[f].v[2];
			const int pairs[3][2] = { { w0, w1 }, { w1, w2 }, { w2, w0 } };
			for (int k = 0; k < 3; ++k)
			{
				const int pa = pairs[k][0] < pairs[k][1] ? pairs[k][0] : pairs[k][1];
				const int pb = pairs[k][0] < pairs[k][1] ? pairs[k][1] : pairs[k][0];
				if (pa != v && pb != v) continue;   // 只压 v 的邻接边
				Vec3 pos; double err;
				SolveCollapse(pa, pb, pos, err);
				aHeap.push(Candidate{ err, pa, pb });
			}
		}
	};

	for (int v = 1; v <= nVerts; ++v)
		PushEdgesAround(v);

	// 法向翻转被拒的边延迟重试：压到“所有当前候选之后”，并按边计重试上限，
	// 保证终止（否则在有机曲面等高翻转率网格上会提前耗尽堆而达不到目标面数）
	std::map<std::pair<int, int>, int> aRejects;
	double gMaxErr = 0.0;

	int nFacesAlive = nFaces;
	int nCollapses = 0;
	const int nIterCap = (nFaces - nTarget) * 4 + 10000;
	// 进度回调状态：按已折叠面数/目标折叠数计百分比，~1% 粒度上报；
	// 回调返回 false（用户取消）→ 跳出循环，返回空句柄（见函数尾部）
	int nLastPct = -1;
	bool bAborted = false;
	while (nFacesAlive > nTarget && !aHeap.empty() && nCollapses < nIterCap)
	{
		const Candidate c = aHeap.top();
		aHeap.pop();
		const int va = c.a, vb = c.b;
		if (va == vb)
			continue;
		if (!EdgeExists(va, vb, aVertFaces[va]))
			continue;   // 已消失的边：直接丢弃（惰性堆的自然清理）

		Vec3 pos; double err;
		SolveCollapse(va, vb, pos, err);
		if (err > gMaxErr) gMaxErr = err;
		if (!CollapseOK(va, vb, pos, aVertFaces[va], aVertFaces[vb]))
		{
			const auto ekey = std::make_pair(va < vb ? va : vb, va < vb ? vb : va);
			auto rit = aRejects.find(ekey);
			const int nRej = (rit == aRejects.end() ? 0 : rit->second);
			if (nRej >= 16)
				continue;   // 放弃该边（重复重试仍翻转）
			if (rit == aRejects.end()) aRejects.emplace(ekey, 1);
			else ++rit->second;
			aHeap.push(Candidate{ gMaxErr + 1.0 + static_cast<double>(nRej), va, vb });
			continue;
		}

		// 执行折叠：vb 并入 va，位置取 QEM 最优点
		++nCollapses;
		aVerts[va] = pos;
		aQ[va] += aQ[vb];
		int nKilled = 0;
		for (int f : aVertFaces[vb])
		{
			if (!aFaces[f].alive) continue;
			Face& fc = aFaces[f];
			int nHits = 0;
			bool bMoved = false;
			for (int k = 0; k < 3; ++k)
			{
				if (fc.v[k] == vb) { fc.v[k] = va; ++nHits; bMoved = true; }
				else if (fc.v[k] == va) ++nHits;
			}
			if (nHits >= 2)
			{
				fc.alive = false;
				++nKilled;
			}
			else if (bMoved)
			{
				aVertFaces[va].insert(f);
			}
		}
		aVertFaces[vb].clear();
		nFacesAlive -= nKilled;

		// 进度上报 + 取消检查（百分比变化时才回调，避免逐折叠开销）
		if (theProgress && nKilled > 0)
		{
			const int nPct = (nFaces - nFacesAlive) * 100 / (nFaces - nTarget);
			if (nPct != nLastPct)
			{
				nLastPct = nPct;
				if (!theProgress(nPct))
				{
					bAborted = true;
					break;
				}
			}
		}

		// 重新压入 va 的邻接边（quadric/位置已更新）
		PushEdgesAround(va);
	}

	if (bAborted)
		return opencascade::handle<Poly_Triangulation>();   // 空 = 取消，原网格不动

	// ---- 4) 收集存活三角，重新编号输出 ----
	std::map<int, int> anOldToNew;
	std::vector<gp_Pnt> aNewNodes;
	std::vector<Poly_Triangle> aNewTris;
	for (int f = 0; f < nFaces; ++f)
	{
		if (!aFaces[f].alive) continue;
		int idx[3];
		for (int k = 0; k < 3; ++k)
		{
			const int v = aFaces[f].v[k];
			auto it = anOldToNew.find(v);
			if (it == anOldToNew.end())
			{
				const int newId = static_cast<int>(aNewNodes.size()) + 1;
				anOldToNew.emplace(v, newId);
				const Vec3& p = aVerts[v];
				aNewNodes.push_back(gp_Pnt(p.x, p.y, p.z));
				idx[k] = newId;
			}
			else
				idx[k] = it->second;
		}
		aNewTris.push_back(Poly_Triangle(idx[0], idx[1], idx[2]));
	}

	if (aNewTris.empty())
		return theSource;

	opencascade::handle<Poly_Triangulation> hResult =
		new Poly_Triangulation(static_cast<int>(aNewNodes.size()),
			static_cast<int>(aNewTris.size()), false, false);
	for (size_t i = 0; i < aNewNodes.size(); ++i)
		hResult->SetNode(static_cast<int>(i) + 1, aNewNodes[i]);
	for (size_t i = 0; i < aNewTris.size(); ++i)
		hResult->SetTriangle(static_cast<int>(i) + 1, aNewTris[i]);
	return hResult;
}
