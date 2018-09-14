
#include "StdafxFP.h"

CWndFactory::CWndFactory()
{
}

CWndFactory::~CWndFactory()
{
}

void CWndFactory::LoadLayouts(LPCTSTR lpszFileName)
{
	m_bBreak = FALSE;

	CScanner scanner;
	if (!scanner.Load_ResFile(lpszFileName))
		return;

	scanner.GetToken();
	while (scanner.tok != FINISHED)
	{
		Layout layout;

		if (scanner.Token == ":")
		{
			layout.inherit = TRUE;

			layout.WID = scanner.GetDefinition();
			layout.WIDC = scanner.GetDefinition();

			if (layout.WID == -1 || layout.WIDC == -1)
				return; // failure

			goto SKIP_ELEMENTS;
		}
		else
			scanner.PutBack();

		layout = LoadLayoutElement(&scanner, "{");

		if (m_bBreak)
			return;

		scanner.GetToken();
		while (scanner.Token != "}" && scanner.tok != FINISHED)
		{
			scanner.PutBack();

			layout.elements.push_back(LoadLayoutElement(&scanner, ";"));

			if (m_bBreak)
				return;

			scanner.GetToken();
		}

	SKIP_ELEMENTS:
		if (!m_mapLayouts.insert(make_pair(layout.WID, layout)).second)
			LogError("Layout key conflict : %d", layout.WID);

		scanner.GetToken();
	}
}

CWndFactory::LayoutElement CWndFactory::LoadLayoutElement(CScanner* scanner, LPCTSTR delimiter)
{
	LayoutElement layoutElement;
	queue<int> q;

	layoutElement.WID = scanner->GetDefinition();
	layoutElement.WIDC = scanner->GetDefinition();

	if (layoutElement.WID == -1 || layoutElement.WIDC == -1)
		goto RETURN_BREAK; // failure

	while (q.size() < 4)
	{
		q.push(scanner->GetNumber());
		if (q.size() < 4 && scanner->Token == delimiter)
		{
			LogError("Failed to load layout element : missing default parameters");
			goto RETURN_BREAK; // failure
		}
	}

	if (q.size() < 4)
		goto RETURN_BREAK; // failure

	layoutElement.position.x = q.front(); q.pop();
	layoutElement.position.y = q.front(); q.pop();
	layoutElement.size.cx = q.front(); q.pop();
	layoutElement.size.cy = q.front(); q.pop();

	scanner->GetToken();
	while (scanner->Token != delimiter && scanner->tok != FINISHED)
	{
		layoutElement.params.push_back(scanner->GetTokenData());
		scanner->GetToken();
	}

	return layoutElement; // success

RETURN_BREAK:
	m_bBreak = TRUE;
	return layoutElement;
}

void CWndFactory::CreateLayoutChildren(CWndBase* pWnd)
{
	if (!pWnd)
		return;

	if(!m_mapLayouts.count(pWnd->m_dwId))
	{
		LogWarning("Layout not found for child : %d", pWnd->m_dwId);
		return;
	}

	Layout layout = m_mapLayouts.at(pWnd->m_dwId);

	if (layout.inherit)
	{
		if (!m_mapLayouts.count(layout.WIDC))
		{
			LogError("Base layout not found : %d", layout.WIDC);
			return;
		}

		layout = m_mapLayouts.at(layout.WIDC);
	}

	for (int i = 0; i < (int)layout.elements.size(); i++)
	{
		int nWID = layout.elements.at(i).WID;

		if (!m_mapConstructors.count(nWID))
		{
			LogError("Layout ID to object link not found : %d", nWID);
			return;
		}

		CWndBase* pWndChild = m_mapConstructors.at(nWID)();

		if (!pWndChild)
		{
			LogError("Failed to create UI object : %d", nWID);
			return;
		}

		layout.elements.at(i).ApplyTo(pWndChild, pWnd);

		pWnd->AddWnd(pWndChild);
	}
}

CWndBase* CWndFactory::CreateWnd(int nWID, CWndBase* pWndParent)
{
	if (!m_mapConstructors.count(nWID))
	{
		LogError("Layout ID to object link not found : %d", nWID);
		return NULL;
	}

	CWndBase* pWnd = m_mapConstructors.at(nWID)();
	
	if (!pWnd)
	{
		LogError("Failed to create UI object : %d", nWID);
		return NULL;
	}

	if (!m_mapLayouts.count(nWID))
	{
		LogWarning("Layout not found : %d", nWID);
		SAFE_DELETE(pWnd);
		return NULL;
	}

	Layout layout = m_mapLayouts.at(nWID);

	if (layout.inherit)
	{
		if (!m_mapLayouts.count(layout.WIDC))
		{
			LogError("Base layout not found : %d", layout.WIDC);
			SAFE_DELETE(pWnd);
			return NULL;
		}

		layout = m_mapLayouts.at(layout.WIDC);
	}

	layout.ApplyTo(pWnd, pWndParent);

	return pWnd;
}

void CWndFactory::RegisterWnds()
{
	RegisterWnd<CWndBase>(WID_BASE);
	RegisterWnd<CWndView>(WID_VIEW);
	RegisterWnd<CWndButton>(WID_BUTTON);
	RegisterWnd<CWndCheckBox>(WID_CHECKBOX);
	RegisterWnd<CWndListBox>(WID_LISTBOX);
	RegisterWnd<CWndScrollBar>(WID_SCROLLBAR);
	RegisterWnd<CWndScrollView>(WID_SCROLLVIEW);
	RegisterWnd<CWndSliderBar>(WID_SLIDERBAR);
	RegisterWnd<CWndTitleBar>(WID_TITLEBAR);

	if (m_pRegisterWndsFunc)
		m_pRegisterWndsFunc();
	else
		LogWarning("Callback method for registering interface constructors not set");
}