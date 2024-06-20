
// MFC_MoneySenseDlg.h: 헤더 파일
//

#pragma once
#include "Server_connect_winsock.h"
#define END 0
#define SEND_START 1
#define WAIT_MESSAGE 2
#define RECEIVE_MESSAGE 3
#define SEND_RESULT 4
#define FINISH 5
#define ERROR 6

// CMFCMoneySenseDlg 대화 상자
class CMFCMoneySenseDlg : public CDialogEx
{
// 생성입니다.
public:
	CMFCMoneySenseDlg(CWnd* pParent = nullptr);	// 표준 생성자입니다.
	virtual ~CMFCMoneySenseDlg();
// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MFC_MONEYSENSE_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.


// 구현입니다.
public:
	HICON m_hIcon;
	Server_connect_winsock& connector = Server_connect_winsock::getInstance();
	bool m_isServerConnected;
	CFont m_font;
	CFont m_font_1;
	CFont newFont;
	vector<Mat> Resultimgs;

	Mat Originalimage; // 원본 이미지 저장
	Mat Currentimage; // 현재 표시할 이미지 저장
	int MouseWheelFlag = 1;

	bool m_bIsServerTaskRunning;        // 서버 작업 실행 상태 플래그

	CStatic m_PicResult;
	CListCtrl m_ListCtrl;
	CListCtrl m_ListCtrl_SortContainer;

	bool m_bButtonStopToggled; // 버튼 토글 상태를 저장하는 변수
	COLORREF m_btnColor; // 버튼 색상

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg LRESULT OnWorkerThreadMessage_ServerState(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWorkerThreadMessage_MessageBox(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWorkerThreadMessage_ShowImage(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWorkerThreadMessage_TextChange(WPARAM wParam, LPARAM lParam);
	afx_msg void DisplayImage(cv::Mat& origin, CStatic& m_pic);
	void DisplayImage(cv::Mat& origin, CDC& dc, CRect& rect);
	void AddListCtrl(const String bill, const double area, const double areaPersent);
	void AddListCtrl_SortContainer(const String bill);

	void DrawButtonImage(bool isGreen);
	void Zoom(float factor, CPoint center);
	void Zoom(float factor);
	//LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
	afx_msg void OnBnClickedButtonStart();
	afx_msg void OnBnClickedButtonStop();
	afx_msg void OnBnClickedButtonSelect();
	afx_msg void OnBnClickedButtonDelete();
	afx_msg void OnBnClickedButtonReset();
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	CButton m_btnStop;
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
