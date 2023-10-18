#include <Windowsx.h>
#include <d2d1.h>
#include <windows.h>

#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
using namespace std;
using std::vector;

#pragma comment(lib, "d2d1")

#include "basewin.h"
#include "resource.h"

enum ShapeMode { LineMode, SquareMode, EllipseMode, SelectMode, DragMode };
enum DrawMode { DragDraw, ClickDraw };

// 颜色枚举
D2D1::ColorF::Enum colors[] = {D2D1::ColorF::Yellow, D2D1::ColorF::Salmon,
							   D2D1::ColorF::LimeGreen, D2D1::ColorF::Purple};

// 安全释放指针
template <class T>
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
	int color;
	vector<D2D1_POINT_2F> points;
	Shape() : color(0) {}
	Shape(size_t* nextColor) {
		color = *nextColor;
		*nextColor = (*nextColor + 1) % ARRAYSIZE(colors);
	}

	virtual void Draw(ID2D1RenderTarget* pRT, ID2D1SolidColorBrush* pBrush) {}
	virtual BOOL HitTest(float x, float y) { return FALSE; }
	virtual void Drag(D2D1_POINT_2F ptMouse, D2D1_POINT_2F nowMouse);
	virtual void Save(fstream* myFile) {}
	virtual void SaveCommon(fstream* myFile);
	static void ReadCommon(string myString, Shape* nowShape);
};

void Shape::Drag(D2D1_POINT_2F ptMouse, D2D1_POINT_2F nowMouse) {
	for (auto i = points.begin(); i != points.end(); i++) {
		i->x += nowMouse.x - ptMouse.x;
		i->y += nowMouse.y - ptMouse.y;
	}
}

void Shape::SaveCommon(fstream* myFile) {
	*myFile << to_string(color) << " ";
	for (auto i : points) {
		*myFile << to_string(i.x) << " ";
		*myFile << to_string(i.y) << " ";
	}
	*myFile << "\n";
}

void Shape::ReadCommon(string myString, Shape* nowShape) {
	stringstream ins(myString);
	ins >> nowShape->color;
	while (ins) {
		nowShape->points.push_back(D2D1::Point2F());
		ins >> nowShape->points.back().x;
		ins >> nowShape->points.back().y;
	}
}

// 椭圆
class Elli : public Shape {
   public:
	D2D1_ELLIPSE ellipse;
	Elli() : Shape() {}
	Elli(size_t* nextColor) : Shape(nextColor) {}

	void Draw(ID2D1RenderTarget* pRT, ID2D1SolidColorBrush* pBrush) {
		if (points.size() >= 2) {
			ellipse =
				D2D1::Ellipse(D2D1::Point2F((points[0].x + points[1].x) / 2,
											(points[0].y + points[1].y) / 2),
							  (points[0].x - points[1].x) / 2,
							  (points[0].y - points[1].y) / 2);
			pBrush->SetColor(D2D1::ColorF(colors[color]));
			pRT->FillEllipse(ellipse, pBrush);
			pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
			pRT->DrawEllipse(ellipse, pBrush);
		} else
			free(this);
	}
	BOOL HitTest(float x, float y) {
		const float a = ellipse.radiusX;
		const float b = ellipse.radiusY;
		const float x1 = x - ellipse.point.x;
		const float y1 = y - ellipse.point.y;
		const float d = ((x1 * x1) / (a * a)) + ((y1 * y1) / (b * b));
		return d <= 1.0f;
	}
	void Save(fstream* myFile) {
		*myFile << "E"
				<< " ";
		SaveCommon(myFile);
	}
	static void Read(string myString, list<shared_ptr<Shape>>* shapes) {
		Elli* newElli = new Elli;
		ReadCommon(myString, newElli);
		(*shapes).push_back(shared_ptr<Elli>(new Elli(*newElli)));
	}
};

// 直线
class Line : public Shape {
   public:
	Line() : Shape() {}
	Line(size_t* nextColor) : Shape(nextColor) {}

	void Draw(ID2D1RenderTarget* pRT, ID2D1SolidColorBrush* pBrush) {
		if (points.size() >= 2) {
			pBrush->SetColor(D2D1::ColorF(colors[color]));
			pRT->DrawLine(points[0], points[1], pBrush);
		} else
			free(this);
	}
	BOOL HitTest(float x, float y) {
		const float x1 = points[0].x;
		const float x2 = points[1].x;
		const float y1 = points[0].y;
		const float y2 = points[1].y;
		return x <= max(x1, x2) && x >= min(x1, x2) && y <= max(y1, y2) &&
			   y >= min(y1, y2) &&
			   abs((x - x1) / (y - y1) - (x - x2) / (y - y2)) <= 0.05;
	}
	void Save(fstream* myFile) {
		*myFile << "L"
				<< " ";
		SaveCommon(myFile);
	}
	static void Read(string myString, list<shared_ptr<Shape>>* shapes) {
		Line* newLine = new Line;
		ReadCommon(myString, newLine);
		(*shapes).push_back(shared_ptr<Line>(new Line(*newLine)));
	}
};

// 方形
class Square : public Shape {
   public:
	D2D1_RECT_F square;
	Square() : Shape() {}
	Square(size_t* nextColor) : Shape(nextColor) {}

	void Draw(ID2D1RenderTarget* pRT, ID2D1SolidColorBrush* pBrush) {
		if (points.size() >= 2) {
			square =
				D2D1::RectF(points[0].x, points[0].y, points[1].x, points[1].y);
			pBrush->SetColor(D2D1::ColorF(colors[color]));
			pRT->FillRectangle(square, pBrush);
			pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
			pRT->DrawRectangle(square, pBrush);
		} else
			free(this);
	}
	BOOL HitTest(float x, float y) {
		const float x1 = square.left;
		const float y1 = square.top;
		const float x2 = square.right;
		const float y2 = square.bottom;
		return x <= max(x1, x2) && x >= min(x1, x2) && y <= max(y1, y2) &&
			   y >= min(y1, y2);
	}
	void Save(fstream* myFile) {
		*myFile << "S"
				<< " ";
		SaveCommon(myFile);
	}
	static void Read(string myString, list<shared_ptr<Shape>>* shapes) {
		Square* newSquare = new Square;
		ReadCommon(myString, newSquare);
		(*shapes).push_back(shared_ptr<Square>(new Square(*newSquare)));
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
	void OnLButtonDouble(int pixelX, int pixelY, DWORD flags);
	void OnLButtonUp();
	void OnMouseMove(int pixelX, int pixelY, DWORD flags);

   public:
	MainWindow()
		: pFactory(NULL),
		  pRenderTarget(NULL),
		  pBrush(NULL),
		  ptMouse(D2D1::Point2F()),
		  nextColor(0),
		  shapemode(EllipseMode),
		  drawmode(DrawMode::DragDraw),
		  newShape(NULL),
		  selection(shapes.end()) {}

	shared_ptr<Shape> newShape;
	list<shared_ptr<Shape>> shapes;
	list<shared_ptr<Shape>>::iterator selection;  // 急需更改
	shared_ptr<Shape> Selection() {				  // 急需更改
		if (selection == shapes.end()) {
			return nullptr;
		} else {
			return (*selection);
		}
	}
	size_t nextColor;
	ShapeMode shapemode;
	DrawMode drawmode;
	void SetShapeMode(ShapeMode m);
	void SetDrawMode(DrawMode m);

	void CreateShape();
	void DragDraw();
	void ClickDraw();

	void ReadFile();
	void SaveFile();
	PCWSTR ClassName() const { return L"Circle Window Class"; }
	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

void MainWindow::CreateShape() {
	switch (shapemode) {
		case EllipseMode: {
			shapes.push_back(shared_ptr<Shape>(new Elli(&nextColor)));
			break;
		}
		case LineMode: {
			shapes.push_back(shared_ptr<Shape>(new Line(&nextColor)));
			break;
		}
		case SquareMode: {
			shapes.push_back(shared_ptr<Shape>(new Square(&nextColor)));
			break;
		}
	}
	newShape = shapes.back();
}

void MainWindow::DragDraw() {
	CreateShape();
	newShape->points.push_back(ptMouse);
	newShape->points.push_back(ptMouse);
}

void MainWindow::ClickDraw() {
	if (newShape == NULL) {
		CreateShape();
	}
	newShape->points.push_back(ptMouse);
}

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
			if ((*i) != newShape || drawmode == DrawMode::DragDraw)
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

	switch (shapemode) {
		case LineMode:
		case EllipseMode:
		case SquareMode: {
			if (drawmode == DrawMode::ClickDraw) {
				ClickDraw();
			}
			if (drawmode == DrawMode::DragDraw && DragDetect(m_hwnd, pt)) {
				SetCapture(m_hwnd);
				DragDraw();
			}
			break;
		}
		case SelectMode:
			ClearSelection();
			if (HitTest(dipX, dipY)) {
				SetCapture(m_hwnd);
				ptMouse = D2D1::Point2F(dipX, dipY);
				SetShapeMode(DragMode);
			}
	}
	InvalidateRect(m_hwnd, NULL, FALSE);
}

void MainWindow::OnLButtonDouble(int pixelX, int pixelY, DWORD flags) {
	if (drawmode == DrawMode::ClickDraw) {
		newShape = NULL;
	}
}

// 抬起鼠标左键
void MainWindow::OnLButtonUp() {
	if (shapemode <= 2 && drawmode == DrawMode::DragDraw) {
		newShape = NULL;
		InvalidateRect(m_hwnd, NULL, FALSE);
	} else if (shapemode == DragMode) {
		SetShapeMode(SelectMode);
	}
	ReleaseCapture();
}

// 按住鼠标左键后进行移动
void MainWindow::OnMouseMove(int pixelX, int pixelY, DWORD flags) {
	const float dipX = DPIScale::PixelsToDipsX(pixelX);
	const float dipY = DPIScale::PixelsToDipsY(pixelY);
	D2D1_POINT_2F nowMouse = D2D1::Point2F(dipX, dipY);

	if (flags & MK_LBUTTON) {
		if (shapemode <= 2 && newShape && drawmode == DrawMode::DragDraw) {
			newShape->points[1] = nowMouse;
		} else if (shapemode == DragMode) {
			Selection()->Drag(ptMouse, nowMouse);
			ptMouse = nowMouse;
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
void MainWindow::SetShapeMode(ShapeMode m) {
	shapemode = m;

	LPWSTR cursor = 0;
	switch (shapemode) {
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

void MainWindow::SetDrawMode(DrawMode m) { drawmode = m; }

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
			SetShapeMode(EllipseMode);
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

		case WM_LBUTTONDBLCLK:
			OnLButtonDouble(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam),
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
					SetShapeMode(LineMode);
					break;

				case ID_E_MODE:
					SetShapeMode(EllipseMode);
					break;

				case ID_S_MODE:
					SetShapeMode(SquareMode);
					break;

				case ID_SELECT_MODE:
					SetShapeMode(SelectMode);
					break;

				case ID_TOGGLE_MODE:
					if (shapemode <= 2) {
						SetShapeMode(SelectMode);
					} else {
						SetShapeMode(EllipseMode);
					}
					break;

				case ID_CLICK:
					SetDrawMode(DrawMode::ClickDraw);
					break;

				case ID_DRAG:
					SetDrawMode(DrawMode::DragDraw);
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