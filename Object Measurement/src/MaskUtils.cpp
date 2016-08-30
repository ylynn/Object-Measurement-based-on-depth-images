/*******************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.

*******************************************************************************/
#include <Windows.h>
#include <WindowsX.h>
#include <vector>
#include "pxcsensemanager.h"
#include "pxccapture.h"
#include "pxcvideomodule.h"
#include "utilities/pxcmaskutils.h"

#include "pxcprojection.h"
#include "pxccapture.h"
#include "pxcsession.h"

#include "pxcblobmodule.h"
#include "pxcblobconfiguration.h"
#include "pxcblobdata.h"
#include "timer.h"
#include "resource1.h"
#include <string>


#include<iostream>
#include<fstream>
#include <vector>
using namespace std;

extern volatile bool g_stop;
extern bool showExtremityPoint;


volatile bool g_connected = false;
extern pxcCHAR g_file[1024];
extern PXCSession *g_session;
int maxBlobToShow;

void SetStatus(HWND hwndDlg, const pxcCHAR *line);
pxcCHAR* GetCheckedDevice(HWND);
bool GetContourState(HWND);
bool GetBlobState(HWND);
bool GetAccessBySizeState(HWND);
bool GetAccessByDirectionState(HWND);
bool GetAccessByDistanceState(HWND);



void SetFPSStatus(HWND hwndDlg, pxcCHAR *line);
void DrawBitmap(HWND, PXCImage*);
void SetMask(PXCImage*, pxcI32);
void ClearBuffer(PXCImage::ImageInfo);
void DrawContour(HWND hwndDlg, pxcI32 accSize, PXCPointI32* point, int blobNumber);
void DrawExtremityPoint(HWND hwndDlg, PXCBlobData::IBlob* blobData, int blobNumber, std::vector<PXCPoint3DF32>, int width);
void DrawBlobNumber(HWND hwndDlg, PXCPoint3DF32 centerPoint, int blobNumber);
void UpdatePanel(HWND);
bool GetPlaybackState(HWND hwndDlg);




/* Checking if sensor device connect or not */
static bool DisplayDeviceConnection(HWND hwndDlg, bool state) {
	if (state) {
		if (!g_connected) SetStatus(hwndDlg, L"Device Reconnected");
		g_connected = true;
	}
	else {
		if (g_connected) SetStatus(hwndDlg, L"Device Disconnected");
		g_connected = false;
	}
	return g_connected;
}



/* Displaying Depth/Mask Images - for depth image only we use a delay of NUMBER_OF_FRAMES_TO_DELAY to sync image with tracking */
static void DisplayPicture(HWND hwndDlg, PXCImage *depth, PXCImage *color,PXCBlobData* blobData, PXCCapture::Device *device)
{
	
	PXCCapture::Device::StreamProfileSet _profiles = {};
	device->QueryStreamProfileSet(PXCCapture::STREAM_TYPE_COLOR | PXCCapture::STREAM_TYPE_DEPTH | PXCCapture::STREAM_TYPE_IR, 0, &_profiles);
	if (!depth) return ;
	//if (!color) return;

	//vertices
	std::vector<PXCPoint3DF32> vertices;
	PXCProjection *p = device->CreateProjection();

	//Obtain vertices data
	vertices.resize(depth->QueryInfo().width * depth->QueryInfo().height);
	PXCImage *imgD = depth;
	pxcStatus sts = p->QueryVertices(imgD, &vertices[0]);
	

	pxcStatus results = PXC_STATUS_NO_ERROR;
	PXCImage* image = depth;
	PXCImage::ImageInfo info = image->QueryInfo();
	ClearBuffer(info);

	PXCImage* image1 = depth;
	//PXCImage::ImageInfo info1 = image1->QueryInfo();
	//ClearBuffer(info1);

	info.format = PXCImage::PIXEL_FORMAT_Y8;
	PXCImage* new_image = g_session->CreateImage(&info);

	//info1.format = PXCImage::PIXEL_FORMAT_RGB32;
	//PXCImage *new_image1 = g_session->CreateImage(&info1);


	PXCImage::ImageData new_bdata;
	results = image->AcquireAccess(PXCImage::ACCESS_WRITE, &new_bdata);
	if (results != PXC_STATUS_NO_ERROR) return ;
	memset(new_bdata.planes[0], 0, new_bdata.pitches[0] * info.height);
	image->ReleaseAccess(&new_bdata);


	int numOfBlobs = blobData->QueryNumberOfBlobs();
	if (maxBlobToShow>numOfBlobs)
	{
		maxBlobToShow = numOfBlobs;
	}



	std::vector<std::vector<PXCPointI32*>> points;
	//std::vector<std::vector<PXCPoint3DF32*>> ppoints;
	std::vector<std::vector<int>> pointsAccualSize;
	std::vector<PXCBlobData::IBlob*> blobDataList;

	points.resize(maxBlobToShow);
	pointsAccualSize.resize(maxBlobToShow);
	blobDataList.resize(maxBlobToShow);

	PXCBlobData::AccessOrderType accessOrder = PXCBlobData::ACCESS_ORDER_NEAR_TO_FAR;
	if (GetAccessByDistanceState(hwndDlg) == true)
	{
		accessOrder = PXCBlobData::ACCESS_ORDER_NEAR_TO_FAR;
	}

	if (GetAccessByDirectionState(hwndDlg) == true)
	{
		accessOrder = PXCBlobData::ACCESS_ORDER_RIGHT_TO_LEFT;
	}


	for (int i = 0; i<maxBlobToShow; i++)
	{
		PXCBlobData::IBlob* blob;
		results = blobData->QueryBlobByAccessOrder(i, accessOrder, blob);

		if (results != PXC_STATUS_NO_ERROR) return ;

		blobDataList[i] = blob;

		results = blob->QuerySegmentationImage(image);
		if (results != PXC_STATUS_NO_ERROR || GetBlobState(hwndDlg) == false)
		{
			DrawBitmap(hwndDlg, image);
		}
		else
		{
			pxcI32 color =  255 - (255 / (maxBlobToShow)* i);
			SetMask(image, color);
			DrawBitmap(hwndDlg, image);

		}


		if (true)
		{

			int numberOfContours = blob->QueryNumberOfContours();


			points[i].resize(numberOfContours);
			std::vector<int> blobPointsSize = pointsAccualSize[i];
			pointsAccualSize[i].resize(numberOfContours);

			if (numberOfContours>0)
			{
				for (int j = 0; j<numberOfContours; ++j)
				{
					pointsAccualSize[i].at(j) = 0;
					int contourSize = blob->QueryContourSize(j);
					points[i].at(j) = 0;
					if (contourSize>0)
					{
						points[i].at(j) = new PXCPointI32[contourSize];
						results = blob->QueryContourPoints(j, contourSize, points[i].at(j));
						if (results != PXC_STATUS_NO_ERROR) continue;
						pointsAccualSize[i].at(j) = contourSize;
					}

				}
			}
		}

	}

	if (results == pxcStatus::PXC_STATUS_NO_ERROR)
	{

		for (int i = 0; i<maxBlobToShow; i++)
		{
			std::vector<PXCPointI32*> blobPoints = points[i];
			std::vector<int> blobPointsSize = pointsAccualSize[i];
		
		


			for (int j = 0; j < blobPointsSize.size(); j++)
			{
				DrawContour(hwndDlg, blobPointsSize[j], blobPoints[j], i);
			}

			if (true)
			{
				DrawExtremityPoint(hwndDlg, blobDataList[i], i + 1,vertices,depth->QueryInfo().width);
			}

			PXCPoint3DF32 point;
			point = blobDataList[i]->QueryExtremityPoint(PXCBlobData::EXTREMITY_CENTER);
			DrawBlobNumber(hwndDlg, point, i + 1);
			PXCPoint3DF32 point1;
			point1 = blobDataList[i]->QueryExtremityPoint(PXCBlobData::EXTREMITY_CLOSEST);
		

		}

	}
	//clear counter points
	size_t pointsLen = points.size();
	for (int k = 0; k<pointsLen; ++k)
	{
		for (int h = 0; h<points[k].size(); ++h)
		{
			if (points[k].at(h) != NULL)
			{
				delete[] points[k].at(h);
				points[k].at(h) = NULL;
			}

		}
		points[k].clear();
	}

	if (numOfBlobs == 0)
	{
		DrawBitmap(hwndDlg, image);
	}
	new_image->Release();
	
	

}



/* Using PXCSenseManager to handle data */
void SimplePipeline(HWND hwndDlg) {

	

	pxcStatus results = PXC_STATUS_NO_ERROR;
	PXCSenseManager *pp = g_session->CreateSenseManager();//
	PXCSenseManager *pp2 = g_session->CreateSenseManager();//
	if (!pp)
	{
		SetStatus(hwndDlg, L"Failed to create SenseManager");
		return;
	}
	if (!pp2)
	{
		SetStatus(hwndDlg, L"Failed to create SenseManager");
		return;
	}
	/* Enable all selected streams in handler */
	//MyHandler handler(hwndDlg, isAdaptive, pp, profiles);


	/* Set Module */
	pxcStatus status = pp->EnableBlob(0);
	pp2->EnableStream(PXCCapture::STREAM_TYPE_COLOR,480, 640, 30);
	pp2->EnableStream(PXCCapture::STREAM_TYPE_DEPTH,480, 640, 30);
	PXCBlobModule *blobModule = pp->QueryBlob();
	if (blobModule == NULL || status != pxcStatus::PXC_STATUS_NO_ERROR)
	{
		SetStatus(hwndDlg, L"Failed to pair the blob module with I/O");
		return;
	}



	/* Set Mode & Source */
	if (GetPlaybackState(hwndDlg)) {
		pp->QueryCaptureManager()->SetFileName(g_file, false);
		pp->QueryCaptureManager()->SetRealtime(false);
		//pp2->QueryCaptureManager()->SetFileName(g_file, false);
		//pp2->QueryCaptureManager()->SetRealtime(false);
	}
	else {
		pp->QueryCaptureManager()->FilterByDeviceInfo(GetCheckedDevice(hwndDlg), 0, 0);
		pp->QueryCaptureManager()->FilterByDeviceInfo(GetCheckedDevice(hwndDlg), 0, 0);
	}





	bool isStop = true;
	/* Init */
	SetStatus(hwndDlg, L"Init Started");
	FPSTimer timer;
	if (pp->Init() == PXC_STATUS_NO_ERROR /*&& pp2->Init() == PXC_STATUS_NO_ERROR*/)
	{

		PXCBlobData* blobData = blobModule->CreateOutput();
		PXCBlobConfiguration* blobConfiguration = blobModule->CreateActiveConfiguration();

		PXCCapture::Device *device = pp->QueryCaptureManager()->QueryDevice();
		//PXCCapture::Device *device2 = pp2->QueryCaptureManager()->QueryDevice();
		PXCCapture::DeviceInfo dinfo;
		device->QueryDeviceInfo(&dinfo);
		//device2->QueryDeviceInfo(&dinfo);
		if (dinfo.model == PXCCapture::DEVICE_MODEL_IVCAM)
		{
			device->SetMirrorMode(PXCCapture::Device::MIRROR_MODE_HORIZONTAL);
			//device2->SetMirrorMode(PXCCapture::Device::MIRROR_MODE_HORIZONTAL);
		}

		

		//pp->EnableStream(PXCCapture::STREAM_TYPE_COLOR);
		//pp->EnableStream(PXCCapture::STREAM_TYPE_DEPTH);



		SetStatus(hwndDlg, L"Streaming");
		g_connected = true;
		while (!g_stop && (isStop = pp->AcquireFrame(true) == PXC_STATUS_NO_ERROR))
		{
			if (DisplayDeviceConnection(hwndDlg, pp->IsConnected()))
			{
				const PXCCapture::Sample *sample = pp->QuerySample();
				//const PXCCapture::Sample *sample2 = pp2->QuerySample();
				if (sample && sample->depth/*&&sample->color*/)
				{
					PXCImage::ImageInfo info = sample->depth->QueryInfo();
					//PXCImage::ImageInfo info1 = sample->color->QueryInfo();


					HWND hwndValue = GetDlgItem(hwndDlg, IDC_BlobSmooth);
					LPWSTR strBlobSmooth = new TCHAR[50];
					GetWindowText(hwndValue, strBlobSmooth, 50);
					float numberBlobSmooth = 1;
					delete[] strBlobSmooth;


					if (numberBlobSmooth >= 0 && numberBlobSmooth <= 1)
					{
						blobConfiguration->SetSegmentationSmoothing(numberBlobSmooth);
					}
					else
					{
						if (numberBlobSmooth<0)
						{
							blobConfiguration->SetSegmentationSmoothing(0);
							SetWindowText(hwndValue, L"0");
						}
						if (numberBlobSmooth>1)
						{
							blobConfiguration->SetSegmentationSmoothing(1);
							SetWindowText(hwndValue, L"1");
						}
					}

					EnableWindow(hwndValue, FALSE);


					hwndValue = GetDlgItem(hwndDlg, IDC_MaxBlobs);
					LPWSTR strMaxBlobs = new TCHAR[50];
					GetWindowText(hwndValue, strMaxBlobs, 50);
					int maxBlobs = 1;
					delete[] strMaxBlobs;
					maxBlobToShow = maxBlobs;
					if (maxBlobs>0 && maxBlobs <= 4)
					{
						blobConfiguration->SetMaxBlobs(maxBlobToShow);
					}
					else
					{
						if (maxBlobs <= 0)
						{
							blobConfiguration->SetMaxBlobs(1);
							SetWindowText(hwndValue, L"1");
						}
						if (maxBlobs>4)
						{
							blobConfiguration->SetMaxBlobs(4);
							SetWindowText(hwndValue, L"4");
						}
					}
					EnableWindow(hwndValue, FALSE);

					hwndValue = GetDlgItem(hwndDlg, IDC_ContourSmooth);
					LPWSTR strContourSmooth = new TCHAR[50];
					GetWindowText(hwndValue, strContourSmooth, 50);
					float numberContourSmooth = _wtof(strContourSmooth);
					delete[] strContourSmooth;

					if (numberContourSmooth >= 0 && numberContourSmooth <= 1)
					{
						blobConfiguration->SetContourSmoothing(numberContourSmooth);
					}
					else
					{
						if (numberContourSmooth<0)
						{
							blobConfiguration->SetContourSmoothing(0);
							SetWindowText(hwndValue, L"0");
						}
						if (numberContourSmooth>1)
						{
							blobConfiguration->SetContourSmoothing(1);
							SetWindowText(hwndValue, L"1");
						}
					}

					EnableWindow(hwndValue, FALSE);

					hwndValue = GetDlgItem(hwndDlg, IDC_MAX_DEPTH);
					LPWSTR strMaxDepth = new TCHAR[50];
					GetWindowText(hwndValue, strMaxDepth, 50);
					float maxDepth = 550;
					delete[] strMaxDepth;
					blobConfiguration->SetMaxDistance(maxDepth);
					
				
					
					



					EnableWindow(hwndValue, FALSE);



					blobConfiguration->EnableContourExtraction(true);
					blobConfiguration->EnableSegmentationImage(true);

					blobConfiguration->ApplyChanges();

					blobData->Update();



					DisplayPicture(hwndDlg, sample->depth,sample->color, blobData,device);
					UpdatePanel(hwndDlg);
					timer.Tick(hwndDlg);
				}
			}
			pp->ReleaseFrame();
		}
		

		

		HWND hwndValue = GetDlgItem(hwndDlg, IDC_BlobSmooth);
		EnableWindow(hwndValue, true);
		wchar_t line[256];
		//swprintf_s(line, L"%.2f", 0);
		//SetWindowText(hwndValue, line);
		hwndValue = GetDlgItem(hwndDlg, IDC_MaxBlobs);
		EnableWindow(hwndValue, true);
		wchar_t line1[256];
		//swprintf_s(line1, L"%.2f", 0);
		//SetWindowText(hwndValue, line1);
		hwndValue = GetDlgItem(hwndDlg, IDC_ContourSmooth);
		EnableWindow(hwndValue, true);
		hwndValue = GetDlgItem(hwndDlg, IDC_MAX_DEPTH);
		EnableWindow(hwndValue, true);
		
		wchar_t line2[256];
		//swprintf_s(line2, L"%.2f", dd);
		//SetWindowText(hwndValue, line2);

		

		


		blobConfiguration->Release();
		blobData->Release();
	}
	//else
	{
		//SetStatus(hwndDlg, L"Init Failed");
		//isStop = false;
	}




	pp->Close();
	pp->Release();

	if (isStop) SetStatus(hwndDlg, L"Stopped");
}



