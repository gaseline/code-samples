
#include "StdafxFP.h"

#ifdef __INCLUDE_WORLD_EDITOR

CWorldEditor::CWorldEditor()
{
	m_bActive = FALSE;
	m_pSelectedObj = NULL;
	m_bHoldingObj = FALSE;
	m_vMovePrev = Vector3(0, 0, 0);
	m_nSelectedEditorObject = 0;

	SetEditorMode(WORLD_EDITOR_SELECT);
	SetEditorAxis(WORLD_EDITOR_XZ);
}

CWorldEditor::~CWorldEditor()
{
	DeleteDeviceObjects();
}

HRESULT CWorldEditor::DeleteDeviceObjects()
{
	for (UINT i = 0; i < m_vecUndoActions.size(); i++)
		CleanUpAction(&m_vecUndoActions[i]);
	m_vecUndoActions.clear();

	for (UINT i = 0; i < m_vecRedoActions.size(); i++)
		CleanUpAction(&m_vecRedoActions[i]);
	m_vecRedoActions.clear();

	return S_OK;
}

void CWorldEditor::Load()
{
	string strFilePath = MAKE_PATH_ASSETS("EditorObjects.fps");

	CScanner scanner;
	if (scanner.Load_ResFile(strFilePath.c_str()))
	{
		OBJECT_TYPE objType = OBJECT_TYPE_STATIC;

		while (scanner.tok != FINISHED)
		{
			scanner.GetToken();

			if (scanner.Token == "#static")
				objType = OBJECT_TYPE_STATIC;
			else if (scanner.Token == "#dynamic")
				objType = OBJECT_TYPE_DYNAMIC;
			else if (!scanner.Token.IsEmpty())
			{
				scanner.Token.Append(".obj");
				m_vecEditorObjects.push_back({ objType, string(scanner.Token) });
			}
		}
	}
	else
		LogWarning("Failed to load world editor file : %s", strFilePath);
}

void CWorldEditor::ToggleActive()
{
	m_bActive = !m_bActive;

	if (m_bActive)
	{
		// Init
	}
	else
	{
		SetEditorMode(WORLD_EDITOR_SELECT);
		SetEditorAxis(WORLD_EDITOR_XZ);
		m_pSelectedObj = NULL;
		m_bHoldingObj = FALSE;
	}
}

void CWorldEditor::SetEditorMode(WORLD_EDITOR_TYPE editorMode)
{
	if (m_editorMode != editorMode)
	{
		m_editorMode = editorMode;

		m_bHoldingObj = FALSE;
		g_Client.m_bMouseL = FALSE;

		if (m_editorMode == WORLD_EDITOR_ADD || m_editorMode == WORLD_EDITOR_TERRAIN)
			m_pSelectedObj = NULL;
	}
}

void CWorldEditor::SetEditorAxis(WORLD_EDITOR_AXIS editorAxis)
{
	if (m_editorAxis != editorAxis)
	{
		m_editorAxis = editorAxis;

		m_dwColorAxisX = m_editorAxis == WORLD_EDITOR_X || m_editorAxis == WORLD_EDITOR_XZ ||
			m_editorAxis == WORLD_EDITOR_XYZ ? COLOR_RED : COLOR_WHITE;
		m_dwColorAxisY = m_editorAxis == WORLD_EDITOR_Y || m_editorAxis == WORLD_EDITOR_XYZ ? COLOR_GREEN : COLOR_WHITE;
		m_dwColorAxisZ = m_editorAxis == WORLD_EDITOR_Z || m_editorAxis == WORLD_EDITOR_XZ ||
			m_editorAxis == WORLD_EDITOR_XYZ ? COLOR_BLUE : COLOR_WHITE;
	}
}

void CWorldEditor::RenderClean()
{
	if (!m_bActive)
		return;

	if (m_pSelectedObj && m_pSelectedObj->m_pBB)
	{
		g_Grp3D.BeginDebugRender();

		// Render bounding box in global space
		g_Client.GetShader()->ResetWorldMatrix();
		g_Grp3D.RenderCube(m_pSelectedObj->m_pBB->vectors, COLOR_WHITE);

		// Render axes in z disabled object space but ignore scaling
		D3DDEVICE->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		D3DDEVICE->SetRenderState(D3DRS_ZENABLE, FALSE);

		if(m_editorMode == WORLD_EDITOR_MOVE)
			g_Client.GetShader()->SetObjectMatrices(m_pSelectedObj->m_matTranslation);
		else
			g_Client.GetShader()->SetObjectMatrices(m_pSelectedObj->m_matRotation * m_pSelectedObj->m_matTranslation);

		g_Grp3D.Render3DLine(Vector3(0, 0, 0), Vector3(0, 3, 0), m_dwColorAxisY);
		g_Grp3D.Render3DLine(Vector3(0, 0, 0), Vector3(3, 0, 0), m_dwColorAxisX);
		g_Grp3D.Render3DLine(Vector3(0, 0, 0), Vector3(0, 0, 3), m_dwColorAxisZ);

		D3DDEVICE->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		D3DDEVICE->SetRenderState(D3DRS_ZENABLE, TRUE);

		g_Grp3D.EndDebugRender();
	}
}

void CWorldEditor::FrameMove(const FLOAT& fTimeFactor)
{
	if (!m_bActive)
		return;

	if (!g_Client.m_bMouseL && !m_bHoldingObj && KeyPressed(VK_CONTROL) && KeyPressedReset('Z'))
	{
		if (KeyPressed(VK_SHIFT))
			Redo();
		else
			Undo();
	}

	if (KeyPressedReset(VK_DELETE))
		ProcessDelete();

	if (KeyPressedReset('Z'))
		SetEditorAxis(WORLD_EDITOR_X);
	if (KeyPressedReset('X'))
		SetEditorAxis(WORLD_EDITOR_Y);
	if (KeyPressedReset('C'))
		SetEditorAxis(WORLD_EDITOR_Z);
	if (KeyPressedReset('V'))
		SetEditorAxis(WORLD_EDITOR_XZ);
	if (KeyPressedReset('B'))
		SetEditorAxis(WORLD_EDITOR_XYZ);
	if (KeyPressedReset('N'))
		m_nSelectedEditorObject = (m_nSelectedEditorObject + 1) % (int)m_vecEditorObjects.size();

	if (KeyPressedReset('1'))
		SetEditorMode(WORLD_EDITOR_SELECT);
	if (KeyPressedReset('2'))
		SetEditorMode(WORLD_EDITOR_MOVE);
	if (KeyPressedReset('3'))
		SetEditorMode(WORLD_EDITOR_ROTATE);
	if (KeyPressedReset('4'))
		SetEditorMode(WORLD_EDITOR_SCALE);
	if (KeyPressedReset('5'))
		SetEditorMode(WORLD_EDITOR_ADD);
	if (KeyPressedReset('6'))
		SetEditorMode(WORLD_EDITOR_TERRAIN);

	switch (m_editorMode)
	{
	case WORLD_EDITOR_SELECT:
		ProcessSelect();
		break;
	case WORLD_EDITOR_MOVE:
		ProcessMove();
		break;
	case WORLD_EDITOR_ROTATE:
		ProcessRotate();
		break;
	case WORLD_EDITOR_SCALE:
		ProcessScale();
		break;
	case WORLD_EDITOR_ADD:
		ProcessAdd();
		break;
	case WORLD_EDITOR_TERRAIN:
		ProcessTerrain();
		break;
	}
}

void CWorldEditor::ProcessSelect()
{
	if (g_Client.m_bMouseL)
	{
		Vector3 vPickRayOrig, vPickRayDir, vIntersect;
		GetPickRay(g_Client.m_ptMouse, g_Client.GetCamera(TRUE), &vPickRayOrig, &vPickRayDir);
		m_pSelectedObj = g_Client.GetWorld()->GetTerrainMng()
			->IntersectObjects(vPickRayOrig, vPickRayDir, g_Client.GetCamera(TRUE)->m_fDepth, &vIntersect);

		g_Client.m_bMouseL = FALSE;
	}
}

void CWorldEditor::ProcessMove()
{
	if (g_Client.m_bMouseL && m_pSelectedObj)
	{
		Vector3 vPickRayOrig, vPickRayDir, vPtOnRay;
		GetPickRay(g_Client.m_ptMouse, g_Client.GetCamera(TRUE), &vPickRayOrig, &vPickRayDir);

		if (m_editorAxis == WORLD_EDITOR_Y)
		{
			vPtOnRay = GetPointOnRayZ(m_pSelectedObj->m_vPosition.z, vPickRayOrig, vPickRayDir);
			Vector3 vPtOnRay2 = GetPointOnRayX(m_pSelectedObj->m_vPosition.x, vPickRayOrig, vPickRayDir);

			if (D3DXVec3Length(&(vPtOnRay2 - vPickRayOrig)) < D3DXVec3Length(&(vPtOnRay - vPickRayOrig)))
				vPtOnRay = vPtOnRay2;
		}
		else
			vPtOnRay = GetPointOnRayY(m_pSelectedObj->m_vPosition.y, vPickRayOrig, vPickRayDir);

		if (m_editorAxis != WORLD_EDITOR_Y)
			vPtOnRay.y = 0;
		else
		{
			vPtOnRay.z = 0;
			vPtOnRay.x = 0;
		}

		if (m_editorAxis == WORLD_EDITOR_X)
			vPtOnRay.z = 0;
		else if (m_editorAxis == WORLD_EDITOR_Z)
			vPtOnRay.x = 0;

		if (!m_bHoldingObj)
		{
			CreateUndoAction(WORLD_EDITOR_MOVE, m_pSelectedObj);
			m_vMovePrev = vPtOnRay;
			m_bHoldingObj = TRUE;
		}

		if (vPtOnRay - m_vMovePrev != Vector3(0, 0, 0))
		{
			RemoveObjFromTerrain(m_pSelectedObj);

			m_pSelectedObj->AddPositionGlobal(vPtOnRay - m_vMovePrev);

			AddObjToTerrain(m_pSelectedObj);

			m_vMovePrev = vPtOnRay;
		}
	}
	else if (!g_Client.m_bMouseL && m_bHoldingObj)
		m_bHoldingObj = FALSE;
}

void CWorldEditor::ProcessRotate()
{
	if (g_Client.m_bMouseL && m_pSelectedObj)
	{
		CPoint ptMouseDelta = g_Client.GetAndResetPtMouseChange();
		FLOAT fDeltaX = (FLOAT)ptMouseDelta.x / 5.0f;
		FLOAT fDeltaY = (FLOAT)ptMouseDelta.y / 5.0f;

		if (fDeltaX == 0 && fDeltaY == 0)
			return;

		if (!m_bHoldingObj)
		{
			CreateUndoAction(WORLD_EDITOR_ROTATE, m_pSelectedObj);
			m_bHoldingObj = TRUE;
		}

		RemoveObjFromTerrain(m_pSelectedObj);

		switch (m_editorAxis)
		{
		case WORLD_EDITOR_XYZ:
		case WORLD_EDITOR_XZ:
		case WORLD_EDITOR_Y:
			m_pSelectedObj->AddRotation(0, fDeltaX, 0);
			break;
		case WORLD_EDITOR_X:
			m_pSelectedObj->AddRotation(fDeltaX, 0, 0);
			break;
		case WORLD_EDITOR_Z:
			m_pSelectedObj->AddRotation(0, 0, fDeltaX);
			break;
		}

		AddObjToTerrain(m_pSelectedObj);
	}
	else if (!g_Client.m_bMouseL && m_bHoldingObj)
		m_bHoldingObj = FALSE;
}

void CWorldEditor::ProcessScale()
{
	if (g_Client.m_bMouseL && m_pSelectedObj)
	{
		CPoint ptMouseDelta = g_Client.GetAndResetPtMouseChange();
		FLOAT fDeltaX = (FLOAT)ptMouseDelta.x / 5.0f;
		FLOAT fDeltaY = (FLOAT)ptMouseDelta.y / 5.0f;

		if (fDeltaX == 0 && fDeltaY == 0)
			return;

		if (!m_bHoldingObj)
		{
			CreateUndoAction(WORLD_EDITOR_SCALE, m_pSelectedObj);
			m_bHoldingObj = TRUE;
		}

		RemoveObjFromTerrain(m_pSelectedObj);

		switch (m_editorAxis)
		{
		case WORLD_EDITOR_XYZ:
			m_pSelectedObj->AddScaling(fDeltaX, fDeltaX, fDeltaX);
			break;
		case WORLD_EDITOR_XZ:
			m_pSelectedObj->AddScaling(fDeltaX, 0, fDeltaX);
			break;
		case WORLD_EDITOR_Y:
			m_pSelectedObj->AddScaling(0, fDeltaX, 0);
			break;
		case WORLD_EDITOR_X:
			m_pSelectedObj->AddScaling(fDeltaX, 0, 0);
			break;
		case WORLD_EDITOR_Z:
			m_pSelectedObj->AddScaling(0, 0, fDeltaX);
			break;
		}

		AddObjToTerrain(m_pSelectedObj);
	}
	else if (!g_Client.m_bMouseL && m_bHoldingObj)
		m_bHoldingObj = FALSE;
}

void CWorldEditor::ProcessAdd()
{
	if (g_Client.m_bMouseL && (UINT)m_nSelectedEditorObject < m_vecEditorObjects.size())
	{
		Vector3 vPickRayOrig, vPickRayDir, vIntersect;
		GetPickRay(g_Client.m_ptMouse, g_Client.GetCamera(TRUE), &vPickRayOrig, &vPickRayDir);
		g_Client.GetWorld()->GetTerrainMng()
			->IntersectRayTerrain(vPickRayOrig, vPickRayDir, g_Client.GetCamera(TRUE)->m_fDepth, &vIntersect);

		CObj* pObj = g_Client.GetWorld()->AddObject(m_vecEditorObjects[m_nSelectedEditorObject], vIntersect, Vector3(0, 0, 0), 1.0f);

		if (pObj)
		{
			CreateUndoAction(WORLD_EDITOR_ADD, pObj);
			m_pSelectedObj = pObj;
		}

		g_Client.m_bMouseL = FALSE;
	}
}

void CWorldEditor::ProcessDelete()
{
	CreateUndoAction(WORLD_EDITOR_DELETE, m_pSelectedObj);
	RemoveObjFromTerrain(m_pSelectedObj);
	m_pSelectedObj = NULL;
}

void CWorldEditor::ProcessTerrain()
{
	CTerrainMng* pTerrainMng = g_Client.GetWorld()->GetTerrainMng();

	if (KeyPressedReset('E'))
		pTerrainMng->AddEditRange(1);
	else if (KeyPressedReset('Q'))
		pTerrainMng->AddEditRange(-1);

	pTerrainMng->ProcessEdit();
}

CString CWorldEditor::GetCurrentEditorObjectName()
{
	string str = m_vecEditorObjects[m_nSelectedEditorObject].strFileName;
	return CString(str.substr(0, str.find_last_of(".")).c_str());
}

OBJECT_TYPE CWorldEditor::GetCurrentEditorObjectType()
{
	return m_vecEditorObjects[m_nSelectedEditorObject].objectType;
}

BOOL CWorldEditor::RemoveObjFromTerrain(CObj* pObj)
{
	return g_Client.GetWorld()->GetTerrainMng()->RemoveObject(pObj, FALSE);
}

BOOL CWorldEditor::AddObjToTerrain(CObj* pObj)
{
	return g_Client.GetWorld()->GetTerrainMng()->AddObject(pObj);
}

void CWorldEditor::CreateAction(WORLD_EDITOR_TYPE actionType, CObj* pObj, vector<Action>& vecActions)
{
	if (!pObj)
		return;

	Action action;
	action.actionType = actionType;

	switch (actionType)
	{
	case WORLD_EDITOR_MOVE:
	case WORLD_EDITOR_ROTATE:
	case WORLD_EDITOR_SCALE:
	{
		ActionDataEdit* pActionData = new ActionDataEdit;
		pActionData->pObj = pObj;
		pActionData->vPosition = pObj->m_vPosition;
		pActionData->vRotation = pObj->m_vRotation;
		pActionData->vScaling = pObj->m_vScaling;
		action.actionData = pActionData;
		break;
	}
	case WORLD_EDITOR_ADD:
	case WORLD_EDITOR_DELETE:
	{
		ActionDataAddDelete* pActionData = new ActionDataAddDelete;
		pActionData->pObj = pObj;
		action.actionData = pActionData;
		break;
	}
	}

	vecActions.push_back(action);

	if (vecActions.size() > WORLD_EDITOR_MAX_ACTIONS)
	{
		CleanUpAction(&vecActions.front());
		vecActions.erase(vecActions.begin());
	}
}

void CWorldEditor::CreateUndoAction(WORLD_EDITOR_TYPE actionType, CObj* pObj, BOOL bClearRedo)
{
	CreateAction(actionType, pObj, m_vecUndoActions);

	if (bClearRedo)
	{
		for (UINT i = 0; i < m_vecRedoActions.size(); i++)
			CleanUpAction(&m_vecRedoActions[i]);
		m_vecRedoActions.clear();
	}
}

void CWorldEditor::CreateRedoAction(WORLD_EDITOR_TYPE actionType, CObj* pObj)
{
	CreateAction(actionType, pObj, m_vecRedoActions);
}

void CWorldEditor::CleanUpAction(Action* pAction)
{
	switch (pAction->actionType)
	{
	case WORLD_EDITOR_DELETE:
		SAFE_DELETE(((ActionDataAddDelete*)pAction->actionData)->pObj);
		break;
	}

	SAFE_DELETE(pAction->actionData);
}

void CWorldEditor::ProcessAction(vector<Action>& vecActions)
{
	if (vecActions.size() == 0)
		return;

	Action action = vecActions.back();
	vecActions.pop_back();

	switch (action.actionType)
	{
	case WORLD_EDITOR_MOVE:
	case WORLD_EDITOR_ROTATE:
	case WORLD_EDITOR_SCALE:
	{
		ActionDataEdit* pActionDataEdit = (ActionDataEdit*)action.actionData;

		if (pActionDataEdit->pObj)
		{
			RemoveObjFromTerrain(pActionDataEdit->pObj);

			pActionDataEdit->pObj->SetPosition(pActionDataEdit->vPosition);
			pActionDataEdit->pObj->SetRotation(pActionDataEdit->vRotation);
			pActionDataEdit->pObj->SetScaling(pActionDataEdit->vScaling);

			AddObjToTerrain(pActionDataEdit->pObj);
		}
		break;
	}
	case WORLD_EDITOR_ADD:
		RemoveObjFromTerrain(((ActionDataAddDelete*)action.actionData)->pObj);
		m_pSelectedObj = NULL;
		break;
	case WORLD_EDITOR_DELETE:
		AddObjToTerrain(((ActionDataAddDelete*)action.actionData)->pObj);
		break;
	}

	SAFE_DELETE(action.actionData);
}

void CWorldEditor::Undo()
{
	if (m_vecUndoActions.size() == 0)
		return;

	Action action = m_vecUndoActions.back();

	switch (action.actionType)
	{
	case WORLD_EDITOR_MOVE:
	case WORLD_EDITOR_ROTATE:
	case WORLD_EDITOR_SCALE:
		CreateRedoAction(action.actionType, ((ActionDataEdit*)action.actionData)->pObj);
		break;
	case WORLD_EDITOR_ADD:
		CreateRedoAction(WORLD_EDITOR_DELETE, ((ActionDataAddDelete*)action.actionData)->pObj);
		break;
	case WORLD_EDITOR_DELETE:
		CreateRedoAction(WORLD_EDITOR_ADD, ((ActionDataAddDelete*)action.actionData)->pObj);
		break;
	}

	ProcessAction(m_vecUndoActions);
}

void CWorldEditor::Redo()
{
	if (m_vecRedoActions.size() == 0)
		return;

	Action action = m_vecRedoActions.back();

	switch (action.actionType)
	{
	case WORLD_EDITOR_MOVE:
	case WORLD_EDITOR_ROTATE:
	case WORLD_EDITOR_SCALE:
		CreateUndoAction(action.actionType, ((ActionDataEdit*)action.actionData)->pObj, FALSE);
		break;
	case WORLD_EDITOR_ADD:
		CreateUndoAction(WORLD_EDITOR_DELETE, ((ActionDataAddDelete*)action.actionData)->pObj, FALSE);
		break;
	case WORLD_EDITOR_DELETE:
		CreateUndoAction(WORLD_EDITOR_ADD, ((ActionDataAddDelete*)action.actionData)->pObj, FALSE);
		break;
	}

	ProcessAction(m_vecRedoActions);
}

#endif // __INCLUDE_WORLD_EDITOR