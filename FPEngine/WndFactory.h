
#pragma once

#define REGISTER_WND(type, id) g_WndFactory.RegisterWnd<type>(id);

class CWndFactory
{
private:
	struct LayoutElement
	{
		int WID,
			WIDC;
		CPoint position;
		CSize size;
		vector<TokenData> params;
		void ApplyTo(CWndBase* pWnd, CWndBase* pWndParent)
		{
			if (pWnd)
			{
				pWnd->m_dwId = WID;
				pWnd->OnLoadLayout(&params);
				pWnd->Create(
					position.x,
					position.y,
					size.cx,
					size.cy,
					pWndParent,
					WIDC);
			}
		}
	};

	struct Layout : LayoutElement
	{
		BOOL inherit = FALSE;
		vector<LayoutElement> elements;
		void Layout::operator = (const LayoutElement& e)
		{
			WID = e.WID;
			WIDC = e.WIDC;
			position = e.position;
			size = e.size;
			params = e.params;
		}
	};

	map<int, Layout> m_mapLayouts;
	BOOL m_bBreak;
	LayoutElement LoadLayoutElement(CScanner* scanner, LPCTSTR delimiter);

	map<int, CWndBase*(*)()> m_mapConstructors;
	template <class T> static CWndBase* Constructor()
	{
		return (CWndBase*)new T();
	}

	void(*m_pRegisterWndsFunc)();

public:
	CWndFactory();
	~CWndFactory();

	template <class T> void RegisterWnd(int nId)
	{
		m_mapConstructors.insert(make_pair(nId, &Constructor<T>));
	}

	void LoadLayouts(LPCTSTR lpszFileName);
	void CreateLayoutChildren(CWndBase* pWnd);
	CWndBase* CreateWnd(int nWID, CWndBase* pWndParent);

	void SetRegisterWndsFunc(void(*pRegisterWndsFunc)()) { m_pRegisterWndsFunc = pRegisterWndsFunc; }

	void RegisterWnds();
};

extern CWndFactory g_WndFactory;