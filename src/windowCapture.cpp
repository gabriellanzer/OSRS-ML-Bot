#include <windowCapture.h>

// Windows dependencies
#include <wingdi.h>
#include <GdiPlus.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ws2_32.lib")

static ULONG_PTR s_gdiplusToken;

void initializeCaptureApi()
{
	// Initialize GDI+
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&s_gdiplusToken, &gdiplusStartupInput, NULL);
}

void shutdownCaptureApi()
{
	// Shutdown GDI+
	Gdiplus::GdiplusShutdown(s_gdiplusToken);
}

void captureScreen(HDC srcHdc, cv::Mat& outMat)
{
    int height = GetDeviceCaps(srcHdc, VERTRES);
    int width = GetDeviceCaps(srcHdc, HORZRES);

    HDC memHdc = CreateCompatibleDC(srcHdc);
    HBITMAP memBit = CreateCompatibleBitmap(srcHdc, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(memHdc, memBit);

    BitBlt(memHdc, 0, 0, width, height, srcHdc, 0, 0, SRCCOPY);

    BITMAPINFOHEADER bi;
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height; // Negative height to indicate top-down bitmap
    bi.biPlanes = 1;
    bi.biBitCount = 24; // 24-bit bitmap (8 bits per channel)
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    static std::vector<BYTE> buffer(width * height * 3); // Buffer to hold bitmap data
    GetDIBits(memHdc, memBit, 0, height, buffer.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);

	// Initialize mat if size does not match
	if (outMat.empty() || outMat.cols != width || outMat.rows != height)
	{
		outMat = cv::Mat(height, width, CV_8UC3);
	}

	// Copy buffer data to cv::Mat using memcpy
	for (int y = 0; y < height; ++y) {
		BYTE* srcRow = buffer.data() + y * width * 3;
		cv::Vec3b* dstRow = outMat.ptr<cv::Vec3b>(y);
		memcpy(dstRow, srcRow, width * 3);
	}

    SelectObject(memHdc, hOldBitmap);
    DeleteObject(memHdc);
    DeleteObject(memBit);
}
