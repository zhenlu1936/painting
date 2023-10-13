#include <Windowsx.h>
#include <d2d1.h>
#include <windows.h>

#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <sstream>
#include <string>
using namespace std;

#pragma comment(lib, "d2d1")

#include "basewin.h"
#include "resource.h"

enum Mode { LineMode, SquareMode, EllipseMode, SelectMode, DragMode };
// 颜色枚举
D2D1::ColorF::Enum colors[] = {D2D1::ColorF::Yellow, D2D1::ColorF::Salmon,
							   D2D1::ColorF::LimeGreen, D2D1::ColorF::Purple};
template <class T>

// 安全释放指针
void SafeRelease(T** ppT) {
	if (*ppT) {
		(*ppT)->Release();
		*ppT = NULL;
	}
}

// 像素点缩放
class DPIScale {
	static float scaleX;
	static float scaleY;

   public:
	static void Initialize(HWND hwnd) { GetDpiForWindow(hwnd); }

	template <typename T>
	static float PixelsToDipsX(T x) {
		return static_cast<float>(x) / scaleX;
	}

	template <typename T>
	static float PixelsToDipsY(T y) {
		return static_cast<float>(y) / scaleY;
	}
};

float DPIScale::scaleX = 1.0f;
float DPIScale::scaleY = 1.0f;

class Elli;
class Line;
class Square;

// 图形
class Shape {
   public:
	virtual void Draw(ID2D1RenderTarget* pRT, ID2D1SolidColorBrush* pBrush) {}
	virtual void Change(D2D1_POINT_2F ptMouse, float x, float y) {}
	virtual BOOL HitTest(float x, float y) { return FALSE; }
	virtual void Drag(D2D1_POINT_2F ptMouse, float x, float y) {}
	virtual void Save(fstream* myFile) {}
};

// 椭圆
class Elli : public Shape {
   public:
	int color;
	D2D1_ELLIPSE ellipse;
	Elli() : color(0), ellipse(D2D1::Ellipse(D2D1::Point2F(), 0, 0)) {}
	Elli(int Color, D2D1_POINT_2F point, float width, float height)
		: color(Color), ellipse(D2D1::Ellipse(point, width, height)) {}

	static void Insert(D2D1_POINT_2F* ptMouse, list<shared_ptr<Shape>>* shapes,
					   list<shared_ptr<Shape>>::iterator* selection,
					   size_t* nextColor) {	 // 必须插入新分配好的空间！
		Elli* newElli = new Elli(*nextColor, *ptMouse, 0, 0);
		*selection = (*shapes).insert((*shapes).end(),
									  shared_ptr<Shape>(new Elli(*newElli)));
		*nextColor = (*nextColor + 1) % ARRAYSIZE(colors);
		*ptMouse = newElli->ellipse.point;
	}
	void Draw(ID2D1RenderTarget* pRT, ID2D1SolidColorBrush* pBrush) {
		pBrush->SetColor(D2D1::ColorF(colors[color]));
		pRT->FillEllipse(ellipse, pBrush);
		pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
		pRT->DrawEllipse(ellipse, pBrush);
	}
	void Change(D2D1_POINT_2F ptMouse, float x, float y) {
		const float width = (x - ptMouse.x) / 2;
		const float height = (y - ptMouse.y) / 2;
		const float x1 = ptMouse.x + width;
		const float y1 = ptMouse.y + height;
		ellipse = D2D1::Ellipse(D2D1::Point2F(x1, y1), width, height);
	}
	BOOL HitTest(float x, float y) {
		const float a = ellipse.radiusX;
		const float b = ellipse.radiusY;
		const float x1 = x - ellipse.point.x;
		const float y1 = y - ellipse.point.y;
		const float d = ((x1 * x1) / (a * a)) + ((y1 * y1) / (b * b));
		return d <= 1.0f;
	}
	void Drag(D2D1_POINT_2F ptMouse, float x, float y) {
		ellipse.point.x += x - ptMouse.x;
		ellipse.point.y += y - ptMouse.y;
	}
	void Save(fstream* myFile) {
		*myFile << "E"
				<< " ";
		*myFile << to_string(color) << " ";
		*myFile << to_string(ellipse.point.x) << " ";
		*myFile << to_string(ellipse.point.y) << " ";
		*myFile << to_string(ellipse.radiusX) << " ";
		*myFile << to_string(ellipse.radiusY) << "\n";
	}
	static void Read(string myString, list<shared_ptr<Shape>>* shapes) {
		Elli* newElli = new Elli;
		stringstream ins(myString);
		ins >> newElli->color;
		ins >> newElli->ellipse.point.x;
		ins >> newElli->ellipse.point.y;
		ins >> newElli->ellipse.radiusX;
		ins >> newElli->ellipse.radiusY;
		(*shapes).insert((*shapes).end(), shared_ptr<Elli>(new Elli(*newElli)));
	}
};

// 直线
class Line : public Shape {
   public:
	D2D1_POINT_2F point1;
	D2D1_POINT_2F point2;
	int color;
	Line() : point1(D2D1::Point2F()), point2(D2D1::Point2F()) {}
	Line(int Color, D2D1_POINT_2F p1, D2D1_POINT_2F p2)
		: color(Color), point1(p1), point2(p2) {}

	static void Insert(D2D1_POINT_2F* ptMouse, list<shared_ptr<Shape>>* shapes,
					   list<shared_ptr<Shape>>::iterator* selection,
					   size_t* nextColor) {
		Line* newLine = new Line(*nextColor, *ptMouse, *ptMouse);
		*selection = (*shapes).insert((*shapes).end(),
									  shared_ptr<Shape>(new Line(*newLine)));
		*nextColor = (*nextColor + 1) % ARRAYSIZE(colors);
		*ptMouse = newLine->point1;
	}
	void Draw(ID2D1RenderTarget* pRT, ID2D1SolidColorBrush* pBrush) {
		pBrush->SetColor(D2D1::ColorF(colors[color]));
		pRT->DrawLine(point1, point2, pBrush);
	}
	void Change(D2D1_POINT_2F ptMouse, float x, float y) {
		point2 = D2D1::Point2F(x, y);
	}
	BOOL HitTest(float x, float y) {
		const float x1 = point1.x;
		const float x2 = point2.x;
		const float y1 = point1.y;
		const float y2 = point2.y;
		return x <= max(x1, x2) && x >= min(x1, x2) && y <= max(y1, y2) &&
			   y >= min(y1, y2) &&
			   abs((x - x1) / (y - y1) - (x - x2) / (y - y2)) <= 0.05;
	}
	void Drag(D2D1_POINT_2F ptMouse, float x, float y) {
		point1.x += x - ptMouse.x;
		point1.y += y - ptMouse.y;
		point2.x += x - ptMouse.x;
		point2.y += y - ptMouse.y;
	}
	void Save(fstream* myFile) {
		*myFile << "L"
				<< " ";
		*myFile << to_string(color) << " ";
		*myFile << to_string(point1.x) << " ";
		*myFile << to_string(point1.y) << " ";
		*myFile << to_string(point2.x) << " ";
		*myFile << to_string(point2.y) << "\n";
	}
	static void Read(string myString, list<shared_ptr<Shape>>* shapes) {
		Line* newLine = new Line;
		stringstream ins(myString);
		ins >> newLine->color;
		ins >> newLine->point1.x;
		ins >> newLine->point1.y;
		ins >> newLine->point2.x;
		ins >> newLine->point2.y;
		(*shapes).insert((*shapes).end(), shared_ptr<Line>(new Line(*newLine)));
	}
};

// 方形
class Square : public Shape {
   public:
	D2D1_RECT_F square;
	int color;
	Square() : color(0), square(D2D1::RectF(0, 0, 0, 0)) {}
	Square(int Color, float left, float top, float right, float bottom)
		: color(Color), square(D2D1::RectF(left, top, right, bottom)) {}

	static void Insert(D2D1_POINT_2F* ptMouse, list<shared_ptr<Shape>>* shapes,
					   list<shared_ptr<Shape>>::iterator* selection,
					   size_t* nextColor) {
		Square* newSquare = new Square(*nextColor, (*ptMouse).x, (*ptMouse).y,
									   (*ptMouse).x, (*ptMouse).y);
		*selection = (*shapes).insert(
			(*shapes).end(), shared_ptr<Shape>(new Square(*newSquare)));
		*nextColor = (*nextColor + 1) % ARRAYSIZE(colors);
		*ptMouse = D2D1::Point2F(newSquare->square.left, newSquare->square.top);
	}
	void Draw(ID2D1RenderTarget* pRT, ID2D1SolidColorBrush* pBrush) {
		pBrush->SetColor(D2D1::ColorF(colors[color]));
		pRT->FillRectangle(square, pBrush);
		pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
		pRT->DrawRectangle(square, pBrush);
	}
	void Change(D2D1_POINT_2F ptMouse, float x, float y) {
		square = D2D1::Rect(ptMouse.x, ptMouse.y, x, y);
	}
	BOOL HitTest(float x, float y) {
		const float x1 = square.left;
		const float y1 = square.top;
		const float x2 = square.right;
		const float y2 = square.bottom;
		return x <= max(x1, x2) && x >= min(x1, x2) && y <= max(y1, y2) &&
			   y >= min(y1, y2);
	}
	void Drag(D2D1_POINT_2F ptMouse, float x, float y) {
		square.left += x - ptMouse.x;
		square.top += y - ptMouse.y;
		square.right += x - ptMouse.x;
		square.bottom += y - ptMouse.y;
	}
	void Save(fstream* myFile) {
		*myFile << "S"
				<< " ";
		*myFile << to_string(color) << " ";
		*myFile << to_string(square.left) << " ";
		*myFile << to_string(square.top) << " ";
		*myFile << to_string(square.right) << " ";
		*myFile << to_string(square.bottom) << "\n";
	}
	static void Read(string myString, list<shared_ptr<Shape>>* shapes) {
		Square* newSquare = new Square;
		stringstream ins(myString);
		ins >> newSquare->color;
		ins >> newSquare->square.left;
		ins >> newSquare->square.top;
		ins >> newSquare->square.right;
		ins >> newSquare->square.bottom;
		(*shapes).insert((*shapes).end(),
						 shared_ptr<Square>(new Square(*newSquare)));
	}
};

// 窗口
class MainWindow : public BaseWindow<MainWindow> {
	HCURSOR hCursor;

	ID2D1Factory* pFactory;
	ID2D1HwndRenderTarget* pRenderTarget;
	ID2D1SolidColorBrush* pBrush;
	D2D1_POINT_2F ptMouse;	// 上一状态鼠标所在的点

	void ClearSelection() { selection = shapes.end(); }

	BOOL HitTest(float x, float y);
	// void MoveSelection(float x, float y);
	HRESULT CreateGraphicsResources();
	void DiscardGraphicsResources();
	void OnPaint();
	void Resize();
	void OnLButtonDown(int pixelX, int pixelY, DWORD flags);
	void OnLButtonUp();
	void OnMouseMove(int pixelX, int pixelY, DWORD flags);
	void OnKeyDown(UINT vkey);

   public:
	MainWindow()
		: pFactory(NULL),
		  pRenderTarget(NULL),
		  pBrush(NULL),
		  ptMouse(D2D1::Point2F()),
		  nextColor(0),
		  selection(shapes.end()) {}

	list<shared_ptr<Shape>> shapes;
	list<shared_ptr<Shape>>::iterator selection;
	shared_ptr<Shape> Selection() {
		if (selection == shapes.end()) {
			return nullptr;
		} else {
			return (*selection);
		}
	}
	size_t nextColor;
	Mode mode;
	void SetMode(Mode m);
	void ReadFile();
	void SaveFile();
	PCWSTR ClassName() const { return L"Circle Window Class"; }
	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// 创建图形资源
HRESULT MainWindow::CreateGraphicsResources() {
	HRESULT hr = S_OK;
	if (pRenderTarget == NULL) {
		RECT rc;
		GetClientRect(m_hwnd, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

		hr = pFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(m_hwnd, size), &pRenderTarget);

		if (SUCCEEDED(hr)) {
			const D2D1_COLOR_F color = D2D1::ColorF(1.0f, 1.0f, 0);
			hr = pRenderTarget->CreateSolidColorBrush(color, &pBrush);
		}
	}
	return hr;
}

// 废弃图形资源
void MainWindow::DiscardGraphicsResources() {
	SafeRelease(&pRenderTarget);
	SafeRelease(&pBrush);
}

// 绘制图形
void MainWindow::OnPaint() {
	HRESULT hr = CreateGraphicsResources();
	if (SUCCEEDED(hr)) {
		PAINTSTRUCT ps;
		BeginPaint(m_hwnd, &ps);

		pRenderTarget->BeginDraw();

		pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::SkyBlue));
		for (auto i = shapes.begin(); i != shapes.end(); ++i) {
			(*i)->Draw(pRenderTarget, pBrush);
		}
		hr = pRenderTarget->EndDraw();
		if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET) {
			DiscardGraphicsResources();
		}
		EndPaint(m_hwnd, &ps);
	}
}

// 改变窗口大小
void MainWindow::Resize() {
	if (pRenderTarget != NULL) {
		RECT rc;
		GetClientRect(m_hwnd, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

		pRenderTarget->Resize(size);

		InvalidateRect(m_hwnd, NULL, FALSE);
	}
}

// 按下鼠标左键
void MainWindow::OnLButtonDown(int pixelX, int pixelY, DWORD flags) {
	const float dipX = DPIScale::PixelsToDipsX(pixelX);
	const float dipY = DPIScale::PixelsToDipsY(pixelY);
	POINT pt = {pixelX, pixelY};
	ptMouse = D2D1::Point2F(dipX, dipY);

	switch (mode) {
		case LineMode: {
			if (DragDetect(m_hwnd, pt)) {
				SetCapture(m_hwnd);
				Line::Insert(&ptMouse, &shapes, &selection, &nextColor);
			}
			break;
		}
		case EllipseMode: {
			if (DragDetect(m_hwnd, pt)) {
				SetCapture(m_hwnd);
				Elli::Insert(&ptMouse, &shapes, &selection, &nextColor);
			}
			break;
		}
		case SquareMode: {
			if (DragDetect(m_hwnd, pt)) {
				SetCapture(m_hwnd);
				Square::Insert(&ptMouse, &shapes, &selection, &nextColor);
			}
			break;
		}
		case SelectMode:
			ClearSelection();
			if (HitTest(dipX, dipY)) {
				SetCapture(m_hwnd);
				ptMouse = D2D1::Point2F(dipX, dipY);
				SetMode(DragMode);
			}
	}
	InvalidateRect(m_hwnd, NULL, FALSE);
}

// 抬起鼠标左键
void MainWindow::OnLButtonUp() {
	if (mode <= 2 && Selection()) {
		ClearSelection();
		InvalidateRect(m_hwnd, NULL, FALSE);
	} else if (mode == DragMode) {
		SetMode(SelectMode);
	}
	ReleaseCapture();
}

// 按住鼠标左键后进行移动
void MainWindow::OnMouseMove(int pixelX, int pixelY, DWORD flags) {
	const float dipX = DPIScale::PixelsToDipsX(pixelX);
	const float dipY = DPIScale::PixelsToDipsY(pixelY);

	if (flags & MK_LBUTTON) {
		if (mode <= 2 && Selection()) {
			Selection()->Change(ptMouse, dipX, dipY);
		} else if (mode == DragMode) {
			Selection()->Drag(ptMouse, dipX, dipY);
			ptMouse = D2D1::Point2F(dipX, dipY);
		}
		InvalidateRect(m_hwnd, NULL, FALSE);
	}
}

// 检测选择时的指针是否在某一图形区域内
BOOL MainWindow::HitTest(float x, float y) {
	for (auto i = shapes.rbegin(); i != shapes.rend(); ++i) {
		if ((*i)->HitTest(x, y)) {
			selection = (++i).base();
			return TRUE;
		}
	}
	return FALSE;
}

// 设置模式
void MainWindow::SetMode(Mode m) {
	mode = m;

	LPWSTR cursor = 0;
	switch (mode) {
		case LineMode:
			cursor = IDC_CROSS;
			break;

		case SquareMode:
			cursor = IDC_CROSS;
			break;

		case EllipseMode:
			cursor = IDC_CROSS;
			break;

		case SelectMode:
			cursor = IDC_HAND;
			break;

		case DragMode:
			cursor = IDC_SIZEALL;
			break;
	}

	hCursor = LoadCursor(NULL, cursor);
	SetCursor(hCursor);
}

// 主程序
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
	MainWindow win;

	if (!win.Create(L"Draw Circles", WS_OVERLAPPEDWINDOW)) {
		return 0;
	}

	HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCEL1));

	ShowWindow(win.Window(), nCmdShow);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		if (!TranslateAccelerator(win.Window(), hAccel, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return 0;
}

// 读取文件
void MainWindow::ReadFile() {
	fstream myFile;
	myFile.open("painting.txt");
	if (myFile) {
		string myString;
		while (myFile) {
			getline(myFile, myString);
			stringstream ins(myString);
			char kind = myString[0];
			myString.erase(0, 2);
			if (kind == 'E') {
				Elli::Read(myString, &shapes);
			}
			if (kind == 'L') {
				Line::Read(myString, &shapes);
			}
			if (kind == 'S') {
				Square::Read(myString, &shapes);
			}
		}
		OnPaint();
		myFile.close();
	}
}

// 保存文件
void MainWindow::SaveFile() {
	fstream myFile;
	myFile.open("painting.txt", ios::out);
	if (myFile) {
		string mystring;
		for (auto i = shapes.begin(); i != shapes.end(); ++i) {
			(*i)->Save(&myFile);
		}
		myFile.close();
	}
}

// 处理信息
LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_CREATE:
			if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
										 &pFactory))) {
				return -1;
			}
			DPIScale::Initialize(m_hwnd);
			SetMode(EllipseMode);
			return 0;

		case WM_DESTROY:
			DiscardGraphicsResources();
			SafeRelease(&pFactory);
			PostQuitMessage(0);
			return 0;

		case WM_PAINT:
			OnPaint();
			return 0;

		case WM_SIZE:
			Resize();
			return 0;

		case WM_LBUTTONDOWN:
			OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam),
						  (DWORD)wParam);
			return 0;

		case WM_LBUTTONUP:
			OnLButtonUp();
			return 0;

		case WM_MOUSEMOVE:
			OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam),
						(DWORD)wParam);
			return 0;

		case WM_SETCURSOR:
			if (LOWORD(lParam) == HTCLIENT) {
				SetCursor(hCursor);
				return TRUE;
			}
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case ID_L_MODE:
					SetMode(LineMode);
					break;

				case ID_E_MODE:
					SetMode(EllipseMode);
					break;

				case ID_S_MODE:
					SetMode(SquareMode);
					break;

				case ID_SELECT_MODE:
					SetMode(SelectMode);
					break;

				case ID_TOGGLE_MODE:
					if (mode <= 2) {
						SetMode(SelectMode);
					} else {
						SetMode(EllipseMode);
					}
					break;

				case ID_READ:
					ReadFile();
					break;

				case ID_SAVE:
					SaveFile();
					break;
			}
			return 0;
	}
	return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}