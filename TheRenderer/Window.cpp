#include "Window.h"
#include <string>
#include <stdexcept>
#include <cstdlib>
#include "ChiliException.h"
#include <sstream>
#include "resource.h"
#include <optional>
std::wstring ConvertToWideString(const std::string& str) {
	size_t len = str.length() + 1; // length including null-terminator
	std::wstring wstr(len, L'\0'); // Initialize a wide string with enough space
	size_t convertedChars = 0; // To store the number of characters converted

	// Use mbstowcs_s for safe conversion
	mbstowcs_s(&convertedChars, &wstr[0], len, str.c_str(), len);

	// Resize the string to fit the actual converted characters (minus null-terminator)
	wstr.resize(convertedChars - 1);
	return wstr;
}

Window::WindowClass Window::WindowClass::wndClass;
Window::WindowClass::WindowClass() noexcept :hInst(GetModuleHandle(nullptr))
{
	WNDCLASSEX wc = { 0 };
	wc.cbSize = sizeof(wc);
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = HandleMsgSetup;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetInstance();
	wc.hIcon = static_cast<HICON>(LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 16, 16, 0));
	wc.hCursor = nullptr;
	wc.hbrBackground = nullptr;
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = L"HD Direct3D Engine Window";
	wc.hIconSm = static_cast<HICON>(LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 16, 16, 0));
	RegisterClassEx(&wc);
}

Window::WindowClass::~WindowClass()
{
	UnregisterClass(L"HD Direct3D Engine Window", GetInstance());
}

const char* Window::WindowClass::GetName() noexcept {
	return wndClassName;
}

HINSTANCE Window::WindowClass::GetInstance() noexcept
{
	return wndClass.hInst;
}

Window::Window(int width, int height, const char* name) noexcept
	: width(width),height(height)
{
	RECT wr;
	wr.left = 100;
	wr.right = width + wr.left;
	wr.top = 100;
	wr.bottom = height + wr.top;
	AdjustWindowRect(&wr, WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU, false);
	//throw CHWND_EXCEPT(ERROR_ARENA_TRASHED);
	hWnd = CreateWindowEx(0, L"HD Direct3D Engine Window", L"Render window", WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU,
		CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
		nullptr, nullptr, WindowClass::GetInstance(), this);


	if (!hWnd)
	{
		DWORD error = GetLastError();
		wchar_t errorMsg[512];
		FormatMessage(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			error,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			errorMsg,
			sizeof(errorMsg) / sizeof(wchar_t),
			nullptr
		);

		MessageBox(nullptr, errorMsg, L"Failed to create window", MB_OK);
	}
	else
	{
		ShowWindow(hWnd, SW_SHOWDEFAULT);
		// Create graphics object
		pGfx = std::make_unique<Graphics>(hWnd);
		UpdateWindow(hWnd); // Ensures the window is redrawn
	}
}

Window::~Window()
{
	DestroyWindow(hWnd);
}

LRESULT WINAPI Window::HandleMsgSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
	// use create parameter passed in from CreateWindow() to store window class pointer at WinAPI side
	if (msg == WM_NCCREATE)
	{
		// extract ptr to window class from creation data
		const CREATESTRUCTW* const pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
		Window* const pWnd = static_cast<Window*>(pCreate->lpCreateParams);
		// set WinAPI-managed user data to store ptr to window instance
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWnd));
		// set message proc to normal (non-setup) handler now that setup is finished
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Window::HandleMsgThunk));
		// forward message to window instance handler
		return pWnd->HandleMsg(hWnd, msg, wParam, lParam);
	}
	// if we get a message before the WM_NCCREATE message, handle with default handler
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void Window::SetTitle(const std::string& title)
{
	if (SetWindowText(hWnd, ConvertToWideString(title).c_str()) == 0)
	{
		//throw CHWND_LAST_EXCEPT();
	}
}

Graphics& Window::Gfx()
{
	return *pGfx;
}

LRESULT CALLBACK Window::HandleMsgThunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
	Window* const pWnd = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	return pWnd->HandleMsg(hWnd, msg, wParam, lParam);
}

LRESULT Window::HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lPARAM) noexcept
{
	switch (msg)
	{
	case WM_CLOSE:
		PostQuitMessage(69);
		return 0;
	case WM_KILLFOCUS:
		keyboard.ClearState();
		break;
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
		if (!(lPARAM && 0x40000000) || keyboard.AutorepeatIsEnabled())
		{
			keyboard.OnKeyPressed(static_cast<unsigned char>(wParam));
		}
		break;
	case WM_SYSKEYUP:
	case WM_KEYUP:
		keyboard.OnKeyReleased(static_cast<unsigned char>(wParam));
		break;
	case WM_CHAR:
		keyboard.OnChar(static_cast<unsigned char>(wParam));
		break;
	case WM_MOUSEMOVE:
	{
		POINTS pt = MAKEPOINTS(lPARAM);
		if (pt.x >= 0 && pt.x < width && pt.y >= 0 && pt.y < height)
		{
			mouse.OnMouseMove(pt.x, pt.y);
			if (!mouse.IsInWindow())
			{
				SetCapture(hWnd);
				mouse.OnMouseEnter();
			}
		}
		else 
		{
			if (wParam & (MK_LBUTTON | MK_RBUTTON))
			{
				mouse.OnMouseMove(pt.x, pt.y);
			}
			else
			{
				ReleaseCapture();
				mouse.OnMouseLeave();
			}
		}
	}
	break;
	case WM_LBUTTONDOWN:
	{
		POINTS pt = MAKEPOINTS(lPARAM);
		mouse.OnLeftPressed(pt.x, pt.y);
	}
	break;
	case WM_RBUTTONDOWN:
	{
		POINTS pt = MAKEPOINTS(lPARAM);
		mouse.OnRightPressed(pt.x, pt.y);
	}
	break;
	case WM_LBUTTONUP:
	{
		POINTS pt = MAKEPOINTS(lPARAM);
		mouse.OnLeftReleased(pt.x, pt.y);
	}
	break;
	case WM_RBUTTONUP:
	{
		POINTS pt = MAKEPOINTS(lPARAM);
		mouse.OnRightReleased(pt.x, pt.y);
	}
	break;
	case WM_MOUSEWHEEL:
	{
		POINTS pt = MAKEPOINTS(lPARAM);
		const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		mouse.OnWheelDelta(pt.x, pt.y, delta);
	}
	break;
	}
	return DefWindowProc(hWnd, msg, wParam, lPARAM);
}

Window::Exception::Exception(int line, const char* file, HRESULT hr) noexcept
	:
	ChiliException(line, file), hr(hr)
{}

const char* Window::Exception::what() const noexcept {
	std::ostringstream oss;
	oss << GetType() << std::endl
		<< "[Error code]" << GerErrorCode() << std::endl
		<< "[Description]" << GetErrorString() << std::endl
		<< GetOriginString();
	whatBuffer == oss.str();
	return whatBuffer.c_str();
}

const char* Window::Exception::GetType() const noexcept
{
	return "Custom Window Exception";
}

std::string Window::Exception::TranslateErrorCode(HRESULT hr) noexcept
{
	char* pMsgBuf = nullptr;
	DWORD nMsgLen = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPSTR>(&pMsgBuf), 0, nullptr
	);
	if (nMsgLen == 0)
	{
		return "Unidentified error";
	}
	std::string errorString = pMsgBuf;
	LocalFree(pMsgBuf);
	return errorString;
}
HRESULT Window::Exception::GerErrorCode() const noexcept
{
	return hr;
}

std::string Window::Exception::GetErrorString() const noexcept {
	return TranslateErrorCode(hr);
}

std::optional<int> Window::ProcessMessages()
{
	MSG msg;
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			return msg.wParam;
		}
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return {};
}