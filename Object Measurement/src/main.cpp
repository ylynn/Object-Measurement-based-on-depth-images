/*******************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.

*******************************************************************************/

//#include "afxcmn.h"
//#include "afxwin.h"


#include <Windows.h>
#include <WindowsX.h>
#include <commctrl.h>
#include "resource1.h"
#include "pxcsession.h"
#include "pxccapture.h"
#include "pxcmetadata.h"
#include "service/pxcsessionservice.h"
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <string>
#include "utilities/pxcmaskutils.h"
#include "PXCBlobData.h"

#include<iostream>
#include<fstream>
#include <vector>
using namespace std;

#define IDC_STATUS 10000
#define ID_DEVICEX 21000
#define ID_MODULEX 22000



HINSTANCE   g_hInst = 0;
PXCSession *g_session = 0;
pxcCHAR g_file[1024] = { 0 };

/* Panel Bitmap */
HBITMAP     g_bitmap = 0;

/* None Gesture */
HBITMAP     g_none = 0;

/* Threading control */
volatile bool g_running = false;
volatile bool g_stop = true;

HANDLE m_thread = NULL;


bool m_currentStatus = false;

float maxRangeValue = 1000;
HPEN lineColor1 = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
HPEN lineColor2 = CreatePen(PS_SOLID, 3, RGB(0, 0, 255));

HPEN red = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
HPEN blue = CreatePen(PS_SOLID, 3, RGB(0, 0, 255));
HPEN green = CreatePen(PS_SOLID, 3, RGB(0, 153, 0));
HPEN lightBlue = CreatePen(PS_SOLID, 3, RGB(0, 255, 255));
HPEN purple = CreatePen(PS_SOLID, 3, RGB(255, 0, 255));
HPEN yellow = CreatePen(PS_SOLID, 3, RGB(255, 255, 0));
HPEN white = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
HPEN darkRed = CreatePen(PS_SOLID, 3, RGB(130, 0, 0));
HBRUSH redBrush = CreateSolidBrush(RGB(255, 0, 0));
HBRUSH greenBrush = CreateSolidBrush(RGB(0, 153, 0));
HBRUSH darkRedBrush = CreateSolidBrush(RGB(130, 0, 0));

pxcI32* m_buffer = NULL;
pxcI32 m_bufferSize = 0;

unsigned char *charBuffer = NULL;

/* Control Layout */
int g_controls[] = { IDC_BLOB, IDC_CONTOUR, IDC_SCALE, IDC_MIRROR, IDC_PARAMS, ID_START, ID_STOP, IDC_FPS, IDC_EDITSPIN, IDC_SPIN, IDC_BlobSmooth, IDC_MaxBlobs, IDC_ContourSmooth, IDC_BLOBDataPoints, IDC_TEXT1, IDC_TEXT2, IDC_TEXT3, IDC_TEXT4, IDC_MAX_DEPTH, IDC_RADIO_BY_SIZE, IDC_RADIO_BY_DISTANCE, IDC_RADIO_BY_DIRECTION, IDC_BLOB_DATA };
RECT g_layout[3 + sizeof(g_controls) / sizeof(g_controls[0])];

void convertTo8bpp(unsigned short * pSrc, int iSize, unsigned char * pDst);
void DrawPoint(HDC dc2, int pointImageX, int pointImageY,/*const pxcCHAR* text,*/HPEN pen, HBRUSH brush);

void SaveLayout(HWND hwndDlg) {
	GetClientRect(hwndDlg, &g_layout[0]);
	ClientToScreen(hwndDlg, (LPPOINT)&g_layout[0].left);
	ClientToScreen(hwndDlg, (LPPOINT)&g_layout[0].right);
	GetWindowRect(GetDlgItem(hwndDlg, IDC_PANEL), &g_layout[1]);
	GetWindowRect(GetDlgItem(hwndDlg, IDC_STATUS), &g_layout[2]);
	for (int i = 0; i<sizeof(g_controls) / sizeof(g_controls[0]); i++)
		GetWindowRect(GetDlgItem(hwndDlg, g_controls[i]), &g_layout[3 + i]);
}

void RedoLayout(HWND hwndDlg) {
	RECT rect;
	GetClientRect(hwndDlg, &rect);

	/* Status */
	SetWindowPos(GetDlgItem(hwndDlg, IDC_STATUS), hwndDlg,
		0,
		rect.bottom - (g_layout[2].bottom - g_layout[2].top),
		rect.right - rect.left,
		(g_layout[2].bottom - g_layout[2].top), SWP_NOZORDER);

	/* Panel */
	SetWindowPos(GetDlgItem(hwndDlg, IDC_PANEL), hwndDlg,
		(g_layout[1].left - g_layout[0].left),
		(g_layout[1].top - g_layout[0].top),
		rect.right - (g_layout[1].left - g_layout[0].left) - (g_layout[0].right - g_layout[1].right),
		rect.bottom - (g_layout[1].top - g_layout[0].top) - (g_layout[0].bottom - g_layout[1].bottom),
		SWP_NOZORDER);

	/* Buttons & CheckBoxes */
	for (int i = 0; i<sizeof(g_controls) / sizeof(g_controls[0]); i++) {
		SetWindowPos(GetDlgItem(hwndDlg, g_controls[i]), hwndDlg,
			rect.right - (g_layout[0].right - g_layout[3 + i].left),
			(g_layout[3 + i].top - g_layout[0].top),
			(g_layout[3 + i].right - g_layout[3 + i].left),
			(g_layout[3 + i].bottom - g_layout[3 + i].top),
			SWP_NOZORDER);
	}
}


static void PopulateDevice(HMENU menu) {
	DeleteMenu(menu, 0, MF_BYPOSITION);

	PXCSession::ImplDesc desc;
	memset(&desc, 0, sizeof(desc));
	desc.group = PXCSession::IMPL_GROUP_SENSOR;
	desc.subgroup = PXCSession::IMPL_SUBGROUP_VIDEO_CAPTURE;
	HMENU menu1 = CreatePopupMenu();
	for (int i = 0, k = ID_DEVICEX;; i++) {
		PXCSession::ImplDesc desc1;
		if (g_session->QueryImpl(&desc, i, &desc1)<PXC_STATUS_NO_ERROR) break;
		PXCCapture *capture = 0;
		if (g_session->CreateImpl<PXCCapture>(&desc1, &capture)<PXC_STATUS_NO_ERROR) continue;
		for (int j = 0;; j++) {
			PXCCapture::DeviceInfo dinfo;
			if (capture->QueryDeviceInfo(j, &dinfo)<PXC_STATUS_NO_ERROR) break;
			AppendMenu(menu1, MF_STRING, k++, dinfo.name);
		}
		capture->Release();
	}
	CheckMenuRadioItem(menu1, 0, GetMenuItemCount(menu1), 0, MF_BYPOSITION);
	InsertMenu(menu, 0, MF_BYPOSITION | MF_POPUP, (UINT_PTR)menu1, L"Device");
}

static int GetChecked(HMENU menu) {
	for (int i = 0; i<GetMenuItemCount(menu); i++)
		if (GetMenuState(menu, i, MF_BYPOSITION)&MF_CHECKED) return i;
	return 0;
}

pxcCHAR* GetCheckedDevice(HWND hwndDlg) {
	HMENU menu = GetSubMenu(GetMenu(hwndDlg), 0);	// ID_DEVICE
	static pxcCHAR line[256];
	GetMenuString(menu, GetChecked(menu), line, sizeof(line) / sizeof(pxcCHAR), MF_BYPOSITION);
	return line;
}

void SetFPSStatus(HWND hwndDlg, pxcCHAR *line) {
	HWND hwndStatus = GetDlgItem(hwndDlg, IDC_FPS);
	SetWindowText(hwndStatus, line);
}




static DWORD WINAPI ThreadProc(LPVOID arg) {

	void SimplePipeline(HWND hwndDlg);
	SimplePipeline((HWND)arg);
	PostMessage((HWND)arg, WM_COMMAND, ID_STOP, 0);
	g_running = false;
	CloseHandle(m_thread);
	return 0;
}

static DWORD WINAPI ThreadProcAdvanced(LPVOID arg) {
	void AdvancedPipeline(HWND hwndDlg);
	AdvancedPipeline((HWND)arg);
	PostMessage((HWND)arg, WM_COMMAND, ID_STOP, 0);
	g_running = false;
	CloseHandle(m_thread);
	return 0;
}

void SetStatus(HWND hwndDlg, const pxcCHAR *line) {
	HWND hwndStatus = GetDlgItem(hwndDlg, IDC_STATUS);
	SetWindowText(hwndStatus, line);
}



bool GetAccessBySizeState(HWND hwndDlg) {

	return (Button_GetState(GetDlgItem(hwndDlg, IDC_RADIO_BY_SIZE))&BST_CHECKED);
}

bool GetAccessByDirectionState(HWND hwndDlg) {

	return (Button_GetState(GetDlgItem(hwndDlg, IDC_RADIO_BY_DIRECTION))&BST_CHECKED);
}

bool GetAccessByDistanceState(HWND hwndDlg) {

	return (Button_GetState(GetDlgItem(hwndDlg, IDC_RADIO_BY_DISTANCE))&BST_CHECKED);
}


bool GetContourState(HWND hwndDlg) {

	return (Button_GetState(GetDlgItem(hwndDlg, IDC_CONTOUR))&BST_CHECKED);
}

bool GetBlobState(HWND hwndDlg) {

	return (Button_GetState(GetDlgItem(hwndDlg, IDC_BLOB))&BST_CHECKED);
}



bool GetPlaybackState(HWND hwndDlg) {
	return (GetMenuState(GetMenu(hwndDlg), ID_MODE_PLAYBACK, MF_BYCOMMAND)&MF_CHECKED) != 0;
}




void ClearBuffer(PXCImage::ImageInfo info)
{

	int bufferSize = info.width * info.height;
	if (bufferSize != m_bufferSize)
	{
		m_bufferSize = bufferSize;
		if (m_buffer) delete[] m_buffer;
		m_buffer = new pxcI32[m_bufferSize];
	}

	if (m_bufferSize>0){
		memset(m_buffer, 0, m_bufferSize*sizeof(pxcI32));
	}
}

void SetMask(PXCImage* image, pxcI32 color)
{
	PXCImage::ImageInfo info = image->QueryInfo();
	PXCImage::ImageData data;
	if (image->AcquireAccess(PXCImage::ACCESS_READ, PXCImage::PIXEL_FORMAT_Y8, &data) == PXC_STATUS_NO_ERROR)
	{
		BITMAPINFO binfo;
		memset(&binfo, 0, sizeof(binfo));
		binfo.bmiHeader.biWidth = (int)info.width;
		binfo.bmiHeader.biHeight = -(int)info.height;
		binfo.bmiHeader.biBitCount = 32;
		binfo.bmiHeader.biPlanes = 1;
		binfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		binfo.bmiHeader.biCompression = BI_RGB;

		int bufferSize = info.width * info.height;
		if (bufferSize != m_bufferSize)
		{
			m_bufferSize = bufferSize;
			if (m_buffer) delete[] m_buffer;
			m_buffer = new pxcI32[m_bufferSize];
			memset(m_buffer, 0, m_bufferSize*sizeof(pxcI32));
		}

		pxcI32 pitch = data.pitches[0];
		pxcBYTE* row = (pxcBYTE*)data.planes[0];
		pxcI32* dst = m_buffer;
		for (int j = 0; j<-binfo.bmiHeader.biHeight; j++){
			for (int j = 0; j<binfo.bmiHeader.biWidth; j++)
			{
				if (row[j] != 0){
					unsigned char* rgb = (unsigned char*)dst;
					rgb[0] = color;
					rgb[1] = color;
					rgb[2] = color;
					rgb[3] = color;
				}
				dst++;
			}
			row += pitch;
		}

		image->ReleaseAccess(&data);
	}
}

void string2wchar_t(wchar_t* wchar, const std::string &str)
{
	int index = 0;
	while (index < (int)str.size())
	{
		wchar[index] = (wchar_t)str[index];
		++index;
	}
	wchar[index] = 0;
}

void DrawBlobNumber(HWND hwndDlg, PXCPoint3DF32 centerPoint, int blobNumber)
{
	if (!g_bitmap) return;

	HWND hwndPanel = GetDlgItem(hwndDlg, IDC_PANEL);
	HDC dc = GetDC(hwndPanel);
	if (dc == NULL){
		return;
	}
	HDC dc2 = CreateCompatibleDC(dc);
	if (dc2 == NULL){
		ReleaseDC(hwndDlg, dc);
		return;
	}
	SelectObject(dc2, g_bitmap);

	BITMAP bm;
	GetObject(g_bitmap, sizeof(bm), &bm);



	std::string s = std::to_string(blobNumber);
	pxcCHAR arr[1000];
	string2wchar_t(arr, s);
	TextOut(dc2, centerPoint.x + 2, centerPoint.y + 2, arr, 2);

	DeleteDC(dc2);
	ReleaseDC(hwndPanel, dc);
}

void DrawExtremityPoint(HWND hwndDlg, PXCBlobData::IBlob* blobData, int blobNumber, std::vector<PXCPoint3DF32> vertices,int width)
{
	float length;
	if (!g_bitmap) return ;

	HWND hwndPanel = GetDlgItem(hwndDlg, IDC_PANEL);
	HDC dc = GetDC(hwndPanel);
	if (dc == NULL){
		return ;
	}
	HDC dc2 = CreateCompatibleDC(dc);
	if (dc2 == NULL){
		ReleaseDC(hwndDlg, dc);
		
	}
	SelectObject(dc2, g_bitmap);

	BITMAP bm;
	GetObject(g_bitmap, sizeof(bm), &bm);

	PXCPoint3DF32 point;
	point = blobData->QueryExtremityPoint(PXCBlobData::EXTREMITY_BOTTOM_MOST);

	int pointImageX = (int)point.x;
	int pointImageY = (int)point.y;
	int bottomX = (int)point.x;
	int bottomY = (int)point.y;
	float BX = vertices[bottomY*width + bottomX].x;
	float BY = vertices[bottomY*width + bottomX].y;
	float BZ = vertices[bottomY*width + bottomX].z;

	

	ofstream fout;
	fout.open("test.txt", ios::app);
	fout << "verticesize" << sizeof(vertices) << endl;
	fout << "bottom:(" << BX << ',' << BY << ','<<BZ<<')';
	DrawPoint(dc2, pointImageX, pointImageY, green, greenBrush);

	point = blobData->QueryExtremityPoint(PXCBlobData::EXTREMITY_TOP_MOST);

	pointImageX = (int)point.x;
	pointImageY = (int)point.y;
	int topX = (int)point.x;
	int topY = (int)point.y;
	float TX = vertices[topY*width + topX].x;
	float TY = vertices[topY*width + topX].y;
	float TZ = vertices[topY*width + topX].z;
	fout << "top:(" << TX << ',' << TY << ','<<TZ<<')';
	DrawPoint(dc2, pointImageX, pointImageY, green, greenBrush);

	point = blobData->QueryExtremityPoint(PXCBlobData::EXTREMITY_RIGHT_MOST);

	pointImageX = (int)point.x;
	pointImageY = (int)point.y;
	int rightX = (int)point.x;
	int rightY = (int)point.y;
	float RX = vertices[rightY*width + rightX].x;
	float RY = vertices[rightY*width + rightX].y;
	float RZ = vertices[rightY*width + rightX].z;

	fout << "right:(" << rightX << ',' << RY <<','<<RZ<< ')';
	DrawPoint(dc2, pointImageX, pointImageY, green, greenBrush);

	point = blobData->QueryExtremityPoint(PXCBlobData::EXTREMITY_LEFT_MOST);

	pointImageX = (int)point.x;
	pointImageY = (int)point.y;
	int leftX = (int)point.x;
	int leftY = (int)point.y;
	float LX = vertices[leftY*width + leftX].x;
	float LY = vertices[leftY*width + leftX].y;
	float LZ = vertices[leftY*width + leftX].z;
	fout << "left:(" << leftX << ',' << LY <<','<<LZ<< ')'<<endl;
	length = abs(LX - RX);
	float wwidth = abs(TY - BY);
	fout << "length:"<<length << endl;
	fout << "width:" << wwidth << endl;
	fout << "size:" <<abs( LX - RX )<< '*' << abs(TY - BY) << endl;
	HWND hwndValue = GetDlgItem(hwndDlg, IDC_BlobSmooth);
	EnableWindow(hwndValue, true);
	wchar_t line[256];
	swprintf_s(line, L"%.2f", length);
	SetWindowText(hwndValue, line);

	HWND hwndValue1 = GetDlgItem(hwndDlg, IDC_MaxBlobs);
	EnableWindow(hwndValue1, true);
	wchar_t line1[256];
	swprintf_s(line1, L"%.2f", wwidth);
	SetWindowText(hwndValue1, line1);





	DrawPoint(dc2, pointImageX, pointImageY, green, greenBrush);

	DrawPoint(dc2, leftX, topY, green, greenBrush);
	DrawPoint(dc2, rightX, topY, green, greenBrush);
	DrawPoint(dc2, leftX, bottomY, green, greenBrush);
	DrawPoint(dc2, rightX, bottomY, green, greenBrush);

	for (int i = leftX; i > rightX; i--){
		DrawPoint(dc2, i, topY, green, greenBrush);
		DrawPoint(dc2, i, bottomY, green, greenBrush);
	}

	for (int i = topY; i <bottomY; i++){
		DrawPoint(dc2, leftX, i, green, greenBrush);
		DrawPoint(dc2, rightX, i, green, greenBrush);
	}

	point = blobData->QueryExtremityPoint(PXCBlobData::EXTREMITY_CENTER);

	pointImageX = (int)point.x;
	pointImageY = (int)point.y;
	DrawPoint(dc2, pointImageX, pointImageY, darkRed, darkRedBrush);

	point = blobData->QueryExtremityPoint(PXCBlobData::EXTREMITY_CLOSEST);

	pointImageX = (int)point.x;
	pointImageY = (int)point.y;
	int pointImageZ = (int)point.z;
	float CX = vertices[pointImageY*width + pointImageX].x;
	float CY = vertices[pointImageY*width + pointImageX].y;
	float CZ = vertices[pointImageY*width + pointImageX].z;
	fout << "closest:(" << CX << ',' << CY << ',' << CZ << ')' << endl;
	float dmax = 0;
	float dmin = CZ;
	
	for (int i = (rightX<leftX ? rightX : leftX); i <( rightX>leftX ? rightX : leftX); i++){
		for (int j = (bottomY<topY?bottomY:topY); j < (bottomY>topY?bottomY:topY); j++){
			if (vertices[j*width + i].z > dmax) { dmax = vertices[j*width + i].z; }
			if (vertices[j*width + i].z < dmin) { dmin = vertices[j*width + i].z; }
		}
	}
	fout << "dmax" << dmax << "dmin" << dmin << endl;
	float ddepth = dmax-CZ;
	HWND hwndValue2 = GetDlgItem(hwndDlg, IDC_MAX_DEPTH);
	EnableWindow(hwndValue2, true);
	wchar_t line2[256];
	swprintf_s(line2, L"%.2f", ddepth);
	SetWindowText(hwndValue2, line2);
	fout << endl;
	fout.close();
	DrawPoint(dc2, pointImageX, pointImageY, red, redBrush);




	DeleteDC(dc2);
	ReleaseDC(hwndPanel, dc);
	
}//end function


void DrawPoint(HDC dc2, int pointImageX, int pointImageY,/*const pxcCHAR* text,*/HPEN pen, HBRUSH brush)
{
	int sz=1 ;

	SelectObject(dc2, pen);
	SelectObject(dc2, brush);
	//Pie(dc2,pointImageX-sz,pointImageY-sz,pointImageX+sz,pointImageY+sz,pointImageX+sz,pointImageY+sz,pointImageX+sz,pointImageY+sz);
	Ellipse(dc2, pointImageX - sz, pointImageY - sz, pointImageX + sz, pointImageY + sz);

	/*SetDCPenColor(dc2, RGB(255, 255, 255));
	TextOut(dc2, pointImageX, pointImageY, text, 1 );*/
}



void DrawContour(HWND hwndDlg, pxcI32 accSize, PXCPointI32* point, int blobNumber)
{
	if (!g_bitmap) return;

	HWND hwndPanel = GetDlgItem(hwndDlg, IDC_PANEL);
	HDC dc = GetDC(hwndPanel);
	if (dc == NULL){
		return;
	}
	HDC dc2 = CreateCompatibleDC(dc);
	if (dc2 == NULL){
		ReleaseDC(hwndDlg, dc);
		return;
	}
	SelectObject(dc2, g_bitmap);

	BITMAP bm;
	GetObject(g_bitmap, sizeof(bm), &bm);
	HPEN lineColor = blue;


	SelectObject(dc2, lineColor);

	if (point != NULL && accSize>0)
	{
		for (int i = 0; i < accSize; ++i)
		{
			int currentX = point[i].x;
			int currentY = point[i].y;
			MoveToEx(dc2, currentX, currentY, 0);

			if (i + 1<accSize)
			{
				int nextX = point[i + 1].x;
				int nextY = point[i + 1].y;
				LineTo(dc2, nextX, nextY);
				MoveToEx(dc2, nextX, nextY, 0);
			}
			else
			{
				int lastX = point[0].x;
				int lastY = point[0].y;
				LineTo(dc2, lastX, lastY);
				MoveToEx(dc2, lastX, lastY, 0);
			}
		}
	}

	DeleteDC(dc2);
	ReleaseDC(hwndPanel, dc);
}




void DrawBitmap(HWND hwndDlg, PXCImage *image) {
	if (g_bitmap) {
		DeleteObject(g_bitmap);
		g_bitmap = 0;
	}
	PXCImage::ImageInfo info = image->QueryInfo();
	PXCImage::ImageData data;

	if (!charBuffer)
	{
		charBuffer = new unsigned char[info.width*info.height * 4];
	}

	if (info.format == PXCImage::PIXEL_FORMAT_DEPTH || info.format == PXCImage::PIXEL_FORMAT_Y16 || info.format == PXCImage::PIXEL_FORMAT_RGB32)
	{
		if (image->AcquireAccess(PXCImage::ACCESS_READ, PXCImage::PIXEL_FORMAT_RGB32, &data) >= PXC_STATUS_NO_ERROR)
		{
			HWND hwndPanel = GetDlgItem(hwndDlg, IDC_PANEL);
			HDC dc = GetDC(hwndPanel);
			if (dc == NULL){ return; }
			BITMAPINFO binfo;
			memset(&binfo, 0, sizeof(binfo));
			binfo.bmiHeader.biWidth = (int)info.width;
			binfo.bmiHeader.biHeight = -(int)info.height;
			binfo.bmiHeader.biBitCount = 32;
			binfo.bmiHeader.biPlanes = 1;
			binfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			binfo.bmiHeader.biCompression = BI_RGB;

			convertTo8bpp((unsigned short*)data.planes[0], info.width*info.height, charBuffer);
			g_bitmap = CreateDIBitmap(dc, &binfo.bmiHeader, CBM_INIT, charBuffer, &binfo, DIB_RGB_COLORS);

			ReleaseDC(hwndPanel, dc);
			image->ReleaseAccess(&data);
		}
	}
	if (info.format == PXCImage::PIXEL_FORMAT_Y8)
	{
		HWND hwndPanel = GetDlgItem(hwndDlg, IDC_PANEL);
		HDC dc = GetDC(hwndPanel);
		if (dc == NULL){ return; }
		BITMAPINFO binfo;
		memset(&binfo, 0, sizeof(binfo));
		binfo.bmiHeader.biWidth = (int)info.width;
		binfo.bmiHeader.biHeight = -(int)info.height;
		binfo.bmiHeader.biBitCount = 32;
		binfo.bmiHeader.biPlanes = 1;
		binfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		binfo.bmiHeader.biCompression = BI_RGB;

		g_bitmap = CreateDIBitmap(dc, &binfo.bmiHeader, CBM_INIT, m_buffer, &binfo, DIB_RGB_COLORS);
		ReleaseDC(hwndPanel, dc);
	}
}

void convertTo8bpp(unsigned short * pSrc, int iSize, unsigned char * pDst)
{
	float fMaxValue = maxRangeValue;
	unsigned char cVal;
	for (int i = 0; i < iSize; i++, pSrc++, pDst += 4)
	{
		cVal = (unsigned char)((*pSrc) / fMaxValue * 255);
		if (cVal != 0)
			cVal = 255 - cVal;

		pDst[0] = cVal;
		pDst[1] = cVal;
		pDst[2] = cVal;
		pDst[3] = 255;
	}
}

static HBITMAP ResizeBitmap(HWND hwnd, HBITMAP bitmap)
{
	RECT rect;
	GetClientRect(hwnd, &rect);

	BITMAP bm;
	GetObject(bitmap, sizeof(BITMAP), &bm);

	HDC dc = GetDC(hwnd);
	if (dc == NULL){
		return NULL;
	}
	HDC dc2 = CreateCompatibleDC(dc);
	if (dc2 == NULL){
		ReleaseDC(hwnd, dc);
		return NULL;
	}
	SelectObject(dc2, bitmap);

	HDC dc3 = CreateCompatibleDC(dc);
	if (dc3 == NULL){
		DeleteDC(dc2);
		ReleaseDC(hwnd, dc);
		return NULL;
	}

	HBITMAP bitmap2 = CreateCompatibleBitmap(dc, rect.right, rect.bottom);

	SelectObject(dc3, bitmap2);
	ReleaseDC(hwnd, dc);

	SetStretchBltMode(dc3, HALFTONE);
	StretchBlt(dc3, 0, 0, rect.right, rect.bottom, dc2, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

	DeleteDC(dc3);
	DeleteDC(dc2);
	return bitmap2;
}

static RECT GetResizeRect(RECT rc, BITMAP bm) { /* Keep the aspect ratio */
	RECT rc1;
	float sx = (float)rc.right / (float)bm.bmWidth;
	float sy = (float)rc.bottom / (float)bm.bmHeight;
	float sxy = sx<sy ? sx : sy;
	rc1.right = (int)(bm.bmWidth*sxy);
	rc1.left = (rc.right - rc1.right) / 2 + rc.left;
	rc1.bottom = (int)(bm.bmHeight*sxy);
	rc1.top = (rc.bottom - rc1.bottom) / 2 + rc.top;
	return rc1;
}

void UpdatePanel(HWND hwndDlg) {
	if (!g_bitmap) return;

	HWND panel = GetDlgItem(hwndDlg, IDC_PANEL);
	RECT rc;
	GetClientRect(panel, &rc);

	HDC dc = GetDC(panel);
	if (dc == NULL){
		return;
	}

	HDC dc2 = CreateCompatibleDC(dc);
	if (dc2 == NULL){
		ReleaseDC(hwndDlg, dc);
		return;
	}

	HBITMAP bitmap = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
	if (bitmap == NULL)
	{
		DeleteDC(dc2);
		ReleaseDC(hwndDlg, dc);
		return;
	}

	SelectObject(dc2, bitmap);
	FillRect(dc2, &rc, (HBRUSH)GetStockObject(GRAY_BRUSH));
	SetStretchBltMode(dc2, HALFTONE);

	/* Draw the main window */
	HDC dc3 = CreateCompatibleDC(dc);
	if (dc3 == NULL){
		DeleteDC(dc2);
		DeleteObject(bitmap);
		ReleaseDC(hwndDlg, dc);
		return;
	}
	SelectObject(dc3, g_bitmap);
	BITMAP bm;
	GetObject(g_bitmap, sizeof(BITMAP), &bm);

	bool scale = Button_GetState(GetDlgItem(hwndDlg, IDC_SCALE))&BST_CHECKED;
	bool mirror = Button_GetState(GetDlgItem(hwndDlg, IDC_MIRROR))&BST_CHECKED;
	if (mirror) {
		if (scale) {
			RECT rc1 = GetResizeRect(rc, bm);
			StretchBlt(dc2, rc1.left + rc1.right - 1, rc1.top, -rc1.right, rc1.bottom, dc3, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
		}
		else {
			StretchBlt(dc2, bm.bmWidth - 1, 0, -bm.bmWidth, bm.bmHeight, dc3, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
		}
	}
	else {
		if (scale) {
			RECT rc1 = GetResizeRect(rc, bm);
			StretchBlt(dc2, rc1.left, rc1.top, rc1.right, rc1.bottom, dc3, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
		}
		else {
			BitBlt(dc2, 0, 0, rc.right, rc.bottom, dc3, 0, 0, SRCCOPY);
		}
	}

	DeleteDC(dc3);
	DeleteDC(dc2);
	ReleaseDC(hwndDlg, dc);

	HBITMAP bitmap2 = (HBITMAP)SendMessage(panel, STM_GETIMAGE, 0, 0);
	if (bitmap2) DeleteObject(bitmap2);
	SendMessage(panel, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bitmap);
	InvalidateRect(panel, 0, TRUE);

	DeleteObject(bitmap);
}



static void GetPlaybackFile(void) {
	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFilter = L"RSSDK clip (*.rssdk)\0*.rssdk\0Old format clip (*.pcsdk)\0*.pcsdk\0All Files (*.*)\0*.*\0\0";
	ofn.lpstrFile = g_file; g_file[0] = 0;
	ofn.nMaxFile = sizeof(g_file) / sizeof(pxcCHAR);
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
	if (!GetOpenFileName(&ofn)) g_file[0] = 0;
}


INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM) {
	HMENU menu = GetMenu(hwndDlg);
	HMENU menu1;
	bool currentStatus = false;


	switch (message) {
	case WM_INITDIALOG:
		CheckDlgButton(hwndDlg, IDC_BLOB, BST_CHECKED);
		CheckDlgButton(hwndDlg, IDC_SCALE, BST_CHECKED);
		CheckDlgButton(hwndDlg, IDC_MIRROR, BST_CHECKED);
		CheckDlgButton(hwndDlg, IDC_PARAMS, BST_CHECKED);
		CheckDlgButton(hwndDlg, IDC_RADIO_BY_SIZE, BST_CHECKED);
		PopulateDevice(menu);
		SaveLayout(hwndDlg);
		return TRUE;
	case WM_COMMAND:
		menu1 = GetSubMenu(menu, 0);
		if (LOWORD(wParam) >= ID_DEVICEX && LOWORD(wParam)<ID_DEVICEX + GetMenuItemCount(menu1)) {
			CheckMenuRadioItem(menu1, 0, GetMenuItemCount(menu1), LOWORD(wParam) - ID_DEVICEX, MF_BYPOSITION);
			return TRUE;
		}
		menu1 = GetSubMenu(menu, 1);
		if (LOWORD(wParam) >= ID_MODULEX && LOWORD(wParam)<ID_MODULEX + GetMenuItemCount(menu1)) {
			CheckMenuRadioItem(menu1, 0, GetMenuItemCount(menu1), LOWORD(wParam) - ID_MODULEX, MF_BYPOSITION);
			return TRUE;
		}
		switch (LOWORD(wParam)) {
		case IDCANCEL:
			g_stop = true;
			if (g_running) {
				PostMessage(hwndDlg, WM_COMMAND, IDCANCEL, 0);
			}
			else {
				DestroyWindow(hwndDlg);
				PostQuitMessage(0);
			}
			return TRUE;
		case IDC_BLOBDataPoints:
			currentStatus = (Button_GetState(GetDlgItem(hwndDlg, IDC_BLOBDataPoints))&BST_CHECKED);
			if (currentStatus == true && m_currentStatus == false)
			{
				CheckDlgButton(hwndDlg, IDC_BLOBDataPoints, BST_CHECKED);
				m_currentStatus = true;
			}
			else if (currentStatus == false && m_currentStatus == true)
			{
				CheckDlgButton(hwndDlg, IDC_BLOBDataPoints, BST_UNCHECKED);
				m_currentStatus = false;
			}
			else
			{
				CheckDlgButton(hwndDlg, IDC_BLOBDataPoints, BST_UNCHECKED);
				m_currentStatus = false;
			}
			return TRUE;
		case ID_START:
			Button_Enable(GetDlgItem(hwndDlg, ID_START), false);
			Button_Enable(GetDlgItem(hwndDlg, ID_STOP), true);
			for (int i = 0; i<GetMenuItemCount(menu); i++)
				EnableMenuItem(menu, i, MF_BYPOSITION | MF_GRAYED);
			DrawMenuBar(hwndDlg);
			g_stop = false;
			g_running = true;
#if 1
			m_thread = CreateThread(0, 0, ThreadProc, hwndDlg, 0, 0);
#else
			m_thread = CreateThread(0, 0, ThreadProcAdvanced, hwndDlg, 0, 0);
#endif
			Sleep(0);
			return TRUE;
		case ID_STOP:
			g_stop = true;
			if (g_running) {
				PostMessage(hwndDlg, WM_COMMAND, ID_STOP, 0);
			}
			else {
				for (int i = 0; i<GetMenuItemCount(menu); i++)
					EnableMenuItem(menu, i, MF_BYPOSITION | MF_ENABLED);
				DrawMenuBar(hwndDlg);
				Button_Enable(GetDlgItem(hwndDlg, ID_START), true);
				Button_Enable(GetDlgItem(hwndDlg, ID_STOP), false);
			}
			return TRUE;
		case ID_MODE_LIVE:
			CheckMenuItem(menu, ID_MODE_LIVE, MF_CHECKED);
			CheckMenuItem(menu, ID_MODE_PLAYBACK, MF_UNCHECKED);
			CheckMenuItem(menu, ID_MODE_RECORD, MF_UNCHECKED);
			EnableWindow(GetDlgItem(hwndDlg, IDC_EDITSPIN), false);
			return TRUE;
		case ID_MODE_PLAYBACK:
			CheckMenuItem(menu, ID_MODE_LIVE, MF_UNCHECKED);
			CheckMenuItem(menu, ID_MODE_PLAYBACK, MF_CHECKED);
			CheckMenuItem(menu, ID_MODE_RECORD, MF_UNCHECKED);
			EnableWindow(GetDlgItem(hwndDlg, IDC_EDITSPIN), false);
			GetPlaybackFile();
			return TRUE;
			/*case IDC_EDIT_GREEN:
			HDC hdcStatic = (HDC) wParam;
			SetTextColor(hdcStatic, RGB(255,255,255));
			SetBkColor(hdcStatic, RGB(255,0,0));
			return TRUE;*/

		}
		break;
	case WM_TIMER:
		SendMessage(GetDlgItem(hwndDlg, wParam), STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)g_none);
		KillTimer(hwndDlg, wParam);
		return TRUE;
	case WM_SIZE:
		RedoLayout(hwndDlg);
		return TRUE;

	}





	return FALSE;
}

#pragma warning(disable:4706) /* assignment within conditional */
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int)
{
	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	LPWSTR *szArgList;
	int argCount;

	szArgList = CommandLineToArgvW(GetCommandLine(), &argCount);

	InitCommonControls();
	g_hInst = hInstance;

	g_session = PXCSession::CreateInstance();
	if (g_session == NULL) {
		MessageBoxW(0, L"Failed to create an SDK session", L"Mask Utils", MB_ICONEXCLAMATION | MB_OK);
		return 1;
	}

	


	/* Optional steps to send feedback to Intel Corporation to understand how often each SDK sample is used. */
	PXCMetadata * md = g_session->QueryInstance<PXCMetadata>();
	if (md) {
		pxcCHAR sample_name[] = L"Blob Viewer";
		md->AttachBuffer(PXCSessionService::FEEDBACK_SAMPLE_INFO, (pxcBYTE*)sample_name, sizeof(sample_name));
	}

	

	HWND hWnd = CreateDialogW(hInstance, MAKEINTRESOURCE(IDD_MAINFRAME), 0, DialogProc);
	if (!hWnd)  {
		MessageBoxW(0, L"Failed to create a window", L"Mask Utils", MB_ICONEXCLAMATION | MB_OK);
		return 1;
	}

	HWND hwndValue = GetDlgItem(hWnd, IDC_BlobSmooth);
	wchar_t line[256];
	swprintf_s(line, L"%.2f", 0.0);
	SetWindowText(hwndValue, line);

	HWND hwndMAxBlobsValue = GetDlgItem(hWnd, IDC_MaxBlobs);
	wchar_t lineMaxBlobs[256];
	swprintf_s(lineMaxBlobs, L"%.2f", 0.0);
	SetWindowText(hwndMAxBlobsValue, lineMaxBlobs);

	HWND hwndContourSmoothValue = GetDlgItem(hWnd, IDC_ContourSmooth);
	wchar_t lineContourSmooth[256];
	swprintf_s(lineContourSmooth, L"%d", 0);
	SetWindowText(hwndContourSmoothValue, lineContourSmooth);

	HWND hwndMaxDepth = GetDlgItem(hWnd, IDC_MAX_DEPTH);
	wchar_t lineMaxDepth[256];
	swprintf_s(lineMaxDepth, L"%.2f", 0.0);
	SetWindowText(hwndMaxDepth, lineMaxDepth);
	//SetTextColor((HDC)hwndMaxDepth, RGB(255, 0, 0));

	HWND hWnd2 = CreateStatusWindow(WS_CHILD | WS_VISIBLE, L"OK", hWnd, IDC_STATUS);
	if (!hWnd2) {
		MessageBoxW(0, L"Failed to create a status bar", L"Mask Utils", MB_ICONEXCLAMATION | MB_OK);
		return 1;
	}

	UpdateWindow(hWnd);

	MSG msg;
	for (int sts; (sts = GetMessageW(&msg, NULL, 0, 0));) {
		if (sts == -1) return sts;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	g_stop = true;
	while (g_running) Sleep(5);
	g_session->Release();
	if (charBuffer)
		delete[] charBuffer;
	return (int)msg.wParam;
}

