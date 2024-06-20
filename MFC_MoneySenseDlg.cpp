
// MFC_MoneySenseDlg.cpp: 구현 파일
//

#include "pch.h"
#include "framework.h"
#include "MFC_MoneySense.h"
#include "MFC_MoneySenseDlg.h"
#include "afxdialogex.h"
#include "Server_connect_winsock.h"
#include "Bill_Detected.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CMFCMoneySenseDlg 대화 상자



CMFCMoneySenseDlg::CMFCMoneySenseDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_MFC_MONEYSENSE_DIALOG, pParent)
	, m_isServerConnected(false) // 초기값 설정
	, m_bButtonStopToggled(false) // 초기화
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CMFCMoneySenseDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATIC_PicResult, m_PicResult);

	DDX_Control(pDX, IDC_LIST1, m_ListCtrl);
	DDX_Control(pDX, IDC_LIST2, m_ListCtrl_SortContainer);
	DDX_Control(pDX, IDC_BUTTON_Stop, m_btnStop);
}

BEGIN_MESSAGE_MAP(CMFCMoneySenseDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_MESSAGE(WM_WORKER_THREAD_MESSAGE_SERVERSTATE, &CMFCMoneySenseDlg::OnWorkerThreadMessage_ServerState) 
	ON_MESSAGE(WM_WORKER_THREAD_MESSAGE_MESSAGEBOX, &CMFCMoneySenseDlg::OnWorkerThreadMessage_MessageBox) 
	ON_MESSAGE(WM_WORKER_THREAD_MESSAGE_SHOWIMAGE, &CMFCMoneySenseDlg::OnWorkerThreadMessage_ShowImage)
	ON_MESSAGE(WM_WORKER_THREAD_MESSAGE_TEXTSTATE, &CMFCMoneySenseDlg::OnWorkerThreadMessage_TextChange) 
	ON_BN_CLICKED(IDC_BUTTON_Start, &CMFCMoneySenseDlg::OnBnClickedButtonStart)
	ON_BN_CLICKED(IDC_BUTTON_Stop, &CMFCMoneySenseDlg::OnBnClickedButtonStop)
	ON_WM_DRAWITEM()    
	ON_BN_CLICKED(IDC_BUTTON_Select, &CMFCMoneySenseDlg::OnBnClickedButtonSelect)
	ON_BN_CLICKED(IDC_BUTTON_Delete, &CMFCMoneySenseDlg::OnBnClickedButtonDelete)
	ON_BN_CLICKED(IDC_BUTTON_Reset, &CMFCMoneySenseDlg::OnBnClickedButtonReset)
	ON_WM_MOUSEHWHEEL()
	ON_WM_MOUSEWHEEL()
	ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

// 서버 상태 버튼 갱신
LRESULT CMFCMoneySenseDlg::OnWorkerThreadMessage_ServerState(WPARAM wParam, LPARAM lParam)
{
	bool* Server_Flag = reinterpret_cast<bool*>(lParam);
	if (Server_Flag != nullptr)
	{
		m_isServerConnected = *Server_Flag;
		delete Server_Flag;

		// 버튼 이미지를 다시 그리기 - 전체 갱신이 아닌 사각형 부분(버튼)만 갱신시키도록
		InvalidateRect(CRect(1150, 27, 1150+ 40, 27 + 40), FALSE);
		UpdateWindow();
	}
	return 0;
}

// 전달받은 메세지로 메세지박스 출력
LRESULT CMFCMoneySenseDlg::OnWorkerThreadMessage_MessageBox(WPARAM wParam, LPARAM lParam)
{
	CString* pMessage = reinterpret_cast<CString*>(lParam);
	AfxMessageBox(*pMessage);
	delete pMessage;  // 메모리 해제
	return 0;
}

// 저장되있는 맨 마지막 이미지 출력
LRESULT CMFCMoneySenseDlg::OnWorkerThreadMessage_ShowImage(WPARAM wParam, LPARAM lParam)
{
	if (!Resultimgs.empty())
	{
		DisplayImage(Resultimgs.back(), m_PicResult);
	}
	return 0;
}

// 작업 상황 갱신
LRESULT CMFCMoneySenseDlg::OnWorkerThreadMessage_TextChange(WPARAM wParam, LPARAM lParam)
{
	CString* pMessage = reinterpret_cast<CString*>(lParam);

	// 작업 상황을 나타내는 ID를 받아 갱신
	GetDlgItem(IDC_STATIC_State)->SetWindowTextW(*pMessage);
	GetDlgItem(IDC_STATIC_State)->Invalidate();

	delete pMessage;  // 메모리 해제

	return 0;
}


// 쓰레드 - 2초마다 서버상태를 확인.
UINT WorkerThreadProc_ConnectServer(LPVOID pParam) {
	CMFCMoneySenseDlg* pDlg = (CMFCMoneySenseDlg*)pParam;
	std::string ip = "localhost";
	std::string port = "8765";
	//std::string ip = "192.168.0.222";
	//std::string port = "8765";
	bool Server_Flag = false;	// 현재 서버 상태
	bool prevServerFlag = false;  // 이전 상태 추적 변수

	while (true) {
		if (!Server_Flag) {
			// 서버로 연결 시도
			Server_Flag = pDlg->connector.ConnectToServer(ip, port);
		}

		// 연결 상태 확인
		Server_Flag = pDlg->connector.IsConnected();

		// 서버 연결 실패 시 연결 닫기
		if (!Server_Flag) {
			pDlg->connector.CloseConnection();
		}

		// 상태가 변경되었을 때만 메시지 전송
		if (Server_Flag != prevServerFlag) {
			PostMessage(pDlg->m_hWnd, WM_WORKER_THREAD_MESSAGE_SERVERSTATE, 0, (LPARAM)new bool(Server_Flag));
			prevServerFlag = Server_Flag;  // 이전 상태 업데이트
		}

		// 2초마다 확인
		Sleep(2000);
	}
}

// 쓰레드 - Start 버튼 서버에 전송
UINT WorkerThreadProc_Start(LPVOID pParam) {
	CMFCMoneySenseDlg* pDlg = (CMFCMoneySenseDlg*)pParam;

	std::string message = "";
	std::string receivemessage = "";
	String isBill;
	double Area;
	double AreaPercent;
	CString* pMessage;
	int step = SEND_START; // switch에 들어갈 현재 진행상황
	const int timeout_seconds = 20; // 타임아웃 시간
	auto start_time = std::chrono::steady_clock::now();
	vector<uchar>* img_data;
	while (step != END) {	// step이 END가 아니면 무한 루프
		switch (step) {
		case SEND_START: //서버에 Start 보내기
			if (pDlg->connector.SendMessageToServer("Start")) {	// Start를 받지 못하는 상황이면 에러
				// 서버에서 수신했다면 다음 step
				step = WAIT_MESSAGE;
				start_time = std::chrono::steady_clock::now(); // 타이머 초기화
				message = "이동중";
				pMessage = new CString(message.c_str());
				PostMessage(pDlg->m_hWnd, WM_WORKER_THREAD_MESSAGE_TEXTSTATE, 0, (LPARAM)pMessage);
			}
			else {
				step = ERROR;
				message = "Error";
			}
			break;
		case WAIT_MESSAGE: // 메세지 대기 - 타임아웃
			if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count() > timeout_seconds) {
				step = ERROR;
				message = "Time Out";
				break;
			}
			receivemessage = pDlg->connector.ReceiveMessage();
			// 메세지를 받았다면 다음 step
			if (!receivemessage.empty())
				step = RECEIVE_MESSAGE;
			break;
		case RECEIVE_MESSAGE: // 메세지가 왔을경우
			if (receivemessage == "Error")	// 받은 메세지가 Error 일경우
			{
				pMessage = new CString(receivemessage.c_str());
				PostMessage(pDlg->m_hWnd, WM_WORKER_THREAD_MESSAGE_MESSAGEBOX, 0, (LPARAM)pMessage);
				step = END;
			}
			else if (receivemessage == "Move_OK") // 받은 메세지가 Move_OK 일경우
			{
				// 현재 상태 갱신
				message = "이동완료";
				pMessage = new CString(message.c_str());
				PostMessage(pDlg->m_hWnd, WM_WORKER_THREAD_MESSAGE_TEXTSTATE, 0, (LPARAM)pMessage);
				step = WAIT_MESSAGE;
			}
			else if (receivemessage == "Cam_OK") // 받은 메세지가 Cam_OK 일경우
			{
				// 현재 상태 갱신
				message = "카메라작동";
				pMessage = new CString(message.c_str());
				PostMessage(pDlg->m_hWnd, WM_WORKER_THREAD_MESSAGE_TEXTSTATE, 0, (LPARAM)pMessage);
				step = WAIT_MESSAGE;
			}
			else if (receivemessage == "Film_OK") // 받은 메세지가 Film_OK 일경우
			{
				// 현재 상태 갱신
				message = "촬상완료";
				pMessage = new CString(message.c_str());
				PostMessage(pDlg->m_hWnd, WM_WORKER_THREAD_MESSAGE_TEXTSTATE, 0, (LPARAM)pMessage);
				step = WAIT_MESSAGE;
				Sleep(1000);
			}
			else // 받은 메세지가 OK 일경우
			{
				// 현재 상태 갱신
				message = "검사중";
				pMessage = new CString(message.c_str());
				PostMessage(pDlg->m_hWnd, WM_WORKER_THREAD_MESSAGE_TEXTSTATE, 0, (LPARAM)pMessage);
				// img_data에 바이트 단위로 엔코딩된 데이터 수신
				img_data = new vector<uchar>(receivemessage.begin(), receivemessage.end());
				if (!img_data->empty()) {
					cv::Mat img = cv::imdecode(*img_data, cv::IMREAD_COLOR);
					if (!img.empty())
					{
						// Bill_Detected를 호출하여 이미지 검사
						Bill_Detected Detected_img(img_data);
						isBill = Detected_img.getIsBill();
						AreaPercent = Detected_img.getAreaPercent();
						Area = Detected_img.getArea();
						// 결과이미지들을 저장하는 벡터에 저장
						pDlg->Resultimgs.push_back(Detected_img.getImage());
						pDlg->AddListCtrl(Detected_img.getIsBill(), Detected_img.getArea(), Detected_img.getAreaPercent()*100);
						// Show Img
						PostMessage(pDlg->m_hWnd, WM_WORKER_THREAD_MESSAGE_SHOWIMAGE, 0, 0);
						// 이미지를 받았다면 다음 step
						step = SEND_RESULT;
					}
					else
					{
						message = "Is Not Img";
						pMessage = new CString(message.c_str());
						PostMessage(pDlg->m_hWnd, WM_WORKER_THREAD_MESSAGE_MESSAGEBOX, 0, (LPARAM)pMessage);
						step = END;
					}
				}
				else
				{
					message = "img_data is empty";
					pMessage = new CString(message.c_str());
					PostMessage(pDlg->m_hWnd, WM_WORKER_THREAD_MESSAGE_MESSAGEBOX, 0, (LPARAM)pMessage);
					step = END;
				}
				delete img_data;
			}
			break;
		case SEND_RESULT: // 검사결과 메세지 전송
			if (isBill == "-") {	// 지폐가 아닐경우
				if (pDlg->connector.SendMessageToServer("Reverse"))
					step = FINISH;
				else
				{
					message = "Message Send Fail";
					pMessage = new CString(message.c_str());
					PostMessage(pDlg->m_hWnd, WM_WORKER_THREAD_MESSAGE_MESSAGEBOX, 0, (LPARAM)pMessage);
					step = END;
				}
			}
			else {	// 지폐일경우
				if(AreaPercent >= 0.75)	// 지폐이고 면적률이 75% 이상일때
				{
					if (isBill == "1000")
						message = "OK_0";
					else if (isBill == "5000")
						message = "OK_1";
					else if (isBill == "10000")
						message = "OK_2";
					else if (isBill == "50000")
						message = "OK_3";

					if (pDlg->connector.SendMessageToServer(message))
					{
						step = FINISH;
						// 지폐통에 검사결과 추가
						pDlg->AddListCtrl_SortContainer(isBill);
					}
					else
					{
						message = "Message Send Fail";
						pMessage = new CString(message.c_str());
						PostMessage(pDlg->m_hWnd, WM_WORKER_THREAD_MESSAGE_MESSAGEBOX, 0, (LPARAM)pMessage);
						step = END;
					}
				}
				else // 지폐이나 면적률이 75% 미만일때
				{
					if (pDlg->connector.SendMessageToServer("Reverse"))
						step = FINISH;
					else
					{
						message = "Message Send Fail";
						pMessage = new CString(message.c_str());
						PostMessage(pDlg->m_hWnd, WM_WORKER_THREAD_MESSAGE_MESSAGEBOX, 0, (LPARAM)pMessage);
						step = END;
					}
				}

			}
				break;
		case FINISH: //정상 종료시
			message = "Finsh";
			pMessage = new CString(message.c_str());
			PostMessage(pDlg->m_hWnd, WM_WORKER_THREAD_MESSAGE_MESSAGEBOX, 0, (LPARAM)pMessage);
			message = "검사완료";
			pMessage = new CString(message.c_str());
			PostMessage(pDlg->m_hWnd, WM_WORKER_THREAD_MESSAGE_TEXTSTATE, 0, (LPARAM)pMessage);
			step = END;			break;

		case ERROR: //에러 처리
		{
			pMessage = new CString(message.c_str());
			PostMessage(pDlg->m_hWnd, WM_WORKER_THREAD_MESSAGE_MESSAGEBOX, 0, (LPARAM)pMessage);
			message = "에러";
			pMessage = new CString(message.c_str());
			PostMessage(pDlg->m_hWnd, WM_WORKER_THREAD_MESSAGE_TEXTSTATE, 0, (LPARAM)pMessage);
			step = END;
		}
		break;
		}
		Sleep(100); // 100ms 대기
	}

	// 동시 작업 불가 락 해제
	pDlg->m_bIsServerTaskRunning = false;

	return 0;
}

// CMFCMoneySenseDlg 메시지 처리기

BOOL CMFCMoneySenseDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 이 대화 상자의 아이콘을 설정합니다.  응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	m_bIsServerTaskRunning = false; // 초기 상태는 작업 실행 중이 아님

	// 새로운 위치와 크기 지정
	int x = 15;
	int y = 122;
	int width = 812;
	int height = 560;

	// Picture control 크기 조정
	m_PicResult.MoveWindow(x, y, width, height);
	//서버 연결 스레드 설정
	AfxBeginThread(WorkerThreadProc_ConnectServer, this);

	// 버튼 색상 및 폰트 설정
	CString fontname = _T("Tahoma");

	m_font.CreatePointFont(280, fontname);
	m_font_1.CreatePointFont(200, fontname);

	GetDlgItem(IDC_BUTTON_Start)->SetFont(&m_font);
	GetDlgItem(IDC_BUTTON_Stop)->SetFont(&m_font);
	GetDlgItem(IDC_BUTTON_Reset)->SetFont(&m_font);
	GetDlgItem(IDC_BUTTON_Select)->SetFont(&m_font_1);
	GetDlgItem(IDC_BUTTON_Delete)->SetFont(&m_font_1);

	// List Control 초기화
	CRect rt;     // 리스트 컨트롤의 크기를 가져올 변수

	m_ListCtrl.GetWindowRect(&rt);
	m_ListCtrl.SetExtendedStyle(LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);   // 리스트 컨트롤에 선표시 및 Item 선택시 한행 전체 선택

	m_ListCtrl.InsertColumn(0, TEXT("지폐 여부"), LVCFMT_CENTER, rt.Width() * 0.2);     // 첫번째
	m_ListCtrl.InsertColumn(1, TEXT("면적"), LVCFMT_LEFT, rt.Width() * 0.2);   // 두번째
	m_ListCtrl.InsertColumn(2, TEXT("면적%"), LVCFMT_LEFT, rt.Width() * 0.2);   // 세번째
	m_ListCtrl.InsertColumn(3, TEXT("교환 기준"), LVCFMT_LEFT, rt.Width() * 0.2);   // 세번째
	m_ListCtrl.InsertColumn(4, TEXT("불량 여부"), LVCFMT_LEFT, rt.Width() * 0.205);   // 세번째

	// 분류통 List Control 초기화

	m_ListCtrl_SortContainer.GetWindowRect(&rt);
	// 리스트 컨트롤 설정
	m_ListCtrl_SortContainer.SetExtendedStyle(LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);   // 리스트 컨트롤에 선표시 및 Item 선택시 한행 전체 선택


	CImageList imgGap;
	imgGap.Create(1, 46, ILC_COLORDDB, 1, 0);
	m_ListCtrl_SortContainer.SetImageList(&imgGap, LVSIL_SMALL);

	m_ListCtrl_SortContainer.InsertColumn(0, _T("지폐 종류"), LVCFMT_LEFT, rt.Width() * 0.5);
	m_ListCtrl_SortContainer.InsertColumn(1, _T("개수"), LVCFMT_LEFT, rt.Width() * 0.505);
	//m_ListCtrl_SortContainer.SetExtendedStyle(LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES);

	// 필요한 데이터만 삽입
	int nItem;

	nItem = m_ListCtrl_SortContainer.InsertItem(0, TEXT("1000"));       // 2행 1열
	m_ListCtrl_SortContainer.SetItemText(nItem, 1, TEXT("0"));           // 2행 2열 - 필요한 경우 개수를 입력

	nItem = m_ListCtrl_SortContainer.InsertItem(1, TEXT("5000"));       // 3행 1열
	m_ListCtrl_SortContainer.SetItemText(nItem, 1, TEXT("0"));           // 3행 2열 - 필요한 경우 개수를 입력

	nItem = m_ListCtrl_SortContainer.InsertItem(2, TEXT("10000"));      // 4행 1열
	m_ListCtrl_SortContainer.SetItemText(nItem, 1, TEXT("0"));           // 4행 2열 - 필요한 경우 개수를 입력

	nItem = m_ListCtrl_SortContainer.InsertItem(3, TEXT("50000"));      // 5행 1열
	m_ListCtrl_SortContainer.SetItemText(nItem, 1, TEXT("0"));           // 5행 2열 - 필요한 경우 개수를 입력

	// 버튼 설정
	// 버튼 핸들을 가져옴
	CWnd* pButton_1 = GetDlgItem(IDC_BUTTON_Start);
	if (pButton_1 != nullptr)
	{
		// 시작 버튼 크기, 위치 설정
		int x_btn_start = 845;  // X 좌표
		int y_btn_start = 553;  // Y 좌표
		int width_btn_start = 160;  // 너비
		int height_btn_start = 130;  // 높이

		// SetWindowPos 함수를 사용하여 버튼 크기와 위치 설정
		pButton_1->SetWindowPos(nullptr, x_btn_start, y_btn_start, width_btn_start, height_btn_start, SWP_NOZORDER);
	}

	CWnd* pButton_2 = GetDlgItem(IDC_BUTTON_Stop);
	if (pButton_2 != nullptr)
	{
		// 멈춤 버튼 크기, 위치 설정
		int x_btn_stop = 1032;  
		int y_btn_stop = 553;  
		int width_btn_stop = 160;  
		int height_btn_stop = 130;  

		// SetWindowPos 함수를 사용하여 버튼 크기와 위치 설정
		pButton_2->SetWindowPos(nullptr, x_btn_stop, y_btn_stop, width_btn_stop, height_btn_stop, SWP_NOZORDER);
	}

	CWnd* pButton_3 = GetDlgItem(IDC_BUTTON_Reset);
	if (pButton_3 != nullptr)
	{
		// 초기화 버튼 크기, 위치 설정
		int x_btn_Reset = 1214; 
		int y_btn_Reset = 552;  
		int width_btn_Reset = 160;  
		int height_btn_Reset = 130;  

		// SetWindowPos 함수를 사용하여 버튼 크기와 위치 설정
		pButton_3->SetWindowPos(nullptr, x_btn_Reset, y_btn_Reset, width_btn_Reset, height_btn_Reset, SWP_NOZORDER);
	}

	CWnd* pButton_4 = GetDlgItem(IDC_BUTTON_Select);
	if (pButton_4 != nullptr)
	{
		// 선택 버튼 크기, 위치 설정
		int x_btn_Select = 1214;
		int y_btn_Select = 353;
		int width_btn_Select = 160;
		int height_btn_Select = 80;

		// SetWindowPos 함수를 사용하여 버튼 크기와 위치 설정
		pButton_4->SetWindowPos(nullptr, x_btn_Select, y_btn_Select, width_btn_Select, height_btn_Select, SWP_NOZORDER);
	}

	CWnd* pButton_5 = GetDlgItem(IDC_BUTTON_Delete);
	if (pButton_5 != nullptr)
	{
		// 삭제 버튼 크기, 위치 설정
		int x_btn_Delete = 1214;
		int y_btn_Delete = 451;
		int width_btn_Delete = 160;
		int height_btn_Delete = 80;

		// SetWindowPos 함수를 사용하여 버튼 크기와 위치 설정
		pButton_5->SetWindowPos(nullptr, x_btn_Delete, y_btn_Delete, width_btn_Delete, height_btn_Delete, SWP_NOZORDER);
	}

	CFont font;
	LOGFONT logfont;
	::ZeroMemory(&logfont, sizeof(logfont));
	_tcscpy_s(logfont.lfFaceName, _T("Tahoma")); // 예: Arial 폰트 사용
	logfont.lfHeight = 48;
	logfont.lfWeight = FW_BOLD; // 글꼴 두께 설정 (FW_NORMAL, FW_BOLD 등)
	font.CreateFontIndirect(&logfont);
	GetDlgItem(IDC_STATIC_State)->SetFont(&font);
	//GetDlgItem(IDC_STATIC_State2)->SetFont(&font);
	font.Detach();





	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 애플리케이션의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void CMFCMoneySenseDlg::OnPaint()
{
	CPaintDC dc(this);

	CImage image_back1, image_back2, image_back3, image_logo, image_flag, image_title, font_server;
	if (image_back1.Load(_T("back_1.png")) != S_OK ||
		image_back2.Load(_T("back_2.png")) != S_OK ||
		image_back3.Load(_T("back_2.png")) != S_OK ||
		image_logo.Load(_T("logo.png")) != S_OK ||
		image_flag.Load(_T("flag.png")) != S_OK ||
		image_title.Load(_T("title.png")) != S_OK ||
		font_server.Load(_T("server.png")) != S_OK)
	{
		AfxMessageBox(_T("이미지를 로드할 수 없습니다."));
	}
	else
	{
		CRect rect;
		GetWindowRect(&rect);

		int back1_Width = rect.Width();
		int back1_Height = 95;
		int back2_Width = rect.Width() / 2;
		int back2_Height = rect.Height();
		int back3_Width = rect.Width() / 2;
		int back3_Height = rect.Height();
		int logo_Width = 190;
		int logo_Height = 55;
		int flag_Width = 160;
		int flag_Height = 95;
		int title_Width = 380;
		int title_Height = 60;
		int server_Width = 150;
		int server_Height = 40;

		image_back1.StretchBlt(dc.m_hDC, 0, 0, back1_Width, back1_Height, 0, 0, image_back1.GetWidth(), image_back1.GetHeight(), SRCCOPY);
		image_back2.StretchBlt(dc.m_hDC, 0, 95, back2_Width, back2_Height, 0, 0, image_back2.GetWidth(), image_back2.GetHeight(), SRCCOPY);
		image_back3.StretchBlt(dc.m_hDC, (rect.Width() / 2), 95, back3_Width, back3_Height, 0, 0, image_back3.GetWidth(), image_back3.GetHeight(), SRCCOPY);
		image_logo.StretchBlt(dc.m_hDC, 20, 20, logo_Width, logo_Height, 0, 0, image_logo.GetWidth(), image_logo.GetHeight(), SRCCOPY);
		image_flag.StretchBlt(dc.m_hDC, 1237, 0, flag_Width, flag_Height, 0, 0, image_flag.GetWidth(), image_flag.GetHeight(), SRCCOPY);
		image_title.StretchBlt(dc.m_hDC, 320, 17, title_Width, title_Height, 0, 0, image_title.GetWidth(), image_title.GetHeight(), SRCCOPY);
		font_server.StretchBlt(dc.m_hDC, 995, 26, server_Width, server_Height, 0, 0, font_server.GetWidth(), font_server.GetHeight(), SRCCOPY);
		DrawButtonImage(m_isServerConnected);
	}
}


// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
HCURSOR CMFCMoneySenseDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


// 서버 상태에 따라 서버 상태 이미지 다시 그리기
void CMFCMoneySenseDlg::DrawButtonImage(bool isGreen)
{
	CClientDC dc(this);

	CImage image_button;
	if (isGreen)
	{
		image_button.Load(_T("button_green.png"));
	}
	else
	{
		image_button.Load(_T("button_red.png"));
	}

	if (image_button.IsNull())
	{
		AfxMessageBox(_T("이미지를 로드할 수 없습니다."));
		return;
	}

	int button_Width = 40;
	int button_Height = 40;
	image_button.StretchBlt(dc.m_hDC, 1150, 27, button_Width, button_Height, 0, 0, image_button.GetWidth(), image_button.GetHeight(), SRCCOPY);
}


// Start 버튼을 눌렀을때
void CMFCMoneySenseDlg::OnBnClickedButtonStart()
{
	// 이미 Start 버튼을 눌렀을때
	if (m_bIsServerTaskRunning) {
		// 이미 작업이 실행 중임을 알리는 메시지 박스 표시
		AfxMessageBox(_T("이미 실행 중입니다."));
		return;
	}

	// 작업 실행 중 플래그 설정
	m_bIsServerTaskRunning = true;
	
	// 쓰레드 - 서버로 메시지 보내기
	AfxBeginThread(WorkerThreadProc_Start, this);
}

CMFCMoneySenseDlg::~CMFCMoneySenseDlg() {
	connector.CloseConnection();
}

void CMFCMoneySenseDlg::OnBnClickedButtonStop()
{
	m_bButtonStopToggled = !m_bButtonStopToggled; // 토글 상태 변경

	// 버튼만 다시 그리도록 InvalidateRect 사용
	CWnd* pWnd = GetDlgItem(IDC_BUTTON_Stop);
	if (pWnd)
	{
		pWnd->Invalidate();
		pWnd->UpdateWindow();
	}
}

// Mat 형태를 picture Control에 표현 - 4배수 적용
void CMFCMoneySenseDlg::DisplayImage(cv::Mat& origin, CStatic& m_pic)
{
	Currentimage = origin;
	Originalimage = origin;
	CRect rect;
	m_pic.GetClientRect(&rect); // 픽쳐 컨트롤의 크기를 얻습니다.

	CDC* pDC = m_pic.GetDC();
	CImage mfcImg;
	cv::Mat outImg;

	cv::flip(origin, outImg, 0); // 이미지를 뒤집습니다.

	// 흑백 이미지인 경우 채널을 3으로 변경합니다.
	if (outImg.channels() == 1) {
		cv::cvtColor(outImg, outImg, cv::COLOR_GRAY2BGR);
	}

	// 원본 이미지와 픽쳐 컨트롤의 비율을 계산합니다.
	double imgAspect = (double)outImg.cols / (double)outImg.rows;
	double rectAspect = (double)rect.Width() / (double)rect.Height();

	int newWidth, newHeight;
	if (imgAspect > rectAspect) {
		// 이미지가 더 넓은 경우
		newWidth = rect.Width();
		newHeight = static_cast<int>(rect.Width() / imgAspect);
	}
	else {
		// 이미지가 더 높은 경우
		newWidth = static_cast<int>(rect.Height() * imgAspect);
		newHeight = rect.Height();
	}

	// 이미지의 크기를 조정합니다.
	cv::resize(outImg, outImg, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_AREA);

	// 흰색 배경 이미지를 생성합니다.
	cv::Mat whiteBg(rect.Height(), rect.Width(), CV_8UC3, cv::Scalar(255, 255, 255));

	// 이미지를 중앙에 배치합니다.
	int offsetX = (rect.Width() - newWidth) / 2;
	int offsetY = (rect.Height() - newHeight) / 2;
	outImg.copyTo(whiteBg(cv::Rect(offsetX, offsetY, newWidth, newHeight)));

	// 이미지를 4의 배수로 맞춥니다.
	int step = (whiteBg.cols * whiteBg.elemSize() + 3) & ~3;
	cv::Mat alignedImg;
	alignedImg.create(whiteBg.rows, whiteBg.cols, whiteBg.type());
	for (int i = 0; i < whiteBg.rows; i++) {
		memcpy(alignedImg.ptr(i), whiteBg.ptr(i), whiteBg.cols * whiteBg.elemSize());
	}

	// 이미지 데이터를 CImage로 변환합니다.
	mfcImg.Create(whiteBg.cols, whiteBg.rows, 24);
	BITMAPINFO bitInfo = { {sizeof(BITMAPINFOHEADER), whiteBg.cols, whiteBg.rows, 1, 24}, 0 };

	pDC->SetStretchBltMode(HALFTONE);
	StretchDIBits(mfcImg.GetDC(), 0, 0, whiteBg.cols, whiteBg.rows, 0, 0, whiteBg.cols, whiteBg.rows, whiteBg.data, &bitInfo, DIB_RGB_COLORS, SRCCOPY);

	// 화면에 이미지를 출력합니다.
	mfcImg.BitBlt(*pDC, rect.left, rect.top);
	mfcImg.ReleaseDC();
	whiteBg.release();
	outImg.release();
	MouseWheelFlag = 1;
}




// 검사 결과 표시
void CMFCMoneySenseDlg::AddListCtrl(const string bill, const double area, const double areaPercent)
{
	int num = m_ListCtrl.GetItemCount();
	CString CS_bill(bill.c_str()); // std::string을 CString으로 변환
	m_ListCtrl.InsertItem(num, CS_bill);

	if (CS_bill == "-")
	{
	}
	else
	{
		CString CS_area;
		CString CS_areaPercent;
		CString CS_exchange_standard;
		CString CS_faulty = _T("X");
		if (areaPercent >= 75.0) {
			CS_exchange_standard = _T("전액 교환");
			

		}
		else if (areaPercent >= 40.0) {
			CS_exchange_standard = _T("반액 교환");
			CS_faulty = _T("O");
		}
		else {
			CS_exchange_standard = _T("무효 처리");
			CS_faulty = _T("O");
		}



		CS_area.Format(_T("%.2f"), area); // double을 CString으로 변환
		CS_areaPercent.Format(_T("%.2f%%"), areaPercent); // double을 CString으로 변환, 퍼센트 표시 추가

		m_ListCtrl.SetItem(num, 1, LVIF_TEXT, CS_area, NULL, NULL, NULL, NULL);
		m_ListCtrl.SetItem(num, 2, LVIF_TEXT, CS_areaPercent, NULL, NULL, NULL, NULL);
		m_ListCtrl.SetItem(num, 3, LVIF_TEXT, CS_exchange_standard, NULL, NULL, NULL, NULL);
		m_ListCtrl.SetItem(num, 4, LVIF_TEXT, CS_faulty, NULL, NULL, NULL, NULL);
	}

}

// 지폐통에 검사결과 추가
void CMFCMoneySenseDlg::AddListCtrl_SortContainer(const String bill)
{
	CString CS_bill(bill.c_str()); 
	// 특정 행을 찾아 카운트를 증가
	for (int i = 0; i < m_ListCtrl_SortContainer.GetItemCount(); ++i) {
		CString strBillType = m_ListCtrl_SortContainer.GetItemText(i, 0);
		if (strBillType == CS_bill) {
			CString strCount = m_ListCtrl_SortContainer.GetItemText(i, 1);
			int count = _ttoi(strCount); 
			count++;
			CString strNewCount;
			strNewCount.Format(_T("%d"), count);
			m_ListCtrl_SortContainer.SetItemText(i, 1, strNewCount);
			break;
		}
	}
}

void CMFCMoneySenseDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	// 그리기 작업을 처리하기 위한 디바이스 컨텍스트(DC) 객체 생성
	CDC dc;

	// 프레임워크가 전달한 그리기 컨텍스트를 dc 객체에 연결
	dc.Attach(lpDrawItemStruct->hDC);

	// 버튼의 사각형 영역을 가져옴
	RECT rtBtn = lpDrawItemStruct->rcItem;

	// 버튼 배경 색상 설정
	if (nIDCtl == IDC_BUTTON_Stop) // Stop 버튼의 ID
	{
		// Stop 버튼의 배경 색상 설정 (토글 상태에 따라)
		if (m_bButtonStopToggled)
		{
			dc.FillSolidRect(&rtBtn, RGB(20,20,20)); 
		}
		else
		{
			dc.FillSolidRect(&rtBtn, RGB(95,95,95)); 
		}
	}
	else if (nIDCtl == IDC_BUTTON_Select) // Select 버튼의 ID
	{
		// Select 버튼의 배경 색상 설정 (회색)
		dc.FillSolidRect(&rtBtn, RGB(191, 191, 191));
	}
	else if (nIDCtl == IDC_BUTTON_Delete) // Delete 버튼의 ID
	{
		// Delete 버튼의 배경 색상 설정 (회색)
		dc.FillSolidRect(&rtBtn, RGB(191, 191, 191));
	}
	else  // 나머지 메인 버튼
	{
		// 기본 버튼 배경 색상 설정 (짙은 회색)
		dc.FillSolidRect(&rtBtn, RGB(95, 95, 95));
	}

	// 버튼의 3D 테두리 설정
	dc.Draw3dRect(&rtBtn, RGB(255, 255, 255), RGB(0, 0, 0));

	// 버튼의 상태를 가져옴
	UINT nState = lpDrawItemStruct->itemState;

	// 버튼이 선택된 상태인 경우
	if (nState & ODS_SELECTED)
	{
		// 눌린 것처럼 보이도록 버튼 테두리를 안쪽으로 움푹하게 그림
		dc.DrawEdge(&rtBtn, EDGE_SUNKEN, BF_RECT);
	}
	else  // 그렇지 않은 경우
	{
		// 기본 버튼처럼 보이도록 버튼 테두리를 도드라지게 그림
		dc.DrawEdge(&rtBtn, EDGE_RAISED, BF_RECT);
	}

	// 버튼 텍스트를 가져옴
	CString strText;
	GetDlgItem(nIDCtl)->GetWindowText(strText);

	// 텍스트를 그릴 때 배경을 투명하게 설정
	dc.SetBkMode(TRANSPARENT);
	// 텍스트 색상을 흰색으로 설정
	dc.SetTextColor(RGB(255, 255, 255));

	// 버튼의 사각형 영역 안에 텍스트를 가로, 세로 중앙에 단일 줄로 그림
	dc.DrawText(strText, &rtBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	// 그리기 작업이 끝났으므로 디바이스 컨텍스트를 분리
	dc.Detach();
}

// Select 버튼 눌렀을때 - 선택된 결과창의 이미지 표시
void CMFCMoneySenseDlg::OnBnClickedButtonSelect()
{
	POSITION pos = m_ListCtrl.GetFirstSelectedItemPosition();  // 선택된 행의 위치 구함
	if (pos != NULL)
	{
		int idx = m_ListCtrl.GetNextSelectedItem(pos);  // 행의 위치를 int 형으로 변환
		if (idx >= 0 && idx < Resultimgs.size())  // 인덱스가 유효한지 확인
		{
			DisplayImage(Resultimgs[idx], m_PicResult);  // 이미지를 Picture Control에 표시
		}
	}
}

// Delete 버튼 눌렀을때 - 선택된 결과창 결과 삭제
void CMFCMoneySenseDlg::OnBnClickedButtonDelete()
{
	POSITION pos;
	pos = m_ListCtrl.GetFirstSelectedItemPosition();  // 선택된 행의 위치 구함
	int idx = m_ListCtrl.GetNextSelectedItem(pos);  // 행의 위치를 int 형으로 변환

	// 범위 검사
	if (idx >= 0 && idx < Resultimgs.size()) {
		Resultimgs.erase(Resultimgs.begin() + idx);  // 해당 인덱스의 항목을 삭제

		m_ListCtrl.DeleteItem(idx);    // UI에서도 해당 항목 삭제
	}
}


// Reset 버튼 눌렀을때 - 검사결과 리셋
void CMFCMoneySenseDlg::OnBnClickedButtonReset()
{
	// 리스트 컨트롤 초기화
	m_ListCtrl.DeleteAllItems();
	m_ListCtrl_SortContainer.DeleteAllItems();

	// 필요한 데이터만 삽입
	int nItem;

	nItem = m_ListCtrl_SortContainer.InsertItem(0, TEXT("1000"));       // 2행 1열
	m_ListCtrl_SortContainer.SetItemText(nItem, 1, TEXT("0"));           // 2행 2열 - 필요한 경우 개수를 입력

	nItem = m_ListCtrl_SortContainer.InsertItem(1, TEXT("5000"));       // 3행 1열
	m_ListCtrl_SortContainer.SetItemText(nItem, 1, TEXT("0"));           // 3행 2열 - 필요한 경우 개수를 입력

	nItem = m_ListCtrl_SortContainer.InsertItem(2, TEXT("10000"));      // 4행 1열
	m_ListCtrl_SortContainer.SetItemText(nItem, 1, TEXT("0"));           // 4행 2열 - 필요한 경우 개수를 입력

	nItem = m_ListCtrl_SortContainer.InsertItem(3, TEXT("50000"));      // 5행 1열
	m_ListCtrl_SortContainer.SetItemText(nItem, 1, TEXT("0"));           // 5행 2열 - 필요한 경우 개수를 입력
	// 이미지 벡터 초기화
	Resultimgs.clear();

	cv::Mat WhiteImage(600, 600, CV_8UC3, cv::Scalar(255, 255, 255)); // 빨간색 (BGR)

	// 흰색 이미지를 m_PicResult에 표시
	DisplayImage(WhiteImage, m_PicResult);

}

// 이미지 확대 기능
void CMFCMoneySenseDlg::Zoom(float factor)
{
	if (Originalimage.empty()) return;

	CRect rect;
	m_PicResult.GetClientRect(&rect);

	// 비율에 맞는 새로운 크기를 계산합니다.
	float imgAspectRatio = (float)Originalimage.cols / (float)Originalimage.rows;
	float ctrlAspectRatio = (float)rect.Width() / (float)rect.Height();
	int newWidth, newHeight;

	if (imgAspectRatio > ctrlAspectRatio) {
		// 컨트롤의 너비에 맞춥니다.
		newWidth = rect.Width();
		newHeight = static_cast<int>(rect.Width() / imgAspectRatio);
	}
	else {
		// 컨트롤의 높이에 맞춥니다.
		newHeight = rect.Height();
		newWidth = static_cast<int>(rect.Height() * imgAspectRatio);
	}

	// 확대/축소 비율 적용
	newWidth = static_cast<int>(newWidth * factor);
	newHeight = static_cast<int>(newHeight * factor);

	// 최소 크기 제한
	if (newWidth < 50 || newHeight < 50) return;

	cv::resize(Originalimage, Currentimage, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_AREA);

	CDC* pDC = m_PicResult.GetDC();
	CDC memDC;
	memDC.CreateCompatibleDC(pDC);
	CBitmap bitmap;
	bitmap.CreateCompatibleBitmap(pDC, rect.Width(), rect.Height());
	memDC.SelectObject(&bitmap);

	CBrush brush(RGB(255, 255, 255));
	memDC.FillRect(rect, &brush);

	// 이미지를 중앙에 위치시키기 위한 오프셋 계산
	int offsetX = (rect.Width() - newWidth) / 2;
	int offsetY = (rect.Height() - newHeight) / 2;

	DisplayImage(Currentimage, memDC, CRect(offsetX, offsetY, offsetX + newWidth, offsetY + newHeight));

	pDC->BitBlt(0, 0, rect.Width(), rect.Height(), &memDC, 0, 0, SRCCOPY);

	m_PicResult.ReleaseDC(pDC);
	bitmap.DeleteObject();
	memDC.DeleteDC();
}

// 이미지 확대기능 - 마우스 포인터 기준
void CMFCMoneySenseDlg::Zoom(float factor, CPoint center)
{
	if (Originalimage.empty()) return;

	CRect rect;
	m_PicResult.GetClientRect(&rect);
	ScreenToClient(&center); // 마우스 포인터 위치를 클라이언트 영역의 좌표로 변환합니다.

	// 이미지와 컨트롤의 비율을 계산합니다.
	float imgAspectRatio = (float)Originalimage.cols / (float)Originalimage.rows;
	float ctrlAspectRatio = (float)rect.Width() / (float)rect.Height();
	int newWidth, newHeight;

	// 비율에 따라 새로운 크기를 계산합니다.
	if (imgAspectRatio > ctrlAspectRatio) {
		newWidth = rect.Width();
		newHeight = static_cast<int>(rect.Width() / imgAspectRatio);
	}
	else {
		newHeight = rect.Height();
		newWidth = static_cast<int>(rect.Height() * imgAspectRatio);
	}

	// 확대/축소 비율을 적용합니다.
	newWidth = static_cast<int>(newWidth * factor);
	newHeight = static_cast<int>(newHeight * factor);

	if (newWidth < 50 || newHeight < 50) return; // 최소 크기 제한

	cv::resize(Originalimage, Currentimage, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_LINEAR);

	// 마우스 포인터를 중심으로 이미지를 확대/축소합니다.
	float dx = (center.x * factor) - center.x;
	float dy = (center.y * factor) - center.y;

	// 이미지를 출력할 위치를 계산합니다.
	int offsetX = (rect.Width() - newWidth) / 2 - dx;
	int offsetY = (rect.Height() - newHeight) / 2 - dy;

	// 범위를 벗어나지 않도록 조정합니다.
	offsetX = min(max(offsetX, rect.Width() - newWidth), 0);
	offsetY = min(max(offsetY, rect.Height() - newHeight), 0);

	CDC* pDC = m_PicResult.GetDC();
	CDC memDC;
	memDC.CreateCompatibleDC(pDC);
	CBitmap bitmap;
	bitmap.CreateCompatibleBitmap(pDC, rect.Width(), rect.Height());
	memDC.SelectObject(&bitmap);

	CBrush brush(RGB(255, 255, 255));
	memDC.FillRect(rect, &brush);

	// 이미지를 출력합니다.
	DisplayImage(Currentimage, memDC, CRect(offsetX, offsetY, offsetX + newWidth, offsetY + newHeight));

	pDC->BitBlt(0, 0, rect.Width(), rect.Height(), &memDC, 0, 0, SRCCOPY);

	m_PicResult.ReleaseDC(pDC);
	bitmap.DeleteObject();
	memDC.DeleteDC();
}

// 이미지 확대시 다시 그려주는 함수
void CMFCMoneySenseDlg::DisplayImage(cv::Mat& image, CDC& dc, CRect& rect)
{
	// 이미지 비율을 유지하면서 rect 크기에 맞추기 위해 새로운 크기 계산
	double aspectRatio = static_cast<double>(image.cols) / static_cast<double>(image.rows);
	int newWidth = rect.Width();
	int newHeight = static_cast<int>(newWidth / aspectRatio);

	if (newHeight > rect.Height()) {
		newHeight = rect.Height();
		newWidth = static_cast<int>(newHeight * aspectRatio);
	}

	// 이미지의 크기를 조정합니다.
	cv::Mat resizedImg;
	cv::resize(image, resizedImg, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_AREA);

	// 이미지 데이터를 CImage로 변환
	CImage mfcImg;
	mfcImg.Create(resizedImg.cols, resizedImg.rows, 24);
	BITMAPINFO bitInfo;
	ZeroMemory(&bitInfo, sizeof(BITMAPINFO));
	bitInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitInfo.bmiHeader.biWidth = resizedImg.cols;
	bitInfo.bmiHeader.biHeight = -resizedImg.rows; // 이미지가 올바른 방향으로 표시되도록 높이를 음수로 설정
	bitInfo.bmiHeader.biPlanes = 1;
	bitInfo.bmiHeader.biBitCount = 24;
	bitInfo.bmiHeader.biCompression = BI_RGB;

	StretchDIBits(mfcImg.GetDC(), 0, 0, resizedImg.cols, resizedImg.rows, 0, 0, resizedImg.cols, resizedImg.rows, resizedImg.data, &bitInfo, DIB_RGB_COLORS, SRCCOPY);

	// 화면에 이미지 출력
	int offsetX = (rect.Width() - resizedImg.cols) / 2;
	int offsetY = (rect.Height() - resizedImg.rows) / 2;
	mfcImg.BitBlt(dc, rect.left + offsetX, rect.top + offsetY);
	mfcImg.ReleaseDC();
}

// 마우스 휠 이벤트
BOOL CMFCMoneySenseDlg::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (!m_bButtonStopToggled)	// Zoom 버튼이 안눌려있을 경우 리턴
	{
		return CDialogEx::OnMouseWheel(nFlags, zDelta, pt);
	}
	// 마우스 포인터가 스크린 좌표로 제공되므로 이를 클라이언트 좌표로 변환합니다.
	ScreenToClient(&pt);

	// 마우스 포인터가 m_PicResult 컨트롤 위에 있는지 확인
	CRect picRect;
	m_PicResult.GetClientRect(&picRect);
	m_PicResult.ClientToScreen(&picRect);
	ScreenToClient(&picRect);

	if (!picRect.PtInRect(pt)) {
		return CDialogEx::OnMouseWheel(nFlags, zDelta, pt);
	}

	// 마우스 포인터 위치를 컨트롤 내부 좌표로 변환
	CPoint localPt = pt;
	ScreenToClient(&localPt);
	// 마우스 휠 이벤트 처리
	if (zDelta > 0) { // 휠 스크롤 업
		if (MouseWheelFlag == 1) {
			Zoom(1.4f, localPt); // 이미지 확대
			MouseWheelFlag++;
		}
		else if (MouseWheelFlag == 2) {
			Zoom(1.7f, localPt); // 이미지 확대
			MouseWheelFlag++;
		}
		else if (MouseWheelFlag == 3) {
			Zoom(2.0f, localPt); // 이미지 확대
			MouseWheelFlag++;
		}
		else if (MouseWheelFlag == 4) {
			Zoom(3.0f, localPt); // 이미지 확대
			MouseWheelFlag++;
		}
	}
	else { // 휠 스크롤 다운
		if (MouseWheelFlag == 5) {
			Zoom(2.0f, localPt); // 이미지 축소
			MouseWheelFlag--;
		}
		else if (MouseWheelFlag == 4) {
			Zoom(1.7f, localPt); // 이미지 축소
			MouseWheelFlag--;
		}
		else if (MouseWheelFlag == 3) {
			Zoom(1.4f, localPt); // 이미지 축소
			MouseWheelFlag--;
		}
		else if (MouseWheelFlag == 2) {
			Zoom(1.0f); // 이미지 축소 - 원본
			MouseWheelFlag--;
		}
	}

	return CDialogEx::OnMouseWheel(nFlags, zDelta, pt);
}

HBRUSH CMFCMoneySenseDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

	// TODO:  여기서 DC의 특성을 변경합니다.
	if (nCtlColor == CTLCOLOR_STATIC)
	{
		pDC->SetBkColor(RGB(223, 223, 223));
		hbr = ::CreateSolidBrush(RGB(223, 223, 223));
		pDC->SetTextColor(RGB(100, 100, 100)); // 글자색을 하얀색으로 설정
	}
	// TODO:  기본값이 적당하지 않으면 다른 브러시를 반환합니다.
	return hbr;
}
