
#include "StdafxFP.h"

////////////////////////////////////////////////////////////////////////////////
//// CTerrainMng ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool SortObjFrontToBack(CObj* pObj1, CObj* pObj2)
{
	if (pObj1->m_fDistCamera < pObj2->m_fDistCamera)
		return true;
	return false;
}

FLOAT CTerrainMng::m_fTileSize;
FLOAT CTerrainMng::m_fTextureSizeFactor;
int CTerrainMng::m_nMaxChunkSize;
LPDIRECT3DINDEXBUFFER9 CTerrainMng::m_pIB[LOD_CHUNK_RANGE_COUNT][LOD_CHUNK_EDIT_COUNT];

CTerrainMng::CTerrainMng()
{
	m_pChunk = NULL;
	m_pTexture = NULL;
	for (int i = 0; i < LOD_CHUNK_RANGE_COUNT; i++)
		for (int j = 0; j < LOD_CHUNK_EDIT_COUNT; j++)
			m_pIB[i][j] = NULL;

	m_nTerrainSize = 0;
	m_nMaxChunkSize = 0;

#ifdef __INCLUDE_WORLD_EDITOR
	m_nEditRange = -1;
#endif // __INCLUDE_WORLD_EDITOR
}

CTerrainMng::~CTerrainMng()
{
	DeleteDeviceObjects();
}

HRESULT CTerrainMng::InvalidateDeviceObjects()
{
	if (m_pChunk)
		m_pChunk->InvalidateDeviceObjects();

	for (UINT i = 0; i < m_vecObjsDynamic.size(); i++)
		m_vecObjsDynamic.Get(i)->InvalidateDeviceObjects();

	return S_OK;
}

HRESULT CTerrainMng::RestoreDeviceObjects()
{
	if (m_pChunk)
		m_pChunk->RestoreDeviceObjects();

	for (UINT i = 0; i < m_vecObjsDynamic.size(); i++)
		m_vecObjsDynamic.Get(i)->RestoreDeviceObjects();

	return S_OK;
}

HRESULT CTerrainMng::DeleteDeviceObjects(BOOL bDeleteIBs)
{
	m_vecVisibleObjs.RemoveAll(FALSE);

	if (m_pChunk)
	{
		m_pChunk->DeleteDeviceObjects();
		SAFE_DELETE(m_pChunk);
	}

	if (bDeleteIBs)
		DeleteIndexBuffers();

	g_nLoadedFaces -= m_nTerrainSize * m_nTerrainSize * 2;

	for (UINT i = 0; i < m_vecObjsDynamic.size(); i++)
		m_vecObjsDynamic.Get(i)->DeleteDeviceObjects();
	m_vecObjsDynamic.RemoveAll();

	return S_OK;
}

HRESULT CTerrainMng::DeleteIndexBuffers()
{
	for (int i = 0; i < LOD_CHUNK_RANGE_COUNT; i++)
		for (int j = 0; j < LOD_CHUNK_EDIT_COUNT; j++)
			SAFE_RELEASE(m_pIB[i][j]);
	return S_OK;
}

HRESULT CTerrainMng::Load(LPCTSTR szFileName)
{
	DeleteDeviceObjects(FALSE);

	const int nMaxChunkSizePrev = m_nMaxChunkSize;

	// TODO: load terrain data from world files
	// Only supports powers of 2 right now because of quad tree and LOD generation
	m_nTerrainSize = 1024; // 32 / 128 / 1024
	m_fTileSize = 1;
	m_fTextureSizeFactor = 8;
	m_nMaxChunkSize = 32;

	if (nMaxChunkSizePrev != m_nMaxChunkSize)
	{
		DeleteIndexBuffers();
		LoadIndexBuffers();
	}

	m_pChunk = new CTerrainChunk();
	m_pChunk->Load(szFileName, 0, 0, m_nTerrainSize);
	m_pChunk->LoadVertexBuffers();

	g_nLoadedFaces += m_nTerrainSize * m_nTerrainSize * 2;

	for (UINT i = 0; i < m_vecObjsDynamic.size(); i++)
		m_vecObjsDynamic.Get(i)->InitDeviceObjects();

	return S_OK;
}

HRESULT CTerrainMng::LoadIndexBuffers()
{
	int nIBSizeNear = m_nMaxChunkSize * m_nMaxChunkSize * 6 * sizeof(WORD);

	if (GetLODDivision(LOD_CHUNK_RANGE_COUNT - 1) >= m_nMaxChunkSize || // Prevent memory corruption in later calculations
		nIBSizeNear / (int)pow(GetLODDivision(LOD_CHUNK_RANGE_COUNT - 1), 2) -
		GetLODSubtraction(LOD_CHUNK_RANGE_COUNT - 1, LOD_CHUNK_EDIT_COUNT - 1) * 3 <= 0) // Prevent negative buffer sizes
	{
		LogError("Too many LOD ranges or chunk size too small, resulting in invalid index buffer size : Terrain");
		return E_FAIL;
	}

	for (int i = 0; i < LOD_CHUNK_RANGE_COUNT; i++)
	{
		for (int j = 0; j < LOD_CHUNK_EDIT_COUNT; j++)
		{
			if (FAILED(D3DDEVICE->CreateIndexBuffer(nIBSizeNear / (int)pow(GetLODDivision(i), 2) - GetLODSubtraction(i, j) * 3,
				D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &m_pIB[i][j], NULL)))
			{
				LogError("Failed to create index buffers : Terrain");
				return E_FAIL;
			}
		}
	}

	SetIndices();

	return S_OK;
}

void CTerrainMng::SetIndices()
{
	LPDIRECT3DINDEXBUFFER9 pIB;
	int iIB = 0;
	int iIBSkip = 0;
	BOOL bSwapTri = FALSE;
	WORD* pIBData;
	int nVertexWidth,
		botLeft, botRight, topLeft, topRight;
	BOOL bEdited,
		bTop, bRight, bBottom, bLeft;

	for (int i = 0; i < LOD_CHUNK_RANGE_COUNT; i++)
	{
		for (int j = 0; j < LOD_CHUNK_EDIT_COUNT; j++)
		{
			pIB = m_pIB[i][j];
			if (pIB)
			{
				iIB = 0;
				iIBSkip = GetLODDivision(i);
				pIB->Lock(0, 0, (void**)&pIBData, 0);
				nVertexWidth = m_nMaxChunkSize + 1;
				for (int x = 0; x < m_nMaxChunkSize; x += iIBSkip)
				{
					for (int z = 0; z < m_nMaxChunkSize; z += iIBSkip)
					{
						botLeft = x * nVertexWidth + z;
						botRight = (x + iIBSkip) * nVertexWidth + z;
						topLeft = x * nVertexWidth + (z + iIBSkip);
						topRight = (x + iIBSkip) * nVertexWidth + (z + iIBSkip);

						bSwapTri = (x / iIBSkip + z / iIBSkip) % 2 != 0;

						bEdited = FALSE;

						bLeft = x == 0;
						bRight = x == m_nMaxChunkSize - iIBSkip;
						bBottom = z == 0;
						bTop = z == m_nMaxChunkSize - iIBSkip;

						if (bLeft && LOD_CHUNK_EDIT_INCLUDES_L(j))
						{
							bEdited = TRUE;
							if (bSwapTri)
							{
								AddTriToIndexBuffer(pIBData, iIB, topLeft, botRight, botLeft - iIBSkip);
								if (!bTop || !LOD_CHUNK_EDIT_INCLUDES_LT(j))
									AddTriToIndexBuffer(pIBData, iIB, topLeft, topRight, botRight);
							}
							else if (!bBottom || !LOD_CHUNK_EDIT_INCLUDES_BL(j))
								AddTriToIndexBuffer(pIBData, iIB, botLeft, topRight, botRight);
						}

						if (bRight && LOD_CHUNK_EDIT_INCLUDES_R(j))
						{
							bEdited = TRUE;
							if (bSwapTri)
							{
								AddTriToIndexBuffer(pIBData, iIB, topLeft, topRight + iIBSkip, botRight);
								if (!bBottom || !LOD_CHUNK_EDIT_INCLUDES_RB(j))
									AddTriToIndexBuffer(pIBData, iIB, topLeft, botRight, botLeft);
							}
							else if (!bTop || !LOD_CHUNK_EDIT_INCLUDES_TR(j))
								AddTriToIndexBuffer(pIBData, iIB, topLeft, topRight, botLeft);
						}

						if (bBottom && LOD_CHUNK_EDIT_INCLUDES_B(j))
						{
							bEdited = TRUE;
							if (bSwapTri)
							{
								if (!bRight || !LOD_CHUNK_EDIT_INCLUDES_RB(j))
									AddTriToIndexBuffer(pIBData, iIB, topLeft, topRight, botRight);
							}
							else
							{
								AddTriToIndexBuffer(pIBData, iIB, botLeft, topRight, botRight + iIBSkip * nVertexWidth);
								if (!bLeft || !LOD_CHUNK_EDIT_INCLUDES_BL(j))
									AddTriToIndexBuffer(pIBData, iIB, topLeft, topRight, botLeft);
							}
						}

						if (bTop && LOD_CHUNK_EDIT_INCLUDES_T(j))
						{
							bEdited = TRUE;
							if (bSwapTri)
							{
								if (!bLeft || !LOD_CHUNK_EDIT_INCLUDES_LT(j))
									AddTriToIndexBuffer(pIBData, iIB, topLeft, botRight, botLeft);
							}
							else
							{
								AddTriToIndexBuffer(pIBData, iIB, botLeft, topLeft - iIBSkip * nVertexWidth, topRight);
								if (!bRight || !LOD_CHUNK_EDIT_INCLUDES_TR(j))
									AddTriToIndexBuffer(pIBData, iIB, botLeft, topRight, botRight);
							}
						}

						if (!bEdited)
						{
							if (bSwapTri)
							{
								AddTriToIndexBuffer(pIBData, iIB, topLeft, botRight, botLeft);
								AddTriToIndexBuffer(pIBData, iIB, topLeft, topRight, botRight);
							}
							else
							{
								AddTriToIndexBuffer(pIBData, iIB, topLeft, topRight, botLeft);
								AddTriToIndexBuffer(pIBData, iIB, botLeft, topRight, botRight);
							}
						}
					}
				}
				pIB->Unlock();
			}
		}
	}
}

void CTerrainMng::AddTriToIndexBuffer(WORD* pIBData, int& iIB, int i1, int i2, int i3)
{
	pIBData[iIB++] = i1;
	pIBData[iIB++] = i2;
	pIBData[iIB++] = i3;
}

#ifdef __INCLUDE_WORLD_EDITOR
void CTerrainMng::RenderEditRange()
{
	if (g_Client.GetWorldEditor()->GetEditorMode() != WORLD_EDITOR_TERRAIN || m_nEditRange < 0)
		return;

	g_Client.GetShader()->ResetWorldMatrix();

	g_Grp3D.BeginDebugRender();

	for (int x = -m_nEditRange; x < m_nEditRange + 1; x++)
	{
		for (int z = -m_nEditRange; z < m_nEditRange + 1; z++)
		{
			float fX = m_vEditTri[0].x + (x * m_fTileSize);
			float fZ = m_vEditTri[0].z + (z * m_fTileSize);
			int nX = (int)(fX / m_fTileSize);
			int nZ = (int)(fZ / m_fTileSize);

			if (!VertexOnTerrain(nX, nZ)) continue;

			float fY = GetVertexHeight(nX, nZ) + 0.1f;

			float fLength = D3DXVec3Length(&(Vector3((FLOAT)fX, m_vEditTri[0].y, (FLOAT)fZ) - m_vEditTri[0]));
			if (fLength > m_nEditRange * m_fTileSize) continue;

			Vector3 pTri[3];
			pTri[0] = Vector3(fX - 0.2f, fY, fZ - 0.2f);
			pTri[1] = Vector3(fX + 0.2f, fY, fZ + 0.2f);
			pTri[2] = Vector3(fX + 0.2f, fY, fZ - 0.2f);

			g_Grp3D.Render3DTri(pTri, COLOR_WHITE);
		}
	}

	g_Grp3D.EndDebugRender();
}
#endif // __INCLUDE_WORLD_EDITOR

void CTerrainMng::Render()
{
	g_Client.GetShader()->ResetWorldMatrix();
	g_Client.GetShader()->ResetParams();
	m_vecVisibleObjs.RemoveAll(FALSE);

	if(m_pChunk)
		m_pChunk->Render();
}

void CTerrainMng::RenderClean()
{
	if (m_pChunk)
		m_pChunk->RenderClean();

#ifdef __INCLUDE_WORLD_EDITOR
	RenderEditRange();
#endif // __INCLUDE_WORLD_EDITOR

	for (UINT i = 0; i < m_vecVisibleObjs.size(); i++)
		if (m_vecVisibleObjs[i])
			m_vecVisibleObjs[i]->RenderClean();
}

void CTerrainMng::RenderObjects()
{
	CObj* pObj = NULL;

	// Render dynamic objects
	for (UINT i = 0; i < m_vecObjsDynamic.size(); i++)
	{
		pObj = m_vecObjsDynamic.Get(i);

		if (pObj && pObj->m_pBB && !g_Client.GetCamera(g_Config.m_nCameraMode == 3, TRUE)->CullBoundingBox(pObj->m_pBB))
			m_vecVisibleObjs.Add(pObj);
	}

	// Calculate distance to camera for all visible objects
	Vector3 vPosCamera = g_Client.GetCamera(g_Config.m_nCameraMode == 3, TRUE)->m_vPosition;

	for (UINT i = 0; i < m_vecVisibleObjs.size(); i++)
	{
		pObj = m_vecVisibleObjs.Get(i);
		pObj->m_fDistCamera = D3DXVec3Length(&(pObj->m_vPosition - vPosCamera));
	}

	// Sort visible objects
	sort(m_vecVisibleObjs.begin(), m_vecVisibleObjs.end(), SortObjFrontToBack);

	// Render solid objects first (front to back)
	for (UINT i = 0; i < m_vecVisibleObjs.size(); i++)
	{
		pObj = m_vecVisibleObjs.Get(i);
		if (pObj->GetMaxAlphaState() == ALPHA_NONE)
			pObj->Render();
	}

	// Render transparent objects next (front to back)
	for (UINT i = 0; i < m_vecVisibleObjs.size(); i++)
	{
		pObj = m_vecVisibleObjs.Get(i);
		if (pObj->GetMaxAlphaState() == ALPHA_TEST)
			pObj->Render();
	}

	// Render translucent objects on top (back to front)
	for (UINT i = m_vecVisibleObjs.size(); i > 0; i--)
	{
		pObj = m_vecVisibleObjs.Get(i - 1);
		if (pObj->GetMaxAlphaState() == ALPHA_BLEND)
			pObj->Render();
	}
}

void CTerrainMng::FrameMove(const FLOAT& fTimeFactor)
{
	for (UINT i = 0; i < m_vecVisibleObjs.size(); i++)
		m_vecVisibleObjs.Get(i)->FrameMove(fTimeFactor); // TODO: continue animation times when culled
}

#ifdef __INCLUDE_WORLD_EDITOR
void CTerrainMng::ProcessEdit()
{
	if (g_Client.m_bMouseL)
	{
		int fMouseDeltaY = g_Client.m_ptMouseOld.y - g_Client.m_ptMouse.y;

		if (fMouseDeltaY != 0)
		{
			vector<CTerrainChunk*> pEditedChunks;

			for (int x = -m_nEditRange; x < m_nEditRange + 1; x++)
			{
				for (int z = -m_nEditRange; z < m_nEditRange + 1; z++)
				{
					float fX = m_vEditTri[0].x + (x * m_fTileSize);
					float fZ = m_vEditTri[0].z + (z * m_fTileSize);
					int nX = (int)(fX / m_fTileSize);
					int nZ = (int)(fZ / m_fTileSize);

					if (!VertexOnTerrain(nX, nZ)) continue;

					float fY = GetVertexHeight(nX, nZ);

					float fLength = D3DXVec3Length(&(Vector3((FLOAT)fX, m_vEditTri[0].y, (FLOAT)fZ) - m_vEditTri[0]));
					float fMaxLength = m_nEditRange * m_fTileSize;

					if (fLength > fMaxLength) continue;

					float fEditStrength = -pow(fLength, 2);
					fEditStrength += pow(fMaxLength, 2);
					fEditStrength *= fMouseDeltaY;
					fEditStrength /= 1000.0f;

					SetVertexHeight(nX, nZ, fY + fEditStrength, &pEditedChunks);
				}
			}

			g_Client.m_ptMouseOld.y = g_Client.m_ptMouse.y;

			for (UINT i = 0; i < pEditedChunks.size(); i++)
				pEditedChunks[i]->SetVertices();

			pEditedChunks.clear();
		}
	}
	else
	{
		Vector3 vPickRayOrig, vPickRayDir, vecIntersect;
		GetPickRay(g_Client.m_ptMouse, g_Client.GetCamera(TRUE), &vPickRayOrig, &vPickRayDir);
		IntersectRayTerrain(vPickRayOrig, vPickRayDir, g_Client.GetCamera(TRUE)->m_fDepth, &vecIntersect);
		GetTerrainTri(vecIntersect.x, vecIntersect.z, m_vEditTri);
	}
}
#endif // __INCLUDE_WORLD_EDITOR

BOOL CTerrainMng::VecOnTerrain(FLOAT fX, FLOAT fZ)
{
	return fX >= 0 && fX < m_nTerrainSize * m_nTerrainSize &&
		fZ >= 0 && fZ < m_nTerrainSize * m_nTerrainSize;
};

BOOL CTerrainMng::VertexOnTerrain(int nX, int nZ)
{
	return nX >= 0 && nX <= m_nTerrainSize &&
		nZ >= 0 && nZ <= m_nTerrainSize;
};

void CTerrainMng::GetTerrainTri(FLOAT fX, FLOAT fZ, Vector3* pTri)
{
	if (!VecOnTerrain(fX, fZ))
		return;

	m_pChunk->GetTerrainTri(fX, fZ, pTri);
}

FLOAT CTerrainMng::GetVertexHeight(int nX, int nZ)
{
	if (!VertexOnTerrain(nX, nZ))
		return 0.0f;

	return m_pChunk->GetVertexHeight(nX, nZ);
}

void CTerrainMng::SetVertexHeight(int nX, int nZ, FLOAT fY, vector<CTerrainChunk*>* pEditedChunks)
{
	if (!VertexOnTerrain(nX, nZ))
		return;

	m_pChunk->SetVertexHeight(nX, nZ, fY, pEditedChunks);
}

BOOL CTerrainMng::IntersectTerrain(Vector3 &vPickRayOrig, Vector3 &vPickRayDir, FLOAT fDist, Vector3* pIntersect)
{
	if (pIntersect == NULL)
		return FALSE;

	Vector3 pTri[3];
	GetTerrainTri(vPickRayOrig.x, vPickRayOrig.z, pTri);

	return IntersectTriangle(pTri[0], pTri[1], pTri[2], vPickRayOrig, vPickRayDir, pIntersect, &fDist);
}

BOOL CTerrainMng::IntersectRayTerrain(Vector3 &vPickRayOrig, Vector3 &vPickRayDir, FLOAT fDist, Vector3* pIntersect)
{
	if (pIntersect == NULL)
		return FALSE;

	Vector3 pTri[3];
	float fDistStep = m_fTileSize / 2;

	Vector3 vPickRayTest;
	BOOL bSwapToDest;

	for (int i = 0; i < (int)(fDist / fDistStep); i++)
	{
		vPickRayOrig += vPickRayDir * fDistStep;

		if (!VecOnTerrain(vPickRayOrig.x, vPickRayOrig.z))
			continue;

		vPickRayTest = vPickRayOrig;
		bSwapToDest = FALSE;

		for (float x = -fDistStep; x <= fDistStep; x += fDistStep)
		{
			for (float z = -fDistStep; z <= fDistStep; z += fDistStep)
			{
				GetTerrainTri(vPickRayTest.x + x, vPickRayTest.z + z, pTri);

				if (IntersectTriangle(pTri[0], pTri[1], pTri[2], vPickRayOrig, vPickRayDir, pIntersect, &fDistStep))
					return TRUE;

				if (!bSwapToDest && x == fDistStep && z == fDistStep)
				{
					bSwapToDest = TRUE;
					vPickRayTest = vPickRayOrig + vPickRayDir * fDistStep;
					x = -fDistStep;
					z = -fDistStep;
				}
			}
		}
	}

	return FALSE;
}

CObj* CTerrainMng::IntersectObjects(Vector3 &vPickRayOrig, Vector3 &vPickRayDir, FLOAT fDist, Vector3* pIntersect)
{
	if (!pIntersect)
		return NULL;

	Vector3 vIntersect;
	FLOAT fRayLength = D3DXVec3Length(&(vPickRayDir * fDist));
	FLOAT fLength, fClosestLength = fRayLength;
	CObj* pObjIntersect = NULL;
	BoundingBox* pBB;
	FLOAT fBBRadius;
	Vector3 vClosestPtOnRay, vPickRayDest, vBBCenter;

	for (UINT i = 0; i < m_vecVisibleObjs.size(); i++)
	{
		// Skip if object or boundingbox is invalid
		if (!m_vecVisibleObjs[i] || !m_vecVisibleObjs[i]->m_pBB)
			continue;

		pBB = m_vecVisibleObjs[i]->m_pBB;

		// Skip if ray has no chance of hitting the boundingbox
		vBBCenter = pBB->GetCenter();
		fBBRadius = pBB->GetRadius();
		vPickRayDest = vPickRayOrig + vPickRayDir * fDist;
		vClosestPtOnRay = GetClosestPointOnRay(vBBCenter, vPickRayOrig, vPickRayDest);

		if (D3DXVec3Length(&(vBBCenter - vClosestPtOnRay)) > fBBRadius ||
			D3DXVec3Length(&(vClosestPtOnRay - vPickRayOrig)) > fRayLength + fBBRadius ||
			D3DXVec3Length(&(vClosestPtOnRay - vPickRayDest)) > fRayLength + fBBRadius)
			continue;

		// If intersection found, find point closest to ray origin
		if (IntersectRayBoundingBox(pBB, vPickRayOrig, vPickRayDir, fDist, &vIntersect, TRUE))
		{
			fLength = D3DXVec3Length(&(vIntersect - vPickRayOrig));
			if (fLength < fClosestLength)
			{
				fClosestLength = fLength;
				*pIntersect = vIntersect;
				pObjIntersect = m_vecVisibleObjs[i];
			}
		}
	}

	return pObjIntersect;
}

void CTerrainMng::UpdateBoundingBox(CTerrainChunk* pChunk)
{
	if (m_pChunk)
		m_pChunk->UpdateBoundingBox(pChunk);
}

BOOL CTerrainMng::AddObject(CObj* pObj, BOOL bFront)
{
	if (!pObj || !VecOnTerrain(pObj->m_vPosition.x, pObj->m_vPosition.z))
		return FALSE;

	if (pObj->m_objectType == OBJECT_TYPE_DYNAMIC)
	{
		m_vecObjsDynamic.Add(pObj, bFront);
		return TRUE;
	}
	else if (pObj->m_objectType == OBJECT_TYPE_STATIC)
		return m_pChunk->AddObject(pObj, bFront);

	return FALSE;
}

BOOL CTerrainMng::RemoveObject(CObj* pObj, BOOL bDelete)
{
	if (!pObj)
		return FALSE;

	if (pObj->m_pChunk)
		return pObj->m_pChunk->RemoveObject(pObj, bDelete);

	BOOL bRemoved = m_vecObjsDynamic.Remove(pObj, bDelete);
	if (bRemoved)
		m_vecVisibleObjs.Remove(pObj, FALSE);
	return bRemoved;
}

int CTerrainMng::GetLODDivision(int nLODRange)
{
	return nLODRange == 0 ? 1 : (int)pow(2, nLODRange);
}

int CTerrainMng::GetLODSubtraction(int nLODRange, int nLODEdit)
{
	return ((int)((nLODEdit + 3) / 4) * (m_nMaxChunkSize / 2)) / GetLODDivision(nLODRange);
}

LOD_CHUNK_RANGE CTerrainMng::GetLODRange(int nX, int nZ)
{
	CCamera* pCamera = g_Client.GetCamera(g_Config.m_nCameraMode == 3);

	Vector3 vCamera = pCamera->m_vPosition;
	Vector3 vChunkCenter((nX + m_nMaxChunkSize / 2) * m_fTileSize,
		vCamera.y, (nZ + m_nMaxChunkSize / 2) * m_fTileSize);
	FLOAT fDistance = D3DXVec3Length(&(vChunkCenter - vCamera));

	// Max distance checked is camera depth-1.0f to make sure that the result doesn't overflow the enum
	return (LOD_CHUNK_RANGE)(int)(min(fDistance, pCamera->m_fDepth - 1.0f) / (pCamera->m_fDepth / LOD_CHUNK_RANGE_COUNT));
}

LOD_CHUNK_EDIT CTerrainMng::GetLODEdit(int nX, int nZ, int nLODRange)
{
	BOOL bTop = FALSE, bRight = FALSE, bBottom = FALSE, bLeft = FALSE;

	if (GetLODRange(nX, nZ + m_nMaxChunkSize) > nLODRange) bTop = TRUE;
	if (GetLODRange(nX + m_nMaxChunkSize, nZ) > nLODRange) bRight = TRUE;
	if (GetLODRange(nX, nZ - m_nMaxChunkSize) > nLODRange) bBottom = TRUE;
	if (GetLODRange(nX - m_nMaxChunkSize, nZ) > nLODRange) bLeft = TRUE;

	if (bTop && bRight && bBottom) return LOD_CHUNK_EDIT_TRB;
	else if (bRight && bBottom && bLeft) return LOD_CHUNK_EDIT_RBL;
	else if (bBottom && bLeft && bTop) return LOD_CHUNK_EDIT_BLT;
	else if (bLeft && bTop && bRight) return LOD_CHUNK_EDIT_LTR;
	else if (bTop && bRight) return LOD_CHUNK_EDIT_TR;
	else if (bRight && bBottom) return LOD_CHUNK_EDIT_RB;
	else if (bBottom && bLeft) return LOD_CHUNK_EDIT_BL;
	else if (bLeft && bTop) return LOD_CHUNK_EDIT_LT;
	else if (bTop) return LOD_CHUNK_EDIT_T;
	else if (bRight) return LOD_CHUNK_EDIT_R;
	else if (bBottom) return LOD_CHUNK_EDIT_B;
	else if (bLeft) return LOD_CHUNK_EDIT_L;
	else return LOD_CHUNK_EDIT_N;
}

////////////////////////////////////////////////////////////////////////////////
//// CTerrainChunk /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

CTerrainChunk::CTerrainChunk()
{
	m_nX = 0;
	m_nZ = 0;
	m_nChunkSize = 0;
	m_pChunks = NULL;
	m_pVB = NULL;
	m_pHeightMap = NULL;
	m_pBB = NULL;
}

CTerrainChunk::~CTerrainChunk()
{
	DeleteDeviceObjects();

	g_nLoadedChunks--;
}

HRESULT CTerrainChunk::InvalidateDeviceObjects()
{
	if (m_pChunks)
		for (int i = 0; i < 4; i++)
			m_pChunks[i].InvalidateDeviceObjects();

	for (UINT i = 0; i < m_vecObjsStatic.size(); i++)
		m_vecObjsStatic.Get(i)->InvalidateDeviceObjects();

	return S_OK;
}

HRESULT CTerrainChunk::RestoreDeviceObjects()
{
	if (m_pChunks)
		for (int i = 0; i < 4; i++)
			m_pChunks[i].RestoreDeviceObjects();

	for (UINT i = 0; i < m_vecObjsStatic.size(); i++)
		m_vecObjsStatic.Get(i)->RestoreDeviceObjects();

	return S_OK;
}

HRESULT CTerrainChunk::DeleteDeviceObjects()
{
	SAFE_RELEASE(m_pVB);

	if (m_pChunks)
	{
		for (int i = 0; i < 4; i++)
			m_pChunks[i].DeleteDeviceObjects();
		SAFE_DELETE_ARRAY(m_pChunks);
	}

	if (m_pHeightMap)
	{
		for (int i = 0; i < m_nChunkSize + 1; i++)
			SAFE_DELETE_ARRAY(m_pHeightMap[i]);
		SAFE_DELETE_ARRAY(m_pHeightMap);
	}

	SAFE_DELETE(m_pBB);

	for (UINT i = 0; i < m_vecObjsStatic.size(); i++)
		m_vecObjsStatic.Get(i)->DeleteDeviceObjects();
	m_vecObjsStatic.RemoveAll();

	return S_OK;
}

HRESULT CTerrainChunk::Load(LPCTSTR szFileName, int nX, int nZ, int nChunkSize)
{
	DeleteDeviceObjects();

	m_pBB = new BoundingBox();
	if (!m_pBB)
		return E_FAIL;

	m_nX = nX;
	m_nZ = nZ;
	m_nChunkSize = nChunkSize;

	if (m_nChunkSize > CTerrainMng::m_nMaxChunkSize)
	{
		int nChunkSizeHalf = m_nChunkSize / 2;

		m_pChunks = new CTerrainChunk[4];

		m_pChunks[0].Load(szFileName, m_nX, m_nZ, nChunkSizeHalf);
		m_pChunks[1].Load(szFileName, m_nX + nChunkSizeHalf, m_nZ, nChunkSizeHalf);
		m_pChunks[2].Load(szFileName, m_nX, m_nZ + nChunkSizeHalf, nChunkSizeHalf);
		m_pChunks[3].Load(szFileName, m_nX + nChunkSizeHalf, m_nZ + nChunkSizeHalf, nChunkSizeHalf);

		UpdateBoundingBoxToChildren();
	}
	else
	{
		m_pHeightMap = new FLOAT*[m_nChunkSize + 1];
		for (int i = 0; i < m_nChunkSize + 1; i++)
			m_pHeightMap[i] = new FLOAT[m_nChunkSize + 1];

		for (int i = 0; i < m_nChunkSize + 1; i++)
			for (int j = 0; j < m_nChunkSize + 1; j++)
				m_pHeightMap[i][j] = 0;// (FLOAT)PerlinNoise_2D((double)(m_nX + i), (double)(m_nZ + j), 9, 0.45, 1) * 400.0f; // 7 0.45 0 50, 7 0.45 0 100, 8 0.45 0 200, 9 0.45 1 400

		UpdateBoundingBox();
	}

	// TODO: load objects from world files

	for (UINT i = 0; i < m_vecObjsStatic.size(); i++)
		m_vecObjsStatic.Get(i)->InitDeviceObjects();

	g_nLoadedChunks++;

	return S_OK;
}

HRESULT CTerrainChunk::LoadVertexBuffers()
{
	if (m_pChunks)
	{
		for (int i = 0; i < 4; i++)
			m_pChunks[i].LoadVertexBuffers();
	}
	else
	{
		if (FAILED(D3DDEVICE->CreateVertexBuffer((m_nChunkSize + 1) * (m_nChunkSize + 1) * sizeof(D3DTERRAINVERTEX),
			D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &m_pVB, NULL)))
		{
			LogError("Failed to create vertex buffers : Terrain");
			return E_FAIL;
		}

		SetVertices();
	}

	return S_OK;
}

void CTerrainChunk::RenderGrid()
{
	g_Grp3D.BeginDebugRender();

	int nSecond = 0;
	for (int x = 0; x < m_nChunkSize; x++)
	{
		g_Grp3D.Render3DLine(
			Vector3(
				m_nX*CTerrainMng::m_fTileSize + x*CTerrainMng::m_fTileSize,
				m_pHeightMap[x][nSecond] + 0.1f,
				m_nZ*CTerrainMng::m_fTileSize + nSecond*CTerrainMng::m_fTileSize),
			Vector3(
				m_nX*CTerrainMng::m_fTileSize + (x + 1)*CTerrainMng::m_fTileSize,
				m_pHeightMap[x + 1][nSecond] + 0.1f,
				m_nZ*CTerrainMng::m_fTileSize + nSecond*CTerrainMng::m_fTileSize),
			COLOR_WHITE);

		if (x == m_nChunkSize - 1 && nSecond == 0)
		{
			x = -1;
			nSecond = m_nChunkSize;
		}
	}

	nSecond = 0;
	for (int z = 0; z < m_nChunkSize; z++)
	{
		g_Grp3D.Render3DLine(
			Vector3(
				m_nX*CTerrainMng::m_fTileSize + nSecond*CTerrainMng::m_fTileSize,
				m_pHeightMap[nSecond][z] + 0.1f,
				m_nZ*CTerrainMng::m_fTileSize + z*CTerrainMng::m_fTileSize),
			Vector3(
				m_nX*CTerrainMng::m_fTileSize + nSecond*CTerrainMng::m_fTileSize,
				m_pHeightMap[nSecond][z + 1] + 0.1f,
				m_nZ*CTerrainMng::m_fTileSize + (z + 1)*CTerrainMng::m_fTileSize),
			COLOR_WHITE);

		if (z == m_nChunkSize - 1 && nSecond == 0)
		{
			z = -1;
			nSecond = m_nChunkSize;
		}
	}

	g_Grp3D.EndDebugRender();
}

void CTerrainChunk::Render()
{
	if (!g_Client.GetCamera(g_Config.m_nCameraMode == 3, TRUE)->CullBoundingBox(m_pBB))
	{
		if (m_pChunks)
		{
			for (int i = 0; i < 4; i++)
				m_pChunks[i].Render();
		}
		else if(g_Client.GetWorld()->GetTerrainMng()->m_pTexture)
		{
			g_Client.GetShader()->ResetWorldMatrix();

			int nLODRange = CTerrainMng::GetLODRange(m_nX, m_nZ);
			int nLODEdit = CTerrainMng::GetLODEdit(m_nX, m_nZ, nLODRange);
			int nLODDivision = (int)pow(CTerrainMng::GetLODDivision(nLODRange), 2);
			int nLODSubtraction = CTerrainMng::GetLODSubtraction(nLODRange, nLODEdit);

			D3DDEVICE->SetFVF(D3DFVF_D3DTERRAINVERTEX);
			D3DDEVICE->SetTexture(0, g_Client.GetWorld()->GetTerrainMng()->m_pTexture->GetTexture());
			D3DDEVICE->SetStreamSource(0, m_pVB, 0, sizeof(D3DTERRAINVERTEX));
			D3DDEVICE->SetIndices(CTerrainMng::m_pIB[nLODRange][nLODEdit]);
			D3DDEVICE->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, (m_nChunkSize + 1) * (m_nChunkSize + 1),
				0, (m_nChunkSize * m_nChunkSize * 2) / nLODDivision - nLODSubtraction);
			D3DDEVICE->SetTexture(0, NULL);

			g_nVisibleFaces += (m_nChunkSize * m_nChunkSize * 2) / nLODDivision - nLODSubtraction;
		}

		CObj* pObj = NULL;
		for (UINT i = 0; i < m_vecObjsStatic.size(); i++)
		{
			pObj = m_vecObjsStatic.Get(i);

			if (pObj && pObj->m_pBB && !g_Client.GetCamera(g_Config.m_nCameraMode == 3, TRUE)->CullBoundingBox(pObj->m_pBB))
				g_Client.GetWorld()->GetTerrainMng()->m_vecVisibleObjs.Add(pObj);
		}

		g_nVisibleChunks++;
	}
}

void CTerrainChunk::RenderClean()
{
	if ((g_Config.m_bRenderBoundingBox || g_Config.m_bRenderGrid) &&
		!g_Client.GetCamera(g_Config.m_nCameraMode == 3, TRUE)->CullBoundingBox(m_pBB))
	{
		if (m_pChunks)
		{
			for (int i = 0; i < 4; i++)
				m_pChunks[i].RenderClean();
		}

		g_Client.GetShader()->ResetWorldMatrix();

		if (g_Config.m_bRenderGrid && !m_pChunks)
			RenderGrid();

		if (g_Config.m_bRenderBoundingBox && m_pBB)
		{
			g_Grp3D.BeginDebugRender();
			g_Grp3D.RenderCube(m_pBB->vectors, COLOR_WHITE);
			g_Grp3D.EndDebugRender();
		}
	}
}

void CTerrainChunk::SetVertices()
{
	if (m_pVB)
	{
		FLOAT fTileSize = CTerrainMng::m_fTileSize;
		FLOAT fTextureSizeFactor = CTerrainMng::m_fTextureSizeFactor;

		D3DTERRAINVERTEX* pVB;
		int nV = 0;
		FLOAT fTemp = 1.0f / m_nChunkSize;
		FLOAT fTemp2 = fTemp*(fTileSize - 1) / m_nChunkSize;

		m_pVB->Lock(0, 0, (void**)&pVB, 0);
		for (int x = 0; x < m_nChunkSize + 1; x++)
		{
			for (int z = 0; z < m_nChunkSize + 1; z++)
			{
				pVB[nV].p = Vector3(m_nX*fTileSize + x*fTileSize,
					m_pHeightMap[x][z], m_nZ*fTileSize + z*fTileSize);
				Vector3 pTri[3];
				if (x == m_nChunkSize || z == m_nChunkSize) // For the last vertex row we need tris from the next chunk
					g_Client.GetWorld()->GetTerrainMng()->GetTerrainTri(pVB[nV].p.x, pVB[nV].p.z, pTri);
				else
					GetTerrainTri(pVB[nV].p.x, pVB[nV].p.z, pTri);
				Vector3 vec1 = pTri[2] - pTri[0];
				Vector3 vec2 = pTri[2] - pTri[1];
				D3DXVec3Cross(&pVB[nV].n, &vec1, &vec2);
				D3DXVec3Normalize(&pVB[nV].n, &pVB[nV].n);
				pVB[nV].tu1 = (float)((float)((FLOAT)x) / fTileSize) / fTextureSizeFactor;
				pVB[nV].tv1 = (float)((float)((FLOAT)z) / fTileSize) / fTextureSizeFactor;
				pVB[nV].tu2 = (float)(fTemp2*(x*fTileSize) + (fTemp / 2)) / fTextureSizeFactor;
				pVB[nV].tv2 = (float)(fTemp2*(z*fTileSize) + (fTemp / 2)) / fTextureSizeFactor;
				nV++;
			}
		}
		m_pVB->Unlock();
	}
}

void CTerrainChunk::GetTerrainTri(FLOAT fX, FLOAT fZ, Vector3* pTri)
{
	FLOAT fTileSize = CTerrainMng::m_fTileSize;

	int nXX = (int)(fX / fTileSize);
	int nZZ = (int)(fZ / fTileSize);

	if (m_pChunks)
	{
		for (int i = 0; i < 4; i++)
		{
			if (m_pChunks[i].VertexOnChunk(nXX, nZZ))
			{
				m_pChunks[i].GetTerrainTri(fX, fZ, pTri);
				return;
			}
		}
	}
	else if (VertexOnChunk(nXX, nZZ))
	{
		FLOAT fVX = nXX*fTileSize;
		FLOAT fVZ = nZZ*fTileSize;

		nXX -= m_nX;
		nZZ -= m_nZ;

		if ((nXX + nZZ) % 2 == 0)
		{
			if (fX - fVX <= fZ - fVZ)
			{
				pTri[0] = { fVX, m_pHeightMap[nXX][nZZ], fVZ };
				pTri[1] = { fVX, m_pHeightMap[nXX][nZZ + 1], fVZ + fTileSize };
				pTri[2] = { fVX + fTileSize, m_pHeightMap[nXX + 1][nZZ + 1], fVZ + fTileSize };
			}
			else
			{
				pTri[0] = { fVX, m_pHeightMap[nXX][nZZ], fVZ };
				pTri[1] = { fVX + fTileSize, m_pHeightMap[nXX + 1][nZZ + 1], fVZ + fTileSize };
				pTri[2] = { fVX + fTileSize, m_pHeightMap[nXX + 1][nZZ], fVZ };
			}
		}
		else
		{
			if (fTileSize - (fX - fVX) > fZ - fVZ)
			{
				pTri[0] = { fVX + fTileSize, m_pHeightMap[nXX + 1][nZZ], fVZ };
				pTri[1] = { fVX, m_pHeightMap[nXX][nZZ], fVZ };
				pTri[2] = { fVX, m_pHeightMap[nXX][nZZ + 1], fVZ + fTileSize };
			}
			else
			{
				pTri[0] = { fVX + fTileSize, m_pHeightMap[nXX + 1][nZZ], fVZ };
				pTri[1] = { fVX, m_pHeightMap[nXX][nZZ + 1], fVZ + fTileSize };
				pTri[2] = { fVX + fTileSize, m_pHeightMap[nXX + 1][nZZ + 1], fVZ + fTileSize };
			}
		}
	}
}

BOOL CTerrainChunk::VecOnChunk(FLOAT fX, FLOAT fZ)
{
	FLOAT fTileSize = CTerrainMng::m_fTileSize;
	return fX >= m_nX * fTileSize &&
		fX < m_nX * fTileSize + m_nChunkSize * fTileSize &&
		fZ >= m_nZ * fTileSize &&
		fZ < m_nZ * fTileSize + m_nChunkSize * fTileSize;
};

BOOL CTerrainChunk::BoundingBoxOnChunk(BoundingBox* pBB)
{
	if (pBB)
	{
		for (int i = 0; i < 8; i++)
			if (!VecOnChunk(pBB->vectors[i].x, pBB->vectors[i].z))
				return FALSE;
		return TRUE;
	}
	return FALSE;
}

BOOL CTerrainChunk::VertexOnChunk(int nX, int nZ, bool bHeightMap)
{
	if (nX >= m_nX && nX < m_nX + m_nChunkSize + (bHeightMap ? 1 : 0) &&
		nZ >= m_nZ && nZ < m_nZ + m_nChunkSize + (bHeightMap ? 1 : 0))
		return TRUE;
	return FALSE;
}

FLOAT CTerrainChunk::GetVertexHeight(int nX, int nZ)
{
	if (m_pChunks)
	{
		for (int i = 0; i < 4; i++)
		{
			if (m_pChunks[i].VertexOnChunk(nX, nZ, TRUE))
				return m_pChunks[i].GetVertexHeight(nX, nZ);
		}
	}
	else if (VertexOnChunk(nX, nZ, TRUE))
		return m_pHeightMap[nX - m_nX][nZ - m_nZ];

	return 0.0f;
}

void CTerrainChunk::SetVertexHeight(int nX, int nZ, FLOAT fY, vector<CTerrainChunk*>* pEditedChunks, BOOL bUpdateBoundingBox)
{
	if (m_pChunks)
	{
		for (int i = 0; i < 4; i++)
		{
			if (m_pChunks[i].VertexOnChunk(nX, nZ, TRUE))
				m_pChunks[i].SetVertexHeight(nX, nZ, fY, pEditedChunks); // No return here, edit duplicated vertices too (chunk edge connections)
		}

		if (bUpdateBoundingBox)
			UpdateBoundingBoxToChildren();
	}
	else if (VertexOnChunk(nX, nZ, TRUE))
	{
		if (m_pHeightMap[nX - m_nX][nZ - m_nZ] != fY)
		{
			m_pHeightMap[nX - m_nX][nZ - m_nZ] = fY;

			if (bUpdateBoundingBox)
				UpdateBoundingBox();

			if (pEditedChunks && find(pEditedChunks->begin(), pEditedChunks->end(), this) == pEditedChunks->end())
				pEditedChunks->push_back(this);
		}
	}
}

void CTerrainChunk::UpdateBoundingBox()
{
	if (m_pBB)
	{
		FLOAT fMinY = m_pHeightMap[0][0];
		FLOAT fMaxY = m_pHeightMap[0][0];

		for (int i = 0; i < m_nChunkSize + 1; i++)
		{
			for (int j = 0; j < m_nChunkSize + 1; j++)
			{
				if (m_pHeightMap[i][j] < fMinY)
					fMinY = m_pHeightMap[i][j];
				if (m_pHeightMap[i][j] > fMaxY)
					fMaxY = m_pHeightMap[i][j];
			}
		}

		UpdateBoundingBox(fMinY, fMaxY);
	}
}

void CTerrainChunk::UpdateBoundingBox(FLOAT fMinY, FLOAT fMaxY)
{
	if (m_pBB)
	{
		for (UINT i = 0; i < m_vecObjsStatic.size(); i++)
		{
			CObj* pObj = m_vecObjsStatic.Get(i);
			if (pObj && pObj->m_pBB)
			{
				BoundingBox* pBB = pObj->m_pBB;
				for (UINT j = 0; j < 8; j++)
				{
					fMinY = min(fMinY, pBB->vectors[j].y);
					fMaxY = max(fMaxY, pBB->vectors[j].y);
				}
			}
		}

		Vector3 vMin(
			m_nX * CTerrainMng::m_fTileSize,
			fMinY,
			m_nZ * CTerrainMng::m_fTileSize);
		Vector3 vMax(
			vMin.x + m_nChunkSize * CTerrainMng::m_fTileSize,
			fMaxY,
			vMin.z + m_nChunkSize * CTerrainMng::m_fTileSize);

		m_pBB->SetBounds(vMin, vMax);
	}
}

void CTerrainChunk::UpdateBoundingBoxToChildren()
{
	if (m_pChunks[0].m_pBB && m_pChunks[1].m_pBB && m_pChunks[2].m_pBB && m_pChunks[3].m_pBB)
	{
		UpdateBoundingBox(
			min(m_pChunks[0].m_pBB->vMin.y, min(m_pChunks[1].m_pBB->vMin.y,
				min(m_pChunks[2].m_pBB->vMin.y, m_pChunks[3].m_pBB->vMin.y))),
			max(m_pChunks[0].m_pBB->vMax.y, max(m_pChunks[1].m_pBB->vMax.y,
				max(m_pChunks[2].m_pBB->vMax.y, m_pChunks[3].m_pBB->vMax.y))));
	}
}

void CTerrainChunk::UpdateBoundingBox(CTerrainChunk* pChunk)
{
	if (!pChunk)
		return;

	if (pChunk == this)
	{
		if (m_pChunks)
			UpdateBoundingBoxToChildren();
		else if (!m_pChunks)
			UpdateBoundingBox();
		return;
	}

	if (m_pChunks)
	{
		for (int i = 0; i < 4; i++)
		{
			if (m_pChunks[i].VertexOnChunk(pChunk->m_nX, pChunk->m_nZ))
			{
				m_pChunks[i].UpdateBoundingBox(pChunk);
				break;
			}
		}
	}

	UpdateBoundingBoxToChildren();
}

BOOL CTerrainChunk::AddObject(CObj* pObj, BOOL bFront)
{
	BOOL bAdded = FALSE;

	if (pObj && pObj->m_pBB)
	{
		if (m_pChunks)
		{
			for (int i = 0; i < 4; i++)
			{
				if (m_pChunks[i].AddObject(pObj, bFront))
				{
					bAdded = TRUE;
					break;
				}
			}
		}

		if ((!m_pChunks || !bAdded) && BoundingBoxOnChunk(pObj->m_pBB))
		{
			m_vecObjsStatic.Add(pObj, bFront);
			bAdded = TRUE;
			pObj->m_pChunk = this;
		}
	}

	if (bAdded)
	{
		if (m_pChunks)
			UpdateBoundingBoxToChildren();
		else
			UpdateBoundingBox();
	}

	return bAdded;
}

BOOL CTerrainChunk::RemoveObject(CObj* pObj, BOOL bDelete)
{
	if (pObj && m_vecObjsStatic.Remove(pObj, bDelete))
	{
		pObj->m_pChunk = NULL;

		CTerrainMng* pTerrainMng = g_Client.GetWorld()->GetTerrainMng();
		pTerrainMng->m_vecVisibleObjs.Remove(pObj, FALSE);
		pTerrainMng->UpdateBoundingBox(this);
		return TRUE;
	}

	return FALSE;
}