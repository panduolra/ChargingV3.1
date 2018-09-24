// Page3_SetAddressCAN.cpp : 实现文件
//

#include "stdafx.h"
#include "Page3_SetAddressCAN.h"
#include "afxdialogex.h" 
#include "CommonFunction.h"
#include "common.h"
extern char g_AppPath[256] = { 0 };
char g_szSendBuff[256] = { 0 };
char g_szReceBuff[MAX_BUF_SIZE] = { 0 };
#define MAX_SIZE 1024

// CPage3_SetAddressCAN 对话框

IMPLEMENT_DYNAMIC(CPage3_SetAddressCAN, CDialogEx)

CPage3_SetAddressCAN::CPage3_SetAddressCAN(CWnd* pParent /*=NULL*/)
	: CDialogEx(CPage3_SetAddressCAN::IDD, pParent)
{
	m_canDeviceProcessId = 0;
	m_hPipe = 0;
	m_pParent = pParent;
}

CPage3_SetAddressCAN::~CPage3_SetAddressCAN()
{
}

void CPage3_SetAddressCAN::setStateRect(CRect &rect)
{
	CWnd* pState = (CWnd*)GetDlgItem(IDC_STATIC);
	if (pState)
	{
		pState->MoveWindow(&rect);
	}
}

void CPage3_SetAddressCAN::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CPage3_SetAddressCAN, CDialogEx)
	ON_WM_PAINT()
	ON_BN_CLICKED(IDC_BTN_OPEN_CAN, &CPage3_SetAddressCAN::OnBnClickedBtnOpenCan)
	ON_BN_CLICKED(IDC_BTN_CLOSE_CAN, &CPage3_SetAddressCAN::OnBnClickedBtnCloseCan)
	ON_BN_CLICKED(IDC_BTN_R_ADRESS_CAN, &CPage3_SetAddressCAN::OnBnClickedBtnRAdressCan)
	ON_BN_CLICKED(IDC_BTN_W_ADDRESS_CAN, &CPage3_SetAddressCAN::OnBnClickedBtnWAddressCan)
	ON_EN_CHANGE(IDC_EDIT_CANID, &CPage3_SetAddressCAN::OnEnChangeEditCanid)
	ON_BN_CLICKED(IDC_BTN_Verify, &CPage3_SetAddressCAN::OnBnClickedBtnVerify)
END_MESSAGE_MAP()


// CPage3_SetAddressCAN 消息处理程序


BOOL CPage3_SetAddressCAN::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  在此添加额外的初始化

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常:  OCX 属性页应返回 FALSE
}


void CPage3_SetAddressCAN::OnPaint()
{ 
	CPaintDC dc(this); // device context for painting
	// TODO:  在此处添加消息处理程序代码
	// 不为绘图消息调用 CDialogEx::OnPaint()

	CRect rect;  //矩形
	GetClientRect(&rect);
	rect.bottom -= 5;
	rect.left += 5;
	rect.top += 5;
	rect.right -= 5;
	dc.FillSolidRect(rect, RGB(202, 12, 22));
}


bool CPage3_SetAddressCAN::isPrepareToSendCAN()
{
	if (m_canDeviceProcessId == 0)
	{
		m_pPrintfFun(CString(L"错误CAN进程未打开，不能通讯!\n"));
		return false;
	}
	if (m_hPipe == 0)
	{
		m_pPrintfFun(CString(L"错误CAN进程有名通道未打开，不能通讯!\n"));
		return false;
	}
	return true;
}

void CPage3_SetAddressCAN::OnBnClickedBtnOpenCan()
{
	// TODO:  在此添加控件通知处理程序代码
	CString strPrintf;
	//1 查询充电柜主是否存在进程
	strcpy_s(g_AppPath ,256, COM_F::WStringToMBytes( COM_F::getAppDir().c_str()).c_str());
	std::wstring strPath = COM_F::getAppDir(); strPath += L"\\set.ini";
	wchar_t szMainCharingProcessName[256] = { 0 };
	DWORD result = GetPrivateProfileStringW(
		L"SET",        // INI文件中的一个字段名[节名]可以有很多个节名
		L"windowTitle",        // lpAppName 下的一个键名，也就是里面具体的变量名
		L"",					// 如果为空,则把个变量赋给lpReturnedString
		szMainCharingProcessName,  // 存放键值的指针变量,用于接收INI文件中键值(数据)的接收缓冲区
		256,            // lpReturnedString的缓冲区大小
		strPath.c_str()        // INI文件的路径
		);
	if (wcslen(szMainCharingProcessName) != 0)
	{
		DWORD pid = COM_F::GetProcessidFromName(szMainCharingProcessName);
		if (pid){
			strPrintf.Format(L"进程《%s》 已经存在，请先关闭。\n",szMainCharingProcessName);
			m_pPrintfFun(strPrintf);
			return;
		}
	}

	//2 查找can通讯进程id， 如果存在先强制关闭， 在启动can通讯进程，	
	m_canDeviceProcessId = COM_F::GetProcessidFromName(COM_F::MBytesToWString(CANDEVICETRANSMITION).c_str());
	if (m_canDeviceProcessId)
		COM_F::closeProcessFromId(m_canDeviceProcessId);

	//启动
	char szServerPath[256] = { 0 };
	sprintf_s(szServerPath, 256, "%s/%s", (g_AppPath), CANDEVICETRANSMITION);
	COM_F::startProcessFromPath(COM_F::MBytesToWString(szServerPath).c_str());
	::Sleep(500);
	m_canDeviceProcessId = COM_F::GetProcessidFromName(COM_F::MBytesToWString(CANDEVICETRANSMITION).c_str());

	if (m_canDeviceProcessId <= 0)
	{
		m_pPrintfFun(CString("启动CAN通讯进程失败!"));
		return;
	}
		//3 连接有名管道， 使用重叠io 方式读取
	m_hPipe = COM_F::connectServerNamePipe(TEXT(PIPE_NAME));
	if (INVALID_HANDLE_VALUE == m_hPipe)
	{
		m_pPrintfFun(CString("连接有名通道失败!"));
		return ;
	}

	//打开设备
	sprintf_s(g_szSendBuff, 256, "%s,F1", C2S);  
	sendToCanDeviceProcess(g_szSendBuff, strlen(g_szSendBuff));
	::Sleep(100);
	char szReceive[MAX_SIZE] = { 0 };
	receiveFromCanDeviceProcess(szReceive);
	
	//设置父窗口焦点
	m_pParent->SetFocus();

}


void CPage3_SetAddressCAN::OnBnClickedBtnCloseCan()
{
	// TODO:  在此添加控件通知处理程序代码
	//判断进程id
	if (m_canDeviceProcessId > 0)
	{
		//判断有名通道句柄
		if (m_hPipe > 0)
		{
			//发送进程推出报文
			sprintf_s(g_szSendBuff, 256, "%s,%s", C2S, "FF");
			sendToCanDeviceProcess(g_szSendBuff, strlen(g_szSendBuff));
			
			receiveFromCanDeviceProcess(g_szReceBuff);
			//m_canDeviceProcessId = 0;
			//CloseHandle(m_hPipe);
			//m_hPipe = 0;
			return ;
		}
		if (COM_F::closeProcessFromId(m_canDeviceProcessId))
		{
			m_canDeviceProcessId = 0;
			m_hPipe = 0;
		}
		m_pPrintfFun(CString(L"结束了CAN通讯进程\n"));
	}
	m_pParent->SetFocus();
}


//发送
int CPage3_SetAddressCAN::sendToCanDeviceProcess(char * szData, int nLength)
{
	if (m_hPipe > 0)
	{
		DWORD wlen = 0;
		WriteFile(m_hPipe, szData, nLength, &wlen, 0);//向CAN进程送内容
		CString str = L"发送："; str += szData; str += L"\n";
		m_pPrintfFun(str);
		Sleep(10);
		return wlen;
	}
	return 0;

}

//接收
int CPage3_SetAddressCAN::receiveFromCanDeviceProcess(char * szData)
{
	if (m_hPipe > 0)
	{
		DWORD rlen = 0;
		ReadFile(m_hPipe, szData, MAX_SIZE, &rlen, NULL); //接受CAN进程发送过来的内容
		CString str = L"接收："; str += CString(szData);
		m_pPrintfFun(str);
	}
	return 0;
}


void CPage3_SetAddressCAN::OnBnClickedBtnRAdressCan()
{
	// TODO:  在此添加控件通知处理程序代码
	if (!isPrepareToSendCAN())
		return;
	sprintf_s(g_szSendBuff, 256, "%s,%s,R", C2S, "F3");
	sendToCanDeviceProcess(g_szSendBuff, strlen(g_szSendBuff));
	Sleep(200);
	receiveFromCanDeviceProcess(g_szReceBuff);
	m_pParent->SetFocus();
}


void CPage3_SetAddressCAN::OnBnClickedBtnWAddressCan()
{
	// TODO:  在此添加控件通知处理程序代码
	if (!isPrepareToSendCAN())
		return;

	CString strCanId; 
	GetDlgItem(IDC_EDIT_CANID)->GetWindowText(strCanId);
	if (strCanId.IsEmpty())
	{
		m_pPrintfFun(L"错误：CAN ID 不能为空。\n"); 
		return;
	}
	sprintf_s(g_szSendBuff, 256, "%s,%s,W,%s", C2S, "F3", COM_F::WStringToMBytes(strCanId).c_str());
	sendToCanDeviceProcess(g_szSendBuff, strlen(g_szSendBuff));
	Sleep(200);
	receiveFromCanDeviceProcess(g_szReceBuff);

	m_pParent->SetFocus();
}


void CPage3_SetAddressCAN::OnEnChangeEditCanid()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码

	//m_pParent->SetFocus();
}


void CPage3_SetAddressCAN::OnBnClickedBtnVerify()
{
	// TODO:  在此添加控件通知处理程序代码
	
	if (!isPrepareToSendCAN())
		return;
	CString strCanId;
	GetDlgItem(IDC_EDIT_CANID)->GetWindowText(strCanId);
	if (strCanId.IsEmpty())
	{
		m_pPrintfFun(L"错误：CAN ID 不能为空。\n");
		return;
	}
	sprintf_s(g_szSendBuff, 256, "%s,%s,%s", C2S, "F4", COM_F::WStringToMBytes(strCanId).c_str());
	int len =strlen(g_szSendBuff);
	sendToCanDeviceProcess(g_szSendBuff, len);
	Sleep(200);
	receiveFromCanDeviceProcess(g_szReceBuff);

	m_pParent->SetFocus();
}
